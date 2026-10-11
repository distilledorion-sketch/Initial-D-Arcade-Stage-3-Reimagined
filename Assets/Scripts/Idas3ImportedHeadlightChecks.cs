using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;

// Opt-in, frozen-frame production rendering check. Never runs during normal play.
public static class Idas3ImportedHeadlightChecks
{
    [Serializable] sealed class Report {
        public int course,projectors,brighterPixels,darkerPixels,maximumGain,originalPassDifferences,maskedDifferences;
        public double averageGainWhereBrighter;
        public bool importedNight,passed;
        public string device,scope;
    }
    public static void Run(Idas3SceneRenderer scene,RenderTexture target,string output,int course,bool importedNight,Action<bool,string> check)
    {
        check(target!=null,"Headlight comparison needs an offscreen target");
        var shader=Resources.Load<Shader>("Idas3Headlight");
        var projectors=UnityEngine.Object.FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None)
            .Where(r=>r.enabled&&r.gameObject.activeInHierarchy&&r.sharedMaterial&&r.sharedMaterial.shader==shader).ToArray();
        check(!importedNight||projectors.Length>0,"Imported night race has no projected headlight");
        float[] frame=new float[184];Marshal.Copy(scene.CurrentFrame.frameConstants,frame,0,frame.Length);
        check((frame[31]>.5f)==importedNight,"Imported headlight frame flag does not match course/time");
        var materials=projectors.Select(r=>r.sharedMaterial).Distinct().ToArray();
        var fills=materials.Select(m=>m.GetFloat("_ImportedHeadlightFill")).ToArray();
        var masks=materials.Select(m=>m.GetInteger("viewMask")).ToArray();
        var cameras=Resources.FindObjectsOfTypeAll<Camera>().Where(c=>(c.enabled||c==scene.MainCamera)&&c.gameObject.activeInHierarchy&&c.targetTexture==target)
            .OrderBy(c=>c.depth).ThenBy(c=>c.name,StringComparer.Ordinal).ToArray();
        check(cameras.Contains(scene.MainCamera),"Headlight capture omitted main camera");
        check(!scene.MirrorCamera.enabled||cameras.Contains(scene.MirrorCamera),"Headlight capture omitted mirror");
        int samples=target.antiAliasing;var prior=RenderTexture.active;
        var image=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);
        var originals=new Dictionary<Material,Material>();
        Color32[] Shot(string name){
            foreach(var camera in cameras)camera.Render();
            RenderTexture.active=target;image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();
            if(name!=null)File.WriteAllBytes(Path.Combine(output,"headlight-"+name+".png"),image.EncodeToPNG());
            return image.GetPixels32();
        }
        int Differences(Color32[] a,Color32[] b)=>a.Where((p,i)=>!p.Equals(b[i])).Count();
        try {
            if(samples!=1){target.Release();target.antiAliasing=1;check(target.Create(),"Headlight AA1 target");}
            var boosted=Shot("after");
            foreach(var m in materials)m.SetFloat("_ImportedHeadlightFill",0);
            var source=Shot("before");
            foreach(var m in materials){
                var original=new Material(m){shader=Resources.Load<Shader>("Idas3SceneDirect"),renderQueue=m.renderQueue};
                // Native words are shader uniforms, not ShaderLab properties.
                // Reapply them explicitly when changing the reference shader.
                foreach(string word in new[]{"pcw","tsp","gmp","original","emissive","billboard","courseLightRange","_LightScope","viewMask","courseCullMode"})
                    original.SetInteger(word,m.GetInteger(word));
                original.SetFloat("glossCoefficient",m.GetFloat("glossCoefficient"));
                original.SetVector("_LightOverride",m.GetVector("_LightOverride"));
                originals.Add(m,original);
            }
            foreach(var r in projectors)r.sharedMaterial=originals[r.sharedMaterial];
            var oldShader=Shot("source-reference");
            foreach(var r in projectors)r.sharedMaterial=originals.Single(p=>p.Value==r.sharedMaterial).Key;
            foreach(var r in projectors)r.enabled=false;
            var off=Shot("off");
            foreach(var r in projectors)r.enabled=true;
            for(int i=0;i<materials.Length;i++){materials[i].SetFloat("_ImportedHeadlightFill",fills[i]);materials[i].SetInteger("viewMask",0);}
            var masked=Shot(null);
            var report=new Report{course=course,projectors=projectors.Length,importedNight=importedNight,device=SystemInfo.graphicsDeviceName,
                originalPassDifferences=Differences(source,oldShader),maskedDifferences=Differences(off,masked),
                scope="Same frozen native frame, complete main/mirror/HUD camera stack. Original shader comparison, boost off, projectors hidden and viewMask=0. AA1 readback; no display FPS claim."};
            long gain=0;
            for(int i=0;i<boosted.Length;i++){
                int delta=boosted[i].r+boosted[i].g+boosted[i].b-source[i].r-source[i].g-source[i].b;
                if(delta>0){report.brighterPixels++;gain+=delta;report.maximumGain=Math.Max(report.maximumGain,delta);}
                if(delta<0)report.darkerPixels++;
            }
            report.averageGainWhereBrighter=report.brighterPixels>0?gain/(3.0*report.brighterPixels):0;
            File.WriteAllText(Path.Combine(output,"headlights.json"),JsonUtility.ToJson(report,true));
            check(report.originalPassDifferences==0,"Headlight source pass differs from original shader");
            check(report.maskedDifferences==0,"Headlight fill ignored the native view mask");
            check(report.darkerPixels==0,"Headlight correction darkened pixels");
            check(importedNight?report.brighterPixels>1000:report.brighterPixels==0,"Headlight fill did not respect imported-night scope");
            report.passed=true;File.WriteAllText(Path.Combine(output,"headlights.json"),JsonUtility.ToJson(report,true));
        }
        finally {
            foreach(var r in projectors){
                var pair=originals.FirstOrDefault(p=>p.Value==r.sharedMaterial);
                if(pair.Key!=null)r.sharedMaterial=pair.Key;r.enabled=true;
            }
            for(int i=0;i<materials.Length;i++){materials[i].SetFloat("_ImportedHeadlightFill",fills[i]);materials[i].SetInteger("viewMask",masks[i]);}
            foreach(var m in originals.Values)UnityEngine.Object.Destroy(m);
            UnityEngine.Object.Destroy(image);RenderTexture.active=prior;
            if(samples!=1){target.Release();target.antiAliasing=samples;target.Create();}
        }
    }
}
