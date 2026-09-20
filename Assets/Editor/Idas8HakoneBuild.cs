using System;
using System.IO;
using UnityEditor;
using UnityEditor.SceneManagement;
using UnityEditor.Build.Reporting;
using UnityEngine;

public static class Idas8HakoneBuild
{
    [MenuItem("Initial D/Build Experimental Hakone")]
    public static void Build() {
        string product=PlayerSettings.productName,version=PlayerSettings.bundleVersion;
        var setup=EditorSceneManager.GetSceneManagerSetup();
        try {
            PlayerSettings.productName="Initial D Unity Hakone Test";
            PlayerSettings.bundleVersion="0.0.1-hakone-experiment";
            var scene=EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
            new GameObject("Hakone course").AddComponent<Idas8HakoneCourse>().testBuild=true;
            const string scenePath="Assets/Scenes/HakoneTest.unity";
            EditorSceneManager.SaveScene(scene,scenePath); AssetDatabase.SaveAssets();
            const string output="Builds/HakoneTest"; Directory.CreateDirectory(output);
            var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{scenePath},locationPathName=output+"/InitialDUnity.exe",target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
            if(report.summary.result!=BuildResult.Succeeded) throw new Exception("Hakone build failed: "+report.summary.result);
            string dest=output+"/InitialDUnity_Data/StreamingAssets/HAKONE"; Directory.CreateDirectory(dest);
            const string importedSource="Verification/hakone-port-20260917/unity-data";
            foreach(string file in Directory.GetFiles(importedSource,"*",SearchOption.AllDirectories)){
                string target=Path.Combine(dest,Path.GetRelativePath(importedSource,file));
                Directory.CreateDirectory(Path.GetDirectoryName(target));File.Copy(file,target,true);
            }
            foreach(string variant in new[]{"day_dry","day_wet","night_dry","night_wet"})foreach(bool reverse in new[]{false,true}){
                string title=variant.Replace('_',' ')+(reverse?" Uphill":" Downhill");
                string args=" -hakone-race"+(variant.StartsWith("night")?" -hakone-night":"")+(variant.EndsWith("wet")?" -hakone-wet":"")+(reverse?" -hakone-uphill":"");
                File.WriteAllText(Path.Combine(output,"Run Hakone "+title+".cmd"),"@echo off\r\ncd /d \"%~dp0\"\r\nstart \"\" \"InitialDUnity.exe\""+args+"\r\n");
            }
            const string source="Builds/Current/InitialDUnity_Data/StreamingAssets/IDAS3";
            File.WriteAllText(output+"/InitialDUnity_Data/StreamingAssets/d3-assets.txt",Path.GetFullPath(source));
            File.WriteAllText(output+"/READ ME.txt","HAKONE � D3 race integration test\nUses the existing Unity race host, D3 car/gearbox/contact solver, controls, cameras, sound and HUD.\nOriginal Stage 8 day/dry, day/wet, night/dry and night/wet scenery, skies and road boundaries. Contact triangles are converted from the extracted road strip, not Stage 8's collision executable. Myogi supplies dry/wet vehicle handling; Akina supplies vehicle lighting tables. D3 rain, spray and projected headlights are reused.\nNormal game controls and pause/options apply. R restarts. Condition launchers select all four conditions and downhill/uphill.\nStage 8 reflection/scattering shader parity is not claimed. Local test references the existing main-build D3 assets.\nSeparate test saves. Hakone has its own course/model/personal records, checkpoint splits, D3 results/points/tuning, route-based coaching, local rankings and Continue flow. Original D3 personal-record slots are preserved. Hakone is a separate Time Attack course in this test build; launch normally for course selection.\n");
        } finally {
            PlayerSettings.productName=product; PlayerSettings.bundleVersion=version; AssetDatabase.SaveAssets();
            if(!Application.isBatchMode) EditorSceneManager.RestoreSceneManagerSetup(setup);
        }
    }
}
