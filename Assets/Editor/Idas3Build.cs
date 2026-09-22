using System;
using System.Diagnostics;
using System.IO;
using UnityEditor;
using UnityEditor.Build.Reporting;
using UnityEditor.SceneManagement;
using UnityEngine;
using UnityEngine.Rendering;
using Debug = UnityEngine.Debug;

[InitializeOnLoad]
public static class Idas3Build
{
    static Idas3Build()
    {
        EditorApplication.delayCall += () => {
            if (!EditorApplication.isPlayingOrWillChangePlaymode && !File.Exists("Assets/Scenes/InitialDUnityScene.unity")) Configure();
        };
        AssemblyReloadEvents.beforeAssemblyReload += StopHosts;
        EditorApplication.playModeStateChanged += state => {
            if (state == PlayModeStateChange.ExitingPlayMode) StopHosts();
        };
    }

    private static void StopHosts()
    {
        foreach (var host in UnityEngine.Object.FindObjectsByType<Idas3Game>(FindObjectsSortMode.None)) host.StopNative();
        foreach (var host in UnityEngine.Object.FindObjectsByType<Idas3SceneGame>(FindObjectsSortMode.None)) host.StopNative();
    }

    [MenuItem("Initial D/Configure Project")]
    public static void Configure()
    {
        PlayerSettings.companyName = "Chris";
        PlayerSettings.productName = "Initial D Unity";
        PlayerSettings.bundleVersion = "0.3.95-community-replays.24";
        PlayerSettings.colorSpace = ColorSpace.Gamma;
        PlayerSettings.allowUnsafeCode = true;
        PlayerSettings.defaultScreenWidth = 1280;
        PlayerSettings.defaultScreenHeight = 720;
        PlayerSettings.fullScreenMode = FullScreenMode.Windowed;
        PlayerSettings.resizableWindow = true;
        PlayerSettings.runInBackground = true;
        PlayerSettings.enableFrameTimingStats = true;
        // Imported materials are created at runtime, so scene scanning cannot
        // discover their instanced variants. Keep them in standalone builds.
        var graphics = new SerializedObject(AssetDatabase.LoadAllAssetsAtPath("ProjectSettings/GraphicsSettings.asset")[0]);
        graphics.FindProperty("m_InstancingStripping").intValue = 2; // Keep All
        graphics.ApplyModifiedPropertiesWithoutUndo();
        PlayerSettings.SetUseDefaultGraphicsAPIs(BuildTarget.StandaloneWindows64, false);
        PlayerSettings.SetGraphicsAPIs(BuildTarget.StandaloneWindows64, new[] { GraphicsDeviceType.Direct3D11 });
        PlayerSettings.SetScriptingBackend(UnityEditor.Build.NamedBuildTarget.Standalone, ScriptingImplementation.Mono2x);
        QualitySettings.vSyncCount = 1;
        QualitySettings.antiAliasing = 4;
        // Per-texture anisotropy. Without this the anisoLevel each original
        // material asks for is ignored and angled surfaces alias.
        QualitySettings.anisotropicFiltering = AnisotropicFiltering.ForceEnable;
        GraphicsSettings.defaultRenderPipeline = null;
        int previousQuality = QualitySettings.GetQualityLevel();
        for (int level = 0; level < QualitySettings.names.Length; ++level)
        {
            QualitySettings.SetQualityLevel(level, false);
            QualitySettings.renderPipeline = null;
        }
        QualitySettings.SetQualityLevel(previousQuality, false);
        var settingsAssets = AssetDatabase.LoadAllAssetsAtPath("ProjectSettings/ProjectSettings.asset");
        if (settingsAssets.Length > 0)
        {
            var settings = new SerializedObject(settingsAssets[0]);
            var inputHandler = settings.FindProperty("activeInputHandler");
            // Keep legacy keyboard/menu input alongside HID gamepad support.
            if (inputHandler != null) { inputHandler.intValue = 2; settings.ApplyModifiedPropertiesWithoutUndo(); }
        }
        var plugin = AssetImporter.GetAtPath("Assets/Plugins/x86_64/Idas3Unity.dll") as PluginImporter;
        if (plugin != null)
        {
            plugin.SetCompatibleWithAnyPlatform(false);
            plugin.SetCompatibleWithEditor(true);
            plugin.SetEditorData("OS", "Windows");
            plugin.SetEditorData("CPU", "x86_64");
            plugin.SetCompatibleWithPlatform(BuildTarget.StandaloneWindows64, true);
            plugin.SetPlatformData(BuildTarget.StandaloneWindows64, "CPU", "x86_64");
            plugin.SaveAndReimport();
        }
        Directory.CreateDirectory("Assets/Scenes");
        const string scenePath = "Assets/Scenes/InitialDUnityScene.unity";
        if (!File.Exists(scenePath))
        {
            // A fresh batch editor starts with an unsaved Untitled scene.
            // Unity forbids additive scene creation in that state; this owned
            // batch process has no user edits to preserve.
            var scene = EditorSceneManager.NewScene(NewSceneSetup.EmptyScene,
                Application.isBatchMode ? NewSceneMode.Single : NewSceneMode.Additive);
            var host = new GameObject("Initial D — Unity game");
            UnityEngine.SceneManagement.SceneManager.MoveGameObjectToScene(host, scene);
            host.AddComponent<Camera>();
            host.AddComponent<Idas3SceneGame>();
            EditorSceneManager.SaveScene(scene, scenePath);
            if (!Application.isBatchMode) EditorSceneManager.CloseScene(scene, true);
        }
        EditorBuildSettings.scenes = new[] { new EditorBuildSettingsScene(scenePath, true) };
        AssetDatabase.SaveAssets();
        foreach(string path in Directory.GetFiles("Assets/Resources/Challenger","*.png")){
            var texture=AssetImporter.GetAtPath(path.Replace('\\','/')) as TextureImporter;
            if(texture==null)continue;
            texture.textureCompression=TextureImporterCompression.Uncompressed;
            texture.mipmapEnabled=false;texture.alphaIsTransparency=true;
            texture.wrapMode=TextureWrapMode.Clamp;texture.filterMode=FilterMode.Bilinear;
            texture.SaveAndReimport();
        }
        Debug.Log("Initial D: Windows x64 / Direct3D11 / gamma display configured.");
    }

