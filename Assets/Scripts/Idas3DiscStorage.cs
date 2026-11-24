using System.IO;
using UnityEngine;

internal static class Idas3DiscStorage
{
    internal static string persistentDataPath => Path.Combine(Path.GetDirectoryName(Application.dataPath), "UserData");
    internal static string temporaryCachePath => Path.Combine(persistentDataPath, "Temp");
}
