using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// A standalone diagnostic scene renders the production menu in a fresh prefix.
// Pixel checks deliberately target glyphs, not the red frame or empty panels.
public sealed class Idas3MenuFontSmoke : MonoBehaviour
{
    [Serializable] private sealed class Report {
        public bool passed;
        public string error,font,graphics,version;
        public string[] installedFonts;
        public int oldFontLatinGlyphs,oldFontJapaneseGlyphs;
        public int titlePixels,japanesePixels,buttonPixels,checks,catalogTracks,catalogGlyphs;
    }
    [Serializable] private sealed class Catalog {public Track[] tracks;}
    [Serializable] private sealed class Track {public string title,artist,collection;}
    private readonly Report report=new Report();
    private string folder;
    private Idas3RaceMusicMenu menu;
    private double deadline;
    private bool finished;

    private void Start()
    {
        var args=Environment.GetCommandLineArgs();
        if(Array.IndexOf(args,"-idas3-custom-menu-language-smoke")>=0){enabled=false;gameObject.AddComponent<Idas3CustomMenuLanguageSmoke>();return;}
        if(Array.IndexOf(args,"-idas3-updater-flow-smoke")>=0){enabled=false;gameObject.AddComponent<Idas3UpdaterFlowSmoke>();return;}
        int at=Array.IndexOf(args,"-idas3-menu-font-smoke");
        if(at<0||at+1>=args.Length){Application.Quit(2);return;}
        folder=Path.GetFullPath(args[at+1]);
        if(Directory.Exists(folder)||File.Exists(folder)){Application.Quit(2);return;}
        Directory.CreateDirectory(folder);deadline=Time.realtimeSinceStartupAsDouble+90;
        Screen.SetResolution(1200,720,FullScreenMode.Windowed);
        Application.runInBackground=true;QualitySettings.antiAliasing=0;
        StartCoroutine(Guard(Run()));
    }
    private void Update(){if(!finished&&folder!=null&&Time.realtimeSinceStartupAsDouble>deadline)Finish("Font rendering timed out.");}
    private IEnumerator Guard(IEnumerator routine)
    {
        while(!finished){object current=null;bool more=false;Exception error=null;
            try{more=routine.MoveNext();if(more)current=routine.Current;}catch(Exception e){error=e;}
            if(error!=null){Finish(error.ToString());yield break;}
            if(!more){Finish(null);yield break;}yield return current;
        }
    }
    private void Check(bool condition,string message){++report.checks;if(!condition)throw new InvalidOperationException(message);}
    private Idas3RaceMusicMenu CreateMenu()
    {
        var value=new GameObject("Actual Sound Room").AddComponent<Idas3RaceMusicMenu>();
        value.Initialize(new[]{
            new Idas3RaceMusicMenu.Entry{id=2000,key="fixture.jp",title="ロキ 初音ミク",artist="みきとP",stage=11,duration=180},
            new Idas3RaceMusicMenu.Entry{id=1,key="fixture.en",title="SPEED LOVER",artist="SPEEDMAN",stage=3,duration=200}
        },2000);
        value.SetOpen(true);return value;
    }
    private static int CountGlyphs(Font font,string text)
    {
        if(font==null)return 0;
        font.RequestCharactersInTexture(text,18,FontStyle.Normal);int found=0;
        foreach(char c in text)if(font.GetCharacterInfo(c,out var g,18,FontStyle.Normal)&&g.advance>0&&g.glyphWidth>0&&g.glyphHeight>0)++found;
        return found;
    }
    private IEnumerator Run()
    {
        report.version=Application.version;report.graphics=SystemInfo.graphicsDeviceName;
        report.installedFonts=Font.GetOSInstalledFontNames();
        var oldFont=Font.CreateDynamicFontFromOSFont(new[]{"Segoe UI","Yu Gothic UI","Noto Sans CJK JP","Arial","DejaVu Sans"},18);
        report.oldFontLatinGlyphs=CountGlyphs(oldFont,"SOUNDROOM");report.oldFontJapaneseGlyphs=CountGlyphs(oldFont,"ロキ初音ミク");Destroy(oldFont);
        menu=CreateMenu();for(int i=0;i<10;++i)yield return null;
        var font=menu.DiagnosticFont;
        Check(font!=null&&font==Resources.Load<Font>("Fonts/NotoSansJP-Regular"),"Menu did not load the bundled font.");report.font=font.name;
        foreach(string text in new[]{"SOUNDROOM","ロキ初音ミク","↑↓←→★☆▶Ⅱ×–—›‹"})
            Check(CountGlyphs(font,text)==text.Length,"Missing bundled glyph: "+text);
        var catalog=JsonUtility.FromJson<Catalog>(File.ReadAllText(Path.Combine(Application.streamingAssetsPath,"SoundRoom","catalog.json")));
        Check(catalog?.tracks!=null&&catalog.tracks.Length>0,"Packaged song catalog is missing.");
        var glyphs=new HashSet<char>();
        foreach(var track in catalog.tracks)foreach(char c in track.title+track.artist+track.collection)
            if(!char.IsWhiteSpace(c))glyphs.Add(c);
        foreach(char c in glyphs)Check(CountGlyphs(font,c.ToString())==1,"Missing catalog character: U+"+((int)c).ToString("X4"));
        report.catalogTracks=catalog.tracks.Length;report.catalogGlyphs=glyphs.Count;
        // Also prove that destroying one menu does not destroy a shared font.
        menu.SetOpen(false);Destroy(menu.gameObject);yield return null;menu=CreateMenu();
        for(int i=0;i<5;++i)yield return null;
        Check(menu.DiagnosticFont==font,"Reopening Sound Room lost its shared font.");
        var target=new RenderTexture(Screen.width,Screen.height,0,RenderTextureFormat.ARGB32);
        Check(target.Create(),"Font capture target failed.");
        try{
            menu.RequestDiagnosticCapture(target);
            while(!menu.DiagnosticCaptureReady)yield return null;
            var prior=RenderTexture.active;var picture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
            try{
                RenderTexture.active=target;picture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);picture.Apply();
                File.WriteAllBytes(Path.Combine(folder,"sound-room.png"),picture.EncodeToPNG());
                report.titlePixels=TextPixels(picture,new Rect(26,17,500,48));
                report.japanesePixels=TextPixels(picture,new Rect(278,195,388,25));
                report.buttonPixels=TextPixels(picture,new Rect(822,566,270,34));
                Check(report.titlePixels>500,"Sound Room title is invisible.");
                Check(report.japanesePixels>80,"Japanese song title is invisible.");
                Check(report.buttonPixels>100,"Use for Race button text is invisible.");
                Check(menu.DiagnosticStageLabelsFit,"Bundled font clips library labels.");
            }finally{RenderTexture.active=prior;Destroy(picture);}
        }finally{menu.CancelDiagnosticCapture();target.Release();Destroy(target);}
        menu.Search("ロキ");Check(menu.VisibleTrackCount==1,"Japanese search failed with bundled text.");
        int selected=-1;menu.Selected+=id=>selected=id;menu.Activate();Check(selected==2000,"Menu selection failed.");
    }
    private static int TextPixels(Texture2D picture,Rect rect)
    {
        var safe=Screen.safeArea;float scale=Mathf.Min(1.5f,Mathf.Min(safe.width/1160f,safe.height/704f));
        float left=safe.x+(safe.width-1120*scale)*.5f,top=Screen.height-safe.yMax+(safe.height-664*scale)*.5f;
        int count=0;var pixels=picture.GetPixels32();
        for(int y=Mathf.CeilToInt(top+rect.y*scale);y<top+rect.yMax*scale;++y)
            for(int x=Mathf.CeilToInt(left+rect.x*scale);x<left+rect.xMax*scale;++x){
                int row=picture.height-1-y;if(x<0||x>=picture.width||row<0||row>=picture.height)continue;
                var color=pixels[row*picture.width+x];if(color.r>160&&color.g>160&&color.b>160)++count;
            }
        return count;
    }
    private void Finish(string error)
    {
        if(finished)return;finished=true;report.error=error;report.passed=error==null;
        if(folder!=null)File.WriteAllText(Path.Combine(folder,"report.json"),JsonUtility.ToJson(report,true));
        Application.Quit(report.passed?0:1);
    }
}
