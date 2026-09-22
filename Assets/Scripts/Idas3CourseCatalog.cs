using System;
using System.IO;
using UnityEngine;

// Course identities are stable across saves, replays, matchmaking and the board.
public static class Idas3CourseCatalog
{
    public static readonly string[] Names={"Myogi","Usui","Akagi","Akina","Happogahara","Irohazaka","Shomaru","Tsuchisaka","Akina Snow","Hakone","Sadamine","Enna Skyline","Myogi (Special Stage)","Usui (Special Stage)","Momiji Line"};
    public static readonly string[] Packs={"HAKONE","SADAMINE","ENNA","MYOGI_SPECIAL","USUI_SPECIAL","MOMIJI"};
    public static readonly string[] Slugs={"hakone","sadamine","enna","myogi_special","usui_special","momiji"};
    public static int Count=>Names.Length;
    public static int ConditionCount=>Count*2;
    internal static string SceneName(Idas3Native.Status status){
        // Imported races expose a legacy owner index in status.course; flags identify the actual map.
        int course=(status.flags&IdasSpecialStageEnnaCourse.SceneFlag)!=0?IdasSpecialStageEnnaCourse.CourseId(status.flags):
            (status.flags&16384u)!=0?((status.flags&524288u)!=0?10:9):Mathf.Clamp(status.course,0,8);
        return Names[course];
    }
    public static bool RequiresNight(int course)=>course==4||course==8||course>=11&&course<Count;
    public static string DirectionToken(int course,bool reverse){
        if(course<0||course>=Count)throw new ArgumentOutOfRangeException(nameof(course));
        if(course<2)return reverse?"cw":"ccw";
        if(course==4||course==6||course==7)return reverse?"ib":"ob";
        if(course==5&&reverse)return "rev";
        return reverse?"uh":"dh";
    }
    public static bool Available(int course)=>course>=0&&course<Count&&(course<9||File.Exists(Path.Combine(Application.streamingAssetsPath,Packs[course-9],"menu.idastex")));
    public static int NextAvailable(int course,int direction){
        for(int i=0;i<Count;++i){course=(course+(direction<0?Count-1:1))%Count;if(Available(course))return course;}
        throw new InvalidOperationException("No courses installed");
    }
}
