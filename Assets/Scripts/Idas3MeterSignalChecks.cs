using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.Rendering;

public static class Idas3MeterSignalChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static void RenderPlacementPreviews(){
        Debug.Log(RunChecks());
        string output=Path.GetFullPath("Verification/discord-bugs-20260926/meter-placement");Directory.CreateDirectory(output);
        var host=new GameObject("Meter placement QA");var camera=host.AddComponent<Camera>();camera.enabled=false;camera.cullingMask=0;
        camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=new Color(.025f,.03f,.045f);camera.allowHDR=camera.allowMSAA=false;
        var target=new RenderTexture(1280,720,24,RenderTextureFormat.ARGB32);target.Create();camera.targetTexture=target;
        var pixels=new Texture2D(1280,720,TextureFormat.RGB24,false);var commands=new CommandBuffer();camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,commands);
        try{
            foreach(int style in new[]{1,12,60})foreach(bool chase in new[]{false,true})using(var renderer=new Idas3ArcadeHud()){
                var options=new Idas3GameOptions.Values{hudMeterStyle=style};
                commands.Clear();renderer.Build(options,new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,rpm=6000,revLimit=8500,speedKmh=160,gear=4,throttle=.7f},1280,720,.4f,true,out _,chase);
                renderer.Render(commands,1280,720);camera.Render();var previous=RenderTexture.active;
                try{RenderTexture.active=target;pixels.ReadPixels(new Rect(0,0,1280,720),0,0);pixels.Apply();}finally{RenderTexture.active=previous;}
                File.WriteAllBytes(Path.Combine(output,style+"-"+(chase?"chase":"bumper")+".png"),pixels.EncodeToPNG());
            }
        }finally{camera.RemoveCommandBuffer(CameraEvent.BeforeForwardOpaque,commands);commands.Dispose();camera.targetTexture=null;UnityEngine.Object.DestroyImmediate(pixels);target.Release();UnityEngine.Object.DestroyImmediate(target);UnityEngine.Object.DestroyImmediate(host);}
        Debug.Log("Meter placement GPU previews: "+output);
    }
    public static string RunChecks(){
        int checks=0;void Check(bool ok,string why){++checks;if(!ok)throw new InvalidOperationException(why);}
        foreach(float limit in new[]{7500f,8500f,9500f,12000f}){
            var t=new Idas3ArcadeHud.Telemetry{revLimit=limit,rpm=limit-1000};
            Check(Idas3ArcadeHud.ShiftWarning(t)==0,"Shift warning starts before the approach range");
            t.rpm=limit-750;Check(Mathf.Abs(Idas3ArcadeHud.ShiftWarning(t)-.5f)<.001f,"Shift warning is not gradual");
            t.rpm=limit-500;Check(Idas3ArcadeHud.ShiftWarning(t)==1,"Shift warning cannot reach full brightness at attainable RPM");
        }
        float[] speeds={0,89.99f,90,149.99f,150,209.99f,210,300};
        int[] bands={0,0,1,1,2,2,3,3};
        int families=0;
        for(int i=0;i<Idas3ArcadeMeterCatalog.Count;++i){
            var meter=Idas3ArcadeMeterCatalog.Get(Idas3ArcadeMeterCatalog.StyleAt(i));if(meter==null)continue;
            var layer=Array.Find(meter.layers,l=>l.role=="speed1");
            if(layer==null||layer.speedTextures.Length!=4)continue;
            ++families;
            using(var renderer=new Idas3ImportedMeter()){
                var options=new Idas3GameOptions.Values{hudMeterStyle=meter.id+2};
                var sprites=new List<Idas3ArcadeHud.Sprite>();
                for(int n=0;n<speeds.Length;++n){
                    var t=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,revLimit=8500,rpm=4000,gear=3,speedKmh=speeds[n]};
                    var expected=Resources.Load<Texture2D>(layer.speedTextures[bands[n]]);
                    Check(expected,"Missing authored speed atlas");
                    sprites.Clear();renderer.Compose(sprites,meter,options,t,.125f);
                    Check(sprites.Exists(s=>s.texture==expected&&s.color.a>0),meter.name+" did not submit its speed-color atlas at "+speeds[n]);
                }
            }
        }
        Check(families>50,"Speed family coverage unexpectedly disappeared");
        int wideCount=0;
        for(int i=1;i<Idas3ArcadeMeterCatalog.Count;++i){
            var options=new Idas3GameOptions.Values{hudMeterStyle=Idas3ArcadeMeterCatalog.StyleAt(i)};
            foreach(var resolution in new[]{new Vector2(1280,720),new Vector2(1920,1080),new Vector2(1024,768)}){
                var bumper=Idas3ArcadeHud.MeterBounds(resolution.x,resolution.y,options);
                var chase=Idas3ArcadeHud.MeterBounds(resolution.x,resolution.y,options,true);
                Check(bumper.xMin>=0&&chase.xMin>=0&&bumper.xMax<=resolution.x&&chase.xMax<=resolution.x&&
                    bumper.yMin>=0&&chase.yMax<=resolution.y,"Default meter left the viewport");
                if(Idas3ArcadeHud.UsesWidePlacement(options)){
                    ++wideCount;Check(Mathf.Abs(bumper.center.x-resolution.x*.5f)<.01f&&chase.center.x<bumper.center.x,"Wide meter camera anchors did not change");
                    var legacy=options.Clone();legacy.hudMeterLayout=0;
                    Check(bumper.width>=Idas3ArcadeHud.MeterBounds(resolution.x,resolution.y,legacy).width,"Wide default is smaller than the legacy fit");
                }else Check(bumper==chase,"Compact meter changed camera anchors");
            }
            var saved=options.Clone();saved.hudMeterLayout=-1;saved.SetHudOffset(2,new Vector2(-.18f,-.07f));saved.SetHudSizePercent(2,123);
            saved=Idas3GameOptions.Normalize(saved);Check(saved.hudMeterLayout==0,"Migration moved a custom saved meter");
            Check(Idas3ArcadeHud.MeterBounds(1280,720,saved)==Idas3ArcadeHud.MeterBounds(1280,720,saved,true),"Legacy custom placement follows camera unexpectedly");
            var copy=new Idas3GameOptions.Values();Idas3GameOptions.CopyHudLayout(saved,copy);
            Check(copy.hudMeterLayout==0&&copy.HudOffset(2)==saved.HudOffset(2)&&copy.HudSizePercent(2)==123,"Layout Apply/Cancel copy lost saved meter placement");
        }
        Check(wideCount>30,"Wide meter placement coverage missing");
        // The source Double Ace set has only a neutral atlas. Check its live
        // digit tint, including a frozen clock and animated high-speed color.
        var doubleAce=Idas3ArcadeMeterCatalog.Get(60);
        using(var renderer=new Idas3ImportedMeter()){
            var options=new Idas3GameOptions.Values{hudMeterStyle=60};var sprites=new List<Idas3ArcadeHud.Sprite>();
            Color Digit(float speed,float time){
                sprites.Clear();renderer.Compose(sprites,doubleAce,options,new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,revLimit=8500,rpm=4000,gear=3,speedKmh=speed},time);
                return sprites.Find(s=>s.texture&&s.texture.name=="T_Meter58_SpdNum04").color;
            }
            var red=Digit(89,0);var yellow=Digit(90,0);var blue=Digit(150,0);
            Check(red.r>red.g*5&&yellow.r>yellow.b*5&&yellow.g>yellow.b*5&&blue.b>blue.r*5,"Double Ace speed tint bands missing");
            var rainbow=Digit(210,.2f);Check(rainbow==Digit(210,.2f),"Paused rainbow advanced");
            Check(rainbow!=Digit(210,.7f),"High-speed rainbow did not animate");
        }
        return "PASS "+checks+" meter signal checks across "+families+" authored speed families and four engine RPM ranges";
    }
}
