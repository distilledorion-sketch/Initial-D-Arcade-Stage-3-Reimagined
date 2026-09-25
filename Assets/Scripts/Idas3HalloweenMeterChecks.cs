using System;
using System.Collections.Generic;
using UnityEngine;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Variant=Idas3ArcadeMeterCatalog.Variant;
using Sprite=Idas3ArcadeHud.Sprite;

// Uses the production selector and compositor with the imported Halloween art.
// This proves texture availability, selection and submitted geometry/opacity;
// the complete catalog GPU checks additionally exercise shader output.
public static class Idas3HalloweenMeterChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException("Halloween meter: "+message);}
        var meter=Idas3ArcadeMeterCatalog.Get(85);
        Check(meter!=null&&meter.id==83,"source 83 is unavailable");
        Layer Find(string name){foreach(var layer in meter.layers)if(layer.name==name)return layer;throw new InvalidOperationException("Missing Halloween layer: "+name);}
        var face=Find("CenterMeter");var transmission=Find("CarMode");
        Check(face.textureVariants.Length==8,"expected the eight original A dial frames");
        foreach(var variant in face.textureVariants)Check(variant.day=="A","fixture no longer represents an A-only source set");
        string prefix="ArcadeHud/Catalog/Textures/IND/UI/Race/Meter/83/Texture/";
        string[] artwork={"T_Meter83_BaseParts_01","T_Meter83_BaseParts_02","T_Meter83_BaseParts_03",
            "T_Meter83_BaseParts_Center","T_Meter83_Cantera","T_Meter83_PointRmp","T_Meter83_SpeedNum_Oth01"};
        string[] frameNumbers={"01","03","05","08"};int[] limits={8000,9000,10000,13000};
        int resident=Idas3ImportedMeter.ResidentTextureCount;
        using(var renderer=new Idas3ImportedMeter()){
            var options=new Idas3GameOptions.Values{hudMeterStyle=85,hudShiftLights=true,hudPedalIndicators=true};
            for(int index=0;index<limits.Length;++index)foreach(uint flags in new uint[]{1,3,5,7}){
                bool automatic=(flags&2)!=0;
                var data=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=flags,gear=4,revLimit=limits[index],
                    rpm=limits[index]*.6f,speedKmh=123,throttle=.75f,brake=.35f};
                string frame="T_Meter83_Rmp"+frameNumbers[index]+"_A";
                string mode="T_Meter83_BaseMission_"+(automatic?"02":"01");
                Check(Idas3ImportedMeter.SelectTexture(face,data)==prefix+"Frame/"+frame,"wrong "+limits[index]+" dial with flags "+flags);
                Check(Idas3ImportedMeter.SelectTexture(transmission,data)==prefix+mode,"wrong transmission label with flags "+flags);
                var sprites=new List<Sprite>();renderer.Compose(sprites,meter,options,data,.125f);
                bool Has(string name){foreach(var sprite in sprites)
                    if(sprite.texture&&sprite.texture.name==name&&sprite.color.a>0&&sprite.rect.width>0&&sprite.rect.height>0)return true;
                    return false;}
                Check(Has(frame),"selected dial texture was not rendered: "+frame+", flags "+flags);
                Check(Has(mode),"selected AT/MT texture was not rendered: "+mode);
                foreach(string texture in artwork)Check(Has(texture),"source decoration was omitted: "+texture+", flags "+flags);
                Check(renderer.DriftSpriteCount==0,"drift lamps appear without a drift signal");
                data.flags|=8;data.driftOpacity=1;data.rpm=data.revLimit;
                sprites.Clear();renderer.Compose(sprites,meter,options,data,.25f);
                Check(renderer.DriftSpriteCount==2,"both authored green lantern glow layers must respond to drifting");
                Check(Has("T_Meter83_LampEf_01")&&Has("T_Meter83_LampEf_02"),"drift glow textures are missing");
                Check(Has("T_Meter83_RevLamp")&&Has("T_Meter83_RevLampBlack"),"authored rev-warning layers are missing");
                foreach(string texture in artwork)Check(Has(texture),"lamp activation removed source decoration: "+texture);
            }
        }
        Check(Idas3ImportedMeter.ResidentTextureCount==resident,"checks retained imported texture leases");

        // The lantern and its drift glow share the source Cantera parent pivot.
        // Exercise the real imported owner curve through production Compose.
        using(var renderer=new Idas3ImportedMeter()){
            var options=new Idas3GameOptions.Values{hudMeterStyle=85,hudShiftLights=true,hudPedalIndicators=true};
            var data=new Idas3ArcadeHud.Telemetry{version=2,flags=1,gear=3,revLimit=8500,rpm=4500,speedKmh=90};
            Sprite Lantern(float time){var sprites=new List<Sprite>();renderer.Compose(sprites,meter,options,data,time);
                foreach(var sprite in sprites)if(sprite.texture&&sprite.texture.name=="T_Meter83_Cantera")return sprite;
                throw new InvalidOperationException("Halloween lantern was not composed");}
            var layer=Find("Image_Cantera");Idas3ArcadeMeterCatalog.Curve swing=null;
            foreach(var curve in layer.curves)if(curve.owner?.name=="Cantera"&&curve.property=="Rotation"&&curve.animation.Contains("DriftLamp_InOut")){swing=curve;break;}
            Check(swing!=null,"recovered lantern rotation is missing");
            Check(Math.Abs(Idas3MeterAnimationState.DurationSeconds(swing)-1.25f)<.0001f,"source InOut playback changed");
            var origin=Lantern(0);var point=new Vector3(layer.width*.5f,layer.height*.15f,0);
            var pivot=origin.transform.MultiplyPoint3x4(point);
            data.flags=9;data.driftOpacity=1;Lantern(.1f);
            var entered=Lantern(.1f+10000f/24000);
            float Angle(Sprite sprite)=>Mathf.Atan2(sprite.transform.m10,sprite.transform.m00)*Mathf.Rad2Deg;
            Check(Math.Abs(Mathf.DeltaAngle(Angle(origin),Angle(entered))-5)<.01f,"source entry peak did not rotate the lantern by five degrees");
            Check(Vector3.Distance(pivot,entered.transform.MultiplyPoint3x4(point))<.002f,"entry moved the source hanging pivot");
            var settled=Lantern(1.5f);
            Check(Math.Abs(Mathf.DeltaAngle(Angle(origin),Angle(settled)))<.001f,"completed entry did not restore the neutral lantern");
            data.flags=1;data.driftOpacity=.7f;Lantern(2);
            var exited=Lantern(2+20000f/24000);
            Check(Math.Abs(Mathf.DeltaAngle(Angle(origin),Angle(exited))-5)<.01f,"exit did not reverse the recovered rotation curve");
            Check(Vector3.Distance(pivot,exited.transform.MultiplyPoint3x4(point))<.002f,"exit moved the source hanging pivot");
        }
        Check(Idas3ImportedMeter.ResidentTextureCount==resident,"lantern checks retained texture leases");

        // A requested night frame must still win when it actually exists, and
        // falling back between light variants must never swap transmission.
        var synthetic=new Layer{texture="default",textureVariants=new[]{
            new Variant{texture="wrong-scale-night",day="B",maxRpm=8000,state="manual"},
            new Variant{texture="matching-day",day="A",maxRpm=13000,state="manual"},
            new Variant{texture="wrong-transmission-night",day="B",maxRpm=13000,state="automatic"},
            new Variant{texture="matching-night",day="B",maxRpm=13000,state="manual"}}};
        var input=new Idas3ArcadeHud.Telemetry{flags=5,revLimit=13000};
        Check(Idas3ImportedMeter.SelectTexture(synthetic,input)=="matching-night","night preference lost when B is supplied");
        input.flags=1;
        Check(Idas3ImportedMeter.SelectTexture(synthetic,input)=="matching-day","day preference lost when A is supplied");
        input.flags=7;
        Check(Idas3ImportedMeter.SelectTexture(synthetic,input)=="wrong-transmission-night","automatic selection ignored its own compatible variant");
        synthetic.textureVariants=new[]{new Variant{texture="automatic-only",day="B",maxRpm=13000,state="automatic"}};
        input.flags=5;
        Check(Idas3ImportedMeter.SelectTexture(synthetic,input)=="default","fallback crossed an incompatible transmission state");
        synthetic.textureVariants=new[]{new Variant{texture="night-only",day="B",maxRpm=13000}};
        input.flags=1;
        Check(Idas3ImportedMeter.SelectTexture(synthetic,input)=="night-only","B-only sources cannot fall back during daytime");
        return "Halloween meter checks passed: "+checks;
    }
}
