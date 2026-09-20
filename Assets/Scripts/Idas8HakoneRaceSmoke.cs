using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

// Input/capture fixture only. Every step goes through Idas3SceneGame/App.
public sealed class Idas8HakoneRaceSmoke : MonoBehaviour
{
    static Idas8HakoneRaceSmoke active;Idas3SceneGame host;int frames,scenario;string output;
    static readonly string[] variants={"day_dry","day_wet","night_dry","night_wet"};
    bool menuMode;int menuOwner=-1;
    Vector3 start;float maximumSpeed,maximumRpm;
    void Start(){
        active=this;host=FindAnyObjectByType<Idas3SceneGame>();output=Path.Combine(Application.dataPath,"../Verification");Directory.CreateDirectory(output);
        start=host.GetComponent<Camera>().transform.position;
        var options=new Idas3Native.Options{size=(uint)Marshal.SizeOf<Idas3Native.Options>()};Idas3Native.Idas3SceneGetOptions(ref options);options.cameraView=1;
        Idas3Native.Idas3SceneApplyOptions(ref options);
        menuMode=Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-menu-smoke")>=0;
        if(menuMode){
            string pack=Path.Combine(Application.streamingAssetsPath,"HAKONE");
            if(Idas3Native.Idas3SceneStartImportedCourseConditions(pack,0,0,0)!=1||Idas3Native.Idas3SceneShowImportedCourseMenu(pack)!=1)throw new Exception(Idas3Native.Error());
        }else BeginScenario();
    }
    internal static void PrepareFrame(ref Idas3Native.FrameInput frame){
        if(active==null)return;
        frame=new Idas3Native.FrameInput{size=(uint)Marshal.SizeOf<Idas3Native.FrameInput>(),flags=1,deltaSeconds=.1};
        if(active.menuMode){
            bool menu=(active.host.Status.flags&1u)!=0;int owner=menu?active.host.Status.frontendStage:20;
            if(owner!=active.menuOwner){active.menuOwner=owner;active.frames=0;}
            if(menu){
                if(active.frames==4&&((active.scenario==1&&owner>=7&&owner<=9)||(active.scenario==2&&owner==6)))frame.SetKey(68);
                if(active.frames==8)frame.SetKey(13);
            }else frame.SetKey(87);
        }else frame.SetKey(87);
    }
    void LateUpdate(){
        try{
            if(!host.Ready||host.Failure!=null)throw new Exception(host.Failure??"D3 host not ready");
            if(menuMode){CheckMenu();return;}
            maximumSpeed=Mathf.Max(maximumSpeed,host.Status.speedMetresPerSecond);maximumRpm=Mathf.Max(maximumRpm,host.Status.rpm);
            frames++;
            if(frames==10&&scenario==1&&Array.IndexOf(Environment.GetCommandLineArgs(),"-hakone-menu-art")>=0)
                Capture("hakone-menu-background",true);
            if(frames==10){
                var source=host.GetComponent<Idas3SceneRenderer>();
                var words=new float[92];Marshal.Copy(source.CurrentFrame.frameConstants,words,0,words.Length);
                int lights=0;for(int i=14;i<22;i++)if(words[i*4+3]>0)lights++;
                if(lights!=(scenario>=4?8:0))throw new Exception("Streetlight night/day activation or eight-light bound failed");
                if(scenario==5||scenario==7){
                    var on=Capture("hakone-streetlights-on-"+variants[scenario/2]);
                    Color32[] off,carOff;
                    try{source.SetDiagnosticCourseLampVisibility(false);off=Capture("hakone-streetlights-off-"+variants[scenario/2]);
                        source.SetDiagnosticCourseLampVisibility(false,true);carOff=Capture("hakone-car-streetlights-off-"+variants[scenario/2]);}
                    finally{source.SetDiagnosticCourseLampVisibility(true);}
                    int total=Changed(on,off),car=Changed(on,carOff);
                    if(total<100||car<20)throw new Exception($"Streetlight pixels did not light road/car: {total}/{car}");
                    File.AppendAllText(Path.Combine(output,"d3-streetlights.txt"),$"PASS {variants[scenario/2]}: {lights} nearby lamps, {total} illuminated pixels, {car} car pixels changed\n");
                }
            }
            if(frames==10||frames==120)Capture("hakone-d3-"+variants[scenario/2]+"-"+(scenario%2==0?"down":"up")+"-"+frames);
            if(frames==120&&scenario>=4){
                var lamps=new System.Collections.Generic.List<MeshRenderer>();
                foreach(var renderer in FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None))
                    if(renderer.enabled&&renderer.sharedMaterial!=null&&!renderer.sharedMaterial.IsKeywordEnabled("IDAS_IMPORTED_COURSE")&&unchecked((uint)renderer.sharedMaterial.GetInteger("pcw"))==0x8a00071eu)lamps.Add(renderer);
                if(lamps.Count==0)throw new Exception("D3 projected headlights missing from night scene");
                try{foreach(var lamp in lamps)lamp.enabled=false;Capture("hakone-headlights-off-"+scenario);}
                finally{foreach(var lamp in lamps)lamp.enabled=true;}
            }
            if(frames<150)return;
            if(maximumSpeed<4||maximumRpm<2000||Vector3.Distance(start,host.GetComponent<Camera>().transform.position)<10)throw new Exception($"D3 car did not drive: {maximumSpeed} m/s, {maximumRpm} rpm");
            uint expected=16384u|((scenario%2==1)?32768u:0u)|(scenario>=4?65536u:0u)|((scenario/2%2==1)?131072u:0u);
            if((host.Status.flags&245760u)!=expected||FindAnyObjectByType<Idas8HakoneCourse>().LoadedVariant!=variants[scenario/2])throw new Exception("Condition state/scenery mismatch");
            File.AppendAllText(Path.Combine(output,"d3-weather.txt"),$"PASS {variants[scenario/2]} direction {scenario%2}: existing D3 host/solver, max speed {maximumSpeed:F2} m/s, RPM {maximumRpm:F0}, {host.GetComponent<Idas3UnityUi>().DrawCount} HUD draws\n");
            if(++scenario<8)BeginScenario();
            else {Debug.Log("HAKONE D3 WEATHER PASS");Application.Quit(0);enabled=false;active=null;}
        }catch(Exception e){Debug.LogException(e);Application.Quit(1);enabled=false;active=null;}
    }
    void CheckMenu(){
        frames++;
        bool menu=(host.Status.flags&1u)!=0;
        if(frames>120)throw new Exception("Hakone menu transition timed out at "+menuOwner);
        if(menu){
            if(frames==4&&scenario==0)Capture("hakone-menu-live-"+host.Status.frontendStage);
            return;
        }
        if(frames!=45)return;
        uint expected=scenario==0?16384u:scenario==1?245760u:0u;
        if((host.Status.flags&245760u)!=expected)throw new Exception("Menu launched wrong course/conditions: "+host.Status.flags);
        if(host.Status.speedMetresPerSecond<1)throw new Exception("Menu race did not drive");
        if(scenario<2&&FindAnyObjectByType<Idas8HakoneCourse>().LoadedVariant!=(scenario==0?"day_dry":"night_wet"))throw new Exception("Menu scenery mismatch");
        if(scenario==2&&host.Status.course!=0)throw new Exception("Original Myogi entry replaced");
        Capture("hakone-menu-race-"+scenario);
        File.AppendAllText(Path.Combine(output,"d3-menu.txt"),$"PASS menu scenario {scenario}: native selection, flags {host.Status.flags}, moving D3 car, return to course\n");
        if(Idas3Native.Idas3SceneReturnToCourse()!=1)throw new Exception(Idas3Native.Error());
        frames=0;menuOwner=-1;
        if(++scenario==3){Debug.Log("HAKONE MENU PASS");Application.Quit(0);enabled=false;active=null;}
    }
    void BeginScenario(){
        if(Idas3Native.Idas3SceneStartImportedCourseConditions(Path.Combine(Application.streamingAssetsPath,"HAKONE"),scenario%2,scenario>=4?1:0,scenario/2%2)!=1)throw new Exception(Idas3Native.Error());
        frames=0;maximumSpeed=maximumRpm=0;start=host.GetComponent<Camera>().transform.position;
    }
    static int Changed(Color32[] on,Color32[] off){
        int count=0;for(int i=0;i<on.Length;i++)if(Math.Max(Math.Abs(on[i].r-off[i].r),Math.Max(Math.Abs(on[i].g-off[i].g),Math.Abs(on[i].b-off[i].b)))>1)count++;
        return count;
    }
    Color32[] Capture(string name,bool sceneryOnly=false){
        var camera=host.GetComponent<Camera>();var rt=new RenderTexture(1280,720,24);var prior=RenderTexture.active;
        int mask=camera.cullingMask;if(sceneryOnly)camera.cullingMask=1<<28;
        var pixels=new Texture2D(1280,720,TextureFormat.RGB24,false);
        try{camera.targetTexture=rt;camera.Render();if(!sceneryOnly)host.GetComponent<Idas3UnityUi>().RenderOverlayForCapture();RenderTexture.active=rt;
            pixels.ReadPixels(new Rect(0,0,1280,720),0,0);pixels.Apply();File.WriteAllBytes(Path.Combine(output,name+".png"),pixels.EncodeToPNG());return pixels.GetPixels32();
        }finally{camera.cullingMask=mask;camera.targetTexture=null;RenderTexture.active=prior;rt.Release();Destroy(rt);Destroy(pixels);}
    }
}
