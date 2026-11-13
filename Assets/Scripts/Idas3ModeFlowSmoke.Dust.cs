using System;
using System.Collections;
using System.IO;
using UnityEngine;

public sealed partial class Idas3ModeFlowSmoke {
    [Serializable] private sealed class DustSequenceSample {
        public int scenario,tick,brown,white,particles;public float speedKmh;public string digest;
    }
    [Serializable] private sealed class DustSequencePixels {
        public int scenario,tick,changedPixels,brownShift,restoredDifferences;
    }
    private IEnumerator RoadsideDustSequence(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        var visibleSamples=new int[2];
        for(int key=0;key<28;++key){
            string prefix="dust-sequence-"+key;DustSequenceSample first=null;
            for(int phase=0;phase<3;++phase){
                yield return Fixture(550+key*3+phase);refresh.Invoke(host,null);yield return Frames(2);
                var sample=JsonUtility.FromJson<DustSequenceSample>(File.ReadAllText(Path.Combine(root,prefix+"-"+phase+".json")));
                if(phase==0)first=sample;
                if(phase==1)Check(sample.particles==0,"Smoke baseline removes every source particle");
                if(phase==2)Check(sample.digest==first.digest,"Restored dust state and random seed are exact");
                if(sample.scenario>=2)Check(sample.brown==0,"Night/rain excludes brown dirt while retaining legitimate white smoke");
                yield return Capture(prefix+"-"+phase);
            }
            var on=DustPixels(prefix+"-0");var off=DustPixels(prefix+"-1");var restored=DustPixels(prefix+"-2");
            Check(on.Length==off.Length&&on.Length==restored.Length,"Dust sequence image dimensions");
            var result=new DustSequencePixels{scenario=first.scenario,tick=first.tick};
            for(int i=0;i<on.Length;++i){var a=on[i];var b=off[i];var c=restored[i];
                if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++result.restoredDifferences;
                if(a.r==b.r&&a.g==b.g&&a.b==b.b)continue;
                ++result.changedPixels;if(a.r*b.g>a.g*b.r&&a.r*b.b>a.b*b.r)++result.brownShift;
            }
            File.WriteAllText(Path.Combine(root,prefix+"-pixels.json"),JsonUtility.ToJson(result,true));
            Check(result.restoredDifferences==0,"Dust sequence restoration reproduces Unity pixels");
            if(first.scenario<2&&result.changedPixels>100&&result.brownShift>20){
                Check(first.brown>0,"Visible dirt has an actual source brown emitter");++visibleSamples[first.scenario];
            }
        }
        Check(visibleSamples[0]>=2&&visibleSamples[1]>=2,"Both dry directions render brown dirt at multiple recorded frames");
        Finish(true,null);
    }
    private IEnumerator MovingMirror(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        bool expectOld=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-mirror-smoke-reference")>=0;
        int mirrorChanges=0,mainChanges=0;
        for(int key=0;key<6;++key){
            string prefix="moving-mirror-"+key;
            for(int phase=0;phase<3;++phase){
                yield return Fixture(670+key*3+phase);refresh.Invoke(host,null);yield return Frames(2);
                Check(host.GetComponent<Idas3SceneRenderer>().MirrorCamera.enabled,"Moving fixture renders mirror");
                yield return Capture(prefix+"-"+phase);
            }
            var on=DustPixels(prefix+"-0");var off=DustPixels(prefix+"-1");var restored=DustPixels(prefix+"-2");
            Check(on.Length==off.Length&&on.Length==restored.Length,"Moving mirror image dimensions");
            var rect=host.GetComponent<Idas3SceneRenderer>().MirrorCamera.pixelRect;
            int changed=0,mirror=0,restoredDifferences=0;
            for(int i=0;i<on.Length;++i){
                var a=on[i];var b=off[i];var c=restored[i];
                if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++restoredDifferences;
                if(a.r==b.r&&a.g==b.g&&a.b==b.b)continue;
                ++changed;if(rect.Contains(new Vector2(i%Screen.width,i/Screen.width)))++mirror;
            }
            File.WriteAllText(Path.Combine(root,prefix+"-pixels.txt"),"changed="+changed+" mirror="+mirror+" restoredDifferences="+restoredDifferences+"\n");
            Check(restoredDifferences==0,"Restoring moving smoke reproduces Unity pixels");
            if(!expectOld)Check(mirror==0,"Source smoke must not be submitted to the arcade rear-view camera");
            mirrorChanges+=mirror;mainChanges+=changed-mirror;
        }
        Check(mainChanges>100,"Real smoke/dirt remains visible in the main view");
        if(expectOld)Check(mirrorChanges>100,"Reference reproduces unwanted moving mirror smoke");
        Finish(true,null);
    }
    private IEnumerator StoppedMirror(){
        yield return Until(()=>host.Ready,600,"Scene initialized");yield return Frames(3);frozen=true;
        var refresh=typeof(Idas3SceneGame).GetMethod("RefreshScene",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic);
        for(int scenario=0;scenario<6;++scenario){
            string prefix="parked-mirror-"+scenario;
            foreach(int phase in new[]{0,1,2}){
                yield return Fixture(530+scenario*3+phase);refresh.Invoke(host,null);yield return Frames(2);
                Check(host.GetComponent<Idas3SceneRenderer>().MirrorCamera.enabled,"Parked fixture renders the rear-view mirror");
                yield return Capture(prefix+"-"+phase);
            }
            var visible=DustPixels(prefix+"-0");var baseline=DustPixels(prefix+"-1");var restored=DustPixels(prefix+"-2");
            Check(visible.Length==baseline.Length&&visible.Length==restored.Length,"Mirror image dimensions");
            int changed=0,restoredDifferences=0;
            for(int i=0;i<visible.Length;++i){
                var a=visible[i];var b=baseline[i];var c=restored[i];
                if(a.r!=b.r||a.g!=b.g||a.b!=b.b)++changed;
                if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++restoredDifferences;
            }
            File.WriteAllText(Path.Combine(root,prefix+"-pixels.txt"),"changed="+changed+" restoredDifferences="+restoredDifferences+"\n");
            Check(restoredDifferences==0,"Restoring parked particle state reproduces Unity pixels");
            Check(changed==0,"Parked car renders lingering smoke or dust through Unity");
        }
        Finish(true,null);
    }
    private Color32[] DustPixels(string name){
        var image=new Texture2D(2,2,TextureFormat.RGBA32,false);
        try{
            Check(image.LoadImage(File.ReadAllBytes(Path.Combine(root,name+".png"))),"Dust capture decode");
            return image.GetPixels32();
        }finally{Destroy(image);}
    }
    private IEnumerator RoadsideDust(){
        // Retain the existing command-line entry point. A single frame can
        // hide newly emitted dirt behind the car, and rain has valid white
        // spray. The sequence checks source colour admission and exact
        // restoration without requiring all rain particles to be absent.
        yield return RoadsideDustSequence();
    }
}
