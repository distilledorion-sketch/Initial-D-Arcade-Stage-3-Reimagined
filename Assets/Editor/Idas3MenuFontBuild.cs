using System;
using System.IO;
using UnityEditor;
using UnityEditor.Build;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;

public sealed class Idas3MenuFontBuild : IPreprocessBuildWithReport
{
    public int callbackOrder=>0;
    public void OnPreprocessBuild(BuildReport report)
    {
        foreach(string language in new[]{"JP","SC"}){
            string path="Assets/Resources/Fonts/NotoSans"+language+"-Regular.otf";
            var importer=AssetImporter.GetAtPath(path) as TrueTypeFontImporter;
            if(importer==null||!importer.includeFontData||AssetDatabase.LoadAssetAtPath<Font>(path)==null)
                throw new BuildFailedException("Custom menus require bundled Noto fonts with Include Font Data enabled: "+language);
        }
    }

    // Isolated real-menu renderer. No ROM, native game initialization, personal
    // saves or installed OS fonts are needed to verify the actual text output.
    public static void BuildDiagnostic()
    {
        const string scenePath="Assets/Scenes/MenuFontDiagnostic.unity";
        if(File.Exists(scenePath))throw new IOException("Temporary font diagnostic scene already exists.");
        try{
            var scene=EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
            var camera=new GameObject("Camera").AddComponent<Camera>();
            camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;
            new GameObject("Sound Room font check").AddComponent<Idas3MenuFontSmoke>();
            EditorSceneManager.SaveScene(scene,scenePath);
            var result=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
                scenes=new[]{scenePath},locationPathName="Builds/MenuFontCheck/InitialDUnity.exe",
                target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
            if(result.summary.result!=BuildResult.Succeeded)throw new BuildFailedException("Font diagnostic build failed.");
        }finally{AssetDatabase.DeleteAsset(scenePath);}
    }
}
