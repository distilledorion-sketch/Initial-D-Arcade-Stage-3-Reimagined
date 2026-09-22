using System;
using System.Text;
using UnityEngine;

internal static class Idas3DiscordChecks
{
    internal static void Run(Action<bool,string> check)
    {
        var s=new Idas3DiscordPresence.Snapshot{condition=6,car=0,night=1,weather=1,ticks6000=804000};
        var status=new Idas3Native.Status{racePhase=2};
        var ta=Idas3DiscordPresence.Describe(s,status,false,false);
        check(ta.details=="Time Attack · Akina Downhill"&&ta.state.StartsWith("Night / Wet"),"Time Attack course and conditions");
        check(ta.elapsedSeconds==134&&ta.timed,"Elapsed clock uses native race time");
        string[] courseNames={"Myogi","Usui","Akagi","Akina","Happogahara","Irohazaka","Shomaru","Tsuchisaka","Akina Snow","Hakone","Sadamine","Enna Skyline","Myogi (Special Stage)","Usui (Special Stage)","Momiji Line"};
        string[] forward={"Counterclockwise","Counterclockwise","Downhill","Downhill","Outbound","Downhill","Outbound","Outbound","Downhill","Downhill","Downhill","Downhill","Downhill","Downhill","Downhill"};
        string[] reverse={"Clockwise","Clockwise","Uphill","Uphill","Inbound","Reverse","Inbound","Inbound","Uphill","Uphill","Uphill","Uphill","Uphill","Uphill","Uphill"};
        check(courseNames.Length==Idas3CourseCatalog.Count,"Presence checks cover every course");
        for(int course=0;course<courseNames.Length;++course)foreach(int direction in new[]{0,1}){
            s.condition=course*2+direction;var d=Idas3DiscordPresence.Describe(s,status,false,false);
            string expected=courseNames[course]+" "+(direction==0?forward[course]:reverse[course]);
            check(d.details=="Time Attack · "+expected,"Course "+course+" direction "+direction);
            var r=Idas3DiscordPresence.DescribeReplay(new Idas3ReplayData.Details{condition=s.condition,mode=0});
            check(r.state==expected+" · Day / Dry · Time Attack","Replay course "+course+" direction "+direction);
        }
        foreach(int condition in new[]{-1,Idas3CourseCatalog.ConditionCount}){
            s.condition=condition;check(Idas3DiscordPresence.Describe(s,status,false,false).details=="Time Attack · Choosing a course","Invalid course "+condition);
        }
        s.condition=0;check(Idas3DiscordPresence.Describe(s,status,false,false).details.EndsWith("Counterclockwise"),"Myogi forward direction");
        s.condition=19;check(Idas3DiscordPresence.Describe(s,status,false,false).details.EndsWith("Uphill"),"Hakone reverse direction");
        s.mode=1;s.opponentName="TAK";check(Idas3DiscordPresence.Describe(s,status,false,false).details=="Online Battle · vs TAK","Online opponent");
        s.mode=2;s.opponentName="Takumi";check(Idas3DiscordPresence.Describe(s,status,false,false).details=="Legend of the Streets · vs Takumi","Legend opponent");
        s.mode=3;check(Idas3DiscordPresence.Describe(s,status,false,false).details.StartsWith("Bunta Challenge"),"Bunta mode");
        status.flags=1;status.frontendStage=0;check(Idas3DiscordPresence.Describe(s,status,false,false).details=="At the title screen","Attract demo is a menu");
        for(int stage=1;stage<=12;stage++){status.frontendStage=stage;check(!Idas3DiscordPresence.Describe(s,status,false,false).timed,"Menu stage "+stage+" has no race timer");}
        check(Idas3DiscordPresence.Describe(s,status,true,false).session=="lobby","Online lobby");
        check(Idas3DiscordPresence.Describe(s,status,true,true).session=="matchmaking","Quick match");
        status.flags=262144;check(Idas3DiscordPresence.Describe(s,status,false,false).session=="results","Results clear timer");
        check(Idas3DiscordPresence.DescribeReplay(null).details=="Browsing replays","Replay library");
        var replay=new Idas3ReplayData.Details{condition=6,mode=1,playerName="CHRIS",opponentName="TAK",night=1,weather=1};
        var rd=Idas3DiscordPresence.DescribeReplay(replay);check(rd.details.Contains("CHRIS vs TAK")&&rd.state.Contains("Akina Downhill")&&!rd.timed,"Replay battle names and map");
        var p=Idas3DiscordPresence.Build(ta,DateTime.UtcNow);
        check(p.Assets.LargeImageKey==Idas3DiscordPresence.LogoUrl&&p.Assets.LargeImageText=="Initial D Arcade Stage 3","Approved logo and title");
        check(p.Buttons.Length==1&&p.Buttons[0].Label=="View Leaderboard"&&p.Secrets==null,"Leaderboard button without join secrets");
        rd.details=new string('\u754c',100);check(Encoding.UTF8.GetByteCount(Idas3DiscordPresence.Build(rd,DateTime.UtcNow).Details)<=128,"UTF8 presence limit");
        var options=new Idas3GameOptions.Values();var clone=options.Clone();clone.discordPresence=false;
        check(options.discordPresence&&!Idas3GameOptions.Equivalent(options,clone),"Presence preference participates in settings");
        JsonUtility.FromJsonOverwrite("{\"version\":1}",options);check(options.discordPresence,"Existing settings retain default presence");
        var actual=Idas3DiscordPresence.ReadSnapshot(new byte[2048]);check(actual!=null&&actual.car>=0&&actual.car<35,"Live native presence bridge");
    }
}
