using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;

public static class Idas3StorageUpdateChecks
{
    public static void Run()
    {
        string proof=Path.GetFullPath("Verification/storage-optimization/updater-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(proof);File.WriteAllText(Path.Combine(proof,"ISOLATED_UPDATE_TEST.txt"),"Private synthetic install only.");
        string helper=Path.Combine(proof,"installer.exe");File.Copy("Assets/Resources/UpdateInstaller.exe.bytes",helper);
        int checks=0;
        foreach(bool patch in new[]{false,true}) {
            string root=Path.Combine(proof,patch?"patch-game":"full-game"),session=Path.Combine(proof,patch?"patch-session":"full-session");
            Directory.CreateDirectory(root);Directory.CreateDirectory(session);
            var files=new Dictionary<string,byte[]> {
                ["InitialDUnity.exe"]=Encoding.UTF8.GetBytes("isolated synthetic executable"),
                ["UnityPlayer.dll"]=Encoding.UTF8.GetBytes("isolated synthetic runtime"),
                ["InitialDUnity_Data/Managed/Assembly-CSharp.dll"]=Encoding.UTF8.GetBytes("isolated synthetic scripts"),
                ["InitialDUnity_Data/data.unity3d"]=Encoding.UTF8.GetBytes("UnityFS\0isolated synthetic packed data")
            };
            foreach(string name in Idas3CompressedPlayerLayout.RetiredLooseFiles)
                files.Add("InitialDUnity_Data/"+name,Array.Empty<byte>());
            foreach(var pair in files) {
                if(pair.Key.EndsWith("data.unity3d",StringComparison.Ordinal))continue;
                string path=Path.Combine(root,pair.Key);Directory.CreateDirectory(Path.GetDirectoryName(path));
                File.WriteAllBytes(path,pair.Value.Length==0?new byte[1024*1024]:pair.Value);
            }
            string save=Path.Combine(root,"userdata/save.txt");Directory.CreateDirectory(Path.GetDirectoryName(save));File.WriteAllText(save,"fixture progress");
            var records=new List<Idas3UpdateStaging.PatchFile>();
            foreach(var pair in files) {
                using(var sha=SHA256.Create())records.Add(new Idas3UpdateStaging.PatchFile {path=pair.Key,size=pair.Value.Length,
                    sha256=BitConverter.ToString(sha.ComputeHash(pair.Value)).Replace("-","").ToLowerInvariant(),included=true});
            }
            string archive=Path.Combine(session,"game.zip");
            using(var zip=ZipFile.Open(archive,ZipArchiveMode.Create)) {
                foreach(var pair in files)using(var output=zip.CreateEntry(pair.Key).Open())output.Write(pair.Value,0,pair.Value.Length);
                if(patch)using(var output=new StreamWriter(zip.CreateEntry("update-patch.json").Open()))
                    output.Write(JsonUtility.ToJson(new Idas3UpdateStaging.Patch {schema=1,baseVersion="old",targetVersion="new",files=records.ToArray()}));
            }
            string plan=Idas3UpdateStaging.Prepare(root,session,archive,Idas3UpdateStaging.Hash(archive),patch,"old","new",0,0,JsonUtility.FromJson<Idas3UpdateStaging.Patch>);
            var start=new ProcessStartInfo(helper,"\""+plan+"\" --test") {UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden};
            using(var process=Process.Start(start)) {
                if(!process.WaitForExit(30000)||process.ExitCode!=0)throw new Exception("Compressed layout migration failed: "+session);
            }
            foreach(var record in records) {
                string path=Path.Combine(root,record.path);
                if(new FileInfo(path).Length!=record.size||Idas3UpdateStaging.Hash(path)!=record.sha256)throw new Exception("Migration did not replace "+record.path);
                ++checks;
            }
            if(File.ReadAllText(save)!="fixture progress")throw new Exception("Migration touched a save");++checks;
            if(Directory.Exists(Path.Combine(session,"backup"))||Directory.Exists(Path.Combine(session,"stage")))throw new Exception("Migration left payload backups");++checks;
        }
        File.WriteAllText(Path.Combine(proof,"report.txt"),"PASS "+checks+" checks: full and patch migration, retired loose payloads replaced, private save preserved, payload cleanup.\n");
        UnityEngine.Debug.Log("Storage update migration PASS: "+checks+" checks. "+proof);
    }
}
