using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using UnityEditor;
using UnityEngine;

// Production render/bridge checks using a synthetic, stationary recording in
// isolated replay storage. No player saves or ROM files are accessed.
public static class Idas3ReplayPresentationChecks {
    const BindingFlags Hidden=BindingFlags.Instance|BindingFlags.NonPublic|BindingFlags.Public;
    static string Output=Path.GetFullPath("Verification/paint-replay-20261007");
    static bool nightBattleChecks;
    public static void RunNightBattleFixes(){Output=Path.GetFullPath("Verification/night-battle-freecam-20261008");nightBattleChecks=true;Run();}
    static int checks;
    static void Check(bool ok,string label){if(!ok)throw new Exception(label);checks++;}
    static object Get(object o,string name)=>o.GetType().GetField(name,Hidden).GetValue(o);
    static void Set(object o,string name,object value)=>o.GetType().GetField(name,Hidden).SetValue(o,value);
    static object Call(object o,string name,params object[] args)=>o.GetType().GetMethod(name,Hidden).Invoke(o,args);
    [StructLayout(LayoutKind.Sequential,Pack=8)] struct Input {
        public uint size,flags;public double delta;public uint k0,k1,k2,k3,k4,k5,k6,k7,buttons;
        public int lx,ly,rx,ry;public uint lt,rt,connected;public int width,height;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneInitialize([MarshalAs(UnmanagedType.LPUTF8Str)]string assets,[MarshalAs(UnmanagedType.LPUTF8Str)]string saves,int width,int height);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneStep(ref Input input);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneShutdown();
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneModeFlowFixture(int scene);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3ReplayStart(int condition,int weather,int night,int car,int manual);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneSetSunGlare(int enabled);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3ReplayAppearance(uint[] values,int count);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3ReplayOpponentStart(int car,int enemy,uint[] values,int count);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3ReplayFreeCamera(float x,float y,float z,float tx,float ty,float tz);
    static Idas3ReplayData Fixture(Vector3 p,float yaw,int color=-1){
        using var stream=new MemoryStream();using var writer=new BinaryWriter(stream);
        var json=Encoding.UTF8.GetBytes("{\"condition\":0,\"weather\":0,\"night\":0,\"car\":0,\"manual\":1,\"ticks6000\":60000}");
        writer.Write(json.Length);writer.Write(json);writer.Write(color<0?0x31524449u:0x32524449u);writer.Write(60000u);writer.Write(601u);writer.Write(60u);
        if(color>=0){writer.Write(96u);writer.Write(160u);writer.Write(1u);writer.Write(12u);for(int i=0;i<16;i++)writer.Write(i==1?(uint)color:0u);}
        for(uint i=0;i<=600;i++){
            writer.Write(i);writer.Write(p.x);writer.Write(p.y);writer.Write(p.z);writer.Write(yaw);writer.Write(0f);writer.Write(1);
            if(color>=0)for(int n=0;n<33;n++)writer.Write(n==17?i*100:n==24?4u:0u);
        }
        writer.Flush();return Idas3ReplayData.Parse(stream.ToArray());
    }
    static Color32[] Shot(Camera camera,Idas3UnityUi ui,string name){
        var target=new RenderTexture(1280,720,24){antiAliasing=4};target.Create();
        var old=RenderTexture.active;var oldTarget=camera.targetTexture;var image=new Texture2D(1280,720,TextureFormat.RGB24,false);
        try{
            camera.targetTexture=target;camera.Render();ui.RenderOverlayForCapture();RenderTexture.active=target;
            image.ReadPixels(new Rect(0,0,1280,720),0,0);image.Apply();
            File.WriteAllBytes(Path.Combine(Output,name+".png"),image.EncodeToPNG());return image.GetPixels32();
        }finally{camera.targetTexture=oldTarget;RenderTexture.active=old;target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(image);}
    }
    static void FreeCameraDirections(Idas3ReplayViewer viewer,Idas3SceneRenderer renderer,Camera camera){
        var position=(Vector3)Get(viewer,"freePosition");float yaw=(float)Get(viewer,"freeYaw"),pitch=(float)Get(viewer,"freePitch");
        foreach(float heading in new[]{0f,90f,180f,270f})foreach(float tilt in new[]{-35f,0f,35f}){
            Action reset=()=>{Set(viewer,"freePosition",position);Set(viewer,"freeYaw",heading);Set(viewer,"freePitch",tilt);Call(viewer,"Present");};
            reset();var frame=renderer.CurrentFrame.mainCamera;var forward=(frame.target-frame.eye).normalized;
            var right=Vector3.Cross(forward,frame.up).normalized;
            var marker=frame.eye+forward*30;var center=camera.WorldToViewportPoint(marker);
            Check(camera.WorldToViewportPoint(marker+right).x>center.x,"Native screen-right projection changed");
            foreach(float side in new[]{-1f,1f}){
                reset();Call(viewer,"ApplyFreeCameraInput",new Vector3(side,0,0),Vector2.zero,2f);Call(viewer,"Present");
                Check(Vector3.Dot(renderer.CurrentFrame.mainCamera.eye-position,right)*side>1.9f,"A/D or left stick moved in the wrong screen direction");
                Check((camera.WorldToViewportPoint(marker).x-center.x)*side<0,"Freecam strafe did not move scenery oppositely on screen");
                reset();Call(viewer,"ApplyFreeCameraInput",Vector3.zero,new Vector2(side*5,0),0f);Call(viewer,"Present");
                Check((camera.WorldToViewportPoint(marker).x-center.x)*side<0,"Mouse/right-stick horizontal look is reversed");
            }
            reset();Call(viewer,"ApplyFreeCameraInput",Vector3.forward,Vector2.zero,2f);Call(viewer,"Present");
            Check(Vector3.Dot(renderer.CurrentFrame.mainCamera.eye-position,forward)>1.9f,"Forward movement regressed");
            reset();Call(viewer,"ApplyFreeCameraInput",Vector3.zero,new Vector2(0,5),0f);Call(viewer,"Present");
            Check(camera.WorldToViewportPoint(marker).y<center.y,"Vertical look regressed");
        }
        foreach(int fps in new[]{30,60,144}){
            Set(viewer,"freePosition",position);Set(viewer,"freeYaw",0f);Set(viewer,"freePitch",0f);
            for(int i=0;i<fps;++i)Call(viewer,"ApplyFreeCameraInput",Vector3.right,Vector2.zero,12f/fps);
            Check(Vector3.Distance((Vector3)Get(viewer,"freePosition"),position+Vector3.left*12)<.01f,"Strafing speed depends on FPS");
            for(int i=0;i<fps;++i)Call(viewer,"ApplyFreeCameraInput",Vector3.zero,new Vector2(100f/fps,0),0f);
            Check(Mathf.Abs(Mathf.DeltaAngle((float)Get(viewer,"freeYaw"),260))<.01f,"Controller look speed depends on FPS");
        }
        Set(viewer,"freePosition",position);Set(viewer,"freeYaw",yaw);Set(viewer,"freePitch",pitch);Call(viewer,"Present");
    }
    public static void Run(){
        Directory.CreateDirectory(Output);ShaderUtil.allowAsyncCompilation=false;
        Idas3ControllerMenuChecks.Run();
        string saves=Path.Combine(Output,nightBattleChecks?"userdata":"replay-viewer-session");Directory.CreateDirectory(saves);
        if(nightBattleChecks)File.WriteAllText(Path.Combine(Output,"ISOLATED_MODE_FLOW_TEST.txt"),"Private night battle presentation fixture");
        Check(Idas3SceneInitialize(Path.GetFullPath("Native"),saves,1280,720)==1,"Scene initialize");
        var go=new GameObject("Private replay presentation check");var camera=go.AddComponent<Camera>();
        var renderer=go.AddComponent<Idas3SceneRenderer>();renderer.Initialize(camera);
        var ui=go.AddComponent<Idas3UnityUi>();ui.Initialize(camera);
        var viewer=go.AddComponent<Idas3ReplayViewer>();viewer.enabled=false;
        Set(viewer,"scene",renderer);Set(viewer,"ui",ui);Set(viewer,"<View>k__BackingField",camera);
        try{
            if(nightBattleChecks){
                Check(Idas3SceneModeFlowFixture(-17)==1,"Night battle app checks failed; see private report");
                // Start a fresh viewer owner after the authority fixture, just
                // as the actual replay viewer runs in a separate process.
                Check(Idas3SceneShutdown()==1,"Night battle fixture shutdown");
                Check(Idas3SceneInitialize(Path.GetFullPath("Native"),Path.Combine(Output,"replay-viewer-session"),1280,720)==1,"Replay fixture initialization");
            }
            Check(Idas3ReplayStart(0,0,0,0,1)==1,"Replay start");
            var input=new Input{size=88,flags=1,width=1280,height=720};Check(Idas3SceneStep(ref input)==1,"Initial replay render");renderer.ApplyFrame();
            var c=renderer.CurrentFrame.mainCamera;var forward=(c.target-c.eye);forward.y=0;forward.Normalize();
            var p=c.eye+forward*7-Vector3.up*3;float yaw=Mathf.Atan2(forward.x,forward.z);
            var replay=Fixture(p,yaw);Set(viewer,"replay",replay);Set(viewer,"seconds",5d);Call(viewer,"Present");
            var initial=renderer.CurrentFrame.mainCamera;
            Shot(camera,ui,"chase-hud");
            for(int i=0;i<5;i++){Call(viewer,"CycleCamera");Call(viewer,"Present");Check((int)Get(viewer,"cameraMode")==((i+1)%5),"Camera cycle missed freecam or existing view");}
            Set(viewer,"cameraMode",4);Call(viewer,"ResetFreeCamera");Call(viewer,"Present");
            var free=(Vector3)Get(viewer,"freePosition");Check(Vector3.Distance(free,renderer.CurrentFrame.mainCamera.eye)<.001f,"Free camera did not reach native rendering");
            FreeCameraDirections(viewer,renderer,camera);
            Set(viewer,"freePosition",free+new Vector3(9,8,6));Set(viewer,"freeYaw",(float)Get(viewer,"freeYaw")+30);Call(viewer,"Present");
            Check(Vector3.Distance(free+new Vector3(9,8,6),renderer.CurrentFrame.mainCamera.eye)<.001f,"Free camera movement ignored");
            Shot(camera,ui,"free-camera-hud");
            var moved=renderer.CurrentFrame.mainCamera.eye;Call(viewer,"Seek",8d);Call(viewer,"Present");
            Check(Vector3.Distance(moved,renderer.CurrentFrame.mainCamera.eye)<.001f,"Seeking dragged the free camera with the car");
            Check(Idas3ReplayFreeCamera(float.NaN,0,0,0,0,1)==0&&Idas3ReplayFreeCamera(0,0,0,0,0,0)==0,"Invalid free camera accepted");
            Call(viewer,"Present");Check(Vector3.Distance(moved,renderer.CurrentFrame.mainCamera.eye)<.001f,"Invalid camera call changed last valid pose");
            var uiCamera=(Camera)Get(ui,"foregroundCamera");
            Set(viewer,"gameHudVisible",false);Call(viewer,"Present");
            Check(!uiCamera.enabled&&!renderer.MirrorCamera.enabled&&(bool)Get(viewer,"controlsVisible"),"Game HUD hide changed replay overlay or left HUD visible");
            Shot(camera,ui,"free-camera-clean");
            Set(viewer,"controlsVisible",false);Set(viewer,"gameHudVisible",true);Call(viewer,"Present");
            Check(uiCamera.enabled&&!(bool)Get(viewer,"controlsVisible"),"Replay overlay and game HUD are coupled");
            Set(viewer,"controlsVisible",true);Set(viewer,"cameraMode",0);Call(viewer,"Present");
            Check(Vector3.Distance(initial.eye,renderer.CurrentFrame.mainCamera.eye)<.001f,"Exiting free camera failed to restore chase");
            foreach(int paint in new[]{0,7,16,92})Check(Fixture(p,yaw,paint).Appearance[1]==paint,"Replay parser lost expanded paint");
            bool rejected=false;try{Fixture(p,yaw,93);}catch(InvalidDataException){rejected=true;}Check(rejected,"Replay parser accepted paint 93");
            var appearance=new uint[15];appearance[1]=92;
            Check(Idas3ReplayAppearance(appearance,15)==1,"Expanded player paint rejected by replay bridge");
            Check(Idas3ReplayOpponentStart(8,-1,appearance,15)==1,"Expanded opponent paint rejected by replay bridge");
            appearance[1]=93;Check(Idas3ReplayAppearance(appearance,15)==0,"Invalid player paint accepted");
            // Restore a legacy replay so recorded detail does not override its fixture pose.
            Check(Idas3ReplayStart(0,0,0,0,1)==1,"Sun fixture replay start");
            Set(viewer,"cameraMode",4);Set(viewer,"freePosition",p+Vector3.up*3);
            using(var r=new BinaryReader(File.OpenRead("Native/data/original_assets/environment/environment.bin"))){
                r.ReadBytes(8);var sun=new Vector3(r.ReadSingle(),r.ReadSingle()-p.y-3,r.ReadSingle()).normalized;
                Set(viewer,"freeYaw",Mathf.Atan2(sun.x,sun.z)*Mathf.Rad2Deg);Set(viewer,"freePitch",-Mathf.Asin(sun.y)*Mathf.Rad2Deg);
            }
            Check(Idas3SceneSetSunGlare(1)==1,"Enable glare");Call(viewer,"Present");var on=renderer.CurrentFrame.rangeCount;var onImage=Shot(camera,ui,"sun-glare-on");
            Check(Idas3SceneSetSunGlare(0)==1,"Disable glare");Call(viewer,"Present");var off=renderer.CurrentFrame.rangeCount;var offImage=Shot(camera,ui,"sun-glare-off");
            Check(on>off,"Sun glare toggle did not remove flare geometry");
            Check(onImage.Zip(offImage,(a,b)=>a.Equals(b)?0:1).Sum()>100,"Sun glare toggle made no visible difference");
            Check(Idas3SceneSetSunGlare(2)==0,"Invalid glare setting accepted");
            Check(Idas3SceneSetSunGlare(1)==1,"Restore glare");Call(viewer,"Present");Check(renderer.CurrentFrame.rangeCount==on,"Restoring glare changed unrelated geometry");
            File.WriteAllText(Path.Combine(Output,"presentation-PASS.txt"),"PASS "+checks+" checks: existing/free cameras, seek isolation, independent HUD/overlay, shared replay paints, sun flare geometry and actual renders.\n");
            Debug.Log("REPLAY_PRESENTATION_PASS "+checks);
        }finally{
            Idas3SceneShutdown();go.SetActive(false);((Transform)Get(renderer,"worldRoot")).gameObject.SetActive(false);
            foreach(var item in (System.Collections.IEnumerable)Get(renderer,"objects")){
                var field=item.GetType().GetField("vertexCache",Hidden);var cache=field.GetValue(item);
                if((bool)cache.GetType().GetProperty("IsCreated").GetValue(cache))((IDisposable)cache).Dispose();field.SetValue(item,Activator.CreateInstance(field.FieldType));
            }
            foreach(string name in new[]{"framesBuffer","lightsBuffer","fogBuffer"}){((ComputeBuffer)Get(renderer,name)).Release();Set(renderer,name,null);}
        }
    }
}
