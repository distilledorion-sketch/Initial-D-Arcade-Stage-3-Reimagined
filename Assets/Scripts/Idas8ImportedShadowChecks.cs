using System;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

// Opt-in GPU regression in the isolated mode-flow player, not a startup test.
internal static class Idas8ImportedShadowChecks {
    static byte[] MaskDds(bool compressed){
        // Four increasing visibility columns. Exercise the same DDS decoder
        // as Sadamine (A8) and Hakone (DXT5), including compressed alpha.
        var b=new byte[144];
        Action<int,uint> word=(at,n)=>Array.Copy(BitConverter.GetBytes(n),0,b,at,4);
        word(0,0x20534444);word(4,124);word(12,4);word(16,4);word(28,1);word(76,32);
        if(compressed){
            word(80,4);word(84,0x35545844);b[128]=0;b[129]=255;
            ulong indices=0;int[] columns={0,2,5,1};
            for(int i=0;i<16;i++)indices|=(ulong)columns[i%4]<<(3*i);
            for(int i=0;i<6;i++)b[130+i]=(byte)(indices>>(8*i));
        }else{
            word(80,2);word(88,8);word(104,255);
            for(int i=0;i<16;i++)b[128+i]=(byte)(85*(i%4));
        }
        return b;
    }
    internal static void Gpu(Action<bool,string> check,string output){
        var shader=Resources.Load<Shader>("Idas8ImportedShadowCheck");
        check(shader!=null&&shader.isSupported,"Production imported fragment supported");
        var m=new Material(shader);
        var mesh=new Mesh();mesh.vertices=new[]{new Vector3(-1,-1,.5f),new Vector3(1,-1,.5f),new Vector3(1,1,.5f),new Vector3(-1,1,.5f)};
        mesh.uv=new[]{Vector2.zero,Vector2.right,Vector2.one,Vector2.up};
        mesh.uv2=new[]{Vector2.right,Vector2.zero,Vector2.up,Vector2.one};
        mesh.colors=new[]{Color.white,Color.white,Color.white,Color.white};mesh.triangles=new[]{0,1,2,0,2,3};
        var target=new RenderTexture(64,64,0,RenderTextureFormat.ARGB32,RenderTextureReadWrite.Linear);
        var pixels=new Texture2D(64,64,TextureFormat.RGBA32,false,true);var old=RenderTexture.active;
        var commands=new CommandBuffer{name="Imported shadow regression"};
        try{
            check(target.Create(),"Shadow GPU target");
            foreach(bool compressed in new[]{false,true}){
                var mask=Idas8HakoneCourse.DecodeTexture(MaskDds(compressed),compressed?"DXT5-test":"A8-test");
                try{
                    mask.filterMode=FilterMode.Point;mask.wrapMode=TextureWrapMode.Clamp;
                    int[] alpha=compressed?new[]{0,51,204,255}:new[]{0,85,170,255};
                    foreach(bool overlay in new[]{false,true})foreach(int uv in new[]{0,1}){
                        m.mainTexture=overlay?mask:Texture2D.whiteTexture;
                        m.SetTexture("_ImportedShadowTex",mask);m.SetFloat("_ImportedHasShadow",overlay?0:1);
                        m.SetFloat("_ImportedShadowOnly",overlay?1:0);m.SetFloat("_ImportedShadowUv",uv);
                        m.SetFloat("_SrcBlend",(float)(overlay?BlendMode.SrcAlpha:BlendMode.One));
                        m.SetFloat("_DstBlend",(float)(overlay?BlendMode.OneMinusSrcAlpha:BlendMode.Zero));
                        commands.Clear();commands.SetRenderTarget(target);commands.SetViewport(new Rect(0,0,64,64));
                        commands.ClearRenderTarget(true,true,Color.white);
                        commands.SetViewProjectionMatrices(Matrix4x4.identity,Matrix4x4.identity);
                        commands.SetGlobalVector("_ProjectionParams",new Vector4(1,.1f,100,.01f));
                        commands.DrawMesh(mesh,Matrix4x4.identity,m,0,0);Graphics.ExecuteCommandBuffer(commands);
                        RenderTexture.active=target;
                        pixels.ReadPixels(new Rect(0,0,64,64),0,0);pixels.Apply();
                        File.WriteAllBytes(Path.Combine(output,$"mask-{mask.name}-{(overlay?"overlay":"baked")}-uv{uv}.png"),pixels.EncodeToPNG());
                        for(int column=0;column<4;column++){
                            int a=alpha[!overlay&&uv==1?3-column:column];
                            float expected=overlay?a:255*.32f+.68f*a;
                            Color32 pixel=pixels.GetPixel(8+16*column,32);
                            check(Mathf.Abs(pixel.r-expected)<3&&Mathf.Abs(pixel.g-expected)<3&&Mathf.Abs(pixel.b-expected)<3,
                                $"{mask.name} {(overlay?"overlay":"baked")} UV{uv} visibility {a}: {pixel.r}, expected {expected}");
                        }
                    }
                }finally{UnityEngine.Object.Destroy(mask);}
            }
            m.SetColor("_ImportedUntexturedShadow",new Color(127/255f,127/255f,127/255f,1));
            m.SetFloat("_SrcBlend",(float)BlendMode.DstColor);m.SetFloat("_DstBlend",(float)BlendMode.Zero);
            foreach(var background in new[]{Color.white,new Color(.2f,.5f,.8f)}){
                commands.Clear();commands.SetRenderTarget(target);commands.SetViewport(new Rect(0,0,64,64));commands.ClearRenderTarget(true,true,background);
                commands.SetViewProjectionMatrices(Matrix4x4.identity,Matrix4x4.identity);commands.DrawMesh(mesh,Matrix4x4.identity,m,0,0);Graphics.ExecuteCommandBuffer(commands);
                RenderTexture.active=target;pixels.ReadPixels(new Rect(0,0,64,64),0,0);pixels.Apply();var actual=pixels.GetPixel(32,32);
                check(Mathf.Abs(actual.r-background.r*127/255f)<.02f&&Mathf.Abs(actual.g-background.g*127/255f)<.02f&&Mathf.Abs(actual.b-background.b*127/255f)<.02f,"Untextured wet shadow must darken the existing road without painting an opaque strip");
            }
        }finally{
            RenderTexture.active=old;target.Release();commands.Dispose();
            UnityEngine.Object.Destroy(target);UnityEngine.Object.Destroy(pixels);UnityEngine.Object.Destroy(mesh);UnityEngine.Object.Destroy(m);
        }
    }
    internal static void Scene(Action<bool,string> check,Idas8HakoneCourse course,string pack,bool uphill,string variant){
        check(course.LoadedCourse==pack&&course.LoadedVariant==variant,"Requested course and weather loaded");
        string root=Path.Combine(Idas3CourseCatalog.PackPath(pack),variant=="day_dry"?"":variant);
        var manifest=JsonUtility.FromJson<Idas8HakoneCourse.Manifest>(File.ReadAllText(Path.Combine(root,"scene.json")));
        var sourceMaterials=(Material[])typeof(Idas8HakoneCourse).GetField("materials",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(course);
        int up=0,down=0,shared=0,masked=0;
        foreach(var r in course.GetComponentsInChildren<MeshRenderer>()){
            if(!r.gameObject.activeInHierarchy||r.sharedMaterial==null)continue;
            var m=r.sharedMaterial;int direction=Idas8HakoneCourse.SceneryDirection(pack,m.name);
            if(direction!=0){
                if(direction>0)up++;else down++;
                check(r.enabled==((direction>0)==uphill),"Only the selected directional scenery renders: "+m.name);
            }else if(r.enabled)shared++;
            // Repeated source material names can carry different UV bindings.
            int materialIndex=Array.IndexOf(sourceMaterials,m);check(materialIndex>=0,"Renderer uses this variant's material");
            var surface=manifest.materials[materialIndex];
            foreach(var t in surface.textures)if(t.type==6){
                masked++;
                bool overlay=surface.textures.Length==1;
                check(m.GetFloat("_ImportedShadowOnly")== (overlay?1:0),"Type-6 overlay visibility routing");
                if(!overlay)check(m.GetFloat("_ImportedHasShadow")==1&&m.GetFloat("_ImportedShadowUv")==t.uv,"Authored shadow UV set retained");
            }
        }
        check(up>0&&down>0&&shared>0,"Both direction groups exist and common scenery remains visible");
        check(masked>0||pack=="HAKONE"&&variant.StartsWith("night"),"Expected source shadow materials inspected");
    }
}
