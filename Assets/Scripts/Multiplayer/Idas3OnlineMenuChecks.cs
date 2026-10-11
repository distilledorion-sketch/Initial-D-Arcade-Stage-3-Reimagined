using System;
using System.IO;
using UnityEngine;
namespace Idas3.Multiplayer {
public static class Idas3OnlineMenuChecks {
    public static void Run(){
        Idas3OnlineUsernameChecks.Run();
        int checks=0;void Check(bool v,string s){++checks;if(!v)throw new Exception(s);}
        Check(Idas3LobbyNames.ForHost(" Chris ")=="Chris Lobby","Host lobby name");
        Check(Idas3LobbyNames.ForHost(null)=="Driver Lobby","Missing host name");
        Check(!Idas3LobbyNames.ForHost("<b>Chris\n").Contains("<"),"Lobby markup/control sanitizing");
        for(int count=0;count<40;++count){float size=Idas3MultiplayerMenu.ScrollThumbHeight(198,198,count*66);
            Check(count<=3?size==198:size<198,"Scrollbar overflow threshold");Check(size>=24&&size<=198,"Bounded thumb");}
        var a=new Idas3OnlineActivity{Available=true,Online=2,Racing=1,Queuing=1};
        Check(a.Format(a.Online)=="2","Normal count");a.Limited=true;Check(a.Format(2)=="2+","Limited population survey never displayed as exact");
        Check(default(Idas3OnlineActivity).Format(0)=="—","Unavailable never shown as zero");
        a.ObservedAt=100;
        var report=Idas3CommunityTimes.PrepareActivity(a,100.2,-1);
        Check(report!=null&&report.online==2&&report.queuing==1&&report.racing==1&&report.limited&&report.age==1,"Website report preserves counts, truncation and conservative sample age");
        Check(Idas3CommunityTimes.PrepareActivity(a,131,-1)==null,"Website rejects expired survey");
        Check(Idas3CommunityTimes.PrepareActivity(a,99,-1)==null,"Website rejects future survey");
        Check(Idas3CommunityTimes.PrepareActivity(a,101,100)==null,"Website does not republish one survey");
        Check(Idas3CommunityTimes.PrepareActivity(default,101,-1)==null,"Website never reports unknown as zero");
        var empty=new Idas3OnlineActivity{Available=true,ObservedAt=100};
        Check(Idas3CommunityTimes.PrepareActivity(empty,101,-1)?.online==0,"Successful empty EOS survey remains zero");
        var full=new Idas3OnlineActivity{Available=true,Online=200,Queuing=50,Racing=100,Limited=true,ObservedAt=100};
        Check(Idas3CommunityTimes.PrepareActivity(full,101,-1)?.online==200,"EOS search cap is preserved for website");
        full.Online=201;Check(Idas3CommunityTimes.PrepareActivity(full,101,-1)==null,"Invalid EOS count rejected");
        full.Online=100;Check(Idas3CommunityTimes.PrepareActivity(full,101,-1)==null,"Subcounts cannot exceed online count");
        Check(!default(Idas3OnlineActivity).Available,"Room membership must not fabricate a global population count");
        var entries=new[]{
            new Idas3EosActivity.Entry{Owner="one",State="online",At=980},
            new Idas3EosActivity.Entry{Owner="one",State="racing",At=999},
            new Idas3EosActivity.Entry{Owner="two",State="queuing",At=999},
            new Idas3EosActivity.Entry{Owner="stale",State="online",At=909},
            new Idas3EosActivity.Entry{Owner="future",State="online",At=1031},
            new Idas3EosActivity.Entry{Owner="bad-state",State="invalid",At=999},
            new Idas3EosActivity.Entry{Owner="",State="online",At=999}};
        var census=Idas3EosActivity.Summarize(entries,7,1000,25);
        Check(census.Available&&census.Online==2&&census.Queuing==1&&census.Racing==1&&!census.Limited&&census.ObservedAt==25,"EOS counts newest state once per authenticated owner and drops invalid or stale entries");
        Check(Idas3EosActivity.Summarize(entries,200,1000,25).Limited,"EOS raw result cap is shown even after deduplication");
        Check(Idas3EosActivity.Summarize(Array.Empty<Idas3EosActivity.Entry>(),0,1000,25).Available,"Successful empty EOS search is available");
        var crowded=new Idas3EosActivity.Entry[205];
        for(int i=0;i<crowded.Length;++i)crowded[i]=new Idas3EosActivity.Entry{Owner="driver-"+i,State="online",At=999};
        census=Idas3EosActivity.Summarize(crowded,205,1000,25);
        Check(census.Online==200&&census.Limited,"EOS count remains bounded for aggregate reporting");
        Check(Idas3OnlineActivity.StateFor(false,"Offline",false,false,false,false)=="online","Menus are online");
        Check(Idas3OnlineActivity.StateFor(false,"Matching",true,false,false,false)=="queuing","Searching counts as queuing");
        Check(Idas3OnlineActivity.StateFor(false,"Lobby",false,true,false,false)=="queuing","Waiting manual host counts as queuing");
        Check(Idas3OnlineActivity.StateFor(true,"Race",false,true,true,false)=="racing","Running race counts as racing");
        Check(Idas3OnlineActivity.StateFor(true,"Results",false,true,true,false)=="online","Results are not racing");
        Check(Idas3OnlineActivity.StateFor(true,"Returning",false,true,true,false)=="online","Rematch wait is not racing");
        Check(Idas3OnlineActivity.StateFor(true,"Disconnected",false,false,false,true)=="online","Neutral disconnect is not racing");
        Directory.CreateDirectory("Verification/online-menu-20260922");File.WriteAllText("Verification/online-menu-20260922/unit-checks.txt","PASS "+checks+" online activity, lobby names and scrollbar checks\n");
        Idas3ControllerMenuChecks.Run();
    }
}}
