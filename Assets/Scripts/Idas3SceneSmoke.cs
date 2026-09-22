using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
using UnityEngine;
using Unity.Profiling;

// Opt-in integration validation; normal play neither injects input nor captures.
public sealed class Idas3SceneSmoke : MonoBehaviour
{
    private static Idas3SceneSmoke active;
    private Idas3SceneGame host;
    private string root;
    private bool frozen, finished;
    private bool realtimeAudio;
    private int held, pulse, checks;
    private uint padPulse;
    private bool legendCheck, flickerCheck, headlightCheck, attractFlicker, hiresFlicker, driveInspect, cullCheck, depthCheck, introCheck;
    private bool introWarmup, introFoliageCheck, carDoorCheck, diagnosticPoseActive;
    private bool perfCheck, perfLegend, perfNight, perfWet;
    private bool perfFullDrive,perfReverse;
    private readonly float[] driveTelemetry=new float[12];
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneCourseDriveDiagnostic(int enabled,[Out] float[] values,int count);
    private bool rivalCheck, rivalPostResult, rivalFastForward, retireCheck;
    private int rivalRecord;
    private Idas3RivalAudioProbe rivalAudio;
    private readonly List<RivalObservation> rivalObservations=new List<RivalObservation>();
    private int perfCourse=3,perfWarmup=180,perfFrames=600,perfMainRenders,perfAllRenders;
    private int PerformanceCourse => (host.Status.flags&IdasSpecialStageEnnaCourse.SceneFlag)!=0 ? 11 : (host.Status.flags&16384u)!=0 ? ((host.Status.flags&524288u)!=0?10:9) : host.Status.course;
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneModeFlowValue(int field);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneReplayCaptureDiagnostic(int enabled);
    private Camera perfMainCamera;
    private Vector3 diagnosticPoseTarget;
    private ulong introFirstRenderedFrame;
    private int depthCourse=8,doorFrame=600;
    private bool realDelta;
    private RenderTexture hires;
    private double began;
    private readonly List<Shot> shots = new List<Shot>();
    private readonly List<float> submitTimes = new List<float>();
    [Serializable] private class Shot {
        public string name;
        public int meshRenderers, cameras, lights, textures, stage, course, car, uiBuffers;
        public float speed, submitMs;
        public int screenWidth,screenHeight,uiDraws,renderWidth,renderHeight;
        public float cameraAspect,verticalFov,projectionX,projectionY;
        public Rect viewport;
        public Vector3 cameraPosition, cameraTarget, cameraUp;
        public Vector3 sourceCameraPosition, sourceCameraTarget, sourceCameraUp;
        public bool diagnosticCameraOverride, introFoliageBaseline;
        public float cameraNearClip, cameraFarClip;
        public bool depthCancellationBaseline;
        public uint flags;
        public ulong simulationTicks;
        public int attractChild;
        public ulong nativeRenderedFrames, introSourceUpdates;
        public bool audioSourcePlaying;
        public Idas3UnityAudio.Statistics audio;
        public Idas3UnityAudio.CallbackStatistics audioCallbacks;
        public string nativeVerticesSha256, nativeRangesSha256, nativeFrameConstantsSha256, cullingMode;
        public string nativeLightConstantsSha256, nativeFogConstantsSha256;
        public uint screenFadeArgb;
        public int targetAntialiasing, visibleCapturePixels;
        public int manualRenderCameraCount;
        public string[] manuallyRenderedCameras;
        public uint nativeVertexCount, nativeRangeCount;
        public int[] sourceListCounts, sourceDepthCompareCounts;
        public int sourceTranslucentDepthWriteRanges;
        public int snowFlakes,snowPowder;
        public bool mirrorEnabled;
        public Vector3 mirrorPosition, mirrorTarget;
        public RivalStatus rival;
    }
    [Serializable,StructLayout(LayoutKind.Sequential,Pack=8)] private struct RivalStatus {
        public uint size,version,preRaceActive,dialogEnemy,dialogKind,dialogPhase;
        public uint legendActive,choiceVisible,choiceKind,selectedIndex,musicCue,musicPlaying;
        public ulong musicSamplePosition;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3SceneGetRivalStatus(ref RivalStatus status);
    private RivalStatus ReadRivalStatus(){
        var result=new RivalStatus{size=(uint)Marshal.SizeOf<RivalStatus>()};
        Check(result.size==56&&Idas3SceneGetRivalStatus(ref result)==1&&result.version==1,"Rival diagnostic ABI mismatch.");
        return result;
    }
    [Serializable] private class RivalObservation {
        public string name;
        public double wallSeconds;
        public ulong nativeFrame,simulationTicks;
        public RivalStatus source;
        public Idas3UnityAudio.Statistics audio;
        public Idas3RivalAudioProbe.Result pcm;
    }
    [Serializable] private class RivalReport {
        public int seededRecord;
        public bool postResultRequested;
        public string scope="Private seeded source profile; real menu/dialogue owners. PCM is observed after the existing Unity source filter and before listener mute, without reading the native ring a second time. Natural stationary timeout is accelerated only outside audio sampling; screenshots use diagnostic AA1/manual rendering. Prompt artwork correctness requires visual review and the native painter regression.";
        public RivalObservation[] observations;
    }
    [Serializable] private class Report {
        public bool passed, shutdownComplete;
        public string error, unityVersion, device;
        public int checks;
        public double elapsedSeconds;
        public float medianSubmissionMs, p95SubmissionMs;
        public Shot[] captures;
        public string scope="Actual Unity scene: moving attract, quick-start with original handling, bumper/mirror, chase, pause and course menu. Geometry and camera ownership checked in Unity. Does not establish pixel-perfect parity or cover every menu route.";
    }
    [Serializable] private struct PerfSample {
        public int index,unityFrame,meshes,uiDraws,uploadedVertices,geometryUploads,materialUpdates;
        public int depthCandidates,depthDraws,depthRebuilds;
        public long drawCalls,batches,setPassCalls;
        public double sceneRenderMs;
        public uint ranges,vertices;
        public ulong nativeFrame,simulationTicks,timingFrameStart;
        public double wallMs,submissionMs,nativeMs,rendererMs,uiMs,cpuFrameMs,cpuMainMs,cpuRenderMs,gpuMs;
        public long mainThreadAllocatedBytes;
        public float speed;
        public float courseDistance,courseLength,raceProgress,travel;
        public bool wallContact;
        public bool focused,frameTimingValid;
    }
    [Serializable] private class PerfMetric {
        public string name;
        public int count;
        public double mean,p50,p95,p99,min,max;
    }
    [Serializable] private class PerfReport {
        public string schema="idas3-race-performance-v1",unityVersion,device,graphicsApi,mode,settings,commandLine;
        public string scope="Normal automatic rendering, fixed 1/60 native input, held throttle with no steering. FrameTimingManager values can lag the current source frame and unavailable values are -1. Allocation deltas cover the main thread, not native/GPU memory or the audio thread. No captures, readbacks or file writes occur during measurement.";
        public int course,warmupFrames,sampleFrames,width,height,antialiasing,vSync,targetFrameRate,mainCameraRenderEvents,allCameraRenderEvents;
        public bool baseline,nightRequested,wetRequested,frameTimingEnabled,mirrorEnabled,developmentBuild;
        public ulong firstNativeFrame,lastNativeFrame,firstSimulationTick,lastSimulationTick;
        public int replayFramesBefore,replayFramesAfter,rivalReplayFramesBefore,rivalReplayFramesAfter;
        public bool replayCaptureEnabled;
        public bool fullDrive,reverseRequested,finishedRace,timeUp,timerGraceAllowed; public int diagnosticTimeExtensions;
        public float finalDistance,courseLength,furthestRaceProgress;
        public int requestedFrameCap;
        public bool legacyFrameCap,highResolutionFrameTimer;
        public int[] gcCollections;
        public double measuredSeconds;
        public PerfMetric[] metrics;
        public PerfSample[] samples;
        public Idas3UnityAudio.Statistics audioBefore,audioAfter;
    }
    private static int DiagnosticInt(string[] args,string option,int fallback,int min,int max)
    {
        int at=Array.IndexOf(args,option);if(at<0)return fallback;
        if(at+1>=args.Length||!int.TryParse(args[at+1],out int value)||value<min||value>max)
            throw new ArgumentException(option+" requires an integer from "+min+" to "+max+".");
        return value;
    }
    internal static bool Configure(Idas3SceneGame host, ref string saves)
    {
        var args=Environment.GetCommandLineArgs();
        int index=Array.IndexOf(args,"-idas3-scene-smoke");
        if(index<0)return false;
        if(index+1>=args.Length)throw new ArgumentException("Scene smoke requires a NEW output directory.");
        string output=Path.GetFullPath(args[index+1]);
        if(Directory.Exists(output)||File.Exists(output))throw new IOException("Use a NEW scene smoke directory: "+output);
        Directory.CreateDirectory(output);
        saves=Path.Combine(output,"userdata");
        if(Array.IndexOf(args,"-idas3-scene-night-check")>=0){Directory.CreateDirectory(saves);File.WriteAllText(Path.Combine(saves,"settings.txt"),"3 0 0 0 1 1 1 0\n");}
        bool requestedDepth=Array.IndexOf(args,"-idas3-scene-depth-check")>=0;
        bool requestedPerf=Array.IndexOf(args,"-idas3-scene-perf-check")>=0;
        bool requestedRival=Array.IndexOf(args,"-idas3-scene-rival-check")>=0;
        int selectedRivalRecord=DiagnosticInt(args,"-idas3-scene-rival-record",0,0,16);
        if(requestedRival&&selectedRivalRecord!=0&&selectedRivalRecord!=1&&selectedRivalRecord!=16)
            throw new ArgumentException("-idas3-scene-rival-record requires 0 (first), 1 (lost), or 16 (won).");
        int selectedPerfCourse=DiagnosticInt(args,"-idas3-scene-perf-course",3,0,11);
        int selectedPerfWarmup=DiagnosticInt(args,"-idas3-scene-perf-warmup",180,60,3600);
        int selectedPerfFrames=DiagnosticInt(args,"-idas3-scene-perf-frames",600,60,72000);
        bool selectedPerfNight=Array.IndexOf(args,"-idas3-scene-perf-night")>=0;
        bool selectedPerfWet=Array.IndexOf(args,"-idas3-scene-perf-wet")>=0;
        bool requestedFoliage=Array.IndexOf(args,"-idas3-scene-intro-foliage-check")>=0;
        bool requestedDoor=Array.IndexOf(args,"-idas3-scene-car-door-check")>=0;
        bool requestedIntro=Array.IndexOf(args,"-idas3-scene-intro-check")>=0||requestedFoliage||requestedDoor;
        int selectedDoorFrame=600,doorFrameAt=Array.IndexOf(args,"-idas3-scene-door-frame");
        if(requestedDoor&&doorFrameAt>=0){
            if(doorFrameAt+1>=args.Length||!int.TryParse(args[doorFrameAt+1],out selectedDoorFrame)||(selectedDoorFrame!=90&&selectedDoorFrame!=600))
                throw new ArgumentException("-idas3-scene-door-frame requires 90 or 600.");
        }
        int selectedDepthCourse=8,depthCourseAt=Array.IndexOf(args,"-idas3-scene-depth-course");
        if(requestedDepth&&depthCourseAt>=0){
            if(depthCourseAt+1>=args.Length||!int.TryParse(args[depthCourseAt+1],out selectedDepthCourse)||selectedDepthCourse<0||selectedDepthCourse>8)
                throw new ArgumentException("-idas3-scene-depth-course requires a course index from 0 to 8.");
        }
        if(Array.IndexOf(args,"-idas3-scene-cull-check")>=0||requestedDepth||requestedIntro||requestedRival){
            Directory.CreateDirectory(saves);
            // Myogi, original handling, forward, dry, automatic. This profile
            // is created before the native host starts and never touches saves.
            File.WriteAllText(Path.Combine(saves,"settings.txt"),(requestedDepth?selectedDepthCourse:0)+" 0 0 0 0 1 1 0\n");
            File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        }
        if(requestedRival)SeedRivalProfile(saves,selectedRivalRecord);
        if(requestedPerf){
            Directory.CreateDirectory(saves);
            // F5 resets wet and route. Start dry/day, then select the requested
            // TA conditions through the original choice owners below.
            File.WriteAllText(Path.Combine(saves,"settings.txt"),"3 0 0 0 0 1 1 0\n");
            File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        }
        File.WriteAllText(Path.Combine(output,"ISOLATED_SCENE_TEST.txt"),"Diagnostic saves and captures; no ordinary user profile loaded.\n");
        active=host.gameObject.AddComponent<Idas3SceneSmoke>();
        active.host=host;active.root=output;active.began=Time.realtimeSinceStartupAsDouble;
        active.realtimeAudio=Array.IndexOf(args,"-idas3-scene-realtime-audio")>=0;
        active.legendCheck=Array.IndexOf(args,"-idas3-scene-legend-check")>=0;
        active.driveInspect=Array.IndexOf(args,"-idas3-scene-drive-inspect")>=0;
        active.flickerCheck=Array.IndexOf(args,"-idas3-scene-flicker-check")>=0;
        active.headlightCheck=Array.IndexOf(args,"-idas3-scene-headlight-check")>=0;
        active.attractFlicker=Array.IndexOf(args,"-idas3-scene-attract-flicker")>=0;
        active.hiresFlicker=Array.IndexOf(args,"-idas3-scene-hires-flicker")>=0;
        active.depthCheck=requestedDepth;active.depthCourse=selectedDepthCourse;
        active.introCheck=requestedIntro;active.introFoliageCheck=requestedFoliage;
        active.carDoorCheck=requestedDoor;active.doorFrame=selectedDoorFrame;
        active.perfCheck=requestedPerf;active.perfCourse=selectedPerfCourse;
        active.perfWarmup=selectedPerfWarmup;active.perfFrames=selectedPerfFrames;
        active.perfNight=selectedPerfNight;active.perfWet=selectedPerfWet;
        active.perfFullDrive=Array.IndexOf(args,"-idas3-perf-full-drive")>=0;
        active.perfReverse=Array.IndexOf(args,"-idas3-perf-reverse")>=0;
        active.perfLegend=Array.IndexOf(args,"-idas3-scene-perf-legend")>=0;
        active.rivalCheck=requestedRival;active.rivalRecord=selectedRivalRecord;
        active.retireCheck=Array.IndexOf(args,"-idas3-scene-retire-check")>=0;
        active.rivalPostResult=active.retireCheck||Array.IndexOf(args,"-idas3-scene-rival-post-result")>=0;
        active.cullCheck=Array.IndexOf(args,"-idas3-scene-cull-check")>=0||requestedDepth||requestedIntro||requestedRival;
        if (!Application.isEditor) Screen.SetResolution(Array.IndexOf(args,"-idas3-scene-wide-check")>=0?1600:1280,Array.IndexOf(args,"-idas3-scene-wide-check")>=0?700:720,FullScreenMode.Windowed);
        if (Array.IndexOf(args,"-idas3-scene-render-check") >= 0||Array.IndexOf(args,"-idas3-scene-resolution-check") >= 0||active.realtimeAudio||active.cullCheck) AudioListener.volume=0;
        if(requestedPerf){
            Screen.SetResolution(2560,1080,FullScreenMode.Windowed);
            QualitySettings.antiAliasing=4;QualitySettings.vSyncCount=0;Application.targetFrameRate=-1;
            AudioListener.volume=0;
        }
        // Exercise Unity AudioSource too. Source output remains at user's volume.
        return true;
    }
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame)
    {
        if(active==null)return true;
        if(active.finished||active.frozen)return false;
        frame=new Idas3Native.FrameInput{size=(uint)Marshal.SizeOf<Idas3Native.FrameInput>(),flags=1,deltaSeconds=active.realtimeAudio||active.realDelta?Math.Min(Time.unscaledDeltaTime,.25):1.0/60};
        if(active.introCheck&&active.introWarmup)frame.deltaSeconds=.25;
        if(active.rivalCheck&&active.rivalFastForward)frame.deltaSeconds=.25;
        if(active.held!=0)frame.SetKey(active.held);
        if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}
        if(active.padPulse!=0){frame.padConnected=1;frame.padButtons=active.padPulse;active.padPulse=0;}
        return true;
    }
    private void Start(){StartCoroutine(Guard(Run()));}
    private void Update()
    {
        if(finished)return;
        if(host.Failure!=null){Finish(false,host.Failure);return;}
        if(Time.realtimeSinceStartupAsDouble-began>(legendCheck||headlightCheck?2400:600)){Finish(false,"Scene smoke exceeded its time budget.");return;}
        if(!perfCheck&&host.Ready&&host.SubmissionMilliseconds>0&&host.Status.racePhase==2&&(host.Status.flags&1)==0)
            submitTimes.Add(host.SubmissionMilliseconds);
    }
    private void Check(bool condition,string why){
        ++checks;if(condition)return;
        if((cullCheck||perfCheck)&&host!=null){var s=host.Status;
            why+=" [state="+s.state+", stage="+s.frontendStage+", course="+s.course+
                ", car="+s.car+", racePhase="+s.racePhase+", flags=0x"+s.flags.ToString("X")+
                ", attractChild="+s.attractChild+
                ", ticks="+s.simulationTicks+", output="+s.width+"x"+s.height+
                "; expected stages Mode5/Course6/Route7/Weather8/Time9/Rival10]";
        }
        throw new InvalidOperationException(why);
    }
    private IEnumerator Guard(IEnumerator routine)
    {
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&!finished)
        {
            object value=null;Exception failure=null;
            try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}
            if(failure!=null){Finish(false,failure.ToString());yield break;}
            if(value is IEnumerator child)stack.Push(child);else yield return value;
        }
    }
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Key(int key){pulse=key;yield return null;yield return null;}
    private IEnumerator PadStart(){padPulse=0x0010;yield return null;yield return null;}
    private IEnumerator Capture(string name,bool world)
    {
        frozen=true;if(!perfCheck)yield return new WaitForEndOfFrame();
        var s=host.Status;var camera=host.GetComponent<Camera>();
        var record=new Shot{name=name,meshRenderers=FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None).Length,
            cameras=FindObjectsByType<Camera>(FindObjectsSortMode.None).Length,lights=FindObjectsByType<Light>(FindObjectsSortMode.None).Length,
            textures=Resources.FindObjectsOfTypeAll<Texture2D>().Length,stage=s.frontendStage,course=s.course,car=s.car,
            uiBuffers=camera.commandBufferCount,speed=s.speedMetresPerSecond,flags=s.flags,simulationTicks=s.simulationTicks,
            cameraPosition=camera.transform.position,submitMs=host.SubmissionMilliseconds};
        record.attractChild=s.attractChild;record.nativeRenderedFrames=s.renderedFrames;
        if(rivalCheck)record.rival=ReadRivalStatus();
        if(introCheck&&s.renderedFrames>=introFirstRenderedFrame)record.introSourceUpdates=s.renderedFrames-introFirstRenderedFrame;
        Check(s.reserved==1,"Legacy framebuffer host is active.");
        var ui=host.GetComponent<Idas3UnityUi>();
        Check(ui!=null&&ui.UnresolvedSurfaces==0,"Unity UI is missing a source surface.");
        Check(ui.DrawCount>0,"Unity UI submitted no elements.");
        var target = camera.targetTexture;
        record.screenWidth=target!=null?target.width:Screen.width;record.screenHeight=target!=null?target.height:Screen.height;record.uiDraws=ui.DrawCount;
        record.renderWidth=s.width;record.renderHeight=s.height;record.cameraAspect=camera.aspect;
        record.verticalFov=camera.fieldOfView;record.projectionX=camera.projectionMatrix.m00;record.projectionY=camera.projectionMatrix.m11;
        record.cameraNearClip=camera.nearClipPlane;record.cameraFarClip=camera.farClipPlane;
        record.cameraUp=camera.transform.up;
        record.viewport=host.GetComponent<Idas3SceneRenderer>().ViewportRect;
        if(cullCheck||perfCheck){
            var scene=host.GetComponent<Idas3SceneRenderer>();var source=scene.CurrentFrame;
            record.sourceCameraPosition=source.mainCamera.eye;record.sourceCameraTarget=source.mainCamera.target;record.sourceCameraUp=source.mainCamera.up;
            record.diagnosticCameraOverride=diagnosticPoseActive;
            record.cameraTarget=diagnosticPoseActive?diagnosticPoseTarget:source.mainCamera.target;
            record.cameraUp=diagnosticPoseActive?camera.transform.up:source.mainCamera.up;
            record.introFoliageBaseline=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-intro-foliage-baseline")>=0;
            record.depthCancellationBaseline=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-depth-cancellation-baseline")>=0;
            record.cullingMode=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-cull-baseline")>=0?"baseline-cull-off":"source-culling";
            record.screenFadeArgb=source.screenFadeArgb;record.targetAntialiasing=target!=null?target.antiAliasing:QualitySettings.antiAliasing;
            record.nativeVerticesSha256=PointerSha256(source.vertices,checked((int)source.vertexCount*64));
            record.nativeRangesSha256=PointerSha256(source.ranges,checked((int)source.rangeCount*64));
            record.nativeFrameConstantsSha256=PointerSha256(source.frameConstants,184*4);
            record.nativeLightConstantsSha256=PointerSha256(source.lightConstants,1248*4);
            record.nativeFogConstantsSha256=PointerSha256(source.fogConstants,136*4);
            record.mirrorEnabled=scene.MirrorCamera.enabled;
            record.mirrorPosition=source.mirrorCamera.eye;record.mirrorTarget=source.mirrorCamera.target;
            if(depthCheck||introCheck){
                record.nativeVertexCount=source.vertexCount;record.nativeRangeCount=source.rangeCount;
                record.sourceListCounts=new int[8];record.sourceDepthCompareCounts=new int[8];
                for(int i=0;i<source.rangeCount;++i){
                    int offset=checked(i*64);uint pcw=unchecked((uint)Marshal.ReadInt32(source.ranges,offset+16));
                    uint isp=unchecked((uint)Marshal.ReadInt32(source.ranges,offset+20));
                    ++record.sourceListCounts[(pcw>>24)&7];++record.sourceDepthCompareCounts[(isp>>29)&7];
                    if(((pcw>>24)&7)==2&&(isp&(1u<<26))==0)++record.sourceTranslucentDepthWriteRanges;
                    if(depthCheck&&s.course==8&&unchecked((uint)Marshal.ReadInt32(source.ranges,offset+12))==0x941024d2u){
                        int texture=Marshal.ReadInt32(source.ranges,offset+8)-(int)source.textureCount+11;
                        int quads=Marshal.ReadInt32(source.ranges,offset+4)/6;
                        Check(texture==1||texture==7,"Snow submitted rain/water-trail textures.");
                        if(texture==1)record.snowFlakes+=quads;else record.snowPowder+=quads;
                    }
                }
                if(depthCheck&&s.course==8){
                    Check(record.snowFlakes>0,"Akina Snow submitted no snowfall.");
                    if(name.StartsWith("depth-driving-",StringComparison.Ordinal)&&s.speedMetresPerSecond>2)
                        Check(record.snowPowder>0,"Moving car submitted no tire snow powder.");
                    bool reduced=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-weather-reduced")>=0;
                    Check(record.snowFlakes+record.snowPowder<=(reduced?88:352),"Snow exceeded its particle budget.");
                }
                if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-depth-dump")>=0&&
                    (name=="depth-driving-11"||name=="depth-camera-static")){
                    DumpPointer(name+".vertices.bin",source.vertices,checked((int)source.vertexCount*64));
                    DumpPointer(name+".ranges.bin",source.ranges,checked((int)source.rangeCount*64));
                    DumpPointer(name+".frame-constants.bin",source.frameConstants,184*4);
                }
                if(introCheck&&(name=="intro-0090"||name=="intro-0250"||name=="intro-0480"||
                    name=="intro-0535"||name=="intro-0600"||name=="intro-0900"||name=="intro-1420"||
                    (carDoorCheck&&name.StartsWith("door-side-",StringComparison.Ordinal)&&name.EndsWith("-static",StringComparison.Ordinal)))){
                    DumpPointer(name+".vertices.bin",source.vertices,checked((int)source.vertexCount*64));
                    DumpPointer(name+".ranges.bin",source.ranges,checked((int)source.rangeCount*64));
                    DumpPointer(name+".frame-constants.bin",source.frameConstants,184*4);
                    DumpPointer(name+".light-constants.bin",source.lightConstants,1248*4);
                    DumpPointer(name+".fog-constants.bin",source.fogConstants,136*4);
                }
            }
        }
        Check(record.renderWidth==record.screenWidth&&record.renderHeight==record.screenHeight,"Native presentation does not match the actual output resolution.");
        Check(record.viewport==new Rect(0,0,1,1),"The output has a fixed-aspect letterbox.");
        Check(camera.rect==new Rect(0,0,1,1),"Main scene camera does not cover the whole output.");
        Check(host.GetComponent<AudioListener>()==null,"Audio DSP callback shares its object with a listener.");
        Check(Idas3Native.Idas3UnityGetTexture()==IntPtr.Zero,"Native D3D framebuffer exists in the Unity scene path.");
        var sound=host.GetComponent<AudioSource>();
        record.audioSourcePlaying=sound!=null&&sound.isPlaying;
        record.audio=Idas3UnityAudio.ReadStatistics();
        record.audioCallbacks=host.GetComponent<Idas3UnityAudio>().ReadCallbackStatistics();
        if(name=="race-bumper"||name=="legend-bumper-mirror")
        {
            Check(record.audioSourcePlaying,"Unity AudioSource is not playing the original mix.");
            Check(record.audio.consumedFrames>0,"Unity audio callback has consumed no original PCM.");
        }
        if(world){
            Check(record.meshRenderers>0,"No Unity mesh renderers exist.");Check(record.textures>1,"No scene textures imported.");
            foreach(var geometry in FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None)){
                if(geometry.gameObject.layer!=30)continue; // Imported placements intentionally use local transforms.
                var matrix=geometry.localToWorldMatrix;
                for(int row=0;row<4;++row)for(int col=0;col<4;++col)
                    Check(Mathf.Abs(matrix[row,col]-(row==col?1:0))<.00001f,"Baked world geometry inherited a camera/object transform; culling will be wrong.");
            }
        }
        // Hidden D3D11 windows can return an empty manually resolved MSAA
        // surface. Capture at AA1, as the depth/intro probes do, then restore
        // AA4 before any measured frame. The native scene stays frozen.
        int captureSamples=target!=null?target.antiAliasing:1;
        bool restoreCaptureSamples=perfCheck&&target!=null&&captureSamples>1;
        if(restoreCaptureSamples){target.Release();target.antiAliasing=1;Check(target.Create(),"Diagnostic AA1 target creation");record.targetAntialiasing=1;}
        if((cullCheck||perfCheck||rivalCheck)&&target!=null){
            // The desktop session can suppress automatic presentation even
            // while Update and native simulation continue. Explicitly render
            // this diagnostic target once, in the game's camera depth order.
            // Resources includes the DontSave UI cameras omitted by ordinary
            // FindObjectsByType. The initial clear prevents double blending.
            var cameras=new List<Camera>();
            foreach(var candidate in Resources.FindObjectsOfTypeAll<Camera>())
                if(candidate!=null&&(candidate.enabled||candidate==camera)&&candidate.gameObject.activeInHierarchy&&candidate.targetTexture==target)
                    cameras.Add(candidate);
                cameras.Sort((a,b)=>{int order=a.depth.CompareTo(b.depth);return order!=0?order:string.CompareOrdinal(a.name,b.name);});
            Check(cameras.Count>0&&cameras.Contains(camera),"No enabled main camera targets the culling capture.");
            Check(cameras[0].clearFlags==CameraClearFlags.SolidColor||cameras[0].clearFlags==CameraClearFlags.Skybox,
                "Culling manual camera stack does not begin with a color clear.");
            record.manualRenderCameraCount=cameras.Count;record.manuallyRenderedCameras=new string[cameras.Count];
            for(int i=0;i<cameras.Count;++i){record.manuallyRenderedCameras[i]=cameras[i].name;cameras[i].Render();}
        }
        Texture2D capture;
        if(target!=null){
            var prior=RenderTexture.active;
            RenderTexture resolved=null;
            try{
                var readable=target;
                if((cullCheck||perfCheck)&&target.antiAliasing>1){
                    resolved=RenderTexture.GetTemporary(target.width,target.height,0,target.format,RenderTextureReadWrite.Default,1);
                    target.ResolveAntiAliasedSurface(resolved);readable=resolved;
                }
                RenderTexture.active=readable;capture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
                capture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);capture.Apply();
            }
            finally{RenderTexture.active=prior;if(resolved!=null)RenderTexture.ReleaseTemporary(resolved);}
        }else capture=ScreenCapture.CaptureScreenshotAsTexture();
        if(restoreCaptureSamples){target.Release();target.antiAliasing=captureSamples;Check(target.Create(),"Restore measured AA4 target");}
        Check(capture!=null&&capture.width>0,"Unity screen capture failed.");
        if(cullCheck||perfCheck){
            foreach(var pixel in capture.GetPixels32())if(Mathf.Max(pixel.r,Mathf.Max(pixel.g,pixel.b))>24)++record.visibleCapturePixels;
        }
        File.WriteAllBytes(Path.Combine(root,name+".png"),capture.EncodeToPNG());Destroy(capture);
        shots.Add(record);File.WriteAllText(Path.Combine(root,name+".json"),JsonUtility.ToJson(record,true));
        if(cullCheck||perfCheck)Check(record.visibleCapturePixels>record.screenWidth*record.screenHeight/1000,
            "Culling capture is black or nearly empty: "+name+", visiblePixels="+record.visibleCapturePixels+
            ", screenFadeArgb=0x"+record.screenFadeArgb.ToString("X8")+", samples="+record.targetAntialiasing+".");
        frozen=false;
    }
    private IEnumerator Run()
    {
        yield return Frames(3);Check(host.Ready,"Scene host did not initialize.");
        host.DiagnosticFocusOverride=true;
        Check(host.ControllerDevices.Select("keyboard"),"Isolate physical controller input");yield return Frames(4);
        if(rivalCheck){yield return RivalCheck();yield break;}
        if(perfCheck){yield return PerformanceCheck();yield break;}
        if(introCheck){yield return IntroCheck();yield break;}
        if(depthCheck){yield return DepthCheck();yield break;}
        if(cullCheck){yield return CullCheck();yield break;}
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-resolution-check")>=0)
        {
            yield return Key(116);held=87;yield return Frames(180);held=0;
            yield return Key(67);yield return Frames(20);yield return Key(27);yield return Frames(3);
            Check((host.Status.flags&2)!=0,"Resolution check did not pause the race.");
            var main=host.GetComponent<Camera>();float verticalFov=main.fieldOfView;
            ulong tick=host.Status.simulationTicks;
            var sizes=new[]{new Vector2Int(1280,720),new Vector2Int(2560,1080),new Vector2Int(1280,960),new Vector2Int(900,1200),new Vector2Int(3840,2160),new Vector2Int(5120,1440)};
            foreach(var size in sizes)
            {
                var output=new RenderTexture(size.x,size.y,24,RenderTextureFormat.ARGB32){name="Resolution verification "+size.x+"x"+size.y};
                Check(output.Create(),"Resolution render target could not be created.");
                main.targetTexture=output;yield return Frames(5);
                Check(host.Status.width==size.x&&host.Status.height==size.y,"Native capture did not resize to the output.");
                Check(main.pixelWidth==size.x&&main.pixelHeight==size.y,"Unity is rendering into only part of the target.");
                Check(Mathf.Abs(main.fieldOfView-verticalFov)<.001f,"Resizing changed the vertical driving field of view.");
                Check(Mathf.Abs(Mathf.Abs(main.projectionMatrix.m11/main.projectionMatrix.m00)-size.x/(float)size.y)<.001f,"Camera projection stretches the world at this aspect ratio.");
                Check(host.Status.simulationTicks==tick,"Resizing advanced the paused physics simulation.");
                yield return Capture("resolution-"+size.x+"x"+size.y,true);
                main.targetTexture=null;yield return Frames(2);output.Release();Destroy(output);
            }
            yield return Capture("resolution-window-restored",true);
            Finish(true,null);yield break;
        }
        if(realtimeAudio)
        {
            yield return Key(116);held=87;
            double warm=Time.realtimeSinceStartupAsDouble+5;
            while(Time.realtimeSinceStartupAsDouble<warm)yield return null;
            var before=Idas3UnityAudio.ReadStatistics();
            double start=Time.realtimeSinceStartupAsDouble;
            while(Time.realtimeSinceStartupAsDouble-start<6)yield return null;
            var after=Idas3UnityAudio.ReadStatistics();held=0;
            double consumed=after.consumedFrames-before.consumedFrames,produced=after.producedFrames-before.producedFrames;
            Check(produced>44100*4,"Original mix did not advance with real time.");
            Check(consumed/produced>.85&&consumed/produced<1.15,"Unity audio is dropping or starving the original real-time mix.");
            Check(after.underrunFrames-before.underrunFrames<4410,"Unity audio has repeated dropouts after warmup.");
            File.WriteAllText(Path.Combine(root,"audio-realtime.json"),"{\"produced\":"+produced+",\"consumed\":"+consumed+",\"underrun\":"+(after.underrunFrames-before.underrunFrames)+",\"elapsedSeconds\":"+(Time.realtimeSinceStartupAsDouble-start).ToString(System.Globalization.CultureInfo.InvariantCulture)+"}");
            yield return Capture("race-bumper",true);Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-render-check")>=0)
        {
            yield return Key(116);held=87;yield return Frames(360);held=0;
            yield return Capture("race-bumper",true);
            yield return Key(67);yield return Frames(20);yield return Capture("race-chase",true);
            Finish(true,null);yield break;
        }
        if(driveInspect){yield return DriveInspect();yield break;}
        if(legendCheck){yield return LegendReturn();yield break;}
        if(flickerCheck||hiresFlicker){yield return Flicker();yield break;}
        if(headlightCheck){yield return Headlights();yield break;}
        if(attractFlicker){yield return AttractFlicker();yield break;}
        int wait=0;while(host.Status.attractChild!=7&&wait++<1800)yield return null;
        Check(host.Status.attractChild==7,"Attract driving scene was not reached.");
        yield return Frames(120);yield return Capture("attract-a",true);
        var a=host.GetComponent<Camera>().transform.position;
        yield return Frames(60);yield return Capture("attract-b",true);
        Check((host.GetComponent<Camera>().transform.position-a).sqrMagnitude>0.01f,"Attract camera is static.");
        yield return Key(116);
        Check((host.Status.flags&1)==0&&(host.Status.flags&16)!=0,"Quick start did not use original handling.");
        held=87;yield return Frames(360);held=0;
        Check(host.Status.simulationTicks>=360&&host.Status.speedMetresPerSecond>1,"Original simulation failed to accelerate.");
        yield return Capture("race-bumper",true);
        Check(FindObjectsByType<Camera>(FindObjectsSortMode.None).Length>=2,"Unity rear-view camera missing.");
        var bumper=host.GetComponent<Camera>().transform.position;
        yield return Key(67);yield return Frames(20);yield return Capture("race-chase",true);
        Check((host.GetComponent<Camera>().transform.position-bumper).sqrMagnitude>0.01f,"Camera switch did not move Unity camera.");
        yield return Key(27);Check((host.Status.flags&2)!=0,"Pause failed.");
        ulong ticks=host.Status.simulationTicks;yield return Frames(8);Check(host.Status.simulationTicks==ticks,"Paused solver advanced.");
        yield return Capture("race-paused",true);
        yield return Key(8);yield return Frames(30);
        Check((host.Status.flags&1)!=0&&host.Status.frontendStage==5,"Course return failed.");
        yield return Capture("course-menu",false);
        yield return Key(27);yield return Frames(100);
        Check(host.Status.frontendStage==4,"Mode return failed.");
        yield return Capture("mode-menu",false);
        yield return Key(13);yield return Frames(180);
        Check(host.Status.frontendStage==5,"Legend course selection failed.");
        yield return Key(13);yield return Frames(45);
        Check(host.Status.frontendStage==9,"Legend rival selection failed.");
        yield return Capture("rival-menu",false);
        yield return Key(13);yield return Frames(180);
        Check((host.Status.flags&1)==0,"Legend did not start.");
        yield return Key(67);held=87;yield return Frames(180);held=0;
        var scene=host.GetComponent<Idas3SceneRenderer>();
        Check(scene.MirrorCamera.enabled&&scene.CurrentFrame.viewCount==2,"Legend bumper rear view is missing.");
        yield return Capture("legend-bumper-mirror",true);
        yield return Key(67);yield return Frames(10);yield return Capture("legend-chase",true);
        Finish(true,null);
    }
    // The whole post-result Legend owner in the actual player: a Legend battle
    // that runs out of time, the scripted rival dialogue, the start-button skip,
    // the continue and next-rival prompts, and the race the acceptance starts.
    private static void PutDiagnosticWord(byte[] data,int offset,uint value){
        for(int i=0;i<4;++i)data[offset+i]=(byte)(value>>(i*8));
    }
    private static void SeedRivalProfile(string saves,int record){
        // Exact v1 envelope and fresh defaults from local_driver_profiles.cpp
        // and original_battle_profile.cpp; this only writes NEW test userdata.
        string directory=Path.Combine(saves,"driver_profiles_v1");Directory.CreateDirectory(directory);
        var profile=new byte[24+1228];Array.Copy(Encoding.ASCII.GetBytes("ID3PRF1\0"),profile,8);
        PutDiagnosticWord(profile,8,1);PutDiagnosticWord(profile,12,0);PutDiagnosticWord(profile,16,307);
        for(int i=44;i<=60;i+=4)PutDiagnosticWord(profile,24+i,220);
        PutDiagnosticWord(profile,24+472,1);PutDiagnosticWord(profile,24+476,1);
        for(int i=1;i<=35;++i)profile[24+1044+i]=(byte)i;
        PutDiagnosticWord(profile,24+1140,51);PutDiagnosticWord(profile,24+1164,1);
        PutDiagnosticWord(profile,24+1176,1279);PutDiagnosticWord(profile,24+1180,129);
        profile[24+1187]=4;PutDiagnosticWord(profile,24+1220,1);PutDiagnosticWord(profile,24+1224,4);
        profile[24+116]=(byte)record;
        uint hash=2166136261u;
        unchecked{for(int i=0;i<profile.Length;++i)if(i<20||i>=24){hash^=profile[i];hash*=16777619u;}}
        PutDiagnosticWord(profile,20,hash);File.WriteAllBytes(Path.Combine(directory,"car_00.profile"),profile);
        var setup=new byte[32];Array.Copy(Encoding.ASCII.GetBytes("ID3SET1\0"),setup,8);
        PutDiagnosticWord(setup,8,1);PutDiagnosticWord(setup,16,1);hash=2166136261u;
        unchecked{for(int i=0;i<28;++i){hash^=setup[i];hash*=16777619u;}}
        PutDiagnosticWord(setup,28,hash);File.WriteAllBytes(Path.Combine(directory,"car_00.setup"),setup);
    }
    private void ObserveRival(string name,Idas3RivalAudioProbe.Result pcm=null){
        rivalObservations.Add(new RivalObservation{name=name,wallSeconds=Time.realtimeSinceStartupAsDouble-began,
            nativeFrame=host.Status.renderedFrames,simulationTicks=host.Status.simulationTicks,
            source=ReadRivalStatus(),audio=Idas3UnityAudio.ReadStatistics(),pcm=pcm});
        File.WriteAllText(Path.Combine(root,"rival-observations.json"),JsonUtility.ToJson(new RivalReport{
            seededRecord=rivalRecord,postResultRequested=rivalPostResult,observations=rivalObservations.ToArray()},true));
    }
    private IEnumerator SampleRivalAudio(string name,bool intro){
        realDelta=true;yield return Frames(20);
        var before=ReadRivalStatus();ObserveRival(name+"-before");
        Check(before.musicPlaying!=0,"The source rival owner has no active music: "+name);
        if(intro)Check(before.preRaceActive!=0&&before.musicCue==before.dialogEnemy+3,
            "The challenge dialogue did not request its original rival cue.");
        rivalAudio.Begin(AudioSettings.outputSampleRate,2.0);
        double deadline=Time.realtimeSinceStartupAsDouble+6;
        while(!rivalAudio.Complete&&Time.realtimeSinceStartupAsDouble<deadline)yield return null;
        var pcm=rivalAudio.End(Path.Combine(root,name+".wav"));
        var after=ReadRivalStatus();ObserveRival(name+"-after",pcm);
        Check(pcm.seconds>=1.9&&pcm.peak>.0001&&pcm.rms>.00001,
            "Rival audio was absent or silent at the Unity source output: "+name+", seconds="+pcm.seconds+", RMS="+pcm.rms);
        Check(after.musicPlaying!=0&&after.musicCue==before.musicCue&&after.musicSamplePosition>before.musicSamplePosition,
            "The source rival music cursor did not advance during actual PCM playback.");
        Check(host.GetComponent<AudioSource>().isPlaying,"Unity stopped its audio source during the rival dialogue.");
        realDelta=false;
    }
    private IEnumerator RivalCheck(){
        var main=host.GetComponent<Camera>();
        hires=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32){name="Rival prompt verification",antiAliasing=1};
        Check(hires.Create(),"Rival verification target could not be created.");main.targetTexture=hires;
        // Appended after Idas3UnityAudio so this observer receives its output,
        // never the silent carrier and never a second read of the native ring.
        rivalAudio=host.gameObject.AddComponent<Idas3RivalAudioProbe>();
        var components=host.GetComponents<MonoBehaviour>();
        Check(Array.IndexOf(components,host.GetComponent<Idas3UnityAudio>())<Array.IndexOf(components,rivalAudio),
            "The diagnostic PCM observer precedes the game's source filter.");
        yield return Frames(4);
        yield return Key(116);yield return Frames(20);
        for(int i=0;i<1800&&host.Status.racePhase!=2;++i)yield return null;
        Check(host.Status.racePhase==2,"Rival setup did not finish countdown");
        var setupPause=host.GetComponent<Idas3PauseMenu>();setupPause.SetOpen(true);yield return Frames(4);
        Check(Idas3Native.Idas3SceneReturnToCourse()==1,"Rival setup could not return through managed pause");
        setupPause.SetOpen(false);yield return Frames(30);
        Check(host.Status.frontendStage==6&&(host.Status.flags&1)!=0,"Rival setup did not return to Course6.");
        for(int i=0;i<3;++i){yield return Key(37);yield return Frames(6);}
        Check(host.Status.course==0,"Rival setup did not select Myogi.");
        yield return Key(27);yield return Frames(100);
        Check(host.Status.frontendStage==5,"Rival setup did not reach Mode5.");
        yield return Key(13);yield return Frames(180);
        Check(host.Status.frontendStage==6,"Rival setup did not select Legend Course6.");
        yield return Key(13);yield return Frames(45);
        Check(host.Status.frontendStage==10,"Rival setup did not reach Rival10.");
        yield return Capture("rival-selection",false);
        yield return Key(13);
        for(int i=0;i<180&&ReadRivalStatus().preRaceActive==0;++i)yield return null;
        var entered=ReadRivalStatus();uint expected=rivalRecord==16?14u:rivalRecord==1?7u:0u;
        Check(entered.preRaceActive!=0&&entered.dialogEnemy==0&&entered.dialogKind==expected,
            "Seeded record did not enter the expected first/lost/won rival dialogue (expected kind "+expected+").");
        ObserveRival("intro-enter");yield return Frames(30);
        yield return Capture("rival-intro-before-audio",false);
        yield return SampleRivalAudio("rival-intro",true);
        yield return Capture("rival-intro-after-audio",false);
        for(int i=0;i<240&&ReadRivalStatus().preRaceActive!=0;++i){yield return PadStart();yield return Frames(8);}
        for(int i=0;i<600&&(host.Status.flags&1)!=0;++i)yield return null;
        Check(ReadRivalStatus().preRaceActive==0&&(host.Status.flags&1)==0&&(host.Status.flags&16)!=0,
            "Skipping the source intro did not release an original-handling race.");
        for(int i=0;i<1800&&host.Status.racePhase!=2;++i)yield return Frames(1);
        Check(host.Status.racePhase==2&&(host.Status.flags&1024u)==0,"Rival loading/showcase/countdown did not release the race");
        ObserveRival("race-start");yield return Capture("rival-race-start",true);
        if(rivalPostResult){
            if(retireCheck){
                Check((host.Status.flags&8192u)!=0,"Legend race did not expose retirement");
                var pause=host.GetComponent<Idas3PauseMenu>();pause.SetOpen(true);yield return Frames(3);
                for(int row=0;row<3;++row)pause.Navigate(1);
                pause.Activate();yield return Frames(3);
                pause.Navigate(1);pause.Activate();yield return Frames(3);
                Check(!pause.IsOpen&&host.Status.racePhase==3&&(host.Status.flags&2)==0,"Pause retirement did not finish/unpause the race");
                Check(Idas3Native.Idas3SceneRetire()==0,"Finished race accepted retirement twice");
            }
            rivalFastForward=true;
            for(int i=0;i<3000&&ReadRivalStatus().legendActive==0;++i){
                // A stationary car loses naturally. Only the simulation delta
                // is accelerated; no timer, finish, position or result writes.
                if(host.Status.racePhase==3&&i%12==0){yield return Key(13);}else yield return null;
            }
            rivalFastForward=false;
            Check(ReadRivalStatus().legendActive!=0,"Natural timeout did not reach the Legend return owner.");
            ObserveRival("post-result-enter");yield return Frames(45);
            yield return Capture("post-result-dialogue",false);
            yield return SampleRivalAudio("post-result-dialogue",false);
            for(int i=0;i<2400&&ReadRivalStatus().choiceVisible==0;++i){padPulse=0x0010;yield return null;}
            var prompt=ReadRivalStatus();
            Check(prompt.legendActive!=0&&prompt.choiceVisible!=0&&prompt.choiceKind==0,
                "The timed-out battle did not present its loss Continue prompt.");
            yield return Frames(8);yield return Capture("continue-yes",false);
            yield return Key(39);yield return Frames(4);
            Check(ReadRivalStatus().selectedIndex==1,"Continue selection did not move to No.");
            yield return Capture("continue-no",false);
            yield return Key(37);yield return Frames(4);
            Check(ReadRivalStatus().selectedIndex==0,"Continue selection did not return to Yes.");
            yield return Key(13);yield return Frames(4);
            Check(ReadRivalStatus().choiceVisible!=0&&ReadRivalStatus().choiceKind==0,
                "Continue artwork changed category during confirmation.");
            yield return Capture("continue-confirm",false);ObserveRival("continue-confirm");
            for(int i=0;i<600;++i){
                prompt=ReadRivalStatus();if(prompt.choiceVisible!=0&&prompt.choiceKind==2)break;yield return null;
            }
            prompt=ReadRivalStatus();Check(prompt.choiceVisible!=0&&prompt.choiceKind==2,
                "The loss return did not present the rematch prompt.");
            yield return Frames(8);yield return Capture("rematch-challenge",false);
            yield return Key(39);yield return Frames(4);
            Check(ReadRivalStatus().selectedIndex==1,"Rematch selection did not move to Refuse.");
            yield return Capture("rematch-refuse",false);
            if(retireCheck){
                uint oldKind=ReadRivalStatus().dialogKind;
                yield return Key(13);
                for(int i=0;i<120&&ReadRivalStatus().dialogKind==oldKind;++i)yield return null;
                var reply=ReadRivalStatus();
                Check(reply.legendActive!=0&&reply.dialogKind==oldKind+2&&reply.dialogPhase==1,"Refusal skipped its dialogue");
                yield return Frames(30);yield return Capture("retire-refusal-dialogue",false);
                for(int i=0;i<12000&&ReadRivalStatus().legendActive!=0;++i)yield return null;
                Check(ReadRivalStatus().legendActive==0&&(host.Status.flags&1)!=0&&host.Status.frontendStage==6,"Refusal failed to return to course select");
                // Course ownership changes at full black. Let its restored
                // entry fade finish before requiring visible menu pixels.
                var scene=host.GetComponent<Idas3SceneRenderer>();
                for(int i=0;i<120&&(scene.CurrentFrame.screenFadeArgb>>24)!=0;++i)yield return Frames(1);
                Check((scene.CurrentFrame.screenFadeArgb>>24)==0,"Course menu did not fade in after refusal");
                Check((host.Status.flags&1)!=0&&host.Status.frontendStage==6,"Course menu changed during refusal entry fade");
                yield return Capture("retire-return-course",false);
                main.targetTexture=null;yield return Frames(2);hires.Release();Destroy(hires);hires=null;
                Finish(true,null);yield break;
            }
            yield return Key(37);yield return Frames(4);yield return Key(13);yield return Frames(4);
            Check(ReadRivalStatus().choiceVisible!=0&&ReadRivalStatus().choiceKind==2,
                "Rematch artwork incorrectly changed to a new challenger during confirmation.");
            yield return Capture("rematch-confirm",false);ObserveRival("rematch-confirm");
            for(int i=0;i<1800&&ReadRivalStatus().legendActive!=0;++i){padPulse=0x0010;yield return null;}
            Check(ReadRivalStatus().legendActive==0&&(host.Status.flags&1)==0,
                "Accepting the rematch did not return to the actual race.");
            held=87;yield return Frames(240);held=0;
            Check(host.Status.speedMetresPerSecond>1,"The accepted rematch did not move under throttle.");
            ObserveRival("rematch-race");yield return Capture("rematch-race",true);
        }
        main.targetTexture=null;yield return Frames(2);hires.Release();Destroy(hires);hires=null;
        Finish(true,null);
    }
    private IEnumerator LegendReturn()
    {
        int wait=0;while(host.Status.attractChild!=7&&wait++<1800)yield return null;
        yield return Key(116);yield return Frames(20);
        yield return Key(27);yield return Frames(10);
        yield return Key(8);yield return Frames(60);
        Check((host.Status.flags&1)!=0&&host.Status.frontendStage==5,"Course menu was not reached.");
        yield return Key(27);yield return Frames(100);
        Check(host.Status.frontendStage==4,"Mode menu was not reached.");
        yield return Key(13);yield return Frames(180);
        Check(host.Status.frontendStage==5,"Legend course selection failed.");
        yield return Key(13);yield return Frames(45);
        Check(host.Status.frontendStage==9,"Legend rival selection failed.");
        yield return Key(13);yield return Frames(180);
        Check((host.Status.flags&1)==0,"Legend battle did not start.");
        // No throttle: the source race timer runs out on its own and settles a
        // loss, which is what hands the post-result owner its status.
        double raceStart=Time.realtimeSinceStartupAsDouble;
        bool caughtBanner=false;
        while((host.Status.flags&32)==0)
        {
            if(Time.realtimeSinceStartupAsDouble-raceStart>900)break;
            if(host.Status.racePhase==3)
            {
                // The race-end announcement holds the road view first.
                if(!caughtBanner){caughtBanner=true;yield return Capture("finish-banner",true);
                    yield return Frames(400);yield return Capture("outcome-banner",true);}
                pulse=13;
            }
            yield return null;
        }
        Check((host.Status.flags&32)!=0,"The post-result Legend owner never started.");
        yield return Frames(120);
        yield return Capture("legend-dialogue",false);
        // "PRESS THE START BUTTON TO SKIP" must actually shorten the pages.
        held=27;int skip=0;
        while((host.Status.flags&64)==0&&skip++<12000)yield return null;
        held=0;
        Check((host.Status.flags&64)!=0,"No choice prompt appeared after the dialogue.");
        // Unskipped, this owner first draws a prompt after 5751 updates.
        Check(skip<2000,"The start button did not skip the scripted dialogue.");
        yield return Frames(10);
        yield return Capture("legend-continue-prompt",false);
        // Accept: the owner selects the next rival and the game races it.
        yield return Key(13);yield return Frames(60);
        if((host.Status.flags&64)!=0){yield return Capture("legend-nextrival-prompt",false);yield return Key(13);}
        int settle=0;while((host.Status.flags&32)!=0&&settle++<3600)yield return null;
        Check((host.Status.flags&32)==0,"The post-result owner never handed control back.");
        yield return Frames(120);
        Check((host.Status.flags&1)==0,"Accepting the challenge did not start the next battle.");
        held=87;yield return Frames(180);held=0;
        Check(host.Status.simulationTicks>0&&host.Status.speedMetresPerSecond>1,"The next battle is not actually running.");
        yield return Capture("legend-next-battle",true);
        Finish(true,null);
    }
    // Renders the same paused scene repeatedly and then a slow-moving one, and
    // records how many pixels change between consecutive frames. A paused scene
    // that changes means the draw order is not stable; a moving one that changes
    // far more than its motion accounts for is depth or sampling instability.
    // Drives the course and records it two ways: stills spaced along the road,
    // to look for foliage shaded as dark cards and for distant road drawn over
    // nearer hillside, and bursts of consecutive frames, so the per-pixel
    // alternation that shows up as shimmer can be measured rather than judged.
    private IEnumerator DriveInspect()
    {
        yield return Key(116);
        yield return Frames(90);
        held=87;
        for(int leg=0;leg<8;++leg)
        {
            yield return Frames(150);
            yield return Capture("drive-"+leg.ToString("00"),true);
            for(int f=0;f<12;++f)yield return Moving("burst-"+leg.ToString("00")+"-"+f.ToString("00"));
        }
        held=0;
        Finish(true,null);
    }
    private static string PointerSha256(IntPtr pointer,int bytes)
    {
        if(bytes<0||bytes>128*1024*1024||bytes>0&&pointer==IntPtr.Zero)
            throw new InvalidOperationException("Invalid diagnostic scene buffer.");
        var data=new byte[bytes];if(bytes>0)Marshal.Copy(pointer,data,0,bytes);
        using(var hash=System.Security.Cryptography.SHA256.Create())
            return BitConverter.ToString(hash.ComputeHash(data)).Replace("-","").ToLowerInvariant();
    }
    private void DumpPointer(string name,IntPtr pointer,int bytes)
    {
        if(bytes<0||bytes>128*1024*1024||bytes>0&&pointer==IntPtr.Zero)
            throw new InvalidOperationException("Invalid diagnostic scene dump buffer.");
        var data=new byte[bytes];if(bytes>0)Marshal.Copy(pointer,data,0,bytes);
        File.WriteAllBytes(Path.Combine(root,name),data);
    }
    private static Vector3 DiagnosticVector(string option,Vector3 fallback)
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,option);
        if(at<0)return fallback;
        if(at+1>=args.Length)throw new ArgumentException(option+" requires x,y,z.");
        var values=args[at+1].Split(',');
        if(values.Length!=3)throw new ArgumentException(option+" requires x,y,z.");
        var result=Vector3.zero;
        for(int i=0;i<3;++i){
            result[i]=float.Parse(values[i],System.Globalization.CultureInfo.InvariantCulture);
            if(float.IsNaN(result[i])||float.IsInfinity(result[i]))throw new ArgumentException("Nonfinite diagnostic camera.");
        }
        return result;
    }
    private void SetDiagnosticView(Idas3SceneRenderer scene,Vector3 eye,Vector3 target)
    {
        scene.SetDiagnosticCameraPose(eye,target);diagnosticPoseActive=true;diagnosticPoseTarget=target;
    }
    private void ClearDiagnosticView(Idas3SceneRenderer scene)
    {
        scene.ClearDiagnosticCameraPose();diagnosticPoseActive=false;
    }
    // Observe the real intro owner. Only the preceding logo cards are advanced
    // in larger source-tick batches; child6 and all captured child7 frames use
    // one original tick per update so camera cuts and body motion stay exact.
    private IEnumerator IntroCheck()
    {
        var scene=host.GetComponent<Idas3SceneRenderer>();var main=host.GetComponent<Camera>();
        hires=new RenderTexture(2560,1080,24,RenderTextureFormat.ARGB32){name="Intro regression 2560x1080",antiAliasing=1};
        Check(hires.Create(),"Intro verification render target could not be created.");
        main.targetTexture=hires;introWarmup=true;
        int wait=0;
        while(host.Status.attractChild>=3&&host.Status.attractChild<=5&&wait++<180)yield return null;
        introWarmup=false;
        Check(host.Status.attractChild==6||host.Status.attractChild==7,"Intro setup did not reach the source title-to-demo owner.");
        wait=0;while(host.Status.attractChild!=7&&wait++<120)yield return null;
        Check(host.Status.attractChild==7&&host.Status.frontendStage==0,"Intro source child7 was not reached.");
        introFirstRenderedFrame=host.Status.renderedFrames;
        // The original first camera cuts fall at188 and374; headlights and
        // their light effects change at500 and535. Consecutive samples expose
        // unstable body surfaces without conflating them with a camera cut.
        bool extended=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-intro-extended")>=0;
        int[] samples=extended?new[]{24,90,180,186,188,190,250,372,374,376,480,481,482,483,499,500,501,534,535,536,600,750,752,900,1295,1420}:
            new[]{90,180,190,250,480,481,482,535,600,900};
        foreach(int sample in samples){
            while(host.Status.renderedFrames-introFirstRenderedFrame<(ulong)sample)yield return null;
            Check(host.Status.attractChild==7&&host.Status.frontendStage==0,"Intro diagnostic left the source demo owner.");
            Check(host.Status.renderedFrames-introFirstRenderedFrame==(ulong)sample,"Intro diagnostic skipped the requested source update.");
            Check(scene.CurrentFrame.vertexCount>0&&scene.CurrentFrame.rangeCount>0,"Intro demo submitted no native scene geometry.");
            yield return Capture("intro-"+sample.ToString("0000"),true);
            if(introFoliageCheck&&sample==900)yield return IntroFoliageProbe(scene);
            if(carDoorCheck&&sample==doorFrame)yield return CarDoorProbe(scene);
        }
        main.targetTexture=null;frozen=false;yield return Frames(2);
        hires.Release();Destroy(hires);hires=null;
        Finish(true,null);
    }
    private IEnumerator CarDoorProbe(Idas3SceneRenderer scene)
    {
        frozen=true;
        var reference=shots[shots.Count-1];
        ulong rendered=host.Status.renderedFrames,ticks=host.Status.simulationTicks;
        // Presets from the recorded actor at source frame(sample-1), targeting
        // the upper door at0.65m above its road origin from2.7m away.
        // Evidence: Verification/car-door-clipping-20260909/door-probe-poses.json.
        bool early=doorFrame==90;
        var eye=DiagnosticVector("-idas3-scene-door-eye",early?
            new Vector3(174.833321f,1130.899756f,-288.971582f):new Vector3(174.857426f,1130.899756f,-289.097609f));
        var target=DiagnosticVector("-idas3-scene-door-target",early?
            new Vector3(172.108496f,1130.649756f,-289.130735f):new Vector3(172.132457f,1130.649756f,-289.254261f));
        var secondEye=DiagnosticVector("-idas3-scene-door-eye2",early?
            new Vector3(168.078391f,1130.899756f,-290.379036f):new Vector3(168.101208f,1130.899756f,-290.498862f));
        var secondTarget=DiagnosticVector("-idas3-scene-door-target2",early?
            new Vector3(170.640033f,1130.649756f,-289.436703f):new Vector3(170.663714f,1130.649756f,-289.558881f));
        var eyes=new[]{eye,secondEye};var targets=new[]{target,secondTarget};
        float[] shifts={0,-.04f,-.02f,.02f,.04f};
        for(int side=0;side<2;++side){
            Check((targets[side]-eyes[side]).sqrMagnitude>.01f,"Door probe eye and target coincide.");
            var right=Vector3.Cross(Vector3.up,(eyes[side]-targets[side]).normalized).normalized;
            if(right.sqrMagnitude<.01f)right=Vector3.right;
            for(int i=0;i<shifts.Length;++i){
                frozen=true;var shift=right*shifts[i];
                SetDiagnosticView(scene,eyes[side]+shift,targets[side]+shift);scene.ApplyFrame();
                string name="door-side-"+side+(i==0?"-static":"-motion-"+(i-1).ToString("00"));
                yield return Capture(name,true);
                CheckFrozenDoorScene(reference,rendered,ticks);
            }
        }
        frozen=true;ClearDiagnosticView(scene);scene.ApplyFrame();
        yield return Capture("door-source-restored",true);
        CheckFrozenDoorScene(reference,rendered,ticks);
    }
    private void CheckFrozenDoorScene(Shot reference,ulong rendered,ulong ticks)
    {
        var shot=shots[shots.Count-1];
        Check(host.Status.renderedFrames==rendered&&host.Status.simulationTicks==ticks&&host.Status.attractChild==7,
            "Door camera-only probe advanced native state.");
        Check(shot.nativeVerticesSha256==reference.nativeVerticesSha256&&shot.nativeRangesSha256==reference.nativeRangesSha256&&
            shot.nativeFrameConstantsSha256==reference.nativeFrameConstantsSha256&&
            shot.nativeLightConstantsSha256==reference.nativeLightConstantsSha256&&shot.nativeFogConstantsSha256==reference.nativeFogConstantsSha256,
            "Door camera-only probe changed native geometry, frame, lighting or fog buffers.");
    }
    private IEnumerator IntroFoliageProbe(Idas3SceneRenderer scene)
    {
        frozen=true;
        var source=scene.CurrentFrame;ulong rendered=host.Status.renderedFrames,ticks=host.Status.simulationTicks;
        var vertices=PointerSha256(source.vertices,checked((int)source.vertexCount*64));
        var ranges=PointerSha256(source.ranges,checked((int)source.rangeCount*64));
        var constants=PointerSha256(source.frameConstants,184*4);
        var eye=DiagnosticVector("-idas3-scene-cull-eye",source.mainCamera.eye);
        var target=DiagnosticVector("-idas3-scene-cull-target",source.mainCamera.target);
        Check((target-eye).sqrMagnitude>.01f,"Intro foliage camera eye and target coincide.");
        var right=Vector3.Cross(Vector3.up,(eye-target).normalized).normalized;
        if(right.sqrMagnitude<.01f)right=Vector3.right;
        float[] offsets={0,-.08f,-.06f,-.04f,-.02f,.02f,.04f,.06f,.08f};
        for(int i=0;i<offsets.Length;++i){
            frozen=true;var shift=right*offsets[i];
            SetDiagnosticView(scene,eye+shift,target+shift);scene.ApplyFrame();
            yield return Capture(i==0?"intro-foliage-static":"intro-foliage-motion-"+(i-1).ToString("00"),true);
            var shot=shots[shots.Count-1];
            Check(host.Status.renderedFrames==rendered&&host.Status.simulationTicks==ticks&&host.Status.attractChild==7,
                "Intro foliage camera-only sequence advanced native state.");
            Check(shot.nativeVerticesSha256==vertices&&shot.nativeRangesSha256==ranges&&shot.nativeFrameConstantsSha256==constants,
                "Intro foliage camera-only sequence changed its native scene buffers.");
        }
        frozen=true;ClearDiagnosticView(scene);scene.ApplyFrame();
        yield return Capture("intro-foliage-source-restored",true);
        Check(host.Status.renderedFrames==rendered&&host.Status.simulationTicks==ticks,
            "Restoring the intro foliage camera advanced native state.");
        var restored=shots[shots.Count-1];
        Check(restored.nativeVerticesSha256==vertices&&restored.nativeRangesSha256==ranges&&restored.nativeFrameConstantsSha256==constants,
            "Restoring the intro foliage camera changed native scene buffers.");
    }
    private void CountPerformanceRender(Camera camera)
    {
        ++perfAllRenders;if(camera==perfMainCamera)++perfMainRenders;
    }
    private static PerfMetric SummarizePerformance(string name,PerfSample[] samples,Func<PerfSample,double> value)
    {
        var values=new List<double>(samples.Length);double sum=0;
        foreach(var sample in samples){double n=value(sample);if(n<0||double.IsNaN(n)||double.IsInfinity(n))continue;values.Add(n);sum+=n;}
        var result=new PerfMetric{name=name,count=values.Count};if(values.Count==0)return result;
        values.Sort();int count=values.Count;result.mean=sum/count;result.min=values[0];result.max=values[count-1];
        // Nearest-rank percentiles, kept identical for baseline and optimized runs.
        result.p50=values[Math.Max(0,(int)Math.Ceiling(count*.50)-1)];
        result.p95=values[Math.Max(0,(int)Math.Ceiling(count*.95)-1)];
        result.p99=values[Math.Max(0,(int)Math.Ceiling(count*.99)-1)];return result;
    }
    private IEnumerator PerformanceCheck()
    {
        var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();
        perfMainCamera=host.GetComponent<Camera>();
        Check(perfMainCamera.targetTexture==null,"Performance benchmark must use normal window rendering.");
        bool captureScreens=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-perf-no-captures")<0;
        yield return Frames(8);
        Check(Screen.width==2560&&Screen.height==1080&&host.Status.width==2560&&host.Status.height==1080,
            "Performance window did not reach 2560x1080.");
        int diagnosticAa=DiagnosticInt(Environment.GetCommandLineArgs(),"-idas3-perf-aa",4,0,8);
        QualitySettings.antiAliasing=diagnosticAa;
        bool offscreen=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-perf-offscreen")>=0;
        bool manualRender=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-perf-manual-render")>=0;
        if(offscreen){
            hires=new RenderTexture(2560,1080,24,RenderTextureFormat.ARGB32){name="Performance target",antiAliasing=diagnosticAa};
            Check(hires.Create(),"Performance target creation");perfMainCamera.targetTexture=hires;
        }
        if(manualRender){Check(offscreen,"Manual performance rendering needs its private target");perfMainCamera.enabled=false;}
        Check(QualitySettings.antiAliasing==diagnosticAa&&QualitySettings.vSyncCount==0&&Application.targetFrameRate==-1,
            "Performance benchmark must retain AA4 with uncapped diagnostic presentation.");
        bool aiCheck=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-ai-difficulty-check")>=0;
        if(aiCheck){host.GameOptions.BeginEdit();host.GameOptions.Draft.aiDifficulty=2;Check(host.GameOptions.ApplyDraft(),"Could not select Expert AI");}
        bool captureEnabled=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-perf-replay-off")<0;
        Check(Idas3SceneReplayCaptureDiagnostic(captureEnabled?1:0)==1,"Private capture comparison switch");
        yield return Key(116);yield return Frames(20);
        int startWait=0;
        while(host.Status.racePhase!=2&&startWait++<1800)yield return Frames(1);
        Check(host.Status.racePhase==2,"Performance setup did not finish showcase/countdown");
        var pause=host.GetComponent<Idas3PauseMenu>();pause.SetOpen(true);yield return Frames(4);
        Check(Idas3Native.Idas3SceneReturnToCourse()==1,"Performance setup return-to-course failed");
        pause.SetOpen(false);yield return Frames(30);
        Check((host.Status.flags&1)!=0&&host.Status.frontendStage==6&&host.Status.course==3,
            "Performance setup did not return from F5 to Akina Course6.");
        if(perfLegend){
            Check(perfCourse!=8,"Snow has no Legend race; use Time Attack for course8.");
            yield return Key(27);yield return Frames(100);
            Check(host.Status.frontendStage==5,"Performance setup did not reach Mode5.");
            yield return Key(13);yield return Frames(180);
            Check(host.Status.frontendStage==6,"Performance setup did not select Legend Course6.");
        }
        // Legend and Time Attack have different course lists. Read the actual
        // selected course rather than assuming a Time Attack carousel index.
        // Passing Snow may also change retained wet/night selections below.
        for(int i=0;Idas3SceneModeFlowValue(30)!=perfCourse&&i<12;++i){
            yield return Key(39);yield return Frames(6);
        }
        Check(Idas3SceneModeFlowValue(30)==perfCourse,"Performance course carousel did not select the requested course.");
        for(int choice=0;(host.Status.flags&1)!=0&&choice<6;++choice){
            int stage=host.Status.frontendStage;
            if(perfLegend&&stage==10&&perfWet){yield return Key(39);yield return Frames(8);yield return Key(39);yield return Frames(8);}
            Check(stage>=6&&stage<=10,"Performance setup reached an unexpected selection owner.");
            int conditions=Idas3SceneModeFlowValue(31);
            if(!perfLegend&&stage==7&&perfReverse){yield return Key(39);yield return Frames(6);}
            if(!perfLegend&&((stage==8&&perfWet!=((conditions&1)!=0))||(stage==9&&perfNight!=((conditions&2)!=0)))){yield return Key(39);yield return Frames(6);}
            yield return Key(13);yield return Frames(stage==10?240:60);
            if(perfLegend&&stage==10){
                for(int skip=0;(host.Status.flags&1)!=0&&skip<80;++skip){yield return PadStart();yield return Frames(30);}
            }
        }
        int wait=0;
        while(((host.Status.flags&1)!=0||host.Status.racePhase!=2)&&wait++<900)yield return null;
        Check(PerformanceCourse==perfCourse&&(host.Status.flags&1)==0&&(host.Status.flags&16)!=0&&host.Status.racePhase==2,
            "Performance benchmark did not start the requested race with original handling.");
        Check(Idas3SceneModeFlowValue(31)==((perfWet?1:0)|((perfNight||perfCourse==4||perfCourse==11)?2:0)),"Measured weather/time must match requested conditions");
        if(aiCheck){
            Check(Idas3SceneModeFlowValue(35)==(perfLegend?2:0),"Difficulty applied to wrong race mode");
            host.GameOptions.BeginEdit();host.GameOptions.Draft.aiDifficulty=0;Check(host.GameOptions.ApplyDraft(),"Could not restore Normal");
            Check(Idas3SceneModeFlowValue(34)==0&&Idas3SceneModeFlowValue(35)==(perfLegend?2:0),"Difficulty must stay fixed until the next battle");
            yield return Frames(600);Check(host.Ready,"Expert AI race failed");yield return Capture("ai-race",true);Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-hud-group-check")>=0){
            yield return Frames(240);
            if(perfLegend){
                for(int view=0;view<4&&!scene.MirrorCamera.enabled;++view){yield return Key(67);yield return Frames(4);}
                Check(scene.MirrorCamera.enabled,"Grouped HUD battle test requires an active mirror");
            }
            var fields=new[]{"original","hudTimerSize","hudSpeedometerSize","hudRecordsSize","hudLegendSize","hudOnlineSize","hudMirrorSize","hudTimeExtensionSize","mixed"};
            foreach(string field in fields){
                host.GameOptions.BeginEdit();var values=host.GameOptions.Draft;
                values.hudTimerSize=values.hudSpeedometerSize=values.hudRecordsSize=values.hudLegendSize=values.hudOnlineSize=values.hudMirrorSize=values.hudTimeExtensionSize=2;
                values.hudPositions=new Vector2[10];
                if(field=="mixed"){
                    values.hudTimerSize=0;values.hudSpeedometerSize=3;values.hudRecordsSize=1;values.hudLegendSize=1;values.hudOnlineSize=1;values.hudMirrorSize=0;
                    foreach(int group in new[]{1,2,3,4,5,6,7,8,9})values.hudPositions[group]=new Vector2(group%2==0?-.04f:.04f,.03f);
                }else if(field!="original")typeof(Idas3GameOptions.Values).GetField(field).SetValue(values,4);
                Check(host.GameOptions.ApplyDraft(),"Independent HUD apply failed");
                yield return Frames(3);
                host.GetComponent<Idas3UnityUi>().VerifyHudLayout(Check,perfLegend?6:3);
                if(scene.MirrorCamera.enabled){
                    var raw=scene.CurrentFrame.mirrorCamera.viewport;float scale=values.HudGroupScale(4);
                    var expected=new Rect(.5f+(raw.x/scene.CurrentFrame.width-.5f)*scale,1f-(raw.y+raw.w)/scene.CurrentFrame.height*scale,raw.z/scene.CurrentFrame.width*scale,raw.w/scene.CurrentFrame.height*scale);
                    expected.position+=new Vector2(values.HudOffset(4).x,-values.HudOffset(4).y);
                    Check(Vector4.Distance(new Vector4(expected.x,expected.y,expected.width,expected.height),new Vector4(scene.MirrorCamera.rect.x,scene.MirrorCamera.rect.y,scene.MirrorCamera.rect.width,scene.MirrorCamera.rect.height))<.0001f,"Mirror detached from its frame");
                }
                yield return Capture("independent-"+field,true);
            }
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-hud-map-check")>=0){
            for(int size=0;size<3;++size){
                host.GameOptions.BeginEdit();host.GameOptions.Draft.minimapSize=size;
                Check(host.GameOptions.ApplyDraft(),"Minimap size apply failed");
                yield return Frames(2);yield return Capture("minimap-"+(100+size*25),true);
            }
            foreach(int zoom in new[]{0,1}){
                host.GameOptions.BeginEdit();host.GameOptions.Draft.minimapZoom=zoom;
                Check(host.GameOptions.ApplyDraft(),"Minimap zoom apply failed");
                yield return Frames(2);yield return Capture("minimap-150-zoom-"+(50+zoom*25),true);
            }
            Finish(true,null);yield break;
        }
        if(perfFullDrive){
            Check(Idas3SceneCourseDriveDiagnostic(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-perf-timer-grace")>=0?2:1,driveTelemetry,12)==1,"Isolated full-course driver unavailable");
            Check((driveTelemetry[8]!=0)==perfReverse,"Wrong driving direction selected");
        }
        if(perfLegend){
            for(int i=0;i<4&&!scene.MirrorCamera.enabled;++i){yield return Key(67);yield return Frames(4);}
            Check(scene.MirrorCamera.enabled,"Legend performance race has no bumper mirror.");
        }
        var samples=new PerfSample[perfFrames];var timings=new FrameTiming[1];
        using var drawCalls=ProfilerRecorder.StartNew(ProfilerCategory.Render,"Draw Calls Count",1);
        using var batches=ProfilerRecorder.StartNew(ProfilerCategory.Render,"Batches Count",1);
        using var setPass=ProfilerRecorder.StartNew(ProfilerCategory.Render,"SetPass Calls Count",1);
        bool timingEnabled=FrameTimingManager.IsFeatureEnabled();
        var report=new PerfReport{unityVersion=Application.unityVersion,device=SystemInfo.graphicsDeviceName,developmentBuild=Debug.isDebugBuild,
            graphicsApi=SystemInfo.graphicsDeviceType.ToString(),mode=perfLegend?"Legend (authored rival conditions)":"Time Attack",
            settings=File.ReadAllText(Path.Combine(root,"userdata","settings.txt")).Trim(),
            commandLine=Environment.CommandLine,course=perfCourse,warmupFrames=perfWarmup,sampleFrames=perfFrames,
            width=Screen.width,height=Screen.height,antialiasing=QualitySettings.antiAliasing,vSync=QualitySettings.vSyncCount,
            targetFrameRate=Application.targetFrameRate,baseline=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-perf-baseline")>=0,
            nightRequested=perfNight,wetRequested=perfWet,frameTimingEnabled=timingEnabled,mirrorEnabled=scene.MirrorCamera.enabled,
            samples=samples,gcCollections=new int[GC.MaxGeneration+1]};
        report.fullDrive=perfFullDrive;report.reverseRequested=perfReverse;
        if(perfFullDrive)report.scope="Visible automatic rendering with original race physics, collisions, weather, recording and HUD; diagnostic route-following controls at fixed 60 Hz source steps. One source tick per rendered frame accelerates wall-clock traversal when FPS exceeds 60. No teleport or collision changes. Opt-in diagnostic timer grace and extension counts are reported; this validates rendering coverage, not race qualification. Completion and timeout reported separately. No screenshots or disk writes during timed driving.";
        if(offscreen)report.scope="CPU scene-submission benchmark at 2560x1080 AA4 with matched source ticks and cache-on/off runs. Hidden windows may skip automatic camera rendering; render-event counts are reported. No GPU or display FPS claim; screenshots separately verify rendered output.";
        if(manualRender)report.scope="Fixed-input private benchmark: explicit main Camera.Render to a 2560x1080 AA4 target every measured frame. Main camera automatic rendering disabled to avoid duplicate draws. Frame timing/counter data can lag; this is an offscreen workload, not display FPS. No captures or readbacks during measurement.";
        if(perfWet){
            bool hasRain=false;var weatherFrame=scene.CurrentFrame;
            for(int i=0;i<weatherFrame.rangeCount;++i)hasRain|=unchecked((uint)Marshal.ReadInt32(weatherFrame.ranges,i*64+12))==0x941024d2u;
            Check(hasRain,"Wet fixture submitted no rain geometry");if(captureScreens)yield return Capture("wet-race-start",true);
        }
        if(captureScreens&&perfLegend)yield return Capture("legend-mirror-start",true);
        if(captureScreens&&manualRender&&!perfWet&&!perfLegend)yield return Capture("dry-race-start",true);
        int frameCap=DiagnosticInt(Environment.GetCommandLineArgs(),"-idas3-perf-frame-cap",0,0,360);
        bool legacyCap=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-perf-legacy-cap")>=0;
        if(frameCap>0){
            Check(!manualRender&&!offscreen,"Frame pacing must measure normal automatic rendering.");
            Idas3FramePacingChecks.Run();
            // Exercise the real settings Apply path with only the cap changed.
            host.GameOptions.BeginEdit();host.GameOptions.Draft.vSync=false;host.GameOptions.Draft.frameRateLimit=frameCap;
            Check(host.GameOptions.ApplyDraft(),"Frame cap setting failed.");
            if(legacyCap){Idas3FramePacing.Configure(false,0);Application.targetFrameRate=frameCap;}
            report.requestedFrameCap=frameCap;report.legacyFrameCap=legacyCap;
            report.highResolutionFrameTimer=Idas3FramePacing.HighResolutionTimer;
            report.targetFrameRate=Application.targetFrameRate;
            Check(legacyCap?Application.targetFrameRate==frameCap:Idas3FramePacing.ActiveLimit==frameCap,"Requested frame limiter was not active.");
        }
        held=87;
        for(int i=0;i<perfWarmup;++i){if(timingEnabled)FrameTimingManager.CaptureFrameTimings();yield return null;if(manualRender)perfMainCamera.Render();}
        Check(host.Status.speedMetresPerSecond>1,"Performance warmup did not accelerate the original car.");
        if(captureScreens&&perfWet)yield return Capture("wet-moving-bumper",true);
        if(captureScreens&&perfLegend)yield return Capture("legend-mirror-moving",true);
        if(captureScreens&&manualRender&&!perfWet&&!perfLegend)yield return Capture("dry-moving-bumper",true);
        Camera.onPostRender+=CountPerformanceRender;
        report.audioBefore=Idas3UnityAudio.ReadStatistics();
        report.replayCaptureEnabled=captureEnabled;
        report.replayFramesBefore=Idas3SceneModeFlowValue(32);report.rivalReplayFramesBefore=Idas3SceneModeFlowValue(33);
        report.firstNativeFrame=host.Status.renderedFrames;report.firstSimulationTick=host.Status.simulationTicks;
        for(int generation=0;generation<report.gcCollections.Length;++generation)report.gcCollections[generation]=GC.CollectionCount(generation);
        ulong lastTiming=0,lastNative=host.Status.renderedFrames;
        long previousAllocated=GC.GetAllocatedBytesForCurrentThread();
        long beganTicks=System.Diagnostics.Stopwatch.GetTimestamp(),previousTicks=beganTicks;
        double tickMilliseconds=1000.0/System.Diagnostics.Stopwatch.Frequency;
        // All arrays and delegates are prepared before this loop. No captures,
        // readbacks, JSON serialization or filesystem traffic are performed.
        int measuredFrames=0;
        for(int i=0;i<perfFrames;++i){
            if(timingEnabled)FrameTimingManager.CaptureFrameTimings();
            yield return null;
            long renderStart=System.Diagnostics.Stopwatch.GetTimestamp();
            if(manualRender)perfMainCamera.Render();
            double renderMs=(System.Diagnostics.Stopwatch.GetTimestamp()-renderStart)*tickMilliseconds;
            long now=System.Diagnostics.Stopwatch.GetTimestamp(),allocated=GC.GetAllocatedBytesForCurrentThread();
            var status=host.Status;var source=scene.CurrentFrame;
            Check(PerformanceCourse==perfCourse&&(status.racePhase==2||perfFullDrive&&status.racePhase==3)&&(status.flags&1)==0&&(status.flags&16)!=0,
                "Performance measurement left the original-handling race.");
            Check(status.renderedFrames==lastNative+1,"Performance measurement skipped a native source frame.");
            var sample=new PerfSample{index=i,unityFrame=Time.frameCount,nativeFrame=status.renderedFrames,
                simulationTicks=status.simulationTicks,wallMs=(now-previousTicks)*tickMilliseconds,
                mainThreadAllocatedBytes=Math.Max(0,allocated-previousAllocated),submissionMs=host.SubmissionMilliseconds,
                nativeMs=host.NativeStepMilliseconds,rendererMs=host.RendererMilliseconds,uiMs=host.UiMilliseconds,
                meshes=scene.ActiveMeshCount,ranges=source.rangeCount,vertices=source.vertexCount,uiDraws=ui.DrawCount,
                uploadedVertices=scene.UploadedVertexCount,geometryUploads=scene.GeometryUploadCount,materialUpdates=scene.MaterialUpdateCount,
                depthCandidates=scene.DepthCandidateCount,depthDraws=scene.DepthDrawCount,depthRebuilds=scene.DepthBufferRebuildCount,
                drawCalls=drawCalls.Valid?drawCalls.LastValue:-1,batches=batches.Valid?batches.LastValue:-1,setPassCalls=setPass.Valid?setPass.LastValue:-1,sceneRenderMs=renderMs,
                speed=status.speedMetresPerSecond,focused=Application.isFocused,cpuFrameMs=-1,cpuMainMs=-1,cpuRenderMs=-1,gpuMs=-1};
            if(timingEnabled&&FrameTimingManager.GetLatestTimings(1,timings)>0){
                var timing=timings[0];
                if(timing.frameStartTimestamp!=lastTiming&&timing.cpuFrameTime>0){
                    lastTiming=timing.frameStartTimestamp;sample.frameTimingValid=true;sample.timingFrameStart=lastTiming;
                    sample.cpuFrameMs=timing.cpuFrameTime;sample.cpuMainMs=timing.cpuMainThreadFrameTime;
                    sample.cpuRenderMs=timing.cpuRenderThreadFrameTime;sample.gpuMs=timing.gpuFrameTime>0?timing.gpuFrameTime:-1;
                }
            }
            if(perfFullDrive){
                Check(Idas3SceneCourseDriveDiagnostic(-1,driveTelemetry,12)==1,"Driver telemetry unavailable");
                sample.courseDistance=driveTelemetry[0];sample.courseLength=driveTelemetry[1];sample.raceProgress=driveTelemetry[2];
                sample.wallContact=driveTelemetry[4]!=0;sample.travel=driveTelemetry[9];
                report.timerGraceAllowed=driveTelemetry[11]!=0;report.diagnosticTimeExtensions=(int)driveTelemetry[10];report.finishedRace=driveTelemetry[5]!=0;report.timeUp=driveTelemetry[6]!=0;
                report.finalDistance=driveTelemetry[0];report.courseLength=driveTelemetry[1];report.furthestRaceProgress=driveTelemetry[3];
            }
            samples[i]=sample;measuredFrames=i+1;lastNative=status.renderedFrames;previousTicks=now;previousAllocated=allocated;
            if(perfFullDrive&&report.finishedRace)break;
        }
        if(perfFullDrive){Idas3SceneCourseDriveDiagnostic(0,null,0);Array.Resize(ref samples,measuredFrames);report.samples=samples;report.sampleFrames=measuredFrames;}
        report.measuredSeconds=(previousTicks-beganTicks)/(double)System.Diagnostics.Stopwatch.Frequency;
        Camera.onPostRender-=CountPerformanceRender;held=0;
        report.lastNativeFrame=host.Status.renderedFrames;report.lastSimulationTick=host.Status.simulationTicks;
        report.replayFramesAfter=Idas3SceneModeFlowValue(32);report.rivalReplayFramesAfter=Idas3SceneModeFlowValue(33);
        Check(captureEnabled?report.replayFramesAfter>report.replayFramesBefore:report.replayFramesAfter==0,"Capture on/off comparison took effect");
        report.mainCameraRenderEvents=perfMainRenders;report.allCameraRenderEvents=perfAllRenders;
        report.audioAfter=Idas3UnityAudio.ReadStatistics();
        for(int generation=0;generation<report.gcCollections.Length;++generation)
            report.gcCollections[generation]=GC.CollectionCount(generation)-report.gcCollections[generation];
        report.metrics=new[]{
            SummarizePerformance("wallMs",samples,s=>s.wallMs),SummarizePerformance("submissionMs",samples,s=>s.submissionMs),
            SummarizePerformance("nativeMs",samples,s=>s.nativeMs),SummarizePerformance("rendererMs",samples,s=>s.rendererMs),
            SummarizePerformance("uiMs",samples,s=>s.uiMs),SummarizePerformance("cpuFrameMs",samples,s=>s.cpuFrameMs),
            SummarizePerformance("cpuMainMs",samples,s=>s.cpuMainMs),SummarizePerformance("cpuRenderMs",samples,s=>s.cpuRenderMs),
            SummarizePerformance("gpuMs",samples,s=>s.gpuMs),SummarizePerformance("mainThreadAllocatedBytes",samples,s=>s.mainThreadAllocatedBytes),
            SummarizePerformance("drawCalls",samples,s=>s.drawCalls),SummarizePerformance("batches",samples,s=>s.batches),SummarizePerformance("setPassCalls",samples,s=>s.setPassCalls),SummarizePerformance("sceneRenderMs",samples,s=>s.sceneRenderMs),
            SummarizePerformance("meshes",samples,s=>s.meshes),SummarizePerformance("ranges",samples,s=>s.ranges),
            SummarizePerformance("vertices",samples,s=>s.vertices),SummarizePerformance("uploadedVertices",samples,s=>s.uploadedVertices),
            SummarizePerformance("geometryUploads",samples,s=>s.geometryUploads),SummarizePerformance("materialUpdates",samples,s=>s.materialUpdates),
            SummarizePerformance("uiDraws",samples,s=>s.uiDraws)};
        File.WriteAllText(Path.Combine(root,"performance.json"),JsonUtility.ToJson(report,true));
        if(manualRender)Check(perfMainRenders==measuredFrames,"Every measured frame must render exactly once");
        if(!offscreen)Check(perfMainRenders>=measuredFrames-2,"Unity skipped normal camera rendering during the benchmark; timing is not representative.");
        else Check(report.lastNativeFrame-report.firstNativeFrame==(ulong)measuredFrames,"CPU benchmark skipped scene submissions");
        if(perfFullDrive)Check(report.finishedRace&&!report.timeUp,"Course driver did not finish the race; do not count this as full-track coverage.");
        Check(report.audioAfter.consumedFrames>report.audioBefore.consumedFrames,"Unity audio did not run during performance measurement.");
        if(frameCap>0)Idas3FramePacingChecks.RunPlatform(host.GameOptions);
        Finish(true,null);
    }
    // Course-selectable depth/sorting investigation. All capture machinery and
    // isolated saves are shared with CullCheck; no ordinary game path uses it.
    private IEnumerator DepthCheck()
    {
        var scene=host.GetComponent<Idas3SceneRenderer>();var main=host.GetComponent<Camera>();
        hires=new RenderTexture(2560,1080,24,RenderTextureFormat.ARGB32){name="Depth regression 2560x1080",antiAliasing=1};
        Check(hires.Create(),"Depth verification render target could not be created.");
        main.targetTexture=hires;yield return Frames(4);
        yield return Key(116);yield return Frames(20);
        // The current shortcut includes the original car showcase. Pause and
        // Back are intentionally ignored until that owner releases the race.
        int startWait=0;
        while(host.Status.racePhase!=2&&startWait++<1800)yield return Frames(1);
        Check(host.Status.racePhase==2,"Depth setup did not finish the shortcut showcase/countdown.");
        // Exercise the same command used by the managed pause menu. Its input
        // bindings intentionally consume raw Escape/Back before native input.
        var pause=host.GetComponent<Idas3PauseMenu>();pause.SetOpen(true);yield return Frames(4);
        Check(Idas3Native.Idas3SceneReturnToCourse()==1,"Depth setup could not return through the pause command.");
        pause.SetOpen(false);yield return Frames(30);
        Check((host.Status.flags&1)!=0&&host.Status.frontendStage==6&&host.Status.course==3,
            "Depth setup did not return from the Akina shortcut to Course6.");
        // Follow the course selected by the carousel; its display order is
        // intentionally different from the native course indices.
        for(int i=0;host.Status.course!=depthCourse&&i<11;++i){yield return Key(39);yield return Frames(6);}
        Check(host.Status.course==depthCourse,"Depth setup could not select the requested course.");
        // Snow starts after Route; Happogahara starts after Weather. The
        // native owner chooses those exits, so stop when it leaves the menu.
        for(int choice=0;(host.Status.flags&1)!=0&&choice<6;++choice){
            Check(host.Status.frontendStage>=6&&host.Status.frontendStage<=9,
                "Depth setup reached an unexpected Time Attack selection stage.");
            var selections=Environment.GetCommandLineArgs();
            if((host.Status.frontendStage==7&&Array.IndexOf(selections,"-idas3-scene-depth-reverse")>=0)||
               (host.Status.frontendStage==8&&Array.IndexOf(selections,"-idas3-scene-depth-wet")>=0)||
               (host.Status.frontendStage==9&&Array.IndexOf(selections,"-idas3-scene-depth-night")>=0)){
                yield return Key(39);yield return Frames(6);
            }
            yield return Key(13);yield return Frames(60);
        }
        yield return Frames(180);
        Check(host.Status.course==depthCourse&&(host.Status.flags&1)==0&&(host.Status.flags&16)!=0,
            "Depth check did not start the selected course with original handling.");
        startWait=0;
        while(host.Status.racePhase!=2&&startWait++<1800)yield return Frames(1);
        Check(host.Status.racePhase==2,"Depth setup did not finish the selected course showcase/countdown.");
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-weather-reduced")>=0){
            Check(Idas3Native.Idas3SceneSetPerformance(1)==1,"Reduced weather setting failed.");yield return Frames(4);
        }
        yield return Capture("depth-timeattack-bumper",true);
        yield return Key(67);yield return Frames(12);
        yield return Capture("depth-timeattack-chase",true);
        if(depthCourse!=8){yield return Key(67);yield return Frames(12);}
        held=87;
        for(int i=0;i<12;++i){
            yield return Frames(120);
            Check((host.Status.flags&1)==0&&host.Status.course==depthCourse,"Depth drive left the selected race.");
            yield return Capture("depth-driving-"+i.ToString("00"),true);
            if(i==2&&Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-depth-wet")>=0){
                held=0;pause.SetOpen(true);yield return Frames(4);
                yield return Capture("wet-paused-a",true);yield return Frames(60);yield return Capture("wet-paused-b",true);
                pause.SetOpen(false);yield return Frames(6);yield return Key(67);yield return Frames(12);held=87;
            }
        }
        held=0;
        var args=Environment.GetCommandLineArgs();
        bool hasEye=Array.IndexOf(args,"-idas3-scene-cull-eye")>=0;
        bool hasTarget=Array.IndexOf(args,"-idas3-scene-cull-target")>=0;
        if(hasEye||hasTarget){
            Check(hasEye&&hasTarget,"Depth camera probe requires both eye and target arguments.");
            var eye=DiagnosticVector("-idas3-scene-cull-eye",Vector3.zero);
            var target=DiagnosticVector("-idas3-scene-cull-target",Vector3.zero);
            Check((target-eye).sqrMagnitude>.01f,"Depth diagnostic eye and target coincide.");
            frozen=true;ulong tick=host.Status.simulationTicks;
            SetDiagnosticView(scene,eye,target);scene.ApplyFrame();
            yield return Capture("depth-camera-static",true);
            for(int i=0;i<4;++i){
                frozen=true;SetDiagnosticView(scene,eye+new Vector3(i*.025f,0,0),target);scene.ApplyFrame();
                yield return Capture("depth-camera-motion-"+i.ToString("00"),true);
                Check(host.Status.simulationTicks==tick,"Depth camera-only sequence advanced physics.");
            }
            frozen=true;ClearDiagnosticView(scene);scene.ApplyFrame();
            yield return Capture("depth-source-view-restored",true);
        }
        main.targetTexture=null;frozen=false;yield return Frames(2);
        hires.Release();Destroy(hires);hires=null;
        Finish(true,null);
    }
    // Opt-in matched-scene regression. Run the same player twice, once with
    // the renderer's cull-baseline flag and once normally. Every capture keeps
    // source geometry/camera hashes so visual differences cannot be credited
    // to another race position, resolution, source asset or simulation tick.
    private IEnumerator CullCheck()
    {
        var scene=host.GetComponent<Idas3SceneRenderer>();var main=host.GetComponent<Camera>();
        bool noMsaa=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-scene-cull-no-msaa")>=0;
        hires=new RenderTexture(2560,1080,24,RenderTextureFormat.ARGB32){name="Culling regression 2560x1080",antiAliasing=noMsaa?1:Mathf.Max(1,QualitySettings.antiAliasing)};
        Check(hires.Create(),"Culling verification render target could not be created.");
        main.targetTexture=hires;yield return Frames(4);
        Check(host.Status.width==2560&&host.Status.height==1080,"Culling check output dimensions changed.");
        // F5 is intentionally an Akina shortcut in the native host. Return
        // through the genuine course owner instead of assuming saves override
        // that shortcut, then select Myogi and confirm its three race choices.
        yield return Key(116);yield return Frames(20);
        yield return Key(27);yield return Frames(4);
        yield return Key(8);yield return Frames(30);
        Check((host.Status.flags&1)!=0&&host.Status.frontendStage==6&&host.Status.course==3,
            "Culling setup did not return from the Akina shortcut to Course6.");
        for(int i=0;i<3;++i){yield return Key(37);yield return Frames(6);}
        Check(host.Status.course==0,"Culling setup could not select Myogi from the course carousel.");
        for(int stage=6;stage<=9;++stage){
            Check(host.Status.frontendStage==stage&&(host.Status.flags&1)!=0,
                "Culling setup expected Time Attack selection stage "+stage+".");
            yield return Key(13);yield return Frames(60);
        }
        yield return Frames(180);
        Check(host.Status.course==0&&(host.Status.flags&1)==0&&(host.Status.flags&16)!=0,
            "Culling check did not start Myogi with original handling.");
        yield return Capture("cull-timeattack-bumper",true);
        yield return Key(67);yield return Frames(12);
        yield return Capture("cull-timeattack-chase",true);

        // Use the existing source menu route, because the rear-view mirror is
        // intentionally enabled for Legend battles, not Time Attack.
        yield return Key(27);yield return Frames(4);
        yield return Key(8);yield return Frames(30);
        Check((host.Status.flags&1)!=0&&host.Status.frontendStage==6,"Culling check could not return to Course6.");
        yield return Key(27);yield return Frames(100);
        Check(host.Status.frontendStage==5,"Culling check could not reach Mode5.");
        yield return Key(13);yield return Frames(180);
        Check(host.Status.frontendStage==6,"Culling check could not select Legend Course6.");
        yield return Key(13);yield return Frames(45);
        Check(host.Status.frontendStage==10,"Culling check could not select the Myogi Rival10.");
        yield return Key(13);yield return Frames(240);
        // Current Legend selection includes the source pre-race dialogue.
        // Its Start button skips pages. Use gamepad Start: Escape also reaches
        // the underlying menu's Back binding in the present native host.
        for(int skip=0;(host.Status.flags&1)!=0&&skip<80;++skip){yield return PadStart();yield return Frames(30);}
        yield return Frames(180);
        Check(host.Status.course==0&&(host.Status.flags&1)==0,"Culling check Legend race is not Myogi.");
        for(int i=0;i<4&&!scene.MirrorCamera.enabled;++i){yield return Key(67);yield return Frames(4);}
        Check(scene.MirrorCamera.enabled&&scene.CurrentFrame.viewCount==2,"Culling check could not obtain the actual bumper mirror.");
        yield return Capture("cull-legend-bumper-mirror",true);
        yield return Key(67);yield return Frames(12);
        yield return Capture("cull-legend-chase",true);
        held=87;
        for(int i=0;i<4;++i){yield return Frames(3);yield return Capture("cull-driving-"+i.ToString("00"),true);}
        held=0;

        // Chunk2 is in Myogi's initial source assembly. These coordinates are
        // taken from the exact front/back texture106 triangles in the audit.
        // Freeze native simulation; only this explicit diagnostic view moves.
        var eye=DiagnosticVector("-idas3-scene-cull-eye",new Vector3(462,79,-437));
        var target=DiagnosticVector("-idas3-scene-cull-target",new Vector3(472,78,-435));
        Check((target-eye).sqrMagnitude>.01f,"Culling diagnostic eye and target coincide.");
        frozen=true;ulong tick=host.Status.simulationTicks;
        for(int side=0;side<2;++side){
            var sideEye=side==0?eye:target+(target-eye);
            frozen=true;SetDiagnosticView(scene,sideEye,target);scene.ApplyFrame();
            yield return Capture("cull-tree106-side-"+side,true);
            Check(host.Status.simulationTicks==tick,"Static culling close-up advanced physics.");
        }
        for(int i=0;i<8;++i){
            frozen=true;SetDiagnosticView(scene,eye+new Vector3(i*.025f,0,0),target);scene.ApplyFrame();
            yield return Capture("cull-tree106-motion-"+i.ToString("00"),true);
            Check(host.Status.simulationTicks==tick,"Camera-only culling sequence advanced physics.");
        }
        frozen=true;ClearDiagnosticView(scene);scene.ApplyFrame();
        yield return Capture("cull-source-view-restored",true);
        main.targetTexture=null;frozen=false;yield return Frames(2);
        hires.Release();Destroy(hires);hires=null;
        Finish(true,null);
    }
    // A capture that leaves the game running, unlike Capture(), which freezes it.
    private IEnumerator Moving(string name)
    {
        yield return new WaitForEndOfFrame();
        var camera=host.GetComponent<Camera>();var target=camera.targetTexture;
        Texture2D shot;
        if(target!=null)
        {
            var prior=RenderTexture.active;
            try{RenderTexture.active=target;shot=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
                shot.ReadPixels(new Rect(0,0,target.width,target.height),0,0);shot.Apply();}
            finally{RenderTexture.active=prior;}
        }
        else shot=ScreenCapture.CaptureScreenshotAsTexture();
        Check(shot!=null,"Moving capture failed.");
        File.WriteAllBytes(Path.Combine(root,name+".png"),shot.EncodeToPNG());
        Destroy(shot);
    }
    private IEnumerator Flicker()
    {
        int wait=0;while(host.Status.attractChild!=7&&wait++<1800)yield return null;
        yield return Key(116);yield return Frames(20);
        // Chase view: the car keeps the same screen area, so anything that
        // changes on it is not motion.
        yield return Key(67);yield return Frames(10);
        held=87;yield return Frames(420);held=0;
        yield return Frames(90);
        var log=new StringWriter();
        log.WriteLine("phase,frame,changed_pixels,total_pixels,changed_fraction,car_changed,car_total,car_fraction");
        yield return Key(27);yield return Frames(5);
        Check((host.Status.flags&2)!=0,"The flicker check could not pause the race.");
        yield return Sequence("paused",8,log);
        yield return Key(27);yield return Frames(5);
        Check((host.Status.flags&2)==0,"The flicker check could not resume the race.");
        // Crawl: brake almost to a stop so the scene barely moves. Anything that
        // still changes is not motion.
        held=83;int brake=0;
        while(host.Status.speedMetresPerSecond>1.5f&&brake++<900)yield return null;
        held=0;yield return Frames(2);
        Debug.Log("IDAS3 flicker crawl speed "+host.Status.speedMetresPerSecond);
        if(hiresFlicker)
        {
            hires=new RenderTexture(2560,1440,24,RenderTextureFormat.ARGB32);
            Check(hires.Create(),"High resolution target could not be created.");
            host.GetComponent<Camera>().targetTexture=hires;
            yield return Frames(4);
            Debug.Log("IDAS3 hires target "+host.Status.width+"x"+host.Status.height);
        }
        yield return Sequence("crawling",12,log);
        if(hiresFlicker){host.GetComponent<Camera>().targetTexture=null;yield return Frames(2);hires.Release();}
        // At speed the scene enters and leaves the frustum, which a crawl never
        // exercises. Culling errors only show here.
        held=87;yield return Frames(420);
        Debug.Log("IDAS3 driving speed "+host.Status.speedMetresPerSecond);
        // Simulation steps per presented frame, with the game's own real delta.
        // A 60 Hz simulation presented at 120 Hz alternates 1 and 0.
        realDelta=true;
        var steps=new System.Collections.Generic.Dictionary<ulong,int>();
        ulong prior=Idas3Native.ReadStatus().simulationTicks;
        double t0=Time.realtimeSinceStartupAsDouble;int presented=0;
        for(int i=0;i<600;++i)
        {
            yield return null;++presented;
            var now=Idas3Native.ReadStatus().simulationTicks;
            ulong step=now-prior;prior=now;
            steps.TryGetValue(step,out int n);steps[step]=n+1;
        }
        double seconds=Time.realtimeSinceStartupAsDouble-t0;
        // Frame-time spread, which is what an inconsistent rate actually is.
        var times=new System.Collections.Generic.List<double>();
        long gcBefore=System.GC.GetTotalMemory(false);
        int collections=System.GC.CollectionCount(0);
        double last=Time.realtimeSinceStartupAsDouble;
        for(int i=0;i<600;++i)
        {
            yield return null;
            double now=Time.realtimeSinceStartupAsDouble;
            times.Add((now-last)*1000.0);last=now;
        }
        times.Sort();
        long gcAfter=System.GC.GetTotalMemory(false);
        var ft=new StringWriter();
        ft.WriteLine("measure,milliseconds");
        ft.WriteLine("median,"+times[times.Count/2].ToString("F3",System.Globalization.CultureInfo.InvariantCulture));
        ft.WriteLine("p95,"+times[(int)(times.Count*0.95)].ToString("F3",System.Globalization.CultureInfo.InvariantCulture));
        ft.WriteLine("p99,"+times[(int)(times.Count*0.99)].ToString("F3",System.Globalization.CultureInfo.InvariantCulture));
        ft.WriteLine("max,"+times[times.Count-1].ToString("F3",System.Globalization.CultureInfo.InvariantCulture));
        double med=times[times.Count/2];
        int late=0;foreach(var v in times)if(v>med*1.5)++late;
        ft.WriteLine("frames_over_1.5x_median,"+late);
        ft.WriteLine("gc_collections,"+(System.GC.CollectionCount(0)-collections));
        ft.WriteLine("managed_bytes_growth,"+(gcAfter-gcBefore));
        ft.WriteLine("active_meshes,"+host.GetComponent<Idas3SceneRenderer>().ActiveMeshCount);
        File.WriteAllText(Path.Combine(root,"frametimes.csv"),ft.ToString());
        var text=new StringWriter();
        text.WriteLine("simulation_steps_per_presented_frame,count");
        foreach(var pair in steps)text.WriteLine(pair.Key+","+pair.Value);
        text.WriteLine("presented_fps,"+(presented/seconds).ToString("F2",System.Globalization.CultureInfo.InvariantCulture));
        text.WriteLine("display_refresh,"+Screen.currentResolution.refreshRateRatio.value.ToString("F2",System.Globalization.CultureInfo.InvariantCulture));
        text.WriteLine("vSyncCount,"+QualitySettings.vSyncCount);
        text.WriteLine("targetFrameRate,"+Application.targetFrameRate);
        File.WriteAllText(Path.Combine(root,"steps.csv"),text.ToString());
        yield return Frames(60);   // stay on real deltas through the driving capture
        yield return Sequence("driving",12,log);
        held=0;
        File.WriteAllText(Path.Combine(root,"flicker.csv"),log.ToString());
        Idas3SceneRenderer.DepthCensus=new System.Collections.Generic.Dictionary<string,int>();
        yield return Frames(3);
        var census=Idas3SceneRenderer.DepthCensus;Idas3SceneRenderer.DepthCensus=null;
        var lines=new System.Collections.Generic.List<string>();
        foreach(var pair in census)lines.Add(pair.Key+","+pair.Value);
        lines.Sort();
        File.WriteAllText(Path.Combine(root,"depth-census.csv"),"mode,ranges_over_3_frames\n"+string.Join("\n",lines)+"\n");
        Finish(true,null);
    }
    private IEnumerator Sequence(string phase,int count,StringWriter log)
    {
        Color32[] previous=null;int width=0,height=0;
        for(int i=0;i<count;++i)
        {
            yield return new WaitForEndOfFrame();
            var cam=host.GetComponent<Camera>().transform;
            Debug.Log("IDAS3 flicker "+phase+" "+i+" camera "+cam.position.ToString("F5")
                +" euler "+cam.eulerAngles.ToString("F4")+" speed "+host.Status.speedMetresPerSecond.ToString("F5")
                +" ticks "+host.Status.simulationTicks);
            Texture2D shot;
            var target=host.GetComponent<Camera>().targetTexture;
            if(target!=null)
            {
                var prior=RenderTexture.active;
                try{RenderTexture.active=target;shot=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
                    shot.ReadPixels(new Rect(0,0,target.width,target.height),0,0);shot.Apply();}
                finally{RenderTexture.active=prior;}
            }
            else shot=ScreenCapture.CaptureScreenshotAsTexture();
            Check(shot!=null,"Screen capture failed during the flicker check.");
            var pixels=shot.GetPixels32();width=shot.width;height=shot.height;
            if(previous!=null)
            {
                int changed=0,carChanged=0,carTotal=0;
                // GetPixels32 is bottom-up; the car sits low and centred.
                int x0=width*38/100,x1=width*62/100,y0=height*8/100,y1=height*38/100;
                for(int j=0;j<pixels.Length;++j)
                {
                    var a=previous[j];var b=pixels[j];
                    bool differs=Mathf.Abs(a.r-b.r)+Mathf.Abs(a.g-b.g)+Mathf.Abs(a.b-b.b)>12;
                    if(differs)++changed;
                    int x=j%width,y=j/width;
                    if(x>=x0&&x<x1&&y>=y0&&y<y1){++carTotal;if(differs)++carChanged;}
                }
                log.WriteLine(phase+","+i+","+changed+","+pixels.Length+","+
                    ((double)changed/pixels.Length).ToString("F6",System.Globalization.CultureInfo.InvariantCulture)+","+
                    carChanged+","+carTotal+","+
                    ((double)carChanged/Mathf.Max(1,carTotal)).ToString("F6",System.Globalization.CultureInfo.InvariantCulture));
            }
            File.WriteAllBytes(Path.Combine(root,phase+"-"+i+".png"),shot.EncodeToPNG());
            // A map of what changed, so the unstable regions can be located.
            if(previous!=null&&i==1)
            {
                var map=new Texture2D(width,height,TextureFormat.RGB24,false);
                var marks=new Color32[pixels.Length];
                for(int j=0;j<pixels.Length;++j)
                {
                    var a=previous[j];var b=pixels[j];
                    int d=Mathf.Abs(a.r-b.r)+Mathf.Abs(a.g-b.g)+Mathf.Abs(a.b-b.b);
                    byte v=(byte)Mathf.Clamp(d*3,0,255);
                    marks[j]=d>12?new Color32(v,0,0,255):new Color32((byte)(b.r/3),(byte)(b.g/3),(byte)(b.b/3),255);
                }
                map.SetPixels32(marks);map.Apply();
                File.WriteAllBytes(Path.Combine(root,phase+"-change-map.png"),map.EncodeToPNG());
                Destroy(map);
            }
            previous=pixels;Destroy(shot);
        }
        Debug.Log("IDAS3 flicker phase "+phase+" at "+width+"x"+height);
    }
    // Rival 2 is the first Legend opponent raced at night, so two accepted
    // post-result owners reach a race with two cars and two projected beams.
    private IEnumerator Headlights()
    {
        int wait=0;while(host.Status.attractChild!=7&&wait++<1800)yield return null;
        yield return Key(116);yield return Frames(20);
        yield return Key(27);yield return Frames(10);
        yield return Key(8);yield return Frames(60);
        yield return Key(27);yield return Frames(100);
        Check(host.Status.frontendStage==4,"Mode menu was not reached.");
        yield return Key(13);yield return Frames(180);
        yield return Key(13);yield return Frames(45);
        Check(host.Status.frontendStage==9,"Legend rival selection failed.");
        yield return Key(13);yield return Frames(180);
        Check((host.Status.flags&1)==0,"Legend battle did not start.");
        for(int race=0;race<2;++race)
        {
            yield return TimeOutAndAccept();
            Check((host.Status.flags&1)==0,"Accepting did not start the next battle.");
        }
        // Now on the night rival: let the countdown clear, then capture with the
        // opponent still alongside so the two beams overlap on the road.
        yield return Frames(240);
        yield return Key(67);yield return Frames(10);
        yield return Capture("headlights-chase",true);
        yield return Key(67);yield return Frames(10);
        held=87;yield return Frames(60);held=0;
        yield return Capture("headlights-bumper",true);
        Finish(true,null);
    }
    private IEnumerator TimeOutAndAccept()
    {
        double start=Time.realtimeSinceStartupAsDouble;
        while((host.Status.flags&32)==0)
        {
            if(Time.realtimeSinceStartupAsDouble-start>600)break;
            if(host.Status.racePhase==3)pulse=13;
            yield return null;
        }
        Check((host.Status.flags&32)!=0,"The post-result owner never started.");
        held=27;int skip=0;
        while((host.Status.flags&64)==0&&skip++<12000)yield return null;
        held=0;
        Check((host.Status.flags&64)!=0,"No choice prompt appeared.");
        yield return Key(13);yield return Frames(60);
        if((host.Status.flags&64)!=0){yield return Key(13);}
        int settle=0;while((host.Status.flags&32)!=0&&settle++<3600)yield return null;
        yield return Frames(120);
    }
    // The opening demo drives itself, so consecutive frames differ only by its
    // own motion. Anything that changes far more than that is not motion.
    private IEnumerator AttractFlicker()
    {
        int wait=0;while(host.Status.attractChild!=7&&wait++<1800)yield return null;
        Check(host.Status.attractChild==7,"Attract driving scene was not reached.");
        yield return Frames(240);
        realDelta=true;
        // How many source updates each presented frame consumes. A fixed 60 Hz
        // simulation shown on a 60 Hz display should take exactly one.
        realDelta=true;
        var cadence=new System.Collections.Generic.Dictionary<ulong,int>();
        ulong last=Idas3Native.ReadStatus().renderedFrames;
        for(int i=0;i<600;++i)
        {
            yield return null;
            var now=Idas3Native.ReadStatus().renderedFrames;
            ulong step=now-last; last=now;
            cadence.TryGetValue(step,out int n); cadence[step]=n+1;
        }
        var cadenceText=new StringWriter();
        cadenceText.WriteLine("updates_per_presented_frame,count");
        foreach(var pair in cadence)cadenceText.WriteLine(pair.Key+","+pair.Value);
        File.WriteAllText(Path.Combine(root,"cadence.csv"),cadenceText.ToString());
        Debug.Log("IDAS3 attract cadence written");
        // Back to fixed steps so the captured sequence is reproducible.
        yield return Frames(120);
        var log=new StringWriter();
        log.WriteLine("phase,frame,changed_pixels,total_pixels,changed_fraction,car_changed,car_total,car_fraction");
        yield return Sequence("attract",12,log);
        File.WriteAllText(Path.Combine(root,"flicker.csv"),log.ToString());
        Finish(true,null);
    }
    private void Finish(bool passed,string error)
    {
        if(finished)return;finished=true;frozen=true;
        if(perfCheck)Camera.onPostRender-=CountPerformanceRender;
        host.StopNative();submitTimes.Sort();
        bool stopped=false;
        try { ++checks; stopped=!host.Ready&&Idas3Native.ReadStatus().state==0; }
        catch(Exception exception) { error=(error==null?"":error+"\n")+"Cannot verify native shutdown: "+exception.Message; }
        if(!stopped){passed=false;error=(error==null?"":error+"\n")+"Native scene did not report a stopped state.";}
        var report=new Report{passed=passed,error=error,shutdownComplete=stopped,checks=checks,
            unityVersion=Application.unityVersion,device=SystemInfo.graphicsDeviceName,
            elapsedSeconds=Time.realtimeSinceStartupAsDouble-began,captures=shots.ToArray()};
        if(perfCheck)report.scope="Opt-in race performance benchmark with isolated saves and original handling; automatic 2560x1080 AA4 window rendering, vSync0, fixed source ticks, held-throttle warmup and measurement. See performance.json for sample rows, percentile summaries, frame timing availability, camera render events and allocation scope. No per-frame captures or readbacks.";
        if(cullCheck)report.scope="Opt-in Myogi culling regression at 2560x1080: source Time Attack bumper/chase, Legend bumper/mirror/chase, fixed-step driving, and camera-only tree106 close-ups. Source buffers are hashed for matched baseline/fix comparison. A passing harness establishes execution and capture invariants; visual comparison is separate.";
        if(depthCheck)report.scope="Opt-in depth/sorting investigation on course "+depthCourse+" at 2560x1080 with diagnostic AA1/manual camera rendering: Time Attack bumper/chase and twelve fixed 120-frame driving intervals, plus an optional frozen camera probe. Native geometry hashes and source depth/list counts permit matched comparisons. A passing harness establishes capture invariants, not visual correctness.";
        if(introCheck)report.scope="Opt-in source intro child7 car investigation at 2560x1080 with diagnostic AA1/manual camera rendering. Logo cards3-5 use15 source ticks per update; child6 and the captured intro use single ticks. Captures span early camera cuts, consecutive body-motion frames, and headlight transitions. Intro-relative native update counts, source geometry hashes, range/depth census and selected geometry/lighting dumps support matched visual audits. Passing establishes capture invariants, not car visual correctness.";
        if(introFoliageCheck)report.scope+=" At source update900, the native scene is frozen for a static foliage view, eight camera translations in0.02m steps, and source-view restoration. Each probe asserts unchanged native frame/tick counters and source buffer hashes. Metadata distinguishes the actual diagnostic camera from its original source camera.";
        if(carDoorCheck)report.scope+=" At source update"+doorFrame+", both upper-door/window edges are inspected in a frozen scene: each side has a static view and four small camera translations, followed by source-view restoration. Every probe checks native frame/tick counters plus geometry, frame, lighting and fog hashes. Both static side views include binary scene/lighting dumps; camera metadata distinguishes diagnostic and original poses.";
        if(rivalCheck)report.scope="Opt-in rival intro/prompt regression with a NEW private checksummed profile, seeded rival record "+rivalRecord+". Real source menu owners select Myogi rival0; original intro cue, advancing music cursor, actual pre-listener Unity PCM and visible AA1/manual screenshots are checked. Optional post-result coverage uses a naturally timed-out stationary race, then actual Continue/rematch selection, confirmation and race release. A pass verifies owner/audio/capture invariants; single-prompt artwork correctness requires visual review and native painter tests. See rival-observations.json and PCM WAV files.";
        if(submitTimes.Count>0){report.medianSubmissionMs=submitTimes[submitTimes.Count/2];report.p95SubmissionMs=submitTimes[Math.Min(submitTimes.Count-1,(int)(submitTimes.Count*.95))];}
        if(retireCheck)report.scope="Isolated Legend retirement through actual managed pause commands, timeout cutscene, Continue/rematch refusal, rendered reply and return to Course6. Native tests separately check once-only loss/progress and online rejection.";
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(report,true));
        Debug.Log((passed?"PASS":"FAIL")+" Unity scene smoke: "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(passed?0:1);
#endif
    }
}

