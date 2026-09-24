using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using UnityEngine;
using UnityEngine.Networking;

// Local library only. Decode once on import; the existing native race mixer
// then owns timing, looping, pause, volume and the finish-music handoff.
public sealed class Idas3CustomRaceMusic : MonoBehaviour
{
    internal const int AddId=-100,FirstId=1000;
    public const string FolderName="Custom Music";
    public const string ReadmeText="CUSTOM MUSIC\n\nPlace MP3, OGG or WAV files in this folder, then open Select BGM > CUSTOM.\nYou can also use ADD MUSIC in the game. DELETE removes the song from this library and folder; files imported from elsewhere are left untouched.\n\nUp to 64 songs. Maximum 100 MB per file, 10 minutes, mono/stereo at up to 48 kHz.\nMusic stays on this PC and is not sent to other players.\n";
    const int MaxSamples=32*1024*1024;
    [Serializable] internal sealed class Song {public string file,title,sourceFile,sourceHash;public int rate,channels,samples;public long sourceBytes,sourceModifiedUtcTicks;}
    [Serializable] sealed class Library {public string selected;public List<Song> songs=new List<Song>();}
    Library library=new Library();string folder;Idas3RaceMusicMenu menu;Idas3RaceMusicCatalog catalog;int pickerContext;
    internal bool Busy {get;private set;}
    internal string LastError {get;private set;}
    internal string FolderPath {get;private set;}
    internal int SelectedId {get {int i=library.songs.FindIndex(s=>s.file==library.selected);return catalog.State.selectedIndex==-2&&i>=0?FirstId+i:catalog.State.selectedIndex;}}
    internal string SelectedTitle {get {int i=library.songs.FindIndex(s=>s.file==library.selected);return catalog.State.selectedIndex==-2&&i>=0?library.songs[i].title:catalog.SelectedTitle;}}
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneSetCustomRaceMusic([In] short[] samples,int count,int rate,int channels,int context);
    internal void Initialize(string saveRoot,Idas3RaceMusicMenu view,Idas3RaceMusicCatalog original,string musicFolder=null){
        folder=Path.Combine(saveRoot,"custom-music");menu=view;catalog=original;
        FolderPath=Path.GetFullPath(musicFolder??Path.Combine(Path.GetDirectoryName(Application.dataPath),FolderName));
        try{
            Directory.CreateDirectory(folder);string path=Path.Combine(folder,"library.json");
            if(File.Exists(path)&&new FileInfo(path).Length<128*1024)library=JsonUtility.FromJson<Library>(File.ReadAllText(path))??new Library();
            if(library.songs==null)library.songs=new List<Song>();
            library.songs.RemoveAll(s=>!Valid(s));if(library.songs.Count>64)library.songs.RemoveRange(64,library.songs.Count-64);
            int selected=library.songs.FindIndex(s=>s.file==library.selected);
            if(selected>=0&&!Select(FirstId+selected,2))Debug.LogWarning("Custom music restore: "+LastError);
        }catch(Exception){library=new Library();LastError="Custom music library could not be opened.";}
        try{EnsureMusicFolder();}catch(Exception){LastError="The Custom Music folder could not be opened.";}
        RefreshMenu();
    }
    bool Valid(Song s)=>s!=null&&Guid.TryParseExact(Path.GetFileNameWithoutExtension(s.file),"N",out _)&&s.file==Path.GetFileName(s.file)&&s.file.EndsWith(".pcm")&&s.samples>0&&s.samples<=MaxSamples&&(s.channels==1||s.channels==2)&&s.samples%s.channels==0&&s.rate>=8000&&s.rate<=48000&&s.samples/s.channels<=s.rate*600&&!string.IsNullOrWhiteSpace(s.title)&&s.title.Length<=100;
    void Save(){
        string dest=Path.Combine(folder,"library.json"),temp=dest+".tmp";
        File.WriteAllText(temp,JsonUtility.ToJson(library),new UTF8Encoding(false));
        if(File.Exists(dest))File.Replace(temp,dest,null);else File.Move(temp,dest);
    }
    void RefreshMenu(){
        var entries=new List<Idas3RaceMusicMenu.Entry>(catalog.Entries);
        entries.Add(new Idas3RaceMusicMenu.Entry{id=AddId,stage=9,title="ADD MUSIC…",artist="Import an MP3, OGG or WAV file from this PC"});
        for(int i=0;i<library.songs.Count;i++)entries.Add(new Idas3RaceMusicMenu.Entry{id=FirstId+i,stage=9,title=library.songs[i].title,artist="Custom music · saved on this PC"});
        menu.ReplaceCatalog(entries.ToArray(),SelectedId);
    }
    void SetBusy(bool value){Busy=value;menu.Busy=value;}
    void Error(string message){LastError=message;menu.SetNotice(message);}
    static bool Supported(string path){string ext=Path.GetExtension(path).ToLowerInvariant();return ext==".mp3"||ext==".ogg"||ext==".wav";}
    static bool PlainFile(string path)=>File.Exists(path)&&(File.GetAttributes(path)&FileAttributes.ReparsePoint)==0;
    static bool SourceName(string name)=>!string.IsNullOrEmpty(name)&&name==Path.GetFileName(name)&&name.IndexOfAny(Path.GetInvalidFileNameChars())<0&&Supported(name);
    static string Hash(string path){using(var file=File.OpenRead(path))using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(file)).Replace("-","").ToLowerInvariant();}
    void EnsureMusicFolder(){
        if(Directory.Exists(FolderPath)&&(File.GetAttributes(FolderPath)&FileAttributes.ReparsePoint)!=0)throw new IOException();
        Directory.CreateDirectory(FolderPath);
        string readme=Path.Combine(FolderPath,"README.txt");
        if(!File.Exists(readme))File.WriteAllText(readme,ReadmeText,new UTF8Encoding(false));
    }
    internal void RefreshFolder(int context=0){if(!Busy){pickerContext=context;StartCoroutine(ScanFolder());}}
    IEnumerator ScanFolder(){
        SetBusy(true);LastError=null;string[] files=null;
        try{EnsureMusicFolder();files=Directory.GetFiles(FolderPath);Array.Sort(files,StringComparer.OrdinalIgnoreCase);}
        catch(Exception){Error("The Custom Music folder could not be opened.");}
        try{
            if(files==null)yield break;
            foreach(string path in files){
                if(!Supported(path))continue;
                bool known=false;
                try{
                    var info=new FileInfo(path);
                    if(!PlainFile(path)||info.Length>100*1024*1024){Error("Use regular music files up to 100 MB.");continue;}
                    var existing=library.songs.Find(s=>string.Equals(s.sourceFile,info.Name,StringComparison.OrdinalIgnoreCase));
                    if(existing!=null&&existing.sourceBytes==info.Length&&existing.sourceModifiedUtcTicks==info.LastWriteTimeUtc.Ticks)continue;
                    if(existing==null&&library.songs.Count>=64){Error("Custom Music holds up to 64 songs.");continue;}
                    string hash=Hash(path);
                    known=existing!=null&&existing.sourceHash==hash;
                    if(known){existing.sourceBytes=info.Length;existing.sourceModifiedUtcTicks=info.LastWriteTimeUtc.Ticks;Save();}
                }
                catch(Exception){Error("A file in Custom Music could not be read.");continue;}
                if(!known)yield return ImportCore(path,false);
            }
        }finally{SetBusy(false);}
    }
    internal bool Delete(int id,int context){
        LastError=null;if(Busy){Error("Wait for the music import to finish.");return false;}
        int index=id-FirstId;
        if(index<0||index>=library.songs.Count||(context!=0&&context!=1)){Error("Choose a custom song to delete.");return false;}
        SetBusy(true);var previous=library;var song=library.songs[index];
        var moved=new List<KeyValuePair<string,string>>();bool reset=false,committed=false;
        try{
            if(!Valid(song)||(File.GetAttributes(folder)&FileAttributes.ReparsePoint)!=0)throw new IOException();
            EnsureMusicFolder();catalog.Refresh();
            if(library.selected==song.file&&catalog.State.selectedIndex==-2){
                if(!catalog.Select(-1,context)){Error(catalog.LastError);return false;}reset=true;
            }
            var paths=new List<string>{Path.Combine(folder,song.file)};
            if(SourceName(song.sourceFile)){
                string source=Path.Combine(FolderPath,song.sourceFile);
                // A replacement dropped under the same name belongs to the
                // next import. Never remove different, unimported content.
                if(PlainFile(source)&&Hash(source)==song.sourceHash)paths.Add(source);
            }
            foreach(string path in paths)if(File.Exists(path)){
                if(!PlainFile(path))throw new IOException();
                string staged=path+".deleted-"+Guid.NewGuid().ToString("N");
                File.Move(path,staged);moved.Add(new KeyValuePair<string,string>(path,staged));
            }
            library=new Library{selected=previous.selected==song.file?null:previous.selected,songs=new List<Song>(previous.songs)};
            library.songs.RemoveAt(index);Save();committed=true;
            foreach(var pair in moved)try{File.Delete(pair.Value);}catch(Exception){Debug.LogWarning("A removed custom music cache file could not be cleaned up.");}
            RefreshMenu();menu.SetNotice("Song deleted.");return true;
        }catch(Exception){
            if(!committed){
                library=previous;
                foreach(var pair in moved)try{if(File.Exists(pair.Value)&&!File.Exists(pair.Key))File.Move(pair.Value,pair.Key);}catch(Exception){}
                if(reset)try{Apply(song,context);catalog.Refresh();}catch(Exception){}
            }
            Error("The song could not be deleted. Check that its files are writable.");return false;
        }finally{SetBusy(false);}
    }
    internal void ClearSelection(){library.selected=null;try{Save();}catch(Exception){menu.SetNotice("Song selected, but the preference could not be saved.");}}
    internal bool Select(int id,int context){
        LastError=null;if(Busy){Error("Wait for the music import to finish.");return false;}int i=id-FirstId;
        try{
            if(i<0||i>=library.songs.Count)throw new InvalidDataException();var song=library.songs[i];
            if(!Apply(song,context)){LastError="Choose custom music before starting the race.";return false;}
            library.selected=song.file;Save();catalog.Refresh();return true;
        }catch(Exception){LastError="This custom song is unavailable. Import it again or choose another song.";return false;}
    }
    bool Apply(Song song,int context){
        string path=Path.Combine(folder,song.file);if(!Valid(song)||!PlainFile(path)||new FileInfo(path).Length!=song.samples*2L)throw new InvalidDataException();
        byte[] data=File.ReadAllBytes(path);short[] pcm=new short[song.samples];Buffer.BlockCopy(data,0,pcm,0,data.Length);
        return Idas3SceneSetCustomRaceMusic(pcm,pcm.Length,song.rate,song.channels,context)==1;
    }
    internal void AddMusic(){if(!Busy)StartCoroutine(PickAndImport());}
    IEnumerator PickAndImport(){
        SetBusy(true);menu.SetNotice("Choose an MP3, OGG or WAV file…");
        string path=null;Exception error=null;int complete=0;
        var thread=new Thread(()=>{try{path=ChooseFile(FolderPath);}catch(Exception e){error=e;}finally{Volatile.Write(ref complete,1);}}){IsBackground=true};
        thread.SetApartmentState(ApartmentState.STA);thread.Start();
        while(Volatile.Read(ref complete)==0)yield return null;
        if(error!=null){menu.SetNotice("The music file picker could not open.");SetBusy(false);yield break;}
        if(path==null){menu.SetNotice("");SetBusy(false);yield break;}
        try{yield return ImportCore(path,true);}finally{SetBusy(false);}
    }
    internal IEnumerator ImportFile(string path){
        if(Busy){Error("Wait for the music import to finish.");yield break;}
        SetBusy(true);try{yield return ImportCore(path,true);}finally{SetBusy(false);}
    }
    IEnumerator ImportCore(string path,bool showCustom){
        LastError=null;AudioType type=AudioType.UNKNOWN;bool valid=false;string hash=null;int replacement=-1;bool managed=false;
        try{
            EnsureMusicFolder();path=Path.GetFullPath(path);
            managed=string.Equals(Path.GetDirectoryName(path),FolderPath,StringComparison.OrdinalIgnoreCase);
            if(managed)replacement=library.songs.FindIndex(s=>string.Equals(s.sourceFile,Path.GetFileName(path),StringComparison.OrdinalIgnoreCase));
            string ext=Path.GetExtension(path).ToLowerInvariant();type=ext==".mp3"?AudioType.MPEG:ext==".ogg"?AudioType.OGGVORBIS:ext==".wav"?AudioType.WAV:AudioType.UNKNOWN;
            valid=(replacement>=0||library.songs.Count<64)&&type!=AudioType.UNKNOWN&&PlainFile(path)&&new FileInfo(path).Length>0&&new FileInfo(path).Length<=100*1024*1024;
            if(valid)hash=Hash(path);
        }catch(Exception){}
        if(!valid){LastError="Use an MP3, OGG or WAV up to 100 MB (64 songs maximum).";menu.SetNotice(LastError);yield break;}
        if(library.songs.Exists(s=>s.sourceHash==hash&&string.Equals(s.sourceFile,Path.GetFileName(path),StringComparison.OrdinalIgnoreCase))){
            if(showCustom)menu.ShowCustom();menu.SetNotice("This song is already in Custom Music.");yield break;
        }
        menu.SetNotice("Importing music…");
        using(var request=UnityWebRequestMultimedia.GetAudioClip(new Uri(Path.GetFullPath(path)).AbsoluteUri,type)){
            request.timeout=60;var handler=(DownloadHandlerAudioClip)request.downloadHandler;handler.streamAudio=false;
            yield return request.SendWebRequest();
            if(request.result!=UnityWebRequest.Result.Success){LastError="This audio file could not be decoded. Try another MP3, OGG or WAV.";menu.SetNotice(LastError);yield break;}
            AudioClip clip=null;
            try{
                clip=DownloadHandlerAudioClip.GetContent(request);
                long count=(long)clip.samples*clip.channels;
                if(clip.channels<1||clip.channels>2||clip.frequency<8000||clip.frequency>48000||clip.length>600||count<1||count>MaxSamples)throw new InvalidDataException("Use mono/stereo audio up to 48 kHz, 10 minutes and 64 MB decoded.");
                float[] data=new float[(int)count];if(!clip.GetData(data,0))throw new InvalidDataException("Audio samples could not be read.");
                string title=Path.GetFileNameWithoutExtension(path);var clean=new StringBuilder();foreach(char c in title)if(!char.IsControl(c)&&clean.Length<100)clean.Append(c);title=clean.ToString().Trim();if(title.Length==0)title="Custom song";
                var song=new Song{file=Guid.NewGuid().ToString("N")+".pcm",title=title,rate=clip.frequency,channels=clip.channels,samples=(int)count,sourceHash=hash};
                string dest=Path.Combine(folder,song.file),copied=null;var previous=library;bool replacedSelection=false;
                try{
                    if(Hash(path)!=hash)throw new InvalidDataException("The music file changed during import. Try again.");
                    string source=path;
                    if(!managed){
                        string stem=Path.GetFileNameWithoutExtension(path),ext=Path.GetExtension(path);source=Path.Combine(FolderPath,Path.GetFileName(path));
                        for(int suffix=2;File.Exists(source)||Directory.Exists(source);++suffix)source=Path.Combine(FolderPath,stem+" ("+suffix+")"+ext);
                        File.Copy(path,source);copied=source;
                        if(Hash(source)!=hash)throw new InvalidDataException("The music file changed during import. Try again.");
                    }
                    song.sourceFile=Path.GetFileName(source);
                    var sourceInfo=new FileInfo(source);song.sourceBytes=sourceInfo.Length;song.sourceModifiedUtcTicks=sourceInfo.LastWriteTimeUtc.Ticks;
                    using(var output=new BinaryWriter(File.Create(dest)))foreach(float value in data){if(float.IsNaN(value)||float.IsInfinity(value))throw new InvalidDataException("Invalid audio samples.");output.Write((short)Mathf.Clamp(Mathf.RoundToInt(value*32767),-32768,32767));}
                    library=new Library{selected=previous.selected,songs=new List<Song>(previous.songs)};
                    if(replacement>=0){library.songs[replacement]=song;if(previous.selected==previous.songs[replacement].file)library.selected=song.file;}
                    else library.songs.Add(song);
                    catalog.Refresh();
                    if(replacement>=0&&previous.selected==previous.songs[replacement].file&&catalog.State.selectedIndex==-2){
                        if(!Apply(song,pickerContext))throw new InvalidDataException("Choose custom music before starting the race.");
                        replacedSelection=true;
                    }
                    Save();
                }catch{
                    library=previous;
                    if(replacedSelection)try{Apply(previous.songs[replacement],pickerContext);catalog.Refresh();}catch(Exception){}
                    if(File.Exists(dest))File.Delete(dest);if(copied!=null&&File.Exists(copied))File.Delete(copied);throw;
                }
                if(replacedSelection)catalog.Refresh();
                if(replacement>=0)try{File.Delete(Path.Combine(folder,previous.songs[replacement].file));}catch(Exception){}
                RefreshMenu();if(showCustom)menu.ShowCustom();menu.SetNotice("Music added.");
            }catch(Exception e){LastError=e is InvalidDataException?e.Message:"Music could not be saved. Check available disk space.";menu.SetNotice(LastError);}
            finally{if(clip!=null)Destroy(clip);}
        }
    }
    [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] sealed class OpenFileName{
        public int size;public IntPtr owner,instance;public string filter="Music (MP3, OGG, WAV)\0*.mp3;*.ogg;*.wav\0\0";
        public IntPtr customFilter;public int maxCustomFilter,filterIndex=1;public IntPtr file;public int maxFile=32768;
        public IntPtr fileTitle;public int maxFileTitle;public string initialDir;public string title="Add custom race music";public int flags=0x00080000|0x00001000|0x00000800|0x00000008;
        public short fileOffset,extension;public string defaultExtension;public IntPtr customData,hook;public string template;public IntPtr reserved;public int reserved2,flagsEx;
    }
    [DllImport("comdlg32.dll",CharSet=CharSet.Unicode)] static extern bool GetOpenFileNameW([In,Out] OpenFileName data);
    [DllImport("comdlg32.dll")] static extern uint CommDlgExtendedError();
    static string ChooseFile(string initialDirectory){
        var dialog=new OpenFileName{size=Marshal.SizeOf(typeof(OpenFileName)),initialDir=initialDirectory};dialog.file=Marshal.StringToHGlobalUni(new string('\0',dialog.maxFile));
        try{if(GetOpenFileNameW(dialog))return Marshal.PtrToStringUni(dialog.file);if(CommDlgExtendedError()!=0)throw new IOException();return null;}
        finally{Marshal.FreeHGlobal(dialog.file);}
    }
}
