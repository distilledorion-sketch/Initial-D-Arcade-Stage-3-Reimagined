using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;

// Compare actual production scene materials against native WARP framebuffers.
// Inputs are exported by the same UnitySceneCapture used by the shipping plugin.
public static class Idas3LightingPanelCheck
{
    [StructLayout(LayoutKind.Sequential)] struct UInt4 { public uint x,y,z,w; }
    [Serializable] sealed class Report {
        public bool passed;
        public int cases, comparedPixels, changedPixels, errorsOver2, interiorErrorsOver2, maxError;
        public string device, colorSpace, scope, error;
    }
    static Vector4 V4(BinaryReader r)=>new Vector4(r.ReadSingle(),r.ReadSingle(),r.ReadSingle(),r.ReadSingle());
    static Vector3 V3(BinaryReader r)=>new Vector3(r.ReadSingle(),r.ReadSingle(),r.ReadSingle());
    static Color32 Color(uint p)=>new Color32((byte)(p>>16),(byte)(p>>8),(byte)p,(byte)(p>>24));
    static UInt4 U4(BinaryReader r)=>new UInt4{x=r.ReadUInt32(),y=r.ReadUInt32(),z=r.ReadUInt32(),w=r.ReadUInt32()};
    static FieldInfo Field(string name)=>typeof(Idas3SceneRenderer).GetField(name,BindingFlags.NonPublic|BindingFlags.Instance);
    public static void Run()
    {
        string folder=Path.GetFullPath("Verification/lighting-gpu-20260918"),outputFolder=Path.Combine(folder,"panels-unity");Directory.CreateDirectory(outputFolder);
        var report=new Report {device=SystemInfo.graphicsDeviceName,colorSpace=QualitySettings.activeColorSpace.ToString(),
            scope="36 original course/day/night/wet lightsets; 432 course/player/rival panels, textured/untextured, flat/Gouraud/unlit. Production UnitySceneCapture constants/geometry, ConfigureMaterial, texture upload and scene shader versus native WARP image. Main view only; no gameplay, original cabinet screenshots, headlight projection geometry or imported-course fallback in this fixture."};
        var go=new GameObject("Private lighting panel comparison");var renderer=go.AddComponent<Idas3SceneRenderer>();var camera=go.AddComponent<Camera>();
        camera.enabled=false;camera.cullingMask=0;camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new UnityEngine.Color(.1f,.15f,.2f,1);camera.allowHDR=false;camera.allowMSAA=false;
        var target=new RenderTexture(320,240,24,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);target.Create();camera.targetTexture=target;
        // The disk bank stores RGBA bytes; NativeTextureBank exposes ARGB
        // words to both renderers. Decode the same fixture instead of supplying
        // a separate literal with the red and blue channels reversed.
        uint rgba;
        using(var textureReader=new BinaryReader(File.OpenRead(Path.Combine(folder,"panel.idastex")))) {
            textureReader.BaseStream.Position=32;rgba=textureReader.ReadUInt32();
        }
        uint argb=(rgba&0xff00ff00u)|((rgba&255)<<16)|((rgba>>16)&255);
        IntPtr texel=Marshal.AllocHGlobal(4);Marshal.WriteInt32(texel,unchecked((int)argb));
        var textureType=typeof(Idas3SceneRenderer).GetNestedType("SceneTexture",BindingFlags.NonPublic);
        object record=Activator.CreateInstance(textureType);textureType.GetField("width").SetValue(record,1u);textureType.GetField("height").SetValue(record,1u);
        textureType.GetField("pixelCount").SetValue(record,1ul);textureType.GetField("argb").SetValue(record,texel);
        var records=Array.CreateInstance(textureType,1);records.SetValue(record,0);Field("textureRecords").SetValue(renderer,records);
        var rangeType=typeof(Idas3SceneRenderer).GetNestedType("SceneRange",BindingFlags.NonPublic);
        var configure=typeof(Idas3SceneRenderer).GetMethod("ConfigureMaterial",BindingFlags.Instance|BindingFlags.NonPublic);
        try {
            using(var frameBuffer=new ComputeBuffer(46,16))using(var lightBuffer=new ComputeBuffer(312,16))using(var fogBuffer=new ComputeBuffer(34,16))
            using(var reader=new BinaryReader(File.OpenRead(Path.Combine(folder,"panels.bin"))))
            using(var csv=new StreamWriter(Path.Combine(outputFolder,"pixels.csv"))) {
                if(reader.ReadUInt32()!=0x314e504c||reader.ReadUInt32()!=36||reader.ReadUInt32()!=320||reader.ReadUInt32()!=240)throw new Exception("Invalid panel fixture");
                csv.WriteLine("course,condition,changedPixels,errorsOver2,interiorErrorsOver2,maxError");
                for(int test=0;test<36;++test){
                    uint course=reader.ReadUInt32(),condition=reader.ReadUInt32();int nv=reader.ReadInt32(),nr=reader.ReadInt32();
                    var frames=new Vector4[46];for(int i=0;i<46;++i)frames[i]=V4(reader);frameBuffer.SetData(frames);
                    var lights=new UInt4[312];for(int i=0;i<312;++i)lights[i]=U4(reader);lightBuffer.SetData(lights);
                    var fog=new UInt4[34];for(int i=0;i<34;++i)fog[i]=U4(reader);fogBuffer.SetData(fog);
                    var positions=new Vector3[nv];var normals=new Vector3[nv];var colors=new UnityEngine.Color[nv];var uv=new Vector2[nv];var offsets=new List<Vector4>(nv);
                    for(int i=0;i<nv;++i){positions[i]=V3(reader);normals[i]=V3(reader);colors[i]=V4(reader);uv[i]=new Vector2(reader.ReadSingle(),reader.ReadSingle());offsets.Add(V4(reader));}
                    var ranges=new object[nr];for(int i=0;i<nr;++i){var bytes=reader.ReadBytes(64);var pin=GCHandle.Alloc(bytes,GCHandleType.Pinned);try{ranges[i]=Marshal.PtrToStructure(pin.AddrOfPinnedObject(),rangeType);}finally{pin.Free();}}
                    var expected=new uint[320*240];for(int i=0;i<expected.Length;++i)expected[i]=reader.ReadUInt32();
                    var mesh=new Mesh();mesh.vertices=positions;mesh.normals=normals;mesh.colors=colors;mesh.uv=uv;mesh.SetUVs(1,offsets);mesh.subMeshCount=nr;
                    var materials=new List<Material>();var cmd=new CommandBuffer();
                    cmd.SetGlobalBuffer("_IdasFrameWords",frameBuffer);cmd.SetGlobalBuffer("_IdasLightWords",lightBuffer);cmd.SetGlobalBuffer("_IdasFogWords",fogBuffer);
                    cmd.SetGlobalInt("_IdasView",0);cmd.SetGlobalVector("_IdasDepthProjection",Vector4.zero);
                    for(int i=0;i<nr;++i){object r=ranges[i];int first=(int)(uint)rangeType.GetField("first").GetValue(r),count=(int)(uint)rangeType.GetField("count").GetValue(r);
                        var indices=new int[count];for(int j=0;j<count;++j)indices[j]=first+j;mesh.SetIndices(indices,MeshTopology.Triangles,i,false);
                        var material=new Material(Shader.Find("IDAS3/Original Scene Material"));configure.Invoke(renderer,new object[]{material,r,2000});materials.Add(material);cmd.DrawMesh(mesh,Matrix4x4.identity,material,i,0);
                    }
                    camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,cmd);camera.Render();camera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque,cmd);
                    var previous=RenderTexture.active;RenderTexture.active=target;var image=new Texture2D(320,240,TextureFormat.RGBA32,false,true);image.ReadPixels(new Rect(0,0,320,240),0,0);image.Apply();RenderTexture.active=previous;
                    var actual=image.GetPixels32();int bad=0,interior=0,changed=0,max=0;var nativePixels=new Color32[expected.Length];
                    for(int y=0;y<240;++y)for(int x=0;x<320;++x){int at=y*320+x;var e=Color(expected[at]);nativePixels[(239-y)*320+x]=e;var a=actual[(239-y)*320+x];
                        int error=Math.Max(Math.Abs(a.r-e.r),Math.Max(Math.Abs(a.g-e.g),Math.Abs(a.b-e.b)));max=Math.Max(max,error);if(expected[at]!=expected[0])++changed;
                        if(error>2){++bad;bool inside=x>1&&x<318&&y>1&&y<238;bool ink=expected[at]!=expected[0];
                            if(inside)for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)inside&=(expected[at+dy*320+dx]!=expected[0])==ink;
                            if(inside)++interior;
                        }
                    }
                    File.WriteAllBytes(Path.Combine(outputFolder,$"{course}-{condition}-unity.png"),image.EncodeToPNG());image.SetPixels32(nativePixels);image.Apply();File.WriteAllBytes(Path.Combine(outputFolder,$"{course}-{condition}-native.png"),image.EncodeToPNG());
                    csv.WriteLine($"{course},{condition},{changed},{bad},{interior},{max}");++report.cases;report.comparedPixels+=expected.Length;report.changedPixels+=changed;report.errorsOver2+=bad;report.interiorErrorsOver2+=interior;report.maxError=Math.Max(report.maxError,max);
                    cmd.Release();UnityEngine.Object.DestroyImmediate(image);UnityEngine.Object.DestroyImmediate(mesh);foreach(var material in materials)UnityEngine.Object.DestroyImmediate(material);
                }
                if(reader.BaseStream.Position!=reader.BaseStream.Length)throw new Exception("Trailing panel bytes");
            }
            report.passed=report.interiorErrorsOver2==0;
            if(!report.passed)throw new Exception("Unity/native panel mismatch; inspect exported images and pixels.csv");
        }catch(Exception e){report.error=e.ToString();throw;}
        finally{
            File.WriteAllText(Path.Combine(outputFolder,"summary.json"),JsonUtility.ToJson(report,true));Marshal.FreeHGlobal(texel);
            var cache=(Dictionary<ulong,Texture2D>)Field("textureCache").GetValue(renderer);foreach(var tex in cache.Values)UnityEngine.Object.DestroyImmediate(tex);cache.Clear();
            camera.targetTexture=null;target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(go);
        }
        Debug.Log($"PASS Unity lighting panels: {report.cases} frames, {report.errorsOver2} pixels differ by >2, {report.interiorErrorsOver2} interior errors");
    }
}
