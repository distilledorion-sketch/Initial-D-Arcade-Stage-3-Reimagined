using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;
using UnityEngine.Rendering;

// Recovered meters. Authored art, pivots, atlas cells and scale ranges are
// preserved; telemetry and animation are supplied by this game.
public sealed class Idas3ArcadeHud : IDisposable
{
    [StructLayout(LayoutKind.Sequential,Pack=8)]
    public struct Telemetry {
        public uint size,version,flags;
        public int gear;
        public float speedKmh,rpm,revLimit,throttle,brake;
        public float driftOpacity;
    }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    static extern int Idas3SceneGetHudTelemetry(ref Telemetry value);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    static extern int Idas3SceneCopyHudDriverName([Out] byte[] destination,int capacity);
    internal struct Sprite {
        public Texture texture;public Rect rect,uv;public Color color;
        public float angle,fill;public bool brake,alphaOnly,additive;
        public bool transformed,clipped;public Matrix4x4 transform;public Rect clip;
        public int gaugeMode;public Vector4 gauge;
        public Texture mask;public Matrix4x4 maskTransform;public Vector4 radial;
        // XY scroll, Z rotation in turns, W repeat (otherwise transparent outside).
        public Vector4 sampleMotion;
        public int materialEffect;
        public Texture effectTex1,effectTex2,effectTex3;
        public Vector4 effectParams,effectParams2;
        public Color effectColor1,effectColor2,effectColor3;
    }
    static readonly Dictionary<string,Texture2D> textures=new Dictionary<string,Texture2D>();
    static readonly List<Sprite> previewSprites=new List<Sprite>(64);
    static readonly Idas3ImportedMeter previewImported=new Idas3ImportedMeter();
    static GUIStyle previewLabel,previewName;
    static bool loaded,available;
    static Font font;
    readonly List<Sprite> sprites=new List<Sprite>(64);
    readonly List<Vector3> points=new List<Vector3>(256);
    readonly List<Vector2> uv=new List<Vector2>(256);
    readonly List<Color32> colors=new List<Color32>(256);
    readonly List<int[]> indices=new List<int[]>();
    readonly List<MaterialPropertyBlock> properties=new List<MaterialPropertyBlock>();
    readonly byte[] nameBytes=new byte[128];
    Mesh mesh;Material material,additiveMaterial;
    readonly Idas3ImportedMeter imported=new Idas3ImportedMeter();
    // Per-renderer deterministic input for GPU verification; live rendering
    // leaves this null and reads the game's PCM spectrum.
    internal float[] AudioBandsOverride {get;set;}
    string driverName="PLAYER";float nextNameRefresh;
    internal int SpriteCount=>sprites.Count;
    internal float DriftLampOpacity {get;private set;}
    internal int DriftLampSpriteCount {get;private set;}
    public static bool Available {get{Load();return available;}}
    static Texture2D Texture(string name){textures.TryGetValue(name,out var value);return value;}
    static void Load(){
        if(loaded)return;loaded=true;available=true;
        foreach(var name in new[]{"Base","BaseFrame","BaseMission_01","BaseMission_02","PointRmp_A","PointRmp_B","PointSpd_A","PointSpd_B","RevLamp","PointAccel_Mask","PointBrake_Mask","Spd_A","Spd_B","SpdNum01","ShiftNum",
            "Rmp01_A","Rmp01_B","Rmp03_A","Rmp03_B","Rmp05_A","Rmp05_B","Rmp08_A","Rmp08_B"}){
            var value=Resources.Load<Texture2D>("ArcadeHud/Meter31/T_Meter31_"+name);textures[name]=value;available&=value!=null;
        }
        textures["Nameplate"]=Resources.Load<Texture2D>("ArcadeHud/Extras/T_Race_RivalNamePlate1");
        available&=textures["Nameplate"]!=null;
        textures["DriftCore"]=Resources.Load<Texture2D>("ArcadeHud/Shared/T_Meter00_LampEf_03");
        textures["DriftGlow"]=Resources.Load<Texture2D>("ArcadeHud/Shared/T_Meter00_LampEf_01");
        available&=textures["DriftCore"]!=null&&textures["DriftGlow"]!=null;
        if(!available)Debug.LogWarning("Arcade HUD artwork is incomplete; keeping the original meter.");
    }
    public static Telemetry Demo(float seconds){
        float cycle=Mathf.Repeat(seconds,8)/8f;
        // Synthetic eight-second preview: drift from 2s to 5s, then fade out.
        // This animation is never used to determine the live vehicle's state.
        float phase=cycle*8,drift=Mathf.SmoothStep(0,1,Mathf.InverseLerp(2,2.15f,phase))*(1-Mathf.SmoothStep(0,1,Mathf.InverseLerp(5,5.35f,phase)));
        return new Telemetry{size=40,version=2,flags=1u|(phase>=2&&phase<5?8u:0u),gear=1+Mathf.FloorToInt(cycle*5),speedKmh=35+cycle*170,rpm=1100+Mathf.Repeat(cycle*5,1)*7500,revLimit=8500,
            throttle=cycle<.78f?Mathf.Clamp01(.3f+cycle):0,brake=cycle>.78f?Mathf.InverseLerp(.78f,1,cycle):0,driftOpacity=drift};
    }
    internal static bool Read(out Telemetry value){
        value=new Telemetry{size=40};return Idas3SceneGetHudTelemetry(ref value)==1&&(value.flags&1)!=0;
    }
    internal static int TachMaximum(float limit)=>limit<=8000?8000:limit<=9000?9000:limit<=10000?10000:13000;
    internal static Rect DigitUv(int digit){digit=Mathf.Clamp(digit,0,10);return new Rect((digit%4)/4f,1-(digit/4+1)/3f,.25f,1/3f);}
    internal static Rect MeterBounds(float width,float height,Idas3GameOptions.Values options){
        var size=Dimensions(options);float fit=Mathf.Min(width/1280f,height/720f);
        float scale=Mathf.Min(420f/size.x,240f/size.y)*fit*options.HudGroupScale(2);
        float h=size.y+(options.hudNameplateStyle==1?72:0);
        return new Rect(width-(size.x*scale+20*fit)+options.HudOffset(2).x*width,
            height-(h*scale+16*fit)+options.HudOffset(2).y*height,size.x*scale,h*scale);
    }
    static Vector2 Dimensions(Idas3GameOptions.Values options)=>Idas3MeterLayoutBounds.Get(options.hudMeterStyle).size;
    static float Safe(float value,float fallback=0)=>float.IsNaN(value)||float.IsInfinity(value)?fallback:value;
    static void Add(List<Sprite> list,string name,float x,float y,float w,float h,float angle=0,Color? color=null,float fill=-1,Rect? atlas=null,bool additive=false){
        var texture=Texture(name);if(!texture)return;
        list.Add(new Sprite{texture=texture,rect=new Rect(346+x-w*.5f,164+y-h*.5f,w,h),uv=atlas??new Rect(0,0,1,1),color=color??Color.white,angle=angle,fill=fill,brake=name=="PointBrake_Mask",additive=additive});
    }
    internal static float LampOpacity(Telemetry t)=>t.version>=2?Mathf.Clamp01(Safe(t.driftOpacity)):0;
    static void Compose(List<Sprite> list,Idas3GameOptions.Values options,Telemetry t,float seconds,Idas3ImportedMeter renderer){
        list.Clear();Load();if(!available)return;
        if(options.hudMeterStyle>1){
            var meter=Idas3ArcadeMeterCatalog.Get(options.hudMeterStyle);renderer.Compose(list,meter,options,t,seconds);
            if(meter!=null&&options.hudNameplateStyle==1){var content=Idas3MeterLayoutBounds.Get(options.hudMeterStyle);
                list.Add(new Sprite{texture=Texture("Nameplate"),rect=new Rect(content.center.x-133.5f,content.y-48,267,44),uv=new Rect(0,0,1,1),color=Color.white,fill=-1});}
            return;
        }
        if(options.hudMeterStyle==0)renderer.Dispose();
        float limit=Mathf.Max(1000,Safe(t.revLimit,8500)),rpm=Mathf.Max(0,Safe(t.rpm)),speed=Mathf.Max(0,Safe(t.speedKmh));
        int max=TachMaximum(limit);string face=max==8000?"01":max==9000?"03":max==10000?"05":"08";
        string day=(t.flags&4)!=0?"B":"A";
        Add(list,"Base",0,0,692,328);
        Add(list,"Rmp"+face+"_"+day,0,-22,232,232);
        Add(list,"Spd_"+day,210,3,200,200);
        if(options.hudShiftLights&&rpm>=limit*.92f){
            float warning=rpm>=limit?.6f+.4f*Mathf.Cos(seconds*Mathf.PI*10):Mathf.InverseLerp(limit*.92f,limit,rpm);
            Add(list,"RevLamp",0,-20,180,180,0,new Color(1,.13f,.07f,warning));
        }
        if(options.hudPedalIndicators){
            Add(list,"PointAccel_Mask",-212,2,180,180,0,Color.white,Mathf.Clamp01(Safe(t.throttle)));
            Add(list,"PointBrake_Mask",-211,1,180,180,0,Color.white,Mathf.Clamp01(Safe(t.brake)));
        }
        Add(list,"BaseFrame",-1,2,636,300);
        int gear=Mathf.Clamp(t.gear,0,6),kmh=Mathf.Clamp(Mathf.FloorToInt(speed),0,999);
        Add(list,"ShiftNum",-108,115,44,48,0,null,-1,DigitUv(gear));
        if(options.hudMeterStyle==1){
            // Preserve Stuttgart's established layout while adding its authored
            // gear flash. The recovered canvas has equal extra side margins.
            var source=Idas3ArcadeMeterCatalog.Get(1);int first=list.Count;
            renderer.Compose(list,source,options,t,seconds,true);
            if(source!=null)for(int i=first;i<list.Count;++i){var flash=list[i];
                flash.transform=Matrix4x4.Translate(new Vector3((692-source.width)*.5f,0,0))*flash.transform;list[i]=flash;}
        }
        if(kmh>=100)Add(list,"SpdNum01",-47,107,32,44,0,null,-1,DigitUv(kmh/100));
        if(kmh>=10)Add(list,"SpdNum01",-10,107,32,44,0,null,-1,DigitUv(kmh/10%10));
        Add(list,"SpdNum01",27,107,32,44,0,null,-1,DigitUv(kmh%10));
        Add(list,(t.flags&2)!=0?"BaseMission_02":"BaseMission_01",-108,90,56,32);
        Add(list,"PointRmp_"+day,0,-21,20,208,Mathf.Lerp(-120,120,Mathf.Clamp01(rpm/max)));
        Add(list,"PointSpd_"+day,210,3,14,176,Mathf.Lerp(-120,120,Mathf.Clamp01(speed/240)));
        float drift=LampOpacity(t);
        if(drift>0){
            // Source green Stay colors and layer order (core1, glow3, core2).
            // The source center (-212,+54 y-up) becomes -54 in this y-down canvas.
            // Native opacity includes release fading after the active flag clears.
            Add(list,"DriftCore",-212,-54,36,36,color:new Color(0,1,.2f,drift),additive:true);
            Add(list,"DriftGlow",-212,-54,184,184,color:new Color(0,1,.2f,drift),additive:true);
            Add(list,"DriftCore",-212,-54,36,36,color:new Color(0,1,0,drift),additive:true);
        }
        if(options.hudNameplateStyle==1)Add(list,"Nameplate",0,-190,267,44);
    }
    static Vector2 Transform(Vector2 p,Vector2 pivot,float angle){
        float rad=angle*Mathf.Deg2Rad,c=Mathf.Cos(rad),s=Mathf.Sin(rad);p-=pivot;
        return pivot+new Vector2(p.x*c-p.y*s,p.x*s+p.y*c);
    }
    void Glyphs(string text,Vector2 origin,int size){
        if(font==null)font=Font.CreateDynamicFontFromOSFont("Arial",24);
        font.RequestCharactersInTexture(text,size,FontStyle.Bold);
        float total=0;foreach(char c in text)if(font.GetCharacterInfo(c,out var info,size,FontStyle.Bold))total+=info.advance;
        float x=origin.x-total*.5f;
        foreach(char c in text){
            if(!font.GetCharacterInfo(c,out var g,size,FontStyle.Bold))continue;
            // Dynamic fonts can pack rotated glyphs; store their actual corners.
            points.Add(new Vector3(x+g.minX,origin.y-g.maxY,0));points.Add(new Vector3(x+g.maxX,origin.y-g.maxY,0));
            points.Add(new Vector3(x+g.minX,origin.y-g.minY,0));points.Add(new Vector3(x+g.maxX,origin.y-g.minY,0));
            uv.Add(g.uvTopLeft);uv.Add(g.uvTopRight);uv.Add(g.uvBottomLeft);uv.Add(g.uvBottomRight);
            for(int j=0;j<4;++j)colors.Add(Color.white);
            sprites.Add(new Sprite{texture=font.material.mainTexture,fill=-1,alphaOnly=true});x+=g.advance;
        }
    }
    void Corner(float x,float y,Sprite item,Rect bounds,float scale,float shift,Vector2 sourceOrigin){
        var point=item.transformed?(Vector2)item.transform.MultiplyPoint3x4(new Vector3(x,y,0)):Transform(new Vector2(x,y),item.rect.center,item.angle);
        point-=sourceOrigin;
        points.Add(new Vector3(bounds.x+point.x*scale,bounds.y+(point.y+shift)*scale,0));colors.Add(item.color);
    }
    internal void Build(Idas3GameOptions.Values options,Telemetry data,float width,float height,float seconds,bool preview,out Rect bounds){
        imported.AudioBandsOverride=AudioBandsOverride;
        Compose(sprites,options,data,seconds,imported);DriftLampOpacity=available?LampOpacity(data):0;bounds=MeterBounds(width,height,options);
        DriftLampSpriteCount=options.hudMeterStyle>1?imported.DriftSpriteCount:DriftLampOpacity>0?3:0;
        if(DriftLampSpriteCount==0)DriftLampOpacity=0;
        if(mesh==null){mesh=new Mesh{name="Arcade meter",hideFlags=HideFlags.DontSave};mesh.MarkDynamic();}
        if(material==null){var shader=Resources.Load<Shader>("ArcadeHud");if(!shader)throw new InvalidOperationException("Arcade HUD shader is missing");material=new Material(shader){hideFlags=HideFlags.DontSave};
            additiveMaterial=new Material(shader){hideFlags=HideFlags.DontSave};additiveMaterial.SetInt("_DstBlend",(int)BlendMode.One);}
        var content=Idas3MeterLayoutBounds.Get(options.hudMeterStyle);var size=content.size;float scale=bounds.width/size.x,yShift=options.hudNameplateStyle==1?72:0;
        points.Clear();uv.Clear();colors.Clear();
        int count=sprites.Count;
        for(int i=0;i<count;i++){
            var item=sprites[i];var rect=item.rect;
            Corner(rect.x,rect.y,item,bounds,scale,yShift,content.position);Corner(rect.xMax,rect.y,item,bounds,scale,yShift,content.position);
            Corner(rect.x,rect.yMax,item,bounds,scale,yShift,content.position);Corner(rect.xMax,rect.yMax,item,bounds,scale,yShift,content.position);
            uv.Add(new Vector2(item.uv.x,item.uv.yMax));uv.Add(new Vector2(item.uv.xMax,item.uv.yMax));uv.Add(new Vector2(item.uv.x,item.uv.y));uv.Add(new Vector2(item.uv.xMax,item.uv.y));
        }
        if(options.hudNameplateStyle==1){
            if(preview)driverName="PLAYER";
            else if(seconds>=nextNameRefresh){nextNameRefresh=seconds+1;int length=Idas3SceneCopyHudDriverName(nameBytes,nameBytes.Length);driverName=length>0?Encoding.UTF8.GetString(nameBytes,0,length):"PLAYER";}
            if(driverName.Length>16)driverName=driverName.Substring(0,16);
            Glyphs(driverName,new Vector2(bounds.center.x,bounds.y+53*scale),Mathf.Max(9,Mathf.RoundToInt(28*scale)));
        }
        mesh.Clear();mesh.SetVertices(points);mesh.SetUVs(0,uv);mesh.SetColors(colors);mesh.subMeshCount=sprites.Count;
        while(indices.Count<sprites.Count){int first=indices.Count*4;indices.Add(new[]{first,first+1,first+2,first+2,first+1,first+3});properties.Add(new MaterialPropertyBlock());}
        for(int i=0;i<sprites.Count;i++)mesh.SetIndices(indices[i],MeshTopology.Triangles,i,false);
        mesh.bounds=new Bounds(Vector3.zero,Vector3.one*100000);
    }
    internal void Render(CommandBuffer commands,float width,float height){
        for(int i=0;i<sprites.Count;i++){
            var p=properties[i];p.SetTexture("_MainTex",sprites[i].texture);p.SetVector("_Canvas",new Vector4(width,height,0,0));p.SetFloat("_Fill",sprites[i].fill);p.SetFloat("_Brake",sprites[i].brake?1:0);
            p.SetFloat("_AlphaOnly",sprites[i].alphaOnly?1:0);
            SetImportedProperties(p,sprites[i]);
            commands.DrawMesh(mesh,Matrix4x4.identity,sprites[i].additive?additiveMaterial:material,i,0,p);
        }
    }
    static void SetImportedProperties(MaterialPropertyBlock p,Sprite item){
        p.SetFloat("_MaterialEffect",item.materialEffect);
        p.SetTexture("_EffectTex1",item.effectTex1?item.effectTex1:Texture2D.whiteTexture);
        p.SetTexture("_EffectTex2",item.effectTex2?item.effectTex2:Texture2D.whiteTexture);
        p.SetTexture("_EffectTex3",item.effectTex3?item.effectTex3:Texture2D.whiteTexture);
        p.SetVector("_EffectParams",item.effectParams);p.SetVector("_EffectParams2",item.effectParams2);
        p.SetColor("_EffectColor1",item.effectColor1);p.SetColor("_EffectColor2",item.effectColor2);p.SetColor("_EffectColor3",item.effectColor3);
        p.SetVector("_SampleMotion",item.sampleMotion);
        p.SetTexture("_MaskTex",item.mask?item.mask:Texture2D.whiteTexture);p.SetFloat("_MaskEnabled",item.mask?1:0);p.SetVector("_Radial",item.radial);
        p.SetVector("_MaskTransform0",new Vector4(item.maskTransform.m00,item.maskTransform.m01,item.maskTransform.m03,0));
        p.SetVector("_MaskTransform1",new Vector4(item.maskTransform.m10,item.maskTransform.m11,item.maskTransform.m13,0));
        p.SetFloat("_GaugeMode",item.gaugeMode);p.SetVector("_Gauge",item.gauge);p.SetFloat("_ClipEnabled",item.clipped?1:0);
        p.SetVector("_ClipRect",new Vector4(item.clip.xMin,item.clip.yMin,item.clip.xMax,item.clip.yMax));
        p.SetVector("_Atlas",new Vector4(item.uv.x,item.uv.y,item.uv.width,item.uv.height));
        p.SetVector("_SourceSize",new Vector4(item.rect.width,item.rect.height,0,0));
        p.SetVector("_LocalToMeter0",new Vector4(item.transform.m00,item.transform.m01,item.transform.m03,0));
        p.SetVector("_LocalToMeter1",new Vector4(item.transform.m10,item.transform.m11,item.transform.m13,0));
    }
    // Same authored layers and sample telemetry as the full-screen layout demo.
    internal static int PreviewSpriteCount=>previewSprites.Count;
    internal static bool PreparePreview(Idas3GameOptions.Values options,float seconds){
        if(options==null||options.hudMeterStyle==0||!Available){ReleasePreview();return false;}
        Compose(previewSprites,options,Demo(seconds),seconds,previewImported);return true;
    }
    public static void ReleasePreview(){
        previewImported.Dispose();previewSprites.Clear();
        DestroyOwned(radialPreview);DestroyOwned(additivePreview);radialPreview=null;additivePreview=null;
    }
    public static void DrawPreview(Rect area,Idas3GameOptions.Values options,float seconds){
        if(Event.current.type!=EventType.Repaint)return;
        if(previewLabel==null){previewLabel=new GUIStyle(GUI.skin.label){alignment=TextAnchor.MiddleCenter,fontSize=17,normal={textColor=Color.white}};previewName=new GUIStyle(previewLabel){fontStyle=FontStyle.Bold};}
        bool prepared=PreparePreview(options,seconds);
        if(options.hudMeterStyle==0){GUI.Label(area,"ORIGINAL ARCADE STAGE 3 HUD",previewLabel);return;}
        if(!prepared){GUI.Label(area,"Meter artwork unavailable",previewLabel);return;}
        var content=Idas3MeterLayoutBounds.Get(options.hudMeterStyle);var size=content.size;float h=size.y+(options.hudNameplateStyle==1?72:0),scale=Mathf.Min(area.width/size.x,area.height/h),shift=options.hudNameplateStyle==1?72:0;
        Vector2 origin=new Vector2(area.center.x-size.x*scale*.5f,area.center.y-h*scale*.5f);
        var matrix=GUI.matrix;var color=GUI.color;
        foreach(var item in previewSprites){
            var rect=new Rect(origin.x+(item.rect.x-content.x)*scale,origin.y+(item.rect.y-content.y+shift)*scale,item.rect.width*scale,item.rect.height*scale);
            if(item.transformed){
                GUI.matrix=matrix*Matrix4x4.Translate(new Vector3(origin.x,origin.y+shift*scale,0))*Matrix4x4.Scale(new Vector3(scale,scale,1))*Matrix4x4.Translate(new Vector3(-content.x,-content.y,0))*item.transform;
                GUI.color=Color.white;DrawMaterialPreview(item.rect,item);GUI.matrix=matrix;continue;
            }
            GUI.color=item.color;
            // Rotate in panel coordinates before applying the enclosing menu
            // scale/translation. GUIUtility's screen pivot displaces needles.
            var pivot=new Vector3(rect.center.x,rect.center.y,0);
            GUI.matrix=matrix*Matrix4x4.Translate(pivot)*Matrix4x4.Rotate(Quaternion.Euler(0,0,item.angle))*Matrix4x4.Translate(-pivot);
            // Preview uses the same material/UV path through a GUI-compatible
            // radial material so pedal sweeps match the in-race shader.
            if(item.fill>=0||item.additive){GUI.color=Color.white;DrawMaterialPreview(rect,item);}
            else GUI.DrawTextureWithTexCoords(rect,item.texture,item.uv,true);
            GUI.matrix=matrix;
        }
        GUI.color=color;
        if(options.hudNameplateStyle==1){previewName.fontSize=Mathf.Max(9,Mathf.RoundToInt(28*scale));GUI.Label(new Rect(origin.x+(size.x*.5f-133.5f)*scale,origin.y+22*scale,267*scale,44*scale),"PLAYER",previewName);}
    }
    static Material radialPreview,additivePreview;
    static void DrawMaterialPreview(Rect area,Sprite item){
        if(radialPreview==null){var shader=Resources.Load<Shader>("ArcadeHudPreview");if(shader){radialPreview=new Material(shader){hideFlags=HideFlags.DontSave};
            additivePreview=new Material(shader){hideFlags=HideFlags.DontSave};additivePreview.SetInt("_DstBlend",(int)BlendMode.One);}}
        var previewMaterial=item.additive?additivePreview:radialPreview;
        if(!previewMaterial)return;previewMaterial.SetFloat("_Fill",item.fill);previewMaterial.SetFloat("_Brake",item.brake?1:0);
        previewMaterial.SetFloat("_MaterialEffect",item.materialEffect);
        previewMaterial.SetTexture("_EffectTex1",item.effectTex1?item.effectTex1:Texture2D.whiteTexture);
        previewMaterial.SetTexture("_EffectTex2",item.effectTex2?item.effectTex2:Texture2D.whiteTexture);
        previewMaterial.SetTexture("_EffectTex3",item.effectTex3?item.effectTex3:Texture2D.whiteTexture);
        previewMaterial.SetVector("_EffectParams",item.effectParams);previewMaterial.SetVector("_EffectParams2",item.effectParams2);
        previewMaterial.SetColor("_EffectColor1",item.effectColor1);previewMaterial.SetColor("_EffectColor2",item.effectColor2);previewMaterial.SetColor("_EffectColor3",item.effectColor3);
        previewMaterial.SetVector("_SampleMotion",item.sampleMotion);
        previewMaterial.SetTexture("_MaskTex",item.mask?item.mask:Texture2D.whiteTexture);previewMaterial.SetFloat("_MaskEnabled",item.mask?1:0);previewMaterial.SetVector("_Radial",item.radial);
        previewMaterial.SetVector("_MaskTransform0",new Vector4(item.maskTransform.m00,item.maskTransform.m01,item.maskTransform.m03,0));
        previewMaterial.SetVector("_MaskTransform1",new Vector4(item.maskTransform.m10,item.maskTransform.m11,item.maskTransform.m13,0));
        previewMaterial.SetFloat("_GaugeMode",item.gaugeMode);previewMaterial.SetVector("_Gauge",item.gauge);previewMaterial.SetFloat("_ClipEnabled",item.clipped?1:0);
        previewMaterial.SetVector("_ClipRect",new Vector4(item.clip.xMin,item.clip.yMin,item.clip.xMax,item.clip.yMax));
        previewMaterial.SetVector("_Atlas",new Vector4(item.uv.x,item.uv.y,item.uv.width,item.uv.height));previewMaterial.SetVector("_SourceSize",new Vector4(item.rect.width,item.rect.height,0,0));
        previewMaterial.SetVector("_LocalToMeter0",new Vector4(item.transform.m00,item.transform.m01,item.transform.m03,0));
        previewMaterial.SetVector("_LocalToMeter1",new Vector4(item.transform.m10,item.transform.m11,item.transform.m13,0));
        Graphics.DrawTexture(area,item.texture,item.uv,0,0,0,0,item.color,previewMaterial);
    }
    static void DestroyOwned(UnityEngine.Object value){if(!value)return;if(Application.isPlaying)UnityEngine.Object.Destroy(value);else UnityEngine.Object.DestroyImmediate(value);}
    public void Dispose(){imported.Dispose();DestroyOwned(mesh);DestroyOwned(material);DestroyOwned(additiveMaterial);mesh=null;material=null;additiveMaterial=null;}
}
