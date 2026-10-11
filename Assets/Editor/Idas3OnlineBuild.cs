using System;
using System.IO;
using Idas3.Multiplayer;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.SceneManagement;

// These builds use the existing managed project and never rebuild or stage the
// native game. The probe scene contains only the cloud transport diagnostic.
public static class Idas3OnlineBuild
{
    public static void BuildProbeAndCurrent()
    {
        BuildProbe();
        BuildCurrent();
    }

    public static void BuildProbe()
    {
        BuildProbeAt("Verification/eos-online-20261011/Probe/OnlineProbe.exe");
    }

    public static void BuildSanctionsProbe()
    {
        BuildProbeAt("Verification/eos-sanctions-20261011/Probe/OnlineProbe.exe");
    }

    static void BuildProbeAt(string output)
    {
        const string scenePath="Assets/Editor/OnlineProbe.unity";
        for(int i=0;i<SceneManager.sceneCount;++i)
            if(SceneManager.GetSceneAt(i).isDirty)throw new InvalidOperationException("Save the open scene before building the online probe.");
        var prior=EditorSceneManager.GetSceneManagerSetup();
        try {
            var scene=EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
            new GameObject("Online Transport Probe").AddComponent<Idas3OnlineTransportProbe>();
            if(!EditorSceneManager.SaveScene(scene,scenePath))throw new IOException("Could not save the online probe scene.");
            Directory.CreateDirectory(Path.GetDirectoryName(output));
            Build(scenePath,output);
        }finally{
            // A headless editor can start without a loaded/active scene.
            if(prior.Length>0&&Array.Exists(prior,s=>s.isLoaded)&&Array.Exists(prior,s=>s.isActive))EditorSceneManager.RestoreSceneManagerSetup(prior);
            else EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,NewSceneMode.Single);
        }
    }

    public static void BuildCurrent()
    {
        const string output="Builds/Current/InitialDUnity.exe";
        if(!File.Exists(output))throw new FileNotFoundException("An existing staged player is required; this build does not stage native game data.",output);
        Build("Assets/Scenes/InitialDUnityScene.unity",output);
        File.Copy("Tools/Player-Readme.txt",Path.Combine(Path.GetDirectoryName(output),"READ ME.txt"),true);
        File.Copy("Tools/Multiplayer-Test.txt",Path.Combine(Path.GetDirectoryName(output),"MULTIPLAYER TEST.txt"),true);
    }

    static void Build(string scene,string output)
    {
        // Local client configuration is intentionally excluded from source
        // control. A source checkout must fail clearly instead of packaging an
        // Online button that cannot authenticate. Do not print credentials.
        try{Idas3EosOnlineConfig.Load();}
        catch(Exception){throw new InvalidOperationException("EOS player configuration is missing or invalid. Configure Assets/Resources/Idas3EosOnline.json before building the online player.");}
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
            scenes=new[]{scene},locationPathName=output,target=BuildTarget.StandaloneWindows64,
            options=BuildOptions.CompressWithLz4HC});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Online build failed: "+report.summary.result);
    }
}
