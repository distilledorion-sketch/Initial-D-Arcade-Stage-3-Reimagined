using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

// Read-only metadata plus a validated next-race choice. Audio remains in the
// original native stream decoder; the picker never replaces menu/intro music.
internal sealed class Idas3RaceMusicCatalog
{
    [StructLayout(LayoutKind.Sequential, Pack=8)]
    internal struct MusicState
    {
        public uint size,version,opponentEligible,count;
        public int selectedIndex,activeIndex;
        public uint opponentId,reserved;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3SceneGetRaceMusicState(ref MusicState state);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3SceneCopyRaceMusicText(int index,int field,[Out] byte[] target,int capacity);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3SceneGetRaceMusicStage(int index);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    private static extern int Idas3SceneSetRaceMusicTrack(int index,int context);
    internal MusicState State {get;private set;}
    internal Idas3RaceMusicMenu.Entry[] Entries {get;private set;}
    internal string LastError {get;private set;}
    internal void Initialize()
    {
        Refresh();if(State.count<1||State.count>256)throw new InvalidOperationException("Invalid race music catalog size.");
        var entries=new List<Idas3RaceMusicMenu.Entry>();
        entries.Add(new Idas3RaceMusicMenu.Entry{id=-1,title="AUTOMATIC",artist="ARCADE STAGE 3",stage=0});
        for(int i=0;i<State.count;++i){
            // Keep native IDs stable for existing saves and original audio.
            if(ReadText(i,0)=="stage3.01_gamble_rumble")continue;
            entries.Add(new Idas3RaceMusicMenu.Entry{id=i,title=ReadText(i,1),artist=ReadText(i,2),stage=Idas3SceneGetRaceMusicStage(i)});
        }
        Entries=entries.ToArray();
    }
    internal void Refresh()
    {
        var state=new MusicState{size=(uint)Marshal.SizeOf<MusicState>()};
        if(state.size!=32||Idas3SceneGetRaceMusicState(ref state)!=1||state.version!=1)
            throw new InvalidOperationException("Race music state is unavailable. "+Idas3Native.Error());
        State=state;
    }
    internal bool Select(int id,int context)
    {
        LastError=null;
        if(Entries==null||!Array.Exists(Entries,entry=>entry.id==id)||(context!=0&&context!=1)){LastError="Choose a song from this game's catalog.";return false;}
        if(Idas3SceneSetRaceMusicTrack(id,context)!=1){LastError="The race has started or the music choice could not be saved. "+Idas3Native.Error();return false;}
        Refresh();return true;
    }
    internal string SelectedTitle {
        get {
            if(Entries!=null)foreach(var entry in Entries)if(entry.id==State.selectedIndex)return entry.title;
            return "AUTOMATIC";
        }
    }
    private static string ReadText(int index,int field)
    {
        var bytes=new byte[2048];int count=Idas3SceneCopyRaceMusicText(index,field,bytes,bytes.Length);
        if(count<0||count>=bytes.Length)throw new InvalidOperationException("Race music title could not be read.");
        return Encoding.UTF8.GetString(bytes,0,count);
    }
}
