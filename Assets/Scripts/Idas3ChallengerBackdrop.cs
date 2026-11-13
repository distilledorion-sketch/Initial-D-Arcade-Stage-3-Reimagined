using System;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.UI;

// Capture the previous scene including its last overlay. The caller keeps the
// challenge banner hidden for this first render; this graphic stays empty until
// capture completes, so neither the banner nor dimming is fed back into itself.
[RequireComponent(typeof(CanvasRenderer))]
public sealed class Idas3ChallengerBackdrop : MaskableGraphic
{
    RenderTexture capture;
    CommandBuffer copy;
    Camera captureCamera;
    Material ownedMaterial;
    int frame=-1;
    bool captureMeshPending, captureTopDown;
    public bool Captured { get; private set; }
    internal bool CaptureIsTopDown=>captureTopDown;
    public override Texture mainTexture=>capture!=null?capture:Texture2D.whiteTexture;
    public void BeginCapture(Camera camera,int width,int height)
    {
        EndCapture();
        if(ownedMaterial==null){
            var shader=Resources.Load<Shader>("Challenger/CapturedBackground");
            if(shader==null)throw new InvalidOperationException("Missing challenger captured-background shader");
            ownedMaterial=new Material(shader){name="Original challenger captured background"};material=ownedMaterial;
        }
        capture=new RenderTexture(width,height,0,RenderTextureFormat.ARGB32){
            name="Frozen challenger scene",filterMode=FilterMode.Point,wrapMode=TextureWrapMode.Clamp,
            useMipMap=false,autoGenerateMips=false,antiAliasing=1};
        if(!capture.Create())throw new InvalidOperationException("Could not capture challenger scene");
        // Direct3D screen copies retain the backbuffer's top-down orientation.
        // Camera render targets are already oriented as texture UVs. Remember
        // the source at capture time, even if the camera destination later changes.
        captureTopDown=camera.targetTexture==null&&SystemInfo.graphicsUVStartsAtTop;
        captureCamera=camera;copy=new CommandBuffer{name="Capture scene before challenger notice"};
        // At AfterEverything, CurrentActive may be a temporary UI target.
        // Read the camera's completed destination explicitly.
        copy.Blit(BuiltinRenderTextureType.CameraTarget,capture);
        copy.SetRenderTarget(BuiltinRenderTextureType.CameraTarget);
        camera.AddCommandBuffer(CameraEvent.AfterEverything,copy);
        Camera.onPostRender+=AfterCamera;
        Camera.onPreCull+=BeforeCamera;
        raycastTarget=false;SetAllDirty();
    }
    void AfterCamera(Camera camera)
    {
        if(camera!=captureCamera)return;
        // Unity invokes this callback before executing AfterEverything buffers.
        // Leave the copy attached until this render has fully finished. Also do
        // not rebuild the graphic into this same frame's old-scene capture.
        Captured=true;captureMeshPending=true;Camera.onPostRender-=AfterCamera;
    }
    void BeforeCamera(Camera camera){if(camera==captureCamera&&Captured)RemoveCopy();}
    void RemoveCopy()
    {
        Camera.onPostRender-=AfterCamera;
        Camera.onPreCull-=BeforeCamera;
        if(copy!=null){
            if(captureCamera!=null)captureCamera.RemoveCommandBuffer(CameraEvent.AfterEverything,copy);
            copy.Dispose();copy=null;
        }
        captureCamera=null;
    }
    public void EndCapture()
    {
        RemoveCopy();Captured=false;captureMeshPending=false;captureTopDown=false;
        if(capture!=null){capture.Release();if(Application.isPlaying)Destroy(capture);else DestroyImmediate(capture);capture=null;}
        frame=-1;SetAllDirty();
    }
    public void ApplyFrame(int value){if(frame==value&&!captureMeshPending)return;frame=value;captureMeshPending=false;SetVerticesDirty();}
    protected override void OnPopulateMesh(VertexHelper mesh)
    {
        mesh.Clear();if(capture==null||!Captured)return;
        var rect=rectTransform.rect;
        for(int i=0;i<100;i++){
            Idas3ChallengerAnimation.BackdropVertex(frame,i,out float x,out float y,out byte c);
            mesh.AddVert(new Vector3(rect.xMin+x/640f*rect.width,rect.yMax-y/480f*rect.height,0),
                new Color32(c,c,c,c),new Vector2(x/640f,captureTopDown?y/480f:1-y/480f));
        }
        for(int i=2;i<100;i++){
            if((i&1)==0)mesh.AddTriangle(i-2,i-1,i);else mesh.AddTriangle(i-1,i-2,i);
        }
    }
    protected override void OnDestroy()
    {
        EndCapture();
        if(ownedMaterial!=null){if(Application.isPlaying)Destroy(ownedMaterial);else DestroyImmediate(ownedMaterial);}
        base.OnDestroy();
    }
}
