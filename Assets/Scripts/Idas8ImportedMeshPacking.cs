using System.Collections.Generic;
using UnityEngine;

public sealed partial class Idas8HakoneCourse
{
    // Face preparation deliberately separates the front/back leaf triangles.
    // Recover vertex reuse only when every shader input is exactly identical.
    // No quantization, triangle reordering, removed polygons or changed UVs.
    internal static void PackTreeVertices(ref Vector3[] vertices,ref Vector3[] normals,
        ref Vector2[] uv,ref Vector2[] uv2,ref Color32[] colors,ref int[] indices,
        ref Vector2[] flags,ref List<Vector4> planes)
    {
        var unique=new Dictionary<(Vector3,Vector3,Vector2,Vector2,Color32,Vector2,Vector4),int>(vertices.Length);
        var sources=new List<int>(vertices.Length);
        var remap=new int[vertices.Length];for(int i=0;i<remap.Length;i++)remap[i]=-1;
        var packedIndices=new int[indices.Length];
        for(int i=0;i<indices.Length;i++){
            int source=indices[i],target=remap[source];
            if(target<0){
                var key=(vertices[source],normals[source],uv[source],uv2[source],colors[source],
                    flags==null?Vector2.zero:flags[source],planes[source]);
                if(!unique.TryGetValue(key,out target)){
                    target=sources.Count;unique.Add(key,target);sources.Add(source);
                }
                remap[source]=target;
            }
            packedIndices[i]=target;
        }
        if(sources.Count==vertices.Length)return;
        int count=sources.Count;
        var points=new Vector3[count];var ns=new Vector3[count];
        var ts=new Vector2[count];var ts2=new Vector2[count];
        var cs=new Color32[count];var fs=new Vector2[count];var ps=new List<Vector4>(count);
        for(int i=0;i<count;i++){
            int source=sources[i];points[i]=vertices[source];ns[i]=normals[source];
            ts[i]=uv[source];ts2[i]=uv2[source];cs[i]=colors[source];
            fs[i]=flags==null?Vector2.zero:flags[source];ps.Add(planes[source]);
        }
        vertices=points;normals=ns;uv=ts;uv2=ts2;colors=cs;flags=fs;planes=ps;indices=packedIndices;
    }
}
