using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEditor;
using UnityEngine;

// Exercises the production skinning and render path with real solver states.
public static class Idas3FlexibleOrnamentChecks
{
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static readonly MethodInfo Draw=typeof(Idas3OrnamentRenderer).GetMethod("RenderMotion",Private);
    static readonly PropertyInfo Attachment=typeof(Idas3OrnamentRenderer).GetProperty("ChainAttachment",Private);
    [Serializable] sealed class Report {
        public bool passed;public int checks,models,gpuFrames,rigChecks;
        public float visibleVerticalTravel;public string motion,view,interpolation,output,error;
        public List<string> captures=new List<string>();
    }
    static int checks;
    static void Check(bool value,string message){++checks;if(!value)throw new Exception("Flexible ornament: "+message);}
    static Vector3 CarPosition(int tick,int profile){
        float t=tick/60f,y=0,x=0,z=20*t;
        if(profile==1)x=t<1.2f?-3*t*t:-4.32f-7.2f*(t-1.2f);
        if(profile==2&&t>=.5f&&t<.9f)y=.13f*.5f*(1-Mathf.Cos((t-.5f)/.4f*Mathf.PI*2));
        if(profile==3){x=1.8f*Mathf.Sin(t*1.3f);y=.09f*Mathf.Sin(t*5)+.025f*Mathf.Sin(t*9);z+=1.8f*Mathf.Sin(t*1.5f);}
        return new Vector3(x,y,z);
    }
    static Idas3OrnamentMotion Pose(int tick,int profile){var motion=new Idas3OrnamentMotion();for(int t=0;t<=tick;++t)motion.Sample(CarPosition(t,profile),0,(ulong)t,true,false);return motion;}
    static Texture2D Read(Idas3OrnamentRenderer renderer,int id,Idas3OrnamentMotion motion,Texture2D pixels){
        var texture=Draw.Invoke(renderer,new object[]{id,motion}) as RenderTexture;
        Check(texture&&texture.IsCreated(),"missing dynamic GPU output for "+id);
        var previous=RenderTexture.active;
        try{RenderTexture.active=texture;pixels.ReadPixels(new Rect(0,0,texture.width,texture.height),0,0);pixels.Apply();}
        finally{RenderTexture.active=previous;}
        return pixels;
    }
    static void Pixels(Texture2D image,int id){
        int visible=0,edges=0;var pixels=image.GetPixels32();
        for(int y=0;y<image.height;++y)for(int x=0;x<image.width;++x){
            if(pixels[y*image.width+x].a<16)continue;++visible;
            if(x<2||y<2||x>=image.width-2||y>=image.height-2)++edges;
        }
        Check(visible>100&&visible<pixels.Length/2,"missing geometry or transparency for "+id);
        Check(edges==0,"dynamic ornament clips at viewport edge for "+id);
    }
    static void RigCheck(Idas3OrnamentRenderer renderer,int strapId){
        var rig=Idas3OrnamentChainRig.Load(strapId);Check(rig!=null,"missing source rig "+strapId);
        var skins=(IEnumerable)typeof(Idas3OrnamentRenderer).GetField("chainMeshes",Private).GetValue(renderer);int part=0;
        foreach(var skin in skins){
            var type=skin.GetType();var mesh=(Mesh)type.GetField("mesh",Private).GetValue(skin);
            var rest=(Vector3[])type.GetField("rest",Private).GetValue(skin);var posed=mesh.vertices;
            var bindJoints=(Vector3[])type.GetField("joints",Private).GetValue(skin);
            var posedJoints=(Vector3[])type.GetField("posedJoints",Private).GetValue(skin);
            for(int joint=rig.fixedJoint+1;joint<bindJoints.Length;++joint)
                Check(Mathf.Abs(Vector3.Distance(bindJoints[joint],bindJoints[joint-1])-Vector3.Distance(posedJoints[joint],posedJoints[joint-1]))<.00001f,"recovered link connection stretched");
            var first=new Dictionary<int,int>();var weights=rig.parts[part++].weights;
            for(int vertex=0;vertex<posed.Length;++vertex){
                Check(!float.IsNaN(posed[vertex].x)&&!float.IsNaN(posed[vertex].y)&&!float.IsNaN(posed[vertex].z),"nonfinite skinned chain vertex");
                int rigid=-1;for(int i=0;i<4;++i)if(weights[vertex].weights[i]>.99999f)rigid=weights[vertex].joints[i];
                if(rigid<0)continue;
                if(rigid==rig.fixedJoint)Check(Vector3.Distance(posed[vertex],rest[vertex])<.000001f,"top ring or fixed connector moved");
                if(first.TryGetValue(rigid,out var other))Check(Mathf.Abs(Vector3.Distance(rest[vertex],rest[other])-Vector3.Distance(posed[vertex],posed[other]))<.00001f,"a metal link deformed instead of rotating");
                else first.Add(rigid,vertex);
            }
            var end=(Vector3)type.GetProperty("AttachmentPosition",Private).GetValue(skin);
            Check(Vector3.Distance(end,(Vector3)Attachment.GetValue(renderer))<.000001f,"strap material parts disagree on pendant attachment");
        }
    }
    public static void VerifyOnly(){
        checks=0;var report=new Report();
        string output=Path.GetFullPath("Verification/flexible-ornaments-20260924/gpu-"+DateTime.UtcNow.ToString("HHmmss"));Directory.CreateDirectory(output);report.output=output;
        var pixels=new Texture2D(384,384,TextureFormat.RGBA32,false);
        try{
            Idas3HudCustomizationChecks.Run();
            report.motion=Idas3OrnamentMotionChecks.RunChecks();report.view=Idas3OrnamentMotionChecks.RunViewChecks();
            report.interpolation=Idas3OrnamentInterpolationChecks.RunChecks();Debug.Log(report.interpolation);
            Debug.Log(report.motion+" "+report.view);
            // Find the visibly lifted state using the real, rigid-link skin.
            int peakTick=0;float low=-.9f,high=-.9f;
            using(var renderer=new Idas3OrnamentRenderer()){
                var motion=new Idas3OrnamentMotion();
                for(int tick=0;tick<=240;++tick){
                    motion.Sample(CarPosition(tick,2),0,(ulong)tick,true,false);
                    Read(renderer,1,motion,pixels);++report.gpuFrames;
                    var end=(Vector3)Attachment.GetValue(renderer);low=Mathf.Min(low,end.y);
                    if(end.y>high){high=end.y;peakTick=tick;}
                    if(tick%3==0){string name="bounce-"+(tick/3).ToString("D3")+".png";File.WriteAllBytes(Path.Combine(output,name),pixels.EncodeToPNG());report.captures.Add(name);}
                }
                report.visibleVerticalTravel=high-low;
                Check(report.visibleVerticalTravel>.012f,"vertical road jolt did not lift and rebound the actual rendered chain: "+report.visibleVerticalTravel);
                RigCheck(renderer,1);++report.rigChecks;
            }
            var poses=new[]{new Idas3OrnamentMotion(),Pose(70,1),Pose(peakTick,2),Pose(150,3)};
            var checkedRigs=new HashSet<int>();
            using(var renderer=new Idas3OrnamentRenderer())for(int index=1;index<Idas3OrnamentCatalog.Count;++index){
                int id=Idas3OrnamentCatalog.IdAt(index);var entry=Idas3OrnamentCatalog.Get(id);
                for(int pose=0;pose<poses.Length;++pose){
                    Read(renderer,id,poses[pose],pixels);Pixels(pixels,id);++report.gpuFrames;
                    if(index==1||!checkedRigs.Contains(entry.strapId))File.WriteAllBytes(Path.Combine(output,"strap-"+entry.strapId+"-pose-"+pose+".png"),pixels.EncodeToPNG());
                }
                if(checkedRigs.Add(entry.strapId)){RigCheck(renderer,entry.strapId);++report.rigChecks;}
                ++report.models;
            }
            Check(report.models==280&&checkedRigs.Count==4,"dynamic catalog coverage incomplete");
            using(var renderer=new Idas3OrnamentRenderer()){
                var state=Pose(35,3);var interpolate=typeof(Idas3OrnamentRenderer).GetMethod("RenderInterpolatedMotion",Private);
                Vector3 first=Vector3.zero,last=Vector3.zero;
                foreach(float alpha in new[]{0f,.25f,.5f,.75f,1f}){
                    var texture=interpolate.Invoke(renderer,new object[]{1,state,alpha}) as RenderTexture;
                    Check(texture&&texture.IsCreated(),"subframe renderer has no output");
                    var previous=RenderTexture.active;
                    try{RenderTexture.active=texture;pixels.ReadPixels(new Rect(0,0,384,384),0,0);pixels.Apply();}
                    finally{RenderTexture.active=previous;}
                    Pixels(pixels,1);RigCheck(renderer,1);++report.gpuFrames;
                    var end=(Vector3)Attachment.GetValue(renderer);if(alpha==0)first=end;last=end;
                    File.WriteAllBytes(Path.Combine(output,"subframe-"+Mathf.RoundToInt(alpha*100)+".png"),pixels.EncodeToPNG());
                }
                Check(Vector3.Distance(first,last)>.00001f,"skinned chain is stuck between simulation ticks");
            }
            report.passed=true;
        }catch(Exception error){report.error=error.ToString();throw;}
        finally{
            UnityEngine.Object.DestroyImmediate(pixels);report.checks=checks;
            File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(report,true));
        }
        Debug.Log("Flexible ornament GPU PASS: "+report.models+" models, "+report.gpuFrames+" frames; actual vertical travel "+report.visibleVerticalTravel+"; "+checks+" checks. "+output);
    }
    public static void VerifyAndBuild(){VerifyOnly();Idas3Build.RebuildWindowsPlayer();}

    // Presentation capture using the production solver and skinning. The path
    // is synthetic, with a changing slip angle; it is not a recorded replay.
    public static void ExportDriftPreview(){
        string output=Path.GetFullPath("Verification/flexible-ornaments-20260924/drift-preview");
        Directory.CreateDirectory(output);
        var pixels=new Texture2D(384,384,TextureFormat.RGBA32,false);
        var samples=new System.Text.StringBuilder("frame,time,turn,heading,bodyYaw,attachmentX,attachmentY,pendantRoll\n");
        var motion=new Idas3OrnamentMotion();
        Vector3 position=Vector3.zero;
        float heading=0,minimum=0,maximum=0;
        try{
            using(var renderer=new Idas3OrnamentRenderer())for(int tick=0;tick<=480;++tick){
                float t=tick/60f;
                float turn=t<1?0:t<1.35f?Mathf.SmoothStep(0,1,(t-1)/.35f):t<2.6f?1:
                    t<3.4f?Mathf.Lerp(1,-1,Mathf.SmoothStep(0,1,(t-2.6f)/.8f)):t<4.8f?-1:
                    t<5.3f?Mathf.SmoothStep(-1,0,(t-4.8f)/.5f):0;
                float nextHeading=heading+turn*.36f/60f;
                if(tick>0){float mid=(heading+nextHeading)*.5f;position+=new Vector3(Mathf.Sin(mid),0,Mathf.Cos(mid))*(24f/60f);}
                heading=nextHeading;
                float bodyYaw=heading+turn*.22f;
                motion.Sample(position,bodyYaw,(ulong)tick,true,false);
                if(tick%3!=0)continue;
                Read(renderer,1,motion,pixels);Pixels(pixels,1);
                var end=(Vector3)Attachment.GetValue(renderer);
                minimum=Mathf.Min(minimum,end.x);maximum=Mathf.Max(maximum,end.x);
                string name="drift-"+(tick/3).ToString("D3")+".png";
                File.WriteAllBytes(Path.Combine(output,name),pixels.EncodeToPNG());
                samples.AppendLine(string.Format(System.Globalization.CultureInfo.InvariantCulture,"{0},{1:F3},{2:F4},{3:F4},{4:F4},{5:F6},{6:F6},{7:F4}",tick/3,t,turn,heading,bodyYaw,end.x,end.y,motion.RollDegrees));
            }
            Check(minimum<-.02f&&maximum>.02f,"drift capture did not swing both directions");
            File.WriteAllText(Path.Combine(output,"samples.csv"),samples.ToString());
            File.WriteAllText(Path.Combine(output,"README.txt"),"Actual production ornament solver and recovered mesh renderer. Synthetic 24 m/s left/right drift path with up to 0.36 rad/s path turn rate and 0.22 rad body slip angle. 161 frames at 20 fps, including straightening and settling. Positive turn is native-screen left. No runtime physics changes or rebuild.\n");
            Debug.Log("Drift preview PASS: actual attachment X "+minimum+" to "+maximum+"; "+output);
        }finally{UnityEngine.Object.DestroyImmediate(pixels);}
    }
}
