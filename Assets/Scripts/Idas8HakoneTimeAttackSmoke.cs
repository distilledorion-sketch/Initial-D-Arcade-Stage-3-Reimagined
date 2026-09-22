using System;
using System.IO;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine;

// Explicit isolated integration test; never uses the player's test-build save.
public sealed class Idas8HakoneTimeAttackSmoke : MonoBehaviour {
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneHakoneTimeAttackTest([MarshalAs(UnmanagedType.LPUTF8Str)] string pack);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneModeFlowValue(int field);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneGetPreRaceStatus(ref Idas3PreRaceSmoke.PreRaceStatus status);
    static bool IntroCameraCheck => Array.IndexOf(Environment.GetCommandLineArgs(),"-imported-intro-camera-check")>=0;
    static string output,packName="HAKONE";static Idas8HakoneTimeAttackSmoke active;
    Idas3SceneGame host;int pulse;bool accelerate;
    public static bool Configure(ref string saves){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-enna-ta-smoke");if(at>=0)packName="ENNA";else{at=Array.IndexOf(args,"-sadamine-ta-smoke");if(at>=0)packName="SADAMINE";else at=Array.IndexOf(args,"-hakone-ta-smoke");}if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Supply a new Hakone TA diagnostic directory");
        output=Path.GetFullPath(args[at+1]);if(Directory.Exists(output))throw new IOException("Diagnostic directory must be new");
        Directory.CreateDirectory(output);saves=Path.Combine(output,"userdata");Directory.CreateDirectory(saves);
        if(Array.IndexOf(args,"-idas3-palette-recovery-check")>=0)File.WriteAllText(Path.Combine(output,"PALETTE_RECOVERY_TEST.txt"),"Invalid paint regression in isolated profiles only");
        File.WriteAllText(Path.Combine(output,"HAKONE_TA_TEST.txt"),"Private Hakone Time Attack test saves");return true;
    }
    public static void Attach(Idas3SceneGame host){if(output==null)return;active=host.gameObject.AddComponent<Idas8HakoneTimeAttackSmoke>();active.host=host;host.DiagnosticFocusOverride=true;active.StartCoroutine(active.Guard(active.Run()));}
    internal static void PrepareFrame(ref Idas3Native.FrameInput frame){if(active==null)return;frame=new Idas3Native.FrameInput{size=(uint)Marshal.SizeOf<Idas3Native.FrameInput>(),flags=1,deltaSeconds=1.0/60};if(active.accelerate)frame.SetKey(87);if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}}
    IEnumerator Guard(IEnumerator run){
        var stack=new System.Collections.Generic.Stack<IEnumerator>();stack.Push(run);
        while(stack.Count>0){object current;try{var next=stack.Peek();if(!next.MoveNext()){stack.Pop();continue;}current=next.Current;if(current is IEnumerator child){stack.Push(child);continue;}}catch(Exception e){File.WriteAllText(Path.Combine(output,"failure.txt"),e.ToString());Debug.LogException(e);Application.Quit(1);yield break;}yield return current;}
    }
    IEnumerator Frames(int count){for(int i=0;i<count;i++){if(host.Failure!=null)throw new Exception(host.Failure);yield return null;}}
    IEnumerator Until(Func<bool> condition,int limit,string name){for(int i=0;i<limit&&!condition();i++)yield return null;if(!condition())throw new Exception(name+": "+host.Failure);}
    IEnumerator Key(int key){pulse=key;yield return null;yield return null;}
    IEnumerator Capture(string name,bool sceneryOnly=false){yield return new WaitForEndOfFrame();var camera=host.GetComponent<Camera>();var rt=new RenderTexture(1280,720,24){antiAliasing=Mathf.Max(1,QualitySettings.antiAliasing)};var old=RenderTexture.active;var mask=camera.cullingMask;var tex=new Texture2D(1280,720,TextureFormat.RGB24,false);try{camera.targetTexture=rt;if(sceneryOnly)camera.cullingMask=1<<28;camera.Render();if(!sceneryOnly)host.GetComponent<Idas3UnityUi>().RenderOverlayForCapture();RenderTexture.active=rt;tex.ReadPixels(new Rect(0,0,1280,720),0,0);tex.Apply();File.WriteAllBytes(Path.Combine(output,name+".png"),tex.EncodeToPNG());
        if(name.StartsWith("intro-camera-")){
            Color32 clear=camera.backgroundColor;var pixels=tex.GetPixels32();int uncovered=0,total=0;
            // Central roadway immediately above the bottom 55/480 letterbox.
            for(int y=88;y<115;++y)for(int x=256;x<1024;++x){var c=pixels[y*1280+x];++total;
                if(Math.Abs(c.r-clear.r)<=2&&Math.Abs(c.g-clear.g)<=2&&Math.Abs(c.b-clear.b)<=2)++uncovered;}
            var status=IntroStatus();var nativeCamera=host.GetComponent<Idas3SceneRenderer>().CurrentFrame.mainCamera;
            File.AppendAllText(Path.Combine(output,"intro-camera.csv"),$"{name},{status.phase},{status.presentationFrame},{nativeCamera.nearClip},{uncovered},{total},{nativeCamera.eye.x},{nativeCamera.eye.y},{nativeCamera.eye.z}\n");
            if(uncovered>total/100)throw new Exception("Near-camera road missing: "+name+" clear pixels="+uncovered);
        }
        }finally{camera.cullingMask=mask;camera.targetTexture=null;RenderTexture.active=old;rt.Release();Destroy(rt);Destroy(tex);}}
    IEnumerator CaptureScreen(string name){
        yield return new WaitForEndOfFrame();
        var tex=ScreenCapture.CaptureScreenshotAsTexture();
        File.WriteAllBytes(Path.Combine(output,name+".png"),tex.EncodeToPNG());Destroy(tex);
    }
    Idas3PreRaceSmoke.PreRaceStatus IntroStatus(){
        var s=new Idas3PreRaceSmoke.PreRaceStatus{size=(uint)Marshal.SizeOf<Idas3PreRaceSmoke.PreRaceStatus>()};
        if(Idas3SceneGetPreRaceStatus(ref s)!=1)throw new Exception(Idas3Native.Error());return s;
    }
    IEnumerator CheckIntroCamera(string label){
        bool first=false,second=false;int next=0;
        for(int tick=0;tick<300;++tick){
            var status=IntroStatus();
            if(status.phase==4){
                if(!first||!second)throw new Exception("Both showcase shots not observed");
                if(host.GetComponent<Idas3SceneRenderer>().CurrentFrame.mainCamera.nearClip!=1f)throw new Exception("Intro clipping distance leaked into race");
                yield break;
            }
            if(status.phase!=1&&status.phase!=2)throw new Exception("Missing imported showcase");
            if(host.GetComponent<Idas3SceneRenderer>().CurrentFrame.mainCamera.nearClip!=.1f)throw new Exception("Imported showcase clipping distance");
            if(tick>=next){
                first|=status.phase==1;second|=status.phase==2;
                yield return Capture("intro-camera-"+label+"-"+tick);next=tick+40;
            }
            yield return null;
        }
        throw new Exception("Imported showcase never reached countdown");
    }
    IEnumerator SelectImportedRace(){
        int last=packName=="ENNA"?8:9;
        for(int stage=6;stage<=last;++stage){
            if(host.Status.frontendStage!=stage)throw new Exception("Wrong imported menu stage "+stage);
            yield return Key(13);
            if(stage<last)yield return Until(()=>host.Status.frontendStage!=stage,120,"Imported selection transition");
            else yield return Until(()=>(host.Status.flags&16385u)==16384u,600,"Imported menu launches race");
            yield return Frames(3);
        }
        yield return Until(()=>(host.Status.flags&1024u)==0,600,"Imported loading complete");
    }
    IEnumerator Run(){
        bool performance=Array.IndexOf(Environment.GetCommandLineArgs(),"-imported-scenery-performance-check")>=0;
        bool trees=Array.IndexOf(Environment.GetCommandLineArgs(),"-imported-trees-check")>=0;
        if(trees)Idas8ImportedTreeChecks.Run();
        yield return Frames(3);
        if(packName!="ENNA"&&!FindAnyObjectByType<Idas8HakoneCourse>().testBuild){
            if(host.Status.frontendStage!=0||(host.Status.flags&16385u)!=1u)throw new Exception("Main build must retain normal title startup");
            yield return Capture("main-build-title");
        }
        if(Idas3SceneHakoneTimeAttackTest(Path.Combine(Application.streamingAssetsPath,packName))!=1)throw new Exception(Idas3Native.Error());
        yield return Frames(30);
        if(Idas3SceneModeFlowValue(1)!=1)throw new Exception("Missing Hakone coaching");
        yield return Capture("hakone-analysis-full");
        for(int i=1;i<=4;i++){yield return Key(67);yield return Frames(3);yield return Capture("hakone-analysis-section-"+i);}
        yield return Key(13);
        yield return Until(()=>Idas3SceneModeFlowValue(0)==0&&(host.Status.flags&262144u)!=0,150,"Coaching to points");
        yield return Frames(100);yield return Capture("hakone-points");
        for(int i=0;i<1500&&Idas3SceneModeFlowValue(0)==0;i++){if(i%20==0)pulse=13;yield return null;}
        if(Idas3SceneModeFlowValue(1)!=2)throw new Exception("Missing Hakone ranking");
        yield return Frames(50);yield return Capture("hakone-ranking");yield return Key(13);
        yield return Until(()=>Idas3SceneModeFlowValue(1)==3,120,"Ranking to Continue");yield return Frames(10);yield return Capture("hakone-continue");
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-presentation-check")>=0){
            Screen.SetResolution(2552,1077,FullScreenMode.Windowed);yield return Frames(12);
            yield return CaptureScreen("continue-ultrawide");
            Screen.SetResolution(1280,720,FullScreenMode.Windowed);yield return Frames(12);
        }
        yield return Key(13);
        yield return Until(()=>(host.Status.flags&1u)!=0,150,"Continue returns to course menu");yield return Frames(20);yield return Capture("hakone-saved-records");
        int lastStage=packName=="ENNA"?8:9;
        for(int stage=6;stage<=lastStage;stage++){
            if(host.Status.frontendStage!=stage)throw new Exception("Unexpected retry menu stage");
            if(packName=="ENNA"&&stage==8){
                yield return Capture("enna-weather-dry");
                yield return Key(39);yield return Frames(3);yield return Capture("enna-weather-wet");
            }
            yield return Key(13);
            if(stage<lastStage)yield return Until(()=>host.Status.frontendStage!=stage,120,"Retry selection transition");
            else yield return Until(()=>(host.Status.flags&16385u)==16384u,600,"Menu relaunches Hakone");
            yield return Frames(3);
        }
        // Artwork now lasts 3 seconds, fades for .5, then holds black for 2.
        // Keep a bounded wait longer than that authored 330-frame sequence.
        yield return Until(()=>(host.Status.flags&1024u)==0,600,"Loading completes");yield return Frames(20);yield return Capture("hakone-race-intro");
        if(IntroCameraCheck)yield return CheckIntroCamera("retry");
        accelerate=true;for(int i=0;i<1800&&host.Status.speedMetresPerSecond<=2;i++)yield return null;yield return Capture("hakone-live-record-hud");if(host.Status.speedMetresPerSecond<=2)throw new Exception("Relaunch state: "+JsonUtility.ToJson(host.Status));
        if(packName=="ENNA"){
            if((host.Status.flags&131072u)==0)throw new Exception("Wet menu choice did not launch wet race");
            yield return Frames(120);yield return Capture("enna-driving-headlights");
            var course=FindAnyObjectByType<IdasSpecialStageEnnaCourse>();course.VerifyPresentation();
        }
        if(trees){
            for(int i=0;i<6;i++){yield return Frames(20);FindAnyObjectByType<Idas8HakoneCourse>().VerifyTreeState();yield return Capture("trees-moving-"+i);}
            int aa=QualitySettings.antiAliasing;QualitySettings.antiAliasing=0;yield return Frames(3);FindAnyObjectByType<Idas8HakoneCourse>().VerifyTreeState();yield return Capture("trees-aa-off");
            QualitySettings.antiAliasing=aa;yield return Frames(3);
        }
        accelerate=false;yield return Capture("menu-background",true);
        if(performance)FindAnyObjectByType<Idas8HakoneCourse>().CheckSceneryPerformance(output,"race",true);
        for(int condition=0;condition<8;condition++){
            if(packName=="ENNA"&&condition<4)continue;
            if(Idas3Native.Idas3SceneStartImportedCourseConditions(Path.Combine(Application.streamingAssetsPath,packName),condition&1,(condition>>2)&1,(condition>>1)&1)!=1)throw new Exception(Idas3Native.Error());
            yield return Frames(35);
            var scenery=FindAnyObjectByType<Idas8HakoneCourse>();
            if(packName=="ENNA"){
                if(!FindAnyObjectByType<IdasSpecialStageEnnaCourse>().Loaded||(host.Status.flags&IdasSpecialStageEnnaCourse.SceneFlag)==0)throw new Exception("Enna scenery not loaded");
                FindAnyObjectByType<IdasSpecialStageEnnaCourse>().VerifyPresentation();
                if(((host.Status.flags&131072u)!=0)!=((condition&2)!=0))throw new Exception("Enna weather selection lost");
            }else if(scenery.LoadedCourse!=packName||scenery.LoadedVariant!=Idas8HakoneCourse.Variant(host.Status.flags))throw new Exception("Wrong imported scenery/conditions");
            if(trees)scenery.VerifyTreeState();
            if(performance)scenery.CheckSceneryPerformance(output,"condition-"+condition,false);
            yield return Capture("condition-"+condition);
            if(IntroCameraCheck){
                if(Idas3Native.Idas3SceneShowImportedCourseMenu(Path.Combine(Application.streamingAssetsPath,packName))!=1)throw new Exception(Idas3Native.Error());
                yield return Frames(25);yield return SelectImportedRace();
                yield return CheckIntroCamera("condition-"+condition);
            }
        }
        File.WriteAllText(Path.Combine(output,"version.txt"),Application.version);
        File.WriteAllText(Path.Combine(output,"PASS.txt"),"Native finish/timeout/points/tuning/persistence tests and Unity analysis, five maps, results, ranking, Continue and saved menu record captures passed.");Debug.Log("HAKONE TIME ATTACK PASS");Application.Quit(0);
    }
}
