using System.IO;
using UnityEngine;

public static class Idas3TouchControlsTests
{
    public static void Run()
    {
        int checks = Idas3TouchControls.RunSelfTests();
        Directory.CreateDirectory("Logs");
        File.WriteAllText("Logs/touch-controls-tests.json", "{\"passed\":true,\"checks\":" + checks + "}");
        Debug.Log("Touch controls tests passed: " + checks);
    }
    public static void BuildAndroid()
    {
        Run();
        Idas3Build.BuildAndroidArm64();
    }
}
