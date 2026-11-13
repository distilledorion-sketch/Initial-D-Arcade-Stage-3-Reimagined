using System;
using System.IO;
using UnityEditor;
using UnityEngine;
using UnityEngine.UI;

public static class Idas3ChallengerAnimationChecks
{
    public static void Run()
    {
        const string output="Verification/challenger-animation-20261110/gpu";
        Directory.CreateDirectory(output);
        int renders=0;
        foreach(var size in new[]{new Vector2Int(640,480),new Vector2Int(1280,720)}){
            var cameraGo=new GameObject("Private challenger audit camera");var camera=cameraGo.AddComponent<Camera>();
            camera.enabled=false;camera.orthographic=true;camera.clearFlags=CameraClearFlags.SolidColor;
            camera.backgroundColor=new Color(.25f,.25f,.25f,1);camera.cullingMask=1<<29;camera.allowHDR=false;camera.allowMSAA=false;
            var target=new RenderTexture(size.x,size.y,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
            var canvasGo=new GameObject("Private challenger audit canvas",typeof(RectTransform),typeof(Canvas));canvasGo.layer=29;
            var canvas=canvasGo.GetComponent<Canvas>();canvas.renderMode=RenderMode.ScreenSpaceCamera;canvas.worldCamera=camera;canvas.planeDistance=1;
            canvas.scaleFactor=Mathf.Min(size.x/640f,size.y/480f);
            var safeGo=new GameObject("640 by 480 safe area",typeof(RectTransform));safeGo.layer=29;safeGo.transform.SetParent(canvasGo.transform,false);
            var safe=safeGo.GetComponent<RectTransform>();safe.anchorMin=safe.anchorMax=new Vector2(.5f,.5f);safe.sizeDelta=new Vector2(640,480);
            var bannerGo=new GameObject("Production challenger banner",typeof(RectTransform),typeof(Idas3ChallengerBanner));bannerGo.layer=29;bannerGo.transform.SetParent(safe,false);
            var rect=bannerGo.GetComponent<RectTransform>();rect.anchorMin=rect.anchorMax=rect.pivot=new Vector2(0,1);rect.sizeDelta=new Vector2(640,480);
            var banner=bannerGo.GetComponent<Idas3ChallengerBanner>();banner.Initialize();
            Color32[] settled=null,flash=null;
            foreach(int frame in new[]{0,6,10,20,40,48,56,64,180}){
                banner.ApplyFrame(frame);Canvas.ForceUpdateCanvases();camera.Render();
                var old=RenderTexture.active;RenderTexture.active=target;var picture=new Texture2D(size.x,size.y,TextureFormat.RGBA32,false);
                picture.ReadPixels(new Rect(0,0,size.x,size.y),0,0);picture.Apply();RenderTexture.active=old;
                var pixels=picture.GetPixels32();if(frame==48)flash=pixels;if(frame==64)settled=pixels;
                int colored=0;foreach(var p in pixels)if(Math.Abs(p.r-64)>2||Math.Abs(p.g-64)>2||Math.Abs(p.b-64)>2)colored++;
                if(frame==0&&colored!=0)throw new Exception("Source frame 0 should remain offscreen");
                if(frame>=40&&colored<20000)throw new Exception("Production banner did not render its source geometry");
                File.WriteAllBytes(Path.Combine(output,size.x+"x"+size.y+"-"+frame+".png"),picture.EncodeToPNG());UnityEngine.Object.DestroyImmediate(picture);renders++;
            }
            int changed=0;for(int i=0;i<settled.Length;i++)if(Math.Abs(settled[i].r-flash[i].r)>8||Math.Abs(settled[i].g-flash[i].g)>8||Math.Abs(settled[i].b-flash[i].b)>8)changed++;
            if(changed<500)throw new Exception("Source flash envelope did not affect production pixels");
            UnityEngine.Object.DestroyImmediate(canvasGo);camera.targetTexture=null;target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(cameraGo);
        }
        File.WriteAllText(Path.Combine(output,"PASS.txt"),"PASS "+renders+" production banner renders at 4:3 and widescreen. Numeric source-frame/projection checks are separate. Background effect and live network transition are not established here.\n");
        Debug.Log("PASS original challenger banner GPU renders: "+renders);
    }
}
