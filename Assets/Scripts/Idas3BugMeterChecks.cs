using System;
using System.IO;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

public static class Idas3BugMeterChecks
{
    public static void Run(){
        Debug.Log(Idas3MeterSignalChecks.RunChecks());
        string output=Path.GetFullPath("Verification/discord-bugs-20260927/meter-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss"));Directory.CreateDirectory(output);
        int checks=0;void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
        var go=new GameObject("Meter palette GPU check");var camera=go.AddComponent<Camera>();camera.enabled=false;camera.cullingMask=0;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;camera.allowHDR=camera.allowMSAA=false;
        var target=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
        var pixels=new Texture2D(1280,720,TextureFormat.RGB24,false);var commands=new CommandBuffer();camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,commands);
        try{
            foreach(int id in new[]{0,1,3,4,8,9,10,11,12,13,14,15,19,29,35,37,57,63,68,117}){
                var meter=Idas3ArcadeMeterCatalog.Get(id+2);var original=meter.layers;
                meter.layers=original.Where(l=>l.role=="speed1").ToArray();
                try{
                    foreach(int band in new[]{0,1,2,3})using(var renderer=new Idas3ArcadeHud()){
                        float speed=new[]{88f,100f,180f,220f}[band];var options=new Idas3GameOptions.Values{hudMeterStyle=id+2};
                        commands.Clear();renderer.Build(options,new Idas3ArcadeHud.Telemetry{size=40,version=4,flags=1|(6u<<16),revLimit=8500,speedKmh=speed,gear=5},1280,720,0,true,out _);
                        renderer.Render(commands,1280,720);camera.Render();var previous=RenderTexture.active;
                        try{RenderTexture.active=target;pixels.ReadPixels(new Rect(0,0,1280,720),0,0);pixels.Apply();}finally{RenderTexture.active=previous;}
                        int red=0,yellow=0,blue=0;
                        foreach(var p in pixels.GetPixels32()){
                            if(p.r>80&&p.r>p.g*2&&p.r>p.b*2)++red;
                            if(p.r>80&&p.g>70&&p.b<Math.Min(p.r,p.g)*.5f)++yellow;
                            if(p.b>80&&p.b>p.r*2)++blue;
                        }
                        Check((band==1?yellow:band==2?blue:red)>10,$"{meter.name} lacks visible palette band {band} (red={red},yellow={yellow},blue={blue})");
                        File.WriteAllBytes(Path.Combine(output,$"{id}-{band}.png"),pixels.EncodeToPNG());
                    }
                }finally{meter.layers=original;commands.Clear();}
            }
            GearPixels(Check,output);
            Idas8ImportedShadowChecks.Gpu(Check,output);
            File.WriteAllText(Path.Combine(output,"PASS.txt"),checks+" GPU palette, maximum-gear and imported shadow checks passed");Debug.Log("PASS "+checks+" meter/shadow bug checks: "+output);
        }finally{camera.RemoveAllCommandBuffers();commands.Dispose();camera.targetTexture=null;target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(pixels);UnityEngine.Object.DestroyImmediate(go);}
    }
    internal static void GearPixels(Action<bool,string> Check,string output){
        var go=new GameObject("Gear image regression");var camera=go.AddComponent<Camera>();camera.enabled=false;camera.cullingMask=0;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;camera.allowHDR=camera.allowMSAA=false;
        var target=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
        var pixels=new Texture2D(1280,720,TextureFormat.RGB24,false);var commands=new CommandBuffer();camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,commands);
        int affected=0;
        try{
            for(int index=0;index<Idas3ArcadeMeterCatalog.Count;++index){int style=Idas3ArcadeMeterCatalog.StyleAt(index);var meter=Idas3ArcadeMeterCatalog.Get(style);if(meter==null)continue;
                var gear=meter.layers.Where(l=>l.role=="gear"&&l.texture.Contains("T_Meter01_ShiftNum")).ToArray();if(gear.Length==0)continue;
                ++affected;var original=meter.layers;meter.layers=gear;
                try{foreach(int maximum in new[]{5,6})foreach(int digit in new[]{4,5,6})using(var hud=new Idas3ArcadeHud()){
                    commands.Clear();hud.Build(new Idas3GameOptions.Values{hudMeterStyle=style},new Idas3ArcadeHud.Telemetry{size=40,version=4,flags=1|((uint)maximum<<16),gear=digit,revLimit=8500,speedKmh=80},1280,720,0,true,out _);
                    hud.Render(commands,1280,720);camera.Render();var previous=RenderTexture.active;
                    try{RenderTexture.active=target;pixels.ReadPixels(new Rect(0,0,1280,720),0,0);pixels.Apply();}finally{RenderTexture.active=previous;}
                    int lit=pixels.GetPixels32().Count(p=>Math.Max(p.r,Math.Max(p.g,p.b))>50);
                    Check(lit>30,$"{meter.name} {maximum}-speed gear {digit} is blank ({lit} lit pixels)");
                    if(digit==5&&maximum==6)File.WriteAllBytes(Path.Combine(output,$"gear-{meter.id}.png"),pixels.EncodeToPNG());
                }}finally{meter.layers=original;commands.Clear();}
            }
            Check(affected==26,"Expected shared gear atlases were not present: "+affected);
            File.AppendAllText(Path.Combine(output,"report.txt"),affected+" affected meters rendered in gears 4,5,6 for five- and six-speed cars\n");
        }finally{camera.RemoveAllCommandBuffers();commands.Dispose();camera.targetTexture=null;target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(pixels);UnityEngine.Object.DestroyImmediate(go);}
    }
}
