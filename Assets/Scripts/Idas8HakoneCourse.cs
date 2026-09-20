using System;
using System.IO;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.InputSystem;

// Hakone geometry/lighting only. Idas3SceneGame owns the complete race.
public sealed partial class Idas8HakoneCourse : MonoBehaviour
{
    public bool testBuild;
    [Serializable] public class Tex { public string file; public int uv, type; }
    [Serializable] public class Surface { public string name,kind; public Tex[] textures; public float cutoff; public bool shadow, sky; }
    [Serializable] public class Placement { public string kind; public int[] meshes; public float[] position,forward,angles; public float scale; }
    [Serializable] public class Lighting { public string name; public float[] sunDirection,fogColor; public float distanceScale; }
    [Serializable] public class LightingPoint { public float point; public int profile; }
    [Serializable] public class LightingEvents { public LightingPoint[] points; }
    [Serializable] public class Manifest { public Surface[] materials; public Placement[] instances; public Lighting[] lighting; public LightingEvents[] lightingEvents; public int[] checkpoints, times; public int pathPoints, shapes, triangles; public string source; }
    int lightingProfile=-1,foliageSamples=-1;
    sealed class Scenery { public Placement source; public MeshFilter filter; public MeshRenderer renderer; public Transform transform; public Vector3 position; public bool tree; public int lod=-2; public Quaternion authoredRotation; }
    readonly List<Scenery> scenery=new List<Scenery>(); Mesh[] sourceMeshes; int[] sourceMaterials;
    Manifest data; Vector3[][] roads; Material[] materials;
    readonly Dictionary<string,Texture2D> textures=new Dictionary<string,Texture2D>();
    readonly List<Transform> skies=new List<Transform>();
    Camera view; string root,variant; bool reverse,visible=true; Idas3SceneGame host;
    uint SceneFlags => Idas3ReplayViewer.Instance != null ? Idas3ReplayViewer.Instance.Status.flags : host.Status.flags;
    public string LoadedVariant => variant;
    public int PairedTreeTriangles {get;private set;}
    public string LoadedCourse {get;private set;}
    public static string CourseName(uint flags)=>(flags&524288u)!=0?"SADAMINE":"HAKONE";
    public static string Variant(uint flags) => ((flags&65536u)!=0?"night":"day")+((flags&131072u)!=0?"_wet":"_dry");
    void Start() {
        try {
            host=FindAnyObjectByType<Idas3SceneGame>();view=Idas3ReplayViewer.Instance != null ? Idas3ReplayViewer.Instance.View : host.GetComponent<Camera>();
            if((SceneFlags&16384u)!=0)LoadVariant(Variant(SceneFlags));
            if(testBuild&&(Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-smoke")>=0||Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-menu-smoke")>=0))gameObject.AddComponent<Idas8HakoneRaceSmoke>();
        } catch(Exception e) {Debug.LogException(e);Application.Quit(1);}
    }
    void LateUpdate(){
        if(view==null)return;
        bool active=(SceneFlags&16384u)!=0&&(SceneFlags&(1u|4096u|262144u))==0;
        if(active!=visible){foreach(Transform child in transform)child.gameObject.SetActive(active);visible=active;}
        if(!active)return;
        string wanted=Variant(SceneFlags);
        if(wanted!=variant||LoadedCourse!=CourseName(SceneFlags)){
            try{LoadVariant(wanted);}catch(Exception e){Debug.LogException(e);enabled=false;Application.Quit(1);return;}
        }
        bool backwards=(SceneFlags&32768u)!=0;if(backwards!=reverse){reverse=backwards;lightingProfile=-1;}
        var cameraPosition=view.transform.position;
        foreach(var sky in skies)sky.position=cameraPosition;
        int nearest=0;float best=float.MaxValue;
        for(int i=0;i<roads[0].Length;i+=8){float d=(roads[0][i]-cameraPosition).sqrMagnitude;if(d<best){best=d;nearest=i;}}
        UpdateLighting(nearest);UpdateFoliageAntialiasing();UpdateScenery(cameraPosition);
    }
    void LoadVariant(string selected){
        ClearScene();
        LoadedCourse=CourseName(SceneFlags);root=Path.Combine(Application.streamingAssetsPath,LoadedCourse);
        if(selected!="day_dry")root=Path.Combine(root,selected);
        data=JsonUtility.FromJson<Manifest>(File.ReadAllText(Path.Combine(root,"scene.json")));
        LoadRoad();LoadScene();
        foreach(Transform child in GetComponentsInChildren<Transform>())child.gameObject.layer=28;
        lightingProfile=-1;foliageSamples=-1;variant=selected;
        Debug.Log("HAKONE condition loaded: "+selected+" / "+data.source);
    }
    void ClearScene(){
        foreach(Transform child in transform){child.gameObject.SetActive(false);Destroy(child.gameObject);}
        if(sourceMeshes!=null)foreach(var mesh in sourceMeshes)Destroy(mesh);
        if(materials!=null)foreach(var material in materials)Destroy(material);
        foreach(var texture in textures.Values)Destroy(texture);
        PairedTreeTriangles=0;textures.Clear();scenery.Clear();skies.Clear();sourceMeshes=null;materials=null;
    }
    void OnDestroy(){ClearScene();}
    static void Magic(BinaryReader r,string expected) {
        if(System.Text.Encoding.ASCII.GetString(r.ReadBytes(4))!=expected) throw new InvalidDataException(expected);
    }
    static Vector3 Vec(BinaryReader r) => new Vector3(r.ReadSingle(),r.ReadSingle(),r.ReadSingle());
    void LoadRoad() {
        using(var r=new BinaryReader(File.OpenRead(Path.Combine(root,"road.bin")))) {
            Magic(r,"HKR1"); int count=r.ReadInt32(); if(count!=data.pathPoints || count<2) throw new InvalidDataException("Road count");
            roads=new Vector3[3][];
            for(int side=0;side<3;side++) { roads[side]=new Vector3[count]; for(int i=0;i<count;i++) roads[side][i]=Vec(r); }
        }
    }
    Texture2D Texture(string name) {
        if(textures.TryGetValue(name,out var cached)) return cached;
        byte[] bytes=File.ReadAllBytes(Path.Combine(root,Path.GetFileName(name)));
        if(bytes.Length<128 || System.Text.Encoding.ASCII.GetString(bytes,0,4)!="DDS ") throw new InvalidDataException(name);
        int height=BitConverter.ToInt32(bytes,12),width=BitConverter.ToInt32(bytes,16);
        string fourcc=System.Text.Encoding.ASCII.GetString(bytes,84,4);
        bool alphaOnly=BitConverter.ToUInt32(bytes,80)==2&&BitConverter.ToInt32(bytes,88)==8&&BitConverter.ToUInt32(bytes,104)==255;
        if(fourcc!="DXT1" && fourcc!="DXT5"&&!alphaOnly) throw new InvalidDataException("Unsupported DDS: "+name+" / "+fourcc);
        // Preserve source mipmaps. A8 shadow masks carry opacity only; expand
        // to black RGBA so both projected and baked shadows share the shader.
        int mipCount=Math.Max(1,BitConverter.ToInt32(bytes,28));
        var t=new Texture2D(width,height,alphaOnly?TextureFormat.RGBA32:fourcc=="DXT1"?TextureFormat.DXT1:TextureFormat.DXT5,mipCount,false);
        byte[] payload=new byte[(bytes.Length-128)*(alphaOnly?4:1)];
        if(alphaOnly){for(int i=128;i<bytes.Length;i++)payload[(i-128)*4+3]=bytes[i];}
        else Buffer.BlockCopy(bytes,128,payload,0,payload.Length);
        t.LoadRawTextureData(payload); t.Apply(false,true); t.name=name; t.anisoLevel=8; t.filterMode=FilterMode.Trilinear;
        textures.Add(name,t); return t;
    }
    void LoadScene() {
        var shader=Resources.Load<Shader>("Idas3Scene"); if(shader==null || !shader.isSupported) throw new InvalidOperationException("Shared scene shader missing or unsupported");
        materials=new Material[data.materials.Length];
        for(int i=0;i<materials.Length;i++) {
            var s=data.materials[i]; var m=new Material(shader){name=s.name}; materials[i]=m;
            m.EnableKeyword("IDAS_IMPORTED_COURSE");
            // Repeated cutout trees share meshes/materials. Instance their
            // world transforms; blended road shadows retain their draw order.
            m.enableInstancing=s.kind=="tree"&&Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-imported-instancing-off")<0;
            // The existing D3 scene starts at queue 1000. Imported ground must
            // precede its projected headlights, contact shadows, cars and rain.
            m.renderQueue=900;
            m.SetFloat("_ImportedNight",(SceneFlags&65536u)!=0?1:0);
            m.SetFloat("_ImportedCutoff",s.kind=="gallery"?Mathf.Max(s.cutoff,.3f):s.cutoff);
            m.SetFloat("_ImportedSky",s.sky?1:0);
            foreach(var t in s.textures) {
                if(t.type==1 || s.textures.Length==1) m.mainTexture=Texture(t.file);
                else if(t.type==6) { m.SetTexture("_ImportedShadowTex",Texture(t.file)); m.SetFloat("_ImportedHasShadow",1); }
            }
            if(s.shadow||(s.textures.Length==1&&s.textures[0].type==6)) { m.SetFloat("_SrcBlend",(float)BlendMode.SrcAlpha); m.SetFloat("_DstBlend",(float)BlendMode.OneMinusSrcAlpha); m.SetFloat("_ZWrite",0); m.renderQueue=950; }
            if(s.sky) { m.SetFloat("_ZWrite",0); m.renderQueue=800; }
        }
        using(var r=new BinaryReader(File.OpenRead(Path.Combine(root,"scene.bin")))) {
            Magic(r,"HKN2"); int count=r.ReadInt32(); if(count!=data.shapes) throw new InvalidDataException("Shape count");
            sourceMeshes=new Mesh[count]; sourceMaterials=new int[count];
            for(int shape=0;shape<count;shape++) {
                int mat=r.ReadInt32(),nv=r.ReadInt32(),nt=r.ReadInt32();
                if(mat<0||mat>=materials.Length||nv<0||nv>2000000||nt<0||nt>6000000) throw new InvalidDataException("Mesh header");
                Matrix4x4 matrix=Matrix4x4.identity; for(int row=0;row<3;row++) for(int col=0;col<4;col++) matrix[row,col]=r.ReadSingle();
                var vertices=new Vector3[nv]; var normals=new Vector3[nv]; var uv=new Vector2[nv]; var uv2=new Vector2[nv]; var colors=new Color32[nv];
                for(int i=0;i<nv;i++) {
                    vertices[i]=matrix.MultiplyPoint3x4(Vec(r)); normals[i]=matrix.MultiplyVector(Vec(r)); uv[i]=new Vector2(r.ReadSingle(),r.ReadSingle()); uv2[i]=new Vector2(r.ReadSingle(),r.ReadSingle());
                    colors[i]=new Color32(r.ReadByte(),r.ReadByte(),r.ReadByte(),r.ReadByte());
                }
                int[] indices=new int[nt]; for(int i=0;i<nt;i++) { indices[i]=r.ReadInt32(); if(indices[i]<0||indices[i]>=nv) throw new InvalidDataException("Vertex index"); }
                Vector2[] treeFaces=null;
                if(data.materials[mat].kind=="tree"){
                    treeFaces=PrepareTreeFaces(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices,out int paired);
                    PairedTreeTriangles+=paired;
                }
                var mesh=new Mesh{name="Hakone original shape "+shape,indexFormat=IndexFormat.UInt32};
                mesh.vertices=vertices; mesh.normals=normals; mesh.uv=uv; mesh.uv2=uv2; mesh.colors32=colors; if(treeFaces!=null)mesh.uv3=treeFaces; mesh.triangles=indices; mesh.RecalculateBounds(); mesh.UploadMeshData(true);
                sourceMeshes[shape]=mesh; sourceMaterials[shape]=mat;
                if(data.materials[mat].kind=="tree" || data.materials[mat].kind=="gallery") continue;
                var go=new GameObject(materials[mat].name+" "+shape); go.transform.SetParent(transform,false);
                go.AddComponent<MeshFilter>().sharedMesh=mesh;
                var renderer=go.AddComponent<MeshRenderer>(); renderer.sharedMaterial=materials[mat]; renderer.shadowCastingMode=ShadowCastingMode.Off; renderer.receiveShadows=false;
                if(data.materials[mat].sky) { skies.Add(go.transform); go.transform.localScale=Vector3.one*2000; }
            }
            if(r.BaseStream.Position!=r.BaseStream.Length) throw new InvalidDataException("Trailing scene data");
        }
        foreach(var p in data.instances) {
            if(p.position.Length!=3||p.forward.Length!=3||p.angles.Length!=3||p.scale<=0||p.scale>10) throw new InvalidDataException("Scenery placement");
            foreach(int mesh in p.meshes) if(mesh<0||mesh>=sourceMeshes.Length) throw new InvalidDataException("Scenery mesh reference");
            var go=new GameObject("Hakone "+p.kind); go.transform.SetParent(transform,false);
            go.transform.position=new Vector3(p.position[0],p.position[1],p.position[2]);
            // Source placement direction and extra angles are retained. Exact
            // Stage 8 billboard/rotation conventions still require comparison.
            var forward=new Vector3(p.forward[0],p.forward[1],p.forward[2]);
            go.transform.rotation=(forward.sqrMagnitude>.01f?Quaternion.LookRotation(forward,Vector3.up):Quaternion.identity)*Quaternion.Euler(p.angles[0],p.angles[1],p.angles[2]);
            go.transform.localScale=Vector3.one*p.scale;
            // Placement positions never animate. Cache them and the transform
            // once; only distant cards and spectators change their rotation.
            var item=new Scenery{source=p,filter=go.AddComponent<MeshFilter>(),renderer=go.AddComponent<MeshRenderer>(),transform=go.transform,position=go.transform.position,tree=p.kind=="tree",authoredRotation=go.transform.rotation};
            item.renderer.shadowCastingMode=ShadowCastingMode.Off; item.renderer.receiveShadows=false; scenery.Add(item);
        }
        Debug.Log("HAKONE scenery placements: "+scenery.Count);
    }
    void UpdateLighting(float pathPoint) {
        int selected=0;
        foreach(var e in data.lightingEvents[reverse?1:0].points) if(pathPoint>=e.point) selected=e.profile;
        if(selected==lightingProfile)return;
        lightingProfile=selected; var profile=data.lighting[selected];
        var sun=new Vector4(-profile.sunDirection[0],-profile.sunDirection[1],-profile.sunDirection[2],0);
        var fog=new Color(profile.fogColor[0],profile.fogColor[1],profile.fogColor[2],1);
        // Stage 8 scattering is mapped to the main renderer's native atmosphere
        // curve; this is not an implementation of Stage 8's Mie/Rayleigh shader.
        foreach(var m in materials) {
            m.SetVector("_ImportedSunDirection",sun); m.SetColor("_ImportedFogColor",fog);
            m.SetVector("_ImportedFogRange",new Vector4(85,1/Mathf.Max(.00001f,profile.distanceScale),0,0));
        }
    }
    static int ComparePosition(Vector3 a,Vector3 b){int n=a.x.CompareTo(b.x);if(n==0)n=a.y.CompareTo(b.y);return n!=0?n:a.z.CompareTo(b.z);}
    static (Vector3,Vector3,Vector3) FaceKey(Vector3 a,Vector3 b,Vector3 c,out int side){
        int swaps=0;
        if(ComparePosition(a,b)>0){var t=a;a=b;b=t;swaps++;}
        if(ComparePosition(b,c)>0){var t=b;b=c;c=t;swaps++;}
        if(ComparePosition(a,b)>0){var t=a;a=b;b=t;swaps++;}
        side=1<<(swaps&1);return (a,b,c);
    }
    internal static Vector2[] PrepareTreeFaces(ref Vector3[] vertices,ref Vector3[] normals,ref Vector2[] uv,ref Vector2[] uv2,ref Color32[] colors,ref int[] indices,out int paired){
        var keys=new (Vector3,Vector3,Vector3)[indices.Length/3];
        var sides=new Dictionary<(Vector3,Vector3,Vector3),int>();
        for(int i=0;i<keys.Length;i++){
            keys[i]=FaceKey(vertices[indices[3*i]],vertices[indices[3*i+1]],vertices[indices[3*i+2]],out int side);
            sides.TryGetValue(keys[i],out int mask);sides[keys[i]]=mask|side;
        }
        paired=0;foreach(var key in keys)if(sides[key]==3)paired++;
        if(paired==0)return null;
        // Some source leaves have separate front/back polygons with different
        // UVs and lighting on the exact same plane. Drawing both with Cull Off
        // fights the depth buffer. Tag just those pairs; lone cards stay two-sided.
        var points=new Vector3[indices.Length];var ns=new Vector3[indices.Length];
        var ts=new Vector2[indices.Length];var ts2=new Vector2[indices.Length];
        var cs=new Color32[indices.Length];var flags=new Vector2[indices.Length];var ix=new int[indices.Length];
        for(int i=0;i<indices.Length;i++){
            int source=indices[i];points[i]=vertices[source];ns[i]=normals[source];ts[i]=uv[source];ts2[i]=uv2[source];cs[i]=colors[source];ix[i]=i;
            flags[i]=new Vector2(sides[keys[i/3]]==3?1:0,0);
        }
        vertices=points;normals=ns;uv=ts;uv2=ts2;colors=cs;indices=ix;return flags;
    }
    void UpdateFoliageAntialiasing(){
        int samples=QualitySettings.antiAliasing;if(samples==foliageSamples)return;foliageSamples=samples;
        for(int i=0;i<materials.Length;i++){
            // Coverage smooths leaf silhouettes without transparent depth sorting.
            // Keep the authored alpha test when the player has disabled MSAA.
            bool cutout=data.materials[i].cutoff>0&&!data.materials[i].sky;
            materials[i].SetFloat("_ImportedCoverage",cutout&&samples>1?1:0);
            materials[i].SetFloat("_AlphaToMask",cutout&&samples>1?1:0);
        }
    }
    internal static int TreeLod(float distanceSquared,int previous){
        // Separate enter/exit distances prevent camera shake or corrections
        // from toggling tree meshes or visibility on consecutive frames.
        if(previous==0&&distanceSquared<286f*286f)return 0;
        if(previous==1&&distanceSquared>=234f*234f&&distanceSquared<660f*660f)return 1;
        if(previous==2&&distanceSquared>=540f*540f&&distanceSquared<1760f*1760f)return 2;
        if(previous==-1&&distanceSquared>=1440f*1440f)return -1;
        return distanceSquared<260f*260f?0:distanceSquared<600f*600f?1:distanceSquared<1600f*1600f?2:-1;
    }
    internal void VerifyTreeState(){
        if(PairedTreeTriangles<=0)throw new InvalidOperationException("Source paired leaf faces were not tagged");
        foreach(var item in scenery){
            if(item.source.kind!="tree"||item.lod<0)continue;
            if(!item.renderer.enabled||item.filter.sharedMesh!=sourceMeshes[item.source.meshes[item.lod]])throw new InvalidOperationException("Visible tree lost its LOD mesh");
            if(item.lod==2){var facing=view.transform.position-item.filter.transform.position;facing.y=0;if(facing.sqrMagnitude>.01f&&Vector3.Dot(item.filter.transform.forward,facing.normalized)<.99f)throw new InvalidOperationException("Distant tree is edge-on");}
        }
        for(int i=0;i<materials.Length;i++)if(data.materials[i].cutoff>0&&!data.materials[i].sky){
            if(materials[i].GetFloat("_AlphaToMask")!=(QualitySettings.antiAliasing>1?1:0))throw new InvalidOperationException("Foliage coverage does not follow graphics settings");
        }
    }
    internal static float SceneryDistanceSquared(float distanceSquared,int detail){
        float scale=detail==2?.4f:detail==1?.65f:1f;
        return distanceSquared/(scale*scale);
    }
    void UpdateScenery(Vector3 cameraPosition) {
        int detail=host.GameOptions?.Current.importedSceneryDetail??0;
        foreach(var item in scenery) {
            float d=SceneryDistanceSquared((item.position-cameraPosition).sqrMagnitude,detail);
            bool tree=item.tree;
            int lod=tree?TreeLod(d,item.lod):(d<500*500?0:-1);
            if(item.lod!=lod) {
                item.lod=lod; item.renderer.enabled=lod>=0;
                if(lod>=0) { int index=item.source.meshes[lod]; item.filter.sharedMesh=sourceMeshes[index]; item.renderer.sharedMaterial=materials[sourceMaterials[index]];
                    if(tree&&lod!=2)item.transform.rotation=item.authoredRotation;
                }
            }
            if(lod>=0&&(!tree||lod==2)) {
                Vector3 facing=cameraPosition-item.position; facing.y=0;
                if(facing.sqrMagnitude>.01f) item.transform.rotation=Quaternion.LookRotation(facing,Vector3.up);
            }
        }
    }
}
