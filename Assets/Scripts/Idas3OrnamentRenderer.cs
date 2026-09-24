using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

// Actual recovered geometry rendered in a small transparent HUD viewport.
// Assembly and pendulum motion are adaptations for this game's presentation.
public sealed class Idas3OrnamentRenderer : IDisposable
{
    [StructLayout(LayoutKind.Sequential,Pack=8)]
    internal struct Telemetry {
        public uint size,version;public ulong simulationTicks;public uint flags,car;
        public float x,y,z,yaw;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    static extern int Idas3SceneGetOrnamentTelemetry(ref Telemetry data);
    [StructLayout(LayoutKind.Sequential,Pack=8)]
    internal struct PresentationTiming {
        public uint size,version;public ulong simulationTicks;public float alpha;public uint flags;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    static extern int Idas3SceneGetPresentationTiming(ref PresentationTiming data);
    internal static bool ReadTiming(out PresentationTiming data){data=new PresentationTiming{size=24};return Idas3SceneGetPresentationTiming(ref data)==1&&data.version==1;}
    internal static bool Read(out Telemetry data){data=new Telemetry{size=40};return Idas3SceneGetOrnamentTelemetry(ref data)==1&&data.version==1;}
    sealed class SharedTexture {public Texture2D value;public int users;}
    static readonly Dictionary<string,SharedTexture> texturePool=new Dictionary<string,SharedTexture>();
    readonly Dictionary<string,Texture2D> textures=new Dictionary<string,Texture2D>();
    readonly List<Material> materials=new List<Material>();
    readonly List<Piece> pieces=new List<Piece>();
    readonly List<Idas3OrnamentChainMesh> chainMeshes=new List<Idas3OrnamentChainMesh>();
    struct Piece {public Mesh mesh;public Material material;public Matrix4x4 assembly;public bool chain;}
    Idas3OrnamentCatalog.MeshParts ornament,strap;
    readonly Idas3OrnamentMotion motion=new Idas3OrnamentMotion();
    Camera camera;GameObject cameraObject;RenderTexture target;
    CommandBuffer modelCommands;Mesh screenQuad;Material overlay;
    MaterialPropertyBlock overlayProperties;
    int selectedId;uint lastCar=uint.MaxValue;Quaternion lastRotation;bool rendered,dynamicPose;
    Idas3OrnamentMotion lastMotion;ulong lastRevision;long previewTick=-1;float lastAlpha=1;
    static Idas3OrnamentRenderer preview;
    internal static int ResidentTextureCount=>texturePool.Count;
    internal int SelectedId=>selectedId;
    internal int PartCount=>pieces.Count;
    internal Texture Output=>target;
    internal Quaternion Swing=>motion.Rotation;
    internal float RenderedAlpha=>lastAlpha;
    internal Quaternion RenderedPendantRotation=>motion.RenderPendantRotation(lastAlpha);
    internal Idas3OrnamentMotion Motion=>motion;
    internal Vector3 ChainAttachment=>chainMeshes.Count>0?chainMeshes[0].AttachmentPosition:motion.AttachmentPosition;
    internal void Suspend(){motion.Reset();lastCar=uint.MaxValue;rendered=false;}
    internal static bool PreviewLoaded=>preview!=null;
    Texture2D LoadTexture(string path){
        if(string.IsNullOrEmpty(path))return Texture2D.whiteTexture;
        if(textures.TryGetValue(path,out var found))return found;
        if(!texturePool.TryGetValue(path,out var shared)){
            var value=Resources.Load<Texture2D>(path);if(!value)throw new InvalidOperationException("Missing ornament texture: "+path);
            shared=new SharedTexture{value=value};texturePool.Add(path,shared);
        }
        ++shared.users;textures.Add(path,shared.value);return shared.value;
    }
    static void DestroyOwned(UnityEngine.Object value){if(!value)return;if(Application.isPlaying)UnityEngine.Object.Destroy(value);else UnityEngine.Object.DestroyImmediate(value);}
    void ReleaseSelection(){
        pieces.Clear();chainMeshes.Clear();foreach(var material in materials)DestroyOwned(material);materials.Clear();
        ornament?.Dispose();strap?.Dispose();ornament=strap=null;
        foreach(var pair in textures)if(texturePool.TryGetValue(pair.Key,out var shared)&&--shared.users==0){texturePool.Remove(pair.Key);if(shared.value)Resources.UnloadAsset(shared.value);}
        textures.Clear();selectedId=0;rendered=false;dynamicPose=false;lastMotion=null;previewTick=-1;lastCar=uint.MaxValue;motion.Reset();
    }
    void EnsureRenderer(){
        if(camera)return;
        var shader=Resources.Load<Shader>("ArcadeOrnament");var screenShader=Resources.Load<Shader>("ArcadeOrnamentOverlay");
        if(!shader||!screenShader||!shader.isSupported||!screenShader.isSupported)throw new InvalidOperationException("Ornament shaders are unavailable.");
        cameraObject=new GameObject("Hanging ornament camera"){hideFlags=HideFlags.HideAndDontSave};
        camera=cameraObject.AddComponent<Camera>();camera.enabled=false;camera.cullingMask=0;
        camera.orthographic=true;camera.orthographicSize=1.35f;camera.nearClipPlane=.1f;camera.farClipPlane=20;
        camera.allowHDR=false;camera.allowMSAA=false;camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.clear;
        cameraObject.transform.position=new Vector3(0,-1.1f,-5);cameraObject.transform.rotation=Quaternion.identity;
        target=new RenderTexture(384,384,24,RenderTextureFormat.ARGB32){name="Hanging ornament",hideFlags=HideFlags.DontSave,antiAliasing=1};target.Create();camera.targetTexture=target;
        modelCommands=new CommandBuffer{name="Actual ornament geometry"};camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,modelCommands);
        screenQuad=new Mesh{name="Hanging ornament viewport",hideFlags=HideFlags.DontSave};screenQuad.vertices=new[]{Vector3.zero,Vector3.right,Vector3.up,Vector3.one};
        screenQuad.uv=new[]{new Vector2(0,1),new Vector2(1,1),new Vector2(0,0),new Vector2(1,0)};screenQuad.triangles=new[]{0,1,2,2,1,3};
        screenQuad.bounds=new Bounds(Vector3.zero,Vector3.one*10000);
        overlay=new Material(screenShader){hideFlags=HideFlags.DontSave};overlayProperties=new MaterialPropertyBlock();
    }
    void AddParts(Idas3OrnamentCatalog.MeshParts model,Matrix4x4 assembly,Idas3OrnamentChainRig.Rig rig=null){
        var shader=Resources.Load<Shader>("ArcadeOrnament");
        for(int partIndex=0;partIndex<model.Parts.Length;++partIndex){
            var part=model.Parts[partIndex];
            var source=part.material;var material=new Material(shader){hideFlags=HideFlags.DontSave};
            material.SetTexture("_MainTex",LoadTexture(source.texture));material.SetColor("_Color",source.tint);
            material.SetFloat("_Cutoff",source.alphaCutoff);material.SetInt("_Cull",source.doubleSided?0:2);material.SetInt("_ZWrite",source.transparent?0:1);
            if(rig!=null)chainMeshes.Add(new Idas3OrnamentChainMesh(part.mesh,assembly,rig,partIndex));
            materials.Add(material);pieces.Add(new Piece{mesh=part.mesh,material=material,assembly=rig!=null?Matrix4x4.identity:assembly,chain=rig!=null});
        }
    }
    internal bool Select(int id){
        if(id==selectedId&&id!=0)return true;
        ReleaseSelection();if(id==0)return false;
        var entry=Idas3OrnamentCatalog.Get(id);if(entry==null)return false;
        EnsureRenderer();
        try{
            ornament=Idas3OrnamentCatalog.LoadModel(id);strap=Idas3OrnamentCatalog.LoadStrap(entry.strapId);
            var min=ornament.BoundsMin;var max=ornament.BoundsMax;var size=max-min;
            float scale=Mathf.Min(.92f/Mathf.Max(.001f,size.y),1.15f/Mathf.Max(.001f,size.x));
            scale=Mathf.Min(scale,.7f/Mathf.Max(.001f,size.z));
            // Original native assembly transforms are not available. Align the
            // recovered strap end and the ornament top, keeping genuine meshes.
            if(strap!=null){var a=strap.BoundsMin;var b=strap.BoundsMax;float s=.9f/Mathf.Max(.001f,b.y-a.y);
                var rig=Idas3OrnamentChainRig.Load(entry.strapId);
                if(rig==null||rig.parts.Length!=strap.Parts.Length)throw new InvalidOperationException("Missing recovered chain rig: "+entry.strapId);
                AddParts(strap,Matrix4x4.Scale(Vector3.one*s)*Matrix4x4.Translate(new Vector3(-(a.x+b.x)*.5f,-b.y,-(a.z+b.z)*.5f)),rig);}
            AddParts(ornament,Matrix4x4.Translate(new Vector3(0,-.88f,0))*Matrix4x4.Scale(Vector3.one*scale)*
                Matrix4x4.Translate(new Vector3(-(min.x+max.x)*.5f,-max.y,-(min.z+max.z)*.5f)));
            selectedId=id;return true;
        }catch{ReleaseSelection();throw;}
    }
    internal Texture RenderPose(int id,Quaternion rotation){
        if(!Select(id))return null;
        if(rendered&&!dynamicPose&&Quaternion.Angle(lastRotation,rotation)<.001f)return target;
        foreach(var chain in chainMeshes)chain.Restore();
        modelCommands.Clear();var root=Matrix4x4.Rotate(rotation);
        foreach(var piece in pieces)modelCommands.DrawMesh(piece.mesh,root*piece.assembly,piece.material,0,0);
        camera.Render();lastRotation=rotation;rendered=true;dynamicPose=false;return target;
    }
    internal Texture RenderMotion(int id,Idas3OrnamentMotion state){
        return RenderInterpolatedMotion(id,state,1);
    }
    internal Texture RenderInterpolatedMotion(int id,Idas3OrnamentMotion state,float alpha){
        if(!Select(id))return null;
        alpha=float.IsNaN(alpha)||float.IsInfinity(alpha)?1:Mathf.Clamp01(alpha);
        if(!state.HasRenderMotion)alpha=1;
        if(rendered&&dynamicPose&&ReferenceEquals(lastMotion,state)&&lastRevision==state.PresentationRevision&&lastAlpha==alpha)return target;
        foreach(var chain in chainMeshes)chain.Update(state,alpha);
        // Rest pendant assembly starts at -.88; its pivot follows the actual
        // chain end at -.9, retaining the original .02 attachment overlap.
        var pendant=Matrix4x4.TRS(ChainAttachment,state.RenderPendantRotation(alpha),Vector3.one)*Matrix4x4.Translate(new Vector3(0,.9f,0));
        modelCommands.Clear();
        foreach(var piece in pieces)modelCommands.DrawMesh(piece.mesh,piece.chain?Matrix4x4.identity:pendant*piece.assembly,piece.material,0,0);
        camera.Render();rendered=true;dynamicPose=true;lastMotion=state;lastRevision=state.PresentationRevision;lastAlpha=alpha;return target;
    }
    internal bool UpdateLive(int id,out Telemetry data){
        data=default;
        if(id==0){Dispose();return false;}
        if(!Read(out data)||(data.flags&1)==0){motion.Reset();lastCar=uint.MaxValue;rendered=false;return false;}
        if(!Select(id))return false;
        if(lastCar!=data.car){motion.Reset();lastCar=data.car;rendered=false;}
        motion.Sample(new Vector3(data.x,data.y,data.z),data.yaw,data.simulationTicks,true,(data.flags&2)!=0);
        // Pausing holds the exact displayed subframe, rather than snapping to
        // the newest simulation endpoint when the native clock resets.
        if((data.flags&2)!=0&&rendered&&dynamicPose)return true;
        float alpha=ReadTiming(out var timing)&&timing.simulationTicks==data.simulationTicks?timing.alpha:1;
        RenderInterpolatedMotion(id,motion,alpha);return true;
    }
    internal static Rect ScreenBounds(float width,float height,Idas3GameOptions.Values options=null){
        float fit=Mathf.Min(width/1280f,height/720f),size=240*fit*(options?.HudGroupScale(10)??1);
        var offset=options?.HudOffset(10)??Vector2.zero;
        if(float.IsNaN(offset.x)||float.IsInfinity(offset.x))offset.x=0;
        if(float.IsNaN(offset.y)||float.IsInfinity(offset.y))offset.y=0;
        float top=-size*(.25f/2.7f);
        return new Rect(Mathf.Clamp(width*.66f-size*.5f+offset.x*width,0,Mathf.Max(0,width-size)),
            Mathf.Clamp(top+offset.y*height,top,Mathf.Max(top,height-size)),size,size);
    }
    internal void Render(CommandBuffer commands,float width,float height,Idas3GameOptions.Values options=null){
        if(!rendered||!target)return;
        var rect=ScreenBounds(width,height,options);overlayProperties.SetTexture("_MainTex",target);overlayProperties.SetFloat("_Screen",1);
        overlayProperties.SetVector("_Canvas",new Vector4(width,height,0,0));overlayProperties.SetVector("_Rect",new Vector4(rect.x,rect.y,rect.width,rect.height));
        commands.DrawMesh(screenQuad,Matrix4x4.identity,overlay,0,0,overlayProperties);
    }
    public static void DrawPreview(Rect area,int id,float seconds){
        if(Event.current.type!=EventType.Repaint)return;
        if(id==0){ReleasePreview();return;}
        if(preview==null)preview=new Idas3OrnamentRenderer();
        var texture=preview.RenderPreview(id,seconds);if(!texture)return;
        float size=Mathf.Min(area.width,area.height);var rect=new Rect(area.center.x-size*.5f,area.center.y-size*.5f,size,size);
        preview.overlay.SetFloat("_Screen",0);Graphics.DrawTexture(rect,texture,new Rect(0,0,1,1),0,0,0,0,Color.white,preview.overlay);
    }
    internal Texture RenderPreview(int id,float seconds){
        if(!Select(id))return null;
        long tick=(long)(Mathf.Max(0,seconds)*60);
        if(previewTick<0||tick<previewTick||tick-previewTick>12){motion.Reset();previewTick=Math.Max(-1,tick-90);}
        for(long sample=previewTick+1;sample<=tick;++sample){
            float t=sample/60f;
            // A repeatable demonstration of turns, acceleration and road bumps
            // drives the same solver as gameplay; no mesh-specific sine sway.
            var position=new Vector3(1.8f*Mathf.Sin(t*1.3f),.09f*Mathf.Sin(t*5)+.025f*Mathf.Sin(t*9),20*t+1.8f*Mathf.Sin(t*1.5f));
            motion.Sample(position,0,(ulong)sample,true,false);
        }
        previewTick=tick;return RenderInterpolatedMotion(id,motion,Mathf.Repeat(Mathf.Max(0,seconds)*60,1));
    }
    public static void ReleasePreview(){preview?.Dispose();preview=null;}
    public void Dispose(){
        if(modelCommands!=null){camera?.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque,modelCommands);modelCommands.Dispose();modelCommands=null;}
        ReleaseSelection();if(camera)camera.targetTexture=null;if(target)target.Release();
        DestroyOwned(target);DestroyOwned(screenQuad);DestroyOwned(overlay);DestroyOwned(cameraObject);
        target=null;screenQuad=null;overlay=null;cameraObject=null;camera=null;overlayProperties=null;
    }
}
