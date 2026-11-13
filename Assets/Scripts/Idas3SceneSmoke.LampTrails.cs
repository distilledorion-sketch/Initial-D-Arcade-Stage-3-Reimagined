using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

public sealed partial class Idas3SceneSmoke {
    [Serializable] private sealed class LampTrailObservation {
        public int sourceFrame,nativeRibbons,unityRibbons,changedPixels,redPixels,restoreDifferences;
        public bool nativeStateUnchanged;
    }
    private Color32[] LampTrailPixels(string name){
        var image=new Texture2D(2,2,TextureFormat.RGBA32,false);
        try{
            Check(image.LoadImage(File.ReadAllBytes(Path.Combine(root,name+".png"))),"Lamp-trail capture decode");
            return image.GetPixels32();
        }finally{Destroy(image);}
    }
    // Opt-in packaged-player comparison. Only the ribbon renderers are hidden;
    // the original lamp sprites, headlights and recorded simulation stay fixed.
    private IEnumerator IntroLampTrailProbe(Idas3SceneRenderer scene,int sample){
        frozen=true;
        string prefix="intro-"+sample.ToString("0000");
        var reference=shots[shots.Count-1];
        ulong rendered=host.Status.renderedFrames,ticks=host.Status.simulationTicks;
        var frame=scene.CurrentFrame;
        var result=new LampTrailObservation{sourceFrame=sample-1};
        for(int i=0;i<frame.rangeCount;++i){
            int offset=checked(i*64);
            if(Marshal.ReadInt32(frame.ranges,offset+4)>0&&
               unchecked((uint)Marshal.ReadInt32(frame.ranges,offset+12))==0x941024e2u&&
               Marshal.ReadInt32(frame.ranges,offset+24)==0x6aa)++result.nativeRibbons;
        }
        var ribbons=new List<MeshRenderer>();
        foreach(var renderer in FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None)){
            var material=renderer.sharedMaterial;
            if(!renderer.enabled||!renderer.gameObject.activeInHierarchy||material==null||
               !Idas3SceneRenderer.IsSceneGeometryLayer(renderer.gameObject.layer))continue;
            if(unchecked((uint)material.GetInteger("tsp"))!=0x941024e2u||material.GetInteger("gmp")!=0x6aa)continue;
            Check(material.GetInteger("original")==1&&material.GetInteger("emissive")==1&&material.GetInteger("viewMask")==1,
                "Lamp ribbon lost its original unlit main-camera material");
            ribbons.Add(renderer);
        }
        result.unityRibbons=ribbons.Count;
        Check(result.unityRibbons==result.nativeRibbons,"Unity did not publish every native lamp ribbon");
        Check(Mathf.Abs(host.GetComponent<Camera>().nearClipPlane-.01f)<.00001f,"Attract view-plane lamp geometry was clipped");
        var visible=LampTrailPixels(prefix);
        try{
            foreach(var renderer in ribbons)renderer.enabled=false;
            yield return Capture(prefix+"-no-ribbons",true);
            CheckFrozenDoorScene(reference,rendered,ticks);
        }finally{
            foreach(var renderer in ribbons)if(renderer!=null)renderer.enabled=true;
            frozen=true;
        }
        yield return Capture(prefix+"-restored",true);
        CheckFrozenDoorScene(reference,rendered,ticks);
        result.nativeStateUnchanged=true;
        var removed=LampTrailPixels(prefix+"-no-ribbons");
        var restored=LampTrailPixels(prefix+"-restored");
        Check(visible.Length==removed.Length&&visible.Length==restored.Length,"Lamp-trail capture dimensions changed");
        for(int i=0;i<visible.Length;++i){
            var a=visible[i];var b=removed[i];var c=restored[i];
            if(a.r!=c.r||a.g!=c.g||a.b!=c.b)++result.restoreDifferences;
            if(a.r==b.r&&a.g==b.g&&a.b==b.b)continue;
            ++result.changedPixels;
            if(a.r>b.r&&a.r>a.g&&a.r>a.b)++result.redPixels;
        }
        File.WriteAllText(Path.Combine(root,prefix+"-trails.json"),JsonUtility.ToJson(result,true));
        Check(result.restoreDifferences==0,"Restoring lamp ribbons did not reproduce identical Unity pixels");
        if(sample==851||sample==901||sample==2568||sample==3037)
            Check(result.unityRibbons>=2&&result.changedPixels>20&&result.redPixels>10,"Recorded rear lamp ribbons did not render in Unity");
        if(result.nativeRibbons==0)Check(result.changedPixels==0,"Lamp-free frame changed during ribbon-only comparison");
    }
}
