using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using UnityEngine;
using Idas3.Multiplayer;

// Explicitly enabled private-save player verification. Inputs traverse the
// production binding poll and music router; captures use the real OnGUI pass.
public sealed class Idas3RaceMusicSmoke : MonoBehaviour
{
    private static string pendingRoot;
    private static Idas3RaceMusicSmoke active;
    private Idas3SceneGame host;
    private string root;
    private KeyCode physicalKey;
    private short physicalThumbX;
    private uint padPulse,padHeld;
    private int pulse,checks;
    private bool finished,ownsPlayer;
    private double started;
    private readonly List<string> captures=new List<string>();
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneGetPreRaceStatus(ref Idas3PreRaceSmoke.PreRaceStatus status);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3SceneGetRaceAudioStatus(ref Idas3PreRaceSmoke.RaceAudioStatus status);
    [Serializable] private class AudioObservation {public uint phase;public int selected,active;public Idas3PreRaceSmoke.RaceAudioStatus audio;}
    [Serializable] private class CountdownReport {public bool passed;public string scope;public int expected;public AudioObservation[] observations;}
    [Serializable] private class Report {public bool passed,shutdownComplete;public int checks,selectedIndex,activeIndex;public string applicationVersion=Application.version,error,scope;public string[] captures;}
    public static bool Configure(ref string saves)
    {
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-race-music-smoke");if(at<0)return false;
        if(at+1>=args.Length)throw new ArgumentException("Music diagnostic needs a new directory.");
        pendingRoot=Path.GetFullPath(args[at+1]);if(Directory.Exists(pendingRoot)||File.Exists(pendingRoot))throw new IOException("Use a fresh music diagnostic directory.");
        Directory.CreateDirectory(pendingRoot);saves=Path.Combine(pendingRoot,"userdata");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(saves,"settings.txt"),"0 0 0 0 0 1 1 0\n");File.WriteAllText(Path.Combine(saves,"native_selection.txt"),"0 0\n");
        int fixture=Array.IndexOf(args,"-idas3-music-return-fixture");
        if(fixture>=0){
            if(fixture+1>=args.Length)throw new ArgumentException("Missing music return fixture");
            string profiles=Path.Combine(saves,"driver_profiles_v1");Directory.CreateDirectory(profiles);
            File.Copy(args[fixture+1],Path.Combine(profiles,"car_00.profile"));
        }
        Screen.SetResolution(1200,720,FullScreenMode.Windowed);AudioListener.volume=0;return true;
    }
    public static void Attach(Idas3SceneGame game)
    {
        if(pendingRoot==null)return;active=game.gameObject.AddComponent<Idas3RaceMusicSmoke>();active.host=game;active.root=pendingRoot;active.ownsPlayer=true;
        active.started=Time.realtimeSinceStartupAsDouble;active.StartCoroutine(active.Guard(active.Run()));
    }
    internal static bool PreparePhysicalInput(ref Func<KeyCode,bool> key,ref Idas3ControlBindings.PadState pad)
    {if(active==null||active.finished)return false;key=active.KeyHeld;pad=new Idas3ControlBindings.PadState{connected=active.physicalThumbX!=0,thumbLX=active.physicalThumbX};return true;}
    private bool KeyHeld(KeyCode key)=>physicalKey!=KeyCode.None&&key==physicalKey;
    internal static bool PrepareFrame(ref Idas3Native.FrameInput frame)
    {if(active==null)return true;if(active.finished)return !active.ownsPlayer;if(active.pulse!=0){frame.SetKey(active.pulse);active.pulse=0;}
        // Reserved menu keys bypass driving bindings in the real host too.
        if(active.physicalKey==KeyCode.Return)frame.SetKey(13);else if(active.physicalKey==KeyCode.Escape)frame.SetKey(27);else if(active.physicalKey==KeyCode.Delete)frame.SetKey(46);
        if((active.padPulse|active.padHeld)!=0){frame.padConnected=1;frame.padButtons=active.padPulse|active.padHeld;active.padPulse=0;}if(active.physicalThumbX!=0){frame.padConnected=1;frame.thumbLX=active.physicalThumbX;}return true;}
    private void Check(bool ok,string reason){++checks;if(!ok)throw new InvalidOperationException(reason);}
    private IEnumerator Frames(int count){for(int i=0;i<count;++i)yield return null;}
    private IEnumerator Until(Func<bool> condition,double seconds,string reason){double end=Time.realtimeSinceStartupAsDouble+seconds;while(!condition()&&Time.realtimeSinceStartupAsDouble<end)yield return null;Check(condition(),reason);}
    private IEnumerator Pulse(int key,int wait=5){pulse=key;yield return Frames(wait);}
    private IEnumerator Delay(double seconds){double end=Time.realtimeSinceStartupAsDouble+seconds;while(Time.realtimeSinceStartupAsDouble<end)yield return null;}
    private IEnumerator Hold(KeyCode key,double seconds){physicalKey=key;double end=Time.realtimeSinceStartupAsDouble+seconds;while(Time.realtimeSinceStartupAsDouble<end)yield return null;physicalKey=KeyCode.None;yield return Frames(5);}
    private IEnumerator PadHorizontal(int direction){physicalThumbX=(short)(direction*32767);yield return Delay(.06);physicalThumbX=0;yield return Frames(5);}
    private IEnumerator Guard(IEnumerator routine)
    {
        var stack=new Stack<IEnumerator>();stack.Push(routine);
        while(stack.Count>0&&!finished){object value=null;Exception failure=null;try{if(!stack.Peek().MoveNext()){stack.Pop();continue;}value=stack.Peek().Current;}catch(Exception e){failure=e;}
            if(failure!=null){Finish(false,failure.ToString());yield break;}if(value is IEnumerator child)stack.Push(child);else yield return value;}
    }
    private void Update(){if(ownsPlayer&&!finished&&(host.Failure!=null||Time.realtimeSinceStartupAsDouble-started>(ReturnCheck?360:210)))Finish(false,host.Failure??"Music diagnostic timeout.");}
    private IEnumerator Run()
    {
        yield return Frames(5);Check(host.Ready,"Player initialized");host.DiagnosticFocusOverride=true;
        CheckRaceCatalog();Check(!host.RaceMusicMenu.HintVisible,"Music hint appeared outside opponent selection");yield return Pulse(116);
        yield return Until(()=>host.Status.racePhase==2&&(host.Status.flags&1025u)==0,45,"Quick start did not reach running");
        Check(!host.RaceMusicMenu.HintVisible,"Music hint appeared during racing");
        Check(Idas3Native.Idas3SceneSetPaused(1)==1&&Idas3Native.Idas3SceneReturnToCourse()==1,"Could not return to course selection");yield return Delay(.6);
        Check(host.Status.frontendStage==6,"Expected source course menu");for(int i=0;i<3;++i){yield return Pulse(37);yield return Delay(.15);}
        Check(host.Status.course==0,"Expected Myogi");yield return Pulse(27);yield return Delay(1.8);Check(host.Status.frontendStage==5,"Expected source mode menu");
        yield return Pulse(13);yield return Delay(3.1);Check(host.Status.frontendStage==6,"Expected Legend course menu");yield return Pulse(13);yield return Delay(.85);
        Check(host.Status.frontendStage==10&&host.RaceMusic.State.opponentEligible!=0,"Expected idle opponent selection");
        Check(host.RaceMusicMenu.HintVisible,"Opponent music hint missing");
        if(ReturnCheck){yield return VerifyShowcaseReturn();Finish(true,null);yield break;}
        yield return Capture("opponent-hint");
        yield return Hold(KeyCode.C,.15);Check(!host.RaceMusicMenu.IsOpen,"Short View Change tap opened picker");
        var bindings=host.ControlBindings;bindings.BeginEdit();Check(bindings.TrySetDraftKey(Idas3ControlBindings.ActionId.Camera,Idas3ControlBindings.Slot.Primary,KeyCode.L),"Could not rebind View Change");
        Check(bindings.ApplyDraft(),"Camera binding save failed: "+bindings.LastError);yield return Frames(6);
        yield return Hold(KeyCode.C,.8);Check(!host.RaceMusicMenu.IsOpen,"Removed Camera key still opened picker");
        yield return Hold(KeyCode.L,.85);Check(host.RaceMusicMenu.IsOpen,"Held mapped Camera did not open picker");
        Check(host.Status.frontendStage==10&&host.RaceMusic.State.opponentEligible!=0,"Picker input confirmed opponent underneath");
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-custom-music-check")>=0){yield return CheckCustomMusic();Finish(true,null);yield break;}
        yield return Capture("opponent-picker");yield return CheckStageFilters();
        int previous=host.RaceMusic.State.activeIndex;
        for(int track=102;track<=116;++track){
            if(!host.RaceMusicMenu.IsOpen){yield return Hold(KeyCode.L,.75);Check(host.RaceMusicMenu.IsOpen,"Special Stage selection could not reopen the normal opponent picker");}
            StageFilter(10);HighlightTrack(track);
            if(track==LongestTrack(10))yield return Capture("opponent-special-stage-long-title");
            SelectTrack(track);yield return Frames(3);host.RaceMusic.Refresh();
            Check(!host.RaceMusicMenu.IsOpen&&host.RaceMusic.State.selectedIndex==track,"Special Stage track did not save from the filtered opponent picker: "+track);
            Check(host.RaceMusic.State.activeIndex==previous,"Picker changed currently playing music");
        }
        yield return Capture("opponent-selected");
        yield return Pulse(13);double wait=Time.realtimeSinceStartupAsDouble+45;
        // Start skips only the native character dialogue; stop injecting as
        // soon as the actual car showcase starts.
        for(int frame=0;ReadPresentation().phase==0&&Time.realtimeSinceStartupAsDouble<wait;++frame){if(frame%12==0)padPulse=0x10;yield return null;}
        Check(ReadPresentation().phase==1,"Selected Special Stage race did not reach the first car showcase");
        yield return CheckCountdownAudio(116,"singleplayer");Finish(true,null);
    }
    private bool ReturnCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-music-return-check")>=0;
    [Serializable] private sealed class CustomSongSnapshot {public string file,title,sourceFile;public int rate,channels,samples;}
    [Serializable] private sealed class CustomLibrarySnapshot {public string selected;public List<CustomSongSnapshot> songs;}
    private CustomLibrarySnapshot ReadCustomLibrary()=>JsonUtility.FromJson<CustomLibrarySnapshot>(File.ReadAllText(Path.Combine(root,"userdata","custom-music","library.json")));
    private int CustomId(string title){var songs=ReadCustomLibrary().songs;int index=songs.FindIndex(song=>song.title==title);Check(index>=0,"Custom song absent from persisted library: "+title);return Idas3CustomRaceMusic.FirstId+index;}
    private CustomSongSnapshot CustomSong(string title){var song=ReadCustomLibrary().songs.Find(item=>item.title==title);Check(song!=null,"Custom song metadata absent: "+title);return song;}
    private static string MusicHash(string path){using(var hash=SHA256.Create())using(var input=File.OpenRead(path))return Convert.ToBase64String(hash.ComputeHash(input));}
    private static void WriteBusyMusicWave(string path,int frequency=997){
        const int rate=16000,frames=4000;using(var output=new BinaryWriter(File.Create(path))){
            output.Write(new[]{(byte)'R',(byte)'I',(byte)'F',(byte)'F'});output.Write(36+frames*2);output.Write(new[]{(byte)'W',(byte)'A',(byte)'V',(byte)'E'});
            output.Write(new[]{(byte)'f',(byte)'m',(byte)'t',(byte)' '});output.Write(16);output.Write((short)1);output.Write((short)1);output.Write(rate);output.Write(rate*2);output.Write((short)2);output.Write((short)16);
            output.Write(new[]{(byte)'d',(byte)'a',(byte)'t',(byte)'a'});output.Write(frames*2);for(int i=0;i<frames;++i)output.Write((short)(5000*Math.Sin(i*2*Math.PI*frequency/rate)));
        }
    }
    private static Dictionary<string,string> MusicFiles(string directory){var files=new Dictionary<string,string>(StringComparer.Ordinal);foreach(string file in Directory.GetFiles(directory,"*",SearchOption.AllDirectories))files.Add(file.Substring(directory.Length),MusicHash(file));return files;}
    private void CheckMusicFiles(string directory,Dictionary<string,string> expected,string reason){var current=MusicFiles(directory);Check(current.Count==expected.Count,reason+" (file count)");foreach(var item in expected)Check(current.TryGetValue(item.Key,out string digest)&&digest==item.Value,reason+" ("+item.Key+")");}
    private IEnumerator MusicIdle(){yield return Until(()=>!host.CustomMusic.Busy,70,"Custom music operation did not finish");yield return Frames(2);Check(!host.RaceMusicMenu.Busy,"Music menu stayed busy after library operation");}
    private IEnumerator RestoreCustomLibrary(int expectedId){
        var restored=host.gameObject.AddComponent<Idas3CustomRaceMusic>();
        try{restored.Initialize(Path.Combine(root,"userdata"),host.RaceMusicMenu,host.RaceMusic,host.CustomMusic.FolderPath);Check(restored.SelectedId==expectedId&&restored.LastError==null,"Custom selection did not survive library reload");}
        finally{Destroy(restored);}
        yield return Frames(2);
    }
    private void CheckCustomGone(CustomSongSnapshot song){
        Check(!File.Exists(Path.Combine(root,"userdata","custom-music",song.file)),"Deleted song's decoded audio remains");
        Check(!string.IsNullOrEmpty(song.sourceFile)&&!File.Exists(Path.Combine(host.CustomMusic.FolderPath,song.sourceFile)),"Deleted managed source remains available to be reimported");
        Check(!ReadCustomLibrary().songs.Exists(item=>item.file==song.file),"Deleted song remains in persisted library");
    }
    private IEnumerator CheckCustomMusic(){
        var menu=host.RaceMusicMenu;var library=host.CustomMusic;yield return MusicIdle();menu.NavigateHorizontal(-1);Check(menu.StageFilter==9&&menu.HighlightedTrackId==Idas3CustomRaceMusic.AddId,"Controller reaches custom import action");
        string musicFolder=Path.GetFullPath(library.FolderPath),expectedFolder=Path.GetFullPath(Path.Combine(root,"Custom Music"));
        Check(string.Equals(musicFolder,expectedFolder,StringComparison.OrdinalIgnoreCase),"Custom music diagnostic did not isolate the folder from real player music");
        Check(Directory.Exists(musicFolder),"Custom Music folder was not created");
        menu.RequestDelete();Check(!menu.DeleteConfirmationOpen&&!library.Delete(Idas3CustomRaceMusic.AddId,0),"Add Music can be deleted");
        StageFilter(3);HighlightTrack(1);menu.RequestDelete();Check(!menu.DeleteConfirmationOpen&&!library.Delete(1,0),"Built-in music can be deleted");menu.ShowCustom();
        int activeTrack=host.RaceMusic.State.activeIndex;
        string fixtureRoot=Path.Combine(Path.GetDirectoryName(root),"fixtures");
        string outside=Path.Combine(root,"external-originals");Directory.CreateDirectory(outside);
        foreach(string ext in new[]{"mp3","ogg"})File.Copy(Path.Combine(fixtureRoot,"Custom test."+ext),Path.Combine(outside,"Imported "+ext.ToUpperInvariant()+"."+ext));
        var originals=MusicFiles(outside);
        string folderSongPath=Path.Combine(musicFolder,"Folder auto.wav");File.Copy(Path.Combine(fixtureRoot,"Custom test.wav"),folderSongPath);string folderSongHash=MusicHash(folderSongPath);
        // Dropped songs are found through the production picker-open refresh.
        menu.Back();yield return Frames(5);yield return Hold(KeyCode.L,.85);Check(menu.IsOpen,"Folder scan could not reopen picker");yield return MusicIdle();menu.ShowCustom();
        Check(menu.VisibleTrackCount==2&&CustomId("Folder auto")==Idas3CustomRaceMusic.FirstId,"Picker opening did not import the dropped WAV");
        Check(MusicHash(folderSongPath)==folderSongHash,"Folder import modified the source audio");
        string decodedFolder=Path.Combine(root,"userdata","custom-music");var folderScanSources=MusicFiles(musicFolder);var folderScanLibrary=MusicFiles(decodedFolder);
        library.RefreshFolder();yield return MusicIdle();Check(menu.VisibleTrackCount==2,"Repeated folder scan duplicated a song");
        CheckMusicFiles(musicFolder,folderScanSources,"Repeated scan changed managed sources");CheckMusicFiles(decodedFolder,folderScanLibrary,"Repeated scan changed the decoded library");
        var beforeReplacement=CustomSong("Folder auto");string previousPcmHash=MusicHash(Path.Combine(decodedFolder,beforeReplacement.file));
        HighlightTrack(CustomId("Folder auto"));menu.Activate();yield return Frames(3);host.RaceMusic.Refresh();
        Check(!menu.IsOpen&&library.SelectedTitle=="Folder auto"&&host.RaceMusic.State.selectedIndex==-2,"Folder song could not be selected before replacement");
        yield return Hold(KeyCode.L,.85);Check(menu.IsOpen,"Selected folder song could not reopen picker");yield return MusicIdle();menu.ShowCustom();
        WriteBusyMusicWave(folderSongPath,631);library.RefreshFolder();yield return MusicIdle();host.RaceMusic.Refresh();
        var replacement=CustomSong("Folder auto");
        Check(menu.VisibleTrackCount==2&&library.SelectedId==Idas3CustomRaceMusic.FirstId&&library.SelectedTitle=="Folder auto"&&host.RaceMusic.State.selectedIndex==-2,"Replacing a selected folder song duplicated it or lost the selection");
        Check(replacement.file!=beforeReplacement.file&&replacement.sourceFile==beforeReplacement.sourceFile&&replacement.rate==16000&&replacement.channels==1&&replacement.samples==4000,"Selected folder replacement did not persist the new PCM metadata");
        Check(MusicHash(Path.Combine(decodedFolder,replacement.file))!=previousPcmHash&&!File.Exists(Path.Combine(decodedFolder,beforeReplacement.file)),"Selected folder replacement retained the old PCM cache");
        Check(MusicHash(Path.Combine(fixtureRoot,"Custom test.wav"))==folderSongHash,"Replacing the managed source modified the external WAV fixture");
        menu.Back();yield return Frames(5);yield return Hold(KeyCode.L,.85);Check(menu.IsOpen,"Replaced folder song could not reopen picker");yield return MusicIdle();menu.ShowCustom();
        Check(menu.VisibleTrackCount==2&&menu.SelectedTrackId==Idas3CustomRaceMusic.FirstId&&library.SelectedTitle=="Folder auto"&&host.RaceMusic.State.selectedIndex==-2,"Reopening the picker lost the replaced song selection");
        foreach(string ext in new[]{"mp3","ogg"}){
            yield return library.ImportFile(Path.Combine(outside,"Imported "+ext.ToUpperInvariant()+"."+ext));yield return MusicIdle();
            Check(library.LastError==null,"Custom import failed: "+ext+" "+library.LastError);
            var song=CustomSong("Imported "+ext.ToUpperInvariant());Check(!string.IsNullOrEmpty(song.sourceFile),"Imported song has no managed source");
            Check(MusicHash(Path.Combine(musicFolder,song.sourceFile))==MusicHash(Path.Combine(outside,"Imported "+ext.ToUpperInvariant()+"."+ext)),"Imported managed copy differs from external original");
        }
        Check(menu.VisibleTrackCount==4,"Custom tab has importer and three local tracks");
        var validSources=MusicFiles(musicFolder);var validLibrary=MusicFiles(decodedFolder);
        yield return library.ImportFile(Path.Combine(fixtureRoot,"invalid.wav"));yield return MusicIdle();
        Check(library.LastError!=null&&menu.VisibleTrackCount==4,"Malformed file changed library");
        CheckMusicFiles(musicFolder,validSources,"Malformed import changed managed sources");CheckMusicFiles(decodedFolder,validLibrary,"Malformed import changed decoded library");
        CheckMusicFiles(outside,originals,"Import modified external original files");
        HighlightTrack(CustomId("Imported OGG"));
        menu.Activate();yield return Frames(3);host.RaceMusic.Refresh();
        Check(!menu.IsOpen&&host.RaceMusic.State.selectedIndex==-2&&library.SelectedTitle=="Imported OGG","Custom selection committed");
        Check(host.RaceMusic.State.activeIndex==activeTrack,"Selecting custom music changed menu audio");
        yield return RestoreCustomLibrary(CustomId("Imported OGG"));
        yield return Hold(KeyCode.L,.85);Check(menu.IsOpen,"Custom selection could not reopen picker");yield return MusicIdle();menu.ShowCustom();
        HighlightTrack(CustomId("Folder auto"));var noSources=MusicFiles(musicFolder);var noLibrary=MusicFiles(decodedFolder);
        int deleteEvents=0;Action<int> onDelete=id=>++deleteEvents;menu.DeleteRequested+=onDelete;
        int highlighted=menu.HighlightedTrackId,stage=menu.StageFilter;
        yield return Hold(KeyCode.Delete,.3);Check(menu.DeleteConfirmationOpen&&!menu.DeleteYesSelected&&menu.DeleteTrackId==highlighted,"Held Delete shortcut did not open exactly one dialog defaulting to No");
        yield return Capture("custom-delete-no");
        menu.Navigate(1);Check(menu.DeleteYesSelected&&menu.HighlightedTrackId==highlighted&&menu.StageFilter==stage,"Delete navigation leaked into the underlying song list");
        menu.NavigateHorizontal(-1);Check(!menu.DeleteYesSelected&&menu.HighlightedTrackId==highlighted&&menu.StageFilter==stage,"Delete horizontal navigation changed the underlying stage");
        yield return Hold(KeyCode.Return,.12);Check(!menu.DeleteConfirmationOpen&&menu.IsOpen&&host.Status.frontendStage==10,"Confirming No closed the picker or confirmed the opponent underneath");
        Check(deleteEvents==0,"No dispatched a deletion");
        padHeld=0x4000;yield return Frames(5);Check(menu.DeleteConfirmationOpen&&!menu.DeleteYesSelected,"Controller X did not open deletion on No");
        menu.NavigateHorizontal(1);Check(menu.DeleteYesSelected,"Delete Yes could not be highlighted");menu.Back();yield return Delay(.2);
        Check(!menu.DeleteConfirmationOpen&&menu.IsOpen,"Back closed the picker instead of cancelling deletion");
        padHeld=0;yield return Frames(5);
        menu.RequestDelete();Check(!menu.DeleteYesSelected,"Reopening deletion retained Yes");yield return Hold(KeyCode.Escape,.12);
        Check(!menu.DeleteConfirmationOpen&&menu.IsOpen&&host.Status.frontendStage==10&&deleteEvents==0,"Escape cancelled through the delete modal into the game");
        CheckMusicFiles(musicFolder,noSources,"Cancelled deletion modified managed sources");CheckMusicFiles(decodedFolder,noLibrary,"Cancelled deletion modified the decoded library");
        menu.RequestDelete();menu.NavigateHorizontal(1);yield return RestoreCustomLibrary(CustomId("Imported OGG"));
        Check(!menu.DeleteConfirmationOpen&&menu.IsOpen&&deleteEvents==0,"Catalog replacement retained a stale deletion target");HighlightTrack(CustomId("Folder auto"));
        // Begin a real asynchronous import, then exercise the same public
        // operations while the decoder owns the library.
        string busySource=Path.Combine(outside,"Busy import.wav");WriteBusyMusicWave(busySource);originals=MusicFiles(outside);
        library.StartCoroutine(library.ImportFile(busySource));Check(library.Busy&&menu.Busy,"Asynchronous import did not block conflicting music actions");
        menu.RequestDelete();menu.Navigate(1);menu.NavigateHorizontal(1);menu.Activate();
        Check(!menu.DeleteConfirmationOpen&&menu.IsOpen&&menu.HighlightedTrackId==highlighted&&menu.StageFilter==stage,"Busy import allowed picker navigation, activation or deletion");
        Check(!library.Delete(highlighted,0),"Busy import permitted deletion of an existing song");
        yield return MusicIdle();Check(menu.VisibleTrackCount==5&&CustomId("Busy import")>=Idas3CustomRaceMusic.FirstId,"Busy-state import did not finish");
        CustomSongSnapshot folderSong=CustomSong("Folder auto"),selectedSong=CustomSong("Imported OGG"),survivor=CustomSong("Imported MP3");
        string survivorPcmHash=MusicHash(Path.Combine(decodedFolder,survivor.file)),survivorSourceHash=MusicHash(Path.Combine(musicFolder,survivor.sourceFile));
        HighlightTrack(CustomId("Folder auto"));menu.RequestDelete();Check(!menu.DeleteYesSelected,"Unselected deletion did not start on No");
        yield return PadHorizontal(1);Check(menu.DeleteYesSelected,"Controller could not explicitly select Yes");yield return Hold(KeyCode.Return,.12);host.RaceMusic.Refresh();
        Check(menu.IsOpen&&!menu.DeleteConfirmationOpen&&deleteEvents==1&&menu.VisibleTrackCount==4,"Explicit Yes did not delete exactly one unselected song");CheckCustomGone(folderSong);
        Check(library.SelectedTitle=="Imported OGG"&&library.SelectedId==CustomId("Imported OGG")&&host.RaceMusic.State.selectedIndex==-2,"Deleting another song lost or misindexed the selected song");
        HighlightTrack(CustomId("Imported OGG"));menu.RequestDelete();Check(!menu.DeleteYesSelected,"Selected-song deletion did not start on No");
        yield return PadHorizontal(1);Check(menu.DeleteYesSelected,"Controller could not confirm selected-song deletion");yield return Capture("custom-delete-yes");yield return Hold(KeyCode.Return,.12);host.RaceMusic.Refresh();
        Check(menu.IsOpen&&!menu.DeleteConfirmationOpen&&deleteEvents==2&&menu.VisibleTrackCount==3,"Selected-song deletion did not remove exactly one song");CheckCustomGone(selectedSong);
        Check(host.RaceMusic.State.selectedIndex==-1&&library.SelectedId==-1&&string.IsNullOrEmpty(ReadCustomLibrary().selected),"Deleting the selected custom song did not persist native game-default music");
        Check(host.RaceMusic.State.activeIndex==activeTrack,"Deleting custom music interrupted current menu audio");
        Check(MusicHash(Path.Combine(decodedFolder,survivor.file))==survivorPcmHash&&MusicHash(Path.Combine(musicFolder,survivor.sourceFile))==survivorSourceHash,"Deleting another song modified the surviving custom track");
        CheckMusicFiles(outside,originals,"Deleting imported songs modified external original files");
        var deletedSources=MusicFiles(musicFolder);var deletedLibrary=MusicFiles(decodedFolder);
        yield return RestoreCustomLibrary(-1);library.RefreshFolder();yield return MusicIdle();
        Check(menu.VisibleTrackCount==3&&host.RaceMusic.State.selectedIndex==-1,"Deleted music returned after reload and folder refresh");CheckCustomGone(folderSong);CheckCustomGone(selectedSong);
        CheckMusicFiles(musicFolder,deletedSources,"Reload or rescan changed surviving managed sources");CheckMusicFiles(decodedFolder,deletedLibrary,"Reload or rescan resurrected deleted music");
        yield return Capture("custom-music-after-delete");menu.DeleteRequested-=onDelete;
        HighlightTrack(CustomId("Imported MP3"));menu.Activate();yield return Frames(3);host.RaceMusic.Refresh();
        Check(!menu.IsOpen&&host.RaceMusic.State.selectedIndex==-2&&library.SelectedTitle=="Imported MP3","Surviving custom song could not be selected after deletion");
        yield return Hold(KeyCode.L,.85);Check(menu.IsOpen,"Surviving custom selection could not reopen picker");yield return MusicIdle();menu.ShowCustom();yield return Capture("custom-music-picker");
        Check(menu.DiagnosticStageLabelsFit,"Ten music tabs overlap");menu.Back();yield return Frames(5);
        yield return Pulse(13);double deadline=Time.realtimeSinceStartupAsDouble+45;
        for(int frame=0;ReadPresentation().phase==0&&Time.realtimeSinceStartupAsDouble<deadline;++frame){if(frame%12==0)padPulse=0x10;yield return null;}
        Check(ReadPresentation().phase==1,"Custom music race did not reach showcase");
        yield return CheckCountdownAudio(-2,"custom");
    }
    private IEnumerator VerifyShowcaseReturn(){
        // Myogi's third rival is Shingo (enemy 2). Both fresh/rematch fixtures
        // leave the first rival unbeaten, so the same two moves select him.
        yield return Pulse(39);yield return Delay(.3);yield return Pulse(39);yield return Delay(.3);
        for(int attempt=0;attempt<3;++attempt){
            yield return Hold(KeyCode.C,.85);Check(host.RaceMusicMenu.IsOpen,"Picker did not open before race "+attempt);
            SelectTrack(1+attempt);yield return Frames(8);
            yield return Pulse(13);double deadline=Time.realtimeSinceStartupAsDouble+75;
            while(ReadPresentation().phase==0&&Time.realtimeSinceStartupAsDouble<deadline){
                if(attempt!=1)padPulse=0x10;
                yield return Frames(12);
            }
            uint target=attempt==0?1u:attempt==1?2u:5u;
            yield return Until(()=>ReadPresentation().phase==target,35,"Race did not reach return phase "+target);
            if(attempt==0)Check(ReadPresentation().opponentId==2,"Return regression did not select Shingo");
            Check(!host.RaceMusicMenu.HintVisible,"Music hint appeared in race/showcase");
            host.PauseMenu.SetOpen(true);yield return Frames(2);
            Check(Idas3Native.Idas3SceneReturnToCourse()==1,"Pause course-return command failed");
            host.PauseMenu.SetOpen(false);yield return Delay(.85);
            Check(host.Status.frontendStage==6&&ReadPresentation().reserved==0,"Showcase retained ownership after returning to course select");
            if(attempt==1){yield return Pulse(39);yield return Delay(.4);Check(host.Status.course==1,"Could not select another course");}
            yield return Pulse(13);yield return Until(()=>host.Status.frontendStage==10&&host.RaceMusic.State.opponentEligible!=0,8,"Music eligibility did not recover after course return");
            Check(host.RaceMusicMenu.HintVisible,"Music hint missing after course return");
            yield return Hold(KeyCode.C,.85);Check(host.RaceMusicMenu.IsOpen,"Held View Change did not reopen music after return");
            SelectTrack(4+attempt);yield return Frames(8);
            Check(host.RaceMusic.State.selectedIndex==4+attempt,"Native music selection remained blocked after return");
            File.AppendAllText(Path.Combine(root,"return-sequence-passed.txt"),"Returned at phase "+target+", hint visible, held View Change opened picker and selection committed; course "+host.Status.course+".\n");
        }
    }
    private void SelectTrack(int track)
    {
        HighlightTrack(track);host.RaceMusicMenu.Activate();
    }
    private void HighlightTrack(int track){var menu=host.RaceMusicMenu;for(int i=0;i<menu.VisibleTrackCount&&menu.HighlightedTrackId!=track;++i)menu.Navigate(1);Check(menu.HighlightedTrackId==track,"Track absent from actual filtered picker list: "+track);}
    private void StageFilter(int stage){var menu=host.RaceMusicMenu;for(int i=0;i<11&&menu.StageFilter!=stage;++i)menu.NavigateHorizontal(1);Check(menu.StageFilter==stage,"Could not select Stage"+stage+" filter");}
    private int LongestTrack(int stage){int id=-1,length=-1;foreach(var e in host.RaceMusic.Entries)if(e.stage==stage&&e.title.Length>length){id=e.id;length=e.title.Length;}return id;}
    private IEnumerator CheckStageFilters(){
        var menu=host.RaceMusicMenu;Check(menu.StageFilter==0&&menu.VisibleTrackCount==118,"All Tracks does not show117 songs and the custom importer");
        yield return Frames(6);yield return PadHorizontal(-1);Check(menu.StageFilter==9,"Controller left did not wrap All Tracks to Custom");
        yield return PadHorizontal(1);Check(menu.StageFilter==0,"Controller right did not wrap Custom to All Tracks");
        int[] starts={0,13,19,1,30,44,58,72,86,0,102},counts={117,6,11,12,14,14,14,14,16,1,15};
        foreach(int stage in new[]{1,2,10,3,4,5,6,7,8}){
            menu.NavigateHorizontal(1);Check(menu.StageFilter==stage,"Special Stage tab order is incorrect");
            Check(menu.VisibleTrackCount==counts[stage],"Stage"+stage+" filter count changed");
            int first=menu.HighlightedTrackId;var ids=new HashSet<int>();
            for(int i=0;i<counts[stage];++i){ids.Add(menu.HighlightedTrackId);menu.Navigate(1);}
            Check(ids.Count==counts[stage]&&menu.HighlightedTrackId==first,"Stage"+stage+" vertical wrap duplicated or omitted a track");
            for(int id=starts[stage];id<starts[stage]+counts[stage];++id)Check(ids.Contains(id),"Stage"+stage+" filter omitted native ID"+id);
            if(stage>=4){HighlightTrack(LongestTrack(stage));yield return Capture("stage"+stage+"-filter");}
        }
    }
    private void CheckRaceCatalog()
    {
        Check(host.RaceMusic.State.count==117&&host.RaceMusic.Entries.Length==117,"Expected117 native songs and116 race choices plus default");
        for(int id=1;id<=116;++id){int stage=id<13?3:id<19?1:id<30?2:id<44?4:id<58?5:id<72?6:id<86?7:id<102?8:10;Check(Array.Exists(host.RaceMusic.Entries,e=>e.id==id&&e.stage==stage),"Stable native song ID absent or stage metadata incorrect: "+id);}
        Check(Array.Exists(host.RaceMusic.Entries,e=>e.id==-1&&e.stage==0),"Game default choice missing");
        Check(!Array.Exists(host.RaceMusic.Entries,e=>e.id==0||e.title.Contains("Gamble")||e.artist.Contains("Gamble")),"Gamble Rumble remains a race option");
        Check(!host.RaceMusic.Select(0,1),"Excluded race track can still be selected");
    }
    internal static IEnumerator VerifyLobby(Idas3SceneGame game,Idas3MultiplayerSession session,string directory,string role)
    {
        var previous=active;var test=game.gameObject.AddComponent<Idas3RaceMusicSmoke>();test.host=game;test.root=directory;Directory.CreateDirectory(directory);active=test;
        var oldFocus=game.DiagnosticFocusOverride;game.DiagnosticFocusOverride=true;
        try{
            test.CheckRaceCatalog();var lobby=game.GetComponent<Idas3MultiplayerMenu>();test.Check(lobby!=null,"Lobby component missing");lobby.SetOpen(true);yield return test.Frames(6);
            test.Check(!game.RaceMusicMenu.HintVisible,"Opponent-only hint leaked into lobby");yield return test.Capture("lobby-music-entry");
            var bindings=game.ControlBindings;bindings.BeginEdit();test.Check(bindings.TrySetDraftKey(Idas3ControlBindings.ActionId.Camera,Idas3ControlBindings.Slot.Primary,KeyCode.L)&&bindings.ApplyDraft(),"Lobby Camera rebind failed");yield return test.Frames(6);
            yield return test.Hold(KeyCode.L,.85);test.Check(game.RaceMusicMenu.IsOpen,"Held Camera did not open lobby picker");yield return test.Capture("lobby-picker");
            yield return test.CheckStageFilters();int selected=session.IsHost?86:101;int previousTrack=game.RaceMusic.State.activeIndex;
            test.StageFilter(8);test.HighlightTrack(selected);yield return test.Capture("lobby-stage8-choice");test.SelectTrack(selected);yield return test.Frames(6);game.RaceMusic.Refresh();
            test.Check(!game.RaceMusicMenu.IsOpen&&game.RaceMusic.State.selectedIndex==selected,"Independent local lobby song did not save");
            test.Check(game.RaceMusic.SelectedTitle==Array.Find(game.RaceMusic.Entries,e=>e.id==selected).title,"Filtered catalog displayed the wrong Stage8 title");
            test.Check(game.RaceMusic.State.activeIndex==previousTrack,"Lobby picker changed current audio");yield return test.Capture("lobby-music-selected");
            test.WriteReport("music-lobby-report.json",true,null,false);lobby.SetOpen(false);yield return test.Frames(6);
        }finally{test.physicalKey=KeyCode.None;test.physicalThumbX=0;test.finished=true;active=previous;game.DiagnosticFocusOverride=oldFocus;Destroy(test);}
    }
    internal static IEnumerator VerifyOnlineRace(Idas3SceneGame game,string directory,string role)
    {
        game.RaceMusic.Refresh();int expected=game.MultiplayerSession.IsHost?86:101;
        if(game.RaceMusic.State.selectedIndex!=expected||game.RaceMusic.State.activeIndex!=expected)throw new InvalidOperationException("Online race did not use this player's independent music choice.");
        var test=game.gameObject.AddComponent<Idas3RaceMusicSmoke>();test.host=game;test.root=directory;
        try{yield return test.CheckCountdownAudio(expected,"online-"+role);
            File.WriteAllText(Path.Combine(directory,"music-online-report.json"),JsonUtility.ToJson(new Report{passed=true,checks=test.checks,selectedIndex=game.RaceMusic.State.selectedIndex,activeIndex=game.RaceMusic.State.activeIndex,scope="Actual Stage8 race track stays local: host86, join101. Source showcase holds the stream; countdown starts it and GO preserves it."},true));
        }finally{test.finished=true;Destroy(test);}
    }
    private Idas3PreRaceSmoke.PreRaceStatus ReadPresentation(){var s=new Idas3PreRaceSmoke.PreRaceStatus{size=(uint)Marshal.SizeOf<Idas3PreRaceSmoke.PreRaceStatus>()};Check(Idas3SceneGetPreRaceStatus(ref s)==1&&s.version==1,"Pre-race status unavailable for music timing");return s;}
    private Idas3PreRaceSmoke.RaceAudioStatus ReadAudio(){var s=new Idas3PreRaceSmoke.RaceAudioStatus{size=(uint)Marshal.SizeOf<Idas3PreRaceSmoke.RaceAudioStatus>()};Check(Idas3SceneGetRaceAudioStatus(ref s)==1&&s.version==1,"Race audio status unavailable");return s;}
    private IEnumerator CheckCountdownAudio(int expected,string context){
        var observations=new List<AudioObservation>();bool held=false,countdown=false,advanced=false,running=false;uint prior=0;
        double deadline=Time.realtimeSinceStartupAsDouble+35;double lastStream=0;
        while(Time.realtimeSinceStartupAsDouble<deadline){
            var presentation=ReadPresentation();var audio=ReadAudio();host.RaceMusic.Refresh();
            Check(host.RaceMusic.State.selectedIndex==expected&&host.RaceMusic.State.activeIndex==expected&&audio.track==expected,"Selected race song changed during race presentation");
            if(presentation.phase!=prior){prior=presentation.phase;observations.Add(new AudioObservation{phase=prior,selected=host.RaceMusic.State.selectedIndex,active=host.RaceMusic.State.activeIndex,audio=audio});}
            if(presentation.phase>=1&&presentation.phase<=3){held=true;Check((audio.flags&1)!=0&&audio.streamFrame==0,"Race music played before countdown");}
            else if(presentation.phase==4){
                // The render that ends VS reports Countdown before the next
                // solver opportunity. Native updateAudioScene deliberately
                // holds the stream until that first source countdown tick.
                if(presentation.ownerTicks==0&&presentation.countdownRemaining==240)
                    Check((audio.flags&1)!=0&&audio.streamFrame==0,"Race song escaped the pre-tick countdown hold");
                else{
                    if(!countdown&&observations.Count>0&&(observations[observations.Count-1].audio.flags&1)!=0)
                        observations.Add(new AudioObservation{phase=4,selected=host.RaceMusic.State.selectedIndex,active=host.RaceMusic.State.activeIndex,audio=audio});
                    countdown=true;Check((audio.flags&1)==0&&(audio.flags&8)!=0,"Countdown did not release the loaded race music");advanced|=audio.streamFrame>0;
                }
            }
            else if(presentation.phase==5){running=true;Check((audio.flags&1)==0&&audio.streamFrame>0,"Selected race music did not continue after GO");lastStream=audio.streamFrame;break;}
            yield return null;
        }
        Check(held&&countdown&&advanced&&running,"Could not observe held showcase, audible countdown and running selected song");
        yield return Delay(.3);var final=ReadAudio();host.RaceMusic.Refresh();
        Check(final.track==expected&&host.RaceMusic.State.activeIndex==expected&&final.streamFrame>lastStream,"Selected race song reset or stopped after GO");
        File.WriteAllText(Path.Combine(root,"music-"+context+"-countdown-report.json"),JsonUtility.ToJson(new CountdownReport{passed=true,expected=expected,observations=observations.ToArray(),scope="Read-only actual native music state: held before countdown, stream advances during countdown and afterGO; IDs stay local."},true));
    }
    private static bool PickerHeaderVisible(Texture2D picture)
    {
        if(picture==null)return false;
        var safe=Screen.safeArea;if(safe.width<=0||safe.height<=0)safe=new Rect(0,0,picture.width,picture.height);
        float scale=Mathf.Min(1.5f,Mathf.Min(safe.width/848f,safe.height/640f));
        float left=safe.x+(safe.width-800*scale)*.5f;
        float top=picture.height-safe.yMax+(safe.height-592*scale)*.5f;
        var pixels=picture.GetPixels32();
        // Sample the wide plain red strip above the title. Check both readback
        // orientations, tightly at that strip; dim lobby panels cannot match.
        for(int offset=3;offset<=7;++offset)for(int flip=0;flip<2;++flip){
            int row=Mathf.RoundToInt(top+offset*scale);if(flip==0)row=picture.height-1-row;
            row=Mathf.Clamp(row,0,picture.height-1);int red=0,total=0;
            for(int logicalX=10;logicalX<790;logicalX+=5){
                int x=Mathf.Clamp(Mathf.RoundToInt(left+logicalX*scale),0,picture.width-1);var color=pixels[row*picture.width+x];++total;
                if(color.r>120&&color.g<100&&color.b<100&&color.r>color.g*2&&color.r>color.b*2)++red;
            }
            if(total>100&&red>total*.85f)return true;
        }
        return false;
    }
    private static bool Nonblank(Texture2D picture)
    {
        if(picture==null)return false;var pixels=picture.GetPixels32();int visible=0;
        foreach(var pixel in pixels)if(pixel.r>30||pixel.g>30||pixel.b>30)++visible;
        return visible>pixels.Length/100;
    }
    private void SavePicture(string name,Texture2D picture)
    {
        if(picture==null)return;string path="music-"+name+".png";
        File.WriteAllBytes(Path.Combine(root,path),picture.EncodeToPNG());captures.Add(path);
    }
    private IEnumerator Capture(string name)
    {
        // Hidden standalone windows do not receive IMGUI repaint events.
        // Keep navigation/playback checks usable without claiming visual QA.
        if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-music-no-capture")>=0){yield return Frames(2);yield break;}
        var music=host.RaceMusicMenu;var lobby=host.GetComponent<Idas3MultiplayerMenu>();
        bool drawMusic=music.IsOpen||music.HintVisible,drawLobby=lobby!=null&&lobby.IsOpen,expectPicker=music.IsOpen;
        int oldAa=QualitySettings.antiAliasing;RenderTexture target=null;Texture2D window=null,picture=null;
        try{
            // Independent evidence of the normal window's real GUI stacking.
            // This never requests either diagnostic RT callback.
            QualitySettings.antiAliasing=0;yield return Frames(5);yield return new WaitForEndOfFrame();
            window=ScreenCapture.CaptureScreenshotAsTexture();SavePicture(name+"-window",window);
            bool windowNonblank=Nonblank(window),windowHeader=PickerHeaderVisible(window);
            var camera=host.GetComponent<Camera>();var scene=host.GetComponent<Idas3SceneRenderer>();var ui=host.GetComponent<Idas3UnityUi>();var previous=camera.targetTexture;
            target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){antiAliasing=1};Check(target.Create(),"Capture target failed");
            try{
                camera.targetTexture=target;scene.ApplyFrame();ui.ApplyFrame();var cameras=new List<Camera>();
                foreach(var item in Resources.FindObjectsOfTypeAll<Camera>())if(item!=null&&item.enabled&&item.gameObject.activeInHierarchy&&item.targetTexture==target)cameras.Add(item);
                cameras.Sort((a,b)=>a.depth.CompareTo(b.depth));foreach(var item in cameras)item.Render();
            }finally{camera.targetTexture=previous;scene.ApplyFrame();ui.ApplyFrame();}
            // Direct RT writes need explicit back-to-front sequencing. GUI.depth
            // is not a synchronization contract between separate callbacks.
            if(drawLobby){lobby.RequestDiagnosticCapture(target);yield return Until(()=>lobby.DiagnosticCaptureReady,5,"Lobby Repaint missing for "+name);}
            if(drawMusic){music.RequestDiagnosticCapture(target);yield return Until(()=>music.DiagnosticCaptureReady,5,"Music Repaint missing for "+name);}
            yield return new WaitForEndOfFrame();
            var old=RenderTexture.active;try{
                RenderTexture.active=target;picture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
                picture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);picture.Apply();
            }finally{RenderTexture.active=old;}
            SavePicture(name,picture);bool targetHeader=PickerHeaderVisible(picture);
            File.WriteAllText(Path.Combine(root,"music-"+name+"-capture.json"),JsonUtility.ToJson(new CaptureReport{
                name=name,windowNonblank=windowNonblank,windowPickerHeader=windowHeader,targetPickerHeader=targetHeader,
                expectedPicker=expectPicker,stageLabelsFit=music.DiagnosticStageLabelsFit,width=Screen.width,height=Screen.height,priorAntialiasing=oldAa},true));
            Check(Nonblank(picture),"Capture was blank: "+name);
            if(expectPicker){
                Check(music.DiagnosticStageLabelsFit,"Stage filter label clips or extends outside the nine-button row: "+name);
                Check(targetHeader,"Actual picker red header missing from sequenced capture: "+name);
                Check(windowNonblank&&windowHeader,"Actual window does not show the picker above the lobby: "+name);
            }
        }finally{
            music.CancelDiagnosticCapture();if(lobby!=null)lobby.CancelDiagnosticCapture();
            if(window!=null)Destroy(window);if(picture!=null)Destroy(picture);
            if(target!=null){target.Release();Destroy(target);}QualitySettings.antiAliasing=oldAa;
        }
    }
    [Serializable] private class CaptureReport
    {
        public string name;public bool windowNonblank,windowPickerHeader,targetPickerHeader,expectedPicker,stageLabelsFit;
        public int width,height,priorAntialiasing;
    }
    private void WriteReport(string file,bool passed,string error,bool stopped)
    {
        var state=host.RaceMusic.State;File.WriteAllText(Path.Combine(root,file),JsonUtility.ToJson(new Report{passed=passed,shutdownComplete=stopped,checks=checks,error=error,captures=captures.ToArray(),selectedIndex=state.selectedIndex,activeIndex=state.activeIndex,
            scope=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-music-no-capture")>=0?"Hidden private-save Unity/native player: production controller/key routing, tab order, song choices and countdown playback checked; screenshots and IMGUI appearance not validated.":ReturnCheck?"Private-save actual Unity/native player: first/second car showcase and running-race returns, held View Change routed through production bindings, picker eligibility/visibility/open and native selection checked; hidden test has no IMGUI screenshot validation.":"Private-save actual Unity/native player. Injected physical key reaches production bindings and held-View-Change router; captures are actual scene and OnGUI, with AA1 readback only."},true));
    }
    private void Finish(bool passed,string error)
    {
        if(finished)return;finished=true;physicalKey=KeyCode.None;physicalThumbX=0;padHeld=0;host.DiagnosticFocusOverride=null;bool stopped=false;
        try{host.StopNative();stopped=!host.Ready;}catch(Exception e){passed=false;error=(error??"")+e;}
        WriteReport("report.json",passed&&stopped,error,stopped);Debug.Log((passed?"PASS":"FAIL")+" race music "+error);
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying=false;
#else
        Application.Quit(passed?0:1);
#endif
    }
}
