using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

// Explicit private visual fixtures plus App-level source finish/timeout tests.
public sealed class Idas3ModeFlowSmoke : MonoBehaviour {
    private static bool MenuHighlightCheck => Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-menu-highlight-check")>=0;
    private static bool MenuPresentationCheck => MenuHighlightCheck || Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-menu-presentation-check")>=0;
    private bool headlightsCheck;
    private KeyCode headlightKey;
    private ushort headlightPad;
    private Idas3ControlBindings headlightBindings;
    private ulong lastHighlightHash;
    private static bool LoadingTransitionCheck => Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-loading-transition-check")>=0;
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneModeFlowFixture(int scene);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneModeFlowValue(int field);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneGetPreRaceStatus(ref Idas3PreRaceSmoke.PreRaceStatus status);
    private Idas3PreRaceSmoke.PreRaceStatus StartStatus(){
        var status=new Idas3PreRaceSmoke.PreRaceStatus{size=(uint)Marshal.SizeOf<Idas3PreRaceSmoke.PreRaceStatus>()};
        Check(Idas3SceneGetPreRaceStatus(ref status)!=0,"Read countdown presentation");return status;
    }
    [Serializable] private class Report {
        public string schema="idas3-mode-flow-smoke-v1",applicationVersion,error,scope;
        public bool passed,shutdownComplete; public int checks; public string[] captures;
    }
    private static string pendingRoot; private static Idas3ModeFlowSmoke active;
    private Idas3SceneGame host; private string root; private int checks; private uint padPulse;
    private bool finished,frozen; private double began; private long submittedFrames; private readonly List<string> captures=new List<string>();
    public static bool Configure(ref string saves){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-mode-flow-smoke");if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Mode-flow diagnostic requires a NEW output directory.");
        pendingRoot=Path.GetFullPath(args[at+1]);if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a NEW mode-flow diagnostic directory.");
        Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(pendingRoot,"ISOLATED_MODE_FLOW_TEST.txt"),"Private mode-flow fixtures. No ordinary saves loaded.\n");
        File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");
        File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"6 0\n");
        Screen.SetResolution(1280,720,FullScreenMode.Windowed);AudioListener.volume=0;return true;
    }
    public static void Attach(Idas3SceneGame game){
        if(pendingRoot==null)return;active=game.gameObject.AddComponent<Idas3ModeFlowSmoke>();
        active.host=game;game.DiagnosticFocusOverride=true;active.root=pendingRoot;active.began=Time.realtimeSinceStartupAsDouble;active.StartCoroutine(active.Guard(active.Run()));
    }
    internal static bool PreparePhysicalInput(ref Func<KeyCode,bool> key,ref Idas3ControlBindings.PadState pad){
        if(active==null)return false;
        // Isolate the private frame fixtures before bindings/pause/music read
        // hardware; the user may still be playing the ordinary desktop build.
        key=_=>false;pad=default;return true;
    }
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame){
        if(active==null)return true;if(active.finished||active.frozen)return false;
        frame=new Idas3Native.FrameInput{size=(uint)Marshal.SizeOf<Idas3Native.FrameInput>(),flags=1,deltaSeconds=1.0/60};
        if(active.headlightsCheck){
            active.headlightBindings.Poll(k=>k==active.headlightKey,new Idas3ControlBindings.PadState{connected=true,buttons=active.headlightPad},Time.realtimeSinceStartupAsDouble);
            active.headlightBindings.ApplyDriving(ref frame);
        }
        ++active.submittedFrames;
        if(active.padPulse!=0){frame.padConnected=1;frame.padButtons=active.padPulse;active.padPulse=0;}return true;
    }
    private void Check(bool ok,string message){++checks;if(!ok)throw new InvalidOperationException(message+"; "+Idas3Native.Error());}
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Pad(uint value){padPulse=value;yield return null;yield return null;}
    private IEnumerator Until(Func<bool> predicate,int count,string message){for(int i=0;i<count&&!predicate();++i)yield return null;Check(predicate(),message);}
    private IEnumerator Fixture(int scene){Check(Idas3SceneModeFlowFixture(scene)==1,"Fixture "+scene);yield return Frames(2);}
    private IEnumerator ChooseFullTuneCar(){
        foreach(int stage in new[]{2,3}){
            yield return Until(()=>Idas3SceneModeFlowValue(5)==stage&&Idas3SceneModeFlowValue(27)==1,300,"Saved driver still visits make/car selection "+stage);
            yield return Capture(stage==2?"full-tune-select-make":"full-tune-select-car");
            yield return Pad(0x1000);
        }
        yield return Until(()=>Idas3SceneModeFlowValue(23)==1,300,"Established car skips setup and enters upgrades");
    }
    private sealed class PerformancePlatform : Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>Array.Empty<Idas3GameOptions.ResolutionChoice>();
        public void Apply(Idas3GameOptions.Values a,Idas3GameOptions.Values b,bool displayChanged){}
    }
    private IEnumerator Run(){
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-headlight-toggle-check")>=0){
            yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);
            headlightsCheck=true;headlightBindings=new Idas3ControlBindings();headlightBindings.Initialize(Path.Combine(root,"binding-test"));
            for(int fixture=240;fixture<=242;++fixture){
                Check(Idas3SceneModeFlowFixture(fixture)==1,"Headlight race fixture");yield return Frames(65);
                Check(Idas3SceneModeFlowValue(36)==1,"Night race starts with headlights on");yield return Capture("headlights-"+fixture+"-on");
                headlightKey=KeyCode.H;yield return Frames(65);
                Check(Idas3SceneModeFlowValue(36)==0,"Held H switches lights off once");
                if(fixture!=241)Check(Idas3SceneModeFlowValue(37)==0,"Pop-up headlights close");
                yield return Capture("headlights-"+fixture+"-off");headlightKey=KeyCode.None;yield return Frames(3);
                headlightPad=0x80;yield return Frames(65);
                Check(Idas3SceneModeFlowValue(36)==1,"Right-stick click switches lights back on once");
                yield return Capture("headlights-"+fixture+"-restored");headlightPad=0;yield return Frames(3);
            }
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-signs-enna-check")>=0){
            yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);
            frozen=true;Check(Idas3SceneModeFlowFixture(208)==1,"Enna selection");yield return Capture("enna-course");
            for(int fixture=230;fixture<=237;++fixture){
                frozen=true;Check(Idas3SceneModeFlowFixture(fixture)==1,"Sadamine sponsor fixture: "+Idas3Native.Error());
                typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).Invoke(host,null);
                yield return Frames(3);yield return Capture("sadamine-"+fixture);
            }
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-map-weather-check")>=0){
            yield return Until(()=>host.Ready,600,"Scene initialized");
            yield return Frames(3); // Imported pack registration completes after diagnostic attachment.
            bool baseline=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-weather-baseline")>=0;
            if(baseline)File.WriteAllText(Path.Combine(root,"UNSHELTERED_BASELINE.txt"),"Diagnostic emitter without shelter");
            frozen=true;Check(Idas3SceneModeFlowFixture(207)==1,"Sadamine selection");yield return Capture("sadamine-course");
            for(int fixture=210;fixture<=(baseline?213:225);++fixture){
                frozen=true;Check(Idas3SceneModeFlowFixture(fixture)==1,"Tunnel fixture: "+Idas3Native.Error());
                yield return Capture("tsuchisaka-"+fixture);
            }
            Finish(true,null);yield break;
        }

        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-akagi-intro-check")>=0){
            yield return Until(()=>host.Ready,600,"Scene initialized");
            for(int fixture=200;fixture<=205;++fixture){
                frozen=true;Check(Idas3SceneModeFlowFixture(fixture)==1,"Akagi intro fixture: "+Idas3Native.Error());
                yield return Capture("akagi-"+fixture);
            }
            frozen=true;Check(Idas3SceneModeFlowFixture(206)==1,"All original course starting cells: "+Idas3Native.Error());
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-bunta-difficulty-check")>=0){
            yield return Frames(3);Check(host.Ready,"Scene initialized");
            Check(Idas3SceneModeFlowFixture(-10)==1,"Bunta difficulty regression: "+Idas3Native.Error());
            Finish(true,null);yield break;
        }
        yield return Until(()=>host.Ready,600,"Game initialization");
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-replay-performance-check")>=0){
            yield return Frames(3);frozen=true;
            Check(Idas3SceneModeFlowFixture(-8)==1,"Online/Legend capture keeps exact opponent telemetry");
            var library=host.GetComponent<Idas3ReplayLibrary>();
            yield return Until(()=>Idas3ReplayLibrary.Files(library.Folder).Length==1,600,"Background local save");
            var source=File.ReadAllBytes(Path.Combine(root,"online-player.idr"));
            const int count=18000;var raw=new byte[96+160*count];Array.Copy(source,raw,96);
            Array.Copy(BitConverter.GetBytes(count*100),0,raw,4,4);Array.Copy(BitConverter.GetBytes(count),0,raw,8,4);
            for(int i=0;i<count;i++){Array.Copy(source,96,raw,96+i*160,160);Array.Copy(BitConverter.GetBytes(i+1),0,raw,96+i*160,4);Array.Copy(BitConverter.GetBytes((i+1)*100),0,raw,192+i*160,4);}
            var meta=new Idas3ReplayData.Details{mode=0,condition=0,car=0,ticks6000=count*100,playerName="PERF TEST"};
            var watch=System.Diagnostics.Stopwatch.StartNew();
            string synchronous=Idas3ReplayLibrary.Save(Path.Combine(root,"sync"),meta,raw,Array.Empty<byte>());
            double syncMs=watch.Elapsed.TotalMilliseconds;watch.Restart();
            var job=Idas3ReplayLibrary.SaveAsync(Path.Combine(root,"async"),meta,raw,Array.Empty<byte>());
            double dispatchMs=watch.Elapsed.TotalMilliseconds;int updateFrames=0;
            while(!job.IsCompleted){updateFrames++;yield return null;}
            string asynchronous=job.GetAwaiter().GetResult();double asyncMs=watch.Elapsed.TotalMilliseconds;
            var data=Idas3ReplayData.Load(asynchronous);Check(data.Frames.Length==count,"All 18000 samples survive background save");
            byte[] packet=File.ReadAllBytes(asynchronous);int offset=4+BitConverter.ToInt32(packet,0);var stored=new byte[packet.Length-offset];Array.Copy(packet,offset,stored,0,stored.Length);
            var decoded=Idas3ReplayCodec.Decode(stored);Check(decoded.Length==raw.Length,"Replay length preserved");
            for(int i=0;i<raw.Length;i++)if(raw[i]!=decoded[i])throw new InvalidOperationException("Replay bits changed at "+i);
            Check(updateFrames>0,"Game frames continued during save");
            var run=new Idas3CommunityTimes.Run{ticks6000=count*100};
            var upload=System.Threading.Tasks.Task.Run(()=>Idas3CommunityTimes.ReplayEnvelope(run,Idas3ReplayCodec.Encode(raw)));
            while(!upload.IsCompleted)yield return null;
            Check(upload.GetAwaiter().GetResult().Length>0,"Community replay processing runs off thread");
            File.WriteAllText(Path.Combine(root,"performance.json"),"{\"syncSaveMs\":"+syncMs.ToString(System.Globalization.CultureInfo.InvariantCulture)+",\"asyncDispatchMs\":"+dispatchMs.ToString(System.Globalization.CultureInfo.InvariantCulture)+",\"asyncTotalMs\":"+asyncMs.ToString(System.Globalization.CultureInfo.InvariantCulture)+",\"framesDuringSave\":"+updateFrames+",\"samples\":"+count+",\"exactBytes\":true}");
            Check(Idas3SceneModeFlowFixture(-5)==1,"Actual Time Attack finish generates a required replay");
            host.GameOptions.Current.communityTimes=true;
            var community=host.gameObject.AddComponent<Idas3CommunityTimes>();
            string storage=Path.Combine(root,"community");community.Initialize(host,host.GameOptions,host.PauseMenu,storage,true);
            yield return Until(()=>community.PendingCount==1,1200,"Background community queue commit");
            Check(Directory.GetFiles(storage,"*.idr").Length==1,"Completed shared replay persisted before acknowledgment");
            var pending=File.ReadAllText(Path.Combine(storage,"pending.json"));Check(pending.Contains(Application.version),"Queue keeps current build policy");
            community.StopAllCoroutines();
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-record-separation-check")>=0){
            yield return Frames(3);frozen=true;
            var entry=new Idas3CommunityTimes.Run{id=Guid.NewGuid().ToString(),ruleset=Idas3CommunityTimes.Ruleset,build=Application.version,epoch=2,replayVersion=2,replayAvailable=true,condition=0,weather=0,car=0,ticks6000=60000,nameGlyphs=new[]{164,169,179,170,180},splits=new[]{20000,40000,60000,0}};
            var cache=new Idas3CommunityTimes.Snapshot{ruleset=Idas3CommunityTimes.Ruleset,epoch=2,entries=new[]{entry}};
            Check(Idas3CommunityTimes.UsableCommunitySnapshot(cache),"New replay-backed records remain visible");
            cache.epoch=1;entry.epoch=1;Check(!Idas3CommunityTimes.UsableCommunitySnapshot(cache),"Old season cache rejected");
            cache.epoch=2;entry.epoch=2;entry.build="0.3.94-player-replays.4";Check(!Idas3CommunityTimes.UsableCommunitySnapshot(cache),"Old build cache rejected");
            entry.build=Application.version;entry.replayAvailable=false;Check(!Idas3CommunityTimes.UsableCommunitySnapshot(cache),"Pre-replay cache rejected");
            entry.replayAvailable=true;
            Check(!Idas3CommunityTimes.UseCommunityRecords(true,cache,10,0),"Offline startup does not trust a disk cache");
            Check(Idas3CommunityTimes.UseCommunityRecords(true,cache,10,85),"Successful current fetch enables remote records");
            Check(!Idas3CommunityTimes.UseCommunityRecords(true,cache,86,85),"Expired connection falls back to personal records");
            Check(!Idas3CommunityTimes.UseCommunityRecords(true,cache,10,0),"Failed fetch clears remote display eligibility");
            Check(!Idas3CommunityTimes.UseCommunityRecords(false,cache,10,85),"Disabling community times selects personal records");
            Check(Idas3CommunityTimes.UseCommunityRecords(true,cache,86,160),"Reconnection restores current remote records");
            cache.entries=Array.Empty<Idas3CommunityTimes.Run>();Check(Idas3CommunityTimes.UsableCommunitySnapshot(cache),"Empty fresh board accepted");
            Check(Idas3SceneModeFlowFixture(-9)==1,"Native personal/community record separation");
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-performance-options-check")>=0){
            yield return Frames(3);frozen=true;
            string folder=Path.Combine(root,"options-test");Directory.CreateDirectory(folder);
            File.WriteAllText(Path.Combine(folder,"game-options.json"),"{\"version\":1,\"musicVolume\":0.4}");
            var options=new Idas3GameOptions(new PerformancePlatform());options.Initialize(folder);
            Check(options.Current.rainDetail==0&&options.Current.importedSceneryDetail==0,"Older settings retain original visuals");
            options.BeginEdit();options.SetPerformancePreset(2);
            Check(options.Current.rainDetail==0&&options.Draft.musicVolume==.4f,"Preset only edits draft graphics");
            Check(options.ApplyDraft()&&options.DisplayConfirmationPending,"Low resolution uses display confirmation");
            options.Tick(16);Check(options.Current.rainDetail==0&&options.Current.width==1280,"Timeout reverts every preset setting");
            options.SetPerformancePreset(2);Check(options.ApplyDraft()&&options.ConfirmDisplay(),"Low preset can be confirmed");
            var reloaded=new Idas3GameOptions(new PerformancePlatform());reloaded.Initialize(folder);
            Check(Idas3GameOptions.PerformancePreset(reloaded.Current)==2,"Low settings survive reload");
            options.BeginEdit();options.SetPerformancePreset(1);Check(Idas3GameOptions.PerformancePreset(options.Draft)==1,"Balanced preset values");
            options.SetPerformancePreset(0);Check(Idas3GameOptions.PerformancePreset(options.Draft)==0&&options.Draft.musicVolume==.4f,"Original restores effects without audio changes");
            var invalid=new Idas3GameOptions.Values{rainDetail=99,importedSceneryDetail=-5};var normalized=Idas3GameOptions.Normalize(invalid);
            Check(normalized.rainDetail==1&&normalized.importedSceneryDetail==0,"Malformed detail values clamped; rain always visible");
            foreach(int detail in new[]{0,1,2}){
                float d=Idas8HakoneCourse.SceneryDistanceSquared(200*200,detail);
                Check(Idas8HakoneCourse.TreeLod(d,-2)==(detail==0?0:1),"Scenery detail changes real mesh LOD");
            }
            Check(Idas8HakoneCourse.TreeLod(Idas8HakoneCourse.SceneryDistanceSquared(900*900,2),-2)==-1,"Low culls distant decorative trees");
            var menu=host.PauseMenu;menu.OpenAttractOptions();menu.SelectTab(1);
            for(int i=0;i<5;++i)menu.Navigate(1);
            Check(menu.DiagnosticSelection==6,"Graphics controller reaches quality preset after existing options");menu.Activate();
            Check(Idas3GameOptions.PerformancePreset(host.GameOptions.Draft)==1,"Controller selects Balanced preset");
            menu.Navigate(1);menu.Activate();Check(host.GameOptions.Draft.rainDetail==0,"Controller rain detail toggle");
            menu.Navigate(1);menu.Activate();Check(host.GameOptions.Draft.importedSceneryDetail==2,"Controller scenery toggle");
            menu.Navigate(1);Check(menu.DiagnosticSelection==9,"Graphics controller reaches Reset Defaults");
            menu.Navigate(1);Check(menu.DiagnosticSelection==10,"Graphics controller reaches Apply");
            menu.Navigate(1);Check(menu.DiagnosticSelection==11,"Graphics controller reaches Back");
            menu.SetOpen(false);Check(host.GameOptions.Current.rainDetail==0,"Leaving without Apply discards graphics edits");
            Check(Idas3Native.Idas3SceneSetPerformance(-1)==0&&Idas3Native.Idas3SceneSetPerformance(2)==0,"Native quality boundary rejects invalid values");
            Check(Idas3SceneModeFlowFixture(-7)==1,"Native weather/mirror and race invariance regression");
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-player-replays-check")>=0){
            yield return Frames(3);frozen=true;
            Check(Idas3SceneModeFlowFixture(-8)==1,"Both modes capture private recordings");yield return Frames(4);
            var library=host.GetComponent<Idas3ReplayLibrary>();yield return Until(()=>library!=null&&Idas3ReplayLibrary.Files(library.Folder).Length==1,600,"Asynchronous personal replay save");Check(library!=null&&Idas3ReplayLibrary.Files(library.Folder).Length==1,"Pending personal replay automatically saved locally");
            var saved=Idas3ReplayData.Load(Idas3ReplayLibrary.Files(library.Folder)[0]);Check(saved.Metadata.mode==1&&saved.Opponent!=null,"Stored online replay retains both cars");
            var menu=host.PauseMenu;menu.OpenAttractOptions();menu.SelectTab(6);Check(menu.SelectedTab==6,"Replays category selectable");menu.Activate();
            Check(menu.TryConsumeCommand(out var command)&&command==Idas3PauseMenu.Command.Replays,"Controller can open library");
            host.GameOptions.Draft.communityTimes=true;host.GameOptions.Draft.replayTimeAttack=false;menu.Navigate(1);menu.Activate();
            Check(!host.GameOptions.Draft.replayTimeAttack&&host.GameOptions.Draft.TimeAttackReplayRequired,"Required capture cannot be disabled");
            menu.Navigate(1);menu.Activate();Check(host.GameOptions.Draft.replayOnline,"Controller toggles online capture");menu.Navigate(1);menu.Activate();Check(host.GameOptions.Draft.replayLegend,"Controller toggles Legend capture");
            menu.Back();Check(menu.DiagnosticSelection==0,"Back returns to categories");menu.Navigate(-1);Check(menu.SelectedTab==5,"Other categories remain reachable");menu.SelectTab(6);
            yield return Capture("replays-options");
            menu.SetOpen(false);Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-replay-policy-check")>=0){
            yield return Frames(3);frozen=true;
            var fresh=new Idas3CommunityTimes.Run{id=Guid.NewGuid().ToString(),ruleset=Idas3CommunityTimes.Ruleset,build=Application.version,epoch=2,replayVersion=2,condition=0,weather=0,car=0,ticks6000=60000,nameGlyphs=new[]{181,166,180,181,220},splits=new[]{20000,40000,60000,0}};
            var old=JsonUtility.FromJson<Idas3CommunityTimes.Run>(JsonUtility.ToJson(fresh));old.id=Guid.NewGuid().ToString();old.replayVersion=1;
            var imported=JsonUtility.FromJson<Idas3CommunityTimes.Run>(JsonUtility.ToJson(fresh));imported.id=Guid.NewGuid().ToString();imported.imported=1;
            Check(Idas3CommunityTimes.Uploadable(fresh)&&!Idas3CommunityTimes.Uploadable(old)&&!Idas3CommunityTimes.Uploadable(imported),"Only fresh replay-backed runs are uploadable");
            foreach(string build in new[]{"0.3.93-replay-detail.1","0.3.94-player-replays.4","0.3.95-community-replays.0","0.3.95-other.99","invalid",null})Check(!Idas3CommunityTimes.SupportedBuild(build),"Older/unknown build rejected: "+build);
            foreach(string build in new[]{Application.version,"0.3.95-community-replays.2","0.3.95-community-replays.10","0.3.95","0.3.96","0.4.0"})Check(Idas3CommunityTimes.SupportedBuild(build),"Current/newer build accepted: "+build);
            var previousBuild=JsonUtility.FromJson<Idas3CommunityTimes.Run>(JsonUtility.ToJson(fresh));previousBuild.id=Guid.NewGuid().ToString();previousBuild.build="0.3.94-player-replays.4";
            var previousSeason=JsonUtility.FromJson<Idas3CommunityTimes.Run>(JsonUtility.ToJson(fresh));previousSeason.id=Guid.NewGuid().ToString();previousSeason.epoch=1;
            Check(!Idas3CommunityTimes.Uploadable(previousBuild)&&!Idas3CommunityTimes.Uploadable(previousSeason),"Old build and season queues cannot re-enter rankings");
            Check(Idas3CommunityTimes.Flatten(new Idas3CommunityTimes.Snapshot{ruleset=Idas3CommunityTimes.Ruleset,entries=new[]{old,imported}}).Length==28,"Existing leaderboard history remains readable");
            string community=Path.Combine(root,"community");Directory.CreateDirectory(community);
            File.WriteAllText(Path.Combine(community,"pending.json"),"{\"runs\":["+JsonUtility.ToJson(old)+","+JsonUtility.ToJson(imported)+","+JsonUtility.ToJson(previousBuild)+","+JsonUtility.ToJson(previousSeason)+","+JsonUtility.ToJson(fresh)+"]}");
            byte[] replay=new byte[96+600*160];
            foreach(var item in new[]{(0,0x32524449),(4,60000),(8,600),(12,60),(16,96),(20,160),(24,1),(28,12)})Array.Copy(BitConverter.GetBytes(item.Item2),0,replay,item.Item1,4);
            for(int i=0;i<600;i++){Array.Copy(BitConverter.GetBytes(i+1),0,replay,96+160*i,4);Array.Copy(BitConverter.GetBytes((i+1)*100),0,replay,192+160*i,4);Array.Copy(BitConverter.GetBytes(4),0,replay,220+160*i,4);}
            replay=Idas3ReplayCodec.Encode(replay);
            File.WriteAllBytes(Path.Combine(community,fresh.id+".idr"),replay);
            host.GameOptions.Current.communityTimes=true;
            var client=host.gameObject.AddComponent<Idas3CommunityTimes>();client.Initialize(host,host.GameOptions,host.PauseMenu,community);client.StopAllCoroutines();
            Check(client.PendingCount==1,"Upgrade discards legacy/imported upload queue entries");
            string saved=File.ReadAllText(Path.Combine(community,"pending.json"));Check(saved.Contains(fresh.id)&&!saved.Contains(old.id)&&!saved.Contains(imported.id),"Queue migration is persisted");
            Check(File.Exists(Path.Combine(community,fresh.id+".idr")),"Eligible queued replay is preserved");
            var menu=host.PauseMenu;menu.OpenAttractOptions();menu.SelectTab(5);menu.Navigate(1);Check(menu.DiagnosticSelection==2,"Controller reaches rankings");
            menu.Navigate(1);Check(menu.DiagnosticSelection==3,"Controller reaches Reset after the two Records options");menu.Navigate(1);menu.Navigate(1);Check(menu.DiagnosticSelection==5,"Controller reaches Back without an import row");
            menu.SetOpen(false);Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-replay-local-check")>=0){
            yield return Frames(3);frozen=true;
            Check(Idas3SceneModeFlowFixture(-5)==1,"Native finish/replay fixtures");
            int[] courses={0,1,3,9,10};int index=0;
            foreach(var line in File.ReadAllLines(Path.Combine(root,"native-finishes.jsonl"))){
                var run=JsonUtility.FromJson<Idas3CommunityTimes.Run>(line);
                byte[] replay=File.ReadAllBytes(Path.Combine(root,"native-replay-"+courses[index++]+".idr"));
                Check(Idas3CommunityTimes.ReplayEnvelope(run,replay).Length>replay.Length,"Native replay matches finished run");
            }
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-community-check")>=0){
            yield return Frames(3);frozen=true;
            Check(Idas3SceneModeFlowFixture(-5)==1,"Shared time App regression");
            foreach(var line in File.ReadAllLines(Path.Combine(root,"native-finishes.jsonl"))){
                var run=JsonUtility.FromJson<Idas3CommunityTimes.Run>(line);Check(Idas3CommunityTimes.Valid(run),"Actual native finish validates for upload");
                var snapshot=new Idas3CommunityTimes.Snapshot{ruleset=Idas3CommunityTimes.Ruleset,entries=new[]{run}};
                Check(Idas3CommunityTimes.Flatten(snapshot).Length==14,"Shared snapshot packs native records");
            }
            var menu=host.PauseMenu;menu.OpenAttractOptions();menu.SelectTab(5);bool before=host.GameOptions.Draft.communityTimes;
            menu.Activate();Check(host.GameOptions.Draft.communityTimes!=before,"Controller toggles community sharing");
            menu.Navigate(1);Check(menu.DiagnosticSelection==2,"Controller reaches shared rankings button");
            menu.SetOpen(false);Check(host.GameOptions.Current.communityTimes==before,"Unapplied sharing changes are discarded");
            string community=Path.Combine(root,"community");
            host.GameOptions.Current.communityTimes=true;
            var uploading=host.gameObject.AddComponent<Idas3CommunityTimes>();uploading.Initialize(host,host.GameOptions,menu,community);
            yield return Frames(3);
            string queued=File.ReadAllText(Path.Combine(community,"pending.json"));
            Check(queued.Contains("\"replayVersion\":2"),"Replay version persisted with queued time");
            var replayFiles=Directory.GetFiles(community,"*.idr");Check(replayFiles.Length==1,"Matching replay persisted before network upload");
            string submittedId=Path.GetFileNameWithoutExtension(replayFiles[0]);
            // Recreate the uploader before its initial network delay expires:
            // the finish was acknowledged, so only disk can recover the pair.
            uploading.StopAllCoroutines();Destroy(uploading);yield return Frames(1);
            uploading=host.gameObject.AddComponent<Idas3CommunityTimes>();uploading.Initialize(host,host.GameOptions,menu,community);
            Check(uploading.PendingCount==1,"Restart reloads the acknowledged time/replay pair from disk");
            double deadline=Time.realtimeSinceStartupAsDouble+60;
            while(Time.realtimeSinceStartupAsDouble<deadline&&!File.Exists(Path.Combine(community,"snapshot.json")))yield return null;
            Check(File.Exists(Path.Combine(community,"snapshot.json")),"Unity HTTPS registration/upload/download completed");
            var downloaded=JsonUtility.FromJson<Idas3CommunityTimes.Snapshot>(File.ReadAllText(Path.Combine(community,"snapshot.json")));
            Check(Array.Exists(downloaded.entries,r=>r.id==submittedId&&r.replayAvailable),"Uploaded native finish has server-attached replay");
            Check(File.ReadAllText(Path.Combine(community,"pending.json")).Contains("\"runs\":[]"),"Acknowledged upload removed from durable queue");
            Check(Directory.GetFiles(community,"*.idr").Length==0,"Acknowledged replay file removed from queue");
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-imported-car-light-check")>=0){
            // Attach runs before the host registers the installed course packs.
            yield return Frames(3);
            frozen=true;Check(Idas3SceneModeFlowFixture(-4)==1,"Imported car lighting App regression");
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-full-tune-check")>=0){
            yield return Fixture(140);
            var menu=(Idas3PauseMenu)typeof(Idas3SceneGame).GetField("pauseMenu",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(host);
            menu.OpenAttractOptions();menu.SelectTab(2);yield return Frames(3);
            Check(menu.FullTuneAvailable,"Gameplay Full Tune available for selected saved driver");
            for(int i=0;i<6;++i)menu.Navigate(1);
            // Hidden diagnostic windows do not receive IMGUI repaint events.
            // Exercise the real menu's navigation/action; capture native screens below.
            menu.Activate();yield return Frames(8);
            Check(!menu.IsOpen&&Idas3SceneModeFlowValue(29)==1&&Idas3SceneModeFlowValue(5)==1,"Gameplay action opens save selection");
            yield return Capture("full-tune-save-select");yield return Pad(0x1000);yield return Frames(8);
            yield return ChooseFullTuneCar();
            Check(Idas3SceneModeFlowValue(23)==1&&Idas3SceneModeFlowValue(28)==0,"Existing save goes directly to tuning");
            yield return Frames(75);yield return Capture("forced-upgrade");
            frozen=true;Check(Idas3SceneModeFlowFixture(141)==1,"Actual mandatory upgrade chain and persistence");frozen=false;
            yield return Frames(12);yield return Capture("optional-upgrade");
            yield return Pad(4);yield return Pad(0x1000);yield return Frames(350);yield return Fixture(142);
            yield return Pad(0x2000);yield return Frames(3);
            Check(Idas3SceneModeFlowValue(23)==0&&Idas3SceneModeFlowValue(4)==1,"Controller Back finishes optional tuning");
            yield return Capture("choose-a-mode");
            // Run a second visit to exercise natural completion of every optional offer.
            Check(Idas3Native.Idas3SceneFullTune()==1,"Repeat Full Tune on fully tuned driver");yield return Frames(3);yield return Pad(0x1000);yield return Frames(3);yield return ChooseFullTuneCar();
            frozen=true;Check(Idas3SceneModeFlowFixture(143)==1,"Optional choices naturally finish at mode select, preserve saves and records");frozen=false;
            yield return Frames(2);
            Check(Idas3Native.Idas3SceneFullTune()==1,"Full Tune for a different car in the same save");yield return Frames(3);yield return Pad(0x1000);
            foreach(int stage in new[]{2,3,4,12}){
                yield return Until(()=>Idas3SceneModeFlowValue(5)==stage&&Idas3SceneModeFlowValue(27)==1,300,"Normal new-car setup stage "+stage);
                if(stage==3){yield return Pad(8);yield return Frames(3);yield return Pad(0x8000);yield return Capture("new-car-color-selection");}
                yield return Pad(0x1000);
            }
            yield return Until(()=>Idas3SceneModeFlowValue(5)==11&&Idas3SceneModeFlowValue(27)==1,300,"Normal new-driver name entry");
            for(int i=0;i<6;++i)yield return Pad(0x1000);
            yield return Until(()=>Idas3SceneModeFlowValue(23)==1,100,"New driver goes to upgrades after setup");
            yield return Capture("new-car-forced-upgrade");frozen=true;Check(Idas3SceneModeFlowFixture(145)==1,"New save full flow and retained color/name");frozen=false;
            yield return Frames(2);yield return Capture("new-save-choose-a-mode");yield return Fixture(144);
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-menu-car-flicker-check")>=0){
            foreach(int car in new[]{0,3,8,20,30}){
                yield return Fixture(100+car);yield return Frames(35);
                for(int angle=0;angle<8;angle++){
                    yield return Capture("menu-car-"+car+"-angle-"+angle);
                    if(car==0&&angle==2)for(int f=0;f<12;f++)yield return Capture("menu-car-motion-"+f);
                    yield return Frames(62);
                }
            }
            Finish(true,null);yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-finish-transition-check")>=0){yield return FinishTransition();yield break;}
        if(LoadingTransitionCheck){yield return LoadingTransition();yield break;}
        if(MenuPresentationCheck){
            yield return MenuPresentation();yield break;
        }
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-analysis-banner-check")>=0){
            frozen=true;Check(Idas3SceneModeFlowFixture(-1)==1,"Actual race driving-path recording");frozen=false;
            yield return Fixture(15);yield return Frames(30);yield return Capture("myogi-reverse-driving-line");
            yield return Fixture(16);yield return Frames(30);yield return Capture("akina-driving-line");
            for(int map=1;map<=3;++map){yield return Pad(0x8000);yield return Frames(3);yield return Capture("akina-driving-line-zoom-"+map);}
            for(int scene=17;scene<=20;++scene){yield return Fixture(scene);yield return Capture("legend-banner-"+scene);}
            Finish(true,null);yield break;
        }
        frozen=true;Check(Idas3SceneModeFlowFixture(-1)==1,"Native App finish/timeup and Bunta regression");frozen=false;
        frozen=true;Check(Idas3SceneModeFlowFixture(-2)==1,"Native App original coaching and personal record sequence");frozen=false;
        frozen=true;Check(Idas3SceneModeFlowFixture(-3)==1,"Native App solo and multiplayer finish music routing");frozen=false;
        yield return Fixture(0);yield return Frames(120);
        Check(Idas3SceneModeFlowValue(3)==4,"Bunta four-star progress");yield return Capture("bunta-course-four-stars");
        yield return Fixture(1);yield return Frames(100);
        Check(Idas3SceneModeFlowValue(0)==1&&Idas3SceneModeFlowValue(2)==33,"Bunta challenge scene/music");
        yield return Capture("bunta-challenge");yield return Pad(0x10);
        yield return Until(()=>Idas3SceneModeFlowValue(0)==0,100,"Controller START closes challenge into loading");
        for(int scene=2;scene<=3;++scene){
            yield return Fixture(scene);yield return Frames(100);
            Check(Idas3SceneModeFlowValue(0)==1&&Idas3SceneModeFlowValue(2)==(scene==2?34:35),"Bunta result scene/music");
            yield return Capture(scene==2?"bunta-win":"bunta-loss");yield return Pad(0x10);
            yield return Until(()=>Idas3SceneModeFlowValue(0)==0&&Idas3SceneModeFlowValue(4)==1,100,"Bunta result returns to course selection");
        }
        yield return Fixture(4);yield return Frames(60);
        Check(Idas3SceneModeFlowValue(1)==1&&Idas3SceneModeFlowValue(2)==2,"TA analysis scene/music");
        yield return Capture("time-attack-analysis");yield return Pad(0x1000);
        yield return Until(()=>Idas3SceneModeFlowValue(0)==0,100,"TA analysis accepts controller confirm");
        yield return Fixture(5);yield return Frames(120);
        Check(Idas3SceneModeFlowValue(1)==2,"TA named local ranking");yield return Capture("time-attack-ranking");
        yield return Pad(0x1000);yield return Until(()=>Idas3SceneModeFlowValue(1)==3,60,"Ranking fades to Continue");
        yield return Capture("time-attack-continue");yield return Pad(0x1000);
        yield return Until(()=>Idas3SceneModeFlowValue(0)==0&&Idas3SceneModeFlowValue(4)==1,100,"Continue YES returns to course menu");
        int courseStage=Idas3SceneModeFlowValue(5);
        yield return Fixture(6);yield return Pad(0x8);Check(Idas3SceneModeFlowValue(6)==1,"Controller chooses NO");
        yield return Pad(0x1000);yield return Until(()=>Idas3SceneModeFlowValue(0)==0&&Idas3SceneModeFlowValue(4)==1,100,"Continue NO exits");
        Check(Idas3SceneModeFlowValue(5)!=courseStage,"Continue NO returns to title");
        for(int kind=0;kind<3;++kind){
            yield return Fixture(7+kind);yield return Frames(60);
            Check(Idas3SceneModeFlowValue(9)==kind,"Original coaching kind "+kind);
            yield return Capture("time-attack-coaching-"+kind);
        }
        for(int page=0;page<4;++page){
            yield return Fixture(10+page);yield return Frames(160);
            Check(Idas3SceneModeFlowValue(13)==12&&Idas3SceneModeFlowValue(11)==1&&Idas3SceneModeFlowValue(12)==page,"Original attract model page "+page);
            yield return Capture("time-attack-model-ranking-"+page);
        }
        yield return Fixture(10);yield return Pad(0x200);
        Check(Idas3SceneModeFlowValue(12)==1,"Controller shoulder advances model detail");
        int condition=Idas3SceneModeFlowValue(14);yield return Pad(0x8000);
        Check(Idas3SceneModeFlowValue(14)!=condition,"Controller view changes ranking condition");
        yield return Pad(0x1000);
        yield return Until(()=>Idas3SceneModeFlowValue(5)!=0,120,"Controller confirm leaves attract for game menu");
        yield return Fixture(14);yield return Frames(160);yield return Capture("time-attack-points");
        Finish(true,null);
    }
    private IEnumerator FinishTransition(){
        string[] names={"course","model","personal","none"};
        int[] flags={0x78000000,0x68000000,0x48000000,0};
        for(int variant=0;variant<4;++variant){
        yield return Fixture(66+variant);
        Check(Idas3SceneModeFlowValue(18)==flags[variant],"Finish derives "+names[variant]+" record flags from earlier clocks");
        yield return Capture("finish-road-"+names[variant],0);
        var scene=host.GetComponent<Idas3SceneRenderer>();
        int samples=0;
        while(Idas3SceneModeFlowValue(15)<120){
            Check(Idas3SceneModeFlowValue(16)==0,"Finish remains until two seconds");
            Check((scene.CurrentFrame.screenFadeArgb>>24)==0,"No fade before result handoff");
            ++samples;yield return null;
        }
        Check(samples>100,"Observed finish sequence, not just a settled fixture");
        if(variant==3){
            Check(Idas3SceneModeFlowValue(16)==0,"No-record FINISH remains visible while WIN audio plays");
            Check(Idas3SceneModeFlowValue(17)==0,"No empty summary hold when no record was beaten");
        }else{
        Check(Idas3SceneModeFlowValue(15)==120&&Idas3SceneModeFlowValue(16)==1,"Direct record handoff at 120 ticks");
        Check(Idas3SceneModeFlowValue(0)==0,"Record message stays on the road before analysis");
        Check(Idas3SceneModeFlowValue(17)<=2,"Record panel begins immediately at handoff");
        yield return Capture("new-"+names[variant]+"-record-immediate",0);
        yield return Frames(30);yield return Capture("new-"+names[variant]+"-record-road",0);
        foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(2560,1080)})
            yield return Capture("new-"+names[variant]+"-record-"+size.x+"x"+size.y,0,size.x,size.y);
        }
        Check(Idas3SceneModeFlowValue(19)==0&&Idas3SceneModeFlowValue(20)==2,"Finish stream still playing after the initial banner");
        // The ordinary controller confirm/accelerator input must not skip.
        yield return Pad(0x1000);
        Check(Idas3SceneModeFlowValue(0)==0&&Idas3SceneModeFlowValue(22)==0,"Confirm cannot skip finish audio");
        int previousCursor=Idas3SceneModeFlowValue(21),audioSamples=0;bool sawAudioEnd=false;
        while(Idas3SceneModeFlowValue(0)==0&&audioSamples<3600){
            Check(Idas3SceneModeFlowValue(20)==2,"WIN stays selected until its natural completion");
            int cursor=Idas3SceneModeFlowValue(21);
            Check(cursor>=previousCursor,"Finish audio cursor never restarts");previousCursor=cursor;
            sawAudioEnd|=Idas3SceneModeFlowValue(19)!=0;
            Check((scene.CurrentFrame.screenFadeArgb>>24)==0,"No fade while finish audio completes");
            ++audioSamples;yield return null;
        }
        Check(sawAudioEnd,"Observed the actual WIN stream endpoint before analysis replaced it");
        Check(Idas3SceneModeFlowValue(0)==1&&Idas3SceneModeFlowValue(22)==0,"Natural audio completion reaches analysis without a skip");
        yield return Frames(30);yield return Capture("analysis-after-audio-"+names[variant],0);
        File.AppendAllText(Path.Combine(root,"finish-audio.txt"),names[variant]+": waited "+audioSamples+" frames, last WIN sample "+previousCursor+", endpoint observed "+sawAudioEnd+"\n");
        }
        // Start skips from either FINISH or the subsequent record message.
        foreach(int wait in new[]{30,180}){
            yield return Fixture(66);yield return Frames(wait);
            Check(Idas3SceneModeFlowValue(19)==0,"Start test begins during audio playback");
            yield return Pad(0x10);
            yield return Until(()=>Idas3SceneModeFlowValue(0)==1,4,"Start promptly skips finish audio into analysis");
            Check(Idas3SceneModeFlowValue(22)==1,"Start explicitly marks the audio skip");
        }
        Finish(true,null);
    }
    private IEnumerator LoadingTransition(){
        // Reach ordinary Time Attack selection with a private completed-run
        // fixture, then use the real confirmation/loading/race path.
        yield return Fixture(6);yield return Frames(30);yield return Pad(0x1000);
        yield return Until(()=>host.Status.frontendStage==6&&(host.Status.flags&1)!=0,160,"Continue reaches course selection");
        yield return Frames(30);
        for(int stage=6;stage<=9;++stage){
            Check(host.Status.frontendStage==stage,"Loading test selection stage "+stage);
            yield return Pad(0x1000);
            if(stage<9){yield return Until(()=>host.Status.frontendStage==stage+1,120,"Next selection stage");yield return Frames(30);}
        }
        yield return Until(()=>(host.Status.flags&1024u)!=0,120,"Loading artwork entered");
        var scene=host.GetComponent<Idas3SceneRenderer>();
        yield return Capture("loading-artwork",0);
        ulong frozenTicks=host.Status.simulationTicks;
        bool fadeOut=false,blackCapture=false;long firstBlack=-1,lastBlack=-1;
        for(int i=0;i<420&&(host.Status.flags&1024u)!=0;++i){
            uint alpha=scene.CurrentFrame.screenFadeArgb>>24;
            Check(host.Status.simulationTicks==frozenTicks,"Loading and black hold must not advance race physics");
            if(alpha==255){
                if(firstBlack<0)firstBlack=submittedFrames;
                lastBlack=submittedFrames;
                if(!blackCapture){yield return Capture("loading-black-hold",255);blackCapture=true;}
            }else if(firstBlack>=0)Check(false,"Loading artwork reappeared during black hold");
            else if(!fadeOut&&alpha>=100&&alpha<=160){yield return Capture("loading-artwork-fading",(int)alpha);fadeOut=true;}
            yield return null;
        }
        Check((host.Status.flags&1024u)==0&&firstBlack>=0&&fadeOut,"Loading fades out and releases race");
        // Capture may consume a presentation frame, but no native update while
        // frozen. Count submitted 1/60s inputs, never screenshot wall time.
        long blackFrames=submittedFrames-firstBlack;
        Check(blackFrames>=120&&blackFrames<=121,"Black hold is two seconds at 60 Hz; actual frames="+blackFrames);
        Check((scene.CurrentFrame.screenFadeArgb>>24)==0,"Race intro appears immediately without an incoming fade");
        Check((host.Status.flags&(1u|1024u))==0,"Race remains active after loading transition");
        yield return Capture("loading-race-visible",0);
        foreach(uint age in new uint[]{2,45,62,105,122,165,182,215,235}){
            yield return Until(()=>StartStatus().countdownRemaining<=240-age,900,"Countdown sample "+age);
            yield return Capture("countdown-age-"+age,0);
            if(age==182)foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1280,720),new Vector2Int(1920,1080),new Vector2Int(2560,1080)})
                yield return Capture("countdown-go-"+size.x+"x"+size.y,0,size.x,size.y);
        }
        yield return Until(()=>host.Status.racePhase==2,900,"Intro and countdown release driving");
        yield return Capture("loading-countdown-complete",0);
        File.WriteAllText(Path.Combine(root,"loading-timing.json"),"{\"blackFrames\":"+blackFrames+",\"lastBlackFrame\":"+lastBlack+",\"deltaSeconds\":0.016666666666666666,\"scope\":\"Actual App transition through fixed 60 Hz diagnostic inputs and Unity pixel captures\"}");
        Finish(true,null);
    }
    // Exercise the existing owners and Unity overlay, including the 3D car.
    // The full-black assertion catches a missing fade even if native timing passes.
    private IEnumerator MenuPresentation(){
        yield return Pad(0x1000);yield return Until(()=>host.Status.frontendStage==1,120,"Attract reaches save selection");
        yield return Pad(0x1000);yield return Until(()=>host.Status.frontendStage==2,120,"Empty slot reaches manufacturer selection");
        yield return Frames(20);yield return Capture("make-settled");
        yield return MenuTransition(2,3,"make-to-car");
        yield return Frames(20);yield return Capture("car-settled");
        yield return MenuTransition(3,4,"car-to-transmission");
        // Existing isolated Continue fixture enters the real course-selection
        // owner with a completed driver, avoiding synthetic name-entry shortcuts.
        yield return Fixture(6);yield return Frames(30);yield return Pad(0x1000);
        yield return Until(()=>host.Status.frontendStage==6&&(host.Status.flags&1)!=0,160,"Continue reaches Time Attack course selection");
        yield return Frames(20);
        for(int stage=6;stage<9;++stage){
            yield return Capture("choice-"+stage+"-settled");
            yield return ContinuousChoice(stage,stage+1,"choice-"+stage);
            yield return Frames(20);
        }
        if(MenuHighlightCheck){
            Check(host.Status.frontendStage==9,"Final time-of-day choice");
            yield return Capture("choice-9-settled");
            yield return Pad(0x1000);
            yield return ConfirmationHighlights(9,"choice-9");
            bool partial=false,black=false;
            for(int frame=0;frame<120&&(host.Status.flags&1024u)==0;++frame){
                uint alpha=host.GetComponent<Idas3SceneRenderer>().CurrentFrame.screenFadeArgb>>24;
                if(!partial&&alpha>=100&&alpha<=200){yield return Capture("final-choice-fading",(int)alpha);partial=true;}
                else if(!black&&alpha==255){yield return Capture("final-choice-black",255);black=true;}
                yield return null;
            }
            Check(partial&&black,"Only final choice fades through black before loading");
            Check((host.Status.flags&1024u)!=0,"Final choice reaches loading artwork");
            yield return Capture("final-choice-loading-artwork",0);
        }
        Finish(true,null);
    }
    private IEnumerator ConfirmationHighlights(int stage,string name){
        var images=new HashSet<ulong>();
        for(int sample=0;sample<8;++sample){
            Check(host.Status.frontendStage==stage,"Confirmation remains on selected page "+name);
            yield return Capture(name+"-highlight-"+sample,0);images.Add(lastHighlightHash);
            yield return Frames(1);
        }
        Check(images.Count==2,"Exactly two visible confirmation-glow states in actual Unity pixels "+name+": "+images.Count);
    }
    private IEnumerator ContinuousChoice(int from,int to,string name){
        Check(host.Status.frontendStage==from,"Choice starting stage "+name);
        yield return Pad(0x1000);
        if(MenuHighlightCheck)yield return ConfirmationHighlights(from,name);
        for(int frame=0;frame<240&&host.Status.frontendStage==from;++frame){
            Check(host.GetComponent<Idas3SceneRenderer>().CurrentFrame.screenFadeArgb==0,"Choice remains visible "+name);
            yield return null;
        }
        Check(host.Status.frontendStage==to,"Choice target stage "+name);
        for(int frame=0;frame<20;++frame){
            Check(host.GetComponent<Idas3SceneRenderer>().CurrentFrame.screenFadeArgb==0,"Next choice appears without black "+name);
            yield return null;
        }
        yield return Capture(name+"-continuous",0);
    }
    private IEnumerator MenuTransition(int from,int to,string name){
        Check(host.Status.frontendStage==from,"Transition starting stage "+name);
        yield return Pad(0x1000);bool partial=false,black=false;
        for(int frame=0;frame<240&&host.Status.frontendStage==from;++frame){
            uint alpha=host.GetComponent<Idas3SceneRenderer>().CurrentFrame.screenFadeArgb>>24;
            if(!partial&&alpha>=100&&alpha<=200){yield return Capture(name+"-fading",(int)alpha);partial=true;}
            else if(!black&&alpha==255){yield return Capture(name+"-black",255);black=true;}
            yield return null;
        }
        Check(partial&&black,"Visible outgoing fade and black handoff "+name);
        Check(host.Status.frontendStage==to,"Transition target stage "+name);
        yield return Frames(20);
        Check((host.GetComponent<Idas3SceneRenderer>().CurrentFrame.screenFadeArgb>>24)==0,"Next menu fades fully in "+name);
    }
    private IEnumerator Capture(string name,int expectedFade=-1,int width=0,int height=0){
        frozen=true;var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();
        var previous=camera.targetTexture;var target=new RenderTexture(width>0?width:Screen.width,height>0?height:Screen.height,24,RenderTextureFormat.ARGB32){antiAliasing=1};
        Check(target.Create(),"Capture target");camera.targetTexture=target;
        if(width>0){frozen=false;yield return Frames(2);frozen=true;Check(scene.CurrentFrame.width==width&&scene.CurrentFrame.height==height,"Native HUD capture resolution");}
        scene.ApplyFrame();ui.ApplyFrame();
        var cameras=new List<Camera>();foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
          cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));foreach(var item in cameras)item.Render();
          Idas3PauseMenu captureMenu=null;
          if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-full-tune-check")>=0||Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-player-replays-check")>=0){
              captureMenu=(Idas3PauseMenu)typeof(Idas3SceneGame).GetField("pauseMenu",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(host);
              if(captureMenu.IsOpen){camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();captureMenu.RequestDiagnosticCapture(target);
                  yield return Until(()=>captureMenu.DiagnosticCaptureReady,300,"Gameplay menu repaint (open="+captureMenu.IsOpen+", active="+captureMenu.isActiveAndEnabled+")");yield return new WaitForEndOfFrame();}
          }
        var old=RenderTexture.active;RenderTexture.active=target;var image=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
        image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();RenderTexture.active=old;
        if(MenuHighlightCheck){
            // Only the original course/choice widget strip, excluding countdown,
            // backdrop animation and any moving live car outside this band.
            var pixels=image.GetPixels32();float fit=Mathf.Min(image.width/640f,image.height/480f);
            int x0=Mathf.RoundToInt((image.width-640*fit)*.5f),y0=Mathf.RoundToInt((image.height-480*fit)*.5f);
            ulong hash=14695981039346656037UL;
            unchecked{for(int y=92;y<164;++y)for(int x=32;x<608;++x){var p=pixels[(image.height-1-y0-Mathf.FloorToInt(y*fit))*image.width+x0+Mathf.FloorToInt(x*fit)];hash=(hash^p.r)*1099511628211UL;hash=(hash^p.g)*1099511628211UL;hash=(hash^p.b)*1099511628211UL;}}
            lastHighlightHash=hash;
        }
        int visible=0;foreach(var pixel in image.GetPixels32())if(Math.Max(pixel.r,Math.Max(pixel.g,pixel.b))>24)++visible;
        if(LoadingTransitionCheck&&(name=="loading-race-visible"||name=="loading-countdown-complete")){
            int strip=Mathf.FloorToInt(55*Mathf.Min(image.width/640f,image.height/480f)),lit=0;
            for(int y=0;y<strip-1;++y)for(int x=0;x<image.width;++x){var p=image.GetPixel(x,y);if(Math.Max(p.r,Math.Max(p.g,p.b))>.01f)++lit;}
            Check(name=="loading-race-visible"?lit==0:lit>image.width,"Bottom intro bar covers the frame only before countdown: "+name);
        }
        if(expectedFade==255)Check(visible==0,"Full-black Unity fade covers menu and car "+name);
        else Check(visible>image.width*image.height/20,"Nonblank Unity scene "+name);
        if(expectedFade>=0){
            Check((scene.CurrentFrame.screenFadeArgb>>24)==expectedFade,"Captured native fade alpha "+name);
            foreach(var pixel in image.GetPixels32())
                if(Math.Max(pixel.r,Math.Max(pixel.g,pixel.b))>256-expectedFade)
                    throw new InvalidOperationException("Unity fade did not attenuate every scene pixel: "+name);
            ++checks;
        }
        string file=name+".png";File.WriteAllBytes(Path.Combine(root,file),image.EncodeToPNG());captures.Add(file);
        if(captureMenu!=null)captureMenu.CancelDiagnosticCapture();
        camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();Destroy(image);target.Release();Destroy(target);frozen=false;yield return null;
    }
    private IEnumerator Guard(IEnumerator routine){
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&!finished){object value=null;Exception failure=null;
            try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}
            if(failure!=null){Finish(false,failure.ToString());yield break;}if(value is IEnumerator child)stack.Push(child);else yield return value;
        }
    }
    private void Update(){if(finished)return;if(host.Failure!=null)Finish(false,host.Failure);else if(Time.realtimeSinceStartupAsDouble-began>240)Finish(false,"Mode-flow timeout");}
    private void Finish(bool passed,string error){
        if(finished)return;finished=true;bool stopped=false;
        try{host.StopNative();stopped=!host.Ready;}catch(Exception e){error=(error??"")+e;passed=false;}
        File.WriteAllText(Path.Combine(root,"report.json"),JsonUtility.ToJson(new Report{passed=passed,shutdownComplete=stopped,applicationVersion=Application.version,
            checks=checks,error=error,captures=captures.ToArray(),scope=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-performance-options-check")>=0?
            "Performance options migration, persistence, display rollback, controller navigation, imported foliage LOD and native mirror/weather geometry with unchanged race ticks/car/profile/driving RNG/wet state. Isolated fixtures, not a low-end hardware FPS benchmark.":Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-import-times-check")>=0?
            "Personal import from isolated multi-slot and legacy fixtures, fastest-record deduplication, unknown metadata, exclusion of aggregate-only rows, read-only saves, controller action, live HTTPS uploads, persistent acknowledgments and sharing-off behavior. Disposable remote times require cleanup.":Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-community-check")>=0?
            "Actual native gate finishes on original and imported courses, shared target selection, timeout exclusion, one-shot upload, C# validation/packing, controller settings and live Unity HTTPS registration/upload/snapshot. Controlled gate traversal in isolated saves; not a human driven race. Disposable remote installation requires moderation cleanup.":Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-imported-car-light-check")>=0?
            "Native App lighting sweeps for both imported tracks, both directions, day/night, dry/wet and solo/online. Real race startup and complete path-coordinate coverage; controlled projection near reported crash. No network peer or full driving playthrough. Isolated saves.":Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-full-tune-check")>=0?
            "Gameplay Full Tune action through existing save selection, mandatory upgrade chain and optional purchases/declines; new-driver normal color/transmission/package/name setup; same-save return to mode selection and persistence. Controller inputs use normal native frame routing. Only isolated diagnostic saves changed.":LoadingTransitionCheck?
            "Actual loading artwork fade, two-second full-black hold and immediate race intro through normal selection/loading APIs with fixed 60 Hz diagnostic inputs. Unity pixels validate outgoing fade, full black, visible race without incoming fade, and bottom letterbox removal by driving. Isolated saves; no original timing parity claim beyond the requested two-second hold.":MenuPresentationCheck?
            "Actual menu-owner input transitions and Unity fade-pixel checks for manufacturer/car and continuous Time Attack course/route/weather choices. Optional menu-highlight check samples eight confirmation frames per choice and asserts two distinct widget-strip pixel states. Continue uses an isolated controlled fixture to reach course selection. No ordinary saves or physical devices modified; full original timing parity is a separate native check.":
            "Native App actual finish/timeout, solo and multiplayer finish-music routing, original coaching inputs and per-driver record sequence plus private Unity visual fixtures. Controller input uses the normal frame path. Visual fixtures use controlled source inputs; production analysis uses source race statistics. No ordinary saves or physical devices modified."},true));
        Debug.Log((passed?"PASS":"FAIL")+" mode-flow "+error);Application.Quit(passed?0:1);
    }
}
