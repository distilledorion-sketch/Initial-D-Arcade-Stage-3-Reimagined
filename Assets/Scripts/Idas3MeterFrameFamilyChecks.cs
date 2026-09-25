using System;
using System.Collections.Generic;
using UnityEngine;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Sprite=Idas3ArcadeHud.Sprite;

// Source frame naming is not uniform: AB is one day/night-shared family,
// whereas Classic A1/A2 are complementary layers, not lighting variants.
// Exercise the real selector and compositor so a retained file alone cannot
// hide a missing dial behind an incorrectly assigned runtime role.
public static class Idas3MeterFrameFamilyChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0;
        void Check(bool value,string message){++checks;if(!value)throw new InvalidOperationException("Meter frame family: "+message);}
        int Maximum(int index)=>index<=2?8000:index<=4?9000:index<=7?10000:13000;
        string Frame(int source,int index,string suffix)=>"ArcadeHud/Catalog/Textures/IND/UI/Race/Meter/"+source.ToString("00")+
            "/Texture/Frame/T_Meter"+source.ToString("00")+"_Rmp"+index.ToString("00")+"_"+suffix;
        Layer Find(Idas3ArcadeMeterCatalog.Meter meter,string name){
            foreach(var layer in meter.layers)if(layer.name==name)return layer;
            throw new InvalidOperationException("Missing "+meter.id+" / "+name);
        }
        void Family(Layer layer,int source,string suffix){
            Check(layer.textureVariants!=null&&layer.textureVariants.Length==8,source+" / "+layer.name+" must retain all eight "+suffix+" frames");
            for(int index=1;index<=8;++index){
                string expected=Frame(source,index,suffix);int matches=0;
                foreach(var variant in layer.textureVariants)if(variant.texture==expected){
                    ++matches;Check(variant.index==index&&variant.maxRpm==Maximum(index),expected+" has the wrong scale/index");
                }
                Check(matches==1,expected+" is missing or duplicated in the source family");
            }
        }
        bool Present(List<Sprite> sprites,string resource,out Sprite found){
            string name=resource.Substring(resource.LastIndexOf('/')+1);
            foreach(var sprite in sprites)if(sprite.texture&&sprite.texture.name==name&&sprite.color.a>0&&sprite.rect.width>0&&sprite.rect.height>0){found=sprite;return true;}
            found=default;return false;
        }
        int resident=Idas3ImportedMeter.ResidentTextureCount;
        int[] indices={1,3,5,8};
        foreach(int source in new[]{2,22,23,24,8,7}){
            var meter=Idas3ArcadeMeterCatalog.Get(source+2);
            Check(meter!=null&&meter.id==source,"source "+source+" is unavailable");
            var faces=source==7?new[]{Find(meter,"CenterMeter1"),Find(meter,"CenterMeter2")}:new[]{Find(meter,"CenterMeter")};
            for(int layer=0;layer<faces.Length;++layer){
                Family(faces[layer],source,source==7?"A"+(layer+1):"AB");
                Check(faces[layer].role!="low"&&faces[layer].role!="rev"&&faces[layer].role!="disabled",
                    source+" / "+faces[layer].name+" is a permanent RPM face, not an optional lamp");
            }
            using(var renderer=new Idas3ImportedMeter()){
                var options=new Idas3GameOptions.Values{hudMeterStyle=source+2,hudShiftLights=true,hudPedalIndicators=true};
                var sprites=new List<Sprite>();
                foreach(int index in indices)foreach(uint flags in new uint[]{1,3,5,7}){
                    var data=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=flags,gear=4,revLimit=Maximum(index),
                        rpm=Maximum(index)*.6f,speedKmh=123,throttle=.7f,brake=.2f};
                    sprites.Clear();renderer.Compose(sprites,meter,options,data,.125f);
                    var submitted=new Sprite[faces.Length];
                    for(int layer=0;layer<faces.Length;++layer){
                        string expected=Frame(source,index,source==7?"A"+(layer+1):"AB");
                        Check(Idas3ImportedMeter.SelectTexture(faces[layer],data)==expected,
                            source+" / "+faces[layer].name+" chose the wrong frame at "+data.revLimit+" RPM / flags "+flags);
                        Check(Present(sprites,expected,out submitted[layer]),expected+" is selected but omitted from production Compose");
                    }
                    if(source==7){
                        var a=submitted[0];var b=submitted[1];
                        Check(a.rect==b.rect,"Classic complementary RPM layers have different dimensions");
                        for(int element=0;element<16;++element)Check(Math.Abs(a.transform[element]-b.transform[element])<.002f,
                            "Classic complementary RPM layers do not share the same placement");
                        options.hudShiftLights=false;sprites.Clear();renderer.Compose(sprites,meter,options,data,.25f);
                        Check(Present(sprites,Frame(source,index,"A1"),out _)&&Present(sprites,Frame(source,index,"A2"),out _),
                            "Disabling shift warnings removed Classic's dial numbers or perimeter");
                        options.hudShiftLights=true;
                    }
                }
            }
            Check(Idas3ImportedMeter.ResidentTextureCount==resident,"source "+source+" checks retained imported texture leases");
        }

        // Racing Orange's serialized template points at Pink's meter46, but
        // its recovered runtime registration explicitly supplies meter47 A/B.
        // The widget itself is source-collapsed, so selection is checked without
        // inventing a visibility event or forcing unused artwork onto the HUD.
        var racing=Idas3ArcadeMeterCatalog.Get(49);
        Check(racing!=null&&racing.id==47,"Racing Orange is unavailable");
        var racingFace=Find(racing,"CenterMeter");
        Check(racingFace.textureVariants!=null&&racingFace.textureVariants.Length==16,"Racing Orange must retain eight paired day/night frames");
        foreach(string day in new[]{"A","B"})for(int index=1;index<=8;++index){
            string expected=Frame(47,index,day);int matches=0;
            foreach(var variant in racingFace.textureVariants)if(variant.texture==expected){
                ++matches;Check(variant.index==index&&variant.maxRpm==Maximum(index)&&variant.day==day,expected+" has the wrong scale/day metadata");
            }
            Check(matches==1,expected+" is missing or duplicated");
        }
        foreach(int index in indices)foreach(uint flags in new uint[]{1,3,5,7}){
            var data=new Idas3ArcadeHud.Telemetry{version=2,flags=flags,revLimit=Maximum(index),gear=4};
            Check(Idas3ImportedMeter.SelectTexture(racingFace,data)==Frame(47,index,(flags&4)!=0?"B":"A"),
                "Racing Orange selected the Pink template or wrong day/scale at "+data.revLimit+" / flags "+flags);
        }
        void Replacement(int source,string name,Func<int,string,string> expected){
            var meter=Idas3ArcadeMeterCatalog.Get(source+2);
            Check(meter!=null&&meter.id==source,"replacement source "+source+" is unavailable");
            var layer=Find(meter,name);
            using(var renderer=new Idas3ImportedMeter()){
                var options=new Idas3GameOptions.Values{hudMeterStyle=source+2,hudShiftLights=true,hudPedalIndicators=true};
                var sprites=new List<Sprite>();
                foreach(int index in indices)foreach(uint flags in new uint[]{1,3,5,7}){
                    var data=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=flags,gear=4,revLimit=Maximum(index),
                        rpm=Maximum(index)*.6f,speedKmh=123,throttle=.7f,brake=.2f};
                    string resource=expected(index,(flags&4)!=0?"B":"A");
                    Check(Idas3ImportedMeter.SelectTexture(layer,data)==resource,source+" / "+name+" retained the editor brush at flags "+flags);
                    sprites.Clear();renderer.Compose(sprites,meter,options,data,.125f);
                    Check(Present(sprites,resource,out _),source+" / "+name+" replacement is absent from production Compose: "+resource);
                }
            }
            Check(Idas3ImportedMeter.ResidentTextureCount==resident,"replacement checks retained texture leases for "+source);
        }
        Replacement(25,"LeftMeter",(index,day)=>"ArcadeHud/Catalog/Textures/IND/UI/Race/Meter/25/Texture/Frame/T_Meter25_Spd_"+day);
        Replacement(41,"CenterMeter",(index,day)=>Frame(0,index,day));
        Replacement(9,"CenterMeter",(index,day)=>Frame(9,index,"A"));
        foreach(int source in new[]{49,50,51,53,54,55,60,61,62,84,85,86})
            Replacement(source,"CenterPin",(index,day)=>"ArcadeHud/Catalog/Textures/IND/UI/Race/Meter/00/Texture/T_Meter00_PointRmp_"+day);
        foreach(int source in new[]{26,27,28})
            Replacement(source,"CenterPin",(index,day)=>"ArcadeHud/Catalog/Textures/IND/UI/Race/Meter/"+
                (day=="B"?"09":source.ToString("00"))+"/Texture/T_Meter"+(day=="B"?"09":source.ToString("00"))+"_PointRmp_"+day);
        var pink=Idas3ArcadeMeterCatalog.Get(48);
        Check(pink!=null&&pink.id==46,"Racing Pink is unavailable");
        var pinkFace=Find(pink,"CenterMeter");int sharedNightEntries=0;
        foreach(var variant in pinkFace.textureVariants)if(variant.index==4&&variant.day=="B"){
            ++sharedNightEntries;
            Check(variant.texture==Frame(32,4,"B")&&variant.maxRpm==9000,
                "Racing Pink's original index4 night registration lost its shared Meter32 resource");
        }
        Check(sharedNightEntries==1,"Racing Pink's shared index4 night registration is absent or duplicated");
        return "Meter frame family checks passed: "+checks;
    }
}
