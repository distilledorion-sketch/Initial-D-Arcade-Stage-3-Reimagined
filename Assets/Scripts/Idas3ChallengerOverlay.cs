using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.UI;
using Idas3.Multiplayer;

// Show the arcade search badge until a compatible opponent is connected.
public sealed class Idas3ChallengerOverlay : MonoBehaviour
{
    // Scene geometry uses layer 30. A later UI camera must never redraw it.
    const int OverlayLayer=29;
    Texture2D accepting;
    Idas3SceneGame host;
    Idas3MultiplayerSession session;
    public bool Active => session!=null&&session.ChallengerPending;
    public bool SearchingVisible => session!=null&&session.IsQuickMatching&&!session.HandshakeComplete&&
        !session.IsRacing&&!Active&&host.Ready&&(host.Status.flags&(1u|32u|512u|1024u|4096u))==0&&host.Status.racePhase<3;
    internal int Announcements { get; private set; }
    Camera overlayCamera,sourceCamera;
    Canvas canvas;
    RawImage badge;
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
        badge=Image("Accepting challengers",root.transform,accepting,new Rect(518,286,114,28.5f));
        ApplyFrame();
    }
    public void Initialize(Idas3SceneGame owner,Idas3MultiplayerSession connection)
    {
        host=owner;session=connection;sourceCamera=host.GetComponent<Camera>();
        Texture2D Load(int i){var t=Idas3ChallengerBanner.LoadTexture(i);if(t==null)throw new InvalidOperationException("Missing original challenger texture "+i);return t;}
        accepting=Load(4);
        CreateCanvas();session.ChallengerFound+=Begin;
    }
    void Begin()
    {
        if(session.IsRacing)return;
        session.AcceptBattle();
        host.EnterChallengerLobby();
        session.CompleteChallengerPresentation();
        ++Announcements;
        FlashTaskbar();
    }
    void OnDestroy(){if(session!=null)session.ChallengerFound-=Begin;}
    void LateUpdate()=>ApplyFrame();
    internal void ApplyFrame(){
        if(overlayCamera==null)return;
        bool searching=SearchingVisible;
        overlayCamera.enabled=searching;canvas.enabled=searching;
        if(!overlayCamera.enabled)return;
        overlayCamera.clearFlags=CameraClearFlags.Depth;
        overlayCamera.backgroundColor=Color.black;
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
