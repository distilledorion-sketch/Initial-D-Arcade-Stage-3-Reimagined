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
        SpeedColorPixels(Check,output);
        GearPixels(Check,output);
        Idas8ImportedShadowChecks.Gpu(Check,output);
        File.WriteAllText(Path.Combine(output,"PASS.txt"),checks+" GPU palette, maximum-gear and imported shadow checks passed");Debug.Log("PASS "+checks+" meter/shadow bug checks: "+output);
    }
    public static void VerifySpeedColors(string output){
        Directory.CreateDirectory(output);
        string signals=Idas3MeterSignalChecks.RunChecks();Debug.Log(signals);
        int checks=0;void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
        SpeedColorPixels(Check,output);
        string report=signals+"\nPASS "+checks+" GPU speed color checks: 54 corrected fixed-color HUDs and 23 authored color families";
        File.WriteAllText(Path.Combine(output,"PASS.txt"),report);Debug.Log(report);
    }
    static void SpeedColorPixels(Action<bool,string> Check,string output){
        int[] fixedIds={1,3,4,8,9,10,11,12,13,14,15,19,20,21,25,26,27,28,29,34,35,36,37,42,57,58,63,64,65,68,69,70,78,79,80,81,82,87,88,89,90,91,93,94,95,96,97,105,106,107,108,109,111,117};
        var go=new GameObject("Meter palette GPU check");var camera=go.AddComponent<Camera>();camera.enabled=false;camera.cullingMask=0;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new Color(.18f,.18f,.18f);camera.allowHDR=camera.allowMSAA=false;
        var target=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
        var pixels=new Texture2D(1280,720,TextureFormat.RGB24,false);var commands=new CommandBuffer();camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,commands);
        Color32[] Capture(int style,float speed,float seconds,string filename=null){
            using(var renderer=new Idas3ArcadeHud()){
                var options=new Idas3GameOptions.Values{hudMeterStyle=style};
                if(filename=="double-ace-199")options.SetHudSizePercent(2,200);
                commands.Clear();renderer.Build(options,new Idas3ArcadeHud.Telemetry{size=40,version=4,flags=1|(6u<<16),revLimit=8500,rpm=3500,speedKmh=speed,gear=6},1280,720,seconds,true,out _);
                renderer.Render(commands,1280,720);camera.Render();var previous=RenderTexture.active;
                try{RenderTexture.active=target;pixels.ReadPixels(new Rect(0,0,1280,720),0,0);pixels.Apply();}finally{RenderTexture.active=previous;}
                if(filename!=null)File.WriteAllBytes(Path.Combine(output,filename+".png"),pixels.EncodeToPNG());
                return pixels.GetPixels32();
            }
        }
        int fixedCount=0,coloredCount=0;
        try{
            for(int index=0;index<Idas3ArcadeMeterCatalog.Count;++index){
                int style=Idas3ArcadeMeterCatalog.StyleAt(index);var meter=Idas3ArcadeMeterCatalog.Get(style);if(meter==null)continue;
                var digit=meter.layers.FirstOrDefault(l=>l.role=="speed1");if(digit==null)continue;
                bool fixedColor=fixedIds.Contains(meter.id);
                if(!fixedColor&&digit.speedTextures.Length!=4)continue;
                if(fixedColor){++fixedCount;Check(digit.speedTextures.Length==0,meter.name+" still has synthetic speed bands");}else ++coloredCount;
                var original=meter.layers;Color32[] first=null;
                meter.layers=original.Where(l=>l.role=="speed1").ToArray();
                try{
                    foreach(int band in new[]{0,1,2,3}){
                        // The same digit in each speed band makes pixel equality meaningful.
                        float speed=new[]{88f,98f,158f,218f}[band];
                        var image=Capture(style,speed,fixedColor?band*.31f:0,new[]{0,1,3,4,58,68}.Contains(meter.id)?$"{meter.id}-{band}":null);
                        var background=image[0];
                        Check(image.Count(p=>Math.Max(Math.Abs(p.r-background.r),Math.Max(Math.Abs(p.g-background.g),Math.Abs(p.b-background.b)))>8)>10,meter.name+" has no visible speed digit");
                        if(fixedColor){
                            if(first==null)first=image;
                            Check(first.SequenceEqual(image),meter.name+" changed its fixed digit color in band "+band);
                            // The original white digit has a pale blue bevel;
                            // preserve that artwork, but reject a solid speed tint.
                            if(meter.id==58)Check(image.Count(p=>p.r>240&&p.g>240&&p.b>240)>30,"Double Ace's white digit face was recolored");
                            continue;
                        }
                        if(band<3||!digit.speedRainbow){
                            // Render the source atlas directly as a control, bypassing
                            // speed selection. Preserve pastel/white artwork as-is.
                            string texture=digit.texture;var variants=digit.speedTextures;
                            try{
                                digit.texture=variants[band];digit.speedTextures=Array.Empty<string>();
                                Check(image.SequenceEqual(Capture(style,98,0)),meter.name+" recolored authored speed artwork in band "+band);
                            }finally{digit.texture=texture;digit.speedTextures=variants;}
                            continue;
                        }
                        int red=0,yellow=0,blue=0;
                        foreach(var p in image){
                            if(p.r>80&&p.r>p.g*2&&p.r>p.b*2)++red;
                            if(p.r>80&&p.g>70&&p.b<Math.Min(p.r,p.g)*.5f)++yellow;
                            if(p.b>80&&p.b>p.r*2)++blue;
                        }
                        Check((band==1?yellow:band==2?blue:red)>10,$"{meter.name} lacks visible palette band {band} (red={red},yellow={yellow},blue={blue})");
                    }
                }finally{meter.layers=original;commands.Clear();}
            }
            Check(fixedCount==54&&coloredCount==23,"Speed color coverage: "+fixedCount+" fixed, "+coloredCount+" colored");
            Capture(60,199,0,"double-ace-199");
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
