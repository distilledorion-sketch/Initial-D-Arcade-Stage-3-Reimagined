using System;
using System.Collections.Generic;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Sprite=Idas3ArcadeHud.Sprite;

public static class Idas3MeterAnimatedAtlasChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0,resident=Idas3ImportedMeter.ResidentTextureCount;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException("Animated gear atlas: "+message);}
        var source=Idas3ArcadeMeterCatalog.Get(40);
        Check(source!=null&&source.id==38,"Metallic source missing");
        Idas3ArcadeMeterCatalog.Layer effect=null;
        foreach(var layer in source.layers)if(layer.name=="GearRate01_Blur")effect=layer;
        Check(effect!=null&&effect.role=="gearEffect","Metallic blur not registered as a gear effect");
        Check(effect.atlasCols==1&&effect.atlasRows==7,"original seven-row blur atlas changed");
        Check(effect.texture.EndsWith("T_Meter38_ShiftNum_Blur",StringComparison.Ordinal),"original blur artwork missing");
        var meter=new Meter{id=38,layers=new[]{effect}};
        var options=new Idas3GameOptions.Values{hudMeterStyle=40,hudShiftLights=false,hudPedalIndicators=true};
        // The recovered Index curve goes 3.808882 -> 11.723038 over .625s.
        // These cells cross the atlas boundary while its opacity is still >0.
        float[] ages={.05f,.125f,.25f,.35f,.45f};
        int[] cells={4,5,6,1,2};
        for(int gear=0;gear<=6;++gear)using(var renderer=new Idas3ImportedMeter()){
            var data=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,gear=(gear+1)%7,revLimit=8500,rpm=4500,speedKmh=123};
            var sprites=new List<Sprite>();
            void Sample(float now){sprites.Clear();renderer.Compose(sprites,meter,options,data,now,true);}
            Sample(0);Check(sprites.Count==0,"first observation starts a gear effect");
            data.gear=gear;Sample(1);
            for(int i=0;i<ages.Length;++i){
                Sample(1+ages[i]);Check(sprites.Count==1,"animated blur vanished at "+ages[i]+"s for gear "+gear);
                var sprite=sprites[0];
                Check(Math.Abs(sprite.uv.y-(1-(cells[i]+1)/7f))<.0001f,"source frame sequence ignored at "+ages[i]+"s for gear "+gear);
                Check(Math.Abs(sprite.uv.height-1f/7)<.0001f&&sprite.uv.width==1,"atlas cell size changed");
                Check(sprite.texture&&sprite.texture.name=="T_Meter38_ShiftNum_Blur"&&sprite.color.a>0,"blur artwork/opacity missing");
                var uv=sprite.uv;var color=sprite.color;
                Sample(1+ages[i]);Check(sprites.Count==1&&sprites[0].uv==uv&&sprites[0].color==color,"frozen clock advances blur");
            }
            Sample(1.6f);Check(sprites.Count==0,"blur survives its opacity animation");
            Sample(3);Check(sprites.Count==0,"blur survives event expiry");
        }
        Rect reference=default;
        foreach(int fps in new[]{30,60,144})using(var renderer=new Idas3ImportedMeter()){
            var data=new Idas3ArcadeHud.Telemetry{version=2,flags=1,gear=3,revLimit=8500};
            var sprites=new List<Sprite>();
            renderer.Compose(sprites,meter,options,data,0,true);data.gear=4;
            sprites.Clear();renderer.Compose(sprites,meter,options,data,1,true);
            for(int frame=1;frame/(float)fps<.375f;++frame){sprites.Clear();renderer.Compose(sprites,meter,options,data,1+frame/(float)fps,true);}
            sprites.Clear();renderer.Compose(sprites,meter,options,data,1.375f,true);
            Check(sprites.Count==1,"blur missing at "+fps+" FPS");
            if(fps==30)reference=sprites[0].uv;else Check(sprites[0].uv==reference,"blur frame depends on FPS");
        }
        Check(Idas3ImportedMeter.ResidentTextureCount==resident,"atlas checks retain texture leases");
        return "Animated gear atlas checks passed: "+checks;
    }
}
