using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEngine;
using Idas3.Multiplayer;

// Isolated catalog/wire-identity checks. Gameplay and rendering use the
// existing -gunsai-ta-smoke / -odawara-ta-smoke player integration checks.
public static class Idas3IdZeroChecks
{
    const BindingFlags Hidden=BindingFlags.Static|BindingFlags.NonPublic;
    static int checks;
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneInitialize([MarshalAs(UnmanagedType.LPUTF8Str)]string assets,[MarshalAs(UnmanagedType.LPUTF8Str)]string saves,int width,int height);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneShutdown();
    static void Check(bool ok,string label){if(!ok)throw new Exception(label);++checks;}
    public static void Run()
    {
        string output=Path.GetFullPath("Verification/odawara-gunsai-20261007");
        string saves=Path.Combine(output,"managed-session");Directory.CreateDirectory(saves);
        Check(Idas3SceneInitialize(Path.GetFullPath("Native"),saves,1280,720)==1,"Private native presence session");
        try{typeof(Idas3CourseCatalog).Assembly.GetType("Idas3DiscordChecks").GetMethod("Run",Hidden)
            .Invoke(null,new object[]{(Action<bool,string>)Check});}
        finally{Idas3SceneShutdown();}
        var session=typeof(Idas3MultiplayerSession);
        foreach(int course in new[]{16,17})foreach(bool reverse in new[]{false,true})foreach(bool wet in new[]{false,true})foreach(bool night in new[]{false,true}){
            using var memory=new MemoryStream();using var writer=new BinaryWriter(memory);
            writer.Write(course);writer.Write(reverse);writer.Write(wet);writer.Write(night);writer.Flush();memory.Position=0;
            using var reader=new BinaryReader(memory);
            var choice=(Idas3RaceChoice)session.GetMethod("ReadChoice",Hidden).Invoke(null,new object[]{reader});
            Check(choice.Equals(new Idas3RaceChoice(course,reverse,wet,night)),"Network course selection changed");
            Check(Idas8HakoneCourse.CourseId(course==16?33554432u:67108864u)==course,"Scene flags misidentify course");
        }
        foreach(string pack in new[]{"GUNSAI","ODAWARA"}){
            string name=pack=="GUNSAI"?"GunsaiFingerprint":"OdawaraFingerprint";
            var method=session.GetMethod(name,Hidden);string folder=Path.Combine(output,"fingerprint-"+pack);
            Directory.CreateDirectory(folder);
            foreach(string file in Directory.GetFiles(Path.Combine("RuntimeAssets",pack))){
                if(!file.EndsWith(".rcl")&&!file.EndsWith("_path.bin")&&!file.EndsWith("_path_l.bin")&&!file.EndsWith("_path_r.bin")&&Path.GetFileName(file)!="course.id"&&Path.GetFileName(file)!="race.bin")continue;
                File.Copy(file,Path.Combine(folder,Path.GetFileName(file)),true);
            }
            File.WriteAllBytes(Path.Combine(folder,"menu.idastex"),new byte[]{1});
            string original=(string)method.Invoke(null,new object[]{folder});
            foreach(string file in Directory.GetFiles(folder)){
                if(Path.GetFileName(file)=="menu.idastex")continue;
                byte[] data=File.ReadAllBytes(file),changed=(byte[])data.Clone();changed[changed.Length-1]^=1;
                try{File.WriteAllBytes(file,changed);Check((string)method.Invoke(null,new object[]{folder})!=original,"Handshake ignored "+Path.GetFileName(file));}
                finally{File.WriteAllBytes(file,data);}
            }
        }
        foreach(string name in new[]{"Idas3Scene","Idas3SceneDirect"}){
            var shader=Resources.Load<Shader>(name);Check(shader!=null&&shader.isSupported,"Unsupported scene shader");
            foreach(var message in ShaderUtil.GetShaderMessages(shader))Check(message.severity.ToString()!="Error",message.message);
        }
        File.WriteAllText(Path.Combine(output,"managed-PASS.txt"),"PASS "+checks+" presence, course selection, direction-specific simulation fingerprints and shader checks.\n");
        Debug.Log("IDZERO_MANAGED_PASS "+checks);
    }
}
