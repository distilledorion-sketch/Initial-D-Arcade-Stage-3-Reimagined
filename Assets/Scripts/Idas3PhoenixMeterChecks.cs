using System;
using System.Collections.Generic;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Sprite=Idas3ArcadeHud.Sprite;

// Phoenix's gear flash is a second live gear-number image in its source setup,
// not a static atlas cell. Check the actual production compositor independently
// of the isolated GPU visibility/event checks in Idas3MeterEffectsChecks.
public static class Idas3PhoenixMeterChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException("Phoenix gear effect: "+message);}
        void Near(float actual,float expected,string message)=>Check(Math.Abs(actual-expected)<.0001f,message);
        var source=Idas3ArcadeMeterCatalog.Get(69);
        Check(source!=null&&source.id==67,"source meter is unavailable");
        Layer effect=null;foreach(var layer in source.layers)if(layer.name=="GearRate01_add")effect=layer;
        Check(effect!=null,"source gear effect was not imported");
        Check(effect.role=="gearEffect"&&string.IsNullOrEmpty(effect.disabledReason),"gear effect is still classified as static/disabled");
        Check(effect.atlasCols==4&&effect.atlasRows==3,"source four-column/three-row atlas changed");
        Check(effect.texture=="ArcadeHud/Catalog/Textures/IND/UI/Race/Meter/67/Texture/Num/T_Meter67_ShiftNum","effect does not retain the original number artwork");
        bool opacity=false,scaleX=false,scaleY=false;
        foreach(var curve in effect.curves)if(curve.animation.Contains("Gear_Change")){
            opacity|=curve.property=="RenderOpacity";scaleX|=curve.property=="Scale.X";scaleY|=curve.property=="Scale.Y";
        }
        Check(opacity&&scaleX&&scaleY,"authored flash opacity/scale channels were discarded");
        var meter=new Meter{id=67,layers=new[]{effect}};
        var options=new Idas3GameOptions.Values{hudMeterStyle=69,hudShiftLights=false,hudPedalIndicators=true};
        int initial=Idas3ImportedMeter.ResidentTextureCount;
        for(int gear=0;gear<=6;++gear)using(var renderer=new Idas3ImportedMeter()){
            var data=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,gear=(gear+1)%7,revLimit=8500,rpm=4500,speedKmh=123};
            var sprites=new List<Sprite>();
            void Sample(float time){sprites.Clear();renderer.Compose(sprites,meter,options,data,time,true);}
            Sample(0);Check(sprites.Count==0,"initial observation creates a flash");
            data.gear=gear;Sample(1);Sample(1.125f);
            Check(sprites.Count==1,"actual gear change has no isolated flash for gear "+gear);
            var sprite=sprites[0];
            Check(sprite.texture&&sprite.texture.name=="T_Meter67_ShiftNum","flash uses different art");
            Near(sprite.uv.x,(gear%4)/4f,"wrong current-gear column for "+gear);
            Near(sprite.uv.y,1-(gear/4+1)/3f,"wrong current-gear row for "+gear);
            Near(sprite.uv.width,.25f,"atlas cell width changed");Near(sprite.uv.height,1f/3,"atlas cell height changed");
            Check(sprite.color.a>0,"source opacity animation did not reveal flash");
            // Shift warnings are disabled above: this decoration is still a
            // gear-change effect and must not be gated by the rev-warning option.
            Sample(1.125f);Check(sprites.Count==1,"frozen presentation lost active flash");
            Sample(2.5f);Check(sprites.Count==0,"flash does not expire to its source-hidden state");
        }
        Check(Idas3ImportedMeter.ResidentTextureCount==initial,"checks retained imported textures");
        return "Phoenix meter checks passed: "+checks;
    }
}
