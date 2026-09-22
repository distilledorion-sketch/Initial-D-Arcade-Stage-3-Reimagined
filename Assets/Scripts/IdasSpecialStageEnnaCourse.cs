using System;
using System.IO;
using System.Text;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

// Special Stage supplies scenery. The existing D3 owner drives cars, race rules,
// camera, HUD, audio, points, records and replay playback.
public sealed class IdasSpecialStageEnnaCourse : MonoBehaviour
{
    public const uint SceneFlag=1048576u;
    public static int CourseId(uint flags)=>(flags&8388608u)!=0?14:(flags&4194304u)!=0?13:(flags&2097152u)!=0?12:11;
    public int LoadedCourseId { get; private set; }=-1;
    public bool testBuild;
    [Serializable] sealed class Tree { public string model; public float[] position,rotation,scale; }
    [Serializable] sealed class Spectator { public string texture;public float[] position;public float height,width; }
    [Serializable] sealed class GateSet { public int direction;public string[] models; }
    [Serializable] sealed class ScenerySection { public string model;public int firstNode,lastNode; }
    [Serializable] sealed class Lighting { public float[] ambient,direction,directionalColor,fogColor;public float fogStart,fogEnd;public string sky;public bool skyFollowsCameraXZ; }
    [Serializable] sealed class Manifest { public string geometry;public string[] textureNames,oneSidedTextures; public Tree[] trees;public Spectator[] spectators;public GateSet[] gateSets;public Lighting lighting,wetLighting;public ScenerySection[] scenerySections; }
    [System.Runtime.InteropServices.DllImport("Idas3Unity",CallingConvention=System.Runtime.InteropServices.CallingConvention.Cdecl)]
    static extern int Idas3SceneImportedSourceNode();
    readonly List<(ScenerySection section,GameObject model)> scenerySections=new List<(ScenerySection,GameObject)>();
    int sceneryNode=-1;
    sealed class Part { public Mesh mesh; public int material; }
    readonly List<Mesh> meshes=new List<Mesh>();
    readonly List<Texture2D> textures=new List<Texture2D>();
    readonly List<Material> materials=new List<Material>();
    readonly GameObject[] gates=new GameObject[2];
    readonly GameObject[] skies=new GameObject[2];
    Lighting[] weatherLighting;int appliedWeather=-1;
    int spectatorCount,expectedSpectators;
    Idas3SceneGame host;bool loaded,visible;int samples=-1;
    public bool Loaded=>loaded;
    uint Flags=>Idas3ReplayViewer.Instance!=null?Idas3ReplayViewer.Instance.Status.flags:host!=null?host.Status.flags:0;
    void Start(){host=FindAnyObjectByType<Idas3SceneGame>();}
    void LateUpdate(){
        bool active=(Flags&SceneFlag)!=0&&(Flags&(1u|4096u|262144u))==0;
        if(active&&(!loaded||LoadedCourseId!=CourseId(Flags))){
            try{ClearScenery();Load(CourseId(Flags));}catch(Exception e){Debug.LogException(e);enabled=false;Application.Quit(1);return;}
        }
        if(active!=visible){foreach(Transform child in transform)child.gameObject.SetActive(active);visible=active;}
        if(!active)return;
        UpdateSceneryVisibility();
        bool reverse=(Flags&32768u)!=0;
        for(int i=0;i<2;++i)gates[i].SetActive((i==1)==reverse);
        int weather=(Flags&131072u)!=0?1:0;
        for(int i=0;i<2;++i)skies[i].SetActive(i==weather);
        if(appliedWeather!=weather){
            var light=weatherLighting[weather];
            foreach(var material in materials){
                material.SetVector("_ImportedNightAmbient",Vec(light.ambient)*2);
                material.SetVector("_ImportedFogColor",Vec(light.fogColor));
                material.SetVector("_ImportedFogRange",new Vector4(light.fogStart,light.fogEnd,0,0));
            }
            appliedWeather=weather;
        }
        if(samples==QualitySettings.antiAliasing)return;
        samples=QualitySettings.antiAliasing;
        foreach(var material in materials){bool coverage=material.GetFloat("_ImportedSky")==0&&samples>1;material.SetFloat("_ImportedCoverage",coverage?1:0);material.SetFloat("_AlphaToMask",coverage?1:0);}
    }
    static void Require(bool value,string message){if(!value)throw new InvalidDataException(message);}
    void UpdateSceneryVisibility(){
        if(scenerySections.Count==0)return;
        int node=Idas3SceneImportedSourceNode();
        Require(node>=0,"Special Stage scenery source position missing");
        // The source windows are in forward path order for both directions.
        // Apply them to the shared course objects so the mirror agrees with
        // the main camera, including replay seeks and opponent viewpoints.
        sceneryNode=node;
        foreach(var entry in scenerySections){
            bool show=node>=entry.section.firstNode&&node<=entry.section.lastNode;
            if(entry.model.activeSelf!=show)entry.model.SetActive(show);
        }
    }
    static Vector3 Vec(BinaryReader r){var v=new Vector3(r.ReadSingle(),r.ReadSingle(),r.ReadSingle());Require(float.IsFinite(v.x)&&float.IsFinite(v.y)&&float.IsFinite(v.z),"Non-finite Enna geometry");return v;}
    static Vector3 Vec(float[] p){Require(p!=null&&p.Length==3,"Enna placement");return new Vector3(p[0],p[1],p[2]);}
    static string Str(BinaryReader r){int n=r.ReadInt32();Require(n>0&&n<=64,"Enna model name");return Encoding.UTF8.GetString(r.ReadBytes(n));}
    // These authored gate skins share a plane with an oppositely wound skin.
    // Select the facing side in world space, as for the paired guardrails;
    // depth offsets cannot reliably separate the two triangulations.
    internal static bool IsPairedGateTexture(string name)=>
        name=="IONA_NIT231_004"||name=="IONA_NIT231_006"||name=="IONA_NIT231_007"||
        name=="IONA_NIT231_008"||name=="IONA_NIT231_009"||name=="IONA_NIT241_005";
    static bool IsPairedTexture(Manifest manifest,string name)=>IsPairedGateTexture(name)||
        (manifest.oneSidedTextures!=null&&Array.IndexOf(manifest.oneSidedTextures,name)>=0);
    void Load(int courseId){
        var root=Path.Combine(Application.streamingAssetsPath,Idas3CourseCatalog.Packs[courseId-9]);
        var manifest=JsonUtility.FromJson<Manifest>(File.ReadAllText(Path.Combine(root,"manifest.json")));
        Require(manifest.textureNames.Length>0&&manifest.textureNames.Length<1024&&manifest.trees.Length<10000,"Enna manifest");
        Require(manifest.spectators!=null&&manifest.spectators.Length<1000&&manifest.gateSets?.Length==2&&manifest.lighting!=null,"Enna presentation metadata");
        expectedSpectators=manifest.spectators.Length;
        var lighting=manifest.lighting;
        Require(manifest.wetLighting!=null&&manifest.wetLighting.fogEnd>manifest.wetLighting.fogStart,"Enna wet lighting missing");
        weatherLighting=new[]{lighting,manifest.wetLighting};
        Require(lighting.fogEnd>lighting.fogStart&&lighting.fogStart>=0,"Enna fog range");
        var shader=Resources.Load<Shader>("Idas3Scene");Require(shader!=null&&shader.isSupported,"D3 scene shader unavailable");
        var directShader=Resources.Load<Shader>("Idas3SceneDirect");Require(directShader!=null&&directShader.isSupported,"Direct D3 scene shader unavailable");
        bool gateBaseline=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-enna-gates-baseline")>=0;
        bool geometryBaseline=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-geometry-stage-baseline")>=0;
        foreach(var name in manifest.textureNames){
            Require(Path.GetFileName(name)==name,"Enna texture name");
            var texture=new Texture2D(2,2,TextureFormat.RGBA32,true);textures.Add(texture);
            Require(texture.LoadImage(File.ReadAllBytes(Path.Combine(root,"textures",name+".png"))),"Enna texture decode");
            texture.name=name;texture.filterMode=FilterMode.Trilinear;texture.anisoLevel=8;
            // Authored paired guardrail and gate faces need geometry-stage
            // rejection. All other imported vertices pass through unchanged.
            bool paired=IsPairedTexture(manifest,name)&&!(gateBaseline&&IsPairedGateTexture(name));
            var material=new Material(geometryBaseline||paired?shader:directShader){name="Enna "+name,mainTexture=texture,renderQueue=900,enableInstancing=true};materials.Add(material);
            material.EnableKeyword("IDAS_IMPORTED_COURSE");material.SetFloat("_ImportedNight",1);material.SetFloat("_ImportedCutoff",.3f);
            material.SetFloat("_ImportedPs2Lighting",1);
            material.SetVector("_ImportedNightAmbient",Vec(lighting.ambient)*2);
            material.SetVector("_ImportedFogColor",Vec(lighting.fogColor));
            material.SetVector("_ImportedFogRange",new Vector4(lighting.fogStart,lighting.fogEnd,0,0));
        }
        var models=new Dictionary<string,List<Part>>();
        string geometry=string.IsNullOrEmpty(manifest.geometry)?"enna.bin":manifest.geometry;
        Require(Path.GetFileName(geometry)==geometry,"Special Stage geometry path");
        var partMaterials=new Dictionary<int,int>();
        using(var r=new BinaryReader(File.OpenRead(Path.Combine(root,geometry)))){
            string signature=Encoding.ASCII.GetString(r.ReadBytes(4));bool partFlags=signature=="ENN2";
            Require(partFlags||signature=="ENN1","Special Stage geometry signature");int count=r.ReadInt32();Require(count>0&&count<512,"Enna model count");
            for(int model=0;model<count;++model){
                string name=Str(r);int np=r.ReadInt32();Require(np>=0&&np<512,"Enna part count");var parts=new List<Part>();models.Add(name,parts);
                for(int p=0;p<np;++p){
                    int material=r.ReadInt32(),nv=r.ReadInt32(),ni=r.ReadInt32();
                    int flags=partFlags?r.ReadInt32():0;
                    Require(material>=0&&material<manifest.textureNames.Length&&nv>=0&&nv<1000000&&ni>=0&&ni<3000000&&ni%3==0&&(flags&~3)==0,"Enna mesh header");
                    var vertices=new Vector3[nv];var uv=new Vector2[nv];var colors=new Color32[nv];var indices=new int[ni];
                    for(int v=0;v<nv;++v){
                        // Raw PS2 coordinates: D3's captured camera matrices
                        // already own handedness. Do not apply the Editor-only Z flip.
                        vertices[v]=Vec(r);uv[v]=new Vector2(r.ReadSingle(),r.ReadSingle());
                        byte red=r.ReadByte(),green=r.ReadByte(),blue=r.ReadByte(),alpha=r.ReadByte();
                        colors[v]=new Color32((byte)Math.Min(255,red*2),(byte)Math.Min(255,green*2),(byte)Math.Min(255,blue*2),(byte)Math.Min(255,alpha*2));
                    }
                    for(int i=0;i<ni;++i){indices[i]=r.ReadInt32();Require(indices[i]>=0&&indices[i]<nv,"Enna index");}
                    var mesh=new Mesh{name=name+"_"+p,indexFormat=IndexFormat.UInt32};meshes.Add(mesh);
                    mesh.vertices=vertices;mesh.uv=uv;mesh.colors32=colors;
                    // The shared D3 shader reads these channels even for PS2
                    // PCT meshes. Explicit zeros prevent missing UV streams
                    // aliasing texture UVs as paired-leaf rejection flags.
                    mesh.normals=new Vector3[nv];mesh.uv2=new Vector2[nv];
                    var faceFlags=new Vector2[nv];
                    // Paired guardrail and gate skins share the same plane.
                    // Reuse the D3 imported-face rejection in world space so
                    // both main and mirror cameras draw only the facing side.
                    if((flags&2)!=0||IsPairedTexture(manifest,manifest.textureNames[material])&&!(gateBaseline&&IsPairedGateTexture(manifest.textureNames[material])))
                        for(int v=0;v<nv;++v)faceFlags[v]=Vector2.right;
                    mesh.uv3=faceFlags;
                    mesh.triangles=indices;mesh.RecalculateBounds();mesh.UploadMeshData(true);
                    if(flags!=0){
                        int key=material*4+flags;
                        if(!partMaterials.TryGetValue(key,out int variant)){
                            var copy=new Material(materials[material]);
                            if((flags&2)!=0)copy.shader=shader;
                            if((flags&1)!=0){copy.SetFloat("_ZWrite",0);copy.renderQueue=950;}
                            variant=materials.Count;materials.Add(copy);partMaterials.Add(key,variant);
                        }
                        material=variant;
                    }
                    parts.Add(new Part{mesh=mesh,material=material});
                }
            }
            Require(r.BaseStream.Position==r.BaseStream.Length,"Trailing Enna mesh data");
        }
        GameObject Place(string name){
            var go=new GameObject(name);go.transform.SetParent(transform,false);go.layer=28;
            foreach(var part in models[name]){
                var child=new GameObject(part.mesh.name);child.transform.SetParent(go.transform,false);child.layer=28;
                child.AddComponent<MeshFilter>().sharedMesh=part.mesh;
                var renderer=child.AddComponent<MeshRenderer>();renderer.shadowCastingMode=ShadowCastingMode.Off;renderer.receiveShadows=false;
                var material=materials[part.material];
                if(name.StartsWith("sky",StringComparison.Ordinal)){
                    material=new Material(material){renderQueue=800};material.SetFloat("_ImportedSky",1);material.SetFloat("_ZWrite",0);material.SetFloat("_ImportedCutoff",0);
                    material.SetFloat("_ImportedSkyFollowXZ",lighting.skyFollowsCameraXZ?1:0);materials.Add(material);
                }
                renderer.sharedMaterial=material;
            }
            return go;
        }
        var sections=new Dictionary<string,ScenerySection>();
        if(manifest.scenerySections!=null)foreach(var section in manifest.scenerySections){
            Require(section.firstNode>=0&&section.lastNode>=section.firstNode&&models.ContainsKey(section.model),"Invalid scenery visibility window");
            sections.Add(section.model,section);
        }
        foreach(var name in models.Keys)if(name.StartsWith("crs",StringComparison.Ordinal)&&!name.StartsWith("crslod",StringComparison.Ordinal)&&models[name].Count>0){
            var go=Place(name);
            if(sections.TryGetValue(name,out var section))scenerySections.Add((section,go));
        }
        Require(scenerySections.Count==sections.Count,"Missing scenery visibility model");
        for(int weather=0;weather<2;++weather){
            Require(models.ContainsKey(weatherLighting[weather].sky),"Enna weather sky missing");
            skies[weather]=Place(weatherLighting[weather].sky);
        }
        foreach(var set in manifest.gateSets){
            Require(set.direction>=0&&set.direction<2&&gates[set.direction]==null&&set.models.Length==5,"Enna direction gates");
            var group=new GameObject(set.direction==0?"Downhill gates":"Uphill gates");group.transform.SetParent(transform,false);gates[set.direction]=group;
            foreach(var name in set.models){Require(models.ContainsKey(name),"Enna gate missing");Place(name).transform.SetParent(group.transform,false);}
        }
        foreach(var tree in manifest.trees){
            Require(models.ContainsKey(tree.model),"Enna tree template missing");var go=Place(tree.model);
            go.transform.localPosition=Vec(tree.position);go.transform.localRotation=Quaternion.Euler(Vec(tree.rotation));go.transform.localScale=Vec(tree.scale);
        }
        var spectatorMaterials=new Dictionary<string,Material>();
        var crowd=new GameObject("Original spectators");crowd.transform.SetParent(transform,false);
        foreach(var person in manifest.spectators){
            int index=Array.IndexOf(manifest.textureNames,person.texture);
            Require(index>=0&&person.height>0&&person.height<4&&person.width>0&&person.width<3,"Enna spectator dimensions");
            if(!spectatorMaterials.TryGetValue(person.texture,out var material)){
                material=new Material(materials[index]);material.SetFloat("_ImportedBillboard",1);materials.Add(material);spectatorMaterials.Add(person.texture,material);
            }
            float half=person.width*.5f;
            var mesh=new Mesh{name="Enna spectator "+spectatorCount};meshes.Add(mesh);
            mesh.vertices=new[]{new Vector3(-half,0,0),new Vector3(half,0,0),new Vector3(-half,person.height,0),new Vector3(half,person.height,0)};
            // Source gal textures lie sideways: head at U=1. The PS2 sprite
            // uses a 90-degree rotation and foot pivot (00165430/00188b40).
            mesh.uv=new[]{new Vector2(0,0),new Vector2(0,1),new Vector2(1,0),new Vector2(1,1)};
            var ambient=Vec(lighting.ambient)*2;var tint=new Color(ambient.x,ambient.y,ambient.z,1);
            mesh.colors=new[]{tint,tint,tint,tint};mesh.normals=new Vector3[4];mesh.uv2=new Vector2[4];mesh.uv3=new Vector2[4];mesh.triangles=new[]{0,2,1,1,2,3};
            mesh.bounds=new Bounds(new Vector3(0,person.height*.5f,0),new Vector3(person.width,person.height,person.width));mesh.UploadMeshData(true);
            var go=new GameObject("Spectator "+spectatorCount++);go.layer=28;go.transform.SetParent(crowd.transform,false);go.transform.localPosition=Vec(person.position);
            go.AddComponent<MeshFilter>().sharedMesh=mesh;var renderer=go.AddComponent<MeshRenderer>();renderer.sharedMaterial=material;renderer.shadowCastingMode=ShadowCastingMode.Off;renderer.receiveShadows=false;
        }
        LoadedCourseId=courseId;loaded=true;Debug.Log("SPECIAL_STAGE_SCENERY_LOADED: "+Idas3CourseCatalog.Names[courseId]+", "+manifest.trees.Length+" trees and "+spectatorCount+" spectators");
    }
    public void VerifyPresentation(){
        Require(loaded&&spectatorCount==expectedSpectators&&LoadedCourseId==CourseId(Flags),"Special Stage scenery not loaded");
        int direction=(Flags&32768u)!=0?1:0;
        Require(gates[direction].activeSelf&&!gates[1-direction].activeSelf&&gates[direction].transform.childCount==5,"Wrong Enna gate direction");
        int weather=(Flags&131072u)!=0?1:0;
        Require(appliedWeather==weather&&skies[weather].activeSelf&&!skies[1-weather].activeSelf,"Wrong Enna weather sky");
        foreach(var material in materials)Require(Mathf.Approximately(material.GetVector("_ImportedFogRange").y,weatherLighting[weather].fogEnd),"Wrong Enna weather fog");
        foreach(var material in materials)Require(material.GetFloat("_ImportedPs2Lighting")==1,"Enna source lighting missing");
        foreach(var entry in scenerySections)Require(entry.model.activeSelf==(sceneryNode>=entry.section.firstNode&&sceneryNode<=entry.section.lastNode),"Wrong Special Stage scenery visibility");
    }
    void ClearScenery(){
        for(int i=transform.childCount-1;i>=0;--i){
            var child=transform.GetChild(i);child.gameObject.SetActive(false);
            child.SetParent(null,false);Destroy(child.gameObject);
        }
        foreach(var mesh in meshes)Destroy(mesh);foreach(var material in materials)Destroy(material);foreach(var texture in textures)Destroy(texture);
        meshes.Clear();materials.Clear();textures.Clear();Array.Clear(gates,0,2);Array.Clear(skies,0,2);
        scenerySections.Clear();sceneryNode=-1;
        loaded=visible=false;LoadedCourseId=-1;spectatorCount=0;appliedWeather=samples=-1;
    }
    void OnDestroy(){ClearScenery();}
}
