using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
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
        public bool speedRainbow;
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
    // Only selection metadata is needed to list/validate meters. Do not retain
    // every meter's layers, curves and parent hierarchies for one selected HUD.
    [Serializable] sealed class IndexMeter {public int id;public string name;public LayerMarker[] layers;}
    [Serializable] sealed class LayerMarker { }
    struct Slice {public int offset,length;}
    sealed class Entry {
        public string name;public Slice slice;
        public WeakReference<Meter> loaded;
    }
    static readonly Dictionary<int,Entry> meters=new Dictionary<int,Entry>();
    static byte[] source;
    static string sourceSha256;
    internal static string SourceSha256 {get{Load();return sourceSha256??"";}}
    internal static int LoadedMeterCount {
        get {int count=0;foreach(var entry in meters.Values)if(entry.loaded!=null&&entry.loaded.TryGetTarget(out _))++count;return count;}
    }
    static bool loaded;
    static void Load(){
        if(loaded)return;loaded=true;styles.Add(0);styles.Add(1);
        var json=Resources.Load<TextAsset>("ArcadeHud/Catalog/catalog");
        if(!json)return;
        try {
            source=json.bytes;
            using(var hash=SHA256.Create())sourceSha256=BitConverter.ToString(hash.ComputeHash(source)).Replace("-","").ToLowerInvariant();
            var slices=IndexSlices(source);
            foreach(var slice in slices){
                if(slice.length==0)continue;
                var meter=JsonUtility.FromJson<IndexMeter>(Encoding.UTF8.GetString(source,slice.offset,slice.length));
                if(meter==null||meter.id<0||meter.id>117||meter.id==113||meter.id==114||meters.ContainsKey(meter.id)||meter.layers==null||meter.layers.Length==0)continue;
                meters.Add(meter.id,new Entry{name=meter.name,slice=slice});
                if(meter.id!=31)styles.Add(meter.id+2);
            }
            styles.Sort();
        } finally {Resources.UnloadAsset(json);}
    }
    static List<Slice> IndexSlices(byte[] bytes){
        // Boundary index for our packaged version-1 meters schema, not a JSON
        // deserializer. JsonUtility still parses each header/selected object.
        // Scan UTF-8 directly so indexing needs neither a full UTF-16 copy nor
        // a managed token object/string for every unused animation property.
        int cursor=bytes.Length>=3&&bytes[0]==0xef&&bytes[1]==0xbb&&bytes[2]==0xbf?3:0;
        void Space(){while(cursor<bytes.Length&&(bytes[cursor]==32||bytes[cursor]==9||bytes[cursor]==10||bytes[cursor]==13))++cursor;}
        void Expect(byte value){Space();if(cursor>=bytes.Length||bytes[cursor++]!=value)throw new InvalidDataException("Invalid meter catalogue structure");}
        Expect((byte)'{');Space();
        if(cursor+9<=bytes.Length&&Encoding.ASCII.GetString(bytes,cursor,9)=="\"version\""){
            cursor+=9;Expect((byte)':');Expect((byte)'1');Expect((byte)',');Space();
        }
        foreach(char c in "\"meters\"")if(cursor>=bytes.Length||bytes[cursor++]!=c)throw new InvalidDataException("Meter catalogue array is missing");
        Expect((byte)':');Expect((byte)'[');
        var slices=new List<Slice>();var closings=new byte[128];Space();
        if(cursor<bytes.Length&&bytes[cursor]!=']')while(true){
            Space();int start=cursor,depth=0;bool quoted=false,escaped=false;
            if(cursor+4<=bytes.Length&&bytes[cursor]=='n'&&bytes[cursor+1]=='u'&&bytes[cursor+2]=='l'&&bytes[cursor+3]=='l'){
                cursor+=4;slices.Add(default);
            }else{
                if(cursor>=bytes.Length||bytes[cursor]!='{')throw new InvalidDataException("Invalid meter catalogue object");
                do{
                    if(cursor>=bytes.Length)throw new InvalidDataException("Truncated meter catalogue object");
                    byte c=bytes[cursor++];
                    if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
                    if(c=='"'){quoted=true;continue;}
                    if(c=='{'||c=='['){
                        if(depth==closings.Length)throw new InvalidDataException("Meter catalogue nesting is too deep");
                        closings[depth++]=(byte)(c=='{'?'}':']');
                    }else if(c=='}'||c==']'){
                        if(depth==0||closings[--depth]!=c)throw new InvalidDataException("Unbalanced meter catalogue object");
                    }
                }while(depth>0||quoted);
                slices.Add(new Slice{offset=start,length=cursor-start});
            }
            Space();if(cursor<bytes.Length&&bytes[cursor]==']')break;
            Expect((byte)',');
        }
        Expect((byte)']');Expect((byte)'}');Space();
        if(cursor!=bytes.Length)throw new InvalidDataException("Unexpected data after meter catalogue");
        return slices;
    }
    internal static Meter ReadMeter(byte[] bytes,int ordinal){
        var slices=IndexSlices(bytes);
        if(ordinal<0||ordinal>=slices.Count)throw new InvalidDataException("Meter catalogue entry is missing: "+ordinal);
        var slice=slices[ordinal];return slice.length==0?null:JsonUtility.FromJson<Meter>(Encoding.UTF8.GetString(bytes,slice.offset,slice.length));
    }
    public static int Count {get{Load();return styles.Count;}}
    public static int StyleAt(int index){Load();return styles[Mathf.Clamp(index,0,styles.Count-1)];}
    public static int IndexOfStyle(int style){Load();return Math.Max(0,styles.IndexOf(style));}
    public static bool IsValidStyle(int style){if(style==0||style==1)return true;Load();return styles.Contains(style);}
    public static int SourceId(int style)=>style==1?31:style>=2?style-2:-1;
    public static Meter Get(int style){
        Load();int id=SourceId(style);if(!meters.TryGetValue(id,out var entry))return null;
        if(entry.loaded!=null&&entry.loaded.TryGetTarget(out var cached))return cached;
        var meter=JsonUtility.FromJson<Meter>(Encoding.UTF8.GetString(source,entry.slice.offset,entry.slice.length));
        if(meter==null||meter.id!=id||meter.layers==null||meter.layers.Length==0)throw new InvalidDataException("Invalid meter catalogue entry: "+id);
        // Active gameplay and preview renderers own strong references. Browsing
        // other meters must not permanently retain their detailed metadata.
        entry.loaded=new WeakReference<Meter>(meter);return meter;
    }
    public static string Name(int style){
        if(style==0)return "Original";if(style==1)return "Stuttgart";
        Load();return meters.TryGetValue(SourceId(style),out var entry)?entry.name??"Original":"Original";
    }
    public static bool IsAvailable(int style){
        if(style==0||style==1)return Idas3ArcadeHud.Available;
        Load();return meters.ContainsKey(SourceId(style));
    }
}
