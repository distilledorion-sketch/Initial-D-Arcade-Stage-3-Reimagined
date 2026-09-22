using System;
using System.IO;
using System.Reflection;
public static class Idas3IncrementalUpdateBuild {
    public static void Build(){
        int checks=0;
        var type=typeof(Idas3Updates).Assembly.GetType("Idas3UpdateChecks");
        type.GetMethod("Run",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,new object[]{(Action<bool,string>)((ok,message)=>{checks++;if(!ok)throw new Exception(message);})});
        Directory.CreateDirectory("Verification/incremental-updates-20260922");
        File.WriteAllText("Verification/incremental-updates-20260922/unit-checks.txt","PASS "+checks+" update selection/version checks\n");
        Idas3HudSizeBuild.Build();
    }
}
