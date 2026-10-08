using System;
using System.Collections.Generic;
using UnityEngine;

// Runs only under the explicit isolated imported-course diagnostic.
internal static class Idas8ImportedTreeChecks {
    internal static void Run(){
        CheckPacking();
        foreach(bool dense in new[]{false,true})foreach(float boundary in dense?new[]{80f,200f,1600f}:new[]{260f,600f,1600f}){
            int original=Idas8HakoneCourse.TreeLod((boundary-1)*(boundary-1),-2,dense),lod=original;
            for(int i=0;i<120;i++){float d=boundary+(i%2==0?1:-1);lod=Idas8HakoneCourse.TreeLod(d*d,lod,dense);if(lod!=original)throw new Exception("Tree LOD flickered across a distance boundary");}
            float far=boundary*1.2f;lod=Idas8HakoneCourse.TreeLod(far*far,lod,dense);
            if(lod==original)throw new Exception("Tree LOD never changes at a safe distance");
            float near=boundary*.8f;lod=Idas8HakoneCourse.TreeLod(near*near,lod,dense);
            if(lod!=original)throw new Exception("Tree LOD does not recover on approach");
        }
        if(Idas8HakoneCourse.TreeLod(40*40,-2,true)!=0||Idas8HakoneCourse.TreeLod(400*400,-2,true)!=2||
            Idas8HakoneCourse.TreeLod(1500*1500,-2,true)!=2||Idas8HakoneCourse.TreeLod(1800*1800,-2,true)!=-1)
            throw new Exception("Dense tree LOD lost close detail or shortened the scenery range");
        var vertices=new[]{new Vector3(0,0,0),new Vector3(1,0,0),new Vector3(0,1,0),new Vector3(2,0,0),new Vector3(3,0,0),new Vector3(2,1,0)};
        var normals=new Vector3[6];var uv=new Vector2[6];var uv2=new Vector2[6];var colors=new Color32[6];
        for(int i=0;i<6;i++){normals[i]=Vector3.forward;uv[i]=new Vector2(i,.25f);uv2[i]=new Vector2(.75f,i);colors[i]=new Color32((byte)(i*20),100,200,255);}
        var indices=new[]{0,1,2,2,1,0,3,4,5};var sourceIndices=(int[])indices.Clone();var sourceUv=(Vector2[])uv.Clone();var sourceColors=(Color32[])colors.Clone();
        var flags=Idas8HakoneCourse.PrepareTreeFaces(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices,out int paired);
        if(paired!=2||flags.Length!=9)throw new Exception("Paired leaf topology not detected");
        for(int i=0;i<9;i++){
            if(flags[i].x!=(i<6?1:0)||indices[i]!=i||uv[i]!=sourceUv[sourceIndices[i]]||!colors[i].Equals(sourceColors[sourceIndices[i]]))throw new Exception("Leaf-face fix lost a single card or source UV/lighting");
        }
        var planes=Idas8HakoneCourse.SceneryFacePlanes(vertices,indices,flags);
        for(int i=0;i<9;i+=3){
            if(planes[i]!=planes[i+1]||planes[i]!=planes[i+2])throw new Exception("Leaf corners disagree on their culling plane");
            if(i==6&&planes[i]!=Vector4.zero)throw new Exception("Unpaired leaf card became one-sided");
        }
        // Compare the legacy world-space cross product with the direct path
        // for translated, rotated and scaled placements, from either camera side.
        foreach(float scale in new[]{.2f,1f,8f}){
            var matrix=Matrix4x4.TRS(new Vector3(251,-23,781),Quaternion.Euler(11,73,29),Vector3.one*scale);
            foreach(float side in new[]{-10f,10f}){
                var eye=matrix.MultiplyPoint3x4(new Vector3(.2f,.3f,side));
                var local=matrix.inverse.MultiplyPoint3x4(eye);
                for(int i=0;i<6;i+=3){
                    var a=matrix.MultiplyPoint3x4(vertices[i]);var b=matrix.MultiplyPoint3x4(vertices[i+1]);var c=matrix.MultiplyPoint3x4(vertices[i+2]);
                    bool oldVisible=Vector3.Dot(Vector3.Cross(b-a,c-a),eye-a)>0;
                    bool directVisible=Vector4.Dot(planes[i],new Vector4(local.x,local.y,local.z,1))>0;
                    if(oldVisible!=directVisible)throw new Exception("Direct leaf culling changed the visible authored face");
                }
            }
        }
    }
    static void CheckPacking(){
        // Adjacent leaf triangles share two corners. Then repeat a corner
        // while changing one shader input at a time to cover seams and backs.
        var vertices=new Vector3[13];var normals=new Vector3[13];
        var uv=new Vector2[13];var uv2=new Vector2[13];var colors=new Color32[13];
        var flags=new Vector2[13];var planes=new List<Vector4>(new Vector4[13]);
        var indices=new int[15];
        for(int i=0;i<13;i++){normals[i]=Vector3.forward;colors[i]=new Color32(40,80,120,255);}
        vertices[1]=vertices[4]=Vector3.right;vertices[2]=vertices[3]=Vector3.up;vertices[5]=Vector3.one;
        for(int i=0;i<15;i++)indices[i]=Math.Min(i,12);
        vertices[6]=Vector3.left;normals[7]=Vector3.back;uv[8]=Vector2.right;uv2[9]=Vector2.up;
        colors[10]=new Color32(40,80,121,255);flags[11]=Vector2.right;planes[12]=new Vector4(0,0,1,-2);
        var oldVertices=vertices;var oldNormals=normals;var oldUv=uv;var oldUv2=uv2;
        var oldColors=colors;var oldFlags=flags;var oldPlanes=planes;var oldIndices=indices;
        Idas8HakoneCourse.PackTreeVertices(ref vertices,ref normals,ref uv,ref uv2,ref colors,ref indices,ref flags,ref planes);
        if(vertices.Length!=11||indices.Length!=oldIndices.Length)throw new Exception("Leaf packing failed to reuse shared corners or merged a texture/lighting seam");
        for(int i=0;i<indices.Length;i++){
            int old=oldIndices[i],next=indices[i];
            if(!vertices[next].Equals(oldVertices[old])||!normals[next].Equals(oldNormals[old])||
                !uv[next].Equals(oldUv[old])||!uv2[next].Equals(oldUv2[old])||!colors[next].Equals(oldColors[old])||
                !flags[next].Equals(oldFlags[old])||!planes[next].Equals(oldPlanes[old]))
                throw new Exception("Tree packing changed a triangle corner or its shader inputs");
        }
    }
}
