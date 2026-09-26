using System;
using System.Collections.Generic;
using UnityEngine;
using Meter=Idas3ArcadeMeterCatalog.Meter;
using Layer=Idas3ArcadeMeterCatalog.Layer;
using Curve=Idas3ArcadeMeterCatalog.Curve;

// The import contains source layout, artwork and serialized animation channels.
// This adapter supplies current telemetry; it does not execute Unreal bytecode.
internal sealed class Idas3ImportedMeter : IDisposable
{
    sealed class LoadedTexture {public Texture2D value;public int users;}
    static readonly Dictionary<string,LoadedTexture> pool=new Dictionary<string,LoadedTexture>();
    readonly Dictionary<string,Texture2D> textures=new Dictionary<string,Texture2D>();
    readonly Idas3MeterAnimationState animationState=new Idas3MeterAnimationState();
    readonly Idas3MeterMaterialAnimation materialAnimation=new Idas3MeterMaterialAnimation();
    readonly Idas3MeterNeedleTrails needleTrails=new Idas3MeterNeedleTrails();
    readonly Idas3HalloweenLanternAnimation lanternAnimation=new Idas3HalloweenLanternAnimation();
    readonly float[] audioBands=new float[Idas3MeterAudioSpectrum.BandCount];
    readonly Color32[] audioPixels=new Color32[Idas3MeterAudioSpectrum.BandCount];
    Texture2D audioTexture;
    float audioTime,audioEnergy;
    long audioRevision=-1;
    internal float[] AudioBandsOverride {get;set;}
    Meter current;
    internal int DriftSpriteCount {get;private set;}
    internal static int ResidentTextureCount=>pool.Count;
    static float Safe(float value,float fallback=0)=>float.IsNaN(value)||float.IsInfinity(value)?fallback:value;
    Texture2D Texture(string path){
        if(string.IsNullOrEmpty(path))return null;
        if(textures.TryGetValue(path,out var value))return value;
        if(!pool.TryGetValue(path,out var entry)){
            entry=new LoadedTexture{value=Resources.Load<Texture2D>(path)};
            if(!entry.value){Debug.LogWarning("Missing imported meter texture: "+path);textures[path]=null;return null;}
            pool.Add(path,entry);
        }
        ++entry.users;textures.Add(path,entry.value);return entry.value;
    }
    public void Dispose(){
        foreach(var pair in textures){
            if(!pair.Value||!pool.TryGetValue(pair.Key,out var entry))continue;
            if(--entry.users==0){pool.Remove(pair.Key);Resources.UnloadAsset(entry.value);}
        }
        textures.Clear();current=null;animationState.Reset();materialAnimation.Reset();needleTrails.Reset();lanternAnimation.Reset();
        if(audioTexture){if(Application.isPlaying)UnityEngine.Object.Destroy(audioTexture);else UnityEngine.Object.DestroyImmediate(audioTexture);}
        audioTexture=null;audioEnergy=0;audioRevision=-1;audioTime=0;
    }
    void UpdateAudio(float seconds){
        bool sample=AudioBandsOverride!=null||!audioTexture||
            (seconds!=audioTime&&audioRevision!=Idas3MeterAudioSpectrum.Revision)||
            (audioEnergy>0&&Idas3MeterAudioSpectrum.IsSilent);
        audioTime=seconds;if(!sample)return;
        if(AudioBandsOverride!=null){for(int i=0;i<audioBands.Length;++i)audioBands[i]=i<AudioBandsOverride.Length?AudioBandsOverride[i]:0;}
        else Idas3MeterAudioSpectrum.CopyBands(audioBands);
        audioEnergy=0;
        for(int i=0;i<audioBands.Length;++i){float value=Mathf.Clamp01(Safe(audioBands[i]));audioEnergy=Mathf.Max(audioEnergy,value);
            byte level=(byte)Mathf.RoundToInt(value*255);audioPixels[i]=new Color32(level,level,level,255);}
        if(!audioTexture)audioTexture=new Texture2D(audioBands.Length,1,TextureFormat.RGBA32,false,true){name="Live meter spectrum",filterMode=FilterMode.Point,wrapMode=TextureWrapMode.Clamp,hideFlags=HideFlags.HideAndDontSave};
        audioTexture.SetPixels32(audioPixels);audioTexture.Apply(false,false);audioRevision=Idas3MeterAudioSpectrum.Revision;
    }
    internal static Matrix4x4 Matrix(float[] values,float x=0,float y=0){
        var result=Matrix4x4.identity;
        if(values!=null&&values.Length==6){result.m00=values[0];result.m01=values[1];result.m03=values[2];result.m10=values[3];result.m11=values[4];result.m13=values[5];}
        else{result.m03=x;result.m13=y;}
        return result;
    }
    internal static float Evaluate(Curve curve,float progress){
        int count=Math.Min(curve.times?.Length??0,curve.values?.Length??0);
        if(count==0)return Safe(curve.defaultValue);
        if(count==1)return Safe(curve.values[0]);
        float begin=curve.animationEnd>curve.animationStart?curve.animationStart:curve.times[0];
        float end=curve.animationEnd>curve.animationStart?curve.animationEnd:curve.times[count-1];
        float time=Mathf.Lerp(begin,end,Mathf.Clamp01(progress));
        if(time<=curve.times[0])return Safe(curve.values[0]);
        for(int i=1;i<count;++i)if(time<=curve.times[i]){
            float span=curve.times[i]-curve.times[i-1];
            return Safe(Mathf.Lerp(curve.values[i-1],curve.values[i],span>0?(time-curve.times[i-1])/span:1));
        }
        return Safe(curve.values[count-1]);
    }
    static float Scalar(Layer layer,string name,float fallback){
        if(layer.parameters!=null)foreach(var p in layer.parameters)if(p.name==name)return Safe(p.value,fallback);
        return fallback;
    }
    static Color Tint(float[] value)=>value!=null&&value.Length>=4?new Color(Safe(value[0],1),Safe(value[1],1),Safe(value[2],1),Safe(value[3],1)):Color.white;
    static Color MaterialColor(Layer layer,string name,Color fallback){
        if(layer.vectorParameters!=null)foreach(var value in layer.vectorParameters)if(value.name==name)return Tint(value.values);
        return fallback;
    }
    Texture2D BoundTexture(Layer layer,string name){
        if(layer.textureBindings!=null)foreach(var binding in layer.textureBindings)if(binding.name==name)return Texture(binding.texture);
        return null;
    }
    static bool Contains(string value,string part)=>value!=null&&value.IndexOf(part,StringComparison.OrdinalIgnoreCase)>=0;
    static float AnimationProgress(string name,Idas3ArcadeHud.Telemetry t,float seconds){
        if(Contains(name,"CenterPin"))return Mathf.Clamp01(Safe(t.rpm)/Idas3ArcadeHud.TachMaximum(t.revLimit));
        if(Contains(name,"LeftPin"))return Mathf.Clamp01(Safe(t.speedKmh)/240);
        if(Contains(name,"Accel"))return Mathf.Clamp01(Safe(t.throttle));
        if(Contains(name,"Brake"))return Mathf.Clamp01(Safe(t.brake));
        if(Contains(name,"Drift"))return Idas3ArcadeHud.LampOpacity(t);
        if(Contains(name,"Rev"))return Idas3ArcadeHud.ShiftWarning(t);
        return 0;
    }
    internal static string SelectTexture(Layer layer,Idas3ArcadeHud.Telemetry t){
        if(layer.speedTextures!=null&&layer.speedTextures.Length==4)
            return layer.speedTextures[SpeedColorBand(t.speedKmh)];
        string chosen=(t.flags&4)!=0&&!string.IsNullOrEmpty(layer.nightTexture)?layer.nightTexture:layer.texture;
        int max=Idas3ArcadeHud.TachMaximum(t.revLimit),score=-1;
        bool night=(t.flags&4)!=0,automatic=(t.flags&2)!=0;
        if(layer.textureVariants!=null)foreach(var v in layer.textureVariants){
            if(string.IsNullOrEmpty(v.texture)||
                (!string.IsNullOrEmpty(v.state)&&v.state!=(automatic?"automatic":"manual")))continue;
            if(v.maxRpm>0&&v.maxRpm!=max)continue;
            // Some source sets (including Halloween) supply only A frames.
            // Keep the matching RPM scale at night instead of falling back to
            // the serialized 8,000 face; prefer the requested light variant
            // whenever the matching scale has one.
            int value=(v.maxRpm>0?4:0)+(v.day==(night?"B":"A")?2:0)+(!string.IsNullOrEmpty(v.state)?1:0);
            if(value>score){score=value;chosen=v.texture;}
        }
        return chosen;
    }
    // Recovered WBP_SpeedMeter_Base.GetSpeedColor compares <90, <150, <210.
    internal static int SpeedColorBand(float speed)=>Safe(speed)<90?0:speed<150?1:speed<210?2:3;
    static int Digit(string role,Idas3ArcadeHud.Telemetry t){
        int speed=Mathf.Clamp(Mathf.FloorToInt(Safe(t.speedKmh)),0,999),rpm=Mathf.Clamp(Mathf.FloorToInt(Safe(t.rpm)),0,19999);
        switch(role){case "gear":case "gearEffect":return Mathf.Clamp(t.gear,0,6);case "speed100":return speed>=100?speed/100:10;
            case "speed10":return speed>=10?speed/10%10:10;case "speed1":return speed%10;
            case "rpm10000":return rpm>=10000?rpm/10000%10:10;case "rpm1000":return rpm/1000%10;case "rpm100":return rpm/100%10;case "rpm10":return rpm/10%10;case "rpm1":return rpm%10;default:return -1;}
    }
    struct TransformState {
        public float x,y,angle,sx,sy,shx,shy,opacity,colorAlpha,width,height;
        public bool visible;
        public Matrix4x4 Local(float px,float py){
            var shear=Matrix4x4.identity;shear.m01=Mathf.Tan(shx*Mathf.Deg2Rad);shear.m10=Mathf.Tan(shy*Mathf.Deg2Rad);
            return Matrix4x4.Translate(new Vector3(x+px,y+py,0))*Matrix4x4.Rotate(Quaternion.Euler(0,0,angle))*shear*
                Matrix4x4.Scale(new Vector3(sx,sy,1))*Matrix4x4.Translate(new Vector3(-px,-py,0));
        }
    }
    static TransformState Initial(Layer l)=>new TransformState{x=l.translationX,y=l.translationY,angle=l.angle,sx=l.scaleX,sy=l.scaleY,shx=l.shearX,shy=l.shearY,opacity=l.ownOpacity,colorAlpha=1,width=l.width,height=l.height,visible=!Contains(l.visibility,"Hidden")&&!Contains(l.visibility,"Collapsed")};
    static TransformState Initial(Idas3ArcadeMeterCatalog.Owner o)=>new TransformState{x=o.translationX,y=o.translationY,angle=o.angle,sx=o.scaleX,sy=o.scaleY,shx=o.shearX,shy=o.shearY,opacity=o.opacity,colorAlpha=o.colorAlpha,width=o.width,height=o.height,visible=true};
    bool Progress(Curve c,Idas3ArcadeHud.Telemetry data,float seconds,bool shiftLights,out float progress){
        progress=0;
        if(lanternAnimation.TryProgress(c,out progress))return true;
        if(Contains(c.animation,"Gear_Change"))return animationState.TryProgress(c,out progress);
        if(Contains(c.animation,"LED"))return false; // Authored LED programs use the material clock.
        if(Contains(c.animation,"Corner")||Contains(c.animation,"LowLamp"))return false;
        // A warning can tint the main dial (Classic), rather than a separate
        // lamp. Restore its authored neutral frame when the warning is off;
        // applying Stay at phase zero would leave the dial red all the time.
        if(Contains(c.animation,"RevLamp")&&(!shiftLights||Idas3ArcadeHud.ShiftWarning(data)<=0))
            return !Contains(c.animation,"Stay");
        // The duplicated brake animation is an unused copy of the accelerator
        // sweep, conflicting with the canonical left-hand brake mask.
        if(c.animation=="Anim_BrakePin_2_INST")return false;
        if((Contains(c.animation,"DriftLamp")||Contains(c.animation,"RevLamp"))&&!Contains(c.animation,"Stay"))return false;
        progress=AnimationProgress(c.animation,data,seconds);return true;
    }
    static void Apply(ref TransformState s,string property,float value){
        switch(property){case "Rotation":s.angle=value;break;case "Translation.X":s.x=value;break;case "Translation.Y":s.y=value;break;
            case "Scale.X":s.sx=value;break;case "Scale.Y":s.sy=value;break;case "Shear.X":s.shx=value;break;case "Shear.Y":s.shy=value;break;
            case "RenderOpacity":s.opacity=value;break;case "Color.A":s.colorAlpha=value;break;
            case "Layout.Right":s.width=value;break;case "Visibility":s.visible=value!=1&&value!=2;break;}
    }
    static Rect Box(Matrix4x4 matrix,float width,float height){
        var a=matrix.MultiplyPoint3x4(Vector3.zero);var b=matrix.MultiplyPoint3x4(new Vector3(width,0,0));
        var c=matrix.MultiplyPoint3x4(new Vector3(0,height,0));var d=matrix.MultiplyPoint3x4(new Vector3(width,height,0));
        return Rect.MinMaxRect(Mathf.Min(a.x,b.x,c.x,d.x),Mathf.Min(a.y,b.y,c.y,d.y),Mathf.Max(a.x,b.x,c.x,d.x),Mathf.Max(a.y,b.y,c.y,d.y));
    }
    static Rect Intersect(Rect a,Rect b){float x=Mathf.Max(a.xMin,b.xMin),y=Mathf.Max(a.yMin,b.yMin);
        return new Rect(x,y,Mathf.Max(0,Mathf.Min(a.xMax,b.xMax)-x),Mathf.Max(0,Mathf.Min(a.yMax,b.yMax)-y));}

