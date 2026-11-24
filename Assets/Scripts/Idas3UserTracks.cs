using System;
using System.Collections.Generic;
using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using UnityEngine;

// One scanner per native game or replay session. Numeric IDs travel inside existing
// replay and record formats. The full digest on disk guards against aliases.
internal sealed class Idas3UserTracks
{
    [Serializable] sealed class Manifest { public string key="", name="", direction=""; }
    sealed class Pack { public string root, slug, name, digest; public int id, route; }
    static readonly ConcurrentDictionary<int,Pack> catalog = new ConcurrentDictionary<int,Pack>();
    readonly Dictionary<string,Pack> installed = new Dictionary<string,Pack>(StringComparer.OrdinalIgnoreCase);
    readonly Dictionary<string,string> warnings = new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
    readonly HashSet<string> bundled = new HashSet<string>(Idas3CourseCatalog.Packs,StringComparer.OrdinalIgnoreCase);
    static string GameFolder => Path.GetDirectoryName(Application.dataPath);
    public static bool IsUser(int id) => id>=0x10000000 && id<=0x1fffffff;
    public static string Name(int id) => catalog.TryGetValue(id,out var p)?p.name:"Mod track "+id;
    public static string Direction(int id,bool reverse)
    {
        int route=catalog.TryGetValue(id,out var p)?p.route:6;
        return route==8?(reverse?"ib":"ob"):route==1?(reverse?"ccw":"cw"):(reverse?"uh":"dh");
    }
    public static string Folder(int id) => catalog.TryGetValue(id,out var p)?Path.GetFileName(p.root):throw new InvalidDataException("Missing mod track "+id);
    static void RequireFile(string root,string name)
    {
        if(!File.Exists(Path.Combine(root,name)))throw new InvalidDataException("Missing "+name);
    }
    static Pack Read(string root)
    {
        string manifestPath=Path.Combine(root,"track.json");
        var info=new Manifest();
        if(File.Exists(manifestPath)){
            if(new FileInfo(manifestPath).Length>16384)throw new InvalidDataException("track.json is too large");
            info=JsonUtility.FromJson<Manifest>(File.ReadAllText(manifestPath))??throw new InvalidDataException("Invalid track.json");
        }
        var paths=Directory.GetFiles(root,"*_path.bin").Where(f=>!f.EndsWith("_reverse_path.bin",StringComparison.OrdinalIgnoreCase)).ToArray();
        if(paths.Length!=1)throw new InvalidDataException("Expected one main *_path.bin");
        string filename=Path.GetFileName(paths[0]), slug=filename.Substring(0,filename.Length-"_path.bin".Length);
        foreach(string f in new[]{slug+"_path_l.bin",slug+"_path_r.bin",slug+".rcl","race.bin","lamps.bin","menu.idastex","road.bin","scene.bin","scene.json"})RequireFile(root,f);
        string name=string.IsNullOrWhiteSpace(info.name)?Path.GetFileName(root).Replace('_',' '):info.name.Trim();
        if(name.Length>64||name.Any(char.IsControl))throw new InvalidDataException("Invalid track name");
        byte[] digest;
        using(var hash=SHA256.Create()){
            if(string.IsNullOrEmpty(info.key)){
                using(var data=File.OpenRead(paths[0]))digest=hash.ComputeHash(data);
            }else{
                if(!Regex.IsMatch(info.key,@"\A[a-z0-9][a-z0-9._-]{0,95}\z"))throw new InvalidDataException("Invalid track key");
                digest=hash.ComputeHash(Encoding.UTF8.GetBytes("idas3.user-track.v1:"+info.key));
            }
        }
        int id=0x10000000|((digest[0]&15)<<24)|(digest[1]<<16)|(digest[2]<<8)|digest[3];
        int route;
        switch(info.direction??""){
            case "":case "downhill":route=6;break;
            case "outbound":route=8;break;
            case "clockwise":route=1;break;
            default:throw new InvalidDataException("Unknown track direction");
        }
        return new Pack{root=root,slug=slug,name=name,id=id,route=route,digest=BitConverter.ToString(digest).Replace("-","")};
    }
    static void CheckScenery(string root)
    {
        foreach(string f in new[]{"scene.json","scene.bin","road.bin"})RequireFile(root,f);
        var scene=JsonUtility.FromJson<Idas8HakoneCourse.Manifest>(File.ReadAllText(Path.Combine(root,"scene.json")));
        if(scene?.materials==null||scene.instances==null||scene.shapes<=0||scene.pathPoints<2)
            throw new InvalidDataException("Invalid scenery in "+root);
        foreach(var mat in scene.materials){
            if(mat?.textures==null)throw new InvalidDataException("Missing material textures");
            foreach(var texture in mat.textures){
                string file=texture?.file;
                if(string.IsNullOrEmpty(file)||Path.GetFileName(file)!=file)throw new InvalidDataException("Expected a local texture filename");
                RequireFile(root,file);
            }
        }
    }
    static void ReserveId(Pack p)
    {
        string directory=Path.Combine(GameFolder,"UserData","TrackIds");
        Directory.CreateDirectory(directory);
        string target=Path.Combine(directory,p.id+".txt");
        if(!File.Exists(target)){
            string tmp=Path.Combine(directory,Guid.NewGuid().ToString("N")+".tmp");
            try{
                File.WriteAllText(tmp,p.digest);
                try{File.Move(tmp,target);}catch(IOException) when(File.Exists(target)){}
            }finally{if(File.Exists(tmp))File.Delete(tmp);}
        }
        if(File.ReadAllText(target)!=p.digest)throw new InvalidDataException("Track ID collision. The author must choose a different track key.");
    }
    void Warn(string root,string reason)
    {
        if(warnings.TryGetValue(root,out string previous)&&previous==reason)return;
        warnings[root]=reason;
        Debug.LogWarning("Skipping mod track "+Path.GetFileName(root)+": "+reason);
    }
    public int Discover()
    {
#if UNITY_STANDALONE_WIN || UNITY_EDITOR_WIN
        string maps=Path.Combine(GameFolder,"Mods","Maps");
        if(!Directory.Exists(maps))return 0;
        string[] folders;
        try{folders=Directory.GetDirectories(maps);}catch(IOException e){Warn(maps,e.Message);return 0;}catch(UnauthorizedAccessException e){Warn(maps,e.Message);return 0;}
        var pending=new List<Pack>();
        foreach(var root in folders){
            if(bundled.Contains(Path.GetFileName(root))||installed.ContainsKey(root))continue;
            try{pending.Add(Read(root));}catch(Exception e){Warn(root,e.Message);}
        }
        foreach(var group in pending.GroupBy(p=>p.id)){
            foreach(var p in group){
                try{
                    if(group.Count()!=1||installed.Values.Any(other=>other.id==p.id))
                        throw new InvalidDataException("Duplicate track ID. Give distinct tracks different track.json keys.");
                    CheckScenery(p.root);
                    foreach(string variant in new[]{"day_wet","night_dry","night_wet"}){
                        string dir=Path.Combine(p.root,variant);
                        if(Directory.Exists(dir))CheckScenery(dir);
                    }
                    ReserveId(p);
                    if(Idas3Native.Idas3SceneRegisterUserCourse(p.root,p.id,p.slug,p.name.ToUpperInvariant(),p.route)!=1)throw new InvalidDataException(Idas3Native.Error());
                    installed.Add(p.root,p);catalog[p.id]=p;warnings.Remove(p.root);
                    Debug.Log("MOD_TRACK_REGISTERED "+p.id+" "+p.name);
                }catch(Exception e){Warn(p.root,e.Message);}
            }
        }
#endif
        return installed.Count;
    }
}
