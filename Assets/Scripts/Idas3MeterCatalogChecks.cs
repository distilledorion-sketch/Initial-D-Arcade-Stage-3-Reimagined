using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;
using UnityEngine.Rendering;

// Exercises the production GPU renderer with deterministic telemetry. Pixel
// checks establish visibility and response, not equivalence to source shaders.
public static class Idas3MeterCatalogChecks
{
    const int Width=1280,Height=720;
    [Serializable] sealed class Report {
        public bool passed,sharedCacheRetainedWhileUsed,sharedCacheReleased,previewLifecycleReleased,liveOriginalReleased,emptyFrameIsBlack;
        public int checks,catalogChoices,meters,textureReferences,gpuCases,channelChecks,layoutTranslationChecks,sampledLayoutStyles,alphaTextureBounds,missingAlphaBounds,initialResidentTextures,finalResidentTextures;
        public string graphicsDevice,colorSpace;
        public string coverage="Production camera/command-buffer output at 1280x720 on verified black. Low/high pairs use the same animation time. Probes cover AT/MT, day/night, 8000/9000/10000/13000 tach scales and two animation times. Sources 31/0/3/8/39/43/67/76 isolate RPM, accelerator, brake, gear and speed ones-digit changes and test disabled-pedal invariance. Sources 31/0 test translated Build(preview=true) output. Actual IMGUI DrawPreview positioning and unrecovered source shader equivalence are not established by these checks.";
        public List<MeterResult> results=new List<MeterResult>();
        public List<string> errors=new List<string>();
    }
    [Serializable] sealed class MeterResult {
        public int style,sourceId,layers,enabledLayers,zeroAreaLayers,disabledLayers,variantTextures,curveChannels,peakResidentTextures;
        public string name,lowImage,highImage,thumbnail,error;
        public string[] limitations;
        public Rect bounds;
        public List<ProbeResult> probes=new List<ProbeResult>();
    }
    [Serializable] sealed class ProbeResult {
        public string name,diagnosticImage;
        public uint flags;
        public float revLimit,seconds,rpm,speedKmh,throttle,brake,driftOpacity;
        public int gear,sprites,visiblePixels,frameEdgePixels,outsideNominalBounds,changedPixels,maxChannelChange;
        public Rect visibleBounds;
    }
    sealed class Frame {
        public Color32[] pixels;
        public ProbeResult result;
    }
    sealed class CheckPlatform : Idas3GameOptions.IPlatform {
        public int Width=>1280;public int Height=>720;public int DisplayMode=>0;public double Now=>0;
        public Idas3GameOptions.ResolutionChoice[] Resolutions=>Array.Empty<Idas3GameOptions.ResolutionChoice>();
        public void Apply(Idas3GameOptions.Values previous,Idas3GameOptions.Values next,bool displayChanged){}
    }
    sealed class Runner : IDisposable {
        internal readonly Report report=new Report();
        readonly string output;
        readonly HashSet<string> checkedTextures=new HashSet<string>();
        readonly HashSet<Texture2D> priorTextureObjects=new HashSet<Texture2D>();
        readonly GameObject host;
        readonly Camera camera;
        readonly RenderTexture target;
        readonly Texture2D readback;
        readonly CommandBuffer commands;
        internal Runner(string output){
            this.output=output;
            report.graphicsDevice=SystemInfo.graphicsDeviceName;report.colorSpace=QualitySettings.activeColorSpace.ToString();
            report.initialResidentTextures=Idas3ImportedMeter.ResidentTextureCount;
            foreach(var texture in Resources.FindObjectsOfTypeAll<Texture2D>())priorTextureObjects.Add(texture);
            host=new GameObject("Meter catalog GPU checks"){hideFlags=HideFlags.HideAndDontSave};
            camera=host.AddComponent<Camera>();camera.enabled=false;camera.cullingMask=0;camera.allowHDR=false;camera.allowMSAA=false;
            camera.clearFlags=CameraClearFlags.SolidColor;camera.backgroundColor=Color.black;
            target=new RenderTexture(Width,Height,24,RenderTextureFormat.ARGB32){name="Meter catalog QA",hideFlags=HideFlags.HideAndDontSave,antiAliasing=1};
            Require(target.Create(),"Could not create the meter QA render target");camera.targetTexture=target;
            readback=new Texture2D(Width,Height,TextureFormat.RGB24,false){hideFlags=HideFlags.HideAndDontSave};
            commands=new CommandBuffer{name="Meter catalog GPU checks"};
            camera.AddCommandBuffer(CameraEvent.BeforeForwardOpaque,commands);
        }
        void Require(bool condition,string message){++report.checks;if(!condition)throw new InvalidOperationException(message);}
        void Failure(MeterResult result,string message){
            result.error=string.IsNullOrEmpty(result.error)?message:result.error+"\n"+message;
            report.errors.Add(result.sourceId+" "+result.name+": "+message);Debug.LogError("Meter catalog QA: "+report.errors[report.errors.Count-1]);
        }
        void Verify(bool condition,MeterResult owner,ProbeResult probe,string message){
            ++report.checks;if(condition)return;
            if(string.IsNullOrEmpty(probe.diagnosticImage)){probe.diagnosticImage="meter-"+owner.sourceId.ToString("00")+"-"+probe.name+"-failure.png";Save(probe.diagnosticImage);}
            Failure(owner,message);
        }
        static bool Finite(float value)=>!float.IsNaN(value)&&!float.IsInfinity(value);
        void Numbers(float[] values,string label,int length=0){
            if(values==null||values.Length==0)return;
            if(length>0)Require(values.Length==length,label+" has an invalid array length");
            foreach(float value in values)Require(Finite(value),label+" contains a nonfinite value");
        }
        void Texture(string path,string label,bool required=false){
            if(string.IsNullOrEmpty(path)){Require(!required,label+" has no texture");return;}
            if(!checkedTextures.Add(path))return;
            var texture=Resources.Load<Texture2D>(path);
            Require(texture!=null,label+" texture is missing: "+path);
            try{Require(texture.width>0&&texture.height>0,label+" texture has no pixels: "+path);++report.textureReferences;}
            finally{if(!priorTextureObjects.Contains(texture))Resources.UnloadAsset(texture);}
        }
        void Metadata(Idas3ArcadeMeterCatalog.Meter meter,MeterResult result){
            Require(meter!=null,"Missing source metadata for style "+result.style);
            Require(meter.id==result.sourceId,"Style/source ID mismatch");
            Require(Finite(meter.width)&&Finite(meter.height)&&meter.width>0&&meter.height>0,"Invalid meter canvas");
            Require(meter.layers!=null&&meter.layers.Length>0,"Meter has no source layers");
            result.layers=meter.layers.Length;result.limitations=meter.limitations;
            var roles=new HashSet<string>();
            foreach(var layer in meter.layers){
                Require(layer!=null,"Null source layer");string label=result.sourceId+" / "+layer.name;
                bool disabled=layer.role=="disabled"||!string.IsNullOrEmpty(layer.disabledReason);
                if(disabled){++result.disabledLayers;continue;}
                ++result.enabledLayers;roles.Add(layer.role??"static");
                foreach(float value in new[]{layer.width,layer.height,layer.x,layer.y,layer.pivotX,layer.pivotY,layer.angle,layer.angleMin,layer.angleMax,layer.opacity})Require(Finite(value),label+" has nonfinite geometry or opacity");
                Require(layer.width>=0&&layer.height>=0,label+" has negative dimensions");
                if(layer.width==0||layer.height==0)++result.zeroAreaLayers;
                if(layer.role=="gear"||layer.role=="rpm"||layer.role=="speed"||(layer.role??"").StartsWith("speed",StringComparison.Ordinal)||(layer.role??"").StartsWith("rpm",StringComparison.Ordinal))
                    Require(layer.width>0&&layer.height>0,label+" is a zero-area core meter layer");
                Numbers(layer.transform,label+" transform",6);Numbers(layer.color,label+" color",4);Numbers(layer.brushColor,label+" brush color",4);Numbers(layer.uv,label+" UV",4);Numbers(layer.clipRect,label+" clip",4);
                Require(layer.atlasCols>0&&layer.atlasRows>0,label+" has an invalid atlas");
                Texture(layer.texture,label,true);Texture(layer.nightTexture,label);
                if(layer.textureBindings!=null)foreach(var binding in layer.textureBindings){Require(binding!=null,label+" has a null texture binding");Texture(binding.texture,label+" material binding");}
                if(layer.textureVariants!=null)foreach(var variant in layer.textureVariants){
                    Require(variant!=null,label+" has a null texture variant");Texture(variant.texture,label+" variant",true);++result.variantTextures;
                    Require(variant.maxRpm==0||variant.maxRpm==8000||variant.maxRpm==9000||variant.maxRpm==10000||variant.maxRpm==13000,label+" has an unsupported tach scale");
                }
                if(layer.parameters!=null)foreach(var parameter in layer.parameters)Require(parameter!=null&&Finite(parameter.value),label+" has an invalid material scalar");
                if(layer.vectorParameters!=null)foreach(var parameter in layer.vectorParameters){Require(parameter!=null,label+" has a null material vector");Numbers(parameter.values,label+" material vector");}
                if(layer.curves!=null)foreach(var curve in layer.curves){
                    Require(curve!=null&&Finite(curve.defaultValue),label+" has an invalid curve");++result.curveChannels;
                    Numbers(curve.times,label+" curve times");Numbers(curve.values,label+" curve values");
                    int times=curve.times?.Length??0,values=curve.values?.Length??0;
                    Require(times==values||times==0,label+" has unmatched animation keys");
                    for(int i=1;i<times;++i)Require(curve.times[i]>=curve.times[i-1],label+" has out-of-order animation keys");
                    if(curve.owner!=null){Numbers(curve.owner.transform,label+" animation owner",6);Require(Finite(curve.owner.width)&&Finite(curve.owner.height)&&Finite(curve.owner.pivotX)&&Finite(curve.owner.pivotY),label+" has an invalid animation owner");}
                }
            }
            Require(roles.Contains("speed1")&&roles.Contains("speed10")&&roles.Contains("speed100"),"Meter is missing functional speed digits");
        }
        internal void Run(){
            Idas3MeterLayoutBounds.RecalculateForVerification();
            Require(SystemInfo.graphicsDeviceType!=GraphicsDeviceType.Null,"GPU checks require a graphics device; omit -nographics");
            Require(Marshal.SizeOf<Idas3ArcadeHud.Telemetry>()==40,"HUD telemetry ABI mismatch");
            Require(Idas3ArcadeHud.Available,"Stuttgart/shared meter artwork is incomplete");
            commands.Clear();camera.Render();
            bool black=true;foreach(var pixel in ReadPixels())if(pixel.r!=0||pixel.g!=0||pixel.b!=0){black=false;break;}
            Require(black,"An empty QA frame is not black");
            report.emptyFrameIsBlack=true;
            report.alphaTextureBounds=Idas3MeterLayoutBounds.AlphaTextureCount;
            Require(report.alphaTextureBounds>0,"Offline texture alpha bounds are missing");
            report.catalogChoices=Idas3ArcadeMeterCatalog.Count;
            Require(report.catalogChoices==88,"Catalog must contain Original and 87 imported meters");
            Require(Idas3ArcadeMeterCatalog.StyleAt(0)==0&&Idas3ArcadeMeterCatalog.StyleAt(1)==1,"Original/Stuttgart IDs changed");
            var ids=new HashSet<int>();var styles=new HashSet<int>();
            for(int index=1;index<report.catalogChoices;++index){
                int style=Idas3ArcadeMeterCatalog.StyleAt(index),source=Idas3ArcadeMeterCatalog.SourceId(style);
                var result=new MeterResult{style=style,sourceId=source,name=Idas3ArcadeMeterCatalog.Name(style)};report.results.Add(result);
                try{
                    Require(styles.Add(style)&&ids.Add(source),"Duplicate catalog style or source ID");
                    Require(Idas3ArcadeMeterCatalog.IndexOfStyle(style)==index&&Idas3ArcadeMeterCatalog.IsValidStyle(style),"Catalog lookup failed");
                    Require((source==31&&style==1)||(source!=31&&style==source+2),"Catalog uses an unstable saved ID");
                    Metadata(Idas3ArcadeMeterCatalog.Get(style),result);
                    RenderMeter(result);++report.meters;
                }catch(Exception error){Failure(result,error.ToString());}
            }
            Require(report.results.Count==87,"Catalog did not enumerate all meters");
            SharedCache();
            Lifecycle();
            report.sampledLayoutStyles=Idas3MeterLayoutBounds.SampledStyleCount;
            Require(report.sampledLayoutStyles==report.catalogChoices-2,"QA reused baked bounds instead of sampling every imported style");
            report.finalResidentTextures=Idas3ImportedMeter.ResidentTextureCount;
            Require(report.finalResidentTextures==report.initialResidentTextures,"Imported texture pool grew after all meters were disposed");
            report.missingAlphaBounds=Idas3MeterLayoutBounds.MissingAlphaBoundsCount;
            Require(report.missingAlphaBounds==0,"Some rendered textures lack offline alpha bounds");
            report.passed=report.errors.Count==0;
        }
        void Lifecycle(){
            int before=Idas3ImportedMeter.ResidentTextureCount;
            var go=new GameObject("Meter lifecycle checks"){hideFlags=HideFlags.HideAndDontSave};
            var live=new Idas3ArcadeHud();
            try{
                var selected=new Idas3GameOptions.Values{hudMeterStyle=3};
                var telemetry=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,gear=3,rpm=5000,speedKmh=100,revLimit=8500};
                live.Build(selected,telemetry,Width,Height,.125f,true,out _);
                int liveTextures=Idas3ImportedMeter.ResidentTextureCount;
                Require(liveTextures>before,"Live lifecycle fixture did not acquire imported textures");
                string settings=Path.Combine(output,"lifecycle-settings");Directory.CreateDirectory(settings);
                var options=new Idas3GameOptions(new CheckPlatform());options.Initialize(settings);options.Draft.hudMeterStyle=51;Require(options.ApplyDraft(),"Lifecycle fixture settings did not apply");
                var menu=go.AddComponent<Idas3PauseMenu>();menu.Initialize(options);
                menu.SetOpen(true);menu.SelectTab(7);menu.Activate();var customization=menu.HudCustomization;
                Require(customization!=null&&Idas3ArcadeHud.PreparePreview(customization.Draft,.125f),"Preview lifecycle fixture did not compose");
                Require(Idas3ArcadeHud.PreviewSpriteCount>0&&Idas3ImportedMeter.ResidentTextureCount>liveTextures,"Preview fixture did not acquire distinct resources");
                customization.Close(false);
                Require(Idas3ArcadeHud.PreviewSpriteCount==0&&Idas3ImportedMeter.ResidentTextureCount==liveTextures,"Cancelling customization retained preview textures or released live textures");
                menu.Activate();Idas3ArcadeHud.PreparePreview(customization.Draft,.125f);
                Require(!Idas3ArcadeHud.PreparePreview(new Idas3GameOptions.Values(),.125f)&&Idas3ArcadeHud.PreviewSpriteCount==0&&Idas3ImportedMeter.ResidentTextureCount==liveTextures,"Selecting Original retained imported preview resources");
                Idas3ArcadeHud.PreparePreview(customization.Draft,.125f);customization.Close(true);
                Require(!customization.IsOpen&&options.Current.hudMeterStyle==51&&Idas3ArcadeHud.PreviewSpriteCount==0&&Idas3ImportedMeter.ResidentTextureCount==liveTextures,"Apply did not release only the preview's resources");
                menu.Activate();Idas3ArcadeHud.PreparePreview(customization.Draft,.125f);menu.SetOpen(false);
                // Edit mode does not dispatch this non-ExecuteAlways component's
                // OnDestroy. Its real callback is checked by the player smoke.
                Require(!customization.IsOpen&&Idas3ArcadeHud.PreviewSpriteCount==0&&Idas3ImportedMeter.ResidentTextureCount==liveTextures,"Closing the owning pause menu retained imported preview resources");
                report.previewLifecycleReleased=true;
                var ui=go.AddComponent<Idas3UnityUi>();
                var field=typeof(Idas3UnityUi).GetField("arcadeHud",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);field.SetValue(ui,live);
                ui.ReleaseOriginalMeter(selected);ui.ReleaseOriginalMeter(null);
                Require(ReferenceEquals(field.GetValue(ui),live)&&Idas3ImportedMeter.ResidentTextureCount==liveTextures,"Temporarily hidden custom HUD released its live renderer");
                ui.ReleaseOriginalMeter(new Idas3GameOptions.Values());
                Require(field.GetValue(ui)==null&&Idas3ImportedMeter.ResidentTextureCount==before,"Restoring Original retained imported live textures");
                report.liveOriginalReleased=true;
            }finally{Idas3ArcadeHud.ReleasePreview();live.Dispose();Destroy(go);}
        }
        void SharedCache(){
            int before=Idas3ImportedMeter.ResidentTextureCount;
            var first=new Idas3ArcadeHud();var second=new Idas3ArcadeHud();
            try{
                var options=new Idas3GameOptions.Values{hudMeterStyle=Idas3ArcadeMeterCatalog.StyleAt(2)};
                var telemetry=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,gear=3,rpm=5000,speedKmh=100,revLimit=8500};
                first.Build(options,telemetry,Width,Height,.125f,true,out _);
                int acquired=Idas3ImportedMeter.ResidentTextureCount;
                var record=new MeterResult();
                var baseline=Capture(second,options,telemetry,.125f,"shared-cache-before",null,record);
                Require(Idas3ImportedMeter.ResidentTextureCount==acquired,"Two users loaded duplicate imported textures");
                first.Dispose();
                Require(Idas3ImportedMeter.ResidentTextureCount==acquired,"Disposing one meter evicted textures still used by another");
                var retained=Capture(second,options,telemetry,.125f,"shared-cache-after",baseline.pixels,record);
                Require(retained.result.changedPixels==0&&retained.result.maxChannelChange==0,"Disposing another meter changed the remaining owner's output");
                report.sharedCacheRetainedWhileUsed=true;
            }finally{commands.Clear();first.Dispose();second.Dispose();}
            Require(Idas3ImportedMeter.ResidentTextureCount==before,"Shared meter textures stayed resident after both owners closed");
            report.sharedCacheReleased=true;
        }
        void RenderMeter(MeterResult result){
            int resident=Idas3ImportedMeter.ResidentTextureCount;
            var meter=new Idas3ArcadeHud();
            try{
                var options=new Idas3GameOptions.Values{hudMeterStyle=result.style,hudShiftLights=true,hudPedalIndicators=true,hudNameplateStyle=0};
                var low=new Idas3ArcadeHud.Telemetry{size=40,version=2,flags=1,gear=1,rpm=1000,speedKmh=15,revLimit=8500};
                var high=low;high.gear=5;high.rpm=7000;high.speedKmh=160;high.throttle=1;high.brake=1;
                string stem="meter-"+result.sourceId.ToString("00");
                var a=Capture(meter,options,low,.125f,"low",null,result);result.lowImage=stem+"-low.png";Save(result.lowImage);
                var b=Capture(meter,options,high,.125f,"high",a.pixels,result);result.highImage=stem+"-high.png";Save(result.highImage);
                result.thumbnail=stem+"-crop.png";Crop(b,result.thumbnail);
                Verify(b.result.changedPixels>16&&b.result.maxChannelChange>16,result,b.result,"Low/high telemetry produced no meaningful pixel response");
                foreach(uint flags in new uint[]{3,5,7}){var probe=high;probe.flags=flags;Capture(meter,options,probe,.125f,flags==3?"day-auto":flags==5?"night-manual":"night-auto",b.pixels,result);}
                foreach(float limit in new[]{8000f,10000f,13000f}){var probe=high;probe.revLimit=limit;Capture(meter,options,probe,.125f,"tach-"+limit.ToString("0",System.Globalization.CultureInfo.InvariantCulture),b.pixels,result);}
                Capture(meter,options,high,.575f,"animation-time",b.pixels,result);
                var lamps=high;lamps.rpm=8750;lamps.driftOpacity=1;lamps.flags|=8;
                Capture(meter,options,lamps,.125f,"rev-and-drift",b.pixels,result);
                if(result.sourceId==31||result.sourceId==0||result.sourceId==3||result.sourceId==8||result.sourceId==39||result.sourceId==43||result.sourceId==67||result.sourceId==76)
                    Channels(meter,options,low,a.pixels,result);
                if(result.sourceId==31||result.sourceId==0)Translation(meter,options,low,a,result);
            }finally{
                commands.Clear();meter.Dispose();
                Require(Idas3ImportedMeter.ResidentTextureCount==resident,"Closing this meter retained imported textures");
            }
        }
        void Channels(Idas3ArcadeHud meter,Idas3GameOptions.Values options,Idas3ArcadeHud.Telemetry baseline,Color32[] pixels,MeterResult owner){
            foreach(string channel in new[]{"rpm","accelerator","brake","gear","speed-ones"}){
                var telemetry=baseline;
                switch(channel){case "rpm":telemetry.rpm=7000;break;case "accelerator":telemetry.throttle=1;break;case "brake":telemetry.brake=1;break;case "gear":telemetry.gear=5;break;case "speed-ones":telemetry.speedKmh=16;break;}
                var frame=Capture(meter,options,telemetry,.125f,"channel-"+channel,pixels,owner);
                Verify(frame.result.changedPixels>8&&frame.result.maxChannelChange>16,owner,frame.result,"Isolated "+channel+" input produced no meaningful pixel response");++report.channelChecks;
            }
            var disabled=options.Clone();disabled.hudPedalIndicators=false;
            var off=Capture(meter,disabled,baseline,.125f,"pedals-disabled-low",null,owner);
            var both=baseline;both.throttle=1;both.brake=1;
            var on=Capture(meter,disabled,both,.125f,"pedals-disabled-high",off.pixels,owner);
            Verify(on.result.changedPixels==0&&on.result.maxChannelChange==0,owner,on.result,"Pedal inputs changed pixels while pedal indicators were disabled");++report.channelChecks;
        }
        void Translation(Idas3ArcadeHud meter,Idas3GameOptions.Values options,Idas3ArcadeHud.Telemetry telemetry,Frame baseline,MeterResult owner){
            const int dx=-320,dy=-180;
            var shifted=options.Clone();shifted.SetHudOffset(2,new Vector2(dx/(float)Width,dy/(float)Height));
            Rect nominal=owner.bounds;
            var moved=Capture(meter,shifted,telemetry,.125f,"preview-build-translated",null,owner);
            owner.bounds=nominal;
            Rect before=baseline.result.visibleBounds,after=moved.result.visibleBounds;
            Verify(Mathf.Abs(after.x-before.x-dx)<=1&&Mathf.Abs(after.y-before.y-dy)<=1&&Mathf.Abs(after.width-before.width)<=1&&Mathf.Abs(after.height-before.height)<=1,
                owner,moved.result,"Build(preview=true) did not follow the saved layout offset");
            int mismatches=0;
            for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
                int originalX=x-dx,originalY=y-dy;var actual=moved.pixels[(Height-1-y)*Width+x];
                Color32 expected=originalX>=0&&originalX<Width&&originalY>=0&&originalY<Height?baseline.pixels[(Height-1-originalY)*Width+originalX]:new Color32(0,0,0,255);
                if(Math.Max(Math.Abs(actual.r-expected.r),Math.Max(Math.Abs(actual.g-expected.g),Math.Abs(actual.b-expected.b)))>4)++mismatches;
            }
            Verify(mismatches<=32,owner,moved.result,"Translating the preview build changed more than 32 meter pixels after alignment ("+mismatches+")");++report.layoutTranslationChecks;
        }
        Color32[] ReadPixels(){
            var previous=RenderTexture.active;
            try{RenderTexture.active=target;readback.ReadPixels(new Rect(0,0,Width,Height),0,0);readback.Apply(false);}
            finally{RenderTexture.active=previous;}
            return readback.GetPixels32();
        }
        Frame Capture(Idas3ArcadeHud meter,Idas3GameOptions.Values options,Idas3ArcadeHud.Telemetry telemetry,float seconds,string name,Color32[] comparison,MeterResult owner){
            commands.Clear();meter.Build(options,telemetry,Width,Height,seconds,true,out var bounds);meter.Render(commands,Width,Height);camera.Render();
            owner.bounds=bounds;owner.peakResidentTextures=Math.Max(owner.peakResidentTextures,Idas3ImportedMeter.ResidentTextureCount);
            Require(Finite(bounds.x)&&Finite(bounds.y)&&Finite(bounds.width)&&Finite(bounds.height)&&bounds.width>0&&bounds.height>0,"Invalid runtime meter bounds");
            Require(bounds.xMin>=0&&bounds.yMin>=0&&bounds.xMax<=Width&&bounds.yMax<=Height,"Default meter bounds leave the viewport");
            Require(meter.SpriteCount>0,"Meter produced no renderable sprites");
            var pixels=ReadPixels();var result=new ProbeResult{name=name,flags=telemetry.flags,revLimit=telemetry.revLimit,seconds=seconds,sprites=meter.SpriteCount,
                rpm=telemetry.rpm,speedKmh=telemetry.speedKmh,gear=telemetry.gear,throttle=telemetry.throttle,brake=telemetry.brake,driftOpacity=telemetry.driftOpacity};
            int xMin=Width,yMin=Height,xMax=-1,yMax=-1;
            for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
                int index=(Height-1-y)*Width+x;var pixel=pixels[index];
                if(Math.Max(pixel.r,Math.Max(pixel.g,pixel.b))>8){
                    ++result.visiblePixels;xMin=Math.Min(xMin,x);xMax=Math.Max(xMax,x);yMin=Math.Min(yMin,y);yMax=Math.Max(yMax,y);
                    if(x==0||y==0||x==Width-1||y==Height-1)++result.frameEdgePixels;
                    if(!bounds.Contains(new Vector2(x+.5f,y+.5f)))++result.outsideNominalBounds;
                }
                if(comparison!=null){var other=comparison[index];int change=Math.Max(Math.Abs(pixel.r-other.r),Math.Max(Math.Abs(pixel.g-other.g),Math.Abs(pixel.b-other.b)));
                    result.maxChannelChange=Math.Max(result.maxChannelChange,change);if(change>8)++result.changedPixels;}
            }
            if(xMax>=xMin)result.visibleBounds=new Rect(xMin,yMin,xMax-xMin+1,yMax-yMin+1);
            owner.probes.Add(result);++report.gpuCases;
            Verify(result.visiblePixels>128,owner,result,name+" is blank or contains too few visible meter pixels");
            Verify(result.frameEdgePixels==0,owner,result,name+" reaches the framebuffer edge; meter artwork may be clipped");
            Verify(result.visibleBounds.Overlaps(bounds),owner,result,name+" drew outside the expected meter position");
            return new Frame{pixels=pixels,result=result};
        }
        void Save(string file)=>File.WriteAllBytes(Path.Combine(output,file),readback.EncodeToPNG());
        void Crop(Frame frame,string file){
            Rect bounds=frame.result.visibleBounds;int width=(int)bounds.width,height=(int)bounds.height;
            if(width<=0||height<=0)return;
            var image=new Texture2D(width,height,TextureFormat.RGB24,false){hideFlags=HideFlags.HideAndDontSave};
            try{
                var pixels=new Color32[width*height];int left=(int)bounds.x,lower=Height-(int)bounds.y-height;
                for(int y=0;y<height;++y)Array.Copy(frame.pixels,(lower+y)*Width+left,pixels,y*width,width);
                image.SetPixels32(pixels);image.Apply(false);File.WriteAllBytes(Path.Combine(output,file),image.EncodeToPNG());
            }finally{Destroy(image);}
        }
        internal void Write(){
            File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(report,true));
            var page=new StringBuilder("<!doctype html><meta charset=\"utf-8\"><title>Meter render QA</title><style>body{background:#17191e;color:#fff;font:16px system-ui;margin:24px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(290px,1fr));gap:12px}article{background:#090a0c;padding:12px}img{width:100%;height:180px;object-fit:contain}p{color:#bac0cc;font-size:12px}a{color:inherit}</style><h1>Meter render QA</h1><p>Production GPU output with synthetic telemetry. Source shader equivalence is not established. See report.json for probes and source limitations.</p><main>");
            foreach(var item in report.results){page.Append("<article><strong>").Append(item.sourceId.ToString("00")).Append(" · ").Append(Html(item.name)).Append("</strong>");
                if(!string.IsNullOrEmpty(item.thumbnail))page.Append("<a href=\"").Append(item.highImage).Append("\"><img src=\"").Append(item.thumbnail).Append("\"></a>");
                page.Append("<p>").Append(string.IsNullOrEmpty(item.error)?item.probes.Count+" GPU cases; "+item.disabledLayers+" disabled source layers":Html(item.error)).Append("</p></article>");}
            File.WriteAllText(Path.Combine(output,"index.html"),page.Append("</main>").ToString());
        }
        public void Dispose(){camera.RemoveAllCommandBuffers();camera.targetTexture=null;commands.Dispose();target.Release();Destroy(readback);Destroy(target);Destroy(host);}
    }
    static string Html(string value)=>(value??"").Replace("&","&amp;").Replace("<","&lt;").Replace(">","&gt;").Replace("\"","&quot;");
    static void Destroy(UnityEngine.Object value){if(!value)return;if(Application.isPlaying)UnityEngine.Object.Destroy(value);else UnityEngine.Object.DestroyImmediate(value);}
    public static string Run(string outputDirectory=null){
        string output=Path.GetFullPath(outputDirectory??Path.Combine("Verification/meter-import-20260924","render-qa-"+DateTime.Now.ToString("yyyyMMdd-HHmmss")+"-"+Guid.NewGuid().ToString("N").Substring(0,8)));
        if(Directory.Exists(output)&&Directory.GetFileSystemEntries(output).Length>0)throw new IOException("Meter QA output must be fresh: "+output);
        Directory.CreateDirectory(output);
        using(var runner=new Runner(output)){
            try{runner.Run();}
            catch(Exception error){runner.report.passed=false;runner.report.errors.Add(error.ToString());}
            finally{runner.Write();}
            if(!runner.report.passed)throw new InvalidOperationException("Meter catalog QA failed; see "+Path.Combine(output,"report.json"));
            Debug.Log("Meter catalog QA passed: "+runner.report.meters+" meters, "+runner.report.gpuCases+" GPU cases, "+runner.report.checks+" checks. "+output);
        }
        return output;
    }
}
