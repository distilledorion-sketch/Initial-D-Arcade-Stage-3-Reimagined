using System;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Public project configuration only. Service-account keys and access tokens
    // must never be stored in a player build or in this resource.
    [Serializable]
    public sealed class Idas3UnityOnlineConfig
    {
        public string Environment = "production";
        public string QueueName = "idas3-online";
        public string Protocol = "dtls";

        public static Idas3UnityOnlineConfig Load()
        {
            var text = Resources.Load<TextAsset>("Idas3UnityOnline");
            var config = text == null ? new Idas3UnityOnlineConfig() : JsonUtility.FromJson<Idas3UnityOnlineConfig>(text.text);
            if (config == null || string.IsNullOrWhiteSpace(config.Environment) || string.IsNullOrWhiteSpace(config.QueueName))
                throw new InvalidOperationException("Online service settings are missing from this build.");
            if (config.Protocol != "dtls" && config.Protocol != "wss")
                throw new InvalidOperationException("Online service connection settings are invalid.");
            return config;
        }

        public static string AuthenticationProfile
        {
            get {
                var args = System.Environment.GetCommandLineArgs();
                for (int i = 0; i + 1 < args.Length; ++i)
                    if (args[i] == "-idas3-unity-profile") {
                        string value = args[i + 1];
                        if (value.Length < 1 || value.Length > 30) throw new ArgumentException("Invalid online test profile.");
                        foreach (char c in value) if (!char.IsLetterOrDigit(c) && c != '-' && c != '_') throw new ArgumentException("Invalid online test profile.");
                        return value;
                    }
                return "default";
            }
        }

        [DllImport("Idas3Unity", CallingConvention = CallingConvention.Cdecl)]
        static extern int Idas3SceneCopyHudDriverName([Out] byte[] destination, int capacity);

        public static string PlayerName()
        {
            var args = System.Environment.GetCommandLineArgs();
            for (int i = 0; i + 1 < args.Length; ++i)
                if (args[i] == "-idas3-online-probe" && AuthenticationProfile != "default" &&
                    (args[i + 1] == "host" || args[i + 1] == "join")) return "Online probe " + args[i + 1];
            if (Array.IndexOf(args, "-idas3-multiplayer-smoke") >= 0 &&
                Array.IndexOf(args, "-idas3-multiplayer-relay-check") >= 0 && AuthenticationProfile != "default") {
                int role = Array.IndexOf(args, "-idas3-multiplayer-role");
                if (role >= 0 && role + 1 < args.Length && (args[role + 1] == "host" || args[role + 1] == "join"))
                    return args[role + 1] == "host" ? "SMOKE HOST" : "SMOKE JOIN";
            }
            string name = PlayerPrefs.GetString("idas3.online.name", "");
            if (string.IsNullOrWhiteSpace(name)) {
                try {
                    var bytes = new byte[256];
                    int count = Idas3SceneCopyHudDriverName(bytes, bytes.Length);
                    if (count > 0 && count <= bytes.Length) name = Encoding.UTF8.GetString(bytes, 0, count);
                } catch (DllNotFoundException) { } catch (EntryPointNotFoundException) { }
            }
            if (string.IsNullOrWhiteSpace(name)) name = "Driver";
            var clean = new StringBuilder();
            foreach (char c in name.Trim()) if (!char.IsControl(c) && c != '<' && c != '>') clean.Append(c);
            string result = clean.ToString();
            return result.Length == 0 ? "Driver" : result.Substring(0, Math.Min(32, result.Length));
        }
    }
}