    [MenuItem("Initial D/Build Windows Player")]
    public static void BuildWindows()
    {
        // Preserve existing build-script callers without replacing the frozen
        // framebuffer-host reference player in Builds/Windows.
        BuildUnityScene();
    }

    public static void RebuildWindowsPlayer(){
        const string output="Builds/Current/InitialDUnity.exe";
        if(!File.Exists(output))throw new FileNotFoundException("Build the complete Windows package first.",output);
        Configure();
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Windows player build failed: "+report.summary.result);
    }
    public static void RebuildWindowsScripts(){
        const string output="Builds/Current/InitialDUnity.exe";
        if(!File.Exists(output))throw new FileNotFoundException("Build the complete Windows package first.",output);
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.BuildScriptsOnly});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Windows script build failed: "+report.summary.result);
    }

    public static void BuildSadamineStaging(){Configure();BuildPlayer("Builds/Staging/InitialDUnity.exe","Assets/Scenes/InitialDUnityScene.unity");}
    public static void BuildMenuPresentationDiagnostic(){Configure();BuildPlayer("Builds/MenuPresentationCheck/InitialDUnity.exe","Assets/Scenes/InitialDUnityScene.unity");}
    public static void RebuildMenuPresentationDiagnosticScripts(){
        const string output="Builds/MenuPresentationCheck/InitialDUnity.exe";
        if(!File.Exists(output))throw new FileNotFoundException("Build the complete diagnostic package first.",output);
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.BuildScriptsOnly});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Diagnostic script build failed: "+report.summary.result);
    }
    public static void RebuildSadamineStagingScripts(){
        const string output="Builds/Staging/InitialDUnity.exe";
        if(!File.Exists(output))throw new FileNotFoundException("Build the complete staged package first.",output);
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.BuildScriptsOnly});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Staged script build failed: "+report.summary.result);
    }
    public static void RebuildSadamineStagingPlayer(){
        // A release version change needs player data rebuilt too; scripts-only
        // builds retain Application.version from their original full build.
        const string output="Builds/Staging/InitialDUnity.exe";
        if(!File.Exists(output))throw new FileNotFoundException("Build the complete staged package first.",output);
        Configure();
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},locationPathName=output,
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.None});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Staged player build failed: "+report.summary.result);
    }
    public static void BuildPerformanceStaging(){
        // Existing isolated package supplies the unchanged runtime assets.
        Configure();
        var report=BuildPipeline.BuildPlayer(new BuildPlayerOptions{
            scenes=new[]{"Assets/Scenes/InitialDUnityScene.unity"},
            locationPathName="Builds/Staging/InitialDUnity.exe",
            target=BuildTarget.StandaloneWindows64,options=BuildOptions.Development});
        if(report.summary.result!=BuildResult.Succeeded)throw new InvalidOperationException("Performance build failed");
    }

    public static void BuildUnityScene()
    {
        Configure();
        const string scenePath = "Assets/Scenes/InitialDUnityScene.unity";
        EditorBuildSettings.scenes = new[] { new EditorBuildSettingsScene(scenePath,true) };
        AssetDatabase.SaveAssets();
        BuildPlayer("Builds/Current/InitialDUnity.exe",scenePath);
    }

    public static void BuildTestPlayer()
    {
        Configure();
        // Separate output AND saves: testing never changes the released player
        // or the user's normal progress, options, or controller bindings.
        PlayerSettings.productName = "Initial D Unity Test";
        AssetDatabase.SaveAssets();
        BuildPlayer("Builds/ControllerAudioLobbyTest/InitialDUnity.exe", "Assets/Scenes/InitialDUnityScene.unity");
        string readme = "Builds/ControllerAudioLobbyTest/READ ME.txt";
        File.WriteAllText(readme, "SEPARATE TEST BUILD - Desktop Current has not been updated.\n" +
            "This build uses its own saves and settings under Initial D Unity Test.\n" +
            "Both multiplayer players must run this same test build.\n\n" +
            File.ReadAllText(readme).Replace("INITIAL D UNITY - CURRENT BUILD", "INITIAL D UNITY - TEST BUILD")
                .Replace("LocalLow\\Chris\\Initial D Unity\\", "LocalLow\\Chris\\Initial D Unity Test\\"));
    }

    public static void BuildInputFixPlayer()
    {
        Configure();var product=PlayerSettings.productName;var version=PlayerSettings.bundleVersion;
        try {
            PlayerSettings.productName="Initial D Unity Input Test";
            PlayerSettings.bundleVersion="0.3.75-input-fix.1";AssetDatabase.SaveAssets();
            BuildPlayer("Builds/InputFixTest/InitialDUnity.exe","Assets/Scenes/InitialDUnityScene.unity");
            File.WriteAllText("Builds/InputFixTest/READ ME.txt","INPUT FIX TEST - Desktop Current unchanged.\nOptions exit/input recovery and faster keyboard steering.\nExperimental multiplayer authority is NOT enabled.\nThis build uses separate saves under Initial D Unity Input Test.\n");
        }finally {PlayerSettings.productName=product;PlayerSettings.bundleVersion=version;AssetDatabase.SaveAssets();}
    }

    public static void BuildAuthorityPlayer()
    {
        Configure();
        var product=PlayerSettings.productName;var version=PlayerSettings.bundleVersion;
        try {
            PlayerSettings.productName = "Initial D Unity Online Test";
            PlayerSettings.bundleVersion = "0.3.75-authority.1";
            AssetDatabase.SaveAssets();
            BuildPlayer("Builds/OnlineAuthorityTest/InitialDUnity.exe", "Assets/Scenes/InitialDUnityScene.unity");
            foreach(var name in new[]{"READ ME.txt","MULTIPLAYER TEST.txt"})File.Copy("Tools/OnlineAuthority-Test.txt",Path.Combine("Builds/OnlineAuthorityTest",name),true);
            File.WriteAllText("Builds/OnlineAuthorityTest/authority_test.txt", "Experimental input-authority/collision build. Separate saves. Both players must use this build.\n");
        } finally {
            PlayerSettings.productName=product;PlayerSettings.bundleVersion=version;AssetDatabase.SaveAssets();
        }
    }

    private static void BuildPlayer(string relativeOutput,string scenePath)
    {
        string project = Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
        if (!File.Exists(Path.Combine(project, "Assets/Plugins/x86_64/Idas3Unity.dll")))
            throw new FileNotFoundException("Run Build Native.cmd before building Unity.");
        string output = Path.Combine(project, relativeOutput);
        Directory.CreateDirectory(Path.GetDirectoryName(output));
        var report = BuildPipeline.BuildPlayer(new BuildPlayerOptions {
            scenes = new[] { scenePath }, locationPathName = output,
            target = BuildTarget.StandaloneWindows64, options = BuildOptions.None
        });
        if (report.summary.result != BuildResult.Succeeded)
            throw new InvalidOperationException("Unity build failed: " + report.summary.result);
        // Keep the imported course beside the original runtime data, so the
        // shipped player does not depend on the experimental build or source drive.
        string hakoneSource=Path.Combine(project,"RuntimeAssets/HAKONE");
        string hakoneDestination=Path.Combine(Path.GetDirectoryName(output),"InitialDUnity_Data/StreamingAssets/HAKONE");
        foreach(string file in Directory.GetFiles(hakoneSource,"*",SearchOption.AllDirectories)){
            string target=Path.Combine(hakoneDestination,Path.GetRelativePath(hakoneSource,file));
            Directory.CreateDirectory(Path.GetDirectoryName(target));File.Copy(file,target,true);
        }
        string sadamineSource=Path.Combine(project,"RuntimeAssets/SADAMINE");
        string sadamineDestination=Path.Combine(Path.GetDirectoryName(output),"InitialDUnity_Data/StreamingAssets/SADAMINE");
        foreach(string file in Directory.GetFiles(sadamineSource,"*",SearchOption.AllDirectories)){
            string target=Path.Combine(sadamineDestination,Path.GetRelativePath(sadamineSource,file));
            Directory.CreateDirectory(Path.GetDirectoryName(target));File.Copy(file,target,true);
        }
        string ennaSource=Path.Combine(project,"RuntimeAssets/ENNA");
        string ennaDestination=Path.Combine(Path.GetDirectoryName(output),"InitialDUnity_Data/StreamingAssets/ENNA");
        foreach(string file in Directory.GetFiles(ennaSource,"*",SearchOption.AllDirectories)){
            string target=Path.Combine(ennaDestination,Path.GetRelativePath(ennaSource,file));
            Directory.CreateDirectory(Path.GetDirectoryName(target));File.Copy(file,target,true);
        }
        // Post-build staging avoids importing 14,376 native assets into Unity.
        string steamApi = Path.Combine(Path.GetDirectoryName(output), "InitialDUnity_Data/Plugins/x86_64/steam_api64.dll");
        if (!File.Exists(steamApi)) throw new FileNotFoundException("Steam's official Windows x64 runtime was not included in the player.", steamApi);
        File.WriteAllText(Path.Combine(Path.GetDirectoryName(output), "steam_appid.txt"), "480\n", new System.Text.UTF8Encoding(false));
        string destination = Path.Combine(Path.GetDirectoryName(output), Path.GetFileNameWithoutExtension(output)+"_Data/StreamingAssets/IDAS3");
        string script = Path.Combine(project, "Tools/Stage-GameData.ps1");
        var start = new ProcessStartInfo("powershell.exe") {
            Arguments = "-NoProfile -ExecutionPolicy Bypass -File \"" + script + "\" -DestinationRoot \"" + destination + "\"",
            UseShellExecute = false, CreateNoWindow = true, WorkingDirectory = project,
            RedirectStandardOutput = true, RedirectStandardError = true
        };
        // A PowerShell 7 caller can leave its module path in the editor's
        // environment; the Windows PowerShell 5 staging process needs its own.
        start.EnvironmentVariables.Remove("PSModulePath");
        using (var process = Process.Start(start))
        {
            var stdout = process.StandardOutput.ReadToEndAsync();
            var stderr = process.StandardError.ReadToEndAsync();
            process.WaitForExit();
            string stagingLog = stdout.GetAwaiter().GetResult() + stderr.GetAwaiter().GetResult();
            Directory.CreateDirectory(Path.Combine(project, "Logs"));
            File.WriteAllText(Path.Combine(project, "Logs/staging.log"), stagingLog);
            if (process.ExitCode != 0) throw new InvalidOperationException("Game data staging failed: " + stagingLog);
        }
        File.Copy(Path.Combine(project, "Tools/Player-Readme.txt"),
            Path.Combine(Path.GetDirectoryName(output), "READ ME.txt"), true);
        File.Copy(Path.Combine(project, "Tools/Replay Viewer.cmd"),
            Path.Combine(Path.GetDirectoryName(output), "Replay Viewer.cmd"), true);
        File.Copy(Path.Combine(project, "Tools/Multiplayer-Test.txt"),
            Path.Combine(Path.GetDirectoryName(output), "MULTIPLAYER TEST.txt"), true);
        Debug.Log("Initial D Unity player and all game assets: " + output);
    }
}

