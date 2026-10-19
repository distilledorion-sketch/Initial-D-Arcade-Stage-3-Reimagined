using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;

public static class Idas3UpdateFailureBuild {
    public static void Build(){
        Idas3UpdateFailureChecks.Run();
        const string scenePath="Assets/Scenes/UpdateFailureDiagnostic.unity";
        if(File.Exists(scenePath))throw new IOException("Diagnostic scene already exists.");
        try{
            var scene=EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
            var camera=new GameObject("Camera").AddComponent<Camera>();camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;
            new GameObject("Updater error display").AddComponent<Idas3UpdaterFlowSmoke>();
            EditorSceneManager.SaveScene(scene,scenePath);
            var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
                scenes=new[]{scenePath},locationPathName="Verification/updater-launch-error-20261011/player/InitialDUnity.exe",
                target=BuildTarget.StandaloneWindows64,options=BuildOptions.CompressWithLz4HC});
            if(report.summary.result!=BuildResult.Succeeded)throw new Exception("Updater diagnostic build failed.");
        }finally{AssetDatabase.DeleteAsset(scenePath);}
    }
}
