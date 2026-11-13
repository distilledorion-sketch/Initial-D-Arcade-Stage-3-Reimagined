using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.UI;

// Opt-in render regression. Uses production capture/banner components and the
// real player backbuffer as well as an offscreen target; no game or service owner.
public sealed class Idas3ChallengerGraphicsSmoke : MonoBehaviour
{
    public static bool Requested=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-challenger-graphics-check")>=0;
    [Serializable] sealed class Report {public bool passed;public int checks;public string error,device;public List<string> captures=new List<string>();}
    Report report=new Report();string output;
    Camera source,overlay;Canvas canvas;Idas3ChallengerBackdrop backdrop;GameObject canvasGo;Texture2D pattern;
    RenderTexture target;GameObject referenceImage;
    void Check(bool ok,string why){++report.checks;if(!ok)throw new InvalidOperationException(why);}
    Texture2D Picture(){
        if(target==null)return ScreenCapture.CaptureScreenshotAsTexture();
        var before=RenderTexture.active;RenderTexture.active=target;
        var t=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);t.ReadPixels(new Rect(0,0,target.width,target.height),0,0);t.Apply();RenderTexture.active=before;return t;
    }
    void Save(Texture2D t,string name){File.WriteAllBytes(Path.Combine(output,name+".png"),t.EncodeToPNG());report.captures.Add(name+".png");}
    IEnumerator Draw(){Canvas.ForceUpdateCanvases();if(target!=null){source.Render();overlay.Render();}yield return new WaitForEndOfFrame();}
    void Setup(int width,int height,bool offscreen){
        target=offscreen?new RenderTexture(width,height,24,RenderTextureFormat.ARGB32):null;
        if(target!=null)Check(target.Create(),"Offscreen target failed");
        source=new GameObject("Private pattern camera").AddComponent<Camera>();source.depth=1;source.targetTexture=target;source.cullingMask=0;source.clearFlags=CameraClearFlags.SolidColor;source.allowHDR=source.allowMSAA=false;source.enabled=!offscreen;
        overlay=new GameObject("Private production overlay camera").AddComponent<Camera>();overlay.depth=3;overlay.targetTexture=target;overlay.cullingMask=1<<29;overlay.clearFlags=CameraClearFlags.Depth;overlay.orthographic=true;overlay.allowHDR=overlay.allowMSAA=false;overlay.enabled=!offscreen;
        pattern=new Texture2D(64,48,TextureFormat.RGBA32,false){filterMode=FilterMode.Point};var pixels=new Color32[64*48];
        for(int y=0;y<48;++y)for(int x=0;x<64;++x)pixels[y*64+x]=new Color32((byte)(35+x*3),(byte)(45+y*4),(byte)(210-(x+y)%90),255);
        pattern.SetPixels32(pixels);pattern.Apply();
        canvasGo=new GameObject("Private production UI",typeof(RectTransform),typeof(Canvas));canvasGo.layer=29;canvas=canvasGo.GetComponent<Canvas>();canvas.renderMode=RenderMode.ScreenSpaceCamera;canvas.worldCamera=overlay;canvas.planeDistance=1;canvas.scaleFactor=Mathf.Min(width/640f,height/480f);
        referenceImage=new GameObject("Asymmetric reference",typeof(RectTransform),typeof(RawImage));referenceImage.layer=29;referenceImage.transform.SetParent(canvasGo.transform,false);
        var reference=referenceImage.GetComponent<RawImage>();reference.texture=pattern;reference.raycastTarget=false;
        var referenceRect=reference.rectTransform;referenceRect.anchorMin=Vector2.zero;referenceRect.anchorMax=Vector2.one;referenceRect.offsetMin=referenceRect.offsetMax=Vector2.zero;
        var bg=new GameObject("Production backdrop",typeof(RectTransform),typeof(Idas3ChallengerBackdrop));bg.layer=29;bg.transform.SetParent(canvasGo.transform,false);
        var rt=bg.GetComponent<RectTransform>();rt.anchorMin=Vector2.zero;rt.anchorMax=Vector2.one;rt.offsetMin=rt.offsetMax=Vector2.zero;
        backdrop=bg.GetComponent<Idas3ChallengerBackdrop>();bg.SetActive(false);
    }
    void Cleanup(){
        if(backdrop!=null)backdrop.EndCapture();
        if(source!=null){source.targetTexture=null;Destroy(source.gameObject);}if(overlay!=null){overlay.targetTexture=null;Destroy(overlay.gameObject);}
        if(canvasGo!=null)Destroy(canvasGo);if(pattern!=null)Destroy(pattern);if(target!=null){target.Release();Destroy(target);}target=null;
    }
    IEnumerator Cases(){
        AudioListener.volume=0;Application.runInBackground=true;Application.targetFrameRate=60;QualitySettings.vSyncCount=0;
        double deadline=Time.realtimeSinceStartupAsDouble+90;
        while(!Idas3RomGate.Verified||!Idas3Updates.StartupFinished){
            Check(Time.realtimeSinceStartupAsDouble<deadline,"Startup gate did not finish before graphics test");yield return null;
        }
        foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1280,720),new Vector2Int(2560,1080)})foreach(bool offscreen in new[]{false,true}){
            Screen.SetResolution(size.x,size.y,FullScreenMode.Windowed);for(int i=0;i<5;++i)yield return null;
            Check(offscreen||(Screen.width==size.x&&Screen.height==size.y),"Screen resolution not applied");
            string stem=size.x+"x"+size.y+(offscreen?"-target":"-screen");Setup(size.x,size.y,offscreen);
            yield return Draw();yield return Draw();var baseline=Picture();Save(baseline,stem+"-before");
            var referencePixels=baseline.GetPixels32();
            Check(Math.Abs(referencePixels[(size.y/4)*size.x+size.x/4].g-referencePixels[(size.y*3/4)*size.x+size.x/4].g)>70,
                "Asymmetric reference is obscured: "+stem);
            backdrop.BeginCapture(overlay,size.x,size.y);yield return Draw();Check(backdrop.Captured,"Capture callback missing");
            backdrop.gameObject.SetActive(true);backdrop.ApplyFrame(0);
            referenceImage.SetActive(false);source.backgroundColor=Color.magenta;
            yield return Draw();var actual=Picture();Save(actual,stem+"-frozen");
            var a=actual.GetPixels32();var b=baseline.GetPixels32();
            for(int y=13;y<size.y-13;y+=31)for(int x=17;x<size.x-17;x+=37){int at=y*size.x+x;
                Check(Math.Abs(a[at].r-b[at].r)<=3&&Math.Abs(a[at].g-b[at].g)<=3&&Math.Abs(a[at].b-b[at].b)<=3,
                    "Frozen background orientation/color differs at "+stem+" pixel "+x+","+y+" expected "+b[at]+" actual "+a[at]);}
            Destroy(actual);Destroy(baseline);
            Check(overlay.GetCommandBuffers(CameraEvent.AfterEverything).Length==0,"Copy repeated after the frozen frame");
            backdrop.EndCapture();backdrop.gameObject.SetActive(false);source.backgroundColor=new Color(.25f,.25f,.25f,1);
            var safe=new GameObject("640x480 safe area",typeof(RectTransform));safe.layer=29;safe.transform.SetParent(canvasGo.transform,false);
            var sr=safe.GetComponent<RectTransform>();sr.anchorMin=sr.anchorMax=new Vector2(.5f,.5f);sr.sizeDelta=new Vector2(640,480);
            var bannerGo=new GameObject("Production banner",typeof(RectTransform),typeof(Idas3ChallengerBanner));bannerGo.layer=29;bannerGo.transform.SetParent(safe.transform,false);
            var br=bannerGo.GetComponent<RectTransform>();br.anchorMin=br.anchorMax=br.pivot=new Vector2(0,1);br.sizeDelta=new Vector2(640,480);
            var banner=bannerGo.GetComponent<Idas3ChallengerBanner>();banner.Initialize();banner.ApplyFrame(64);
            yield return Draw();var full=Picture();Save(full,stem+"-banner");Destroy(full);
            foreach(var part in banner.GetComponentsInChildren<Idas3ChallengerGraphic>())if(part.name!="Original interrupt part 0")part.gameObject.SetActive(false);
            yield return Draw();var plate=Picture();Save(plate,stem+"-plate");
            var g=JsonUtility.FromJson<Idas3ChallengerBanner.Geometry>(Resources.Load<TextAsset>("Challenger/geometry").text).parts[0];
            float minX=float.MaxValue,maxX=float.MinValue,minY=float.MaxValue,maxY=float.MinValue;
            for(int i=0;i<g.vertices.Length;i+=5){Idas3ChallengerAnimation.Project(64,g.vertices[i],g.vertices[i+1],g.vertices[i+2],out var x,out var y);minX=Mathf.Min(minX,x);maxX=Mathf.Max(maxX,x);minY=Mathf.Min(minY,y);maxY=Mathf.Max(maxY,y);}
            float fit=Mathf.Min(size.x/640f,size.y/480f);int center=Mathf.RoundToInt((size.x-640*fit)*.5f+(minX+maxX)*.5f*fit);int row=Mathf.RoundToInt(size.y-((size.y-480*fit)*.5f+(minY+maxY)*.5f*fit));
            var data=plate.GetPixels32();int radius=Mathf.CeilToInt(3*fit),left=center-radius-8,right=center+radius+8;
            float reference=(data[row*size.x+left].r+data[row*size.x+right].r)*.5f;
            for(int x=center-radius;x<=center+radius;++x)Check(data[row*size.x+x].r>=reference-25,"Banner has an internal border/seam at "+stem+" x="+x);
            Destroy(plate);Cleanup();yield return null;
        }
    }
    IEnumerator Start(){
        var args=Environment.GetCommandLineArgs();int at=Array.IndexOf(args,"-idas3-challenger-graphics-check");output=Path.GetFullPath(args[at+1]);
        if(Directory.Exists(output))throw new InvalidOperationException("Use new graphics evidence directory");Directory.CreateDirectory(output);
        report.device=SystemInfo.graphicsDeviceName+" / "+SystemInfo.graphicsDeviceType;
        // Manually propagate nested coroutine failures into a durable report.
        var stack=new Stack<IEnumerator>();stack.Push(Cases());
        while(stack.Count>0){object value=null;bool moved=false;
            try{moved=stack.Peek().MoveNext();if(moved)value=stack.Peek().Current;}
            catch(Exception e){report.error=e.ToString();Debug.LogError(e);break;}
            if(!moved){stack.Pop();continue;}if(value is IEnumerator nested){stack.Push(nested);continue;}yield return value;
        }
        report.passed=report.error==null;Cleanup();File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(report,true));Application.Quit(report.passed?0:1);
    }
}
