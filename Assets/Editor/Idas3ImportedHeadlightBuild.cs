using System;
using System.IO;
using UnityEditor;
using UnityEngine;

public static class Idas3ImportedHeadlightBuild
{
    public static void Build(){
        ShaderUtil.allowAsyncCompilation=false;
        var shader=Resources.Load<Shader>("Idas3Headlight");
        if(shader==null||!shader.isSupported)throw new Exception("Projected headlight shader unavailable");
        foreach(var error in ShaderUtil.GetShaderMessages(shader))
            if(error.severity.ToString()=="Error")throw new Exception(error.message);
        const string player="Builds/Current/InitialDUnity.exe";
        if(!File.Exists(player))throw new FileNotFoundException("A staged player is required",player);
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=player,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.CompressWithLz4HC});
        if(report.summary.result!=UnityEditor.Build.Reporting.BuildResult.Succeeded)throw new Exception("Headlight player build failed");
    }
}
