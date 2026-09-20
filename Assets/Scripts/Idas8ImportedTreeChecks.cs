using System;
using UnityEngine;

// Runs only under the explicit isolated imported-course diagnostic.
internal static class Idas8ImportedTreeChecks {
    internal static void Run(){
        foreach(float boundary in new[]{260f,600f,1600f}){
            int original=Idas8HakoneCourse.TreeLod((boundary-1)*(boundary-1),-2),lod=original;
            for(int i=0;i<120;i++){float d=boundary+(i%2==0?1:-1);lod=Idas8HakoneCourse.TreeLod(d*d,lod);if(lod!=original)throw new Exception("Tree LOD flickered across a distance boundary");}
            float far=boundary*1.2f;lod=Idas8HakoneCourse.TreeLod(far*far,lod);
            if(lod==original)throw new Exception("Tree LOD never changes at a safe distance");
            float near=boundary*.8f;lod=Idas8HakoneCourse.TreeLod(near*near,lod);
            if(lod!=original)throw new Exception("Tree LOD does not recover on approach");
        }
        var vertices=new[]{new Vector3(0,0,0),new Vector3(1,0,0),new Vector3(0,1,0),new Vector3(2,0,0),new Vector3(3,0,0),new Vector3(2,1,0)};
        var normals=new Vector3[6];var uv=new Vector2[6];var uv2=new Vector2[6];var colors=new Color32[6];
        for(int i=0;i<6;i++){normals[i]=Vector3.forward;uv[i]=new Vector2(i,.25f);uv2[i]=new Vector2(.75f,i);colors[i]=new Color32((byte)(i*20),100,200,255);}
        var indices=new[]{0,1,2,2,1,0,3,4,5};var sourceIndices=(int[])indices.Clone();var sourceUv=(Vector2[])uv.Clone();var sourceColors=(Color32[])colors.Clone();
        var flags=Idas8HakoneCourse.PrepareTreeFaces(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices,out int paired);
        if(paired!=2||flags.Length!=9)throw new Exception("Paired leaf topology not detected");
        for(int i=0;i<9;i++){
            if(flags[i].x!=(i<6?1:0)||indices[i]!=i||uv[i]!=sourceUv[sourceIndices[i]]||!colors[i].Equals(sourceColors[sourceIndices[i]]))throw new Exception("Leaf-face fix lost a single card or source UV/lighting");
        }
    }
}
