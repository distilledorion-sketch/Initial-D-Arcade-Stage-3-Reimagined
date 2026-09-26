using System;
using System.Collections.Generic;
using UnityEngine;

// IDs are saved values, not list positions. Keep the first Stuttgart release's
// value (1), and use source ID + 2 for later imports. Missing source IDs stay gaps.
public static class Idas3ArcadeMeterCatalog
{
    [Serializable] public sealed class Catalog { public Meter[] meters=Array.Empty<Meter>(); }
    [Serializable] public sealed class Meter {
        public int id;public string name,sourceNameJapanese;
        public float width,height;
        public Layer[] layers=Array.Empty<Layer>();
        public string[] limitations=Array.Empty<string>();
    }
    [Serializable] public sealed class Layer {
        public string name,texture,nightTexture,role,disabledReason;
        public string material,materialParent,visibility,visibilityDay;
        public float x,y,width,height,pivotX=.5f,pivotY=.5f,angle,angleMin,angleMax;
        public float opacity=1;
        public float ownOpacity=1,translationX,translationY,scaleX=1,scaleY=1,shearX,shearY;
        public float[] transform,color,brushColor,uv,clipRect;
        public bool additive;
        public int atlasCols=1,atlasRows=1,digitOffset;
        public Variant[] textureVariants=Array.Empty<Variant>();
        public string[] speedTextures=Array.Empty<string>();
        public Curve[] curves=Array.Empty<Curve>();
        public Parameter[] parameters=Array.Empty<Parameter>();
        public TextureBinding[] textureBindings=Array.Empty<TextureBinding>();
        public VectorParameter[] vectorParameters=Array.Empty<VectorParameter>();
        public Owner[] parents=Array.Empty<Owner>();
        public Switcher[] switchers=Array.Empty<Switcher>();
        public Retainer[] retainers=Array.Empty<Retainer>();
    }
    [Serializable] public sealed class Retainer {
        public string material,materialParent;public bool additive;public Owner owner;
        public Parameter[] parameters=Array.Empty<Parameter>();
        public TextureBinding[] textureBindings=Array.Empty<TextureBinding>();
    }
    [Serializable] public sealed class Parameter {public string name;public float value;}
    [Serializable] public sealed class TextureBinding {public string name,texture;}
    [Serializable] public sealed class VectorParameter {public string name;public float[] values;}
    [Serializable] public sealed class Switcher {public string name;public int index,activeIndex;}
    [Serializable] public sealed class Variant {public string texture,day,state;public int maxRpm,index;}
    [Serializable] public sealed class Curve {
        public string animation,parameter,property;
        public float[] times,values;
        public float defaultValue;
        public float animationStart,animationEnd;
        public float playbackStart,playbackEnd,ticksPerSecond=24000;
        public bool tickResolutionRecovered;
        public Owner owner;
    }
    [Serializable] public sealed class Owner {
        public string name;public float[] transform,clipRect;
        public float pivotX=.5f,pivotY=.5f,width,height,angle,translationX,translationY,scaleX=1,scaleY=1,shearX,shearY;
        public float opacity=1,colorAlpha=1;public bool clipsToBounds;
    }
    static readonly List<int> styles=new List<int>();
    static readonly Dictionary<int,Meter> meters=new Dictionary<int,Meter>();
    static bool loaded;
    static void Load(){
        if(loaded)return;loaded=true;styles.Add(0);styles.Add(1);
        var json=Resources.Load<TextAsset>("ArcadeHud/Catalog/catalog");
        if(!json)return;
        var data=JsonUtility.FromJson<Catalog>(json.text);if(data?.meters==null)return;
        Array.Sort(data.meters,(a,b)=>a.id.CompareTo(b.id));
        foreach(var meter in data.meters){
            if(meter==null||meter.id<0||meter.id>89||meters.ContainsKey(meter.id)||meter.layers==null||meter.layers.Length==0)continue;
            meters.Add(meter.id,meter);if(meter.id!=31)styles.Add(meter.id+2);
        }
    }
    public static int Count {get{Load();return styles.Count;}}
    public static int StyleAt(int index){Load();return styles[Mathf.Clamp(index,0,styles.Count-1)];}
    public static int IndexOfStyle(int style){Load();return Math.Max(0,styles.IndexOf(style));}
    public static bool IsValidStyle(int style){if(style==0||style==1)return true;Load();return styles.Contains(style);}
    public static int SourceId(int style)=>style==1?31:style>=2?style-2:-1;
    public static Meter Get(int style){Load();meters.TryGetValue(SourceId(style),out var meter);return meter;}
    public static string Name(int style)=>style==0?"Original":style==1?"Stuttgart":Get(style)?.name??"Original";
    public static bool IsAvailable(int style)=>style==0||style==1?Idas3ArcadeHud.Available:Get(style)!=null;
}
