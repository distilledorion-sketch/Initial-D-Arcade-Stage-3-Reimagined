using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Profiling;

// Opt-in private diagnostic only. Observe after imported scenery LateUpdate,
// then explicitly render the hidden target. No per-frame files or snapshots.
[DefaultExecutionOrder(32000)]
public sealed class Idas3TransitionProfile : MonoBehaviour
{
    [Serializable] private struct Row {
        public int unityFrame,section,stage,phase,course,meshes,uiTextures,depthCapacity,renderedCameras,importLoads;
        public uint flags,ranges,vertices;
        public ulong sourceFrame,ticks;
        public double wallMs,nativeMs,sceneMs,uiMs,networkMs,commandMs,importMs,specialMs,renderMs;
        public bool checkpoint;
    }
    [Serializable] private class Snapshot {
        public string name,importCourse,importVariant;
        public int unityFrame,course,stage,phase,importMeshes,importMaterials,importTextures,specialMeshes,specialMaterials,specialTextures;
        public int unityMeshes,unityMaterials,unityTextures,uiTextures,sceneTextures,depthMaterials,depthCapacity;
        public long unityAllocated,unityReserved,monoUsed,monoHeap,importResourceBytes,specialResourceBytes;
        public ulong sourceFrame,ticks;
    }
    [Serializable] private class Report {
        public bool passed;public string error;
        public string scope="Isolated fixed-step track switching. Manual offscreen 2560x1080 AA4 render after all ordinary LateUpdates and a one-pixel GPU completion readback. Wall times include real transition work, but not ordinary presentation. Checkpoint frames and their following frame are marked and must be excluded from timing. Unity resource sizes are engine estimates, not process RAM. No forced GC or UnloadUnusedAssets.";
        public string[] sections;public Row[] frames;public Snapshot[] snapshots;
        public Idas8HakoneCourse.LoadTimings[] loads;
        public int verifiedPreparedShapes;public long verifiedPreparedTriangles;
    }
    private Idas3SceneGame host;
    private Idas3SceneRenderer scene;
    private Idas3UnityUi ui;
    private Idas8HakoneCourse imported;
    private IdasSpecialStageEnnaCourse special;
    private RenderTexture target;
    private Texture2D completionPixel;
    private string root;
    private readonly List<Camera> cameras=new List<Camera>();
    private readonly List<string> sections=new List<string>();
    private readonly List<Snapshot> snapshots=new List<Snapshot>();
    private readonly List<Idas8HakoneCourse.LoadTimings> loads=new List<Idas8HakoneCourse.LoadTimings>();
    private Row[] rows;
    private int count,section,skip,actualRenders;
    private double previous;
    private bool running,rendering,stopped;
    public string Failure {get;private set;}

