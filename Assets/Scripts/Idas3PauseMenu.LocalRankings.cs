using System;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using Idas3.Multiplayer;

public sealed partial class Idas3PauseMenu
{
    private bool localRankingsOpen, localRankingsFailed;
    private int rankPage;
    private readonly List<int> rankPages=new List<int>();
    private readonly Dictionary<int,List<LocalTime>> localTimes=new Dictionary<int,List<LocalTime>>();
    private struct LocalTime { public string driver; public int car, ticks; }
    private const string RankGlyphs="アイウエオカキクケコサシスセソタチツテトナニヌネノハヒフヘホマミムメモヤユヨッラリルレロワヲンャュョァィゥェォーガギグゲゴザジズゼゾダヂヅデドバビブベボパピプペポあいうえおかきくけこさしすせそたちつてとなにぬねのはひふへほまみむめもやゆよっらりるれろわをんゃゅょぁぃぅぇぉーがぎぐげござじずぜぞだぢづでどばびぶべぼぱぴぷぺぽABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890<>+*‐=;:/\\_|・×○!?()@^〜 ";
    private bool UseLocalRankings=>!options.Current.communityTimes||!options.Draft.communityTimes;

    private void OpenRankings(){
        if(!UseLocalRankings){Application.OpenURL(Idas3CommunityTimes.ServiceUrl);return;}
        // Read the installation's own board even if an online snapshot is loaded.
        const int stride=9;
        int count=Idas3Native.Idas3CopyInstallationRankings(null,0);
        var packed=new int[checked(Math.Max(0,count)*stride)];
        if(count>0)count=Idas3Native.Idas3CopyInstallationRankings(packed,packed.Length);
        rankPage=0;rankPages.Clear();localTimes.Clear();localRankingsFailed=count<0;
        for(int i=0;i<count;i++){
            int p=i*stride,group=packed[p]*2+packed[p+1];
            if(!localTimes.TryGetValue(group,out var rows)){
                localTimes[group]=rows=new List<LocalTime>(10);rankPages.Add(group);
            }
            if(rows.Count>=10)continue;
            var name=new StringBuilder(5);
            for(int g=p+4;g<p+9&&packed[g]!=221;g++)
                if(packed[g]>=0&&packed[g]<RankGlyphs.Length)name.Append(RankGlyphs[packed[g]]);
            rows.Add(new LocalTime{driver=name.Length>0?name.ToString():"—",car=packed[p+2],ticks=packed[p+3]});
        }
        rankPages.Sort();localRankingsOpen=true;
    }
    private void ChangeRankPage(int delta){
        if(rankPages.Count>1&&delta!=0)rankPage=Wrap(rankPage+Math.Sign(delta),rankPages.Count);
    }
    private void LocalRankingsView(){
        Text(new Rect(34,121,900,42),"LOCAL LEADERBOARD",heading);
        if(rankPages.Count==0){
            Text(new Rect(36,200,950,55),localRankingsFailed?"Could not read local times.":"No Time Attack finishes on this installation yet.",wrapped);
        }else{
            int group=rankPages[rankPage],course=group/4;
            string route=Idas3MenuLocalization.T(Idas3CourseCatalog.DirectionLabel(course,(group&2)!=0));
            string weather=Idas3MenuLocalization.T(course==8?"SNOW":(group&1)!=0?"WET":"DRY");
            Text(new Rect(100,171,830,32),Idas3CourseCatalog.DisplayName(course)+"  /  "+route+"  /  "+weather,heading,translate:false);
            if(Button(new Rect(34,166,48,38),"‹",false,rankPages.Count>1))ChangeRankPage(-1);
            if(Button(new Rect(952,166,48,38),"›",false,rankPages.Count>1))ChangeRankPage(1);
            Text(new Rect(37,224,90,26),"RANK",small);
            Text(new Rect(137,224,220,26),"DRIVER",small);
            Text(new Rect(373,224,400,26),"CAR",small);
            Text(new Rect(800,224,190,26),"TIME",small);
            var rows=localTimes[group];
            for(int i=0;i<rows.Count;i++){
                var row=rows[i];float y=251+30*i;
                int ms=row.ticks/6;
                Text(new Rect(38,y,90,28),(i+1).ToString(),label,translate:false);
                Text(new Rect(137,y,220,28),row.driver,label,translate:false);
                Text(new Rect(373,y,420,28),Idas3MultiplayerSession.CarNames[row.car],label,translate:false);
                Text(new Rect(800,y,190,28),string.Format("{0}:{1:00}.{2:000}",ms/60000,ms/1000%60,ms%1000),label,translate:false);
            }
        }
        if(Button(new Rect(731,580,270,38),"BACK",true))localRankingsOpen=false;
    }
}
