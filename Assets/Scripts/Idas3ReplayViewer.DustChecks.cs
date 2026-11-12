using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public sealed partial class Idas3ReplayViewer {
    bool replayDustCheckStarted;
    [Serializable] sealed class ReplayDustCase { public double seconds;public int samples,changedPixels;public bool restored,opponentPov; }
    [Serializable] sealed class ReplayDustReport { public bool passed;public string error,device;public ReplayDustCase[] cases; }
    [Serializable] sealed class ReplayEffectProofCamera { public Vector3 eye,target; }
    [Serializable] sealed class ReplayEffectProofTimes { public double[] seconds; }
    void CheckReplayDust(){
        if(Directory.Exists(proofDirectory)||File.Exists(proofDirectory)){
            Debug.LogError("Use a NEW replay dust proof directory");Application.Quit(1);return;
        }
        var report=new ReplayDustReport{device=SystemInfo.graphicsDeviceName+" / "+SystemInfo.graphicsDeviceType};
        var rows=new List<ReplayDustCase>();
        try{
            bool combined=replay.SmokeFrames.Length>0;
            if(AudioListener.volume!=0||(!combined&&replay.DustFrames.Length==0))throw new InvalidOperationException("Muted extended replay required");
            Directory.CreateDirectory(proofDirectory);playing=false;controlsVisible=false;cameraMode=0;
            // Optional native-fixture camera, only inside this explicit audit.
            // Compare tiny clouds without a chase/orbit camera inside scenery.
            var args=Environment.GetCommandLineArgs();
            double[] requestedTimes=null;
            for(int i=0;i+1<args.Length;i++)if(args[i]=="-idas3-replay-proof-times"){
                requestedTimes=JsonUtility.FromJson<ReplayEffectProofTimes>(File.ReadAllText(args[i+1]))?.seconds;
                if(requestedTimes==null||requestedTimes.Length<2||requestedTimes.Length>12)
                    throw new InvalidDataException("Two to twelve explicit effect proof times required");
                foreach(double at in requestedTimes)if(double.IsNaN(at)||double.IsInfinity(at)||at<0)
                    throw new InvalidDataException("Invalid effect proof time");
            }
            for(int i=0;i+1<args.Length;i++)if(args[i]=="-idas3-replay-proof-camera"){
                var view=JsonUtility.FromJson<ReplayEffectProofCamera>(File.ReadAllText(args[i+1]));
                if(view==null||(view.target-view.eye).sqrMagnitude<.001f)throw new InvalidDataException("Invalid effect proof camera");
                cameraMode=4;freePosition=view.eye;var angles=Quaternion.LookRotation(view.target-view.eye,Vector3.up).eulerAngles;
                freeYaw=angles.y;freePitch=angles.x;
            }
            foreach(int pov in CanSwitchPov?new[]{0,1}:new[]{0}){
                if(pov==1)SwitchPov();
                // The night fixture uses real sharp steering after five
                // seconds. Its earlier straight driving emits no white smoke.
                var times=requestedTimes??(combined&&replay.Metadata.night!=0?new[]{313d/60,5.3,313d/60}:new[]{1d,3d,1d});
                if(requestedTimes!=null)foreach(double at in times)if(at>Viewed.Duration)
                    throw new InvalidDataException("Effect proof time exceeds the viewed recording");
                foreach(int samples in new[]{1,4})foreach(double at in times){
                    Seek(at);Present();var on=ReplayUiCapture(samples);
                    if((combined?Idas3ReplaySmokeSeek(0):Idas3ReplayDustSeek(0))!=1)throw new InvalidOperationException(Idas3Native.Error());
                    var p=Viewed.Sample(seconds);
                    if(Idas3ReplayPose(p.tick,p.position.x,p.position.y,p.position.z,p.yaw,p.speed,p.gear,0,cameraMode,orbit,Screen.width,Screen.height)!=1)
                        throw new InvalidOperationException(Idas3Native.Error());
                    scene.ApplyFrame();ui.ApplyFrame();var off=ReplayUiCapture(samples);
                    Present();var restored=ReplayUiCapture(samples);
                    try{
                        var a=on.GetPixels32();var b=off.GetPixels32();var c=restored.GetPixels32();
                        var row=new ReplayDustCase{seconds=seconds,samples=samples,restored=true,opponentPov=opponentPov};
                        for(int i=0;i<a.Length;i++){
                            if(!ReplayUiPixelEqual(a[i],b[i]))++row.changedPixels;
                            if(!ReplayUiPixelEqual(a[i],c[i]))row.restored=false;
                        }
                        string stem=Path.Combine(proofDirectory,"dust-"+rows.Count);
                        File.WriteAllBytes(stem+"-on.png",on.EncodeToPNG());File.WriteAllBytes(stem+"-off.png",off.EncodeToPNG());
                        rows.Add(row);if(!row.restored)throw new InvalidOperationException("Replay repaint/seek changed restored pixels");
                    }finally{Destroy(on);Destroy(off);Destroy(restored);}
                }
            }
            int minimum=combined&&(replay.Metadata.night!=0||replay.Metadata.weather!=0)?0:100;
            if(!rows.Exists(row=>row.changedPixels>minimum))throw new InvalidOperationException("Recorded source effects absent from Unity viewer");
            report.passed=true;
        }catch(Exception e){report.error=e.ToString();Debug.LogError(e);}
        report.cases=rows.ToArray();Directory.CreateDirectory(proofDirectory);
        File.WriteAllText(Path.Combine(proofDirectory,"report.json"),JsonUtility.ToJson(report,true));
        Debug.Log("Replay dust checks passed="+report.passed+" cases="+rows.Count);Application.Quit(report.passed?0:1);
    }
}
