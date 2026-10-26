using System;
using System.Collections;
using UnityEngine;

public sealed partial class Idas3SceneSmoke
{
    private bool transitionCheck;
    private Idas3TransitionProfile transitionProfiler;

    private IEnumerator TransitionReturnToCourse()
    {
        held=0;
        transitionProfiler.Section("pause-return");
        var pause=host.GetComponent<Idas3PauseMenu>();
        pause.SetOpen(true);yield return Frames(3);
        Check(pause.IsOpen,"Transition check could not open pause.");
        // Exercise the view's normal commands and the host's command owner.
        // Input routing itself is covered by the separate pause/controller suite.
        pause.Navigate(1);pause.Navigate(1);pause.Navigate(1);pause.Activate();
        pause.NavigateHorizontal(1);pause.Activate();yield return Frames(60);
        Check(!pause.IsOpen&&(host.Status.flags&1)!=0&&host.Status.frontendStage==6,
            "Transition pause command did not return to Course6.");
    }

    private IEnumerator TransitionCapture(string name,bool world)
    {
        transitionProfiler.Checkpoint(name);
        transitionProfiler.RestoreCameras();
        bool wasPerf=perfCheck;perfCheck=true;
        try{yield return Capture(name,world);}finally{perfCheck=wasPerf;}
    }

    private IEnumerator TransitionCycles()
    {
        QualitySettings.antiAliasing=4;QualitySettings.vSyncCount=0;Application.targetFrameRate=-1;
        hires=new RenderTexture(2560,1080,24,RenderTextureFormat.ARGB32){name="Transition diagnostic",antiAliasing=4};
        Check(hires.Create(),"Transition target creation failed.");
        host.GetComponent<Camera>().targetTexture=hires;yield return Frames(4);
        transitionProfiler=host.gameObject.AddComponent<Idas3TransitionProfile>();
        transitionProfiler.Initialize(host,hires,root);
        transitionProfiler.Section("initial-quick-start");
        yield return Key(116);
        int wait=0;while(host.Status.racePhase!=2&&wait++<1800)yield return null;
        Check(host.Status.racePhase==2,"Initial transition setup failed to start a race.");
        held=87;yield return Frames(120);held=0;
        yield return TransitionCapture("initial-akina",true);

        int[] courses={3,16,3,17,3,11,3,16,3,17,3,11,3};
        int cycles=DiagnosticInt(Environment.GetCommandLineArgs(),"-idas3-transition-cycles",1,1,3);
        for(int visit=0;visit<courses.Length*cycles;++visit){
            int course=courses[visit%courses.Length];
            yield return TransitionReturnToCourse();
            transitionProfiler.Section("select-"+course);
            for(int i=0;Idas3SceneModeFlowValue(30)!=course&&i<Idas3CourseCatalog.Count;++i){
                yield return Key(39);yield return Frames(6);
            }
            Check(Idas3SceneModeFlowValue(30)==course,"Transition carousel selected the wrong course.");
            yield return TransitionCapture("visit-"+visit.ToString("00")+"-menu-"+course,false);
            transitionProfiler.Section("start-"+course);
            for(int choice=0;(host.Status.flags&1)!=0&&choice<8;++choice){
                int stage=host.Status.frontendStage,conditions=Idas3SceneModeFlowValue(31);
                Check(stage>=6&&stage<=9,"Transition route reached an unexpected menu.");
                if(stage==7&&perfReverse){yield return Key(39);yield return Frames(6);}
                if((stage==8&&perfWet!=((conditions&1)!=0))||(stage==9&&!Idas3CourseCatalog.RequiresNight(course)&&perfNight!=((conditions&2)!=0))){
                    yield return Key(39);yield return Frames(6);
                }
                yield return Key(13);yield return Frames(60);
            }
            wait=0;while(((host.Status.flags&1)!=0||host.Status.racePhase!=2)&&wait++<1800)yield return null;
            Check(PerformanceCourse==course&&(host.Status.flags&1)==0&&host.Status.racePhase==2,
                "Transition did not start the selected race.");
            Check(Idas3SceneModeFlowValue(31)==((perfWet?1:0)|((perfNight||Idas3CourseCatalog.RequiresNight(course))?2:0)),"Transition weather/time differs from the request.");
            transitionProfiler.Section("drive-"+course);
            held=87;yield return Frames(240);held=0;
            Check(host.Status.speedMetresPerSecond>1,"Transition race failed to accelerate.");
            yield return TransitionCapture("visit-"+visit.ToString("00")+"-race-"+course,true);
        }
        yield return TransitionReturnToCourse();yield return Frames(60);
        yield return TransitionCapture("final-course-menu",false);
        Check(transitionProfiler.Failure==null,transitionProfiler.Failure);
        Finish(true,null);
    }
}
