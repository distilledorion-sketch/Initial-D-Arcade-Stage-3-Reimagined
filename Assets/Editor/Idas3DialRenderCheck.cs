using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

// Editor-only comparison of production UI materials with native PVR composition.
// Fixtures contain the original HUD's captured triangles, textures and CPU pixels.
public static class Idas3DialRenderCheck
{
    static Color32 Ink(uint a) => new Color32((byte)(a>>16),(byte)(a>>8),(byte)a,(byte)(a>>24));
    static Vector4 Offset(uint a) { var c=Ink(a); return new Vector4(c.r/255f,c.g/255f,c.b/255f,c.a/255f); }
    public static void Run()
    {
        string root=Path.GetFullPath("Verification/legacy-audit-20260918"), output=Path.Combine(root,"dial-gpu-8");
        Directory.CreateDirectory(output);
        var results=new List<string>();
        var go=new GameObject("Private dial material check");var ui=go.AddComponent<Idas3UnityUi>();
        var camera=go.AddComponent<Camera>();camera.enabled=false;camera.cullingMask=0;camera.allowHDR=false;camera.allowMSAA=false;
        var target=new RenderTexture(640,480,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);target.Create();camera.targetTexture=target;
        ui.Initialize(camera);
        var drawType=typeof(Idas3UnityUi).GetNestedType("Draw",BindingFlags.NonPublic);
        var getMaterial=typeof(Idas3UnityUi).GetMethod("GetMaterial",BindingFlags.Instance|BindingFlags.NonPublic);
        long compared=0,changed=0,largeErrors=0;int maxError=0;
        using(var reader=new BinaryReader(File.OpenRead(Path.Combine(root,"dial-fixtures-8.bin")))) {
            if(reader.ReadUInt32()!=0x384c4144)throw new Exception("Invalid dial fixtures");int count=reader.ReadInt32();
            for(int test=0;test<count;++test) {
                uint layout=reader.ReadUInt32(),tach=reader.ReadUInt32(),bg=reader.ReadUInt32();
                var expected=new uint[640*480];for(int i=0;i<expected.Length;++i)expected[i]=reader.ReadUInt32();
                int nd=reader.ReadInt32(),nv=reader.ReadInt32(),nt=reader.ReadInt32();var draws=new object[nd];
                for(int d=0;d<nd;++d){var value=Activator.CreateInstance(drawType);
                    foreach(string field in new[]{"first","count","texture","tsp","pcw","flags"})drawType.GetField(field).SetValue(value,reader.ReadUInt32());
                    drawType.GetField("opacity").SetValue(value,reader.ReadSingle());drawType.GetField("reserved").SetValue(value,reader.ReadUInt32());
                    drawType.GetField("clip").SetValue(value,new Vector4(reader.ReadSingle(),reader.ReadSingle(),reader.ReadSingle(),reader.ReadSingle()));draws[d]=value;}
                var positions=new Vector3[nv];var uv=new Vector2[nv];var colors=new Color32[nv];var offsets=new Vector4[nv];
                for(int i=0;i<nv;++i){positions[i]=new Vector3(reader.ReadSingle(),reader.ReadSingle(),0);uv[i]=new Vector2(reader.ReadSingle(),reader.ReadSingle());colors[i]=Ink(reader.ReadUInt32());offsets[i]=Offset(reader.ReadUInt32());}
                var textures=new Texture2D[nt];
                for(int i=0;i<nt;++i){int w=reader.ReadInt32(),h=reader.ReadInt32(),bytes=reader.ReadInt32();textures[i]=new Texture2D(w,h,TextureFormat.RGBA32,false,true);textures[i].filterMode=FilterMode.Point;textures[i].LoadRawTextureData(reader.ReadBytes(bytes));textures[i].Apply(false);}
                var mesh=new Mesh();mesh.vertices=positions;mesh.uv=uv;mesh.colors32=colors;mesh.SetUVs(1,new List<Vector4>(offsets));mesh.subMeshCount=nd;
                var cmd=new CommandBuffer();
                for(int d=0;d<nd;++d){object value=draws[d];Func<string,uint> word=field=>(uint)drawType.GetField(field).GetValue(value);
                    int first=(int)word("first"),length=(int)word("count");var indices=new int[length];for(int i=0;i<length;++i)indices[i]=first+i;
                    mesh.SetIndices(indices,MeshTopology.Triangles,d,false);var props=new MaterialPropertyBlock();
                    props.SetTexture("_MainTex",textures[word("texture")]);props.SetVector("_Canvas",new Vector4(640,480,0,0));props.SetVector("_Clip",(Vector4)drawType.GetField("clip").GetValue(value));
                    props.SetFloat("_Opacity",(float)drawType.GetField("opacity").GetValue(value));props.SetColor("_Tint",Color.white);
                    cmd.DrawMesh(mesh,Matrix4x4.identity,(Material)getMaterial.Invoke(ui,new[]{value}),d,0,props);}
                mesh.bounds=new Bounds(Vector3.zero,Vector3.one*100000);
                camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Ink(bg);camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,cmd);camera.Render();camera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque,cmd);
                RenderTexture.active=target;var image=new Texture2D(640,480,TextureFormat.RGBA32,false,true);image.ReadPixels(new Rect(0,0,640,480),0,0);image.Apply();RenderTexture.active=null;
                var actual=image.GetPixels32();int localMax=0,bad=0,ink=0;
                for(int y=0;y<480;++y)for(int x=0;x<640;++x){var e=Ink(expected[y*640+x]);var a=actual[(479-y)*640+x];int error=Math.Max(Math.Abs(a.r-e.r),Math.Max(Math.Abs(a.g-e.g),Math.Abs(a.b-e.b)));
                    ++compared;if(expected[y*640+x]!=bg){++changed;++ink;}if(error>2){++largeErrors;++bad;}maxError=Math.Max(maxError,error);localMax=Math.Max(localMax,error);}
                results.Add($"{layout},{tach},{bg:X8},{ink},{bad},{localMax}");
                File.WriteAllBytes(Path.Combine(output,$"dial-{layout}-{tach}-{bg:X8}.png"),image.EncodeToPNG());
                UnityEngine.Object.DestroyImmediate(image);UnityEngine.Object.DestroyImmediate(mesh);foreach(var tex in textures)UnityEngine.Object.DestroyImmediate(tex);cmd.Release();
            }
            if(reader.BaseStream.Position!=reader.BaseStream.Length)throw new Exception("Trailing fixture bytes");
        }
        File.WriteAllLines(Path.Combine(output,"pixels.csv"),new[]{"layout,tach,background,inkPixels,errorsOver2,maxError"});File.AppendAllLines(Path.Combine(output,"pixels.csv"),results);
        File.WriteAllText(Path.Combine(output,"summary.json"),$"{{\"cases\":{results.Count},\"comparedPixels\":{compared},\"inkPixels\":{changed},\"errorsOver2\":{largeErrors},\"maxError\":{maxError},\"colorSpace\":\"{QualitySettings.activeColorSpace}\",\"device\":\"{SystemInfo.graphicsDeviceName}\"}}");
        // Report mismatches for inspection; silhouette rasterization may differ at edges.
        Debug.Log($"Dial GPU comparison: {results.Count} cases, {largeErrors} pixels differ by >2, max={maxError}");
        camera.targetTexture=null;target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(go);
    }
}
