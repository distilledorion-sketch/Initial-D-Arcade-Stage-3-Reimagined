using UnityEngine;

public static class Idas3HudPlacementBuild
{
    public static void VerifyAndBuild()
    {
        Idas3HudCustomizationChecks.Run();
        Debug.Log(Idas3HudPlacementChecks.RunChecks());
        Idas3Build.RebuildWindowsPlayer();
    }
}
