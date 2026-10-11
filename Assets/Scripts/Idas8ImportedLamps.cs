using System;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

public sealed partial class Idas8HakoneCourse
{
    [Serializable] sealed class LampGlow { public float[] position,direction,color; public float size; }
    [Serializable] sealed class LampLens { public int lamp; public float[] vertices; }
    [Serializable] sealed class LampGlows { public string texture; public LampGlow[] lights; public LampLens[] lenses; }
    Mesh lampGlowMesh;
    Material lampGlowMaterial;

    void LoadLampGlows(){
        if(!IdZero(LoadedCourse)||(SceneFlags&65536u)==0)return;
        string file=Path.Combine(root,"lamp-glows.json");
        if(!File.Exists(file))return; // Older optional course packs still load.
        var data=JsonUtility.FromJson<LampGlows>(File.ReadAllText(file));
        if(data.lights==null||data.lights.Length>1024)throw new InvalidDataException("Lamp glows");
        if(data.lights.Length==0)return;
        var shader=Resources.Load<Shader>("Idas3ImportedLampGlow");
        if(shader==null||!shader.isSupported)throw new InvalidOperationException("Lamp glow shader unavailable");
        lampGlowMaterial=new Material(shader){name=LoadedCourse+" night lamp coronas",mainTexture=Texture(data.texture)};
        int count=data.lights.Length,lenses=data.lenses?.Length??0,total=count*4+lenses*3;
        var vertices=new Vector3[total];var normals=new Vector3[total];
        var uv=new Vector2[total];var corners=new Vector2[total];var colors=new Color[total];var indices=new int[count*6+lenses*3];
        var square=new[]{new Vector2(-1,-1),new Vector2(-1,1),new Vector2(1,1),new Vector2(1,-1)};
        for(int i=0;i<count;i++){
            var l=data.lights[i];
            if(l.position==null||l.position.Length!=3||l.direction==null||l.direction.Length!=3||l.color==null||l.color.Length!=3||!(l.size>0&&l.size<20))throw new InvalidDataException("Lamp glow entry");
            var p=new Vector3(l.position[0],l.position[1],l.position[2]);
            var direction=new Vector3(l.direction[0],l.direction[1],l.direction[2]).normalized;
            for(int j=0;j<4;j++){
                vertices[i*4+j]=p;normals[i*4+j]=direction;
                corners[i*4+j]=square[j]*Mathf.Clamp(l.size*.14f,.15f,.8f);
                uv[i*4+j]=new Vector2(.75f+square[j].x*.25f,.25f+square[j].y*.25f);
                colors[i*4+j]=new Color(l.color[0],l.color[1],l.color[2],1);
            }
            int v=i*4;int[] face={v,v+1,v+2,v,v+2,v+3};Array.Copy(face,0,indices,i*6,6);
        }
        for(int i=0;i<lenses;i++){
            var lens=data.lenses[i];
            if(lens.lamp<0||lens.lamp>=count||lens.vertices==null||lens.vertices.Length!=9)throw new InvalidDataException("Lamp lens");
            var light=data.lights[lens.lamp];
            // A white-hot lens with its source warm tint, not glowing metal.
            var color=Color.Lerp(new Color(light.color[0],light.color[1],light.color[2]),Color.white,.7f);
            for(int j=0;j<3;j++){
                int v=count*4+i*3+j;
                // Small outward offset avoids coplanar flicker with the retained
                // lens face. The fixture shell and nearby occluders keep depth.
                vertices[v]=new Vector3(lens.vertices[j*3],lens.vertices[j*3+1]-.005f,lens.vertices[j*3+2]);
                uv[v]=new Vector2(-1,-1);colors[v]=color;indices[count*6+i*3+j]=v;
            }
        }
        lampGlowMesh=new Mesh{name=LoadedCourse+" lamp coronas"};lampGlowMesh.vertices=vertices;lampGlowMesh.normals=normals;
        lampGlowMesh.uv=uv;lampGlowMesh.uv2=corners;lampGlowMesh.colors=colors;lampGlowMesh.triangles=indices;
        lampGlowMesh.RecalculateBounds();var bounds=lampGlowMesh.bounds;bounds.Expand(4);lampGlowMesh.bounds=bounds;lampGlowMesh.UploadMeshData(true);
        var go=new GameObject("Night lamp coronas");go.transform.SetParent(transform,false);go.layer=28;
        go.AddComponent<MeshFilter>().sharedMesh=lampGlowMesh;
        var renderer=go.AddComponent<MeshRenderer>();renderer.sharedMaterial=lampGlowMaterial;
        renderer.shadowCastingMode=ShadowCastingMode.Off;renderer.receiveShadows=false;
    }
    void ClearLampGlows(){
        if(lampGlowMesh!=null)Destroy(lampGlowMesh);
        if(lampGlowMaterial!=null)Destroy(lampGlowMaterial);
        lampGlowMesh=null;lampGlowMaterial=null;
    }
}
