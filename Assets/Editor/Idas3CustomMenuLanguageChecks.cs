using System;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using UnityEditor;
using UnityEngine;

public static class Idas3CustomMenuLanguageChecks
{
    sealed class Platform:Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now{get;set;}
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values before,Idas3GameOptions.Values after,bool display){}
    }
    static void Check(bool pass,string message){if(!pass)throw new InvalidOperationException(message);}
    public static void RunAndBuild(){Run();Idas3MenuFontBuild.BuildDiagnostic();}
    public static void Run()
    {
        string root=Path.GetFullPath("Verification/custom-menu-languages/settings-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        var go=new GameObject("Custom menu language checks");int count=0,prior=Idas3MenuLocalization.Language;
        try{
            File.WriteAllText(Path.Combine(root,"game-options.json"),"{\"version\":1,\"arcadeTextLanguage\":1,\"musicVolume\":1.5}");
            var platform=new Platform();var settings=new Idas3GameOptions(platform);settings.Initialize(root);
            Check(settings.Current.customMenuLanguage==0,"Old settings must retain English custom menus");
            var menu=go.AddComponent<Idas3PauseMenu>();menu.Initialize(settings);menu.OpenAttractOptions();menu.SelectTab(2);
            for(int i=0;i<5;++i)menu.Navigate(1);
            menu.NavigateHorizontal(1);
            Check(settings.Draft.customMenuLanguage==1&&settings.Current.customMenuLanguage==0&&Idas3MenuLocalization.Language==0,"Draft language escaped Apply");
            Check(settings.ApplyDraft(),settings.LastError);
            Check(Idas3MenuLocalization.Language==1&&settings.Current.arcadeTextLanguage==1,"Custom menu language affected original artwork");
            Check(Idas3MenuLocalization.LoadLanguage(root)==1,"Startup reader lost the menu language");
            settings.BeginEdit();settings.Draft.customMenuLanguage=2;Check(settings.ApplyDraft(),settings.LastError);
            var reload=new Idas3GameOptions(platform);reload.Initialize(root);
            Check(reload.Current.customMenuLanguage==2&&reload.Current.arcadeTextLanguage==1&&reload.Current.musicVolume==1.5f,"Language reload changed other settings");
            settings.BeginEdit();settings.Draft.customMenuLanguage=0;settings.Draft.width=1600;settings.ApplyDraft();settings.RevertDisplay();
            Check(Idas3MenuLocalization.Language==2,"Display rollback failed to restore menu language");
            Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{customMenuLanguage=99}).customMenuLanguage==0,"Invalid language must fall back to English");
            foreach(var row in Idas3MenuLocalization.Entries){
                string placeholders=string.Join(",",Regex.Matches(row.Key,@"\{\d+\}").Cast<Match>().Select(m=>m.Value).OrderBy(x=>x));
                for(int language=1;language<=2;++language){
                    Idas3MenuLocalization.SetLanguage(language);
                    var translated=Idas3MenuLocalization.T(row.Key);Check(!string.IsNullOrWhiteSpace(translated),"Empty translation: "+row.Key);
                    Check(placeholders==string.Join(",",Regex.Matches(translated,@"\{\d+\}").Cast<Match>().Select(m=>m.Value).OrderBy(x=>x)),"Changed placeholders: "+row.Key);
                    ++count;
                }
            }
            for(int language=1;language<=2;++language){
                Idas3MenuLocalization.SetLanguage(language);
                string expected=Idas3MenuLocalization.Format("ROOM / {0}","READY");
                Check(expected.Contains("READY")&&!expected.Contains("ROOM"),"Opaque room code was translated");
                Check(Idas3MenuLocalization.T("Checking game files… 10 / 20")==Idas3MenuLocalization.Format("Checking game files… {0} / {1}","10","20"),"Dynamic update message not translated");
                Check(Idas3MenuLocalization.T("Keep changes")!="Keep changes","Case variants are not translated");
                Check(Idas3MenuLocalization.Font!=null,"Bundled font missing");
            }
            foreach(string name in new[]{"JP","SC"}){
                var importer=(TrueTypeFontImporter)AssetImporter.GetAtPath("Assets/Resources/Fonts/NotoSans"+name+"-Regular.otf");
                Check(importer!=null&&importer.includeFontData,"CJK font is not bundled");
            }
            File.WriteAllText("Verification/custom-menu-languages/settings-report.json","{\"passed\":true,\"translatedEntriesChecked\":"+count+",\"draftPersistenceRollbackAndArtworkIsolation\":true}");
            Debug.Log("Custom menu language checks passed: "+count+" translations.");
        }finally{UnityEngine.Object.DestroyImmediate(go);Idas3MenuLocalization.SetLanguage(prior);}
    }
}
