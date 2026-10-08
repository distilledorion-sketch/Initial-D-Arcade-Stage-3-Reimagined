using System;
using System.Collections;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;
using Idas3.Multiplayer;

// Opt-in isolated player diagnostic; renders production menu owners, never
// connects to Steam or writes to the player's real settings/save directory.
public sealed class Idas3CustomMenuLanguageSmoke:MonoBehaviour
{
    sealed class Platform:Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values before,Idas3GameOptions.Values after,bool display){}
    }
    string folder;bool done;float deadline;int captures,glyphs;
    delegate bool WindowVisitor(IntPtr window,IntPtr argument);
    [DllImport("user32.dll")] static extern bool EnumWindows(WindowVisitor callback,IntPtr argument);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window,out uint process);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr window,StringBuilder text,int count);
    [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int width,int height,uint flags);
    [DllImport("user32.dll")] static extern bool ShowWindow(IntPtr window,int command);
    // A hidden/minimized player suppresses IMGUI Repaint entirely. Keep only
    // this isolated diagnostic's window outside the desktop, without focus.
    static void EnableOffscreenRepaint(){
        if(Application.platform!=RuntimePlatform.WindowsPlayer)return;
        uint own=(uint)System.Diagnostics.Process.GetCurrentProcess().Id;
        EnumWindows((window,arg)=>{
            GetWindowThreadProcessId(window,out uint process);if(process!=own)return true;
            var name=new StringBuilder(128);GetClassName(window,name,128);if(name.ToString()!="UnityWndClass")return true;
            SetWindowPos(window,new IntPtr(1),-32000,-32000,1280,720,0x0010|0x0040);ShowWindow(window,4);return false;
        },IntPtr.Zero);
    }
    void Start(){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-custom-menu-language-smoke");
        if(at<0||at+1>=args.Length){Application.Quit(2);return;}
        folder=Path.GetFullPath(args[at+1]);if(Directory.Exists(folder)){Application.Quit(2);return;}
        Directory.CreateDirectory(folder);Screen.SetResolution(1280,720,FullScreenMode.Windowed);Application.runInBackground=true;deadline=Time.realtimeSinceStartup+120;
        StartCoroutine(Guard(Run()));
    }
    void Update(){if(!done&&Time.realtimeSinceStartup>deadline)Finish("Menu capture timed out.");}
    IEnumerator Guard(IEnumerator routine){
        while(!done){object next=null;bool more=false;Exception failure=null;
            try{more=routine.MoveNext();if(more)next=routine.Current;}catch(Exception e){failure=e;}
            if(failure!=null){Finish(failure.ToString());yield break;}if(!more){Finish(null);yield break;}yield return next;
        }
    }
    IEnumerator Run(){
        yield return null;
        EnableOffscreenRepaint();QualitySettings.vSyncCount=0;Application.targetFrameRate=60;
        var options=new Idas3GameOptions(new Platform());options.Initialize(Path.Combine(folder,"settings"));
        var pause=new GameObject("Actual settings").AddComponent<Idas3PauseMenu>();pause.Initialize(options);
        var music=new GameObject("Actual Sound Room").AddComponent<Idas3RaceMusicMenu>();
        music.Initialize(new[]{new Idas3RaceMusicMenu.Entry{id=2000,key="test.raw",title="PAUSE",artist="READY",stage=11,duration=180},new Idas3RaceMusicMenu.Entry{id=2001,key="test.jp",title="ロキ 初音ミク",artist="みきとP",stage=11,duration=160}},2000);
        var online=new GameObject("Actual online menu").AddComponent<Idas3MultiplayerMenu>();
        var session=new Idas3MultiplayerSession(0,Path.Combine(folder,"online"));online.Initialize(session);online.ManagedControlInput=true;
        var target=new RenderTexture(Screen.width,Screen.height,0,RenderTextureFormat.ARGB32);target.Create();Clear(target);
        for(int language=0;language<3;++language){
            options.BeginEdit();options.Draft.customMenuLanguage=language;options.ApplyDraft();
            var font=Idas3MenuLocalization.Font;
            if(language>0)foreach(var row in Idas3MenuLocalization.Entries){
                string text=row.Value[language-1];font.RequestCharactersInTexture(text,18,FontStyle.Normal);
                foreach(char c in text)if(!char.IsWhiteSpace(c)){
                    if(!font.GetCharacterInfo(c,out var glyph,18,FontStyle.Normal)||glyph.advance<=0)throw new Exception("Missing glyph "+((int)c).ToString("X4"));++glyphs;
                }
            }
            pause.OpenAttractOptions();pause.SelectTab(2);for(int i=0;i<5;++i)pause.Navigate(1);
            Debug.Log("Custom menu capture: settings language "+language);
            pause.RequestDiagnosticCapture(target);while(!pause.DiagnosticCaptureReady)yield return null;
            Save(target,"settings-"+language);
            if(language==0){
                options.Draft.customMenuLanguage=2;
                pause.RequestDiagnosticCapture(target);while(!pause.DiagnosticCaptureReady)yield return null;
                Save(target,"settings-chinese-choice");
            }
            pause.SetOpen(false);
            pause.OpenAttractOptions();pause.SelectTab(5);for(int i=0;i<3;++i)pause.Navigate(1);
            pause.RequestDiagnosticCapture(target);while(!pause.DiagnosticCaptureReady)yield return null;
            Save(target,"online-options-"+language);pause.SetOpen(false);
            music.SetOpen(true);music.RequestDiagnosticCapture(target);while(!music.DiagnosticCaptureReady)yield return null;
            Save(target,"music-"+language);if(!music.DiagnosticStageLabelsFit)throw new Exception("Music library labels clip");
            music.SetOpen(false);
            typeof(Idas3MultiplayerMenu).GetProperty("IsOpen").SetValue(online,true);
            online.RequestDiagnosticCapture(target);while(!online.DiagnosticCaptureReady)yield return null;
            Save(target,"online-"+language);typeof(Idas3MultiplayerMenu).GetProperty("IsOpen").SetValue(online,false);
        }
        music.SetOpen(true);music.Search("PAUSE");if(music.VisibleTrackCount!=1)throw new Exception("Original song metadata lost");
        int chosen=-1;music.Selected+=id=>chosen=id;music.Activate();if(chosen!=2000)throw new Exception("Language changed music identity");
        session.Dispose();target.Release();Destroy(target);Destroy(pause.gameObject);Destroy(music.gameObject);Destroy(online.gameObject);
    }
    void Save(RenderTexture target,string name){
        var prior=RenderTexture.active;RenderTexture.active=target;
        var image=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();
        int textPixels=0;foreach(var c in image.GetPixels32())if(c.r>180&&c.g>180&&c.b>180)++textPixels;
        if(textPixels<1000)throw new Exception("Invisible text in "+name);
        File.WriteAllBytes(Path.Combine(folder,name+".png"),image.EncodeToPNG());Destroy(image);RenderTexture.active=prior;++captures;Clear(target);
    }
    static void Clear(RenderTexture target){var prior=RenderTexture.active;RenderTexture.active=target;GL.Clear(true,true,Color.black);RenderTexture.active=prior;}
    void Finish(string error){if(done)return;done=true;
        File.WriteAllText(Path.Combine(folder,"report.json"),JsonUtility.ToJson(new Report{passed=error==null,error=error,captures=captures,glyphs=glyphs},true));Application.Quit(error==null?0:1);}
    [Serializable] sealed class Report {public bool passed;public string error;public int captures,glyphs;}
}
