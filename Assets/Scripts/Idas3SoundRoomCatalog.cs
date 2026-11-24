using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

internal static class Idas3SoundRoomCatalog
{
    internal const int FirstId=2000;
    [Serializable] internal sealed class Track
    {
        public string key,title,artist,collection,preview,cover,audio;
        public int id,stage;
        public int loopStart,loopEnd,loopSampleRate;
        public float duration;
        public float[] waveform;
    }
    [Serializable] private sealed class Manifest { public int version; public Track[] tracks; }
    static readonly Dictionary<string,Track> byKey=new Dictionary<string,Track>();
    static readonly Dictionary<int,Track> packaged=new Dictionary<int,Track>();
    static bool loaded;
    internal static string Root=>Path.Combine(Application.streamingAssetsPath,"SoundRoom");
    internal static IEnumerable<Track> Packaged {get {Load();return packaged.Values;}}
    internal static Track Find(int id){Load();return packaged.TryGetValue(id,out var t)?t:null;}
    internal static Track Find(string key){Load();return !string.IsNullOrEmpty(key)&&byKey.TryGetValue(key,out var t)?t:null;}
    internal static string AssetPath(string relative)
    {
        if(string.IsNullOrEmpty(relative)||Path.IsPathRooted(relative))return null;
        var root=Path.GetFullPath(Root)+Path.DirectorySeparatorChar;
        var path=Path.GetFullPath(Path.Combine(root,relative));
        return path.StartsWith(root,StringComparison.OrdinalIgnoreCase)?path:null;
    }
    internal static Idas3RaceMusicMenu.Entry Decorate(Idas3RaceMusicMenu.Entry entry)
    {
        var t=Find(entry.key);
        if (t == null) {
#if IDAS3_PORTABLE_PLAYER
            if (entry.stage == 3) {
                string dir = Path.Combine(Application.streamingAssetsPath, "IDAS3/data/original_audio/streams");
                try { entry.discPreview = Idas3DiscMusicPreview.Open(dir, entry.key); }
                catch (Exception error) when (error is IOException || error is UnauthorizedAccessException) {
                    Debug.LogWarning("Could not open preview for " + entry.title + ": " + error.Message);
                }
                if (entry.discPreview != null) {
                    entry.duration = entry.discPreview.SongDuration;
                    entry.collection = "Arcade Stage 3";
                }
            }
#endif
            return entry;
        }
        entry.collection=t.collection;entry.duration=t.duration;entry.previewPath=AssetPath(t.preview);
        entry.coverPath=AssetPath(t.cover);entry.waveform=t.waveform;return entry;
    }
    internal static Idas3RaceMusicMenu.Entry Entry(Track t)=>Decorate(new Idas3RaceMusicMenu.Entry{
        id=t.id,key=t.key,title=t.title,artist=t.artist,stage=t.stage,collection=t.collection});
    static void Load()
    {
        if(loaded)return;loaded=true;
        try{
            string path=Path.Combine(Root,"catalog.json");if(!File.Exists(path))return;
            if(new FileInfo(path).Length>4*1024*1024)throw new InvalidDataException();
            var data=JsonUtility.FromJson<Manifest>(File.ReadAllText(path));
            if(data==null||data.version!=1||data.tracks==null||data.tracks.Length>512)throw new InvalidDataException();
            foreach(var t in data.tracks){
                if(t==null||string.IsNullOrWhiteSpace(t.key)||string.IsNullOrWhiteSpace(t.title)||
                   t.duration<=0||t.duration>600||!File.Exists(AssetPath(t.preview))||byKey.ContainsKey(t.key))throw new InvalidDataException();
                if(t.id>=FirstId&&(t.stage!=11||!File.Exists(AssetPath(t.audio))||packaged.ContainsKey(t.id)))throw new InvalidDataException();
                if(t.loopSampleRate!=0&&(t.loopSampleRate<8000||t.loopSampleRate>192000||t.loopStart<0||t.loopEnd<=t.loopStart))throw new InvalidDataException("Invalid song loop points");
                byKey.Add(t.key,t);if(t.id>=FirstId)packaged.Add(t.id,t);
            }
        }catch(Exception e){byKey.Clear();packaged.Clear();Debug.LogWarning("Sound Room library could not be loaded: "+e.Message);}
    }
}
