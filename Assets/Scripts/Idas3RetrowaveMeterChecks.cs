using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;
using UnityEngine.Rendering;

// Explicit source/composition and GPU regressions; never run during gameplay.
public static class Idas3RetrowaveMeterChecks
{
    static Idas3ArcadeHud.Telemetry Data(int grade=-1)=>new Idas3ArcadeHud.Telemetry {
        size=40,version=4,flags=1u|(6u<<16)|(grade<0?0:8u|((uint)grade<<8)),
        gear=3,speedKmh=123,rpm=6500,revLimit=8500,throttle=.5f,brake=.25f,driftOpacity=grade<0?0:1
    };
    public static string RunChecks(){
        int checks=0;
        void Check(bool condition,string message){++checks;if(!condition)throw new InvalidOperationException("Retrowave: "+message);}
        var meter=Idas3ArcadeMeterCatalog.Get(78);
        Check(meter!=null&&meter.id==76,"missing source meter");
        var outline=meter.layers.Single(l=>l.name=="DriftNeon");
        Check(outline.role=="static"&&outline.curves.Length==0&&outline.switchers.Length==0,"permanent neon outline incorrectly follows drift events");
        var grid=meter.layers.Single(l=>l.name=="Grid");
        Check(Idas3ImportedMeter.TryRetainerProjection(grid,out var inverse),"missing grid perspective");
        var destinations=new[]{new Vector3(.3f,1,0),new Vector3(.7f,1,0),new Vector3(1,0,0),Vector3.zero};
        var sources=new[]{new Vector3(0,1,0),new Vector3(1,1,0),new Vector3(1,0,0),Vector3.zero};
        for(int i=0;i<4;++i)Check(Vector3.Distance(inverse.MultiplyPoint(destinations[i]),sources[i])<.0001f,"retainer corner "+i);
        Check(Mathf.Abs(inverse.MultiplyPoint(new Vector3(.5f,.5f,0)).y-2f/7)<.0001f,"grid uses affine rather than perspective depth");
        Check(inverse.MultiplyPoint(new Vector3(.1f,.95f,0)).x<0,"outside-trapezoid sample is not rejected");
        Check(!Idas3ImportedMeter.TryRetainerProjection(outline,out _),"projection leaked into an ordinary image");
        var options=new Idas3GameOptions.Values{hudMeterStyle=78};
        var sprites=new List<Idas3ArcadeHud.Sprite>();
        using(var renderer=new Idas3ImportedMeter()){
            for(int grade=-1;grade<4;++grade){
                sprites.Clear();renderer.Compose(sprites,meter,options,Data(grade),grade+2);
                Check(sprites.Count(s=>s.texture.name=="T_Meter76_DriftRmp_Outline"&&s.color.a==1)==1,"outline missing/duplicated at grade "+grade);
                Check(grade<0?renderer.DriftSpriteCount==0:renderer.DriftSpriteCount>0,"drift lamps at grade "+grade);
                var projected=sprites.Single(s=>s.texture.name=="T_Meter76_Grid");
                Check(projected.projective2.z==1&&Mathf.Abs(projected.projective2.y)>0,"renderer dropped perspective parameters");
            }
        }
        var transmission=meter.layers.Single(l=>l.role=="transmission");
        var data=Data();
        Check(Idas3ImportedMeter.SelectTexture(transmission,data).EndsWith("_02"),"manual transmission label");
        data.flags|=2;
        Check(Idas3ImportedMeter.SelectTexture(transmission,data).EndsWith("_01"),"automatic transmission label");
        var face=meter.layers.Single(l=>l.name=="Center_Meter");
        foreach(int limit in new[]{7500,8500,9500,12000}){
            data.revLimit=limit;
            var selected=Idas3ImportedMeter.SelectTexture(face,data);
            Check(face.textureVariants.Any(v=>v.texture==selected&&v.maxRpm==Idas3ArcadeHud.TachMaximum(limit)),"incorrect RPM scale "+limit);
        }
        return "PASS "+checks+" Retrowave source/composition checks";
    }

    public static void VerifyGpu(string output){
        Directory.CreateDirectory(output);
        var meter=Idas3ArcadeMeterCatalog.Get(78);var original=meter.layers;
        var options=new Idas3GameOptions.Values{hudMeterStyle=78};options.SetHudSizePercent(2,150);
        Idas3MeterLayoutBounds.Get(78);
        var host=new GameObject("Retrowave GPU verification");var camera=host.AddComponent<Camera>();camera.enabled=false;camera.cullingMask=0;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;camera.allowHDR=camera.allowMSAA=false;
        var target=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
        var pixels=new Texture2D(1280,720,TextureFormat.RGB24,false);var commands=new CommandBuffer();camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,commands);
        Color32[] Capture(Idas3ArcadeHud hud,string name,float seconds,Idas3ArcadeHud.Telemetry data){
            commands.Clear();hud.Build(options,data,1280,720,seconds,true,out _);hud.Render(commands,1280,720);camera.Render();
            var previous=RenderTexture.active;
            try{RenderTexture.active=target;pixels.ReadPixels(new Rect(0,0,1280,720),0,0);pixels.Apply();}finally{RenderTexture.active=previous;}
            if(name!=null)File.WriteAllBytes(Path.Combine(output,name+".png"),pixels.EncodeToPNG());
            return pixels.GetPixels32();
        }
        int Difference(Color32[] a,Color32[] b)=>a.Zip(b,(x,y)=>Math.Max(Math.Abs(x.r-y.r),Math.Max(Math.Abs(x.g-y.g),Math.Abs(x.b-y.b)))>8?1:0).Sum();
        try{
            using(var hud=new Idas3ArcadeHud()){
                for(int grade=-1;grade<4;++grade)Capture(hud,"drift-"+(grade+1),grade+2,Data(grade));
                var grid=original.Single(l=>l.name=="Grid");meter.layers=new[]{grid};
                var first=Capture(hud,"grid-perspective",.125f,Data());
                var animated=Capture(hud,"grid-scrolling",.193f,Data());
                if(Difference(first,animated)<30)throw new InvalidOperationException("Retrowave grid does not scroll");
                if(Difference(animated,Capture(hud,null,.193f,Data()))!=0)throw new InvalidOperationException("Retrowave grid advances while paused");
                var retainers=grid.retainers;
                try{
                    grid.retainers=Array.Empty<Idas3ArcadeMeterCatalog.Retainer>();
                    var flat=Capture(hud,"grid-flat-control",.125f,Data());
                    if(Difference(first,flat)<100)throw new InvalidOperationException("HUD shader ignored the grid perspective (stale shader build?)");
                }finally{grid.retainers=retainers;}
                meter.layers=new[]{original.Single(l=>l.name=="DriftNeon")};
                var idle=Capture(hud,"neon-idle",1,Data());var drifting=Capture(hud,"neon-drifting",2,Data(3));
                if(idle.Count(p=>Math.Max(p.r,Math.Max(p.g,p.b))>20)<100||Difference(idle,drifting)!=0)
                    throw new InvalidOperationException("Permanent neon outline is missing or still tied to drift");
            }
            File.WriteAllText(Path.Combine(output,"PASS.txt"),"PASS: idle neon, four drift grades, scrolling perspective grid, frozen clock, unprojected control");
        }finally{
            meter.layers=original;camera.RemoveAllCommandBuffers();commands.Dispose();camera.targetTexture=null;target.Release();
            UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(pixels);UnityEngine.Object.DestroyImmediate(host);
        }
    }
}
