using System;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

// A presentation-only copy of the selected save's fastest Time Attack run.
// No colliders, audio, physics, opponent state or wall-clock playback timer.
public sealed class Idas3TimeAttackGhost : MonoBehaviour
{
    [Serializable] private sealed class SourceMesh { public string source; public Part[] parts; }
    [Serializable] private sealed class Part { public int material; public Vector3[] vertices; public Vector2[] uv; public int[] indices; }
    private GameObject visual;
    private Mesh[] meshes;
    private Material[] materials;
    private Texture2D[] textures;
    private bool showGhost=true,failed;
    public bool ShowGhost {
        get=>showGhost;
        set {showGhost=value;if(!value&&visual!=null)visual.SetActive(false);}
    }
    public bool Visible=>visual!=null&&visual.activeSelf;
    public double BestTimeSeconds {get;private set;}

    public void ApplyFrame()
    {
        var state=new Idas3Native.GhostState{size=48};
        if(Idas3Native.Idas3SceneGetGhostState(ref state)!=1||state.version!=1){Hide();return;}
        ApplyState(state);
    }
    internal void ApplyState(Idas3Native.GhostState state)
    {
        BestTimeSeconds=(state.flags&1)!=0?state.finishTicks6000/6000.0:0;
        if(!showGhost||failed||(state.flags&2)==0){Hide();return;}
        if(visual==null){
            try {CreateVisual();}
            catch(Exception e){failed=true;Hide();Debug.LogWarning("Time Attack ghost unavailable: "+e.Message);return;}
        }
        // Native car presentation uses RY * RX * RZ. Preserve its slope/bank
        // at every recorded tick, including imported courses and reverse runs.
        var rotation=Quaternion.AngleAxis(state.yaw*Mathf.Rad2Deg,Vector3.up)*
            Quaternion.AngleAxis(state.pitch*Mathf.Rad2Deg,Vector3.right)*
            Quaternion.AngleAxis(state.roll*Mathf.Rad2Deg,Vector3.forward);
        visual.transform.SetPositionAndRotation(new Vector3(state.x,state.y,state.z)+rotation*Vector3.up*.06f,rotation);
        visual.SetActive(true);
    }
    private void Hide(){if(visual!=null)visual.SetActive(false);}
    private void CreateVisual()
    {
        const string root="TimeAttackGhost/";
#if IDAS3_PORTABLE_PLAYER && UNITY_STANDALONE_WIN && !UNITY_EDITOR
        string folder=Path.Combine(Application.streamingAssetsPath,"TimeAttackGhost");
        string json=File.ReadAllText(Path.Combine(folder,"Mesh.json"));
#else
        var data=Resources.Load<TextAsset>(root+"Mesh");
        if(data==null)throw new InvalidOperationException("Ghost mesh is missing.");
        string json=data.text;
#endif
        var shader=Resources.Load<Shader>("Idas3TimeAttackGhost");
        if(shader==null||!shader.isSupported)throw new InvalidOperationException("Ghost mesh or shader is missing.");
        var source=JsonUtility.FromJson<SourceMesh>(json);
        materials=new Material[2];textures=new Texture2D[2];meshes=new Mesh[source.parts.Length];
        for(int i=0;i<2;++i){
            string imageName=i==0?"Shadow.png":"Body.png";
#if IDAS3_PORTABLE_PLAYER && UNITY_STANDALONE_WIN && !UNITY_EDITOR
            byte[] pixels=File.ReadAllBytes(Path.Combine(folder,imageName));
#else
            var bytes=Resources.Load<TextAsset>(root+imageName);
            if(bytes==null)throw new InvalidOperationException("Ghost texture is missing.");
            byte[] pixels=bytes.bytes;
#endif
            textures[i]=new Texture2D(2,2,TextureFormat.RGBA32,false);
            if(!textures[i].LoadImage(pixels,true))throw new InvalidOperationException("Ghost texture is invalid.");
            textures[i].wrapMode=TextureWrapMode.Clamp;textures[i].filterMode=FilterMode.Bilinear;
            materials[i]=new Material(shader){mainTexture=textures[i],renderQueue=3100+i};
            materials[i].SetColor("_Tint",i==0?Color.white:new Color(.5f,.5f,.5f,1));
        }
        visual=new GameObject("Personal-best Time Attack ghost");
        for(int i=0;i<source.parts.Length;++i){
            var part=source.parts[i];var child=new GameObject("Ghost material "+part.material){layer=28};
            child.transform.SetParent(visual.transform,false);
            var mesh=new Mesh{name=source.source+" material "+part.material};meshes[i]=mesh;
            mesh.vertices=part.vertices;mesh.uv=part.uv;mesh.triangles=part.indices;mesh.RecalculateBounds();
            mesh.UploadMeshData(true);
            child.AddComponent<MeshFilter>().sharedMesh=mesh;
            var renderer=child.AddComponent<MeshRenderer>();renderer.sharedMaterial=materials[part.material];
            renderer.shadowCastingMode=ShadowCastingMode.Off;renderer.receiveShadows=false;
            renderer.lightProbeUsage=LightProbeUsage.Off;renderer.reflectionProbeUsage=ReflectionProbeUsage.Off;
        }
    }
    private void OnDisable(){Hide();}
    private void OnDestroy()
    {
        if(visual!=null)Destroy(visual);
        if(meshes!=null)foreach(var mesh in meshes)if(mesh!=null)Destroy(mesh);
        if(materials!=null)foreach(var material in materials)if(material!=null)Destroy(material);
        if(textures!=null)foreach(var texture in textures)if(texture!=null)Destroy(texture);
    }
}
