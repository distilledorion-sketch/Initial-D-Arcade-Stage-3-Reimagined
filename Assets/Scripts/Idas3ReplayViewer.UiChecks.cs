using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// Explicit private replay-viewer diagnostic. No gameplay, save writes or service calls.
public sealed partial class Idas3ReplayViewer {
    bool replayUiCheckStarted;
    [Serializable] sealed class ReplayUiCase {
        public int samples,width,height,addedVertices,changedPixels,bottomRow,edgePixels;
        public double seconds;
        public bool restored;
        public Vector4 bounds;
    }
    [Serializable] sealed class ReplayUiReport {
        public bool passed;
        public string error,device,scope;
        public ReplayUiCase[] cases;
    }
    void ReplayUiExtension(bool visible,double at) {
        foreach(var frame in replay.Frames)frame.state[32]=visible?120u:0u;
        Seek(at);Present();
    }
    Texture2D ReplayUiCapture(int samples) {
        var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){antiAliasing=samples};
        var previous=View.targetTexture;var active=RenderTexture.active;
        try {
            if(!target.Create()||target.antiAliasing!=samples)throw new InvalidOperationException("Requested MSAA target unavailable");
            View.targetTexture=target;View.Render();ui.RenderOverlayForCapture();RenderTexture.active=target;
            var texture=new Texture2D(target.width,target.height,TextureFormat.RGBA32,false);
            texture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);texture.Apply();return texture;
        } finally {View.targetTexture=previous;RenderTexture.active=active;target.Release();Destroy(target);}
    }
    static Vector4 ReplayUiAddedBounds(Vector3[] before,Vector3[] after,out int added) {
        var counts=new Dictionary<Vector3,int>();foreach(var p in before){counts.TryGetValue(p,out int n);counts[p]=n+1;}
        var bounds=new Vector4(float.PositiveInfinity,float.PositiveInfinity,float.NegativeInfinity,float.NegativeInfinity);added=0;
        foreach(var p in after){
            if(counts.TryGetValue(p,out int n)&&n>0){counts[p]=n-1;continue;}
            ++added;bounds.x=Mathf.Min(bounds.x,p.x);bounds.y=Mathf.Min(bounds.y,p.y);bounds.z=Mathf.Max(bounds.z,p.x);bounds.w=Mathf.Max(bounds.w,p.y);
        }
        foreach(int n in counts.Values)if(n!=0)throw new InvalidOperationException("Extension changed unrelated HUD geometry");
        if(added!=12)throw new InvalidOperationException("Expected the two original TIME EXTENSION quads, got "+added+" vertices");
        return bounds;
    }
    static bool ReplayUiPixelEqual(Color32 a,Color32 b)=>a.r==b.r&&a.g==b.g&&a.b==b.b;
    void CheckReplayUi() {
        if(Directory.Exists(proofDirectory)||File.Exists(proofDirectory)) {
            Debug.LogError("Preserve existing replay UI evidence; use a NEW proof directory");Application.Quit(1);return;
        }
        var rows=new List<ReplayUiCase>();var report=new ReplayUiReport{device=SystemInfo.graphicsDeviceName+" / "+SystemInfo.graphicsDeviceType,
            scope="Actual packaged replay loader, native detail/pose/HUD, Unity scene and production overlay cameras/shader. Controlled recorded extension flag; paired off/on/restored captures at 1x and 4x MSAA, including backward and repeated seeks. Isolated replay, no ordinary save writes."};
        try {
            if(AudioListener.volume!=0||!replay.Detailed||replay.Opponent!=null)
                throw new InvalidOperationException("Use a muted, single-track detailed replay and NEW proof directory");
            Directory.CreateDirectory(proofDirectory);playing=false;controlsVisible=false;
            // The private process has its own in-memory presentation options.
            audioOptions=Idas3GameOptions.Normalize(new Idas3GameOptions.Values());ui.HudOptionsOverride=audioOptions;
            cameraMode=0;
            foreach(int samples in new[]{1,4})foreach(double at in new[]{replay.Duration*.75,replay.Duration*.25,replay.Duration*.75}) {
                ReplayUiExtension(false,at);var prior=ui.ReplayCheckPositions();var off=ReplayUiCapture(samples);
                ReplayUiExtension(true,at);var row=new ReplayUiCase{samples=samples,width=Screen.width,height=Screen.height,seconds=at};
                row.bounds=ReplayUiAddedBounds(prior,ui.ReplayCheckPositions(),out row.addedVertices);row.bottomRow=Mathf.FloorToInt(row.bounds.w);
                var on=ReplayUiCapture(samples);ReplayUiExtension(false,at);var restored=ReplayUiCapture(samples);
                try {
                    var a=off.GetPixels32();var b=on.GetPixels32();var c=restored.GetPixels32();row.restored=true;
                    for(int i=0;i<a.Length;++i){if(!ReplayUiPixelEqual(a[i],b[i]))++row.changedPixels;if(!ReplayUiPixelEqual(a[i],c[i]))row.restored=false;}
                    int y=Screen.height-1-row.bottomRow;
                    if(y<0||y>=Screen.height)throw new InvalidOperationException("Extension edge outside frame");
                    for(int x=Math.Max(0,Mathf.FloorToInt(row.bounds.x));x<Math.Min(Screen.width,Mathf.CeilToInt(row.bounds.z));++x)
                        if(!ReplayUiPixelEqual(a[y*Screen.width+x],b[y*Screen.width+x]))++row.edgePixels;
                    string stem=Path.Combine(proofDirectory,"case-"+rows.Count+"-msaa-"+samples);
                    File.WriteAllBytes(stem+"-off.png",off.EncodeToPNG());File.WriteAllBytes(stem+"-on.png",on.EncodeToPNG());
                    rows.Add(row);
                    if(row.changedPixels<200||row.edgePixels!=0||!row.restored)throw new InvalidOperationException("Replay extension missing, has stray edge pixels, or repaint changed the scene: case "+(rows.Count-1));
                } finally {Destroy(off);Destroy(on);Destroy(restored);}
            }
            report.passed=true;
        } catch(Exception e){report.error=e.ToString();Debug.LogError(e);}
        report.cases=rows.ToArray();Directory.CreateDirectory(proofDirectory);
        File.WriteAllText(Path.Combine(proofDirectory,"report.json"),JsonUtility.ToJson(report,true));
        Debug.Log("Replay UI checks passed="+report.passed+" cases="+rows.Count);Application.Quit(report.passed?0:1);
    }
}