// Diagnostic-only passive filter, appended after Idas3UnityAudio on its source
// object. It neither changes samples nor drains the native PCM ring itself.
public sealed class Idas3RivalAudioProbe : MonoBehaviour
{
    [Serializable] public sealed class Result {
        public int sampleRate,channels,samples;
        public double seconds,peak,rms;
        public string file,position="After Idas3UnityAudio source filter, before muted AudioListener";
    }
    private readonly object gate=new object();
    private float[] captured;
    private bool recording;
    private int count,rate,channelCount,target;
    private double duration;
    public bool Complete {get{lock(gate)return captured!=null&&!recording&&count>0;}}
    public void Begin(int sampleRate,double seconds){
        lock(gate){rate=sampleRate;duration=seconds;captured=new float[checked((int)(sampleRate*seconds*8))];
            count=0;channelCount=0;target=0;recording=true;}
    }
    private void OnAudioFilterRead(float[] data,int channels){
        lock(gate){
            if(!recording||channels<=0)return;
            if(channelCount==0){channelCount=channels;target=Math.Min(captured.Length,(int)(rate*duration)*channels);}
            if(channelCount!=channels){recording=false;return;}
            int take=Math.Min(data.Length,target-count);Array.Copy(data,0,captured,count,take);count+=take;
            if(count>=target)recording=false;
        }
    }
    public Result End(string path){
        float[] data;int length,sampleRate,channels;
        lock(gate){recording=false;data=captured;length=count;sampleRate=rate;channels=channelCount;}
        var result=new Result{sampleRate=sampleRate,channels=channels,samples=length,file=Path.GetFileName(path),
            seconds=channels>0?(double)length/(sampleRate*channels):0};
        double squares=0;
        for(int i=0;i<length;++i){double value=data[i];result.peak=Math.Max(result.peak,Math.Abs(value));squares+=value*value;}
        result.rms=length>0?Math.Sqrt(squares/length):0;
        if(channels>0){
            using(var writer=new BinaryWriter(File.Create(path))){
                writer.Write(Encoding.ASCII.GetBytes("RIFF"));writer.Write(36+length*2);writer.Write(Encoding.ASCII.GetBytes("WAVEfmt "));
                writer.Write(16);writer.Write((ushort)1);writer.Write((ushort)channels);writer.Write(sampleRate);
                writer.Write(sampleRate*channels*2);writer.Write((ushort)(channels*2));writer.Write((ushort)16);
                writer.Write(Encoding.ASCII.GetBytes("data"));writer.Write(length*2);
                for(int i=0;i<length;++i)writer.Write((short)Math.Round(Math.Max(-1,Math.Min(1,data[i]))*32767));
            }
        }
        return result;
    }
}
