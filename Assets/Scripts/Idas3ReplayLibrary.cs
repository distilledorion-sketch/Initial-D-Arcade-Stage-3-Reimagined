using System;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using UnityEngine;

// Personal recordings are local files. This component has no network client.
public sealed class Idas3ReplayLibrary : MonoBehaviour
{
    public static string DefaultFolder => Path.Combine(Application.persistentDataPath, "userdata-unity-scene", "replays");
    public string Folder { get; private set; }
    System.Diagnostics.Process viewerProcess;
    internal bool ViewerOpen { get { try { return viewerProcess!=null&&!viewerProcess.HasExited; } catch { return false; } } }
    Idas3SceneGame host; Idas3PauseMenu menu; double retryAt;
    Task<string> saveJob;byte[] savingJson;double nextPoll;
    [DllImport("Idas3Unity", CallingConvention=CallingConvention.Cdecl)] public static extern int Idas3ReplayRecordingOptions(uint flags);
    [DllImport("Idas3Unity", CallingConvention=CallingConvention.Cdecl)] static extern int Idas3LocalReplayRead(int part, byte[] bytes, int capacity);
    [DllImport("Idas3Unity", CallingConvention=CallingConvention.Cdecl)] static extern void Idas3LocalReplayAck();
    public static uint RecordingFlags(Idas3GameOptions.Values v) => (v.replayTimeAttack?1u:0)|(v.replayOnline?2u:0)|(v.replayLegend?4u:0)|(v.communityTimes?8u:0);
    public void Initialize(Idas3SceneGame owner, Idas3PauseMenu view, string saves)
    {
        host=owner;menu=view;Folder=Path.Combine(saves,"replays");
        try { Directory.CreateDirectory(Folder); } catch(Exception) { menu.ReplayStatus="Replay folder unavailable. Check free disk space and folder permissions."; }
    }
    byte[] ReadPart(int part,int maximum)
    {
        int size=Idas3LocalReplayRead(part,null,0);if(size<0||size>maximum)throw new InvalidDataException("Invalid local recording size.");
        if(size==0)return Array.Empty<byte>();
        var bytes=new byte[size];if(Idas3LocalReplayRead(part,bytes,size)!=size)throw new IOException("Local recording changed.");return bytes;
    }
    void Update()
    {
        if(host==null||!host.Ready||Time.realtimeSinceStartupAsDouble<retryAt)return;
        try
        {
            if(saveJob!=null){
                if(!saveJob.IsCompleted)return;
                var completed=saveJob;saveJob=null;string path=completed.GetAwaiter().GetResult();
                // A newer finished race can replace the native pending slot.
                if(ReadPart(0,8192).SequenceEqual(savingJson))Idas3LocalReplayAck();
                savingJson=null;menu.ReplayStatus="Saved: "+Path.GetFileName(path);
            }
            if(Time.realtimeSinceStartupAsDouble<nextPoll)return;
            nextPoll=Time.realtimeSinceStartupAsDouble+.1;
            var json=ReadPart(0,8192);if(json.Length==0)return;
            var metadata=JsonUtility.FromJson<Idas3ReplayData.Details>(Encoding.UTF8.GetString(json));
            savingJson=json;saveJob=SaveAsync(Folder,metadata,ReadPart(1,Idas3ReplayCodec.MaxRaw),ReadPart(2,Idas3ReplayCodec.MaxRaw));
            menu.ReplayStatus="Saving replay…";
        }
        catch(Exception e){retryAt=Time.realtimeSinceStartupAsDouble+10;menu.ReplayStatus="Could not save replay. Check free disk space. Recording remains pending.";Debug.LogWarning("Local replay save: "+e.GetType().Name);}
    }
    public static string Save(string folder,Idas3ReplayData.Details metadata,byte[] player,byte[] opponent)
    {
        metadata.build=Application.version;metadata.id=Guid.NewGuid().ToString("D");
        return SavePrepared(folder,metadata,player,opponent);
    }
    public static Task<string> SaveAsync(string folder,Idas3ReplayData.Details metadata,byte[] player,byte[] opponent)
    {
        metadata.build=Application.version;metadata.id=Guid.NewGuid().ToString("D");
        return Task.Run(()=>SavePrepared(folder,metadata,player,opponent));
    }
    void OnApplicationQuit(){
        // Finish the one owned recording before process exit, without native
        // or scene access on the worker. Failures leave native data unacknowledged.
        if(saveJob!=null)try{saveJob.GetAwaiter().GetResult();}catch(Exception){}
    }
    static string SavePrepared(string folder,Idas3ReplayData.Details metadata,byte[] player,byte[] opponent)
    {
        byte[] a=Idas3ReplayCodec.Encode(player),b=opponent.Length==0?Array.Empty<byte>():Idas3ReplayCodec.Encode(opponent);
        metadata.opponentBytes=b.Length;var packet=Package(metadata,a,b);Idas3ReplayData.Parse(packet);
        Directory.CreateDirectory(folder);
        string stem=FileStem(metadata),temporary=Path.Combine(folder,metadata.id+".tmp");
        File.WriteAllBytes(temporary,packet);
        try{
            for(int n=1;;n++){
                string target=Path.Combine(folder,stem+(n==1?"":"_"+n)+".idreplay");
                try{File.Move(temporary,target);return target;}
                catch(IOException) when(File.Exists(target)){ /* Keep repeated races without overwriting. */ }
            }
        }finally{if(File.Exists(temporary))File.Delete(temporary);}
    }
    public static string FileStem(Idas3ReplayData.Details m)
    {
        // Same direction semantics as the game's original start banner.
        string[] forward={"ccw","ccw","dh","dh","ob","dh","ob","ob","dh","dh","dh","dh"};
        string[] reverse={"cw","cw","uh","uh","ib","rev","ib","ib","uh","uh","uh","uh"};
        int course=m.condition/2;
        string player=m.playerName;
        if(string.IsNullOrWhiteSpace(player)&&m.nameGlyphs!=null){var name=new StringBuilder();foreach(int g in m.nameGlyphs)if(g>=162&&g<=187)name.Append((char)('A'+g-162));else if(g>=188&&g<=197)name.Append((char)('0'+g-188));else if(g==220)name.Append(' ');player=name.ToString();}
        return FileToken(player,"driver")+(m.mode==0?"":"_vs_"+FileToken(m.opponentName,"opponent"))+"_"+
            FileToken(Idas3ReplayData.Courses[course],"course")+"_"+((m.condition&1)==0?forward[course]:reverse[course])+"_"+
            (m.night==1?"night":"day")+"_"+(m.weather==1?"wet":"dry")+"_"+(m.mode==1?"ol":m.mode==2?"lots":"tat");
    }
    static string FileToken(string value,string fallback)
    {
        var token=new StringBuilder();
        foreach(char c in (value??"").Normalize(NormalizationForm.FormKC).ToLowerInvariant()){
            if(token.Length>=32)break;
            if(char.IsLetterOrDigit(c))token.Append(c);
            else if(token.Length>0&&token[token.Length-1]!='_')token.Append('_');
        }
        string result=token.ToString().Trim('_');return result.Length==0?fallback:result;
    }
    public static byte[] Package(Idas3ReplayData.Details metadata,byte[] player,byte[] opponent)
    {
        var json=Encoding.UTF8.GetBytes(JsonUtility.ToJson(metadata));if(json.Length>8192)throw new InvalidDataException("Replay metadata too large.");
        using var output=new MemoryStream();using var writer=new BinaryWriter(output);
        writer.Write(json.Length);writer.Write(json);writer.Write(player);writer.Write(opponent);return output.ToArray();
    }
    public static string[] Files(string folder) => Directory.Exists(folder)?new DirectoryInfo(folder).GetFiles("*.idreplay").OrderByDescending(f=>f.LastWriteTimeUtc).Select(f=>f.FullName).ToArray():Array.Empty<string>();
    public void OpenViewer()
    {
        try
        {
            if(ViewerOpen)return;
            viewerProcess?.Dispose();viewerProcess=null;
            Directory.CreateDirectory(Folder);
            string executable=Path.GetFullPath(Path.Combine(Application.dataPath,"../InitialDUnity.exe"));
            if(Application.isEditor)throw new InvalidOperationException("Open the replay viewer from a built game.");
            Idas3DiscordPresence.YieldToReplay();
            viewerProcess=System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo(executable,"-idas3-replay-viewer -idas3-replay-library \""+Folder+"\""){UseShellExecute=false,WorkingDirectory=Path.GetDirectoryName(executable)});
        }
        catch(Exception e){menu.ReplayStatus="Could not open replay viewer: "+e.Message;}
    }
}
