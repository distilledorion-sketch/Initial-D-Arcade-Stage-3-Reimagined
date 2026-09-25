using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using UnityEngine;

// Content can extend beyond a source widget's nominal canvas. Cache a stable
// union across the supported telemetry range instead of resizing on each frame.
public static class Idas3MeterLayoutBounds
{
    // Bump when composition or bounds sampling changes its authored envelope.
    const int RendererVersion=6;
    [Serializable] sealed class AlphaCatalog {public AlphaTexture[] textures=Array.Empty<AlphaTexture>();}
    [Serializable] sealed class AlphaTexture {public string texture,name;public float x,y,width,height;}
    [Serializable] sealed class BakedCatalog {public int rendererVersion;public string catalogSha256,alphaSha256;public BakedLayout[] layouts;}
    [Serializable] sealed class BakedLayout {public int style;public Rect bounds;}
    static readonly Dictionary<int,Rect> layouts=new Dictionary<int,Rect>();
    static readonly Dictionary<string,Rect> alpha=new Dictionary<string,Rect>(StringComparer.Ordinal);
    static readonly HashSet<string> missingAlpha=new HashSet<string>(StringComparer.Ordinal);
    static bool loaded,bakeChecked,bakeLoaded;
    static int sampledStyles;
    internal static int AlphaTextureCount {get{LoadAlpha();return alpha.Count;}}
    internal static int MissingAlphaBoundsCount=>missingAlpha.Count;
    internal static int SampledStyleCount=>sampledStyles;
    static string HashResource(string path){
        var resource=Resources.Load<TextAsset>(path);
        if(!resource)return "";
        using(var hash=SHA256.Create())return BitConverter.ToString(hash.ComputeHash(resource.bytes)).Replace("-","").ToLowerInvariant();
    }
    static bool Finite(float value)=>!float.IsNaN(value)&&!float.IsInfinity(value);
    static bool Valid(Rect rect)=>Finite(rect.x)&&Finite(rect.y)&&Finite(rect.width)&&Finite(rect.height)&&rect.width>0&&rect.height>0;
    static bool Current(BakedCatalog baked)=>baked!=null&&baked.rendererVersion==RendererVersion&&baked.layouts!=null&&
        !string.IsNullOrEmpty(baked.catalogSha256)&&!string.IsNullOrEmpty(baked.alphaSha256)&&
        baked.catalogSha256==HashResource("ArcadeHud/Catalog/catalog")&&baked.alphaSha256==HashResource("ArcadeHud/Catalog/alpha-bounds");
    static void LoadBaked(){
        if(bakeChecked)return;bakeChecked=true;
        var resource=Resources.Load<TextAsset>("ArcadeHud/Catalog/layout-bounds");if(!resource)return;
        BakedCatalog baked;
        try{baked=JsonUtility.FromJson<BakedCatalog>(resource.text);}catch(ArgumentException){return;}
        if(!Current(baked))return;
        var entries=new Dictionary<int,Rect>();
        foreach(var entry in baked.layouts){
            if(entry==null||entry.style<=0||!Idas3ArcadeMeterCatalog.IsValidStyle(entry.style)||!Valid(entry.bounds)||entries.ContainsKey(entry.style))return;
            entries.Add(entry.style,entry.bounds);
        }
        if(entries.Count!=Idas3ArcadeMeterCatalog.Count-1)return;
        foreach(var entry in entries)layouts[entry.Key]=entry.Value;
        bakeLoaded=true;
    }
    // Editor QA must exercise the sampler even when a current bake exists.
    public static void RecalculateForVerification(){layouts.Clear();alpha.Clear();missingAlpha.Clear();loaded=false;bakeChecked=true;bakeLoaded=false;sampledStyles=0;}
    public static string ExportVerifiedBake(){
        if(sampledStyles!=Idas3ArcadeMeterCatalog.Count-2)throw new InvalidOperationException("All imported layouts must be sampled before baking");
        var baked=new BakedCatalog{rendererVersion=RendererVersion,catalogSha256=HashResource("ArcadeHud/Catalog/catalog"),alphaSha256=HashResource("ArcadeHud/Catalog/alpha-bounds"),layouts=new BakedLayout[Idas3ArcadeMeterCatalog.Count-1]};
        for(int i=1;i<Idas3ArcadeMeterCatalog.Count;++i){int style=Idas3ArcadeMeterCatalog.StyleAt(i);baked.layouts[i-1]=new BakedLayout{style=style,bounds=Get(style)};}
        return JsonUtility.ToJson(baked,true);
    }
    public static void VerifyBakedCache(){
        var expected=new Dictionary<int,Rect>(layouts);
        layouts.Clear();bakeChecked=false;bakeLoaded=false;sampledStyles=0;
        LoadBaked();
        if(!bakeLoaded)throw new InvalidOperationException("The imported bounds bake is missing, stale or invalid");
        foreach(var entry in expected){var actual=Get(entry.Key);var wanted=entry.Value;
            if(Mathf.Abs(actual.x-wanted.x)>.0001f||Mathf.Abs(actual.y-wanted.y)>.0001f||Mathf.Abs(actual.width-wanted.width)>.0001f||Mathf.Abs(actual.height-wanted.height)>.0001f)
                throw new InvalidOperationException("Baked bounds changed for style "+entry.Key);
        }
        if(sampledStyles!=0)throw new InvalidOperationException("The runtime bounds bake unexpectedly sampled a meter");
    }
    static void LoadAlpha(){
        if(loaded)return;loaded=true;
        var resource=Resources.Load<TextAsset>("ArcadeHud/Catalog/alpha-bounds");
        if(!resource)return;
        var catalog=JsonUtility.FromJson<AlphaCatalog>(resource.text);
        if(catalog?.textures==null)return;
        foreach(var item in catalog.textures){
            if(item==null)continue;
            string name=string.IsNullOrEmpty(item.name)?Path.GetFileName(item.texture):item.name;
            if(string.IsNullOrEmpty(name))continue;
            var rect=new Rect(item.x,item.y,item.width,item.height);
            if(alpha.TryGetValue(name,out var existing))rect=Union(existing,rect);
            alpha[name]=rect;
        }
    }
    static Rect Union(Rect a,Rect b){
        if(a.width<=0||a.height<=0)return b;if(b.width<=0||b.height<=0)return a;
        return Rect.MinMaxRect(Mathf.Min(a.xMin,b.xMin),Mathf.Min(a.yMin,b.yMin),Mathf.Max(a.xMax,b.xMax),Mathf.Max(a.yMax,b.yMax));
    }
    static Rect Intersect(Rect a,Rect b){
        float x=Mathf.Max(a.xMin,b.xMin),y=Mathf.Max(a.yMin,b.yMin),right=Mathf.Min(a.xMax,b.xMax),bottom=Mathf.Min(a.yMax,b.yMax);
        return right>x&&bottom>y?Rect.MinMaxRect(x,y,right,bottom):default;
    }
    static Rect VisibleLocal(Idas3ArcadeHud.Sprite sprite){
        // A moving UV samples different alpha regions without moving its quad.
        // Reserve the full quad so rotation/scroll never changes the HUD fit.
        if(sprite.sampleMotion!=Vector4.zero||sprite.materialEffect!=0)return sprite.rect;
        if(!alpha.TryGetValue(sprite.texture.name,out var image)){
            missingAlpha.Add(sprite.texture.name);image=new Rect(0,0,1,1);
        }
        if(image.width<=0||image.height<=0)return default;
        // Import metadata is top-left, while the atlas uses bottom-left UVs.
        var visibleUv=Intersect(new Rect(image.x,1-image.yMax,image.width,image.height),sprite.uv);
        if(visibleUv.width<=0||visibleUv.height<=0||sprite.uv.width<=0||sprite.uv.height<=0)return default;
        var quad=sprite.rect;
        return new Rect(quad.x+(visibleUv.x-sprite.uv.x)/sprite.uv.width*quad.width,
            quad.y+(1-(visibleUv.yMax-sprite.uv.y)/sprite.uv.height)*quad.height,
            visibleUv.width/sprite.uv.width*quad.width,visibleUv.height/sprite.uv.height*quad.height);
    }
    static Vector2 Point(Idas3ArcadeHud.Sprite sprite,float x,float y){
        if(sprite.transformed)return sprite.transform.MultiplyPoint3x4(new Vector3(x,y,0));
        var pivot=sprite.rect.center;float angle=sprite.angle*Mathf.Deg2Rad,c=Mathf.Cos(angle),s=Mathf.Sin(angle);
        x-=pivot.x;y-=pivot.y;return pivot+new Vector2(x*c-y*s,x*s+y*c);
    }
    static Rect VisibleMeter(Idas3ArcadeHud.Sprite sprite){
        var local=VisibleLocal(sprite);if(local.width<=0||local.height<=0)return default;
        var a=Point(sprite,local.xMin,local.yMin);var b=Point(sprite,local.xMax,local.yMin);
        var c=Point(sprite,local.xMin,local.yMax);var d=Point(sprite,local.xMax,local.yMax);
        var bounds=Rect.MinMaxRect(Mathf.Min(Mathf.Min(a.x,b.x),Mathf.Min(c.x,d.x)),Mathf.Min(Mathf.Min(a.y,b.y),Mathf.Min(c.y,d.y)),
            Mathf.Max(Mathf.Max(a.x,b.x),Mathf.Max(c.x,d.x)),Mathf.Max(Mathf.Max(a.y,b.y),Mathf.Max(c.y,d.y)));
        return sprite.clipped?Intersect(bounds,sprite.clip):bounds;
    }
    public static Rect Get(int style){
        if(style<=1)return new Rect(0,0,692,328);
        LoadBaked();
        if(layouts.TryGetValue(style,out var cached))return cached;
        var meter=Idas3ArcadeMeterCatalog.Get(style);
        if(meter==null)return new Rect(0,0,692,328);
        LoadAlpha();
        var options=new Idas3GameOptions.Values{hudMeterStyle=style,hudShiftLights=true,hudPedalIndicators=true,hudNameplateStyle=0};
        var sprites=new List<Idas3ArcadeHud.Sprite>(64);Rect bounds=default;
        using(var renderer=new Idas3ImportedMeter()){
            // Reserve the full audio envelope only in the layout sampler. Live
            // meters always use actual output PCM, including silence.
            var maximumAudio=new float[Idas3MeterAudioSpectrum.BandCount];
            for(int i=0;i<maximumAudio.Length;++i)maximumAudio[i]=1;
            renderer.AudioBandsOverride=maximumAudio;
            // Sixteen intervals include the ends and intermediate needle arcs.
            // All tach faces, transmission/day variants and full lamp states are
            // composed through the production adapter. No native data is read.
            foreach(float limit in new[]{8000f,9000f,10000f,13000f})for(uint state=0;state<4;++state)for(int step=0;step<=16;++step){
                float phase=step/16f;
                var telemetry=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1|(state&1)*2|(state&2)*2|8,
                    gear=1+Mathf.Min(5,Mathf.FloorToInt(phase*6)),speedKmh=phase*300,rpm=phase*limit*1.1f,revLimit=limit,
                    throttle=phase,brake=phase,driftOpacity=1};
                sprites.Clear();renderer.Compose(sprites,meter,options,telemetry,phase*.8f);
                foreach(var sprite in sprites)if(sprite.texture&&sprite.color.a>0)bounds=Union(bounds,VisibleMeter(sprite));
            }
            // A gear event starts at a transition, so the telemetry sweep above
            // cannot sample its complete transient scale/translation envelope.
            // Prime each event and sample it independently, including expiration.
            for(int step=0;step<=48;++step){
                var telemetry=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,gear=3,
                    speedKmh=150,rpm=7000,revLimit=8500,throttle=1,driftOpacity=1};
                sprites.Clear();renderer.Compose(sprites,meter,options,telemetry,0);
                telemetry.gear=4;sprites.Clear();renderer.Compose(sprites,meter,options,telemetry,.1f);
                sprites.Clear();renderer.Compose(sprites,meter,options,telemetry,.1f+step/24f);
                foreach(var sprite in sprites)if(sprite.texture&&sprite.color.a>0)bounds=Union(bounds,VisibleMeter(sprite));
            }
            // Halloween's lantern swings on real drift enter/exit events. The
            // steady lamp samples above cannot cover that motion. Reserve its
            // complete source animation in both directions so the fit stays
            // stable and does not cut off the lantern while it swings.
            if(meter.id==83){
                float duration=0;
                foreach(var layer in meter.layers)if(layer.curves!=null)
                    foreach(var curve in layer.curves)if(curve.owner?.name=="Cantera"&&curve.animation=="Anim_DriftLamp_InOut_INST"&&curve.property=="Rotation")
                        duration=Mathf.Max(duration,Idas3MeterAnimationState.DurationSeconds(curve));
                if(duration>0)foreach(uint drift in new uint[]{0,8})for(int step=0;step<=60;++step){
                    var telemetry=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1|(drift^8),gear=3,
                        speedKmh=150,rpm=7000,revLimit=8500,throttle=1,driftOpacity=1};
                    sprites.Clear();renderer.Compose(sprites,meter,options,telemetry,0);
                    telemetry.flags=1|drift;sprites.Clear();renderer.Compose(sprites,meter,options,telemetry,.1f);
                    sprites.Clear();renderer.Compose(sprites,meter,options,telemetry,.1f+duration*step/60f);
                    foreach(var sprite in sprites)if(sprite.texture&&sprite.color.a>0)bounds=Union(bounds,VisibleMeter(sprite));
                }
            }
        }
        if(bounds.width<=0||bounds.height<=0)bounds=new Rect(0,0,Mathf.Max(1,meter.width),Mathf.Max(1,meter.height));
        // Conservative allowance for filtering and extrema between arc samples.
        float padding=Mathf.Max(4,Mathf.Max(bounds.width,bounds.height)*.015f);
        bounds=Rect.MinMaxRect(bounds.xMin-padding,bounds.yMin-padding,bounds.xMax+padding,bounds.yMax+padding);
        layouts.Add(style,bounds);++sampledStyles;return bounds;
    }
}
