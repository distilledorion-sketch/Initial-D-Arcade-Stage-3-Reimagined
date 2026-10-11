using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Unity.Services.Authentication;
using Unity.Services.Core;
using Unity.Services.Core.Environments;
using Unity.Services.Multiplayer;
using Unity.Services.Relay.Models;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Service callbacks and transport polling run on Unity's main thread. Every
    // request owns its network handler: completing an abandoned request cannot
    // attach to, or dispose, the next request's driver.
    public sealed class Idas3UnityTransport : IIdas3MatchmakingTransport, IIdas3PrivateRoomTransport,
        IIdas3MatchmakingServiceState, IIdas3QueuedMatchmakingTransport, IIdas3OnlineActivityTransport
    {
        internal const string CompatibilityKey = "compatibility", KindKey = "idas3.kind";
        internal const string NameKey = "idas3.name", SecretKey = "idas3.proof";
        internal const string Protocol = "idas3-relay-1";
        public const int MaxPayloadBytes = 65536;
        readonly List<Idas3Room> rooms = new List<Idas3Room>();
        Idas3UnityOnlineConfig config;
        Idas3UnityNetworkHandler route;
        Idas3UnityActivity activity;
        ISession session;
        Task pendingRequest = Task.CompletedTask, cleanupBarrier = Task.CompletedTask;
        CancellationTokenSource cancellation;
        long generation;
        bool disposed, initializing, refreshing;
        double deadline, nextRefresh;
        string busy, build = "", hostId = "";
        static Task serviceInitialization;
        static string serviceEnvironment, serviceProfile;
        static readonly List<Idas3UnityTransport> liveTransports = new List<Idas3UnityTransport>();
        static long nextNativePump;
        public static long NativeLoadKeepAlivePumps { get; private set; }
        public static long NativeLoadBufferedEvents { get; private set; }
        internal static double Now => Time.realtimeSinceStartupAsDouble;

        public Idas3UnityTransport() { liveTransports.Add(this); }
        // Invoked only by the rooted native loading callback on Unity's original
        // main thread. The handler buffers events; gameplay resumes in Poll.
        public static void KeepAliveDuringNativeLoad()
        {
            long now = System.Diagnostics.Stopwatch.GetTimestamp();
            if (now < nextNativePump) return;
            nextNativePump = now + System.Diagnostics.Stopwatch.Frequency / 10;
            foreach (var transport in liveTransports) {
                if (transport.disposed || transport.route == null) continue;
                long before = transport.route.NativeLoadBufferedEvents;
                if (transport.route.KeepAliveOnly()) ++NativeLoadKeepAlivePumps;
                NativeLoadBufferedEvents += transport.route.NativeLoadBufferedEvents - before;
            }
        }

        public string Kind => "Unity Relay";
        public bool Available { get; private set; }
        public bool ServiceConnected => Available && AuthenticationService.Instance.IsSignedIn;
        public bool Connected => route != null && route.Authenticated && session != null;
        public bool IsHost => session != null && session.IsHost;
        public string LocalId { get; private set; } = "";
        public string LocalName { get; private set; } = "Driver";
        public string RemoteId => Connected ? route.RemoteId : "";
        public string RemoteName => Connected ? route.RemoteName : "";
        public string RoomCode => session?.Code ?? "";
        public string Status { get; private set; } = "Online services are not initialized.";
        public IReadOnlyList<Idas3Room> Rooms => rooms;
        public bool IsBusy => initializing || busy != null;
        public bool IsMatching { get; private set; }
        public bool InLobby => session != null;
        public bool PrivateRoom => session != null && session.IsPrivate;
        public ulong RoomOrder => session == null ? 0 : Order(session.Id);
        public int RoomMembers => session?.PlayerCount ?? 0;
        public bool ActivityRequested { get; set; }
        public string ActivityState { get; set; } = "online";
        public bool PublishActivity { get; set; }
        public Idas3OnlineActivity Activity => activity?.Snapshot ?? default;
        public string BuildCompatibility {
            get => build;
            set {
                string next = value ?? "";
                if ((InLobby || IsBusy) && !string.Equals(build, next, StringComparison.Ordinal))
                    throw new InvalidOperationException("Leave the room or queue before changing the game build.");
                build = next;
            }
        }
        public event Action<byte[]> Message;
        public event Action PeerChanged;
        public event Action<string> Error;

        public bool Initialize()
        {
            if (disposed) return false;
            if (Available) return true;
            if (initializing) return false;
            long token = ++generation;
            initializing = true; deadline = Now + 40;
            Status = "Connecting to online services…";
            _ = InitializeAsync(token);
            return false;
        }

        async Task InitializeAsync(long token)
        {
            try {
                config = Idas3UnityOnlineConfig.Load();
                if (string.IsNullOrWhiteSpace(Application.cloudProjectId))
                    throw new InvalidOperationException("Link this game to its Unity Cloud project in Project Settings > Services, then rebuild.");
                string profile = Idas3UnityOnlineConfig.AuthenticationProfile;
                // Unity Services/Authentication are process singletons. Serialize
                // retries instead of racing two anonymous sign-in requests.
                if (serviceInitialization == null || serviceInitialization.IsFaulted || serviceInitialization.IsCanceled) {
                    serviceEnvironment = config.Environment; serviceProfile = profile;
                    serviceInitialization = InitializeServicesAsync(config.Environment, profile);
                }
                if (serviceEnvironment != config.Environment || serviceProfile != profile)
                    throw new InvalidOperationException("Restart the game after changing its online environment or test profile.");
                await serviceInitialization;
                if (!Current(token)) return;
                LocalId = AuthenticationService.Instance.PlayerId;
                LocalName = Idas3UnityOnlineConfig.PlayerName();
                Available = true; initializing = false;
                activity = new Idas3UnityActivity(LocalId);
                Status = "Online ready. Find a match, host a room, or enter a room code.";
            } catch (Exception ex) {
                if (Current(token)) { initializing = false; Available = false; Fail("Online initialization failed: " + ex.Message); }
            }
        }

        static async Task InitializeServicesAsync(string environment, string profile)
        {
            if (UnityServices.State != ServicesInitializationState.Initialized)
                await UnityServices.InitializeAsync(new InitializationOptions().SetEnvironmentName(environment).SetProfile(profile));
            if (!AuthenticationService.Instance.IsSignedIn) await AuthenticationService.Instance.SignInAnonymouslyAsync();
        }

        internal string CompatibilityHash => Hash(Protocol + "|" + build);
        public void Host(string roomName) => StartRoom(roomName, false, false, null);
        public void HostPrivate(string roomName) => StartRoom(roomName, true, false, null);
        public void HostQuickMatch(string roomName) => StartMatchmaking();
        public void StartMatchmaking() => StartRoom("Online race", true, true, null);
        public void CancelMatchmaking() { if (IsMatching) Leave(); }
        public void Join(string roomCode)
        {
            string code = (roomCode ?? "").Trim();
            if (code.Length == 0 || code.Length > 80) { Fail("Enter a valid room code."); return; }
            StartRoom(null, false, false, code);
        }

        void StartRoom(string name, bool privateRoom, bool match, string code)
        {
            if (!CanStart()) return;
            Leave();
            long token = Begin(match ? "Finding another driver" : code == null ? "Creating room" : "Joining room", match ? 180 : 45);
            IsMatching = match;
            var handler = new Idas3UnityNetworkHandler(this, token, RandomToken());
            route = handler;
            pendingRequest = StartRoomAsync(token, handler, name, privateRoom, match, code, cancellation.Token, cleanupBarrier);
        }

        async Task StartRoomAsync(long token, Idas3UnityNetworkHandler handler, string name, bool privateRoom, bool match, string code, CancellationToken cancel, Task previousCleanup)
        {
            ISession result = null;
            try {
                // Lobby removal is keyed by authenticated player ID. A delayed
                // old Leave must finish before this identity rejoins that room.
                await previousCleanup;
                if (!Current(token)) return;
                cancel.ThrowIfCancellationRequested();
                for (int retry = 0; ; ++retry) {
                    await Idas3UnityServiceRequests.WaitAsync(code == null && !match ? Idas3UnityServiceRequests.Kind.Create : Idas3UnityServiceRequests.Kind.Join, cancel);
                    if (!Current(token)) return;
                    string attemptId = Guid.NewGuid().ToString("N");
                    var players = new Dictionary<string, PlayerProperty> {
                        { NameKey, new PlayerProperty(LocalName, VisibilityPropertyOptions.Member) },
                        { SecretKey, new PlayerProperty(handler.Secret, VisibilityPropertyOptions.Member) },
                        { CompatibilityKey, new PlayerProperty(CompatibilityHash, VisibilityPropertyOptions.Member) },
                        { Idas3UnityServiceRequests.AttemptKey, new PlayerProperty(attemptId, VisibilityPropertyOptions.Member) }
                    };
                    var network = new NetworkOptions { RelayProtocol = config.Protocol == "wss" ? RelayProtocol.WSS : RelayProtocol.DTLS };
                    try {
                        if (code != null) {
                            var options = new JoinSessionOptions { Type = Protocol + "-" + Guid.NewGuid().ToString("N"), PlayerProperties = players }
                                .WithNetworkHandler(handler).WithNetworkOptions(network);
                            result = code.StartsWith("ID:", StringComparison.Ordinal)
                                ? await MultiplayerService.Instance.JoinSessionByIdAsync(code.Substring(3), options, cancel)
                                : await MultiplayerService.Instance.JoinSessionByCodeAsync(code.ToUpperInvariant(), options, cancel);
                        } else {
                            var options = new SessionOptions {
                                // Type is a client-local SDK handle key, not a public
                                // protocol field. Unique keys isolate delayed cleanup.
                                Type = Protocol + "-" + Guid.NewGuid().ToString("N"), Name = Clean(name, "Race room"), MaxPlayers = 2, IsPrivate = privateRoom,
                                PlayerProperties = players,
                                SessionProperties = new Dictionary<string, SessionProperty> {
                                    { CompatibilityKey, new SessionProperty(CompatibilityHash, VisibilityPropertyOptions.Public, PropertyIndex.String1) },
                                    { KindKey, new SessionProperty(match ? "queue" : "room", VisibilityPropertyOptions.Public, PropertyIndex.String2) },
                                    { NameKey, new SessionProperty(LocalName) }
                                }
                            }.WithRelayNetwork().WithNetworkHandler(handler).WithNetworkOptions(network);
                            if (match) {
                                var matchmaking = new MatchmakerOptions {
                                    QueueName = config.QueueName,
                                    TicketAttributes = new Dictionary<string, object> { { CompatibilityKey, CompatibilityHash } },
                                    // The queue compares players.customData.compatibility.
                                    PlayerProperties = new Dictionary<string, PlayerProperty> { { CompatibilityKey, new PlayerProperty(CompatibilityHash) } }
                                };
                                result = await MultiplayerService.Instance.MatchmakeSessionAsync(matchmaking, options, cancel);
                            } else result = await MultiplayerService.Instance.CreateSessionAsync(options, cancel);
                        }
                    } catch (Exception requestError) {
                        handler.Dispose();
                        // The SDK can throw after server membership was created but
                        // before exposing its handle. Confirm exact-attempt cleanup
                        // before either a retry or a replacement request proceeds.
                        bool retired = await Idas3UnityServiceRequests.RetireAttemptAsync(attemptId, LocalId);
                        if (!Current(token)) return;
                        if (retry >= 3 || !retired || !Idas3UnityServiceRequests.RateLimited(requestError)) throw;
                        Status = "Online services are busy. Retrying…";
                        await Idas3UnityServiceRequests.BackoffAsync(retry, cancel);
                        if (!Current(token)) return;
                        handler = new Idas3UnityNetworkHandler(this, token, RandomToken()); route = handler;
                        continue;
                    }
                    break;
                }
                if (!Current(token)) { await CleanupSession(result); return; }
                if (result.MaxPlayers != 2 || result.PlayerCount > 2 || Get(result.Properties, CompatibilityKey) != CompatibilityHash)
                    throw new InvalidOperationException("This room uses a different game build or room configuration.");
                session = result; hostId = result.Host;
                session.Changed += OnSessionChanged;
                session.RemovedFromSession += OnRemoved;
                session.Deleted += OnRemoved;
                handler.Attach(result);
                busy = match || !session.IsHost ? "Connecting to driver" : null;
                deadline = Now + 45; nextRefresh = Now + 3;
                Status = "Room " + RoomCode + " — " + (session.PlayerCount > 1 ? "connecting to driver…" : "waiting for another driver.");
                CheckMembership();
            } catch (Exception ex) {
                if (result != null && result != session) await CleanupSession(result);
                if (!Current(token)) return;
                handler.Dispose(); route = null; busy = null; IsMatching = false;
                if (ex is OperationCanceledException) { Leave(); return; }
                Leave(); Fail("Could not connect online: " + ex.Message);
            }
        }

        public void Browse()
        {
            if (!CanStart() || InLobby) return;
            rooms.Clear(); long token = Begin("Finding rooms", 30);
            _ = BrowseAsync(token, cancellation.Token);
        }
        async Task BrowseAsync(long token, CancellationToken cancel)
        {
            try {
                var result = await Idas3UnityServiceRequests.RunAsync(Idas3UnityServiceRequests.Kind.Query, () => MultiplayerService.Instance.QuerySessionsAsync(new QuerySessionsOptions {
                    Count = 50, FilterOptions = new List<FilterOption> {
                        new FilterOption(FilterField.StringIndex1, CompatibilityHash, FilterOperation.Equal),
                        new FilterOption(FilterField.StringIndex2, "room", FilterOperation.Equal),
                        new FilterOption(FilterField.AvailableSlots, "1", FilterOperation.Equal),
                        // MPS 2.4 declares MaxPlayers but its Lobby converter
                        // omits that enum; capacity is validated below instead.
                        new FilterOption(FilterField.IsLocked, "false", FilterOperation.Equal)
                    }
                }, cancel), cancel);
                if (!Current(token)) return;
                foreach (var item in result.Sessions) {
                    if (item.MaxPlayers != 2 || item.AvailableSlots != 1 || item.IsLocked || Get(item.Properties, CompatibilityKey) != CompatibilityHash || Get(item.Properties, KindKey) != "room") continue;
                    rooms.Add(new Idas3Room { Code = "ID:" + item.Id, Name = Clean(item.Name, "Race room"), HostName = Clean(Get(item.Properties, NameKey), "Driver"), Members = 1, Capacity = 2, Order = Order(item.Id) });
                }
                busy = null; Status = rooms.Count == 0 ? "No open rooms found. You can host one." : "Found " + rooms.Count + " open room(s).";
            } catch (Exception ex) { if (Current(token)) { busy = null; if (!(ex is OperationCanceledException)) Fail("Room search failed: " + ex.Message); } }
        }

        public void Poll()
        {
            if (disposed) return;
            if ((initializing || busy != null) && Now > deadline) {
                string request = initializing ? "Online initialization" : busy;
                Leave(); Fail(request + " timed out. Please retry.");
            }
            route?.Poll();
            if (session != null && !refreshing && Now >= nextRefresh) {
                refreshing = true; nextRefresh = Now + 5;
                _ = RefreshAsync(generation, session);
            }
            if (Available) activity?.Poll(ActivityRequested, PublishActivity, ActivityState, CompatibilityHash);
        }
        async Task RefreshAsync(long token, ISession target)
        {
            try { await target.RefreshAsync(); if (Current(token) && session == target) CheckMembership(); }
            catch (Exception) { /* Existing authenticated routes survive a short service outage. */ }
            finally { if (Current(token)) refreshing = false; }
        }
        void OnSessionChanged() => CheckMembership();
        void OnRemoved() { if (session != null) FailRoom("The online room has closed."); }
        void CheckMembership()
        {
            if (session == null) return;
            if (!session.IsMember || session.State == SessionState.Deleted || session.Host != hostId || session.PlayerCount > 2 || Get(session.Properties, CompatibilityKey) != CompatibilityHash) {
                FailRoom("The room membership or game build changed. Please reconnect."); return;
            }
            if (Connected && FindPlayer(session, route.RemoteId) == null) FailRoom("The other driver left the room.");
        }

        public void Send(byte[] data, bool reliable)
        {
            if (!Connected || data == null) return;
            if (data.Length < 1 || data.Length > MaxPayloadBytes) { FailRoom("A game message exceeded the online packet limit."); return; }
            route.SendGame(data, reliable);
        }
        internal void Deliver(Idas3UnityNetworkHandler source, byte[] data) { if (route == source && Connected) Message?.Invoke(data); }
        internal void Admitted(Idas3UnityNetworkHandler source)
        {
            if (route != source || session == null) return;
            busy = null; IsMatching = false;
            Status = "Connected to " + source.RemoteName + ".";
            PeerChanged?.Invoke();
        }
        internal void RouteFailed(Idas3UnityNetworkHandler source, string detail) { if (route == source) FailRoom(detail); }
        internal bool Current(long token) => !disposed && token == generation;

        public void Leave()
        {
            bool notify = Connected;
            ++generation; cancellation?.Cancel(); cancellation?.Dispose(); cancellation = null;
            initializing = false; busy = null; IsMatching = false; refreshing = false;
            var old = session; session = null;
            if (old != null) { old.Changed -= OnSessionChanged; old.RemovedFromSession -= OnRemoved; old.Deleted -= OnRemoved; }
            route?.Dispose(); route = null; hostId = "";
            Task retiring = pendingRequest; pendingRequest = Task.CompletedTask;
            cleanupBarrier = FinishCleanup(cleanupBarrier, retiring, old == null ? Task.CompletedTask : CleanupSession(old));
            Status = Available ? "Online ready." : "Online services are not connected.";
            if (notify) PeerChanged?.Invoke();
        }
        public void Dispose() { if (disposed) return; Leave(); disposed = true; liveTransports.Remove(this); Available = false; activity?.Dispose(); activity = null; }
        internal static async Task CleanupSession(ISession target)
        {
            try {
                if (target.IsHost) await Idas3UnityServiceRequests.RunAsync(Idas3UnityServiceRequests.Kind.Delete, () => target.AsHost().DeleteAsync(), CancellationToken.None);
                else if (target.IsMember) await Idas3UnityServiceRequests.RunAsync(Idas3UnityServiceRequests.Kind.Leave, () => target.LeaveAsync(), CancellationToken.None);
            }
            catch (Exception) { try { if (target.IsMember) await Idas3UnityServiceRequests.RunAsync(Idas3UnityServiceRequests.Kind.Leave, () => target.LeaveAsync(), CancellationToken.None); } catch (Exception) { } }
        }
        static async Task FinishCleanup(Task prior, Task request, Task leave)
        {
            try { await Task.WhenAll(prior, request, leave); } catch (Exception) { }
        }
        bool CanStart()
        {
            if (disposed) return false;
            if (!Available) { Initialize(); return false; }
            if (!ServiceConnected) { Fail("Online sign-in expired. Restart the game to reconnect."); return false; }
            if (IsBusy) return false;
            if (string.IsNullOrWhiteSpace(build)) { Fail("The game build identity is not ready."); return false; }
            return true;
        }
        long Begin(string description, int seconds)
        {
            cancellation?.Cancel(); cancellation?.Dispose(); cancellation = new CancellationTokenSource();
            busy = description; deadline = Now + seconds; Status = description + "…"; return ++generation;
        }
        void FailRoom(string detail) { Leave(); Fail(detail); }
        void Fail(string detail) { Status = detail; Error?.Invoke(detail); }
        internal static IReadOnlyPlayer FindPlayer(ISession target, string id) { foreach (var player in target.Players) if (player.Id == id) return player; return null; }
        internal static string Get(IReadOnlyDictionary<string, SessionProperty> properties, string key) => properties != null && properties.TryGetValue(key, out var value) ? value.Value ?? "" : "";
        internal static string Get(IReadOnlyDictionary<string, PlayerProperty> properties, string key) => properties != null && properties.TryGetValue(key, out var value) ? value.Value ?? "" : "";
        internal static string Clean(string value, string fallback)
        {
            var result = new StringBuilder(); foreach (char c in (value ?? "").Trim()) if (!char.IsControl(c) && c != '<' && c != '>' && result.Length < 48) result.Append(c);
            return result.Length == 0 ? fallback : result.ToString();
        }
        internal static string RandomToken() { var bytes = new byte[32]; using (var rng = RandomNumberGenerator.Create()) rng.GetBytes(bytes); return Convert.ToBase64String(bytes); }
        internal static string Hash(string value) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(value))).Replace("-", "").ToLowerInvariant(); }
        static ulong Order(string id) { using (var sha = SHA256.Create()) return BitConverter.ToUInt64(sha.ComputeHash(Encoding.UTF8.GetBytes(id)), 0); }
    }
}
