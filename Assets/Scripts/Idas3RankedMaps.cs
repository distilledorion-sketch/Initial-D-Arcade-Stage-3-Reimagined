using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using UnityEngine;

// Verify against the game's inventory, never against an identity supplied by a mod.
internal sealed class Idas3RankedMaps
{
    internal const int ArcadeMask=(1<<9)-1;
    internal static int AvailableMask {get;private set;}=ArcadeMask;
    int installed=ArcadeMask;
    readonly Dictionary<int,string> warnings=new Dictionary<int,string>();
    static Dictionary<string,List<Entry>> inventory;
    sealed class Entry {public string path,hash;public long size;}
    static Dictionary<string,List<Entry>> Inventory()
    {
        if(inventory!=null)return inventory;
        var result=new Dictionary<string,List<Entry>>(StringComparer.OrdinalIgnoreCase);
        using(var reader=new StringReader(Resources.Load<TextAsset>("RankedMaps").text)){
            string line;
            while((line=reader.ReadLine())!=null){
                var fields=line.Split('\t');int slash=fields[0].IndexOf('/');
                string pack=fields[0].Substring(0,slash);
                if(!result.TryGetValue(pack,out var rows))result.Add(pack,rows=new List<Entry>());
                rows.Add(new Entry{path=fields[0].Substring(slash+1),size=long.Parse(fields[1]),hash=fields[2]});
            }
        }
        return inventory=result;
    }
    static bool SimulationFile(string name)
    {
        return name.EndsWith(".rcl",StringComparison.Ordinal)||name.EndsWith("_path.bin",StringComparison.Ordinal)||
            name.EndsWith("_path_l.bin",StringComparison.Ordinal)||name.EndsWith("_path_r.bin",StringComparison.Ordinal)||
            name=="course.id"||name=="race.bin"||name=="timer-scale.bin"||name=="race-markers.bin";
    }
    internal static void Verify(int course,string folder)
    {
        // The native loader prefers these subdirectories over the verified route files.
        if(Directory.Exists(Path.Combine(folder,"HOSTFS"))||Directory.Exists(Path.Combine(folder,"path")))
            throw new InvalidDataException("Unexpected alternate route directory");
        var simulation=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        using(var hash=SHA256.Create()){
            foreach(var entry in Inventory()[Idas3CourseCatalog.Packs[course-9]]){
                bool drives=SimulationFile(entry.path);
                if(!drives&&(entry.path.EndsWith(".faces")||entry.path=="driving-import.json"))continue;
                var file=new FileInfo(Path.Combine(folder,entry.path));
                if(!file.Exists||file.Length==0)throw new InvalidDataException("Missing "+entry.path);
                if(!drives)continue;
                simulation.Add(entry.path);
                if(file.Length==entry.size){
                    using(var stream=file.OpenRead()){
                        if(string.Equals(BitConverter.ToString(hash.ComputeHash(stream)).Replace("-",""),entry.hash,StringComparison.OrdinalIgnoreCase))continue;
                    }
                }
                throw new InvalidDataException("Changed race data in "+entry.path);
            }
        }
        foreach(string file in Directory.GetFiles(folder)){
            string name=Path.GetFileName(file).ToLowerInvariant();
            if(SimulationFile(name)&&!simulation.Contains(name))throw new InvalidDataException("Unexpected race data in "+name);
        }
    }
    public int Discover(bool gameplay)
    {
        for(int id=9;id<Idas3CourseCatalog.Count;id++){
            if((installed&(1<<id))!=0)continue;
            string folder=Idas3CourseCatalog.PackPath(Idas3CourseCatalog.Packs[id-9]);
            if(!File.Exists(Path.Combine(folder,"menu.idastex")))continue;
            try{
                Verify(id,folder);
                if(Idas3Native.Idas3SceneRegisterImportedCourse(folder)!=1)throw new InvalidDataException(Idas3Native.Error());
                installed|=1<<id;warnings.Remove(id);
            }catch(Exception e){
                if(!warnings.TryGetValue(id,out string last)||last!=e.Message)Debug.LogWarning("Skipping optional track "+folder+": "+e.Message);
                warnings[id]=e.Message;
            }
        }
        if(gameplay)AvailableMask=installed;
        return installed;
    }
}
