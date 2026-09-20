using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

// Explicit private-save integration diagnostic. No effect without its argument.
public sealed class Idas3PreRaceSmoke : MonoBehaviour
{
    [Serializable,StructLayout(LayoutKind.Sequential,Pack=8)] public struct PreRaceStatus {
        public uint size,version,phase,presentationFrame,shot,countdownRemaining;
        public int countdownDigit;
        public uint playerCar,opponentCar,opponentId,cameraView,playerRanges,rivalRanges,reserved;
        public ulong simulationTicks,ownerTicks;
        public float eyeX,eyeY,eyeZ,targetX,targetY,targetZ,verticalFov;
        public uint condition;
    }
    [StructLayout(LayoutKind.Sequential,Pack=8)] private struct RivalStatus {
        public uint size,version,preRaceActive,dialogEnemy,dialogKind,dialogPhase;
        public uint legendActive,choiceVisible,choiceKind,selectedIndex,musicCue,musicPlaying;
        public ulong musicSamplePosition;
    }
    [Serializable,StructLayout(LayoutKind.Sequential,Pack=8)] public struct RaceAudioStatus {
        public uint size,version,flags;
        public int scene,track;
        public uint reserved;
        public double streamFrame;
        public ulong idlePcmFrames,idleControlFrames,drivingControlFrames;
    }
    [Serializable] private class Observation {
        public string name,playerName,opponentName,verticesSha256,rangesSha256;
        public string row0Name,row1Name,nameOrder;
        public int localUiRow,remoteUiRow;
        public uint row0Car,row1Car;
        public PreRaceStatus source;
        public RaceAudioStatus audio;
        public uint flags,vertexCount,rangeCount,screenFadeArgb;
        public int uiDraws,visiblePixels,manualCameras;
        public bool mirrorEnabled;
        public float speed,rpm;
    }
    [Serializable] private class Report {
        public string schema="idas3-pre-race-showcase-smoke-v2",error;
        public string scope="Fresh private CHRIS profile, actual Legend menu/dialogue route, held throttle during two showcase shots and VS, original countdown then driving. AA1 captures use the unchanged native camera and all enabled Unity cameras in depth order. Numerical checks establish owner/solver/geometry/name invariants; image composition requires visual review.";
        public bool passed,shutdownComplete;
        public int checks;
        public double seconds;
        public Observation[] observations;
        public uint[] phases;
        public int[] countdownDigits;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneGetPreRaceStatus(ref PreRaceStatus status);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneCopyPreRaceName(int side,[Out] byte[] destination,int capacity);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneGetRivalStatus(ref RivalStatus status);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneGetRaceAudioStatus(ref RaceAudioStatus status);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneSetPaused(int paused);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneReturnToCourse();
    private static string pendingRoot;
    private static Idas3PreRaceSmoke active;
    private Idas3SceneGame host;
    private string root;
    private bool finished,frozen,driving;
    private int pulse,checks;
    private uint padPulse;
    private double began;
    private bool onlineObservation,onlineComplete,onlinePassed;
    private string onlineRole,onlineError;
    private uint onlinePrior;
    private int onlinePriorDigit=-1;
    private ulong onlineHeldTicks,onlineHeldOwner;
    private readonly List<Observation> observations=new List<Observation>();
    private readonly List<uint> phases=new List<uint>();
    private readonly List<int> digits=new List<int>();
    private RenderTexture target;

    public static bool Configure(ref string saves){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-pre-race-smoke");if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Pre-race diagnostic requires a NEW output directory.");
        pendingRoot=Path.GetFullPath(args[at+1]);if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a NEW pre-race diagnostic directory.");
        Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");
        File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        File.WriteAllText(Path.Combine(saves,"game-options.json"),JsonUtility.ToJson(new Idas3GameOptions.Values{defaultCamera=0}));
        string assets=Application.isEditor?Path.GetFullPath(Path.Combine(Application.dataPath,"../Native")):Path.Combine(Application.streamingAssetsPath,"IDAS3");
        SeedProfile(saves,File.ReadAllBytes(Path.Combine(assets,"data/original_frontend/name_entry.bin")));
        File.WriteAllText(Path.Combine(pendingRoot,"ISOLATED_PRE_RACE_TEST.txt"),"Private checksummed CHRIS profile. No ordinary saves loaded.\n");
        Screen.SetResolution(1280,720,FullScreenMode.Windowed);AudioListener.volume=0;return true;
    }
    private static void Word(byte[] bytes,int offset,uint value){for(int i=0;i<4;++i)bytes[offset+i]=(byte)(value>>(8*i));}
    private static void SeedProfile(string saves,byte[] nameEntry){
        string directory=Path.Combine(saves,"driver_profiles_v1");Directory.CreateDirectory(directory);
        var p=new byte[24+1228];Array.Copy(Encoding.ASCII.GetBytes("ID3PRF1\0"),p,8);Word(p,8,1);Word(p,12,0);Word(p,16,307);
        for(int i=44;i<=60;i+=4)Word(p,24+i,220);
        for(int i=0;i<5;++i){int glyph=-1;byte letter=(byte)("CHRIS"[i]+128);for(int n=0;n<221;++n)if(nameEntry[20+2*n]==0xa3&&nameEntry[21+2*n]==letter){glyph=n;break;}
            if(glyph<0)throw new InvalidDataException("Source name-entry glyph missing.");Word(p,24+44+i*4,(uint)glyph);}
        Word(p,24+76,5);Word(p,24+472,1);Word(p,24+476,1);for(int i=1;i<=35;++i)p[24+1044+i]=(byte)i;
        Word(p,24+1140,51);Word(p,24+1164,1);Word(p,24+1176,1279);Word(p,24+1180,129);p[24+1187]=4;Word(p,24+1220,1);Word(p,24+1224,4);
        uint hash=2166136261;unchecked{for(int i=0;i<p.Length;++i)if(i<20||i>=24){hash^=p[i];hash*=16777619;}}Word(p,20,hash);
        File.WriteAllBytes(Path.Combine(directory,"car_00.profile"),p);
        var setup=new byte[32];Array.Copy(Encoding.ASCII.GetBytes("ID3SET1\0"),setup,8);Word(setup,8,1);Word(setup,16,1);hash=2166136261;
        unchecked{for(int i=0;i<28;++i){hash^=setup[i];hash*=16777619;}}Word(setup,28,hash);File.WriteAllBytes(Path.Combine(directory,"car_00.setup"),setup);
    }
    public static void Attach(Idas3SceneGame game){if(pendingRoot==null)return;active=game.gameObject.AddComponent<Idas3PreRaceSmoke>();active.host=game;active.root=pendingRoot;active.began=Time.realtimeSinceStartupAsDouble;active.StartCoroutine(active.Guard(active.Run()));}
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame){
        if(active==null)return true;if(active.finished||active.frozen)return false;
        frame=new Idas3Native.FrameInput{size=(uint)Marshal.SizeOf<Idas3Native.FrameInput>(),flags=1,deltaSeconds=1.0/60};
        if(active.driving)frame.SetKey(87);if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}
        if(active.padPulse!=0){frame.padConnected=1;frame.padButtons=active.padPulse;active.padPulse=0;}return true;
    }
    private void Check(bool ok,string text){++checks;if(!ok)throw new InvalidOperationException(text+" [flags="+host.Status.flags+", ticks="+host.Status.simulationTicks+"]");}
    private PreRaceStatus Read(){var s=new PreRaceStatus{size=(uint)Marshal.SizeOf<PreRaceStatus>()};Check(s.size==104&&Idas3SceneGetPreRaceStatus(ref s)==1&&s.version==1,"Pre-race status ABI");return s;}
    private bool Dialogue(){var s=new RivalStatus{size=(uint)Marshal.SizeOf<RivalStatus>()};Check(Idas3SceneGetRivalStatus(ref s)==1,"Rival status getter");return s.preRaceActive!=0;}
    private RaceAudioStatus Audio(){var s=new RaceAudioStatus{size=(uint)Marshal.SizeOf<RaceAudioStatus>()};Check(s.size==56&&Idas3SceneGetRaceAudioStatus(ref s)==1&&s.version==1,"Race audio status ABI");return s;}
    private void CheckVisiblePresentation(PreRaceStatus s){
        Check(s.reserved==(s.phase-1)*120+s.presentationFrame,"VS animation must advance continuously from the first car shot");
        var audio=Audio();Check((audio.flags&16)==0,"Visible showcase cars still have hidden-vehicle audio mute");
        Check((audio.flags&3)==3&&audio.streamFrame==0,"Visible showcase must have source idle while the race song stays held");
    }
    private string Name(int side){var bytes=new byte[512];int length=Idas3SceneCopyPreRaceName(side,bytes,bytes.Length);Check(length>=0&&length<bytes.Length&&bytes[length]==0,"Live name copy");return Encoding.UTF8.GetString(bytes,0,length);}
    private IEnumerator Frames(int n){for(int i=0;i<n;++i)yield return null;}
    private IEnumerator Key(int key){pulse=key;yield return null;yield return null;}
    private IEnumerator Until(Func<bool> predicate,int frames,string error){for(int i=0;i<frames&&!predicate();++i)yield return null;Check(predicate(),error);}
    private IEnumerator Guard(IEnumerator routine){var stack=new Stack<IEnumerator>();stack.Push(routine);while(stack.Count>0&&!finished){object value=null;Exception failure=null;try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}if(failure!=null){Finish(false,failure.ToString());yield break;}if(value is IEnumerator child)stack.Push(child);else yield return value;}}
    private void Update(){if(onlineObservation||finished)return;if(host.Failure!=null)Finish(false,host.Failure);else if(Time.realtimeSinceStartupAsDouble-began>180)Finish(false,"Pre-race diagnostic timeout.");}
    public static Idas3PreRaceSmoke ObserveOnline(Idas3SceneGame game,string directory,string role){
        var observer=game.gameObject.AddComponent<Idas3PreRaceSmoke>();observer.host=game;observer.root=directory;observer.onlineRole=role;observer.onlineObservation=true;observer.began=Time.realtimeSinceStartupAsDouble;return observer;
    }
    public bool OnlineCheckPassed=>onlineComplete&&onlinePassed;
    public string OnlineCheckError=>onlineError??(onlineComplete?null:"Online presentation observation did not reach GO.");
    private void LateUpdate(){
        if(!onlineObservation||onlineComplete)return;
        try{
            if(host.Failure!=null)throw new InvalidOperationException(host.Failure);
            var s=Read();if(s.phase==0&&onlinePrior==0)return;
            if(onlinePrior==0){Check(s.phase==1,"Online showcase did not begin with shot0");onlineHeldTicks=s.simulationTicks;onlineHeldOwner=s.ownerTicks;}
            if(s.phase!=onlinePrior){Check(s.phase==(onlinePrior==2?4:onlinePrior+1),"Online showcase phase order without an extra VS hold");onlinePrior=s.phase;phases.Add(s.phase);Observe("online-phase-"+s.phase,s);}
            if(s.phase<=3){
                CheckVisiblePresentation(s);
                Check(s.simulationTicks==onlineHeldTicks&&s.ownerTicks==onlineHeldOwner&&s.countdownRemaining==240,"Online source solver/countdown advanced during presentation");
                Check(s.playerRanges>0&&s.rivalRanges>0&&s.cameraView==0,"Online showcase omitted a car or changed bumper preference");
                string local=onlineRole=="host"?"SMOKE HOST":"SMOKE JOIN",remote=onlineRole=="host"?"SMOKE JOIN":"SMOKE HOST";
                int localRow=onlineRole=="host"?0:1;
                Check(Name(localRow).Normalize(NormalizationForm.FormKC)==local&&Name(1-localRow).Normalize(NormalizationForm.FormKC)==remote,"Online VS names do not match each admitted driver's fixed grid slot");
                Check(Name(0).Normalize(NormalizationForm.FormKC)=="SMOKE HOST"&&Name(1).Normalize(NormalizationForm.FormKC)=="SMOKE JOIN","Shared showcase must label host/left above guest/right on both clients");
            }else if(s.phase==4){
                Check(s.simulationTicks-onlineHeldTicks==s.ownerTicks-onlineHeldOwner,"Online countdown solver/source owner mismatch");
                if(s.countdownDigit>=1&&s.countdownDigit<=3&&s.countdownDigit!=onlinePriorDigit){
                    Check(onlinePriorDigit<0?s.countdownDigit==3:s.countdownDigit==onlinePriorDigit-1,"Online source countdown digit order");onlinePriorDigit=s.countdownDigit;digits.Add(onlinePriorDigit);
                }
            }else if(s.phase==5){
                ulong elapsed=s.ownerTicks-onlineHeldOwner;
                // A real rendered frame can cover several 60Hz source steps;
                // the native single-step fixture verifies exact step180.
                Check(elapsed>=180&&elapsed<=195&&s.countdownRemaining==240-elapsed,"Online GO sampled outside its original source countdown boundary");
                Check(digits.Count==3&&digits[0]==3&&digits[1]==2&&digits[2]==1,"Online original3/2/1 missing");
                Check(s.cameraView==0&&s.playerRanges==0,"Online GO did not restore bumper view");onlineComplete=onlinePassed=true;WriteOnlineReport();
            }
        }catch(Exception error){onlineComplete=true;onlinePassed=false;onlineError=error.ToString();WriteOnlineReport();}
    }
    private void WriteOnlineReport(){var report=new Report{passed=onlinePassed,error=onlineError,checks=checks,seconds=Time.realtimeSinceStartupAsDouble-began,observations=observations.ToArray(),phases=phases.ToArray(),countdownDigits=digits.ToArray(),
        scope="Passive observation of actual two-player Unity LAN start. No input override, pause, captures, target changes, or clock freezes. Shared showcase rows are fixed grid order: row0 upper-left is the host car, row1 lower-right is the guest car on both clients. Player/opponent fields retain local/remote identity and explicit row annotations identify placement. Both showcase cars are submitted with bumper saved; source solver held through shot0/shot1/VS;3/2/1 and GO observed. Real frame sampling may cover several source ticks; exact step180 is independently verified by the native single-step fixture."};File.WriteAllText(Path.Combine(root,"pre-race-report.json"),JsonUtility.ToJson(report,true));}
    private IEnumerator Run(){
        yield return Frames(5);Check(host.Ready,"Native initialization");host.DiagnosticFocusOverride=true;
        target=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32){name="Pre-race verification",antiAliasing=1};Check(target.Create(),"Capture target creation");host.GetComponent<Camera>().targetTexture=target;
        // Quick-start is setup only; use the same public return command as the
        // pause menu, then every battle choice uses its ordinary source owner.
        yield return Key(116);yield return Until(()=>Read().phase==5,1800,"Quick-start did not finish its presentation/countdown");
        Check(Idas3SceneSetPaused(1)==1&&Idas3SceneReturnToCourse()==1,"Return to source course menu");yield return Frames(30);
        Check(host.Status.frontendStage==6&&(host.Status.flags&1)!=0,"Course6 setup");
        for(int i=0;i<3;++i){yield return Key(37);yield return Frames(6);}Check(host.Status.course==0,"Myogi selection");
        yield return Key(27);yield return Frames(100);Check(host.Status.frontendStage==5,"Mode5 setup");
        yield return Key(13);yield return Frames(180);Check(host.Status.frontendStage==6,"Legend course selection");
        yield return Key(13);yield return Frames(45);Check(host.Status.frontendStage==10,"Rival10 selection");yield return Capture("rival-selection");
        yield return Key(13);yield return Until(Dialogue,600,"Source rival dialogue did not begin");
        // Let the source character scene fade in before the visual capture;
        // vehicle mute is required throughout that transition as well.
        for(int i=0;i<60;++i){Check((Audio().flags&16)!=0,"Hidden character artwork must mute vehicle audio");yield return null;}
        yield return Capture("character-art-hidden-car");
        // One held Start must skip successive pages without needing repeated
        // presses, and must never open pause behind the conversation.
        int skipFrames=0;
        for(;skipFrames<1200&&Dialogue();++skipFrames){padPulse=0x10;Check(!host.PauseMenu.IsOpen,"Dialogue Start opened pause");yield return null;}
        File.WriteAllText(Path.Combine(root,"held-start-skip.txt"),"Held Start closed dialogue after "+skipFrames+" rendered frames.\n");
        Check(!Dialogue(),"Source rival dialogue did not close");driving=true;
        bool capturedLoading=false;
        for(int i=0;i<1200&&Read().phase!=1;++i){
            if((host.Status.flags&1024)!=0){Check((Audio().flags&16)!=0,"Loading artwork must mute vehicle audio");if(!capturedLoading){capturedLoading=true;yield return Capture("loading-hidden-car");}}
            yield return null;
        }
        Check(Read().phase==1,"First showcase shot did not begin");
        var first=Read();ulong frozenTicks=first.simulationTicks,frozenOwner=first.ownerTicks;float speed=host.Status.speedMetresPerSecond,rpm=host.Status.rpm;
        var cameraEyes=new List<Vector3>();var cameraTravel=new float[2];uint prior=0;int priorDigit=-1;var captured=new HashSet<string>();
        for(int i=0;i<2400;++i){
            var s=Read();Check(s.phase>=1&&s.phase<=5,"Unexpected race presentation phase");
            if(s.phase!=prior){Check(s.phase==(prior==2?4:prior+1),"Showcase/countdown phase order without an extra VS hold");prior=s.phase;phases.Add(s.phase);Observe("phase-"+s.phase,s);}
            if(s.phase<=3){
                CheckVisiblePresentation(s);
                Check(s.simulationTicks==frozenTicks&&s.ownerTicks==frozenOwner,"Source solver/countdown advanced during showcase/VS");
                Check(host.Status.speedMetresPerSecond==speed&&host.Status.rpm==rpm,"Player driving state changed during showcase/VS");
                Check(s.cameraView==0,"Showcase changed the saved bumper view");Check(s.playerRanges>0&&s.rivalRanges>0,"Showcase omitted a car with bumper view saved");
                Check(s.countdownRemaining==240,"Source countdown began before the VS screen ended");
                Check(Name(0).Normalize(NormalizationForm.FormKC)=="CHRIS","VS player name did not come from the loaded profile");
                Check(s.opponentId==0&&Name(1).Normalize(NormalizationForm.FormKC)=="IGGY","VS opponent name does not match source rival0 IGGY");
                if(s.reserved>=152&&captured.Add("versus"))yield return Capture("versus");
                string name=s.phase==1?"showcase-shot-0":s.phase==2?"showcase-shot-1":"versus";
                if((s.phase<3||s.reserved>=152)&&captured.Add(name)){if(s.phase<3)cameraEyes.Add(new Vector3(s.eyeX,s.eyeY,s.eyeZ));yield return Capture(name);}
                if(s.phase<3){int index=(int)s.phase-1;cameraTravel[index]=Math.Max(cameraTravel[index],(new Vector3(s.eyeX,s.eyeY,s.eyeZ)-cameraEyes[index]).sqrMagnitude);
                    if(s.presentationFrame>=60&&captured.Add(name+"-moving"))yield return Capture(name+"-moving");
                    if(s.phase==1&&s.presentationFrame>=90&&captured.Add("showcase-shot-0-vs-text"))yield return Capture("showcase-shot-0-vs-text");}
            }else if(s.phase==4){
                Check(s.simulationTicks-frozenTicks==s.ownerTicks-frozenOwner,"Countdown solver and source owner diverged");
                if(s.countdownDigit>=1&&s.countdownDigit<=3&&s.countdownDigit!=priorDigit){
                    Check(priorDigit<0?s.countdownDigit==3:s.countdownDigit==priorDigit-1,"Original countdown digits out of order");
                    priorDigit=s.countdownDigit;digits.Add(priorDigit);yield return Capture("countdown-"+priorDigit);
                }
            }else{
                Check(s.ownerTicks-frozenOwner==180&&s.simulationTicks-frozenTicks==180,"GO must follow exactly 180 original countdown steps");
                Check(digits.Count==3&&digits[0]==3&&digits[1]==2&&digits[2]==1,"Missing source countdown digits");
                Check(captured.Contains("versus"),"VS animation did not reach its readable settled frame152");
                Check(captured.Contains("showcase-shot-0-vs-text"),"Missing first-shot VS text capture");
                Check(s.cameraView==0&&s.playerRanges==0,"Countdown did not restore saved bumper camera");
                Check(cameraEyes.Count==2&&(cameraEyes[0]-cameraEyes[1]).sqrMagnitude>.0001f,"Two showcase shots use the same camera");
                Check(cameraTravel[0]>.0001f&&cameraTravel[1]>.0001f,"Source camera does not move within both showcase shots");
                yield return Capture("go-release-180");
                // The source enables driving at remaining60 while its digit
                // still reads1; the next source update selects GO (digit0).
                yield return Frames(1);var visibleGo=Read();
                Check(visibleGo.phase==5&&visibleGo.countdownDigit==0&&visibleGo.countdownRemaining==59&&visibleGo.ownerTicks-frozenOwner==181,"Visible GO did not follow the exact source release boundary");
                yield return Capture("go");break;
            }
            yield return null;
        }
        Check(phases.Count==4&&phases[3]==5,"Presentation never released driving");yield return Frames(180);
        Check(host.Status.speedMetresPerSecond>1,"Player did not accelerate after GO");yield return Capture("driving-bumper");
        yield return Key(67);yield return Frames(10);Check(Read().playerRanges>0,"Chase camera omitted the player car");yield return Capture("driving-chase");Finish(true,null);
    }
    private static string Hash(IntPtr pointer,int bytes){if(bytes==0)return "";var data=new byte[bytes];Marshal.Copy(pointer,data,0,bytes);using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(data)).Replace("-","").ToLowerInvariant();}
    private Observation Observe(string name,PreRaceStatus s){var source=host.GetComponent<Idas3SceneRenderer>().CurrentFrame;int localRow=onlineObservation&&onlineRole=="join"?1:0;
        var o=new Observation{name=name,source=s,audio=Audio(),playerName=Name(localRow),opponentName=Name(1-localRow),row0Name=Name(0),row1Name=Name(1),localUiRow=localRow,remoteUiRow=1-localRow,
        row0Car=localRow==0?s.playerCar:s.opponentCar,row1Car=localRow==0?s.opponentCar:s.playerCar,nameOrder=onlineObservation?"row0=host/upper-left; row1=guest/lower-right":"row0=player; row1=opponent",flags=host.Status.flags,speed=host.Status.speedMetresPerSecond,rpm=host.Status.rpm,
        vertexCount=source.vertexCount,rangeCount=source.rangeCount,screenFadeArgb=source.screenFadeArgb,mirrorEnabled=host.GetComponent<Idas3SceneRenderer>().MirrorCamera.enabled,
        uiDraws=host.GetComponent<Idas3UnityUi>().DrawCount};observations.Add(o);return o;}
    private IEnumerator Capture(string name){
        frozen=true;yield return null;var o=Observe(name,Read());var scene=host.GetComponent<Idas3SceneRenderer>().CurrentFrame;
        o.verticesSha256=Hash(scene.vertices,checked((int)scene.vertexCount*64));o.rangesSha256=Hash(scene.ranges,checked((int)scene.rangeCount*64));
        Check(host.GetComponent<Idas3UnityUi>().UnresolvedSurfaces==0,"Unresolved source UI surface: "+name);
        var cameras=new List<Camera>();foreach(var camera in Resources.FindObjectsOfTypeAll<Camera>())if(camera!=null&&camera.enabled&&camera.gameObject.activeInHierarchy&&camera.targetTexture==target)cameras.Add(camera);
        cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));var previous=RenderTexture.active;RenderTexture.active=target;GL.Clear(true,true,Color.black);RenderTexture.active=previous;
        foreach(var camera in cameras)camera.Render();o.manualCameras=cameras.Count;Check(cameras.Count>=1,"No capture camera: "+name);
        RenderTexture.active=target;var texture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);texture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);texture.Apply();RenderTexture.active=previous;
        foreach(var pixel in texture.GetPixels32())if(Math.Max(pixel.r,Math.Max(pixel.g,pixel.b))>24)++o.visiblePixels;
        File.WriteAllBytes(Path.Combine(root,name+".png"),texture.EncodeToPNG());Destroy(texture);File.WriteAllText(Path.Combine(root,name+".json"),JsonUtility.ToJson(o,true));
        Check(o.visiblePixels>target.width*target.height/1000,"Black capture: "+name);frozen=false;
    }
    private void Finish(bool passed,string error){if(finished)return;finished=true;frozen=true;
        bool stopped=false;try{host.DiagnosticFocusOverride=null;host.GetComponent<Camera>().targetTexture=null;if(target!=null){target.Release();Destroy(target);target=null;}host.StopNative();stopped=!host.Ready&&Idas3Native.ReadStatus().state==0;}catch(Exception e){passed=false;error=(error??"")+"\nShutdown: "+e;}
        if(!stopped){passed=false;error=(error??"")+"\nNative shutdown incomplete.";}
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(new Report{passed=passed,error=error,shutdownComplete=stopped,checks=checks,seconds=Time.realtimeSinceStartupAsDouble-began,observations=observations.ToArray(),phases=phases.ToArray(),countdownDigits=digits.ToArray()},true));
        Debug.Log((passed?"PASS":"FAIL")+" pre-race showcase: "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(passed?0:1);
#endif
    }
}
