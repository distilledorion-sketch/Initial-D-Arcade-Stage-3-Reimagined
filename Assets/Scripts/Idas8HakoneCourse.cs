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
    [Serializable] public class Surface { public string name,kind; public Tex[] textures; public float cutoff; public bool shadow, sky; public uint diffuse; }
    [Serializable] public class Placement { public string kind; public int[] meshes; public float[] position,forward,angles; public float scale; }
    [Serializable] public class Lighting { public string name; public float[] sunDirection,fogColor; public float distanceScale; }
    [Serializable] public class LightingPoint { public float point; public int profile; }
    [Serializable] public class LightingEvents { public LightingPoint[] points; }
    [Serializable] public class Manifest { public Surface[] materials; public Placement[] instances; public Lighting[] lighting; public LightingEvents[] lightingEvents; public int[] checkpoints, times; public int pathPoints, shapes, triangles; public string source; }
    // Improvised wet presentation of dry-only IDZero scenery (Tools/Add-IdZeroWetLook.py).
    [Serializable] public class WetLook { public string skyMaterial,skyTexture; public float[] fogColor; public float fogRange,brightness,desaturate,shadow; }
    WetLook wet;
    // IDZero area fog (.afg): keys along the path, interpolated at the camera's
    // path point. course_p.fx blends towards the colour by
    // density * (1-(1-t)^2), t = (depth-start)/(end-start).
    [Serializable] public class AreaFogKey { public float point,start,end,density; public float[] color; }
    [Serializable] public class AreaFog { public AreaFogKey[] keys; }
    AreaFog areaFog; float areaFogPoint=-1;
    int lightingProfile=-1,foliageSamples=-1;
    sealed class Scenery { public Placement source; public MeshFilter filter; public MeshRenderer renderer; public Transform transform; public Vector3 position; public bool tree; public int lod=-2; public Quaternion authoredRotation; }
    readonly List<Scenery> scenery=new List<Scenery>(); Mesh[] sourceMeshes; int[] sourceMaterials;
    Manifest data; Vector3[][] roads; Material[] materials;
    readonly Dictionary<string,Texture2D> textures=new Dictionary<string,Texture2D>();
    readonly List<Transform> skies=new List<Transform>();
    readonly List<(MeshRenderer renderer,bool uphill)> directionalScenery=new List<(MeshRenderer,bool)>();
    Camera view; string root,variant; bool reverse,visible=true; Idas3SceneGame host;
    bool idZeroGeometryBaseline;
    uint SceneFlags => Idas3ReplayViewer.Instance != null ? Idas3ReplayViewer.Instance.Status.flags : host.Status.flags;
    public string LoadedVariant => variant;
    public int PairedTreeTriangles {get;private set;}
    public int PairedRoadsideTriangles {get;private set;}
    public string LoadedCourse {get;private set;}
    public static int CourseId(uint flags)=>(flags&67108864u)!=0?17:(flags&33554432u)!=0?16:(flags&16777216u)!=0?15:(flags&524288u)!=0?10:9;
    public static string CourseName(uint flags)=>Idas3CourseCatalog.Packs[CourseId(flags)-9];
    public static string Variant(uint flags) => ((flags&65536u)!=0?"night":"day")+((flags&131072u)!=0?"_wet":"_dry");
    // Courses converted from IDZero's YABX scenery (Tools/idas_efo.py).
    static bool IdZero(string course)=>course=="GUNSAI"||course=="ODAWARA";
    void Start() {
        try {
            host=FindAnyObjectByType<Idas3SceneGame>();view=Idas3ReplayViewer.Instance != null ? Idas3ReplayViewer.Instance.View : host.GetComponent<Camera>();
            if((SceneFlags&16384u)!=0&&(SceneFlags&IdasSpecialStageEnnaCourse.SceneFlag)==0)LoadVariant(Variant(SceneFlags));
            if(testBuild&&(Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-smoke")>=0||Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-menu-smoke")>=0))gameObject.AddComponent<Idas8HakoneRaceSmoke>();
        } catch(Exception e) {Debug.LogException(e);Application.Quit(1);}
    }
    void LateUpdate(){
        if(view==null)return;
        bool active=(SceneFlags&16384u)!=0&&(SceneFlags&(1u|4096u|262144u|IdasSpecialStageEnnaCourse.SceneFlag))==0;
        if(active!=visible){foreach(Transform child in transform)child.gameObject.SetActive(active);visible=active;}
        if(!active)return;
        string wanted=Variant(SceneFlags);
        if(wanted!=variant||LoadedCourse!=CourseName(SceneFlags)){
            try{LoadVariant(wanted);}catch(Exception e){Debug.LogException(e);enabled=false;Application.Quit(1);return;}
        }
        bool backwards=(SceneFlags&32768u)!=0;if(backwards!=reverse){reverse=backwards;lightingProfile=-1;UpdateDirection();}
        var cameraPosition=view.transform.position;
        foreach(var sky in skies)sky.position=cameraPosition;
        int nearest=0;float best=float.MaxValue;
        for(int i=0;i<roads[0].Length;i+=8){float d=(roads[0][i]-cameraPosition).sqrMagnitude;if(d<best){best=d;nearest=i;}}
        if(areaFog!=null){
            // Lighting needs only the coarse point; fog density follows each one.
            for(int i=Mathf.Max(0,nearest-8);i<Mathf.Min(roads[0].Length,nearest+9);++i){float d=(roads[0][i]-cameraPosition).sqrMagnitude;if(d<best){best=d;nearest=i;}}
            UpdateAreaFog(nearest);
        }
        UpdateLighting(nearest);UpdateFoliageAntialiasing();UpdateScenery(cameraPosition);
    }
    void LoadVariant(string selected){
        ClearScene();
        LoadedCourse=CourseName(SceneFlags);root=Path.Combine(Application.streamingAssetsPath,LoadedCourse);
        // IDZero 2.20 ships only dry Gunsai and Odawara scenery. Wet races use
        // that scenery under a Stage 8 rain sky, regraded as wet.json describes,
        // with the ordinary D3 rain and wet vehicle handling.
        string sceneryVariant=IdZero(LoadedCourse)?selected.Replace("_wet","_dry"):selected;
        if(sceneryVariant!="day_dry")root=Path.Combine(root,sceneryVariant);
        wet=null;
        if(sceneryVariant!=selected&&File.Exists(Path.Combine(root,"wet.json"))){
            wet=JsonUtility.FromJson<WetLook>(File.ReadAllText(Path.Combine(root,"wet.json")));
            if(wet.fogColor==null||wet.fogColor.Length!=3||wet.fogRange<=0||wet.brightness<=0||wet.brightness>1||wet.desaturate<0||wet.desaturate>1||wet.shadow<0||wet.shadow>1)throw new InvalidDataException("Wet look");
        }
        data=JsonUtility.FromJson<Manifest>(File.ReadAllText(Path.Combine(root,"scene.json")));
        areaFog=null;areaFogPoint=-1;
        if(File.Exists(Path.Combine(root,"area-fog.json"))){
            areaFog=JsonUtility.FromJson<AreaFog>(File.ReadAllText(Path.Combine(root,"area-fog.json")));
            if(areaFog.keys==null||areaFog.keys.Length<2)throw new InvalidDataException("Area fog");
            for(int i=0;i<areaFog.keys.Length;++i){var k=areaFog.keys[i];
                if(k.color==null||k.color.Length!=3||k.end<=k.start||k.density<0||k.density>1||(i>0&&k.point<=areaFog.keys[i-1].point))throw new InvalidDataException("Area fog key");}
        }
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
        PairedTreeTriangles=PairedRoadsideTriangles=0;textures.Clear();scenery.Clear();skies.Clear();directionalScenery.Clear();sourceMeshes=null;materials=null;
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
        var t=DecodeTexture(File.ReadAllBytes(Path.Combine(root,Path.GetFileName(name))),name);
        textures.Add(name,t); return t;
    }
    internal static Texture2D DecodeTexture(byte[] bytes,string name) {
        if(bytes.Length<128 || System.Text.Encoding.ASCII.GetString(bytes,0,4)!="DDS ") throw new InvalidDataException(name);
        int height=BitConverter.ToInt32(bytes,12),width=BitConverter.ToInt32(bytes,16);
        string fourcc=System.Text.Encoding.ASCII.GetString(bytes,84,4);
        bool alphaOnly=BitConverter.ToUInt32(bytes,80)==2&&BitConverter.ToInt32(bytes,88)==8&&BitConverter.ToUInt32(bytes,104)==255;
        bool rgba=(BitConverter.ToUInt32(bytes,80)&64)!=0&&BitConverter.ToInt32(bytes,88)==32;
        // Odawara's house windows and toll-booth signs are uncompressed R5G6B5.
        bool rgb565=(BitConverter.ToUInt32(bytes,80)&64)!=0&&BitConverter.ToInt32(bytes,88)==16&&BitConverter.ToUInt32(bytes,92)==0xF800u&&
            BitConverter.ToUInt32(bytes,96)==0x7E0u&&BitConverter.ToUInt32(bytes,100)==0x1Fu;
        if(rgb565){
            int expected=128;
            for(int level=0,w=width,h=height;level<Math.Max(1,BitConverter.ToInt32(bytes,28));++level,w=Math.Max(1,w/2),h=Math.Max(1,h/2))expected+=w*h*2;
            if(bytes.Length!=expected)throw new InvalidDataException("Unsupported DDS mip chain: "+name);
        }
        if(fourcc!="DXT1" && fourcc!="DXT5"&&!alphaOnly&&!rgba&&!rgb565) throw new InvalidDataException("Unsupported DDS: "+name+" / "+fourcc);
        // Preserve source mipmaps and mask values. Type-6 shadows store light
        // visibility in alpha (white = lit), interpreted by the material shader.
        // A8 needs black RGB, just like the source DXT5 shadow atlases.
        int mipCount=Math.Max(1,BitConverter.ToInt32(bytes,28));
        var t=new Texture2D(width,height,rgb565?TextureFormat.RGB565:alphaOnly||rgba?TextureFormat.RGBA32:fourcc=="DXT1"?TextureFormat.DXT1:TextureFormat.DXT5,mipCount,false);
        byte[] payload=new byte[(bytes.Length-128)*(alphaOnly?4:1)];
        if(alphaOnly){for(int i=128;i<bytes.Length;i++)payload[(i-128)*4+3]=bytes[i];}
        else if(rgba){
            for(int channel=0;channel<4;++channel){
                uint mask=BitConverter.ToUInt32(bytes,92+channel*4);int shift=0;
                if(mask!=0){while(((mask>>shift)&1)==0)++shift;if((mask>>shift)!=255)throw new InvalidDataException("Unsupported DDS channel mask: "+name);}
                for(int i=128;i<bytes.Length;i+=4)payload[i-128+channel]=mask==0?(byte)255:(byte)((BitConverter.ToUInt32(bytes,i)&mask)>>shift);
            }
        }
        else Buffer.BlockCopy(bytes,128,payload,0,payload.Length);
        t.LoadRawTextureData(payload); t.Apply(false,true); t.name=name; t.anisoLevel=8; t.filterMode=FilterMode.Trilinear;
        return t;
    }
    void LoadScene() {
        // Keep a matched legacy path only for the isolated performance fixture.
        var args=Environment.GetCommandLineArgs();
        idZeroGeometryBaseline=Array.IndexOf(args,"-idas3-scene-smoke")>=0&&Array.IndexOf(args,"-idas3-idzero-geometry-baseline")>=0;
        bool direct=IdZero(LoadedCourse)&&!idZeroGeometryBaseline;
        bool packTrees=direct&&!(Array.IndexOf(args,"-idas3-scene-smoke")>=0&&Array.IndexOf(args,"-idas3-tree-packing-baseline")>=0);
        long treeVerticesBefore=0,treeVerticesAfter=0,treeIndexBytesBefore=0,treeIndexBytesAfter=0;
        var shader=Resources.Load<Shader>("Idas3Scene");
        var directShader=direct?Resources.Load<Shader>("Idas3SceneDirect"):shader;
        if(shader==null||!shader.isSupported||directShader==null||!directShader.isSupported)throw new InvalidOperationException("Shared scene shader missing or unsupported");
        bool roadsideBaseline=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-roadside-foliage-baseline")>=0&&
            Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-sadamine-boundary-check")>=0;
        materials=new Material[data.materials.Length];
        for(int i=0;i<materials.Length;i++) {
            var s=data.materials[i]; var m=new Material(directShader){name=s.name}; materials[i]=m;
            m.EnableKeyword("IDAS_IMPORTED_COURSE");
            if(direct)m.EnableKeyword("IDAS_IMPORTED_VERTEX_FACES");
            m.SetFloat("_ImportedSponsorSigns",(LoadedCourse=="TSUBAKI"?IsTsubakiSignAtlas(s):IsSponsorMaterial(LoadedCourse,s.name))?1:0);
            // Repeated cutout trees share meshes/materials. Instance their
            // world transforms; blended road shadows retain their draw order.
            m.enableInstancing=s.kind=="tree"&&Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-imported-instancing-off")<0;
            // The existing D3 scene starts at queue 1000. Imported ground must
            // precede its projected headlights, contact shadows, cars and rain.
            m.renderQueue=900;
            m.SetFloat("_ImportedNight",(SceneFlags&65536u)!=0?1:0);
            m.SetFloat("_ImportedCutoff",s.kind=="gallery"?Mathf.Max(s.cutoff,.3f):s.cutoff);
            m.SetFloat("_ImportedSky",s.sky?1:0);
            m.SetFloat("_ImportedAuthoredNormals",IdZero(LoadedCourse)?1:0);
            foreach(var t in s.textures) {
                if(t.type==1 || s.textures.Length==1) m.mainTexture=Texture(t.file);
                else if(t.type==6) { m.SetTexture("_ImportedShadowTex",Texture(t.file)); m.SetFloat("_ImportedHasShadow",1); m.SetFloat("_ImportedShadowUv",t.uv); }
            }
            m.SetFloat("_ImportedShadowOnly",s.textures.Length==1&&s.textures[0].type==6?1:0);
            m.SetFloat("_TrackSurface",!s.sky&&!s.shadow&&!(s.textures.Length==1&&s.textures[0].type==6)?1:0);
            if(wet!=null){
                // An overcast sky casts no hard shadows; both values are strengths.
                m.SetFloat("_ImportedHasShadow",m.GetFloat("_ImportedHasShadow")*wet.shadow);
                m.SetFloat("_ImportedShadowOnly",m.GetFloat("_ImportedShadowOnly")*wet.shadow);
                if(s.sky&&s.name==wet.skyMaterial)m.mainTexture=Texture(wet.skyTexture);
            }
            if(s.shadow||(s.textures.Length==1&&s.textures[0].type==6)) { m.SetFloat("_SrcBlend",(float)BlendMode.SrcAlpha); m.SetFloat("_DstBlend",(float)BlendMode.OneMinusSrcAlpha); m.SetFloat("_ZWrite",0); m.renderQueue=950; }
            if(s.shadow&&s.textures.Length==0){
                // Tsubaki's wet roadside shadow has no texture. Its retained
                // RGB diffuse is a multiplicative visibility value, not an
                // opaque white strip from the shader's default texture.
                m.SetColor("_ImportedUntexturedShadow",new Color(((s.diffuse>>16)&255)/255f,((s.diffuse>>8)&255)/255f,(s.diffuse&255)/255f,1));
                m.SetFloat("_SrcBlend",(float)BlendMode.DstColor);m.SetFloat("_DstBlend",(float)BlendMode.Zero);
            }
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
                if(wet!=null&&!data.materials[mat].sky)for(int i=0;i<nv;i++){
                    var c=colors[i];float grey=(c.r*.299f+c.g*.587f+c.b*.114f)*wet.desaturate,keep=1-wet.desaturate;
                    colors[i]=new Color32((byte)((c.r*keep+grey)*wet.brightness),(byte)((c.g*keep+grey)*wet.brightness),(byte)((c.b*keep+grey)*wet.brightness),c.a);
                }
                int[] indices=new int[nt]; for(int i=0;i<nt;i++) { indices[i]=r.ReadInt32(); if(indices[i]<0||indices[i]>=nv) throw new InvalidDataException("Vertex index"); }
                Vector2[] treeFaces=null;
                // Keep text correction inside its own atlas tiles; ordinary
                // road, rock and foliage sharing the atlas retain their UVs.
                if(LoadedCourse=="TSUBAKI"&&IsTsubakiSignAtlas(data.materials[mat]))
                    treeFaces=TsubakiSignFaceTags(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices);
                else if(IsSponsorMaterial(LoadedCourse,data.materials[mat].name))
                    treeFaces=SponsorFaceTags(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices,LoadedCourse=="SADAMINE"&&Path.GetFileName(root)=="night_wet"?.25f:0,LoadedCourse=="HAKONE",LoadedCourse=="TSUBAKI"?4:0);
                if(UsesPairedFoliageFaces(LoadedCourse,data.materials[mat])&&(!roadsideBaseline||data.materials[mat].kind=="tree")){
                    treeFaces=PrepareSceneryFaces(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices,treeFaces,out int paired);
                    if(data.materials[mat].kind=="tree")PairedTreeTriangles+=paired;
                    else PairedRoadsideTriangles+=paired;
                }
                var planes=direct?SceneryFacePlanes(vertices,indices,treeFaces):null;
                bool tree=data.materials[mat].kind=="tree";
                if(tree){treeVerticesBefore+=vertices.Length;treeIndexBytesBefore+=(long)indices.Length*4;}
                if(packTrees&&tree)PackTreeVertices(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices,ref treeFaces,ref planes);
                var indexFormat=packTrees&&tree&&vertices.Length<=65535?IndexFormat.UInt16:IndexFormat.UInt32;
                if(tree){treeVerticesAfter+=vertices.Length;treeIndexBytesAfter+=(long)indices.Length*(indexFormat==IndexFormat.UInt16?2:4);}
                var mesh=new Mesh{name="Hakone original shape "+shape,indexFormat=indexFormat};
                mesh.vertices=vertices; mesh.normals=normals; mesh.uv=uv; mesh.uv2=uv2; mesh.colors32=colors;
                // Unity may alias missing UV streams to texture coordinates.
                // Explicit zeros keep unpaired triangles two-sided; otherwise
                // arbitrary texture UVs can turn into per-corner culling flags.
                if(direct)mesh.uv3=treeFaces??new Vector2[vertices.Length];
                else if(treeFaces!=null)mesh.uv3=treeFaces;
                if(direct)mesh.SetUVs(3,planes);
                mesh.triangles=indices; mesh.RecalculateBounds(); mesh.UploadMeshData(true);
                sourceMeshes[shape]=mesh; sourceMaterials[shape]=mat;
                if(data.materials[mat].kind=="tree" || data.materials[mat].kind=="gallery") continue;
                var go=new GameObject(materials[mat].name+" "+shape); go.transform.SetParent(transform,false);
                go.AddComponent<MeshFilter>().sharedMesh=mesh;
                var renderer=go.AddComponent<MeshRenderer>(); renderer.sharedMaterial=materials[mat]; renderer.shadowCastingMode=ShadowCastingMode.Off; renderer.receiveShadows=false;
                int direction=SceneryDirection(LoadedCourse,data.materials[mat].name);
                if(direction!=0){bool uphill=direction>0;directionalScenery.Add((renderer,uphill));renderer.enabled=uphill==((SceneFlags&32768u)!=0);}
                if(data.materials[mat].sky) { skies.Add(go.transform); go.transform.localScale=Vector3.one*2000; }
                // Stars and other clear-sky layers are hidden behind the rain cloud.
                if(wet!=null&&data.materials[mat].sky&&data.materials[mat].name!=wet.skyMaterial)renderer.enabled=false;
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
        if(direct)Debug.Log($"IDZero tree buffers {LoadedCourse}: vertices {treeVerticesBefore} -> {treeVerticesAfter}; index bytes {treeIndexBytesBefore} -> {treeIndexBytesAfter}");
    }
    internal static int SceneryDirection(string course,string material){
        // Gunsai authors each gate twice at one place (START over FINISH at
        // path 93 for outbound, the reverse at 3094) plus a stop fence behind
        // each start that matches its direction's collision barrier.
        if(course=="GUNSAI")return material.StartsWith("downhill_gate_block_a",StringComparison.Ordinal)||material.StartsWith("downhill_fence_stop1",StringComparison.Ordinal)?-1:
            material.StartsWith("hillclimb_gate_block_a",StringComparison.Ordinal)||material.StartsWith("hillclimb_fence_stop1",StringComparison.Ordinal)?1:0;
        // Odawara's directions take different roads through the corner before
        // the line; these close the other road and sign the one in use.
        if(course=="TSUBAKI"||course=="ODAWARA"){
            if(material.StartsWith("downhill_",StringComparison.Ordinal))return -1;
            if(material.StartsWith("hillclimb_",StringComparison.Ordinal))return 1;
        }
        if(course=="SADAMINE"){
            if(material=="downhill_Cmn_Fence_Mat2"||material=="downhill_gate_checkpoint2"||material=="downhill_gate_lamplight2")return -1;
            if(material=="hillclimb_Cmn_Fence_Mat3"||material=="hillclimb_gate_checkpoint1"||material=="hillclimb_gate_lamplight1")return 1;
        }
        if(course!="HAKONE")return 0;
        if(material=="downhill_O_barricade_panel_mat2"||material=="downhill_O_barricade_panel")return -1;
        if(material=="hillclimb_O_barricade_panel_mat1"||material=="hillclimb_O_barricade_panel")return 1;
        return 0;
    }
    void UpdateDirection(){foreach(var item in directionalScenery)item.renderer.enabled=item.uphill==reverse;}
    void UpdateAreaFog(float pathPoint){
        if(pathPoint==areaFogPoint)return;
        areaFogPoint=pathPoint;var keys=areaFog.keys;Vector4 fog=Vector4.zero;Color color=Color.black;
        for(int i=0;i+1<keys.Length;++i){
            if(pathPoint<keys[i].point||pathPoint>keys[i+1].point)continue;
            var a=keys[i];var b=keys[i+1];float t=(pathPoint-a.point)/(b.point-a.point);
            fog=new Vector4(Mathf.Lerp(a.start,b.start,t),Mathf.Lerp(a.end,b.end,t),Mathf.Lerp(a.density,b.density,t),0);
            color=Color.Lerp(new Color(a.color[0],a.color[1],a.color[2]),new Color(b.color[0],b.color[1],b.color[2]),t);break;
        }
        foreach(var m in materials){m.SetVector("_ImportedAreaFog",fog);m.SetColor("_ImportedAreaFogColor",color);}
    }
    void UpdateLighting(float pathPoint) {
        int selected=0;
        // IDZero's second event list is not the reverse route: every course's
        // night file has the same one, and each day file's holds one constant
        // profile. Both directions follow the first list.
        foreach(var e in data.lightingEvents[!IdZero(LoadedCourse)&&reverse?1:0].points) if(pathPoint>=e.point) selected=e.profile;
        if(selected==lightingProfile)return;
        lightingProfile=selected; var profile=data.lighting[selected];
        var sun=new Vector4(-profile.sunDirection[0],-profile.sunDirection[1],-profile.sunDirection[2],0);
        var fog=wet!=null?new Color(wet.fogColor[0],wet.fogColor[1],wet.fogColor[2],1):new Color(profile.fogColor[0],profile.fogColor[1],profile.fogColor[2],1);
        // Stage 8 scattering is mapped to the main renderer's native atmosphere
        // curve; this is not an implementation of Stage 8's Mie/Rayleigh shader.
        foreach(var m in materials) {
            m.SetVector("_ImportedSunDirection",sun); m.SetColor("_ImportedFogColor",fog);
            m.SetVector("_ImportedFogRange",new Vector4(85,wet!=null?wet.fogRange:1/Mathf.Max(.00001f,profile.distanceScale),0,0));
        }
    }
    internal static bool IsSponsorMaterial(string course,string material)=>course=="TSUBAKI"?
        material=="makersign_all"||material=="makersign_O_barricade_panel"||material=="O_barricade_panel"||
        material=="downhill_O_barricade_panel1"||material=="hillclimb_O_barricade_panel2"||material=="E_grail_ref_w6":
        course=="SADAMINE"?material=="makersign_daydry":course=="HAKONE"&&(material=="makersign_O_barricade_panel_mat3"||material=="makersign_sign"||material=="makersign_adboard"||material=="makersign_O_barricade_panel");
    internal static Vector2[] SponsorFaceTags(ref Vector3[] vertices,ref Vector3[] normals,ref Vector2[] uv,ref Vector2[] uv2,ref Color32[] colors,ref int[] indices,float atlasOffset,bool rotated=false,float atlasColumns=0){
        var points=new List<Vector3>(vertices);var ns=new List<Vector3>(normals);
        var ts=new List<Vector2>(uv);var ts2=new List<Vector2>(uv2);var cs=new List<Color32>(colors);
        var tags=new List<Vector2>(new Vector2[vertices.Length]);
        var copies=new Dictionary<(int,int),int>();
        for(int i=0;i<indices.Length;i+=3){
            var a=uv[indices[i]];var b=uv[indices[i+1]];var c=uv[indices[i+2]];
            // Tsubaki uses the same seven-logo layout with quarter-width cells.
            float columns=atlasColumns>0?atlasColumns:rotated?16:8,rows=rotated?4:16;
            int column=Mathf.FloorToInt(((a.x+b.x+c.x)/3-atlasOffset)*columns),row=Mathf.FloorToInt((a.y+b.y+c.y)/3*rows);
            if(column<0||column>(rotated?6:1)||row<0||row>(rotated?0:3)||(!rotated&&column==1&&row==3))continue;
            float left=atlasOffset+column/columns,right=atlasOffset+(column+1)/columns,top=row/rows,bottom=(row+1)/rows;
            float minU=Mathf.Min(a.x,b.x,c.x),maxU=Mathf.Max(a.x,b.x,c.x);
            float minV=Mathf.Min(a.y,b.y,c.y),maxV=Mathf.Max(a.y,b.y,c.y);
            if(minU<left-.00001f||maxU>right+.00001f||minV<top-.00001f||maxV>bottom+.00001f||maxU-minU<.001f||maxV-minV<.001f)continue;
            for(int k=0;k<3;k++){
                int source=indices[i+k];var key=(source,column);
                if(!copies.TryGetValue(key,out int copy)){
                    copy=points.Count;copies.Add(key,copy);
                    points.Add(vertices[source]);ns.Add(normals[source]);ts.Add(uv[source]);ts2.Add(uv2[source]);cs.Add(colors[source]);
                    // Negative axis marks Hakone's logos, rotated in the atlas:
                    // their horizontal lettering runs toward decreasing V.
                    tags.Add(new Vector2(0,rotated?-(top+bottom):left+right));
                }
                indices[i+k]=copy;
            }
        }
        if(copies.Count==0)return null;
        vertices=points.ToArray();normals=ns.ToArray();uv=ts.ToArray();uv2=ts2.ToArray();colors=cs.ToArray();
        return tags.ToArray();
    }
    static int ComparePosition(Vector3 a,Vector3 b){int n=a.x.CompareTo(b.x);if(n==0)n=a.y.CompareTo(b.y);return n!=0?n:a.z.CompareTo(b.z);}
    static (Vector3,Vector3,Vector3) FaceKey(Vector3 a,Vector3 b,Vector3 c,out int side){
        int swaps=0;
        if(ComparePosition(a,b)>0){var t=a;a=b;b=t;swaps++;}
        if(ComparePosition(b,c)>0){var t=b;b=c;c=t;swaps++;}
        if(ComparePosition(a,b)>0){var t=a;a=b;b=t;swaps++;}
        side=1<<(swaps&1);return (a,b,c);
    }
    internal static bool UsesPairedFoliageFaces(string course,Surface surface)=>surface.kind=="tree"||
        // Tsubaki also authors opposite faces on terrain, barriers, guardrails
        // and roadside cards. Many backs use different UVs or baked colors;
        // drawing both sides together produces holes and depth flicker.
        course=="TSUBAKI"&&(surface.kind=="crs_a"||surface.kind=="crs_m")&&!surface.shadow&&!surface.sky||
        // Odawara does the same on its hillside bushes, fence nets, guardrails
        // and flags: about 96,000 m2 of coincident front and back faces.
        course=="ODAWARA"&&surface.kind=="course"&&!surface.shadow&&!surface.sky||
        course=="SADAMINE"&&surface.cutoff>0&&(surface.name.StartsWith("bush_",StringComparison.Ordinal)||
        surface.name.StartsWith("forest_",StringComparison.Ordinal)||surface.name=="sakura_main"||surface.name=="corner_grass_b");
    internal static Vector2[] PrepareSceneryFaces(ref Vector3[] vertices,ref Vector3[] normals,ref Vector2[] uv,ref Vector2[] uv2,ref Color32[] colors,ref int[] indices,Vector2[] textTags,out int paired){
        var textIndices=textTags==null?null:(int[])indices.Clone();
        var faces=PrepareTreeFaces(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices,out paired);
        if(faces==null)return textTags;
        // PrepareTreeFaces splits vertices per triangle. Preserve the separate
        // sign-reflection axis through that split, including rotated notices.
        if(textTags!=null)for(int i=0;i<faces.Length;++i)faces[i].y=textTags[textIndices[i]].y;
        return faces;
    }
    internal static Vector2[] PrepareTreeFaces(ref Vector3[] vertices,ref Vector3[] normals,ref Vector2[] uv,ref Vector2[] uv2,ref Color32[] colors,ref int[] indices,out int paired){
        var keys=new (Vector3,Vector3,Vector3)[indices.Length/3];
        var sides=new Dictionary<(Vector3,Vector3,Vector3),int>();
        for(int i=0;i<keys.Length;i++){
            keys[i]=FaceKey(vertices[indices[3*i]],vertices[indices[3*i+1]],vertices[indices[3*i+2]],out int side);
            sides.TryGetValue(keys[i],out int mask);sides[keys[i]]=mask|side;
        }
        var covered=OverlappingTreeFaces(vertices,indices);
        paired=0;for(int i=0;i<keys.Length;i++)if(sides[keys[i]]==3||covered[i])paired++;
        if(paired==0)return null;
        // Some source leaves have separate front/back polygons with different
        // UVs and lighting on the exact same plane. Drawing both with Cull Off
        // fights the depth buffer. Tag just those pairs; lone cards stay two-sided.
        var points=new Vector3[indices.Length];var ns=new Vector3[indices.Length];
        var ts=new Vector2[indices.Length];var ts2=new Vector2[indices.Length];
        var cs=new Color32[indices.Length];var flags=new Vector2[indices.Length];var ix=new int[indices.Length];
        for(int i=0;i<indices.Length;i++){
            int source=indices[i];points[i]=vertices[source];ns[i]=normals[source];ts[i]=uv[source];ts2[i]=uv2[source];cs[i]=colors[source];ix[i]=i;
            flags[i]=new Vector2(sides[keys[i/3]]==3||covered[i/3]?1:0,0);
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
    internal static int TreeLod(float distanceSquared,int previous,bool denseModels=false){
        // Separate enter/exit distances prevent camera shake or corrections
        // from toggling tree meshes or visibility on consecutive frames.
        // IDZero's c model is still a detailed 3D tree (~1,100 triangles),
        // unlike Stage 8's two-triangle card. Use it beyond 200 m instead of
        // retaining the ~2,400-triangle models out to 600 m. Visibility range
        // is unchanged, including trees in the mirror and distant hillsides.
        float near=denseModels?80f:260f,far=denseModels?200f:600f;
        if(previous==0&&distanceSquared<(near*1.1f)*(near*1.1f))return 0;
        if(previous==1&&distanceSquared>=(near*.9f)*(near*.9f)&&distanceSquared<(far*1.1f)*(far*1.1f))return 1;
        if(previous==2&&distanceSquared>=(far*.9f)*(far*.9f)&&distanceSquared<1760f*1760f)return 2;
        if(previous==-1&&distanceSquared>=1440f*1440f)return -1;
        return distanceSquared<near*near?0:distanceSquared<far*far?1:distanceSquared<1600f*1600f?2:-1;
    }
    internal static List<Vector4> SceneryFacePlanes(Vector3[] vertices,int[] indices,Vector2[] faces){
        // Paired faces already have separate vertices. Store one identical
        // object-space plane on all three corners: the vertex shader can reject
        // the hidden side without running a geometry shader on millions of
        // tree triangles. Lighting normals and source UVs remain untouched.
        var planes=new Vector4[vertices.Length];
        if(faces==null)return new List<Vector4>(planes);
        for(int i=0;i<indices.Length;i+=3){
            int a=indices[i],b=indices[i+1],c=indices[i+2];
            if(faces[a].x<=.5f)continue;
            var n=Vector3.Cross(vertices[b]-vertices[a],vertices[c]-vertices[a]).normalized;
            var plane=new Vector4(n.x,n.y,n.z,-Vector3.Dot(n,vertices[a]));
            planes[a]=planes[b]=planes[c]=plane;
        }
        return new List<Vector4>(planes);
    }
    bool BillboardTree(int lod)=>lod==2&&(!IdZero(LoadedCourse)||idZeroGeometryBaseline);
    internal void VerifyTreeState(){
        if(PairedTreeTriangles<=0)throw new InvalidOperationException("Source paired leaf faces were not tagged");
        foreach(var item in scenery){
            if(item.source.kind!="tree"||item.lod<0)continue;
            if(!item.renderer.enabled||item.filter.sharedMesh!=sourceMeshes[item.source.meshes[item.lod]])throw new InvalidOperationException("Visible tree lost its LOD mesh");
            if(BillboardTree(item.lod)){var facing=view.transform.position-item.filter.transform.position;facing.y=0;if(facing.sqrMagnitude>.01f&&Vector3.Dot(item.filter.transform.forward,facing.normalized)<.99f)throw new InvalidOperationException("Distant tree is edge-on");}
            else if(Quaternion.Angle(item.transform.rotation,item.authoredRotation)>.01f)throw new InvalidOperationException("3D tree lost its authored orientation");
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
        bool denseModels=IdZero(LoadedCourse)&&!idZeroGeometryBaseline;
        foreach(var item in scenery) {
            float d=SceneryDistanceSquared((item.position-cameraPosition).sqrMagnitude,detail);
            bool tree=item.tree;
            int lod=tree?TreeLod(d,item.lod,denseModels):(d<500*500?0:-1);
            if(item.lod!=lod) {
                item.lod=lod; item.renderer.enabled=lod>=0;
                if(lod>=0) { int index=item.source.meshes[lod]; item.filter.sharedMesh=sourceMeshes[index]; item.renderer.sharedMaterial=materials[sourceMaterials[index]];
                    if(tree&&!BillboardTree(lod))item.transform.rotation=item.authoredRotation;
                }
            }
            if(lod>=0&&(!tree||BillboardTree(lod))) {
                Vector3 facing=cameraPosition-item.position; facing.y=0;
                if(facing.sqrMagnitude>.01f) item.transform.rotation=Quaternion.LookRotation(facing,Vector3.up);
            }
        }
    }
}
