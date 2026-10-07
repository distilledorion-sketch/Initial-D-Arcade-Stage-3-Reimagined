using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEngine;

// Render the production course loaders/shaders, with private settings and
// native replay storage. No player saves, ROMs, network or release files.
public static class Idas3TrackLightingChecks
{
    const BindingFlags Hidden=BindingFlags.Instance|BindingFlags.NonPublic|BindingFlags.Public;
    static readonly string Output=Path.GetFullPath("Verification/track-lighting-20261007");
    static void Check(bool ok,string message){if(!ok)throw new Exception(message);}
    static object Get(object o,string name)=>o.GetType().GetField(name,Hidden).GetValue(o);
    static void Set(object o,string name,object value)=>o.GetType().GetField(name,Hidden).SetValue(o,value);
    static object Call(object o,string name,params object[] args)=>o.GetType().GetMethod(name,Hidden).Invoke(o,args);
    static Vector3 ReadVector(BinaryReader r)=>new Vector3(r.ReadSingle(),r.ReadSingle(),r.ReadSingle());
    sealed class Platform:Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>new[]{new Idas3GameOptions.ResolutionChoice(1280,720)};
        public void Apply(Idas3GameOptions.Values a,Idas3GameOptions.Values b,bool display){}
    }
    public static void Run(){
        Directory.CreateDirectory(Output);ShaderUtil.allowAsyncCompilation=false;
        Settings();
        // Original scenes use the host focus bit even in a headless capture.
        // Capture these first, then disable their renderer callbacks.
        Original();
        foreach(string course in new[]{"HAKONE","SADAMINE"})
            foreach(string variant in new[]{"day_dry","day_wet","night_dry","night_wet"})Imported(course,variant);
        SpecialStage();
        foreach(string name in new[]{"Idas3Scene","Idas3SceneDirect","Idas3OpaqueAlphaDepth"}){
            var shader=Resources.Load<Shader>(name);Check(shader!=null&&shader.isSupported,"Unsupported shader: "+name);
            foreach(var error in ShaderUtil.GetShaderMessages(shader))Check(error.severity.ToString()!="Error",name+": "+error.message);
        }
        File.WriteAllText(Path.Combine(Output,"PASS.txt"),"Settings migration, persistence, cancel and normalization passed. Production shader captures: Akina day/night, Hakone/Sadamine dry/wet/day/night, Enna dry/wet. No shader errors.\n");
        Debug.Log("TRACK_LIGHTING_CHECKS_PASS");
    }
    static void Settings(){
        string saves=Path.Combine(Output,"settings");Directory.CreateDirectory(saves);
        File.WriteAllText(Path.Combine(saves,"game-options.json"),"{\"version\":1,\"masterVolume\":0.6,\"width\":1280,\"height\":720}");
        var o=new Idas3GameOptions(new Platform());o.Initialize(saves);
        Check(o.Current.trackLighting==1&&Mathf.Approximately(o.Current.masterVolume,.6f),"Old settings migration");
        o.BeginEdit();o.Draft.trackLighting=0;Check(o.HasUnsavedChanges,"Lighting edit not detected");
        o.BeginEdit();Check(o.Draft.trackLighting==1&&!o.HasUnsavedChanges,"Lighting cancel");
        o.Draft.trackLighting=0;Check(o.ApplyDraft(),"Lighting settings apply");
        var loaded=new Idas3GameOptions(new Platform());loaded.Initialize(saves);Check(loaded.Current.trackLighting==0,"Original mode persistence");
        Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{trackLighting=99}).trackLighting==1,"Lighting upper bound");
        Check(Idas3GameOptions.Normalize(new Idas3GameOptions.Values{trackLighting=-3}).trackLighting==0,"Lighting lower bound");
    }
    static void Status(Idas3SceneGame host,uint flags){
        var property=typeof(Idas3SceneGame).GetProperty("Status",Hidden);
        var status=Activator.CreateInstance(property.PropertyType);status.GetType().GetField("flags").SetValue(status,flags);property.SetValue(host,status);
    }
    static Camera Camera(out GameObject go){
        go=new GameObject("Private lighting capture");var camera=go.AddComponent<Camera>();camera.enabled=false;
        camera.fieldOfView=60;camera.aspect=16f/9;camera.nearClipPlane=.5f;camera.farClipPlane=12000;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;camera.cullingMask=1<<28;
        return camera;
    }
    static void Frame(Camera camera,ComputeBuffer buffer,bool night){
        var eye=camera.transform.position;
        var vp=GL.GetGPUProjectionMatrix(camera.projectionMatrix,false)*camera.worldToCameraMatrix;
        if(SystemInfo.usesReversedZBuffer)vp.SetRow(2,vp.GetRow(3)-vp.GetRow(2));vp=vp.transpose;
        var words=new Vector4[46];for(int row=0;row<4;row++)words[row]=vp.GetRow(row);
        words[4]=new Vector4(eye.x,eye.y,eye.z,1);words[5]=new Vector4(.025f,.035f,.06f,night?1:0);
        buffer.SetData(words);Shader.SetGlobalBuffer("_IdasFrameWords",buffer);
        Shader.SetGlobalInt("_IdasView",0);Shader.SetGlobalVector("_IdasDepthProjection",Vector4.zero);
    }
    static Color32[] Shot(Camera camera,string name){
        var target=new RenderTexture(1280,720,24){antiAliasing=4};target.Create();
        var old=RenderTexture.active;var oldTarget=camera.targetTexture;var image=new Texture2D(1280,720,TextureFormat.RGB24,false);
        try{
            camera.targetTexture=target;camera.Render();camera.Render();RenderTexture.active=target;
            image.ReadPixels(new Rect(0,0,1280,720),0,0);image.Apply();
            File.WriteAllBytes(Path.Combine(Output,name+".png"),image.EncodeToPNG());return image.GetPixels32();
        }finally{camera.targetTexture=oldTarget;RenderTexture.active=old;target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(image);}
    }
    static void Pair(Camera camera,string name,Idas3SceneRenderer renderer=null){
        Shader.SetGlobalFloat("_IdasTrackLighting",0);if(renderer)renderer.HudOptions.trackLighting=0;
        var original=Shot(camera,name+"-original");
        Shader.SetGlobalFloat("_IdasTrackLighting",1);if(renderer)renderer.HudOptions.trackLighting=1;
        var balanced=Shot(camera,name+"-balanced");
        int changed=0;for(int i=0;i<original.Length;i++)if(!original[i].Equals(balanced[i]))changed++;
        Check(changed>500,name+": lighting option did not affect rendered scenery");
        Debug.Log(name+": "+changed+" pixels changed");
    }
    static void Imported(string name,string variant){
        var camera=Camera(out var go);var host=go.AddComponent<Idas3SceneGame>();host.enabled=false;
        bool night=variant.StartsWith("night");Status(host,16384u|(name=="SADAMINE"?524288u:0u)|(night?65536u:0u)|(variant.EndsWith("wet")?131072u:0u));
        var scenery=new GameObject("Private imported course");var course=scenery.AddComponent<Idas8HakoneCourse>();course.enabled=false;
        string root=Path.GetFullPath(Path.Combine("RuntimeAssets",name,variant=="day_dry"?"":variant));
        Set(course,"host",host);Set(course,"view",camera);Set(course,"root",root);Set(course,"<LoadedCourse>k__BackingField",name);
        Set(course,"data",JsonUtility.FromJson<Idas8HakoneCourse.Manifest>(File.ReadAllText(Path.Combine(root,"scene.json"))));
        var buffer=new ComputeBuffer(46,16);
        try{
            Call(course,"LoadRoad");Call(course,"LoadScene");Call(course,"UpdateDirection");Call(course,"UpdateFoliageAntialiasing");
            var roads=(Vector3[][])Get(course,"roads");int p=roads[0].Length/3;
            camera.transform.position=roads[0][p]+Vector3.up*1.5f;camera.transform.LookAt(roads[0][p+16]+Vector3.up*1.5f);
            Call(course,"UpdateLighting",(float)p);Call(course,"UpdateScenery",camera.transform.position);
            foreach(var t in scenery.GetComponentsInChildren<Transform>()){
                t.gameObject.layer=28;var r=t.GetComponent<MeshRenderer>();
                if(r&&r.sharedMaterial&&r.sharedMaterial.GetFloat("_ImportedSky")!=0)t.position=camera.transform.position;
            }
            Frame(camera,buffer,night);Pair(camera,name+"-"+variant);
        }finally{scenery.SetActive(false);go.SetActive(false);buffer.Dispose();}
        // Keep the disabled objects until editor exit: the runtime loaders use
        // deferred Destroy, which is intentionally unavailable in edit mode.
    }
    static void SpecialStage(){
        var camera=Camera(out var go);var scenery=new GameObject("Private Enna course");
        var course=scenery.AddComponent<IdasSpecialStageEnnaCourse>();course.enabled=false;
        Call(course,"LoadPack",11,Path.GetFullPath("RuntimeAssets/ENNA"));
        Vector3[] road;using(var r=new BinaryReader(File.OpenRead("RuntimeAssets/ENNA/road.bin"))){r.ReadBytes(4);road=new Vector3[r.ReadInt32()];for(int i=0;i<road.Length;i++)road[i]=ReadVector(r);}
        int p=road.Length/3;camera.transform.position=road[p]+Vector3.up*1.5f;camera.transform.LookAt(road[p+16]+Vector3.up*1.5f);
        var skies=(GameObject[])Get(course,"skies");var gates=(GameObject[])Get(course,"gates");gates[1].SetActive(false);
        var materials=(System.Collections.Generic.List<Material>)Get(course,"materials");var lighting=(Array)Get(course,"weatherLighting");
        var buffer=new ComputeBuffer(46,16);
        try{
            for(int wet=0;wet<2;wet++){
                skies[0].SetActive(wet==0);skies[1].SetActive(wet==1);var light=lighting.GetValue(wet);
                var fog=(float[])Get(light,"fogColor");
                foreach(var m in materials){m.SetVector("_ImportedFogRange",new Vector4((float)Get(light,"fogStart"),(float)Get(light,"fogEnd"),0,0));m.SetVector("_ImportedFogColor",new Vector4(fog[0],fog[1],fog[2],0));}
                Frame(camera,buffer,true);Pair(camera,"ENNA-"+(wet==0?"dry":"wet"));
                foreach(var m in materials)m.SetFloat("_ImportedPs2Lighting",0);
                Shader.SetGlobalFloat("_IdasTrackLighting",0);Shot(camera,"ENNA-"+(wet==0?"dry":"wet")+"-old-fog");
                foreach(var m in materials)m.SetFloat("_ImportedPs2Lighting",1);
            }
        }finally{scenery.SetActive(false);go.SetActive(false);buffer.Dispose();}
    }
    [StructLayout(LayoutKind.Sequential,Pack=8)] struct Input {
        public uint size,flags;public double delta;public uint k0,k1,k2,k3,k4,k5,k6,k7,buttons;
        public int lx,ly,rx,ry;public uint lt,rt,connected;public int width,height;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneInitialize([MarshalAs(UnmanagedType.LPUTF8Str)]string assets,[MarshalAs(UnmanagedType.LPUTF8Str)]string saves,int width,int height);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneStep(ref Input input);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneShutdown();
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3ReplayStart(int condition,int weather,int night,int car,int manual);
    public static void Original(){
        string saves=Path.Combine(Output,"replay-viewer-session");Directory.CreateDirectory(saves);
        Check(Idas3SceneInitialize(Path.GetFullPath("Native"),saves,1280,720)==1,"Native scene initialize");
        var camera=Camera(out var go);var renderer=go.AddComponent<Idas3SceneRenderer>();renderer.Initialize(camera);
        try{
            for(int night=0;night<2;night++){
                Check(Idas3ReplayStart(6,0,night,0,0)==1,"Native Akina scene");
                var input=new Input{size=(uint)Marshal.SizeOf<Input>(),flags=1,delta=1.0/60,width=1280,height=720};
                Check(Idas3SceneStep(ref input)==1,"Native scene step");renderer.ApplyFrame();
                Debug.Log("Native lighting capture: "+renderer.ActiveMeshCount+" meshes, eye "+renderer.CurrentFrame.mainCamera.eye);
                Pair(camera,"AKINA-"+(night==0?"day":"night"),renderer);
            }
        }finally{
            Idas3SceneShutdown();go.SetActive(false);
            ((Transform)Get(renderer,"worldRoot")).gameObject.SetActive(false);
            // Edit-mode objects do not receive the runtime destruction loop.
            // Release persistent native allocations before editor shutdown.
            foreach(var item in (System.Collections.IEnumerable)Get(renderer,"objects")){
                var field=item.GetType().GetField("vertexCache",Hidden);var cache=field.GetValue(item);
                if((bool)cache.GetType().GetProperty("IsCreated").GetValue(cache))((IDisposable)cache).Dispose();
                field.SetValue(item,Activator.CreateInstance(field.FieldType));
            }
            foreach(string name in new[]{"framesBuffer","lightsBuffer","fogBuffer"}){
                ((ComputeBuffer)Get(renderer,name)).Release();Set(renderer,name,null);
            }
        }
    }
}
