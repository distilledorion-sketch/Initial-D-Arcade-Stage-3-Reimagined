using System;
using System.Globalization;
using System.Text;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Display names are local preferences, not authentication or moderation IDs.
    internal static class Idas3OnlineUsername
    {
        internal const string PreferenceKey = "idas3.online.name";
        internal const int MaxCharacters = 16, MaxCodeUnits = MaxCharacters * 2;
        internal const string Empty = "Choose an online username before joining a battle.";
        internal const string TooLong = "Use no more than 16 characters.";
        internal const string Invalid = "Use visible characters without formatting or angle brackets.";
#if IDAS3_PORTABLE_PLAYER && UNITY_STANDALONE_WIN && !UNITY_EDITOR
        private static string cachedName;
        private static string NamePath => System.IO.Path.Combine(Idas3DiscStorage.persistentDataPath, "online-username.txt");
        internal static string Saved {
            get {
                if (cachedName != null) return cachedName;
                string value = System.IO.File.Exists(NamePath) ? System.IO.File.ReadAllText(NamePath) : "";
                return cachedName = TryNormalize(value, out var name, out _) ? name : "";
            }
        }
#else
        internal static string Saved => TryNormalize(PlayerPrefs.GetString(PreferenceKey, ""), out var name, out _) ? name : "";
#endif

        internal static bool TryNormalize(string input, out string name, out string error)
        {
            name = ""; error = Empty;
            if (string.IsNullOrWhiteSpace(input)) return false;
            string value;
            try { value = input.Trim().Normalize(NormalizationForm.FormC); }
            catch (ArgumentException) { error = Invalid; return false; }
            int count = 0; bool visible = false;
            for (int i = 0; i < value.Length; ++i) {
                var category = CharUnicodeInfo.GetUnicodeCategory(value, i);
                char c = value[i];
                if (c == '<' || c == '>' || char.IsControl(c) || category == UnicodeCategory.Format ||
                    category == UnicodeCategory.LineSeparator || category == UnicodeCategory.ParagraphSeparator ||
                    category == UnicodeCategory.Surrogate || category == UnicodeCategory.OtherNotAssigned) {
                    error = Invalid; return false;
                }
                if (char.IsHighSurrogate(c)) ++i;
                if (++count > MaxCharacters) { error = TooLong; return false; }
                visible |= !char.IsWhiteSpace(c) && category != UnicodeCategory.NonSpacingMark && category != UnicodeCategory.EnclosingMark;
            }
            if (!visible) return false;
            name = value; error = ""; return true;
        }

        internal static bool TrySave(string input, out string error)
        {
            if (!TryNormalize(input, out string name, out error)) return false;
#if IDAS3_PORTABLE_PLAYER && UNITY_STANDALONE_WIN && !UNITY_EDITOR
            string temp = NamePath + "." + Guid.NewGuid().ToString("N");
            try {
                System.IO.Directory.CreateDirectory(Idas3DiscStorage.persistentDataPath);
                System.IO.File.WriteAllText(temp, name);
                if (System.IO.File.Exists(NamePath)) System.IO.File.Replace(temp, NamePath, null);
                else System.IO.File.Move(temp, NamePath);
                cachedName = name;
                return true;
            } catch (System.IO.IOException) {
                error = "Your online username could not be saved. Please try again.";
                return false;
            } catch (UnauthorizedAccessException) {
                error = "Your online username could not be saved. Please try again.";
                return false;
            } finally {
                try { System.IO.File.Delete(temp); } catch (System.IO.IOException) { } catch (UnauthorizedAccessException) { }
            }
#else
            bool existed = PlayerPrefs.HasKey(PreferenceKey);
            string previous = PlayerPrefs.GetString(PreferenceKey, "");
            try { PlayerPrefs.SetString(PreferenceKey, name); PlayerPrefs.Save(); return true; }
            catch (Exception) {
                try { if (existed) PlayerPrefs.SetString(PreferenceKey, previous); else PlayerPrefs.DeleteKey(PreferenceKey); }
                catch (Exception) { /* A broken preference store must not crash the menu. */ }
                error = "Your online username could not be saved. Please try again."; return false;
            }
#endif
        }

        internal static string DeleteLast(string value)
        {
            if (string.IsNullOrEmpty(value)) return "";
            int length = value.Length - 1;
            if (length > 0 && char.IsLowSurrogate(value[length]) && char.IsHighSurrogate(value[length - 1])) --length;
            return value.Substring(0, length);
        }
    }
}
