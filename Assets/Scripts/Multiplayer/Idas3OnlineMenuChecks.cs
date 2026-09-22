using System;
using System.IO;
using UnityEngine;
namespace Idas3.Multiplayer {
public static class Idas3OnlineMenuChecks {
    public static void Run(){
        int checks=0;void Check(bool v,string s){++checks;if(!v)throw new Exception(s);}
        Check(Idas3LobbyNames.ForHost(" Chris ")=="Chris Lobby","Host lobby name");
        Check(Idas3LobbyNames.ForHost(null)=="Driver Lobby","Missing host name");
        Check(!Idas3LobbyNames.ForHost("<b>Chris\n").Contains("<"),"Lobby markup/control sanitizing");
        for(int count=0;count<40;++count){float size=Idas3MultiplayerMenu.ScrollThumbHeight(198,198,count*66);
            Check(count<=3?size==198:size<198,"Scrollbar overflow threshold");Check(size>=24&&size<=198,"Bounded thumb");}
        var samples=new[]{new Idas3SteamActivity.Sample{Owner=1,Seen=100,State="online"},new Idas3SteamActivity.Sample{Owner=1,Seen=101,State="racing"},new Idas3SteamActivity.Sample{Owner=2,Seen=105,State="queuing"},new Idas3SteamActivity.Sample{Owner=3,Seen=1,State="racing"},new Idas3SteamActivity.Sample{Owner=4,Seen=999,State="online"},new Idas3SteamActivity.Sample{Owner=0,Seen=110,State="online"},new Idas3SteamActivity.Sample{Owner=5,Seen=110,State="bogus"}};
        var a=Idas3SteamActivity.Aggregate(samples,110,false);
        Check(a.Available&&a.Online==2&&a.Racing==1&&a.Queuing==1,"Deduplication, expiry, invalid records, distinct states");
        Check(a.Format(a.Online)=="2","Normal count");a.Limited=true;Check(a.Format(2)=="2+","Truncated Steam result never exact");
        Check(default(Idas3OnlineActivity).Format(0)=="—","Unavailable never shown as zero");
        a=Idas3SteamActivity.Aggregate(Array.Empty<Idas3SteamActivity.Sample>(),110,false);Check(a.Available&&a.Online==0,"Successful empty result");
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
