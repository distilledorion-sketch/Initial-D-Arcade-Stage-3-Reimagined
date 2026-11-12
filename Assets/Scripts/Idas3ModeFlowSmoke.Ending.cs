using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

public sealed partial class Idas3ModeFlowSmoke {
    private Action endingCaptureFilter;
    [Serializable] private sealed class EndingGlareSample {
        public int sample,lamps,textureBase,ranges,vertices;public uint random;
    }
    private IEnumerator EndingGlare(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        var scene=host.GetComponent<Idas3SceneRenderer>();uint random=0;
        for(int phase=0;phase<3;++phase){
            yield return Fixture(660+phase);refresh.Invoke(host,null);yield return Frames(2);
            var sample=JsonUtility.FromJson<EndingGlareSample>(File.ReadAllText(Path.Combine(root,"ending-glare-"+phase+".json")));
            if(phase==0)random=sample.random;else Check(random==sample.random,"Optical headlight comparison preserves RNG");
            Check((sample.lamps>0)==(phase!=1),"Ending optical headlights and removal");
            int ranges=0,vertices=0;var frame=scene.CurrentFrame;
            for(int i=0;i<frame.rangeCount;++i){int at=i*64,texture=Marshal.ReadInt32(frame.ranges,at+8);
                if(texture<sample.textureBase||texture>=sample.textureBase+4)continue;
                ++ranges;vertices+=Marshal.ReadInt32(frame.ranges,at+4);
                Check(Marshal.ReadInt32(frame.ranges,at+40)==1,"Ending headlights belong to main view");
                Check((Marshal.ReadInt32(frame.ranges,at+28)&3)==3,"Ending headlights preserve emissive source material");
                Check((Marshal.ReadInt32(frame.ranges,at+20)&(1<<26))!=0,"Ending glare does not write depth");
            }
            Check(ranges==sample.ranges&&vertices==sample.vertices,"Unity receives all optical headlight geometry");
            yield return Capture("ending-glare-"+phase);
        }
        var on=DustPixels("ending-glare-0");var off=DustPixels("ending-glare-1");var restored=DustPixels("ending-glare-2");
        int changed=0,differences=0;
        for(int i=0;i<on.Length;++i){var a=on[i];var b=off[i];var c=restored[i];
            if(a.r!=b.r||a.g!=b.g||a.b!=b.b)++changed;if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++differences;}
        Check(changed>8,"Ending optical headlights contribute visible pixels");
        Check(differences==0,"Restored ending optical headlight pixels are exact");
        File.WriteAllText(Path.Combine(root,"ending-glare-pixels.json"),"{\"changedPixels\":"+changed+",\"restoredDifferences\":"+differences+"}");
        Finish(true,null);
    }
    [Serializable] private sealed class EndingTireSample {
        public int sample,strips,ranges,vertices;public uint random;
    }
    private IEnumerator EndingTires(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        var scene=host.GetComponent<Idas3SceneRenderer>();uint random=0;
        for(int phase=0;phase<3;++phase){
            yield return Fixture(650+phase);refresh.Invoke(host,null);yield return Frames(2);
            var sample=JsonUtility.FromJson<EndingTireSample>(File.ReadAllText(Path.Combine(root,"ending-tire-"+phase+".json")));
            if(phase==0)random=sample.random;else Check(random==sample.random,"Tyre comparison preserves RNG");
            Check((sample.strips>0)==(phase!=1),"Recorded source tyre strips and removal");
            int ranges=0,vertices=0;var frame=scene.CurrentFrame;
            for(int i=0;i<frame.rangeCount;++i){int at=i*64;
                if(unchecked((uint)Marshal.ReadInt32(frame.ranges,at+12))!=0x941824c0u||Marshal.ReadInt32(frame.ranges,at+24)!=0xaa)continue;
                ++ranges;vertices+=Marshal.ReadInt32(frame.ranges,at+4);
                Check(Marshal.ReadInt32(frame.ranges,at+40)==1,"Ending tyre marks belong to main view");
            }
            Check(ranges==sample.ranges&&vertices==sample.vertices,"Unity receives every tyre-mark vertex");
            yield return Capture("ending-tire-"+phase);
        }
        var on=DustPixels("ending-tire-0");var off=DustPixels("ending-tire-1");var restored=DustPixels("ending-tire-2");
        int changed=0,differences=0;
        for(int i=0;i<on.Length;++i){var a=on[i];var b=off[i];var c=restored[i];
            if(a.r!=b.r||a.g!=b.g||a.b!=b.b)++changed;if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++differences;}
        Check(changed>8,"Original ending tyre marks contribute visible pixels");
        Check(differences==0,"Restored ending tyre pixels are exact");
        File.WriteAllText(Path.Combine(root,"ending-tire-pixels.json"),"{\"changedPixels\":"+changed+",\"restoredDifferences\":"+differences+"}");
        Finish(true,null);
    }
    [Serializable] private sealed class EndingLeafSample {
        public int sample,leaves,textureBase,ranges,vertices;public uint random;
    }
    private IEnumerator EndingLeaves(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        var scene=host.GetComponent<Idas3SceneRenderer>();uint random=0;
        for(int phase=0;phase<3;++phase){
            yield return Fixture(640+phase);refresh.Invoke(host,null);yield return Frames(2);
            var sample=JsonUtility.FromJson<EndingLeafSample>(File.ReadAllText(Path.Combine(root,"ending-leaf-"+phase+".json")));
            if(phase==0)random=sample.random;else Check(random==sample.random,"Leaf comparison preserves RNG");
            Check((sample.leaves>0)==(phase!=1),"Recorded source leaf admission and removal");
            int ranges=0,vertices=0;var frame=scene.CurrentFrame;
            for(int i=0;i<frame.rangeCount;++i){int at=i*64,texture=Marshal.ReadInt32(frame.ranges,at+8);
                if(texture<sample.textureBase||texture>=sample.textureBase+4)continue;
                ++ranges;vertices+=Marshal.ReadInt32(frame.ranges,at+4);
                Check(Marshal.ReadInt32(frame.ranges,at+40)==1,"Ending leaves belong to main view");
            }
            Check(ranges==sample.ranges&&vertices==sample.vertices,"Unity receives every leaf vertex");
            yield return Capture("ending-leaf-"+phase);
        }
        var on=DustPixels("ending-leaf-0");var off=DustPixels("ending-leaf-1");var restored=DustPixels("ending-leaf-2");
        int changed=0,differences=0;
        for(int i=0;i<on.Length;++i){if(!on[i].Equals(off[i]))++changed;if(!on[i].Equals(restored[i]))++differences;}
        Check(changed>8,"Original ending leaves contribute visible pixels");
        Check(differences==0,"Restored ending leaf pixels are exact");
        File.WriteAllText(Path.Combine(root,"ending-leaf-pixels.json"),"{\"changedPixels\":"+changed+",\"restoredDifferences\":"+differences+"}");
        Finish(true,null);
    }
    [Serializable] private sealed class EndingFlareSample {
        public int sample,pieces,textureBase,ranges,vertices;public uint random;
    }
    private IEnumerator EndingFlare(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        var scene=host.GetComponent<Idas3SceneRenderer>();
        for(int scenario=0;scenario<3;++scenario){
            string prefix="ending-flare-"+scenario;uint random=0;
            for(int phase=0;phase<3;++phase){
                yield return Fixture(510+scenario*3+phase);refresh.Invoke(host,null);yield return Frames(2);
                var sample=JsonUtility.FromJson<EndingFlareSample>(File.ReadAllText(Path.Combine(root,prefix+"-"+phase+".json")));
                if(phase==0)random=sample.random;else Check(random==sample.random,"Flare comparison preserves RNG");
                bool visible=scenario==0&&phase!=1;
                Check(sample.pieces==(visible?11:0),"Flare day/rain/night admission and removal");
                int ranges=0,vertices=0;var frame=scene.CurrentFrame;
                if(visible)for(int i=0;i<frame.rangeCount;++i){int at=i*64,texture=Marshal.ReadInt32(frame.ranges,at+8);
                    if(texture<sample.textureBase||texture>=sample.textureBase+7)continue;
                    ++ranges;vertices+=Marshal.ReadInt32(frame.ranges,at+4);
                    int tsp=Marshal.ReadInt32(frame.ranges,at+12),isp=Marshal.ReadInt32(frame.ranges,at+20);
                    Check((uint)isp==0x93000000u&&((uint)tsp==0x9400245bu||(uint)tsp==0x94002452u),"Original flare depth and blend reach Unity");
                    Check((Marshal.ReadInt32(frame.ranges,at+28)&3)==3&&Marshal.ReadInt32(frame.ranges,at+40)==1,"Flare stays unlit in main view");
                }
                Check(ranges==sample.ranges&&vertices==sample.vertices,"Unity receives every flare vertex");
                yield return Capture(prefix+"-"+phase);
            }
            var on=DustPixels(prefix+"-0");var off=DustPixels(prefix+"-1");var restored=DustPixels(prefix+"-2");
            var pixels=new EndingWeatherPixels();
            for(int i=0;i<on.Length;++i){if(!on[i].Equals(off[i]))++pixels.changedPixels;if(!on[i].Equals(restored[i]))++pixels.restoredDifferences;}
            Check(pixels.restoredDifferences==0,"Restored flare pixels are exact");
            Check(scenario==0?pixels.changedPixels>100:pixels.changedPixels==0,"Flare is visible only in daytime dry ending");
            File.WriteAllText(Path.Combine(root,prefix+"-pixels.json"),JsonUtility.ToJson(pixels,true));
        }
        Finish(true,null);
    }
    [Serializable] private sealed class EndingBackfireSample {
        public int sample,textureBase,flames,ranges,vertices,updates,playerFrame,rivalFrame;public uint random;
    }
    private IEnumerator EndingBackfire(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        var scene=host.GetComponent<Idas3SceneRenderer>();
        for(int scenario=0;scenario<4;++scenario){
            string prefix="ending-backfire-"+scenario;uint random=0;EndingBackfireSample sample=null;
            foreach(int phase in new[]{0,1,2}){
                yield return Fixture(490+scenario*3+phase);refresh.Invoke(host,null);yield return Frames(2);
                sample=JsonUtility.FromJson<EndingBackfireSample>(File.ReadAllText(Path.Combine(root,prefix+"-"+phase+".json")));
                if(phase==0)random=sample.random;else Check(random==sample.random,"Backfire comparison preserves shared RNG");
                Check((sample.flames==0)==(phase==1),"Original flame admission and suppression");
                if(phase!=1)Check(sample.updates==sample.sample&&(scenario==3?sample.rivalFrame>=0:sample.playerFrame>=0),"Recorded trigger and source cadence");
                var frame=scene.CurrentFrame;int ranges=0,vertices=0;
                for(int i=0;i<frame.rangeCount;++i){int at=i*64,texture=Marshal.ReadInt32(frame.ranges,at+8);
                    if(texture<sample.textureBase||texture>=sample.textureBase+8)continue;
                    ++ranges;vertices+=Marshal.ReadInt32(frame.ranges,at+4);
                    int flags=Marshal.ReadInt32(frame.ranges,at+28);
                    Check((flags&3)==3&&Marshal.ReadInt32(frame.ranges,at+36)==0&&Marshal.ReadInt32(frame.ranges,at+40)==1,
                        "Flame stays original, unlit and in the main view");
                }
                Check(ranges==sample.ranges&&vertices==sample.vertices,"Unity receives every original flame vertex");
                yield return Capture(prefix+"-"+phase);
            }
            var visible=DustPixels(prefix+"-0");var absent=DustPixels(prefix+"-1");var restored=DustPixels(prefix+"-2");
            var pixels=new EndingWeatherPixels();
            for(int i=0;i<visible.Length;++i){
                if(!visible[i].Equals(absent[i]))++pixels.changedPixels;
                if(!visible[i].Equals(restored[i]))++pixels.restoredDifferences;
            }
            Check(pixels.changedPixels>0&&pixels.restoredDifferences==0,"Backfire removal is visible and restoration is exact");
            File.WriteAllText(Path.Combine(root,prefix+"-pixels.json"),JsonUtility.ToJson(pixels,true));
            //Separate flame geometry from its point light, which can also
            //change nearby body/road pixels in the full-owner comparison.
            var matching=new List<MeshRenderer>();
            foreach(var renderer in FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None)){
                var material=renderer.sharedMaterial;
                if(!renderer.enabled||!renderer.gameObject.activeInHierarchy||material==null||material.mainTexture==null||
                   !Idas3SceneRenderer.IsSceneGeometryLayer(renderer.gameObject.layer))continue;
                bool selected=false;for(int id=sample.textureBase;id<sample.textureBase+8;++id)
                    selected|=material.mainTexture.name.StartsWith("Original texture "+id+" sampler ",StringComparison.Ordinal);
                if(selected)matching.Add(renderer);
            }
            Check(matching.Count==sample.ranges,"Original flame renderers are independently selectable");
            ulong generation=scene.CurrentFrame.frameGeneration,rendered=host.Status.renderedFrames,ticks=host.Status.simulationTicks;
            endingCaptureFilter=()=>{foreach(var renderer in matching)renderer.enabled=false;};
            try{yield return Capture(prefix+"-without-flame");}
            finally{endingCaptureFilter=null;foreach(var renderer in matching)if(renderer!=null)renderer.enabled=true;}
            yield return Capture(prefix+"-without-flame-restored");
            absent=DustPixels(prefix+"-without-flame");restored=DustPixels(prefix+"-without-flame-restored");
            pixels=new EndingWeatherPixels();
            for(int i=0;i<visible.Length;++i){if(!visible[i].Equals(absent[i]))++pixels.changedPixels;if(!visible[i].Equals(restored[i]))++pixels.restoredDifferences;}
            File.WriteAllText(Path.Combine(root,prefix+"-flame-pixels.json"),JsonUtility.ToJson(pixels,true));
            Check(pixels.changedPixels>0&&pixels.restoredDifferences==0,"Flame geometry is visible and restores exactly");
            Check(scene.CurrentFrame.frameGeneration==generation&&host.Status.renderedFrames==rendered&&host.Status.simulationTicks==ticks,
                "Flame-only captures do not advance native state");
        }
        Finish(true,null);
    }
    [Serializable] private sealed class EndingWeatherSample {
        public int sample,textureBase,streaks,splashes,mist,glare,ranges,vertices,updates,flakes;
        public int lampBase,lampRanges,lampVertices,lampRibbons,lampUpdates;
        public uint random;public bool wet,enabled,snow;
    }
    [Serializable] private sealed class EndingWeatherPixels {
        public int changedPixels,restoredDifferences;public bool wet;
    }
    [Serializable] private sealed class EndingWeatherFamily {
        public string family;public int nativeRanges,unityRanges,changedPixels,restoredDifferences;
    }
    private IEnumerator EndingWeatherFamilies(Idas3SceneRenderer scene,string prefix,EndingWeatherSample sample){
        var visible=DustPixels(prefix+"-2");var frame=scene.CurrentFrame;
        ulong generation=frame.frameGeneration,rendered=host.Status.renderedFrames,ticks=host.Status.simulationTicks;
        string[] names={"streaks","splashes","mist","glare"};
        int[][] indices={new[]{6},new[]{4},new[]{7},new[]{8,9}};
        for(int family=0;family<names.Length;++family){
            var result=new EndingWeatherFamily{family=names[family]};var matching=new List<MeshRenderer>();
            var ids=new HashSet<int>();foreach(int index in indices[family])ids.Add(sample.textureBase+index);
            for(int i=0;i<frame.rangeCount;++i)if(ids.Contains(Marshal.ReadInt32(frame.ranges,i*64+8)))++result.nativeRanges;
            foreach(var renderer in FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None)){
                var material=renderer.sharedMaterial;
                if(!renderer.enabled||!renderer.gameObject.activeInHierarchy||material==null||material.mainTexture==null||
                   !Idas3SceneRenderer.IsSceneGeometryLayer(renderer.gameObject.layer))continue;
                bool selected=false;foreach(int id in ids)selected|=material.mainTexture.name.StartsWith("Original texture "+id+" sampler ",StringComparison.Ordinal);
                if(!selected)continue;
                Check(material.GetInteger("original")==1&&material.GetInteger("emissive")==1&&material.GetInteger("viewMask")==1,
                    "Rain family has an original unlit main-view Unity material");
                matching.Add(renderer);
            }
            result.unityRanges=matching.Count;
            Check(result.nativeRanges>0&&result.nativeRanges==result.unityRanges,"Unity material publication for "+result.family);
            string shot=prefix+"-without-"+result.family;
            endingCaptureFilter=()=>{foreach(var renderer in matching)renderer.enabled=false;};
            try{yield return Capture(shot);}
            finally{endingCaptureFilter=null;foreach(var renderer in matching)if(renderer!=null)renderer.enabled=true;}
            yield return Capture(shot+"-restored");
            var removed=DustPixels(shot);var restored=DustPixels(shot+"-restored");
            for(int i=0;i<visible.Length;++i){var a=visible[i];var b=removed[i];var c=restored[i];
                if(a.r!=b.r||a.g!=b.g||a.b!=b.b)++result.changedPixels;
                if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++result.restoredDifferences;
            }
            File.WriteAllText(Path.Combine(root,shot+".json"),JsonUtility.ToJson(result,true));
            Check(result.restoredDifferences==0,"Rain family restores identical pixels: "+result.family);
            Check(scene.CurrentFrame.frameGeneration==generation&&host.Status.renderedFrames==rendered&&host.Status.simulationTicks==ticks,
                "Rain visibility comparisons do not advance native owners");
            // Individual families can be occluded in one chosen view. Record
            // their pixels here; the launcher requires visibility across views.
        }
    }
    private IEnumerator EndingWeather(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        var scene=host.GetComponent<Idas3SceneRenderer>();
        for(int scenario=0;scenario<6;++scenario){
            string prefix="ending-weather-"+scenario;uint random=0;
            foreach(int phase in new[]{0,1,2}){
                yield return Fixture(470+scenario*3+phase);refresh.Invoke(host,null);
                yield return Frames(2);
                var sample=JsonUtility.FromJson<EndingWeatherSample>(File.ReadAllText(Path.Combine(root,prefix+"-"+phase+".json")));
                if(phase==0)random=sample.random;else Check(sample.random==random,"Rain comparison preserves shared RNG");
                Check(sample.snow==(scenario>=4)&&sample.wet==(scenario>=4||(scenario&1)!=0)&&sample.enabled==(sample.wet&&phase!=1),"Weather admission in published fixture");
                int ranges=0,vertices=0;var frame=scene.CurrentFrame;
                for(int i=0;i<frame.rangeCount;++i){
                    int at=checked(i*64),texture=Marshal.ReadInt32(frame.ranges,at+8);
                    if(!sample.enabled||texture<sample.textureBase||texture>=sample.textureBase+11)continue;
                    ++ranges;vertices+=Marshal.ReadInt32(frame.ranges,at+4);
                    int flags=Marshal.ReadInt32(frame.ranges,at+28);
                    Check((flags&3)==3&&Marshal.ReadInt32(frame.ranges,at+36)==0&&Marshal.ReadInt32(frame.ranges,at+40)==1,
                        "Published rain stays original, unlit and in the main view");
                }
                Check(ranges==sample.ranges&&vertices==sample.vertices,"Unity receives all authored rain geometry");
                if(sample.enabled)Check((sample.snow?sample.flakes>0:sample.streaks>0&&sample.splashes>0&&sample.mist>0)&&sample.updates==sample.sample,"Weather owner advances at recorded cadence");
                yield return Capture(prefix+"-"+phase);
            }
            var visible=DustPixels(prefix+"-0");var removed=DustPixels(prefix+"-1");var restored=DustPixels(prefix+"-2");
            Check(visible.Length==removed.Length&&visible.Length==restored.Length,"Ending image dimensions");
            var result=new EndingWeatherPixels{wet=scenario>=4||(scenario&1)!=0};
            for(int i=0;i<visible.Length;++i){var a=visible[i];var b=removed[i];var c=restored[i];
                if(a.r!=b.r||a.g!=b.g||a.b!=b.b)++result.changedPixels;
                if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++result.restoredDifferences;
            }
            File.WriteAllText(Path.Combine(root,prefix+"-pixels.json"),JsonUtility.ToJson(result,true));
            Check(result.restoredDifferences==0,"Restoring rain reproduces identical Unity pixels");
            Check(result.wet?result.changedPixels>100:result.changedPixels==0,"Wet rain is visible; dry ending stays unchanged");
            var finalSample=JsonUtility.FromJson<EndingWeatherSample>(File.ReadAllText(Path.Combine(root,prefix+"-2.json")));
            if(result.wet&&scenario<4)yield return EndingWeatherFamilies(scene,prefix,finalSample);
            yield return EndingLampFamilies(scene,prefix,JsonUtility.FromJson<EndingWeatherSample>(File.ReadAllText(Path.Combine(root,"ending-lamps-"+scenario+"-2.json"))));
        }
        Finish(true,null);
    }
    private IEnumerator EndingLampFamilies(Idas3SceneRenderer scene,string prefix,EndingWeatherSample sample){
        Check(sample.lampUpdates==sample.sample,"Ending lamp owner runs at source cadence");
        var frame=scene.CurrentFrame;var visible=DustPixels(prefix+"-2");
        ulong generation=frame.frameGeneration,rendered=host.Status.renderedFrames,ticks=host.Status.simulationTicks;
        for(int family=0;family<2;++family){
            var result=new EndingWeatherFamily{family=family==0?"rear-ribbons":"rear-lamps"};
            var matching=new List<MeshRenderer>();int vertices=0;
            for(int i=0;i<frame.rangeCount;++i){int at=i*64,id=Marshal.ReadInt32(frame.ranges,at+8);
                if(id!=sample.lampBase&&(family==0||id!=sample.lampBase+1))continue;
                if(Marshal.ReadInt32(frame.ranges,at+4)<=0)continue;
                if(family==0&&(unchecked((uint)Marshal.ReadInt32(frame.ranges,at+12))!=0x941024e2u||
                    Marshal.ReadInt32(frame.ranges,at+24)!=0x6aa))continue;
                ++result.nativeRanges;vertices+=Marshal.ReadInt32(frame.ranges,at+4);
            }
            if(family==0)Check(result.nativeRanges==sample.lampRibbons,"Only original lamp ribbons are selected");
            if(family==1)Check(result.nativeRanges==sample.lampRanges&&vertices==sample.lampVertices,"Unity receives every ending lamp vertex");
            foreach(var renderer in FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None)){
                var material=renderer.sharedMaterial;
                if(!renderer.enabled||!renderer.gameObject.activeInHierarchy||material==null||material.mainTexture==null||
                   !Idas3SceneRenderer.IsSceneGeometryLayer(renderer.gameObject.layer))continue;
                bool selected=material.mainTexture.name.StartsWith("Original texture "+sample.lampBase+" sampler ",StringComparison.Ordinal)||
                    (family==1&&material.mainTexture.name.StartsWith("Original texture "+(sample.lampBase+1)+" sampler ",StringComparison.Ordinal));
                if(!selected)continue;
                if(family==0&&(unchecked((uint)material.GetInteger("tsp"))!=0x941024e2u||material.GetInteger("gmp")!=0x6aa))continue;
                Check(material.GetInteger("original")==1&&material.GetInteger("emissive")==1&&material.GetInteger("viewMask")==1,
                    "Ending lamps retain original unlit main-view material");matching.Add(renderer);
            }
            result.unityRanges=matching.Count;
            Check(result.nativeRanges==result.unityRanges,"Ending lamp geometry published to Unity");
            string shot=prefix+"-without-"+result.family;
            endingCaptureFilter=()=>{foreach(var renderer in matching)renderer.enabled=false;};
            try{yield return Capture(shot);}
            finally{endingCaptureFilter=null;foreach(var renderer in matching)if(renderer!=null)renderer.enabled=true;}
            yield return Capture(shot+"-restored");
            var removed=DustPixels(shot);var restored=DustPixels(shot+"-restored");
            for(int i=0;i<visible.Length;++i){var a=visible[i];var b=removed[i];var c=restored[i];
                if(a.r!=b.r||a.g!=b.g||a.b!=b.b)++result.changedPixels;
                if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++result.restoredDifferences;
            }
            File.WriteAllText(Path.Combine(root,shot+".json"),JsonUtility.ToJson(result,true));
            Check(result.restoredDifferences==0,"Ending lamps restore identical pixels");
            if(result.nativeRanges==0)Check(result.changedPixels==0,"No lamp ribbon changes a ribbon-free frame");
            Check(scene.CurrentFrame.frameGeneration==generation&&host.Status.renderedFrames==rendered&&host.Status.simulationTicks==ticks,
                "Ending lamp comparisons do not advance native owners");
        }
    }
}