    internal void Compose(List<Idas3ArcadeHud.Sprite> result,Meter meter,Idas3GameOptions.Values options,Idas3ArcadeHud.Telemetry data,float seconds,bool gearEffectsOnly=false){
        if(current!=meter){Dispose();current=meter;}
        animationState.Update(meter,data,seconds);materialAnimation.Update(meter,data,seconds);needleTrails.Update(meter,data,seconds);lanternAnimation.Update(meter,data,seconds);seconds=Safe(seconds);
        DriftSpriteCount=0;
        if(meter==null)return;
        if(meter.id==68||meter.id==69||meter.id==70||meter.id==74)UpdateAudio(seconds);
        foreach(var layer in meter.layers){
            if(layer==null||layer.role=="disabled"||!string.IsNullOrEmpty(layer.disabledReason)||layer.width<=0||layer.height<=0)continue;
            bool registeredDay=!string.IsNullOrEmpty(layer.visibilityDay);
            if(registeredDay&&layer.visibilityDay!=((data.flags&4)!=0?"B":"A"))continue;
            bool selected=true;
            bool led=Idas3MeterMaterialAnimation.IsLed(layer);
            if(layer.switchers!=null)foreach(var choice in layer.switchers)
                if(led&&choice.name=="LED_Top")continue;
                else
                if(choice.index!=(choice.name=="DriftLampColor"?1:choice.activeIndex)){selected=false;break;}
            if(!selected)continue;
            Rect ledUv=default;
            if(led&&!materialAnimation.TryLed(layer,out ledUv))continue;
            string role=layer.role??"static";
            if(gearEffectsOnly&&role!="gearEffect"&&role!="gearRoll")continue;
            if(role=="gearEffect"||role=="gearRoll"){
                bool playing=false;
                if(layer.curves!=null)foreach(var curve in layer.curves)
                    if(Contains(curve.animation,"Gear_Change")&&animationState.TryProgress(curve,out _)){playing=true;break;}
                if(!playing)continue;
            }
            // Steampunk's coil decoration reacts to pedals but is not itself
            // a pedal readout. Hiding readouts must not remove its lightning.
            bool pedalDecoration=meter.id==66&&layer.name.StartsWith("coil_",StringComparison.Ordinal);
            if((role=="accel"||role=="brake")&&!options.hudPedalIndicators&&!pedalDecoration)continue;
            if(role=="low")continue; // No recovered low-rev activation rule.
            float opacity=1;
            if(role=="drift")opacity=Idas3ArcadeHud.LampOpacity(data);
            else if(role=="rev")opacity=options.hudShiftLights?Idas3ArcadeHud.ShiftWarning(data):0;
            if(opacity<=0)continue;
            var texture=Texture(SelectTexture(layer,data));if(!texture)continue;
            Color color=Tint(layer.color);
            // The white DAC face reuses the white digit atlas. Its source
            // runtime tint is absent from the export; retain readable contrast.
            bool speedColor=layer.speedTextures!=null&&layer.speedTextures.Length==4;
            bool tintSpeed=meter.id==58&&(role=="speed1"||role=="speed10"||role=="speed100"||layer.name=="SpeedRate");
            if(tintSpeed){
                int band=SpeedColorBand(data.speedKmh);
                if(band<3)color*=band==0?new Color(1,.02f,.02f):band==1?new Color(1,.94f,.02f):new Color(.02f,.66f,1);
            }
            if(meter.id==42&&layer.name.StartsWith("SpeedRate",StringComparison.Ordinal)&&!speedColor)color=new Color(.08f,.08f,.08f,color.a);
            // The fourth atlas is neutral for the animated rainbow. Drive its
            // hue from presentation time so pausing/seeking and FPS stay stable.
            if((speedColor||tintSpeed)&&SpeedColorBand(data.speedKmh)==3)
                color*=Color.HSVToRGB(Mathf.Repeat(Safe(seconds)*.5f,1),.85f,1);
            // Reuse the authored gauge colors. Their vector alpha is commonly
            // zero and is not widget opacity. Circle01's RGB emission survived
            // extraction; sibling graphs require this color-preserving adapter.
            string materialTint=Contains(layer.materialParent,"MaskCircle")?"TrailColor":layer.materialParent=="/Game/IND/UI/MasterMaterial/M_Blink01.M_Blink01"?"BaseColor":null;
            if(materialTint!=null&&layer.vectorParameters!=null)
                foreach(var p in layer.vectorParameters)if(p.name==materialTint&&p.values!=null&&p.values.Length>=3){color.r*=p.values[0];color.g*=p.values[1];color.b*=p.values[2];}
            var matrix=Matrix(layer.transform,layer.x,layer.y);
            var initial=Initial(layer);var state=initial;
            float percentage=Scalar(layer,"Percentage",1),start=Scalar(layer,"StartPosition",0),width=Scalar(layer,"Width",1);
            float scrollU=Scalar(layer,"U Scroll",0),scrollV=Scalar(layer,"V Scroll",0),animatedIndex=-1;
            bool animatedRotation=false,animatedPercentage=false,animatedMaterial=false,clipped=false;
            Rect clip=default;float ancestorAlpha=1;
            if(layer.parents!=null)foreach(var owner in layer.parents){
                var original=Initial(owner);var changed=original;
                if(layer.curves!=null)foreach(var curve in layer.curves){
                    if(curve.owner?.name!=owner.name||!Progress(curve,data,seconds,options.hudShiftLights,out float progress))continue;
                    Apply(ref changed,curve.property,Evaluate(curve,progress));
                }
                ancestorAlpha*=changed.opacity*changed.colorAlpha;
                var origin=Matrix(owner.transform);
                var correction=original.Local(owner.width*owner.pivotX,owner.height*owner.pivotY).inverse*changed.Local(owner.width*owner.pivotX,owner.height*owner.pivotY);
                matrix=origin*correction*origin.inverse*matrix;
                if(owner.clipsToBounds){var bounds=Box(origin*correction,changed.width,changed.height);clip=clipped?Intersect(clip,bounds):bounds;clipped=true;}
            }
            if(!clipped&&layer.clipRect!=null&&layer.clipRect.Length==4){clip=new Rect(layer.clipRect[0],layer.clipRect[1],layer.clipRect[2],layer.clipRect[3]);clipped=true;}
            if(layer.curves!=null)foreach(var curve in layer.curves){
                if(curve.owner!=null&&!string.IsNullOrEmpty(curve.owner.name)&&curve.owner.name!=layer.name||!Progress(curve,data,seconds,options.hudShiftLights,out float progress))continue;
                float value=Evaluate(curve,progress);
                Apply(ref state,curve.property,value);
                if(curve.property=="Rotation")animatedRotation=true;
                switch(curve.property){case "Color.R":color.r=value;break;case "Color.G":color.g=value;break;case "Color.B":color.b=value;break;case "Color.A":color.a=value;state.colorAlpha=1;break;}
                if(curve.parameter=="Percentage"){percentage=value;animatedPercentage=true;animatedMaterial=true;}
                else if(curve.parameter=="StartPosition"){start=value;animatedMaterial=true;}
                else if(curve.parameter=="Width"){width=value;animatedMaterial=true;}
                else if(curve.parameter=="U Scroll")scrollU=value;
                else if(curve.parameter=="V Scroll")scrollV=value;
                else if(curve.parameter=="Index")animatedIndex=value;
            }
            // Numbered Future trails are driven by the owning needle's history
            // in the source widget, not by standalone animation tracks.
            if(needleTrails.TryRotation(layer,out float trailAngle)){state.angle=trailAngle;animatedRotation=true;}
            if(!animatedRotation&&layer.angleMin!=layer.angleMax){
                float phase=role=="rpm"?data.rpm/Idas3ArcadeHud.TachMaximum(data.revLimit):role=="speed"?data.speedKmh/240:role=="accel"?data.throttle:role=="brake"?data.brake:0;
                state.angle=Mathf.Lerp(layer.angleMin,layer.angleMax,Mathf.Clamp01(phase));
            }
            color.a*=state.opacity*ancestorAlpha;
            // Source day-change registration supersedes the editor's initial
            // Hidden/Collapsed flag, without changing its authored opacity.
            if(registeredDay)state.visible=true;
            // These source widgets start hidden and are revealed by source events.
            // Native drift opacity/current rev warning supply those events here.
            if(role=="drift"||role=="rev"){
                state.visible=true;
                // Steampunk's Stay curves already reveal its lamp group and
                // set its light intensity. Activation must preserve that alpha,
                // including its disabled negative-opacity white overlay.
                color.a=meter.id==66?Mathf.Clamp01(color.a)*opacity:opacity;
            }
            // Slate multiplies the brush tint after the animated widget color.
            // Keeping it separate preserves Steampunk's orange glass/glows and
            // source alpha even when a Color track supplies a white highlight.
            color*=Tint(layer.brushColor);
            // Reconstruct the stripped blink operator using its retained range
            // and rate. Authored artwork and additive blending remain intact.
            if(Contains(layer.materialParent,"M_Blink_Add01."))
                color.a*=Mathf.Clamp01(Scalar(layer,"Position",0)+Scalar(layer,"Amplitude",1)*Mathf.Sin(seconds*Scalar(layer,"BlinkSpeed",.5f)*Mathf.PI*2))*Scalar(layer,"BlinkOpacity",1);
            if(!state.visible||color.a<=0)continue;
            matrix*=initial.Local(layer.width*layer.pivotX,layer.height*layer.pivotY).inverse*state.Local(layer.width*layer.pivotX,layer.height*layer.pivotY);
            var uv=layer.uv!=null&&layer.uv.Length==4?new Rect(layer.uv[0],layer.uv[1],layer.uv[2],layer.uv[3]):new Rect(0,0,1,1);
            if(led)uv=ledUv;
            int digit=Digit(role,data);
            if((role=="gearRoll"||role=="gearEffect")&&animatedIndex>=0){
                // Metallic animates its blur atlas independently of the live
                // gear digit. Its Index passes the seven-cell boundary while
                // still visible, so repeat the atlas instead of dropping it.
                int cells=Math.Max(1,layer.atlasCols*layer.atlasRows);
                digit=Mathf.FloorToInt(Mathf.Repeat(animatedIndex,cells));
            }
            if(Contains(layer.materialParent,"FlipBook_Loop")){
                int cells=Math.Max(1,layer.atlasCols*layer.atlasRows);
                // The source retains FlipBook and Speed but strips the time
                // operator. Interpret Speed as cycles per second, independent
                // of render FPS, rather than displaying only its first cell.
                digit=Mathf.Min(cells-1,Mathf.FloorToInt(Mathf.Repeat(seconds*Scalar(layer,"Speed",1),1)*cells));
            }
            if(digit>=0){
                int columns=Math.Max(1,layer.atlasCols),rows=Math.Max(1,layer.atlasRows),index=digit+layer.digitOffset;
                if(index<0||index>=columns*rows)continue;
                uv=new Rect(floatModulo(index,columns)/(float)columns,1-(index/columns+1)/(float)rows,1f/columns,1f/rows);
            }
            int gaugeMode=0;Vector4 gauge=Vector4.zero;
            // Sirius uses a black cover over its colored arc. Its negative
            // source material offsets are not a conventional percentage: reveal
            // the arc as RPM rises by shrinking the remaining black cover.
            if(meter.id==39&&layer.name=="CenterPin")percentage=1-Mathf.Clamp01(data.rpm/Idas3ArcadeHud.TachMaximum(data.revLimit));
            if(Contains(layer.materialParent,"Circle")&&(role=="rpm"||role=="speed"||role=="accel"||role=="brake"||animatedPercentage)){
                if(!animatedMaterial)percentage=Mathf.Clamp01(role=="rpm"?data.rpm/Idas3ArcadeHud.TachMaximum(data.revLimit):role=="speed"?data.speedKmh/240:role=="accel"?data.throttle:data.brake);
                // Circle01 uses a moving end boundary. Circle02 uses a start
                // boundary; their authored pedal arcs run in opposite directions.
                float direction=Contains(layer.materialParent,"MaskCircle02")?-1:1;
                gaugeMode=1;gauge=new Vector4(start,Mathf.Max(.0001f,Mathf.Abs(width)),Mathf.Clamp01(percentage),direction*(width<0?-1:1));
            }
            else if((Contains(layer.materialParent,"Gauge")||Contains(layer.materialParent,"MaskVariable"))&&(role=="rpm"||role=="speed"||role=="accel"||role=="brake"||animatedPercentage)){
                if(!animatedPercentage)percentage=Mathf.Clamp01(role=="rpm"?data.rpm/Idas3ArcadeHud.TachMaximum(data.revLimit):role=="speed"?data.speedKmh/240:role=="accel"?data.throttle:data.brake);
                gaugeMode=2;gauge=new Vector4(0,0,Mathf.Clamp01(percentage),1);
            }
            Texture2D mask=null;var maskTransform=Matrix4x4.identity;bool additive=layer.additive;
            if(layer.retainers!=null)foreach(var retainer in layer.retainers){
                if(!Contains(retainer.materialParent,"RetainerMask")||retainer.owner==null)continue;
                if(retainer.textureBindings!=null)foreach(var binding in retainer.textureBindings)if(binding.name=="RetainerMask")mask=Texture(binding.texture);
                if(!mask)continue;
                var owner=retainer.owner;
                // The retainer mask is fixed in its own canvas while a needle
                // or trail moves underneath it; sampling the child UV is wrong.
                maskTransform=Matrix4x4.Scale(new Vector3(1/Mathf.Max(1,owner.width),1/Mathf.Max(1,owner.height),1))*Matrix(owner.transform).inverse*matrix;
                additive=retainer.additive;break;
            }
            Vector4 radial=Vector4.zero;
            if(Contains(layer.materialParent,"CircleGaugeGradation"))
                radial=new Vector4(Scalar(layer,"MaskRadius",.5f),Scalar(layer,"MaskDensity",8),Scalar(layer,"OpacityIntensity",1),0);
            if(role=="drift")++DriftSpriteCount;
            var sprite=new Idas3ArcadeHud.Sprite{texture=texture,rect=new Rect(0,0,layer.width,layer.height),uv=uv,color=color,fill=-1,additive=additive,
                transformed=true,transform=matrix,gaugeMode=gaugeMode,gauge=gauge,clipped=clipped,clip=clip,mask=mask,maskTransform=maskTransform,radial=radial};
            if(Contains(layer.materialParent,"M_Add_Ball")){
                sprite.materialEffect=1;
                sprite.effectParams=new Vector4(Scalar(layer,"S Radius",.2f),Scalar(layer,"M Radius",.3f),Scalar(layer,"L Radius",.5f),Scalar(layer,"Diamond",5));
                float strength=1;
                if(Contains(layer.materialParent,"Ball_Blink"))strength=(.55f+.45f*Mathf.Sin(seconds*Scalar(layer,"Speed",3)*Mathf.PI*2))*(.8f+.2f*Mathf.Sin(seconds*Scalar(layer,"Speed_2",.1f)*Mathf.PI*2));
                if(Contains(layer.name,"Visualizerbase"))strength*=audioEnergy*.4f;
                sprite.effectParams2=new Vector4(Scalar(layer,"S Density",.6f),Scalar(layer,"M Density",.5f),Scalar(layer,"L Density",.6f),strength);
            }else if(Contains(layer.materialParent,"M_Aura2.")){
                sprite.materialEffect=2;sprite.effectTex1=BoundTexture(layer,"Mask_02");sprite.effectTex2=BoundTexture(layer,"AuraNoise01");sprite.effectTex3=BoundTexture(layer,"AuraNoise02");
                sprite.effectParams=new Vector4(seconds*Scalar(layer,"Speed",1),Scalar(layer,"TexRotateSine",.3f),Scalar(layer,"ColorSine",.05f),0);
                sprite.effectColor1=MaterialColor(layer,"BaseColor",Color.red);sprite.effectColor2=MaterialColor(layer,"LightColor",Color.red);sprite.effectColor3=MaterialColor(layer,"HighLight",Color.yellow);
            }else if(Contains(layer.materialParent,"M_AudioCapture.")){
                sprite.materialEffect=3;sprite.effectTex1=audioTexture;sprite.effectTex2=BoundTexture(layer,"AudioNoise");
                sprite.effectColor1=MaterialColor(layer,"Color1",Color.cyan);sprite.effectColor2=MaterialColor(layer,"Color2",Color.green);
            }else if(led){sprite.materialEffect=4;sprite.effectTex1=BoundTexture(layer,"LedEffect01");sprite.effectTex2=BoundTexture(layer,"LedEffect02");}
            else if(Contains(layer.materialParent,"M_Add02.")){
                sprite.materialEffect=5;sprite.effectTex1=BoundTexture(layer,"Light");
                sprite.effectColor1=MaterialColor(layer,"EffectColor",Color.white);
            }else if(Contains(layer.materialParent,"M_NormalMaskVariable.")){
                sprite.materialEffect=6;sprite.gaugeMode=0;
                sprite.effectTex1=BoundTexture(layer,"MaskGrad");sprite.effectTex2=BoundTexture(layer,"MaskDetail");
                sprite.effectParams=new Vector4(Mathf.Clamp01(percentage),Scalar(layer,"RotationValue",0)*Mathf.Deg2Rad,0,0);
            }else if(Contains(layer.materialParent,"M_Scroll_Opacity.")){
                sprite.materialEffect=7;sprite.effectTex1=texture;sprite.effectTex2=BoundTexture(layer,"Tex_Base01");sprite.effectTex3=BoundTexture(layer,"Tex_Mask");
                sprite.effectParams=new Vector4(Scalar(layer,"Utiling",1),Scalar(layer,"Vtiling",1),seconds*Scalar(layer,"Speed",1)*Scalar(layer,"SpeedX",0),seconds*Scalar(layer,"Speed",1)*Scalar(layer,"SpeedY",0));
                sprite.effectParams2=new Vector4(Scalar(layer,"H_Radius",.7f),Scalar(layer,"H_Density",1),Scalar(layer,"V_Radius",.7f),Scalar(layer,"V_Density",1));
            }
            if(Contains(layer.materialParent,"M_UVScroll."))sprite.sampleMotion=new Vector4(scrollU,-scrollV,0,1);
            if(Contains(layer.materialParent,"MeterRotation")||Contains(layer.materialParent,"EffRotation")){
                // The recovered materials contain two fixed/parameterized
                // textures. Recompose both, with their authored tint and rate.
                // This is a translucent reconstruction, not the stripped graph.
                var first=BoundTexture(layer,"Texture01");var second=BoundTexture(layer,"Texture02");
                if(first)sprite.texture=first;
                sprite.color=color*MaterialColor(layer,"Color1",Color.white);
                sprite.sampleMotion=new Vector4(0,0,materialAnimation.Rotation(layer,"frame01speed",Scalar(layer,"frame01speed",0)),0);
                result.Add(sprite);
                if(second){
                    sprite.texture=second;
                    var tint=Contains(layer.materialParent,"EffRotation")?
                        Color.Lerp(MaterialColor(layer,"Color2-1",Color.white),MaterialColor(layer,"Color2-2",Color.white),.5f):MaterialColor(layer,"Color2",Color.white);
                    sprite.color=color*tint;
                    sprite.sampleMotion=new Vector4(0,0,materialAnimation.Rotation(layer,"frame02speed",Scalar(layer,"frame02speed",Scalar(layer,"frame01speed",0))),0);
                    result.Add(sprite);
                }
            }else result.Add(sprite);
        }
    }
    static int floatModulo(int value,int divisor)=>value%divisor;
}
