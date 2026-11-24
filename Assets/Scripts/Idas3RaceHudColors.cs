using System;
using System.IO;
using UnityEngine;

// Group numbers come from the race compositor's HUD layout.
internal static class Idas3RaceHudColors
{
    [Serializable]
    private sealed class Settings
    {
        public string timer = "", meter = "", records = "", mirrorFrame = "", minimap = "";
        public string legendOpponent = "", onlineOpponent = "", timeExtended = "";
    }

    internal static Color[] Load(string path)
    {
        var colors = new Color[16];
        for (int i = 0; i < colors.Length; ++i) colors[i] = Color.white;
        try {
            var file = new FileInfo(path);
            if (!file.Exists) return colors;
            if (file.Length > 16384) throw new InvalidDataException("Maximum size is 16 KB.");
            var config = JsonUtility.FromJson<Settings>(File.ReadAllText(path));
            if (config == null) return colors;
            Apply(colors, 1, config.timer);
            Apply(colors, 2, config.meter);
            Apply(colors, 3, config.records);
            Apply(colors, 4, config.mirrorFrame);
            Apply(colors, 5, config.minimap);
            Apply(colors, 6, config.legendOpponent);
            Apply(colors, 7, config.onlineOpponent);
            Apply(colors, 9, config.timeExtended);
        } catch (Exception ex) when (ex is IOException || ex is UnauthorizedAccessException || ex is ArgumentException) {
            Debug.LogWarning("Skipped Race Colors.json: " + ex.Message);
        }
        return colors;
    }

    private static void Apply(Color[] colors, int group, string text)
    {
        if (string.IsNullOrEmpty(text)) return;
        if (text.Length == 7 && text[0] == '#' && ColorUtility.TryParseHtmlString(text, out var color))
            colors[group] = color;
        else
            Debug.LogWarning("Skipped an invalid race HUD color. Use #RRGGBB.");
    }
}
