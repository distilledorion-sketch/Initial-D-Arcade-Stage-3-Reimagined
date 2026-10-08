using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEngine;

public static class Idas3OdawaraRecordChecks
{
    static string output="Verification/odawara-records-directions";
    public static void RunGunsai(){output="Verification/gunsai-records-20261008";Run();}
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneInitialize([MarshalAs(UnmanagedType.LPUTF8Str)]string assets,[MarshalAs(UnmanagedType.LPUTF8Str)]string saves,int width,int height);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneShutdown();
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneModeFlowFixture(int scene);
    public static void Run()
    {
        int checks=0;Action<bool,string> check=(ok,why)=>{++checks;if(!ok)throw new Exception(why);};
        string isolated=Path.GetFullPath(Path.Combine(output,"session-"+Guid.NewGuid().ToString("N")));
        Directory.CreateDirectory(isolated);
        File.WriteAllText(Path.Combine(isolated,"ISOLATED_MODE_FLOW_TEST.txt"),"Private record migration test");
        check(Idas3SceneInitialize(Path.GetFullPath("Native"),Path.Combine(isolated,"userdata"),640,480)==1,"Native fixture startup");
        try{
        check(Idas3SceneModeFlowFixture(-6)==1,"App save-selection/record migration fixture");
        var assembly=typeof(Idas3CourseCatalog).Assembly;
        assembly.GetType("Idas3DiscordChecks").GetMethod("Run",BindingFlags.NonPublic|BindingFlags.Static).Invoke(null,new object[]{check});
        var direction=assembly.GetType("Idas3.Multiplayer.Idas3MultiplayerMenu") ?? assembly.GetType("Idas3MultiplayerMenu");
        if(direction==null)foreach(var type in assembly.GetTypes())if(type.Name=="Idas3MultiplayerMenu"){direction=type;break;}
        check(direction!=null,"Online menu type");
        var label=direction.GetMethod("Direction",BindingFlags.NonPublic|BindingFlags.Static);
        foreach(bool reverse in new[]{false,true}){
            string expected=reverse?"COUNTERCLOCKWISE":"CLOCKWISE";
            check(Idas3CourseCatalog.DirectionLabel(17,reverse)==expected,"Odawara catalog label");
            check((string)label.Invoke(null,new object[]{17,reverse})==expected,"Online menu must use corrected route name");
            check(Idas3CourseCatalog.DirectionToken(17,reverse)==(reverse?"ccw":"cw"),"Replay filename direction");
        }
        int prior=Idas3MenuLocalization.Language;
        try{
            foreach(int language in new[]{1,2}){
                Idas3MenuLocalization.SetLanguage(language);
                foreach(string key in new[]{"CLOCKWISE","COUNTERCLOCKWISE","OUTBOUND","INBOUND"})
                    check(Idas3MenuLocalization.T(key)!=key,"Missing translated direction: "+key);
            }
        }finally{Idas3MenuLocalization.SetLanguage(prior);}
        Directory.CreateDirectory(output);
        File.WriteAllText(Path.Combine(output,"managed.txt"),"PASS "+checks+" course/presence/replay/online/localization checks\n");
        Debug.Log("Odawara managed checks passed: "+checks);
        }finally{Idas3SceneShutdown();}
    }
}
