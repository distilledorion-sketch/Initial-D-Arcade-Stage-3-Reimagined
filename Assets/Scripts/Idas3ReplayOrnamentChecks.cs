using System;
using System.IO;
using UnityEngine;

// Isolated IDR2 fixture: no live saves or native game session.
internal static class Idas3ReplayOrnamentChecks {
    static Idas3ReplayData Replay(){
        using var stream=new MemoryStream();using var w=new BinaryWriter(stream);
        w.Write(0x32524449u);w.Write(60000u);w.Write(601u);w.Write(60u);
        w.Write(96u);w.Write(160u);w.Write(1u);w.Write(12u);for(int i=0;i<16;++i)w.Write(0u);
        for(uint i=0;i<=600;++i){
            float t=i/60f,x=4*Mathf.Sin(t*2),z=t*15,y=.1f*Mathf.Sin(t*3),yaw=.2f*Mathf.Cos(t*2);
            w.Write(i);w.Write(x);w.Write(y);w.Write(z);w.Write(yaw);w.Write(54f);w.Write(3);
            for(int n=0;n<33;++n){uint value=0;
                if(n==1)value=unchecked((uint)BitConverter.SingleToInt32Bits(x));
                if(n==2)value=unchecked((uint)BitConverter.SingleToInt32Bits(y+.5f));
                if(n==3)value=unchecked((uint)BitConverter.SingleToInt32Bits(z));
                if(n==17)value=i*100;if(n==24)value=4;w.Write(value);
            }
        }
        w.Flush();return Idas3ReplayData.Parse(Idas3ReplayLibrary.Package(new Idas3ReplayData.Details{ticks6000=60000},stream.ToArray(),Array.Empty<byte>()));
    }
    public static void Run(Action<bool,string> Check){
        var replay=Replay();Quaternion reference=default;Vector3 anchor=default;
        foreach(int fps in new[]{30,60,144,240}){
            var timeline=new Idas3ReplayOrnamentTimeline();var motion=new Idas3OrnamentMotion();
            for(int i=0;i<fps*8.5;++i)timeline.Update(motion,replay,i/(double)fps,0);
            float alpha=timeline.Update(motion,replay,8.5,0);var pose=motion.RenderPendantRotation(alpha);var end=motion.RenderChainPoint(8,alpha);
            Check(Quaternion.Angle(pose,Quaternion.identity)>1,"Replay ornament did not react to the recorded turns");
            if(fps==30){reference=pose;anchor=end;}else Check(Quaternion.Angle(reference,pose)<.05f&&Vector3.Distance(anchor,end)<.0001f,"Replay ornament depends on render FPS "+fps);
            for(int i=0;i<100;++i)alpha=timeline.Update(motion,replay,8.5,0);
            Check(Quaternion.Angle(pose,motion.RenderPendantRotation(alpha))<.05f&&Vector3.Distance(end,motion.RenderChainPoint(8,alpha))<.0001f,"Paused ornament keeps moving");
            alpha=timeline.Update(motion,replay,4.2,1);
            var fresh=new Idas3OrnamentMotion();var freshTime=new Idas3ReplayOrnamentTimeline();float freshAlpha=freshTime.Update(fresh,replay,4.2,1);
            Check(Quaternion.Angle(motion.RenderPendantRotation(alpha),fresh.RenderPendantRotation(freshAlpha))<.05f&&Vector3.Distance(motion.RenderChainPoint(8,alpha),fresh.RenderChainPoint(8,freshAlpha))<.0001f,"Seek retained stale ornament motion");
        }
    }
}
