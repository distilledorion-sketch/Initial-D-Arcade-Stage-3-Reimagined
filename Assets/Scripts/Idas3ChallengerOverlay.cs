using System;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;
using Idas3.Multiplayer;

// Presentation is separate from discovery: retries never announce a match,
// and this unscaled clock keeps running through pause and focus changes.
public sealed class Idas3ChallengerOverlay : MonoBehaviour
{
    // Scene geometry uses layer 30. A later UI camera must never redraw it.
    const int OverlayLayer=29;
    Texture2D accepting;
    Idas3SceneGame host;
    Idas3MultiplayerSession session;
    enum Phase { Idle, Notice, Black, Reveal }
    const double FadeSeconds=.24;
    Phase phase;
    double began;
    int blackFrame;
    RenderTexture previousMenu;
    int previousMenuFrame=-1;
    internal bool NoticeVisible => phase==Phase.Notice && backdrop.Captured;
    internal float BlackAlpha {
        get {
            double elapsed=Time.realtimeSinceStartupAsDouble-began;
            if(phase==Phase.Notice)return Mathf.Clamp01((float)((elapsed-Idas3ChallengerAnimation.Duration)/FadeSeconds));
            if(phase==Phase.Black)return 1;
            if(phase==Phase.Reveal)return Mathf.Clamp01(1-(float)(elapsed/FadeSeconds));
            return 0;
        }
    }
    public bool Active => phase!=Phase.Idle;
    public bool SearchingVisible => session!=null&&session.IsQuickMatching&&!session.HandshakeComplete&&
        !session.IsRacing&&!Active&&host.Ready&&(host.Status.flags&(1u|32u|512u|1024u|4096u))==0&&host.Status.racePhase<3;
    internal int Announcements { get; private set; }
    Camera overlayCamera,sourceCamera;
    Canvas canvas;
    RawImage badge;
    Idas3ChallengerBackdrop backdrop;
    bool captureStarted,captureBadge;
    Idas3ChallengerBanner banner;
    RawImage Image(string name,Transform parent,Texture texture,Rect rect,float uvWidth=1){
        var go=new GameObject(name,typeof(RectTransform),typeof(CanvasRenderer),typeof(RawImage));go.layer=OverlayLayer;go.transform.SetParent(parent,false);
        var img=go.GetComponent<RawImage>();img.texture=texture;img.raycastTarget=false;img.uvRect=new Rect(0,1,uvWidth,-1);
        var rt=img.rectTransform;rt.anchorMin=rt.anchorMax=new Vector2(0,1);rt.pivot=new Vector2(0,1);rt.anchoredPosition=new Vector2(rect.x,-rect.y);rt.sizeDelta=rect.size;
        return img;
    }
    void CreateCanvas(){
        var cameraObject=new GameObject("Challenger overlay camera");cameraObject.transform.SetParent(transform,false);overlayCamera=cameraObject.AddComponent<Camera>();
        overlayCamera.depth=3;overlayCamera.cullingMask=1<<OverlayLayer;overlayCamera.clearFlags=CameraClearFlags.Depth;overlayCamera.orthographic=true;overlayCamera.allowHDR=false;overlayCamera.allowMSAA=false;
        var go=new GameObject("Original challenger overlay",typeof(RectTransform),typeof(Canvas));go.layer=OverlayLayer;go.transform.SetParent(transform,false);
        canvas=go.GetComponent<Canvas>();canvas.renderMode=RenderMode.ScreenSpaceCamera;canvas.worldCamera=overlayCamera;canvas.planeDistance=1;
        var backgroundObject=new GameObject("Captured challenger background",typeof(RectTransform),typeof(Idas3ChallengerBackdrop));
        backgroundObject.layer=OverlayLayer;backgroundObject.transform.SetParent(go.transform,false);
        var backgroundRect=backgroundObject.GetComponent<RectTransform>();backgroundRect.anchorMin=Vector2.zero;backgroundRect.anchorMax=Vector2.one;
        backgroundRect.offsetMin=backgroundRect.offsetMax=Vector2.zero;
        backdrop=backgroundObject.GetComponent<Idas3ChallengerBackdrop>();
        var root=new GameObject("640 by 480 safe area",typeof(RectTransform));root.layer=OverlayLayer;root.transform.SetParent(go.transform,false);
        var rect=root.GetComponent<RectTransform>();rect.anchorMin=rect.anchorMax=new Vector2(.5f,.5f);rect.sizeDelta=new Vector2(640,480);
        var bannerObject=new GameObject("Original interrupt banner",typeof(RectTransform),typeof(Idas3ChallengerBanner));
        bannerObject.layer=OverlayLayer;bannerObject.transform.SetParent(root.transform,false);
        var bannerRect=bannerObject.GetComponent<RectTransform>();bannerRect.anchorMin=bannerRect.anchorMax=bannerRect.pivot=new Vector2(0,1);bannerRect.sizeDelta=new Vector2(640,480);
        banner=bannerObject.GetComponent<Idas3ChallengerBanner>();banner.Initialize();
        badge=Image("Accepting challengers",root.transform,accepting,new Rect(518,286,114,28.5f));
        ApplyFrame();
    }
    public void Initialize(Idas3SceneGame owner,Idas3MultiplayerSession connection)
    {
        host=owner;session=connection;sourceCamera=host.GetComponent<Camera>();
        Texture2D Load(int i){var t=Idas3ChallengerBanner.LoadTexture(i);if(t==null)throw new InvalidOperationException("Missing original challenger texture "+i);return t;}
        accepting=Load(4);
        CreateCanvas();session.ChallengerFound+=Begin;
        StartCoroutine(RememberOpenMenu());
    }
    // IMGUI menus are painted after the cameras. Retain their completed frame
    // only while a connection is possible. Races take a single camera capture.
    IEnumerator RememberOpenMenu(){
        var end=new WaitForEndOfFrame();
        while(true){
            yield return end;
            if(!Active&&!session.HandshakeComplete&&host.WaitingWithChallengerMenu){
                if(previousMenu==null||previousMenu.width!=Screen.width||previousMenu.height!=Screen.height){
                    ReleaseMenu();
                    previousMenu=new RenderTexture(Screen.width,Screen.height,0,RenderTextureFormat.ARGB32){
                        name="Last matchmaking menu frame",filterMode=FilterMode.Point,wrapMode=TextureWrapMode.Clamp};
                    previousMenu.Create();
                }
                ScreenCapture.CaptureScreenshotIntoRenderTexture(previousMenu);
                previousMenuFrame=Time.frameCount;
            }else ReleaseMenu();
        }
    }
    void ReleaseMenu(){
        if(previousMenu!=null){previousMenu.Release();Destroy(previousMenu);previousMenu=null;}
        previousMenuFrame=-1;
    }
    void Begin()
    {
        if(Active||session.IsRacing)return;
        // This reflects the last rendered scene. HandshakeComplete has already
        // changed, so querying SearchingVisible here would lose its old badge.
        captureBadge=canvas.enabled&&overlayCamera.enabled&&badge.gameObject.activeInHierarchy;
        if(captureStarted){backdrop.EndCapture();captureStarted=false;}
        if(previousMenu!=null&&previousMenuFrame>=Time.frameCount-1&&host.HasChallengerMenu){
            backdrop.UseCompletedFrame(previousMenu);previousMenu=null;captureStarted=true;
        }
        host.BeginChallenger();
        phase=Phase.Notice;began=Time.realtimeSinceStartupAsDouble;++Announcements;
        FlashTaskbar();
    }
    internal void Tick()
    {
        if(!Active)return;
        if(!session.HandshakeComplete||!session.InLobby||session.IsRacing){
            if(phase!=Phase.Reveal)host.CancelChallenger();
            phase=Phase.Idle;session.CompleteChallengerPresentation();return;
        }
        double now=Time.realtimeSinceStartupAsDouble;
        if(phase==Phase.Notice&&now-began>=Idas3ChallengerAnimation.Duration+FadeSeconds){
            phase=Phase.Black;blackFrame=Time.frameCount;began=now;
        }else if(phase==Phase.Black&&Time.frameCount>blackFrame&&now-began>=.10){
            // Change the native owner only while the entire display is black.
            host.EnterChallengerLobby();
            phase=Phase.Reveal;began=Time.realtimeSinceStartupAsDouble;
        }else if(phase==Phase.Reveal&&now-began>=FadeSeconds){
            phase=Phase.Idle;session.CompleteChallengerPresentation();
        }
    }
    void OnDestroy(){
        if(session!=null)session.ChallengerFound-=Begin;
        if(backdrop!=null)backdrop.EndCapture();
        ReleaseMenu();
    }
    void OnGUI(){
        float alpha=BlackAlpha;
        if(alpha<=0||Event.current.type!=EventType.Repaint)return;
        // Cover the full output, including the widescreen margins and IMGUI prompts.
        GUI.depth=-32767;
        var color=GUI.color;var matrix=GUI.matrix;
        try{GUI.matrix=Matrix4x4.identity;GUI.color=new Color(0,0,0,alpha);
            GUI.DrawTexture(new Rect(0,0,Screen.width,Screen.height),Texture2D.whiteTexture);}
        finally{GUI.color=color;GUI.matrix=matrix;}
    }
    void LateUpdate()=>ApplyFrame();
    internal void ApplyFrame(){
        if(overlayCamera==null)return;
        bool searching=SearchingVisible;
        bool showOldScene=phase==Phase.Notice||phase==Phase.Black;
        if(!showOldScene&&captureStarted){backdrop.EndCapture();captureStarted=false;captureBadge=false;}
        overlayCamera.enabled=showOldScene||searching;canvas.enabled=overlayCamera.enabled;
        if(!overlayCamera.enabled)return;
        var source=sourceCamera;overlayCamera.targetTexture=source.targetTexture;
        overlayCamera.rect=new Rect(0,0,1,1);
        int width=source.targetTexture!=null?source.targetTexture.width:Screen.width;
        int height=source.targetTexture!=null?source.targetTexture.height:Screen.height;
        canvas.scaleFactor=Mathf.Min(width/640f,height/480f);
        // Center the badge in the actual gap: the timing/driver panel ends
        // 194 source pixels from the top; the dial begins 161 from the bottom.
        // Both HUD groups scale by fit and anchor to opposite viewport edges.
        float fit=canvas.scaleFactor;
        var options=host.GameOptions?.Current;
        float panelScale=options?.HudGroupScale((host.Status.flags&8192u)!=0?6:3)??1f;
        float dialScale=options?.HudGroupScale(2)??1f;
        float panelBottom=194f*fit*panelScale, dialTop=height-161f*fit*dialScale;
        float centerY=(panelBottom+dialTop)*.5f, centerX=width-80f*fit;
        float safeLeft=(width-640f*fit)*.5f, safeTop=(height-480f*fit)*.5f;
        // The independent badge stays in the gap between the resized groups.
        float hudScale=options?.HudGroupScale(8)??1f;
        centerX=width-80f*fit*Mathf.Max(panelScale,dialScale,hudScale);
        var offset=options?.HudOffset(8)??Vector2.zero;centerX+=width*offset.x;centerY+=height*offset.y;
        badge.rectTransform.localScale=Vector3.one*hudScale;
        badge.rectTransform.anchoredPosition=new Vector2(
            (centerX-safeLeft)/fit-badge.rectTransform.sizeDelta.x*.5f*hudScale,
            -(centerY-safeTop)/fit+badge.rectTransform.sizeDelta.y*.5f*hudScale);
        if(showOldScene){
            if(!captureStarted){backdrop.BeginCapture(overlayCamera,width,height);captureStarted=true;}
            int frame=Idas3ChallengerAnimation.FrameAt(Time.realtimeSinceStartupAsDouble-began);
            backdrop.ApplyFrame(frame);banner.ApplyFrame(frame);
        }
        // First render is the unchanged old scene (source animation frame 0).
        // Capture the search badge once, then display it only inside the frozen
        // picture, where it receives the same travelling brightness bands.
        backdrop.gameObject.SetActive(showOldScene);banner.gameObject.SetActive(showOldScene&&backdrop.Captured);
        badge.gameObject.SetActive(searching||(showOldScene&&!backdrop.Captured&&captureBadge));
    }
    [StructLayout(LayoutKind.Sequential)] struct FlashInfo {public uint size;public IntPtr window;public uint flags,count,timeout;}
    [DllImport("user32.dll")] static extern bool FlashWindowEx(ref FlashInfo info);
    void FlashTaskbar()
    {
#if UNITY_STANDALONE_WIN && !UNITY_EDITOR
        if(Application.isFocused||Idas3SceneGame.SilentTestOutput)return;
        try{using(var p=System.Diagnostics.Process.GetCurrentProcess()){
            var info=new FlashInfo{size=(uint)Marshal.SizeOf<FlashInfo>(),window=p.MainWindowHandle,flags=2,count=6,timeout=0};
            if(info.window!=IntPtr.Zero)FlashWindowEx(ref info);
        }}catch(Exception e){Debug.LogWarning("Challenger taskbar notification: "+e.Message);}
#endif
    }
}
