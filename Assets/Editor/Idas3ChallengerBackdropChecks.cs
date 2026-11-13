using System;
using System.IO;
using System.Collections.Generic;
using UnityEditor;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.UI;

public static class Idas3ChallengerBackdropChecks
{
    const string Root="Verification/challenger-animation-20261110";
    static Color32[] Read(RenderTexture target,string filename=null)
    {
        var old=RenderTexture.active;RenderTexture.active=target;
        var image=new Texture2D(target.width,target.height,TextureFormat.RGBA32,false);
        image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();RenderTexture.active=old;
        var pixels=image.GetPixels32();if(filename!=null)File.WriteAllBytes(filename,image.EncodeToPNG());
        UnityEngine.Object.DestroyImmediate(image);return pixels;
    }
    public static void Run()
    {
        string output=Environment.GetEnvironmentVariable("IDAS3_CHALLENGER_GPU_OUTPUT")??Root+"/backdrop-gpu";
        if(!Path.GetFullPath(output).StartsWith(Path.GetFullPath(Root)+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new Exception("Private GPU output must stay inside verification");
        Directory.CreateDirectory(output);
        var shades=new Dictionary<int,byte[]>();var positions=new float[50];
        foreach(var line in File.ReadAllLines(Root+"/backdrop-source.csv")){
            var a=line.Split(',');int tick=int.Parse(a[0]),index=int.Parse(a[1]);if((index&1)!=0)continue;
            if(!shades.ContainsKey(tick))shades[tick]=new byte[50];shades[tick][index/2]=byte.Parse(a[4]);
            positions[index/2]=BitConverter.ToSingle(BitConverter.GetBytes(uint.Parse(a[2])),0);
        }
        int checks=0,renders=0;
        foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1280,720)}){
            var target=new RenderTexture(size.x,size.y,24,RenderTextureFormat.ARGB32);target.Create();
            var sourceGo=new GameObject("Private frozen scene source");var source=sourceGo.AddComponent<Camera>();
            source.enabled=false;source.targetTexture=target;source.cullingMask=0;source.clearFlags=CameraClearFlags.SolidColor;
            source.allowHDR=false;source.allowMSAA=false;source.backgroundColor=Color.clear;
            var pattern=new Texture2D(64,48,TextureFormat.RGBA32,false);pattern.filterMode=FilterMode.Point;
            var colors=new Color32[64*48];
            for(int y=0;y<48;y++)for(int x=0;x<64;x++)colors[y*64+x]=new Color32((byte)(40+x*3),(byte)(60+y*3),(byte)(200-(x+y)%100),0);
            pattern.SetPixels32(colors);pattern.Apply();
            var paint=new CommandBuffer{name="Private reference pattern (zero alpha)"};paint.Blit(pattern,BuiltinRenderTextureType.CameraTarget);
            source.AddCommandBuffer(CameraEvent.AfterEverything,paint);
            var cameraGo=new GameObject("Private captured background camera");var camera=cameraGo.AddComponent<Camera>();
            camera.enabled=false;camera.targetTexture=target;camera.clearFlags=CameraClearFlags.Depth;camera.cullingMask=1<<29;
            camera.allowHDR=false;camera.allowMSAA=false;camera.orthographic=true;
            var canvasGo=new GameObject("Private backdrop canvas",typeof(RectTransform),typeof(Canvas));canvasGo.layer=29;
            var canvas=canvasGo.GetComponent<Canvas>();canvas.renderMode=RenderMode.ScreenSpaceCamera;canvas.worldCamera=camera;canvas.planeDistance=1;
            var badgeGo=new GameObject("Previous scene overlay badge",typeof(RectTransform),typeof(CanvasRenderer),typeof(RawImage));badgeGo.layer=29;badgeGo.transform.SetParent(canvasGo.transform,false);
            var badge=badgeGo.GetComponent<RawImage>();badge.texture=Texture2D.whiteTexture;badge.color=Color.green;
            badge.rectTransform.anchorMin=badge.rectTransform.anchorMax=new Vector2(.75f,.45f);badge.rectTransform.sizeDelta=new Vector2(120,36);
            var go=new GameObject("Production captured background",typeof(RectTransform),typeof(Idas3ChallengerBackdrop));go.layer=29;go.transform.SetParent(canvasGo.transform,false);
            var rect=go.GetComponent<RectTransform>();rect.anchorMin=Vector2.zero;rect.anchorMax=Vector2.one;rect.offsetMin=rect.offsetMax=Vector2.zero;
            go.SetActive(false);
            var backdrop=go.GetComponent<Idas3ChallengerBackdrop>();backdrop.BeginCapture(camera,size.x,size.y);
            // Capture the old UI too, before showing any challenge pixels.
            source.Render();Canvas.ForceUpdateCanvases();camera.Render();var baseline=Read(target);
            if(!backdrop.Captured)throw new Exception("Initial scene capture did not complete");
            badgeGo.SetActive(false);go.SetActive(true);
            foreach(int frame in new[]{0,6,10,20,40,60,90,180}){
                // Change the entire old scene after capture. Production must
                // keep the first picture, with no feedback/darkening accumulation.
                if(frame!=0){source.RemoveCommandBuffer(CameraEvent.AfterEverything,paint);source.backgroundColor=Color.magenta;source.Render();}
                backdrop.ApplyFrame(frame);Canvas.ForceUpdateCanvases();camera.Render();renders++;
                if(!backdrop.Captured||camera.GetCommandBuffers(CameraEvent.BeforeForwardOpaque).Length!=0||camera.GetCommandBuffers(CameraEvent.AfterEverything).Length!=0)throw new Exception("Scene copy was not one-shot");
                var actual=Read(target,output+"/"+size.x+"x"+size.y+"-"+frame+".png");
                var debugMesh=backdrop.canvasRenderer.GetMesh();
                Debug.Log("Backdrop diagnostic: frame="+frame+" rect="+rect.rect+" canvas="+canvas.pixelRect+" depth="+backdrop.depth+" cull="+backdrop.canvasRenderer.cull+" vertices="+debugMesh.vertexCount+" shader="+backdrop.materialForRendering.shader.name+" supported="+backdrop.materialForRendering.shader.isSupported+" center="+actual[actual.Length/2+size.x/2]);
                for(int y=13;y<size.y;y+=31)for(int x=17;x<size.x;x+=37){
                    float sx=(x+.5f)*640/size.x,sy=(size.y-y-.5f)*480/size.y;
                    float strip=sx+sy*160/512;int pair=0;while(pair<48&&strip>positions[pair+1])pair++;
                    float amount=(strip-positions[pair])/(positions[pair+1]-positions[pair]);
                    float c=Mathf.Lerp(shades[frame][pair],shades[frame][pair+1],amount)/255f,factor=c*c;
                    int at=y*size.x+x;var b=baseline[at];var a=actual[at];
                    if(Math.Abs(a.r-b.r*factor)>3||Math.Abs(a.g-b.g*factor)>3||Math.Abs(a.b-b.b*factor)>3||a.a!=255)
                        throw new Exception("Captured strip differs: size="+size+" frame="+frame+" pixel="+x+","+y+" expected="+b+" * "+factor+" actual="+a);
                    checks++;
                }
            }
            backdrop.EndCapture();backdrop.BeginCapture(camera,size.x,size.y);backdrop.EndCapture();
            if(camera.GetCommandBuffers(CameraEvent.BeforeForwardOpaque).Length!=0||camera.GetCommandBuffers(CameraEvent.AfterEverything).Length!=0)throw new Exception("Canceled capture retained GPU command");
            UnityEngine.Object.DestroyImmediate(canvasGo);source.RemoveCommandBuffer(CameraEvent.AfterEverything,paint);paint.Dispose();
            source.targetTexture=camera.targetTexture=null;target.Release();UnityEngine.Object.DestroyImmediate(target);
            UnityEngine.Object.DestroyImmediate(pattern);UnityEngine.Object.DestroyImmediate(sourceGo);UnityEngine.Object.DestroyImmediate(cameraGo);
        }
        string result="PASS "+renders+" production captured-background renders, "+checks+" pixel checks against executed source strip; previous overlay retained, opaque capture, one-shot copy, freeze and cancellation at 4:3/widescreen.";
        File.WriteAllText(output+"/PASS.txt",result);Debug.Log(result);
    }
}
