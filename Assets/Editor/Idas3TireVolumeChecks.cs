using System;
using System.IO;
using UnityEngine;
public static class Idas3TireVolumeChecks {
    sealed class Platform:Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values before,Idas3GameOptions.Values after,bool display){}
    }
    public static void Run(){
        int checks=0;void Check(bool ok,string why){checks++;if(!ok)throw new Exception(why);}
        string root="Verification/tire-volume-20260922/settings-"+Guid.NewGuid().ToString("N");Directory.CreateDirectory(root);
        foreach(float volume in new[]{0f,.35f,1f}){
            string folder=Path.Combine(root,volume.ToString(System.Globalization.CultureInfo.InvariantCulture));Directory.CreateDirectory(folder);
            string json="{\"version\":1,\"engineVolume\":"+volume.ToString(System.Globalization.CultureInfo.InvariantCulture)+",\"musicVolume\":0.4}";
            string file=Path.Combine(folder,"game-options.json");File.WriteAllText(file,json);
            var options=new Idas3GameOptions(new Platform());options.Initialize(folder);
            Check(options.LastError==null&&options.Current.engineVolume==volume&&options.Current.tireVolume==volume,"Legacy combined engine/tire volume changed");
            Check(File.ReadAllText(file)==json,"Loading overwrote saved options");
            options.BeginEdit();options.Draft.tireVolume=.2f;
            Check(options.HasUnsavedChanges&&options.Current.tireVolume==volume,"Tire draft applied before APPLY");
            options.BeginEdit();Check(options.Current.tireVolume==volume,"Cancel changed tire volume");
            options.BeginEdit();options.Draft.tireVolume=0;Check(options.ApplyDraft(),"Tire APPLY failed");
            var reload=new Idas3GameOptions(new Platform());reload.Initialize(folder);
            Check(reload.Current.tireVolume==0&&reload.Current.engineVolume==volume&&reload.Current.musicVolume==.4f,"Zero tire volume did not persist independently");
            reload.BeginEdit();reload.Draft.engineVolume=.8f;Check(reload.ApplyDraft()&&reload.Current.tireVolume==0,"Engine slider changed tire setting");
            reload.BeginEdit();reload.ResetDraft();Check(reload.Draft.tireVolume==1&&reload.Draft.engineVolume==1,"Audio defaults incorrect");
        }
        foreach(float invalid in new[]{-1f,2f,float.NaN,float.PositiveInfinity}){
            var v=new Idas3GameOptions.Values{tireVolume=invalid};var normalized=Idas3GameOptions.Normalize(v);
            Check(normalized.tireVolume>=0&&normalized.tireVolume<=1,"Invalid tire volume not bounded");
        }
        File.WriteAllText("Verification/tire-volume-20260922/settings-checks.txt","PASS "+checks+" independent audio settings and migration checks\n");
        Idas3ControllerMenuChecks.Run();
    }
    public static void Build(){Run();Idas3HudSizeBuild.Build();}
}