    internal void Initialize(Idas3SceneGame game,RenderTexture output,string directory)
    {
        host=game;target=output;root=directory;scene=host.GetComponent<Idas3SceneRenderer>();ui=host.GetComponent<Idas3UnityUi>();
        var arguments=Environment.GetCommandLineArgs();int cycleOption=Array.IndexOf(arguments,"-idas3-transition-cycles"),cycles=1;
        if(cycleOption>=0&&(cycleOption+1>=arguments.Length||!int.TryParse(arguments[cycleOption+1],out cycles)||cycles<1||cycles>3))
            throw new ArgumentException("-idas3-transition-cycles requires an integer from 1 to 3.");
        rows=new Row[checked(30000*cycles)];
        imported=FindAnyObjectByType<Idas8HakoneCourse>();special=FindAnyObjectByType<IdasSpecialStageEnnaCourse>();
        foreach(var camera in Resources.FindObjectsOfTypeAll<Camera>())
            if(camera.gameObject.activeInHierarchy&&camera.targetTexture==target)cameras.Add(camera);
        cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));
        if(!cameras.Contains(scene.MainCamera)||!cameras.Contains(scene.MirrorCamera))throw new InvalidOperationException("Transition stack is missing a world camera.");
        completionPixel=new Texture2D(1,1,TextureFormat.RGBA32,false);
        Section("setup");previous=Time.realtimeSinceStartupAsDouble;running=true;
        Camera.onPostRender+=Rendered;
    }
    internal void Section(string label){sections.Add(label);section=sections.Count-1;}
    private int Course => (host.Status.flags&IdasSpecialStageEnnaCourse.SceneFlag)!=0?IdasSpecialStageEnnaCourse.CourseId(host.Status.flags):
        (host.Status.flags&16384u)!=0?Idas8HakoneCourse.CourseId(host.Status.flags):host.Status.course;
    private bool Active(Camera camera)=>camera!=scene.MirrorCamera||scene.ShowGameHud&&scene.CurrentFrame.viewCount==2;
    internal void RestoreCameras(){foreach(var camera in cameras)camera.enabled=Active(camera);}
    private void Rendered(Camera camera){if(rendering&&cameras.Contains(camera))++actualRenders;}
    private void LateUpdate()
    {
        if(!running||!host.Ready)return;
        try{
            foreach(var camera in cameras)camera.enabled=false;
            double start=Time.realtimeSinceStartupAsDouble;int expected=0;actualRenders=0;rendering=true;
            try{foreach(var camera in cameras)if(Active(camera)){camera.Render();++expected;}}
            finally{rendering=false;}
            if(actualRenders!=expected)throw new InvalidOperationException("Transition cameras skipped or duplicated a render.");
            var previousTarget=RenderTexture.active;
            try{RenderTexture.active=target;completionPixel.ReadPixels(new Rect(0,0,1,1),0,0,false);}
            finally{RenderTexture.active=previousTarget;}
            double now=Time.realtimeSinceStartupAsDouble;
            if(count==rows.Length)throw new InvalidOperationException("Transition trace capacity exceeded.");
            var state=host.Status;var frame=scene.CurrentFrame;
            rows[count++]=new Row{unityFrame=Time.frameCount,section=section,stage=state.frontendStage,phase=state.racePhase,course=Course,flags=state.flags,
                sourceFrame=state.renderedFrames,ticks=state.simulationTicks,ranges=frame.rangeCount,vertices=frame.vertexCount,meshes=scene.ActiveMeshCount,
                uiTextures=ui.SourceTextureCount,depthCapacity=scene.DepthBatchCapacityVertices,renderedCameras=expected,wallMs=(now-previous)*1000,
                nativeMs=host.NativeStepMilliseconds,sceneMs=host.RendererMilliseconds,uiMs=host.UiMilliseconds,networkMs=host.NetworkMilliseconds,
                commandMs=host.CommandMilliseconds,importMs=imported!=null?imported.DiagnosticUpdateMs:0,specialMs=special!=null?special.DiagnosticUpdateMs:0,
                importLoads=imported!=null?imported.DiagnosticLoadCount:0,renderMs=(now-start)*1000,checkpoint=skip>0};
            previous=now;if(skip>0)--skip;
            if(imported!=null&&loads.Count<imported.DiagnosticLoadCount)loads.Add(imported.DiagnosticLoad);
        }catch(Exception error){Failure=error.ToString();running=false;}
    }
    internal void Checkpoint(string name)
    {
        if(Failure!=null)throw new InvalidOperationException(Failure);
        skip=2;
        Idas3ResourceCensus.Capture(root,name);
        var state=host.Status;
        snapshots.Add(new Snapshot{name=name,unityFrame=Time.frameCount,course=Course,stage=state.frontendStage,phase=state.racePhase,sourceFrame=state.renderedFrames,ticks=state.simulationTicks,
            importCourse=imported!=null?imported.LoadedCourse:null,importVariant=imported!=null?imported.LoadedVariant:null,
            importMeshes=imported!=null?imported.RetainedMeshCount:0,importMaterials=imported!=null?imported.RetainedMaterialCount:0,importTextures=imported!=null?imported.RetainedTextureCount:0,
            importResourceBytes=imported!=null?imported.RetainedResourceBytes():0,
            specialMeshes=special!=null?special.RetainedMeshCount:0,specialMaterials=special!=null?special.RetainedMaterialCount:0,specialTextures=special!=null?special.RetainedTextureCount:0,
            specialResourceBytes=special!=null?special.RetainedResourceBytes():0,
            unityMeshes=Resources.FindObjectsOfTypeAll<Mesh>().Length,unityMaterials=Resources.FindObjectsOfTypeAll<Material>().Length,unityTextures=Resources.FindObjectsOfTypeAll<Texture2D>().Length,
            uiTextures=ui.SourceTextureCount,sceneTextures=scene.UploadedTextureCount,depthMaterials=scene.DepthMaterialCount,depthCapacity=scene.DepthBatchCapacityVertices,
            unityAllocated=Profiler.GetTotalAllocatedMemoryLong(),unityReserved=Profiler.GetTotalReservedMemoryLong(),monoUsed=Profiler.GetMonoUsedSizeLong(),monoHeap=Profiler.GetMonoHeapSizeLong()});
    }
    internal void StopAndSave(bool passed,string error)
    {
        if(stopped)return;stopped=true;running=false;Camera.onPostRender-=Rendered;
        var data=new Row[count];Array.Copy(rows,data,count);
        File.WriteAllText(Path.Combine(root,"transitions.json"),JsonUtility.ToJson(new Report{passed=passed&&Failure==null,error=error??Failure,sections=sections.ToArray(),frames=data,snapshots=snapshots.ToArray(),loads=loads.ToArray(),verifiedPreparedShapes=Idas8HakoneCourse.VerifiedPreparedShapes,verifiedPreparedTriangles=Idas8HakoneCourse.VerifiedPreparedTriangles},true));
    }
    private void OnDestroy(){Camera.onPostRender-=Rendered;if(completionPixel!=null)Destroy(completionPixel);}
}
