using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

// Archive preparation runs inside the game's existing Mono runtime. The small
// native helper only verifies, swaps and restarts; no installed shell or .NET is needed.
public static class Idas3UpdateStaging {
    [Serializable] public sealed class Patch {public int schema;public string baseVersion,targetVersion;public PatchFile[] files;}
    [Serializable] public sealed class PatchFile {public string path,sha256;public long size;public bool included;}
    public sealed class PatchRejectedException : IOException {public PatchRejectedException(string message):base(message){}}
    private sealed class Record {public string path,oldHash,newHash;public long size;public bool changed;}
    private static readonly string[] Required={"InitialDUnity.exe","UnityPlayer.dll","InitialDUnity_Data/globalgamemanagers","InitialDUnity_Data/Managed/Assembly-CSharp.dll"};
    private static readonly HashSet<string> Roots=new HashSet<string>(new[]{"InitialDUnity_Data","MonoBleedingEdge","D3D12"},StringComparer.OrdinalIgnoreCase);
    private static readonly HashSet<string> Top=new HashSet<string>(new[]{"InitialDUnity.exe","UnityPlayer.dll","UnityCrashHandler64.exe","dstorage.dll","dstoragecore.dll","steam_appid.txt","READ ME.txt","Replay Viewer.cmd","MULTIPLAYER TEST.txt"},StringComparer.OrdinalIgnoreCase);
    private static readonly HashSet<string> Private=new HashSet<string>(new[]{"userdata","userdata-unity-scene","community-times","replays","custom-music","ADMIN-ACCESS.txt","identity.json","game-options.json","deploy.private.json","library.json","pending.json"},StringComparer.OrdinalIgnoreCase);
    public static string SafeName(string value){
        if(string.IsNullOrEmpty(value)||value.Length>220)throw new IOException("Invalid update path.");
        string name=value.Replace('\\','/');var parts=name.Split('/');
        foreach(string part in parts)if(part.Length==0||part=="."||part==".."||part.EndsWith(".")||part.EndsWith(" ")||Private.Contains(part)||Regex.IsMatch(part,@"[<>:""|?*\x00-\x1f]")||Regex.IsMatch(part,@"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)",RegexOptions.IgnoreCase))throw new IOException("Unsafe update path.");
        if(parts.Length>1?!Roots.Contains(parts[0]):!Top.Contains(name))throw new IOException("Unexpected file in game update.");
        return name;
    }
    public static string Inside(string root,string relative){
        string path=Path.GetFullPath(Path.Combine(root,relative));
        if(!path.StartsWith(Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new IOException("Update path escaped its folder.");
        return path;
    }
    public static void NoLinks(string path){
        for(string p=Path.GetFullPath(path);!string.IsNullOrEmpty(p);p=Path.GetDirectoryName(p)){
            if(string.Equals(p.TrimEnd(Path.DirectorySeparatorChar),Path.GetPathRoot(p).TrimEnd(Path.DirectorySeparatorChar),StringComparison.OrdinalIgnoreCase))break;
            if((File.Exists(p)||Directory.Exists(p))&&(File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0)throw new IOException("Update folders cannot contain links.");
        }
    }
    public static string Hash(string path){using(var input=File.OpenRead(path))using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(input)).Replace("-","").ToLowerInvariant();}
    private static void PutString(BinaryWriter writer,string value){writer.Write(value.Length);writer.Write(Encoding.Unicode.GetBytes(value));}
    private static byte[] Digest(string value){var bytes=new byte[32];if(value!=null)for(int i=0;i<32;i++)bytes[i]=Convert.ToByte(value.Substring(i*2,2),16);return bytes;}
    public static string Prepare(string root,string session,string archive,string digest,bool patch,string baseVersion,string targetVersion,int parentId,long parentFileTime,Func<string,Patch> parse,Action<int,int> progress=null){
        root=Path.GetFullPath(root);session=Path.GetFullPath(session);NoLinks(root);NoLinks(session);
        if(!File.Exists(Path.Combine(root,"InitialDUnity.exe")))throw new IOException("Game executable is missing.");
        if(!string.Equals(Path.GetFullPath(archive),Inside(session,"game.zip"),StringComparison.OrdinalIgnoreCase)||!Regex.IsMatch(digest??"",@"\A[0-9a-f]{64}\z")||Hash(archive)!=digest)throw new IOException("Update archive verification failed.");
        string stage=Inside(session,"stage"),backup=Inside(session,"backup");
        if(Directory.Exists(stage)||Directory.Exists(backup))throw new IOException("Update session already used.");
        var records=new List<Record>();var inventory=new Dictionary<string,PatchFile>(StringComparer.OrdinalIgnoreCase);
        using(var zip=ZipFile.OpenRead(archive)){
            if(patch){
                try{
                    ZipArchiveEntry manifest=null;foreach(var entry in zip.Entries)if(entry.FullName=="update-patch.json"){if(manifest!=null)throw new IOException("Duplicate manifest.");manifest=entry;}
                    if(manifest==null||manifest.Length>16*1024*1024)throw new IOException("Patch manifest missing or too large.");
                    Patch data;using(var reader=new StreamReader(manifest.Open()))data=parse(reader.ReadToEnd());
                    if(data==null||data.schema!=1||data.baseVersion!=baseVersion||data.targetVersion!=targetVersion||data.files==null||data.files.Length==0||data.files.Length>100000)throw new IOException("Patch version or inventory mismatch.");
                    foreach(var file in data.files){
                        if(file==null||file.size<0||file.size>24L*1024*1024*1024||!Regex.IsMatch(file.sha256??"",@"\A[0-9a-f]{64}\z"))throw new IOException("Invalid patch inventory.");
                        file.path=SafeName(file.path);inventory.Add(file.path,file);
                    }
                }catch(Exception e){throw new PatchRejectedException("Invalid patch: "+e.Message);}
            }
            var entries=new Dictionary<string,ZipArchiveEntry>(StringComparer.OrdinalIgnoreCase);long expanded=0;
            foreach(var entry in zip.Entries){
                if(patch&&entry.FullName=="update-patch.json")continue;
                string name=SafeName(entry.FullName.TrimEnd('/'));
                if((entry.ExternalAttributes&0x400)!=0||((entry.ExternalAttributes>>16)&0xf000)==0xa000)throw new IOException("Links are not allowed in update archives.");
                NoLinks(Inside(root,name));if(entry.Name.Length==0)continue;
                if(entries.ContainsKey(name))throw new IOException("Duplicate update path.");entries.Add(name,entry);
                expanded=checked(expanded+entry.Length);if(expanded>24L*1024*1024*1024)throw new IOException("Update archive is too large.");
                if(patch&&(!inventory.ContainsKey(name)||!inventory[name].included||inventory[name].size!=entry.Length))throw new PatchRejectedException("Unexpected patch payload.");
            }
            foreach(string required in Required)if(patch?!inventory.ContainsKey(required):!entries.ContainsKey(required))throw new IOException("Incomplete game package.");
            int completed=0,total=patch?inventory.Count:entries.Count;if(progress!=null)progress(completed,total);
            if(patch)foreach(var file in inventory.Values){
                if(file.included){if(!entries.ContainsKey(file.path))throw new PatchRejectedException("Missing patch payload.");continue;}
                string dest=Inside(root,file.path);NoLinks(dest);
                if(!File.Exists(dest)||new FileInfo(dest).Length!=file.size||Hash(dest)!=file.sha256)throw new PatchRejectedException("Existing files need a full download.");
                records.Add(new Record{path=file.path,oldHash=file.sha256,newHash=file.sha256,size=file.size});
                if(progress!=null)progress(++completed,total);
            }
            long existing=0;foreach(string name in entries.Keys){string path=Inside(root,name);if(File.Exists(path))existing=checked(existing+new FileInfo(path).Length);}
            var tempDrive=new DriveInfo(Path.GetPathRoot(session));var targetDrive=new DriveInfo(Path.GetPathRoot(root));
            long reserve=64L*1024*1024;
            if(tempDrive.AvailableFreeSpace<expanded+existing+reserve+(tempDrive.Name==targetDrive.Name?expanded:0)||targetDrive.AvailableFreeSpace<expanded+reserve)throw new IOException("Not enough free disk space for the update.");
            Directory.CreateDirectory(stage);Directory.CreateDirectory(backup);
            foreach(var pair in entries){
                string name=pair.Key,dest=Inside(root,name),staged=Inside(stage,name);Directory.CreateDirectory(Path.GetDirectoryName(staged));
                using(var input=pair.Value.Open())using(var output=new FileStream(staged,FileMode.CreateNew,FileAccess.Write))input.CopyTo(output);
                if(new FileInfo(staged).Length!=pair.Value.Length)throw new IOException("Extracted size mismatch.");
                string next=Hash(staged),old=File.Exists(dest)?Hash(dest):null;
                if(patch&&next!=inventory[name].sha256)throw new PatchRejectedException("Patch payload hash mismatch.");
                records.Add(new Record{path=name,oldHash=old,newHash=next,size=pair.Value.Length,changed=old!=next});
                if(progress!=null)progress(++completed,total);
            }
        }
        string plan=Inside(session,"install.plan");
        using(var writer=new BinaryWriter(new FileStream(plan,FileMode.CreateNew,FileAccess.Write))){
            writer.Write(Encoding.ASCII.GetBytes("IDUPD002"));PutString(writer,root);writer.Write(parentId);writer.Write(parentFileTime);writer.Write(records.Count);
            foreach(var r in records){PutString(writer,r.path);writer.Write((byte)(r.changed?1:0));writer.Write((byte)(r.oldHash!=null?1:0));writer.Write(r.size);writer.Write(Digest(r.oldHash));writer.Write(Digest(r.newHash));}
        }
        return plan;
    }
}
