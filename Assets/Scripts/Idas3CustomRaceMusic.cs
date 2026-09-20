using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using UnityEngine;
using UnityEngine.Networking;

// Local library only. Decode once on import; the existing native race mixer
// then owns timing, looping, pause, volume and the finish-music handoff.
internal sealed class Idas3CustomRaceMusic : MonoBehaviour
{
    internal const int AddId=-100,FirstId=1000;
    const int MaxSamples=32*1024*1024;
    [Serializable] internal sealed class Song {public string file,title;public int rate,channels,samples;}
    [Serializable] sealed class Library {public string selected;public List<Song> songs=new List<Song>();}
    Library library=new Library();string folder;Idas3RaceMusicMenu menu;Idas3RaceMusicCatalog catalog;
    internal bool Busy {get;private set;}
    internal string LastError {get;private set;}
    internal int SelectedId {get {int i=library.songs.FindIndex(s=>s.file==library.selected);return catalog.State.selectedIndex==-2&&i>=0?FirstId+i:catalog.State.selectedIndex;}}
    internal string SelectedTitle {get {int i=library.songs.FindIndex(s=>s.file==library.selected);return catalog.State.selectedIndex==-2&&i>=0?library.songs[i].title:catalog.SelectedTitle;}}
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneSetCustomRaceMusic([In] short[] samples,int count,int rate,int channels,int context);
    internal void Initialize(string saveRoot,Idas3RaceMusicMenu view,Idas3RaceMusicCatalog original){
        folder=Path.Combine(saveRoot,"custom-music");menu=view;catalog=original;
        try{
            Directory.CreateDirectory(folder);string path=Path.Combine(folder,"library.json");
            if(File.Exists(path)&&new FileInfo(path).Length<128*1024)library=JsonUtility.FromJson<Library>(File.ReadAllText(path))??new Library();
            if(library.songs==null)library.songs=new List<Song>();
            library.songs.RemoveAll(s=>!Valid(s));if(library.songs.Count>64)library.songs.RemoveRange(64,library.songs.Count-64);
            int selected=library.songs.FindIndex(s=>s.file==library.selected);
            if(selected>=0&&!Select(FirstId+selected,2))Debug.LogWarning("Custom music restore: "+LastError);
        }catch(Exception){library=new Library();LastError="Custom music library could not be opened.";}
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
    internal void ClearSelection(){library.selected=null;try{Save();}catch(Exception){menu.SetNotice("Song selected, but the preference could not be saved.");}}
    internal bool Select(int id,int context){
        LastError=null;int i=id-FirstId;
        try{
            if(i<0||i>=library.songs.Count)throw new InvalidDataException();var song=library.songs[i];
            string path=Path.Combine(folder,song.file);if(!Valid(song)||new FileInfo(path).Length!=song.samples*2L)throw new InvalidDataException();
            byte[] data=File.ReadAllBytes(path);short[] pcm=new short[song.samples];Buffer.BlockCopy(data,0,pcm,0,data.Length);
            if(Idas3SceneSetCustomRaceMusic(pcm,pcm.Length,song.rate,song.channels,context)!=1){LastError="Choose custom music before starting the race.";return false;}
            library.selected=song.file;Save();catalog.Refresh();return true;
        }catch(Exception){LastError="This custom song is unavailable. Import it again or choose another song.";return false;}
    }
    internal void AddMusic(){if(!Busy)StartCoroutine(PickAndImport());}
    IEnumerator PickAndImport(){
        Busy=true;menu.SetNotice("Choose an MP3, OGG or WAV file…");
        string path=null;Exception error=null;int complete=0;
        var thread=new Thread(()=>{try{path=ChooseFile();}catch(Exception e){error=e;}finally{Volatile.Write(ref complete,1);}}){IsBackground=true};
        thread.SetApartmentState(ApartmentState.STA);thread.Start();
        while(Volatile.Read(ref complete)==0)yield return null;
        if(error!=null){menu.SetNotice("The music file picker could not open.");Busy=false;yield break;}
        if(path==null){menu.SetNotice("");Busy=false;yield break;}
        yield return ImportFile(path);Busy=false;
    }
    internal IEnumerator ImportFile(string path){
        LastError=null;AudioType type=AudioType.UNKNOWN;bool valid=false;
        try{
            string ext=Path.GetExtension(path).ToLowerInvariant();type=ext==".mp3"?AudioType.MPEG:ext==".ogg"?AudioType.OGGVORBIS:ext==".wav"?AudioType.WAV:AudioType.UNKNOWN;
            valid=library.songs.Count<64&&type!=AudioType.UNKNOWN&&new FileInfo(path).Length>0&&new FileInfo(path).Length<=100*1024*1024;
        }catch(Exception){}
        if(!valid){LastError="Use an MP3, OGG or WAV up to 100 MB (64 songs maximum).";menu.SetNotice(LastError);yield break;}
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
                var song=new Song{file=Guid.NewGuid().ToString("N")+".pcm",title=title,rate=clip.frequency,channels=clip.channels,samples=(int)count};
                string dest=Path.Combine(folder,song.file);
                try{
                    using(var output=new BinaryWriter(File.Create(dest)))foreach(float value in data){if(float.IsNaN(value)||float.IsInfinity(value))throw new InvalidDataException("Invalid audio samples.");output.Write((short)Mathf.Clamp(Mathf.RoundToInt(value*32767),-32768,32767));}
                    library.songs.Add(song);try{Save();}catch{library.songs.Remove(song);throw;}
                }catch{if(File.Exists(dest))File.Delete(dest);throw;}
                RefreshMenu();menu.ShowCustom();menu.SetNotice("Imported. Select the song to use it in your next race.");
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
    static string ChooseFile(){
        var dialog=new OpenFileName{size=Marshal.SizeOf(typeof(OpenFileName))};dialog.file=Marshal.StringToHGlobalUni(new string('\0',dialog.maxFile));
        try{if(GetOpenFileNameW(dialog))return Marshal.PtrToStringUni(dialog.file);if(CommDlgExtendedError()!=0)throw new IOException();return null;}
        finally{Marshal.FreeHGlobal(dialog.file);}
    }
}
