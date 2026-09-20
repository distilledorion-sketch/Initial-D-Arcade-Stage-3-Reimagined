using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;
using Idas3.Multiplayer;

// Read-only observations of the live two-client race. This fixture does not
// inject positions, progress, HUD text, section times or a simulated opponent.
public sealed class Idas3MultiplayerHudSmoke
{
    [Serializable,StructLayout(LayoutKind.Sequential,Pack=8)] private struct HudStatus
    {
        public uint size,version,active,cameraView,localCar,remoteCar,sourceProfileMode,game2dCommands,portraitCommands,
            playerGlyphs,rivalGlyphs,mirrorEnabled,playerMapMarkers,rivalMapMarkers,sectionCount,sectionCapacity,elapsed6000;
        public float signedAdvantage,localProgressMetres,remoteProgressMetres;
        [MarshalAs(UnmanagedType.ByValArray,SizeConst=4)] public uint[] cumulativeSections;
        [MarshalAs(UnmanagedType.ByValArray,SizeConst=4)] public uint[] renderedSectionDurations;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3MultiplayerGetHudStatus(ref HudStatus status);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3MultiplayerCopyHudText(int side,StringBuilder destination,int capacity);
    [Serializable] private sealed class Capture
    {
        public string file,kind="AA1-actual-window",driverText,opponentText;
        public uint cameraView,viewCount;
        public int width,height,visiblePixels,mirrorVisiblePixels,mirrorDistinctColors,mirrorRanges,mainRanges,uiDraws,
            localCarMainRanges,localCarMirrorRanges,remoteCarMirrorRanges;
        public uint unresolvedSurfaces;
        public ulong frameGeneration,localRaceTicks,remoteRaceTicks;
        public Vector4 mirrorViewport;
        public HudStatus hud;
    }
    [Serializable] private sealed class Report
    {
        public string schema="idas3-network-race-hud-v1",applicationVersion=Application.version,role,error;
        public string scope="Two actual LAN clients and real source-handling progress. Both early driving cameras are captured from the actual window. Native diagnostics copy the last HUD paint's commands, text, markers and section durations; scene records and Unity camera state independently verify rear-view rendering. Signed gap is checked against each client's exact local/remote presentation inputs, not assumed simultaneous peer snapshots. This does not claim a completed sector or internet latency coverage.";
        public bool passed,bothCameras,localRemoteRows,closeSignedGap,sectionsVerified,mapMarkers,mirrorGeometry;
        public int checks;
        public Capture[] captures;
    }
    private static Idas3MultiplayerHudSmoke active;
    private string root,role;
    private readonly List<Capture> captures=new List<Capture>();
    private readonly Report report=new Report();
    private static bool Enabled=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-hud-check")>=0;
    private static string Arg(string[] args,string key){int i=Array.IndexOf(args,key);return i>=0&&i+1<args.Length?args[i+1]:null;}
    private static bool SamePath(string a,string b)=>string.Equals(Path.GetFullPath(a).TrimEnd(Path.DirectorySeparatorChar),Path.GetFullPath(b).TrimEnd(Path.DirectorySeparatorChar),StringComparison.OrdinalIgnoreCase);
    private static Idas3MultiplayerHudSmoke Ensure(string directory,string role)
    {
        if(!Enabled)return null;
        var args=Environment.GetCommandLineArgs();string actualRoot=Arg(args,"-idas3-multiplayer-smoke");
        if(string.IsNullOrEmpty(actualRoot)||!SamePath(directory,actualRoot)||Arg(args,"-idas3-multiplayer-role")!=role||
            (role!="host"&&role!="join")||Array.IndexOf(args,"-idas3-multiplayer-showcase-check")<0||
            Array.IndexOf(args,"-idas3-multiplayer-steam-check")>=0||Array.IndexOf(args,"-idas3-multiplayer-quick-check")>=0||
            !File.Exists(Path.Combine(directory,"ISOLATED_MULTIPLAYER_TEST.txt"))||!Directory.Exists(Path.Combine(directory,"userdata")))
            throw new InvalidOperationException("Race HUD check requires the explicit isolated two-peer showcase diagnostic.");
        if(active==null){active=new Idas3MultiplayerHudSmoke{root=Path.GetFullPath(directory),role=role};active.report.role=role;}
        if(!SamePath(active.root,directory)||active.role!=role)throw new InvalidOperationException("HUD diagnostic role/root changed.");
        return active;
    }
    private void Check(bool value,string why){++report.checks;if(!value)throw new InvalidOperationException(why+" ["+role+"]");}
    private static bool Finite(float value)=>!float.IsNaN(value)&&!float.IsInfinity(value);
    private string Text(int side)
    {
        var buffer=new StringBuilder(256);int count=Idas3MultiplayerCopyHudText(side,buffer,buffer.Capacity);
        Check(count>0&&count<buffer.Capacity,"Rendered online HUD text is missing.");return buffer.ToString().Normalize(NormalizationForm.FormKC);
    }
    private HudStatus Read()
    {
        var status=new HudStatus{size=(uint)Marshal.SizeOf<HudStatus>(),cumulativeSections=new uint[4],renderedSectionDurations=new uint[4]};
        Check(status.size==112&&Idas3MultiplayerGetHudStatus(ref status)==1&&status.version==1,"Online HUD status ABI failed.");return status;
    }
    private static IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    public static IEnumerator VerifyView(Idas3SceneGame game,Idas3MultiplayerSession session,string directory,string role,uint expectedView)
    {
        var self=Ensure(directory,role);if(self==null)yield break;
        int previousAA=QualitySettings.antiAliasing;Texture2D picture=null;
        // MoveNext exceptions are propagated by the existing multiplayer smoke
        // coroutine guard. The finalizer retains partial evidence on failure.
        bool complete=false;
        try{
            self.Check(expectedView<=1,"Invalid HUD camera request.");
            foreach(var previous in self.captures)self.Check(previous.cameraView!=expectedView,"HUD camera was checked twice.");
            QualitySettings.antiAliasing=0;yield return Frames(3);yield return new WaitForEndOfFrame();
            var hud=self.Read();var scene=game.GetComponent<Idas3SceneRenderer>();var ui=game.GetComponent<Idas3UnityUi>();var frame=scene.CurrentFrame;
            var menu=game.GetComponent<Idas3MultiplayerMenu>();
            self.Check(game.Ready&&session.IsRacing&&session.RaceReleased&&game.Status.racePhase==2&&!menu.IsOpen,"HUD capture is not an unobstructed running race.");
            self.Check(hud.active==1&&hud.cameraView==expectedView&&hud.sourceProfileMode==3,"Race did not use the compact online battle HUD.");
            self.Check(hud.localCar==(uint)session.LocalCar&&hud.remoteCar==(uint)session.RemoteCar,"HUD car identifiers are not local/opponent.");
            string driver=self.Text(0),opponent=self.Text(1);
            string hostText="SMOKE HOST [AE86 TRUENO]",guestText="SMOKE JOIN [BNR34]";
            self.Check(driver==(role=="host"?hostText:guestText)&&opponent==(role=="host"?guestText:hostText),"DRIVER/OPPONENT names or original car codes are swapped or missing.");
            self.Check(hud.game2dCommands>8&&hud.portraitCommands==0&&hud.playerGlyphs>0&&hud.rivalGlyphs>0,"Actual HUD draw omitted labels/names or submitted an offline portrait.");
            self.Check(Finite(hud.signedAdvantage)&&Finite(hud.localProgressMetres)&&Finite(hud.remoteProgressMetres),"Gap inputs are not finite.");
            float difference=hud.localProgressMetres-hud.remoteProgressMetres;
            self.Check(Mathf.Abs(hud.signedAdvantage-difference)<=.001f,"Rendered advantage does not preserve signed local-minus-opponent distance.");
            self.Check(Mathf.Abs(hud.signedAdvantage)<100f,"Early race HUD check missed the close-gap case.");
            self.Check(hud.sectionCapacity>=1&&hud.sectionCapacity<=4&&hud.sectionCount<hud.sectionCapacity&&hud.elapsed6000>0,"Running HUD timing metadata is invalid.");
            uint previousTime=0;
            for(uint row=0;row<hud.sectionCapacity;++row){
                uint expected=uint.MaxValue;
                if(row<=hud.sectionCount){uint cumulative=row<hud.sectionCount?hud.cumulativeSections[row]:hud.elapsed6000;
                    self.Check(cumulative>=previousTime,"HUD cumulative section records went backwards.");expected=cumulative-previousTime;previousTime=cumulative;}
                self.Check(hud.renderedSectionDurations[row]==expected,"HUD section row used cumulative time or filled a future row.");
            }
            self.Check(hud.playerMapMarkers==1&&hud.rivalMapMarkers==1,"Minimap did not submit both source player/opponent markers.");
            self.Check(hud.mirrorEnabled==1&&frame.viewCount==2&&scene.MirrorCamera!=null&&scene.MirrorCamera.enabled,"Rear-view mirror is absent in this driving camera.");
            var viewport=frame.mirrorCamera.viewport;
            self.Check(viewport.x>=0&&viewport.y>=0&&viewport.z>0&&viewport.w>0&&viewport.x+viewport.z<=frame.width+1&&viewport.y+viewport.w<=frame.height+1,"Mirror aperture is invalid or outside the rendered resolution.");
            self.Check(Mathf.Abs(viewport.z/viewport.w-5f)<.05f,"Original rear-view aperture lost its 5:1 proportions.");
            int mirrorRanges=0,mainRanges=0,localMain=0,localMirror=0,remoteMirror=0;
            for(int i=0;i<frame.rangeCount;++i){int offset=checked(i*64);uint count=unchecked((uint)Marshal.ReadInt32(frame.ranges,offset+4));
                uint flags=unchecked((uint)Marshal.ReadInt32(frame.ranges,offset+28)),scope=unchecked((uint)Marshal.ReadInt32(frame.ranges,offset+36));
                uint mask=unchecked((uint)Marshal.ReadInt32(frame.ranges,offset+40));if(count==0)continue;if((mask&1)!=0)++mainRanges;if((mask&2)!=0)++mirrorRanges;
                if((flags&32)!=0&&scope==2){if((mask&1)!=0)++localMain;if((mask&2)!=0)++localMirror;}
                if((flags&32)!=0&&scope==3&&(mask&2)!=0)++remoteMirror;}
            self.Check(mainRanges>0&&mirrorRanges>0,"Rear-view camera has no submitted world geometry.");
            self.Check(localMirror==0&&remoteMirror>0,"Mirror includes the local body/plate or excludes the remote car.");
            if(expectedView==1)self.Check(localMain>0,"Chase camera omitted the local car from the main view.");
            self.Check(ui.DrawCount>0&&ui.UnresolvedSurfaces==0,"Race UI submissions are missing or unresolved.");
            picture=ScreenCapture.CaptureScreenshotAsTexture();self.Check(picture!=null&&picture.width==frame.width&&picture.height==frame.height,"Actual HUD capture dimensions differ from native rendering.");
            var pixels=picture.GetPixels32();int visible=0,mirrorVisible=0;var mirrorColors=new HashSet<int>();
            foreach(var pixel in pixels)if(Math.Max(pixel.r,Math.Max(pixel.g,pixel.b))>24)++visible;
            int left=Mathf.Clamp(Mathf.CeilToInt(viewport.x+2),0,picture.width-1),right=Mathf.Clamp(Mathf.FloorToInt(viewport.x+viewport.z-2),0,picture.width);
            int top=Mathf.Clamp(Mathf.CeilToInt(viewport.y+2),0,picture.height-1),bottom=Mathf.Clamp(Mathf.FloorToInt(viewport.y+viewport.w-2),0,picture.height);
            for(int y=top;y<bottom;y+=2)for(int x=left;x<right;x+=2){var pixel=pixels[(picture.height-1-y)*picture.width+x];
                if(Math.Max(pixel.r,Math.Max(pixel.g,pixel.b))>24)++mirrorVisible;mirrorColors.Add((pixel.r>>3)<<10|(pixel.g>>3)<<5|(pixel.b>>3));}
            self.Check(visible>picture.width*picture.height/100&&mirrorVisible>10&&mirrorColors.Count>4,"Actual window or rear-view mirror is blank.");
            string filename="multiplayer-hud-"+(expectedView==0?"bumper":"chase")+".png";File.WriteAllBytes(Path.Combine(directory,filename),picture.EncodeToPNG());
            self.captures.Add(new Capture{file=filename,driverText=driver,opponentText=opponent,cameraView=expectedView,viewCount=frame.viewCount,
                width=picture.width,height=picture.height,visiblePixels=visible,mirrorVisiblePixels=mirrorVisible,mirrorDistinctColors=mirrorColors.Count,
                mirrorRanges=mirrorRanges,mainRanges=mainRanges,localCarMainRanges=localMain,localCarMirrorRanges=localMirror,remoteCarMirrorRanges=remoteMirror,
                uiDraws=ui.DrawCount,unresolvedSurfaces=ui.UnresolvedSurfaces,frameGeneration=frame.frameGeneration,
                localRaceTicks=session.LocalSnapshot.raceTicks,remoteRaceTicks=session.RemoteSnapshot.raceTicks,mirrorViewport=viewport,hud=hud});
            complete=true;
        }finally{
            if(picture!=null)UnityEngine.Object.Destroy(picture);QualitySettings.antiAliasing=previousAA;
            if(!complete)self.report.error="HUD view check failed; see parent multiplayer report for the exception.";self.Write(false);
        }
    }
    private void Write(bool passed){report.passed=passed;report.captures=captures.ToArray();File.WriteAllText(Path.Combine(root,"multiplayer-hud-report.json"),JsonUtility.ToJson(report,true));}
    public static void RequirePassed()
    {
        if(!Enabled)return;if(active==null)throw new InvalidOperationException("Race HUD check was requested but never executed.");
        active.Check(active.report.error==null&&active.captures.Count==2,"Both race HUD camera checks did not complete.");
        active.report.bothCameras=active.captures.Exists(c=>c.cameraView==0)&&active.captures.Exists(c=>c.cameraView==1);
        active.Check(active.report.bothCameras,"Race HUD checks omitted a driving camera.");
        active.report.localRemoteRows=active.report.closeSignedGap=active.report.sectionsVerified=active.report.mapMarkers=active.report.mirrorGeometry=true;
        active.Write(true);
    }
}
