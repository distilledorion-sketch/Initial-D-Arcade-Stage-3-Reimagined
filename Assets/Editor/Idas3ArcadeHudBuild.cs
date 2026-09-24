using System;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEngine;

public static class Idas3ArcadeHudBuild
{
    public static void VerifyAndBuild()
    {
        Idas3HudCustomizationChecks.Run();
        if(Marshal.SizeOf<Idas3ArcadeHud.Telemetry>()!=40)throw new Exception("HUD telemetry ABI size mismatch");
        if(!Idas3ArcadeHud.Available)throw new Exception("Missing HUD textures");
        foreach(string name in new[]{"ArcadeHud","ArcadeHudPreview"}){
            var shader=Resources.Load<Shader>(name);
            if(shader==null||ShaderUtil.ShaderHasError(shader))throw new Exception("Invalid HUD shader: "+name);
        }
        Idas3Build.RebuildWindowsPlayer();
    }
}
