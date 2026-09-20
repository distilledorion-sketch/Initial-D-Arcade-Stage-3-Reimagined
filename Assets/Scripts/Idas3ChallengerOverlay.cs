using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;
using Idas3.Multiplayer;

// Presentation is separate from discovery: retries never announce a match,
// and this unscaled clock keeps running through pause and focus changes.
public sealed class Idas3ChallengerOverlay : MonoBehaviour
{
    const double Duration=2.8;
    // Scene geometry uses layer 30. A later UI camera must never redraw it.
    const int OverlayLayer=29;
    Texture2D background,eyes,lettering,flash,accepting;
    Idas3SceneGame host;
    Idas3MultiplayerSession session;
    double began;
    public bool Active { get; private set; }
    public bool SearchingVisible => session!=null&&session.IsQuickMatching&&!session.HandshakeComplete&&
        !session.IsRacing&&!Active&&host.Ready&&(host.Status.flags&(1u|32u|512u|1024u|4096u))==0&&host.Status.racePhase<3;
    internal int Announcements { get; private set; }
    Camera overlayCamera;
    Canvas canvas;
    RawImage shade,bandImage,eyeImage,titleImage,flashImage,badge;
    RectTransform band;
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
        var root=new GameObject("640 by 480 safe area",typeof(RectTransform));root.layer=OverlayLayer;root.transform.SetParent(go.transform,false);
        var rect=root.GetComponent<RectTransform>();rect.anchorMin=rect.anchorMax=new Vector2(.5f,.5f);rect.sizeDelta=new Vector2(640,480);
        shade=Image("Dim",root.transform,Texture2D.blackTexture,new Rect(-640,-480,1920,1440));shade.color=new Color(1,1,1,.35f);
        bandImage=Image("Original interrupt background",root.transform,background,new Rect(12,205,616,82.72f));band=bandImage.rectTransform;
        eyeImage=Image("Original eyes",band,eyes,new Rect(2.44f,10.81f,190.2f,60.67f),.75f);
        titleImage=Image("Challenge received",band,lettering,new Rect(191.23f,5.96f,424.77f,70.79f),.75f);
        flashImage=Image("Original flashing lettering",band,flash,new Rect(191.23f,5.96f,424.77f,70.79f),.75f);
        badge=Image("Accepting challengers",root.transform,accepting,new Rect(518,286,114,28.5f));
        ApplyFrame();
    }
    public void Initialize(Idas3SceneGame owner,Idas3MultiplayerSession connection)
    {
        host=owner;session=connection;
        Texture2D Load(int i){var t=Resources.Load<Texture2D>("Challenger/interrupt_"+i);if(t==null)throw new InvalidOperationException("Missing original challenger texture "+i);return t;}
        background=Load(0);eyes=Load(1);lettering=Load(2);flash=Load(3);accepting=Load(4);
        CreateCanvas();session.ChallengerFound+=Begin;
    }
    void Begin()
    {
        if(Active||session.IsRacing)return;
        host.BeginChallenger();Active=true;began=Time.realtimeSinceStartupAsDouble;++Announcements;
        FlashTaskbar();
    }
    internal void Tick()
    {
        if(!Active)return;
        if(!session.HandshakeComplete||!session.InLobby||session.IsRacing){
            Active=false;host.CancelChallenger();session.CompleteChallengerPresentation();return;
        }
        if(Time.realtimeSinceStartupAsDouble-began<Duration)return;
        // Keep Ready disabled until the native offline owner is fully retired.
        host.EnterChallengerLobby();Active=false;session.CompleteChallengerPresentation();
    }
    void OnDestroy(){if(session!=null)session.ChallengerFound-=Begin;}
    void LateUpdate()=>ApplyFrame();
    internal void ApplyFrame(){
        if(overlayCamera==null)return;
        var source=host.GetComponent<Camera>();overlayCamera.targetTexture=source.targetTexture;
        overlayCamera.rect=new Rect(0,0,1,1);
        int width=source.targetTexture!=null?source.targetTexture.width:Screen.width;
        int height=source.targetTexture!=null?source.targetTexture.height:Screen.height;
        canvas.scaleFactor=Mathf.Min(width/640f,height/480f);
        // Center the badge in the actual gap: the timing/driver panel ends
        // 194 source pixels from the top; the dial begins 161 from the bottom.
        // Both HUD groups scale by fit and anchor to opposite viewport edges.
        float fit=canvas.scaleFactor;
        float panelBottom=194f*fit, dialTop=height-161f*fit;
        float centerY=(panelBottom+dialTop)*.5f, centerX=width-80f*fit;
        float safeLeft=(width-640f*fit)*.5f, safeTop=(height-480f*fit)*.5f;
        badge.rectTransform.anchoredPosition=new Vector2(
            (centerX-safeLeft)/fit-badge.rectTransform.sizeDelta.x*.5f,
            -(centerY-safeTop)/fit+badge.rectTransform.sizeDelta.y*.5f);
        overlayCamera.enabled=Active||SearchingVisible;canvas.enabled=overlayCamera.enabled;
        shade.gameObject.SetActive(Active);bandImage.gameObject.SetActive(Active);badge.gameObject.SetActive(SearchingVisible);
        if(Active){
            float age=(float)(Time.realtimeSinceStartupAsDouble-began);
            float slide=(1-Mathf.SmoothStep(0,1,Mathf.Clamp01(age/.2f)))*640;
            band.anchoredPosition=new Vector2(12+slide,-205);
            flashImage.enabled=age>.2f&&((int)((age-.2f)*8)&1)==0;
        }
    }
    [StructLayout(LayoutKind.Sequential)] struct FlashInfo {public uint size;public IntPtr window;public uint flags,count,timeout;}
    [DllImport("user32.dll")] static extern bool FlashWindowEx(ref FlashInfo info);
    void FlashTaskbar()
    {
#if UNITY_STANDALONE_WIN && !UNITY_EDITOR
        if(Application.isFocused)return;
        try{using(var p=System.Diagnostics.Process.GetCurrentProcess()){
            var info=new FlashInfo{size=(uint)Marshal.SizeOf<FlashInfo>(),window=p.MainWindowHandle,flags=2,count=6,timeout=0};
            if(info.window!=IntPtr.Zero)FlashWindowEx(ref info);
        }}catch(Exception e){Debug.LogWarning("Challenger taskbar notification: "+e.Message);}
#endif
    }
}
