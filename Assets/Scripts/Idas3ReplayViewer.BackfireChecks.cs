using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public sealed partial class Idas3ReplayViewer {
    bool replayBackfireCheckStarted;
    [Serializable] sealed class ReplayBackfireCase {public double seconds;public int phase,samples,changedPixels;public bool opponentPov,restored;}
    [Serializable] sealed class ReplayBackfireReport {public bool passed;public string error;public ReplayBackfireCase[] cases;}
    void CheckReplayBackfire(){
        if(Directory.Exists(proofDirectory)||File.Exists(proofDirectory)){Debug.LogError("Use NEW backfire evidence directory");Application.Quit(1);return;}
        var report=new ReplayBackfireReport();var rows=new List<ReplayBackfireCase>();
        try{
            if(AudioListener.volume!=0||!replay.Detailed)throw new InvalidOperationException("Muted detailed replay required");
            var source=Array.Exists(replay.Frames,p=>p.backfire>0)?replay:replay.Opponent;
            if(source==null)throw new InvalidDataException("No accepted flash in recording");
            int first=Array.FindIndex(source.Frames,p=>p.backfire==1);
            if(first<0||first+3>=source.Frames.Length)throw new InvalidDataException("Missing complete recorded flash");
            var times=new[]{source.Frames[first].tick/60d,source.Frames[first+1].tick/60d,source.Frames[first+2].tick/60d,source.Frames[0].tick/60d,source.Frames[first].tick/60d};
            Directory.CreateDirectory(proofDirectory);playing=false;controlsVisible=false;
            foreach(int pov in CanSwitchPov?new[]{0,1}:new[]{0}){
                if(pov==1)SwitchPov();
                foreach(int samples in new[]{1,4})foreach(double at in times){
                    var anchor=source.Sample(at);var forward=new Vector3(Mathf.Sin(anchor.yaw),0,Mathf.Cos(anchor.yaw));
                    var right=new Vector3(forward.z,0,-forward.x);
                    cameraMode=4;freePosition=anchor.position-forward*5+right*3+Vector3.up*1.5f;
                    var angles=Quaternion.LookRotation(anchor.position+Vector3.up*.6f-freePosition,Vector3.up).eulerAngles;
                    freeYaw=angles.y;freePitch=angles.x;
                    Seek(at);Present();var on=ReplayUiCapture(samples);
                    if(Idas3ReplayBackfire(-1,-1)!=1)throw new InvalidOperationException(Idas3Native.Error());
                    var p=Viewed.Sample(seconds);
                    if(Idas3ReplayPose(p.tick,p.position.x,p.position.y,p.position.z,p.yaw,p.speed,p.gear,0,cameraMode,orbit,Screen.width,Screen.height)!=1)throw new InvalidOperationException(Idas3Native.Error());
                    scene.ApplyFrame();ui.ApplyFrame();var off=ReplayUiCapture(samples);
                    Present();var restored=ReplayUiCapture(samples);
                    try{
                        var a=on.GetPixels32();var b=off.GetPixels32();var c=restored.GetPixels32();
                        var row=new ReplayBackfireCase{seconds=at,phase=(int)anchor.backfire-1,samples=samples,opponentPov=opponentPov,restored=true};
                        for(int i=0;i<a.Length;i++){if(!ReplayUiPixelEqual(a[i],b[i]))row.changedPixels++;if(!ReplayUiPixelEqual(a[i],c[i]))row.restored=false;}
                        string stem=Path.Combine(proofDirectory,"backfire-"+rows.Count);
                        File.WriteAllBytes(stem+"-on.png",on.EncodeToPNG());File.WriteAllBytes(stem+"-off.png",off.EncodeToPNG());
                        rows.Add(row);
                        if(!row.restored||(row.phase>=0?row.changedPixels==0:row.changedPixels!=0))throw new InvalidOperationException("Recorded flash absent, stale, or changed on repaint");
                    }finally{Destroy(on);Destroy(off);Destroy(restored);}
                }
            }
            report.passed=true;
        }catch(Exception e){report.error=e.ToString();Debug.LogError(e);}
        report.cases=rows.ToArray();Directory.CreateDirectory(proofDirectory);
        File.WriteAllText(Path.Combine(proofDirectory,"report.json"),JsonUtility.ToJson(report,true));
        Application.Quit(report.passed?0:1);
    }
}
