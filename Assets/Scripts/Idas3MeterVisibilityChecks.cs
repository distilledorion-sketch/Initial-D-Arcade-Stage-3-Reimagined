using System;
using System.Collections.Generic;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Sprite=Idas3ArcadeHud.Sprite;
using Telemetry=Idas3ArcadeHud.Telemetry;

// Recovered source setup calls can replace a widget's editor visibility or
// animate only the color of permanent artwork. Exercise production Compose
// with real imported layers, without modifying the shared catalog.
public static class Idas3MeterVisibilityChecks
{
    static readonly int[] NightMeters={2,7,8,9,22,23,24,25,26,27,28,29,36};
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0,initial=Idas3ImportedMeter.ResidentTextureCount;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException("Meter source visibility: "+message);}
        void Near(float actual,float expected,string message)=>Check(Math.Abs(actual-expected)<.0001f,message);
        Meter Source(int id){var value=Idas3ArcadeMeterCatalog.Get(id+2);Check(value!=null&&value.id==id,"missing source "+id);return value;}
        Layer Find(Meter meter,string name){foreach(var layer in meter.layers)if(layer.name==name)return layer;throw new InvalidOperationException("Missing source widget "+meter.id+"/"+name);}
        Telemetry Data(bool night=false,bool automatic=false,float rpm=4500)=>new Telemetry{
            size=40,version=2,flags=1u|(night?4u:0u)|(automatic?2u:0u),gear=4,revLimit=8500,rpm=rpm,speedKmh=123,throttle=.7f,brake=.2f};
        var options=new Idas3GameOptions.Values{hudPedalIndicators=true,hudShiftLights=false};
        int registrations=0;
        foreach(int id in NightMeters){
            var source=Source(id);
            foreach(string name in id==7?new[]{"CenterMeterLight","LeftMeterLight"}:new[]{"Light"}){
                ++registrations;var layer=Find(source,name);
                Check(layer.visibilityDay=="B",id+"/"+name+" lost its source night registration");
                Check(layer.role=="static"&&string.IsNullOrEmpty(layer.disabledReason),id+"/"+name+" disabled static lighting");
                var isolated=new Meter{id=id,layers=new[]{layer}};
                using(var renderer=new Idas3ImportedMeter()){
                    var sprites=new List<Sprite>();
                    void Sample(Telemetry data,float time){sprites.Clear();renderer.Compose(sprites,isolated,options,data,time);}
                    foreach(bool automatic in new[]{false,true}){
                        Sample(Data(automatic:automatic),0);Check(sprites.Count==0,id+"/"+name+" remains on during day");
                        var night=Data(night:true,automatic:automatic);Sample(night,1);
                        Check(sprites.Count==1,id+"/"+name+" remains hidden at night");
                        var expected=Resources.Load<Texture2D>(Idas3ImportedMeter.SelectTexture(layer,night));
                        Check(expected&&sprites[0].texture==expected,id+"/"+name+" replaces the source artwork");
                        Check(sprites[0].color.a>0,id+"/"+name+" loses source opacity");
                        Color color=sprites[0].color;
                        Sample(night,1);Check(sprites.Count==1&&sprites[0].color==color,id+"/"+name+" changes on a held clock");
                        Sample(Data(automatic:automatic),2);Check(sprites.Count==0,id+"/"+name+" fails to hide when day returns");
                    }
                }
            }
        }
        Check(registrations==14,"night setup coverage changed");

        var classic=Source(7);var face=Find(classic,"CenterMeter1");
        Check(face.role=="static","Classic RPM numbers still treated as a low-rev lamp");
        Check(face.texture.Contains("T_Meter07_Rmp01_A1"),"Classic core dial artwork changed");
        var classicFace=new Meter{id=7,layers=new[]{face}};
        using(var renderer=new Idas3ImportedMeter()){
            var sprites=new List<Sprite>();
            void Sample(float rpm,bool warning){options.hudShiftLights=warning;sprites.Clear();renderer.Compose(sprites,classicFace,options,Data(rpm:rpm),.125f);}
            Sample(4500,false);Check(sprites.Count==1,"Classic face missing with warnings disabled");
            Color neutral=sprites[0].color;
            Near(neutral.r,.6f,"Classic neutral red channel");Near(neutral.g,.6f,"Classic neutral green channel");Near(neutral.b,.6f,"Classic neutral blue channel");Near(neutral.a,1,"Classic baseline opacity");
            Sample(8500,false);Check(sprites.Count==1&&sprites[0].color==neutral,"warning option off removes/tints the face");
            Sample(4500,true);Check(sprites.Count==1&&sprites[0].color==neutral,"inactive warning leaves a red face");
            Sample(8500,true);Check(sprites.Count==1,"redline warning hides permanent RPM numbers");
            Check(sprites[0].color.r>sprites[0].color.g&&sprites[0].color.g<neutral.g,"source redline color was not applied");
            Near(sprites[0].color.a,neutral.a,"warning changes core face opacity");
            Sample(4500,true);Check(sprites.Count==1&&sprites[0].color==neutral,"face fails to restore neutral color after warning");
        }

        var steampunk=Source(66);var glow=Find(steampunk,"RevBase");
        Check(glow.role=="rev","shared Steampunk warning glow still classified low-only");
        Check(glow.texture.EndsWith("T_Meter66_LampEf_02",StringComparison.Ordinal),"Steampunk glow artwork changed");
        var warningGlow=new Meter{id=66,layers=new[]{glow}};
        using(var renderer=new Idas3ImportedMeter()){
            var sprites=new List<Sprite>();
            void Sample(float rpm,bool warning){options.hudShiftLights=warning;sprites.Clear();renderer.Compose(sprites,warningGlow,options,Data(rpm:rpm),.125f);}
            Sample(4500,true);Check(sprites.Count==0,"Steampunk glow lights before rev warning");
            Sample(8500,false);Check(sprites.Count==0,"Steampunk glow ignores warning option");
            Sample(8500,true);Check(sprites.Count==1,"registered Steampunk over-rev glow is missing");
            Near(sprites[0].color.a,glow.ownOpacity*glow.brushColor[3],"Steampunk glow discards its authored alpha");
            Sample(4500,true);Check(sprites.Count==0,"Steampunk warning glow stays on after RPM falls");
        }
        Check(Idas3ImportedMeter.ResidentTextureCount==initial,"visibility checks retained texture leases");
        return "Meter visibility checks passed: "+checks;
    }
}
