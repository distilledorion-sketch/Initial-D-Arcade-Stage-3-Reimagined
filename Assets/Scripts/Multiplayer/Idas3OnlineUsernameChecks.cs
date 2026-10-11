using System;
using System.IO;

namespace Idas3.Multiplayer
{
    // Explicit checks only; normal launches never alter a player's preference.
    public static class Idas3OnlineUsernameChecks
    {
        public static int Run(string root = "Verification/online-username-20261011")
        {
            int checks = 0;
            void Check(bool value, string reason) { ++checks; if (!value) throw new Exception("Online username: " + reason); }
            foreach (string value in new[] { "Chris", "READY", "日本の運転手", "中文玩家", "Driver 123_-", "A😀B", new string('A', 16) }) {
                Check(Idas3OnlineUsername.TryNormalize(value, out string name, out string error) && name == value && error.Length == 0, "valid name " + value);
                Check(name.Length <= 32, "name fits existing handshake without truncation");
            }
            Check(Idas3OnlineUsername.TryNormalize("  Chris  ", out string trimmed, out _) && trimmed == "Chris", "trim surrounding whitespace");
            Check(Idas3OnlineUsername.TryNormalize("Cafe\u0301", out string normalized, out _) && normalized == "Café", "normalize Unicode accents");
            foreach (string value in new[] { null, "", " \t\n ", "\u0301", "a\nb", "a\0b", "<b>Name</b>", "a\u202Eb", "a\u200Bb", "a\u2028b", "\ud800", "\udc00", new string('a', 17) })
                Check(!Idas3OnlineUsername.TryNormalize(value, out _, out string error) && !string.IsNullOrEmpty(error), "reject invalid or invisible name");
            string emoji = ""; for (int i = 0; i < 16; ++i) emoji += "😀";
            Check(Idas3OnlineUsername.TryNormalize(emoji, out string maximum, out _) && maximum.Length == 32, "maximum supplementary characters stay whole");
            Check(!Idas3OnlineUsername.TryNormalize(emoji + "a", out _, out _), "scalar length limit");
            Check(Idas3OnlineUsername.DeleteLast("A😀") == "A", "delete complete supplementary character");
            Check(Idas3OnlineUsername.DeleteLast("日本") == "日", "delete Japanese character");
            Check(Idas3OnlineUsername.DeleteLast("") == "", "delete empty draft");
            Directory.CreateDirectory(root);File.WriteAllText(Path.Combine(root, "validation-checks.txt"), "PASS " + checks + " username validation and Unicode boundary checks. No live preferences modified.\n");
            return checks;
        }
    }
}
