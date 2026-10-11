using System;
using System.Text;
using Epic.OnlineServices.Platform;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // EOS game-client credentials are distributed with the executable. Their
    // policy must grant only player Connect, Lobby and P2P actions; never put an
    // organization/admin/service-account credential in this resource.
    [Serializable]
    public sealed class Idas3EosOnlineConfig
    {
        public string ProductId = "", SandboxId = "", DeploymentId = "";
        public string ClientId = "", ClientSecret = "";

        public static Idas3EosOnlineConfig Load()
        {
            var asset = Resources.Load<TextAsset>("Idas3EosOnline");
            var value = asset == null ? null : JsonUtility.FromJson<Idas3EosOnlineConfig>(asset.text);
            if (value == null || string.IsNullOrWhiteSpace(value.ProductId) || string.IsNullOrWhiteSpace(value.SandboxId) ||
                string.IsNullOrWhiteSpace(value.DeploymentId) || string.IsNullOrWhiteSpace(value.ClientId) || string.IsNullOrWhiteSpace(value.ClientSecret))
                throw new InvalidOperationException("Online service settings are missing from this build.");
            Validate(value.ProductId, PlatformInterface.OPTIONS_PRODUCTID_MAX_LENGTH, "product ID");
            Validate(value.SandboxId, PlatformInterface.OPTIONS_SANDBOXID_MAX_LENGTH, "sandbox ID");
            Validate(value.DeploymentId, PlatformInterface.OPTIONS_DEPLOYMENTID_MAX_LENGTH, "deployment ID");
            Validate(value.ClientId, PlatformInterface.CLIENTCREDENTIALS_CLIENTID_MAX_LENGTH, "client ID");
            Validate(value.ClientSecret, PlatformInterface.CLIENTCREDENTIALS_CLIENTSECRET_MAX_LENGTH, "game-client credential");
            return value;
        }
        static void Validate(string value, int limit, string label)
        {
            if (Encoding.UTF8.GetByteCount(value) > limit) throw new InvalidOperationException("The online " + label + " in this build exceeds the SDK limit.");
            foreach (char c in value) if (char.IsWhiteSpace(c) || char.IsControl(c)) throw new InvalidOperationException("The online " + label + " in this build contains invalid whitespace.");
        }

        public static string AuthenticationProfile {
            get {
                var args = System.Environment.GetCommandLineArgs();
                int index = Array.IndexOf(args, "-idas3-online-profile");
                if (index < 0) return "default";
                if (index + 1 >= args.Length) throw new ArgumentException("Invalid online test profile.");
                string value = args[index + 1];
                if (value.Length < 1 || value.Length > 30) throw new ArgumentException("Invalid online test profile.");
                foreach (char c in value) if (!char.IsLetterOrDigit(c) && c != '-' && c != '_') throw new ArgumentException("Invalid online test profile.");
                return value;
            }
        }

        internal static bool DiagnosticRun {
            get {
                var args = System.Environment.GetCommandLineArgs();
                return AuthenticationProfile != "default" && (Array.IndexOf(args, "-idas3-online-probe") >= 0 ||
                    Array.IndexOf(args, "-idas3-multiplayer-smoke") >= 0 &&
                    (Array.IndexOf(args, "-idas3-multiplayer-p2p-check") >= 0 || Array.IndexOf(args, "-idas3-multiplayer-relay-check") >= 0 ||
                     Array.IndexOf(args, "-idas3-multiplayer-online-check") >= 0 || Array.IndexOf(args, "-idas3-multiplayer-unity-check") >= 0) ||
                    Array.IndexOf(args, "-idas3-online-controller-smoke") >= 0 &&
                    (Array.IndexOf(args, "-idas3-online-controller-online") >= 0 || Array.IndexOf(args, "-idas3-online-controller-unity") >= 0));
            }
        }
        internal static bool ForceRelay => DiagnosticRun && Array.IndexOf(System.Environment.GetCommandLineArgs(), "-idas3-probe-force-relay") >= 0;

        public static string PlayerName()
        {
            var args = System.Environment.GetCommandLineArgs();
            if (DiagnosticRun) {
                int role = Array.IndexOf(args, "-idas3-online-probe");
                if (role >= 0 && role + 1 < args.Length && (args[role + 1] == "host" || args[role + 1] == "join")) return "Online probe " + args[role + 1];
                role = Array.IndexOf(args, "-idas3-multiplayer-role");
                if (role >= 0 && role + 1 < args.Length && (args[role + 1] == "host" || args[role + 1] == "join")) return args[role + 1] == "host" ? "SMOKE HOST" : "SMOKE JOIN";
            }
            // Services may initialize before the first-use name prompt. The
            // session prevents matchmaking until a chosen name has been saved.
            string name = Idas3OnlineUsername.Saved;
            return name.Length == 0 ? "Driver" : name;
        }
    }
}
