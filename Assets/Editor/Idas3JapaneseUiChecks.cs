using System;
using System.IO;
using UnityEngine;

public static class Idas3JapaneseUiChecks
{
    sealed class Platform:Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values before,Idas3GameOptions.Values after,bool display){}
    }
    static void Check(bool ok,string why){if(!ok)throw new Exception(why);}
    public static void RunAndBuild(){Run();Idas3IdZeroPerformanceBuild.Build();}
    public static void Run(){
        string proof=Path.GetFullPath("Verification/japanese-revc-20261008");
        string root=Path.Combine(proof,"settings-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        var go=new GameObject("Japanese UI options checks");
        try{
            string path=Path.Combine(root,"game-options.json");
            File.WriteAllText(path,"{\"version\":1,\"musicVolume\":1.5}");
            var options=new Idas3GameOptions(new Platform());options.Initialize(root);
            Check(options.Current.arcadeTextLanguage==0&&Idas3GameOptions.LoadArcadeTextLanguage(root)==0,"Older settings did not retain English");
            var menu=go.AddComponent<Idas3PauseMenu>();menu.Initialize(options);menu.OpenAttractOptions();menu.SelectTab(2);
            for(int row=0;row<4;++row)menu.Navigate(1);
            menu.NavigateHorizontal(1);
            Check(options.Draft.arcadeTextLanguage==1&&options.Current.arcadeTextLanguage==0&&options.HasUnsavedChanges,"Language edit bypassed Apply");
            Check(Idas3GameOptions.LoadArcadeTextLanguage(root)==0,"Unapplied language reached startup");
            Check(options.ApplyDraft(),"Language setting did not save: "+options.LastError);
            Check(Idas3GameOptions.LoadArcadeTextLanguage(root)==1,"Startup did not read Japanese");
            var reload=new Idas3GameOptions(new Platform());reload.Initialize(root);
            Check(reload.Current.arcadeTextLanguage==1&&reload.Current.musicVolume==1.5f,"Settings reload changed another option");
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{arcadeTextLanguage=99}).arcadeTextLanguage==0,"Unknown language did not fall back to English");
            options.BeginEdit();options.Draft.arcadeTextLanguage=0;Check(options.ApplyDraft(),"English setting did not save");
            Check(Idas3GameOptions.LoadArcadeTextLanguage(root)==0,"Could not switch back to English");
            File.WriteAllText(Path.Combine(proof,"settings-checks.json"),"{\"passed\":true,\"checks\":\"legacy defaults, navigation, draft isolation, persistence, restart reader, invalid values, English restore\"}");
        }finally{UnityEngine.Object.DestroyImmediate(go);}
    }
}
