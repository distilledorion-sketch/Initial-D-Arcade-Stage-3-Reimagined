using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Epic.OnlineServices;
using Epic.OnlineServices.Connect;
using Epic.OnlineServices.Platform;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // The SDK is process-wide. All callbacks are queued before touching game
    // state, including callbacks from Tick during a synchronous native load.
    internal static class Idas3EosPlatform
    {
        static readonly Queue<Action> callbacks = new Queue<Action>();
        static Task authentication;
        static bool ticking, dispatching, stopped, renewing, sdkInitialized;
        static string identityConfig;
        static ulong expiryNotify, loginNotify;
        static double nextRenew;
        internal static PlatformInterface Handle { get; private set; }
        internal static ProductUserId User { get; private set; }
        internal static string Failure { get; private set; }
        internal static bool IsStopping => stopped;
        internal static bool SignedIn => Handle != null && User != null && Handle.GetConnectInterface().GetLoginStatus(User) == LoginStatus.LoggedIn;
        internal static double Now => (double)System.Diagnostics.Stopwatch.GetTimestamp() / System.Diagnostics.Stopwatch.Frequency;

        internal static Task InitializeAsync(Idas3EosOnlineConfig config)
        {
            string identity = config.ProductId + ":" + config.SandboxId + ":" + config.DeploymentId + ":" + config.ClientId;
            if (Handle != null && identityConfig != identity) throw new InvalidOperationException("Restart the game after changing online service settings.");
            if (authentication == null || authentication.IsFaulted || authentication.IsCanceled) authentication = InitializeCore(config, identity);
            return authentication;
        }

        static async Task InitializeCore(Idas3EosOnlineConfig config, string identity)
        {
            if (stopped) throw new InvalidOperationException("Online services are shutting down.");
            if (Handle == null) {
                Idas3EosNativeLibrary.Initialize();
                var init = new InitializeOptions { ProductName = "Initial D Arcade Stage 3 Reimagined", ProductVersion = "1.0" };
                Check(PlatformInterface.Initialize(ref init), "Online SDK initialization", Result.AlreadyConfigured);
                sdkInitialized = true;
                Application.quitting -= Shutdown; Application.quitting += Shutdown;
#if UNITY_EDITOR
                UnityEditor.AssemblyReloadEvents.beforeAssemblyReload -= Shutdown;
                UnityEditor.AssemblyReloadEvents.beforeAssemblyReload += Shutdown;
                UnityEditor.EditorApplication.playModeStateChanged -= EditorPlayState;
                UnityEditor.EditorApplication.playModeStateChanged += EditorPlayState;
#endif
                var options = new Options {
                    ProductId = config.ProductId, SandboxId = config.SandboxId, DeploymentId = config.DeploymentId,
                    ClientCredentials = new ClientCredentials { ClientId = config.ClientId, ClientSecret = config.ClientSecret },
                    Flags = PlatformFlags.DisableOverlay | PlatformFlags.DisableSocialOverlay | (Application.isEditor ? PlatformFlags.LoadingInEditor : PlatformFlags.None),
                    IsServer = false, TickBudgetInMilliseconds = 2,
                    CacheDirectory = Path.Combine(Application.persistentDataPath, "EOS", Idas3EosOnlineConfig.AuthenticationProfile),
                    TaskNetworkTimeoutSeconds = 30
                };
                Handle = PlatformInterface.Create(ref options);
                if (Handle == null) throw new InvalidOperationException("Online SDK could not create the game client.");
                identityConfig = identity;
                var pump = new GameObject("Online service callbacks");
                pump.hideFlags = HideFlags.HideInHierarchy;
                UnityEngine.Object.DontDestroyOnLoad(pump);
                pump.AddComponent<Idas3EosPlatformPump>();
            }
            Failure = null;
            var connect = Handle.GetConnectInterface();
            // DeviceModel describes hardware; it is NOT an identity override.
            // Never delete an existing device credential to fabricate test users.
            var created = new TaskCompletionSource<Result>();
            var device = new CreateDeviceIdOptions { DeviceModel = Application.platform.ToString() };
            connect.CreateDeviceId(ref device, null, (ref CreateDeviceIdCallbackInfo info) => {
                var result = info.ResultCode; Defer(() => created.TrySetResult(result));
            });
            Check(await created.Task, "Creating the local online identity", Result.DuplicateNotAllowed);
            await Login();
            if (expiryNotify == 0) {
                var expire = new AddNotifyAuthExpirationOptions();
                expiryNotify = connect.AddNotifyAuthExpiration(ref expire, null, (ref AuthExpirationCallbackInfo info) => Defer(() => nextRenew = Now));
                var login = new AddNotifyLoginStatusChangedOptions();
                loginNotify = connect.AddNotifyLoginStatusChanged(ref login, null, (ref LoginStatusChangedCallbackInfo info) => {
                    bool expired = info.CurrentStatus != LoginStatus.LoggedIn;
                    if (expired) Defer(() => nextRenew = Now);
                });
            }
        }

        static async Task Login()
        {
            var connect = Handle.GetConnectInterface();
            var complete = new TaskCompletionSource<LoginCallbackInfo>();
            var options = new LoginOptions {
                Credentials = new Credentials { Type = ExternalCredentialType.DeviceidAccessToken },
                UserLoginInfo = new UserLoginInfo { DisplayName = Idas3EosOnlineConfig.PlayerName() }
            };
            connect.Login(ref options, null, (ref LoginCallbackInfo info) => { var copy = info; Defer(() => complete.TrySetResult(copy)); });
            var result = await complete.Task;
            if (result.ResultCode == Result.InvalidUser && result.ContinuanceToken != null) {
                var created = new TaskCompletionSource<CreateUserCallbackInfo>();
                var create = new CreateUserOptions { ContinuanceToken = result.ContinuanceToken };
                connect.CreateUser(ref create, null, (ref CreateUserCallbackInfo info) => { var copy = info; Defer(() => created.TrySetResult(copy)); });
                var newUser = await created.Task;
                Check(newUser.ResultCode, "Creating the online player");
                if (newUser.LocalUserId == null || !newUser.LocalUserId.IsValid()) throw new InvalidOperationException("Online services returned an invalid player identity.");
                if (User != null && User.ToString() != newUser.LocalUserId.ToString()) throw new Idas3EosIdentityChangedException();
                User = newUser.LocalUserId;
            } else {
                Check(result.ResultCode, "Signing in to online services");
                if (result.LocalUserId == null || !result.LocalUserId.IsValid()) throw new InvalidOperationException("Online services returned an invalid player identity.");
                if (User != null && User.ToString() != result.LocalUserId.ToString()) throw new Idas3EosIdentityChangedException();
                User = result.LocalUserId;
            }
            if (User == null || !User.IsValid()) throw new InvalidOperationException("Online services returned an invalid player identity.");
            nextRenew = double.PositiveInfinity;
        }

        internal static void Defer(Action action)
        {
            if (stopped) return;
            if (callbacks.Count >= 4096) { Failure = "Online service event buffer is full. Please reconnect."; return; }
            callbacks.Enqueue(action);
        }
        internal static void Tick(bool dispatch)
        {
            if (Handle == null || stopped || ticking) return;
            ticking = true;
            try { Handle.Tick(); } finally { ticking = false; }
            if (!dispatch || dispatching) return;
            dispatching = true;
            try {
                int budget = 4096;
                while (callbacks.Count != 0 && budget-- > 0) callbacks.Dequeue()();
                if (authentication != null && authentication.Status == TaskStatus.RanToCompletion && !renewing && Now >= nextRenew) {
                    renewing = true; _ = Renew();
                }
            } finally { dispatching = false; }
        }
        static async Task Renew()
        {
            try { await Login(); Failure = null; }
            catch (Idas3EosIdentityChangedException ex) { Failure = ex.Message; nextRenew = double.PositiveInfinity; }
            catch (Exception) { nextRenew = Now + 10; }
            finally { renewing = false; }
        }
        internal static void Check(Result result, string operation, Result also = Result.Success)
        {
            if (result != Result.Success && result != also && result != Result.NoChange) throw new Idas3EosServiceException(result, operation);
        }
        static void Shutdown()
        {
            if (stopped) return;
            stopped = true;
            // Close route handles while their native platform is still alive.
            // Delayed lobby cleanup sees IsStopping and cannot use freed handles.
            Idas3EosTransport.ShutdownAll();
            if (Handle != null) {
                var connect = Handle.GetConnectInterface();
                if (expiryNotify != 0) connect.RemoveNotifyAuthExpiration(expiryNotify);
                if (loginNotify != 0) connect.RemoveNotifyLoginStatusChanged(loginNotify);
                Handle.Release(); Handle = null;
            }
            if (sdkInitialized) { PlatformInterface.Shutdown(); sdkInitialized = false; }
            Idas3EosNativeLibrary.Shutdown();
            callbacks.Clear();
        }
#if UNITY_EDITOR
        static void EditorPlayState(UnityEditor.PlayModeStateChange state)
        {
            if (state == UnityEditor.PlayModeStateChange.ExitingPlayMode) Shutdown();
            // Also support Enter Play Mode with domain reload disabled.
            if (state == UnityEditor.PlayModeStateChange.EnteredEditMode && stopped) {
                authentication = null; User = null; identityConfig = null; expiryNotify = loginNotify = 0;
                stopped = renewing = ticking = dispatching = false; Failure = null; nextRenew = 0;
            }
        }
#endif
    }

    internal sealed class Idas3EosIdentityChangedException : InvalidOperationException
    {
        internal Idas3EosIdentityChangedException() : base("The online player identity changed. Restart the game.") { }
    }
    internal sealed class Idas3EosServiceException : InvalidOperationException
    {
        internal Result ResultCode { get; }
        internal Idas3EosServiceException(Result code, string operation) : base(operation + " failed (" + code + ").") { ResultCode = code; }
    }

    internal sealed class Idas3EosPlatformPump : MonoBehaviour
    {
        void Update() => Idas3EosPlatform.Tick(true);
    }
}
