using System;
using UnityEngine;

// Called only by the explicit private-save update diagnostic.
internal static class Idas3UpdateChecks
{
    internal static string Fixture(string tag){
        string file="Initial-D-Arcade-Stage-3-Reimagined-test-Windows-x64.zip";
        return JsonUtility.ToJson(new Idas3Updates.Release{tag_name=tag,html_url=Idas3Updates.RepositoryUrl+"/releases/tag/"+Uri.EscapeDataString(tag),
            assets=new[]{new Idas3Updates.Asset{name=file,state="uploaded",size=123,digest="sha256:"+new string('a',64),
                browser_download_url=Idas3Updates.RepositoryUrl+"/releases/download/"+Uri.EscapeDataString(tag)+"/"+file}}});
    }
    internal static void Run(Action<bool,string> check){
        foreach(var pair in new[]{
            new[]{"v0.3.95-community-replays.10","0.3.95-community-replays.9"},
            new[]{"0.3.96","0.3.95-community-replays.999"},
            new[]{"0.3.95","0.3.95-community-replays.6"},
            new[]{"1.0.0","0.99.99"},new[]{"1.0.0-beta","1.0.0-alpha"},
            new[]{"1.0.0-rc.1","1.0.0-rc"},new[]{"1.0.0-alpha","1.0.0-9"}}){
            check(Idas3Updates.TryCompareVersions(pair[0],pair[1],out int newer)&&newer>0,"Newer version: "+pair[0]);
            check(Idas3Updates.TryCompareVersions(pair[1],pair[0],out int older)&&older<0,"Older version: "+pair[1]);
        }
        check(Idas3Updates.TryCompareVersions("v0.3.95-community-replays.6+abc","0.3.95-community-replays.6+xyz",out int equal)&&equal==0,"Version prefix and metadata ignored");
        foreach(string bad in new[]{"","latest","0.3","01.3.4","1.0.0-alpha.01","1.0.0-","1.0.0\n",new string('9',100)})
            check(!Idas3Updates.TryCompareVersions(bad,"1.0.0",out _),"Invalid version rejected");
        const string current="0.3.95-community-replays.6",next="v0.3.95-community-replays.7";
        var json=Fixture(next);
        check(Idas3Updates.Evaluate(current,200,json).state==Idas3Updates.CheckState.Available,"New Windows build offered");
        check(Idas3Updates.Evaluate(current,200,Fixture("v"+current)).state==Idas3Updates.CheckState.Current,"Same version is current");
        check(Idas3Updates.Evaluate(current,200,Fixture("v0.3.95-community-replays.5")).state==Idas3Updates.CheckState.Current,"Do not offer downgrade");
        foreach(long code in new long[]{0,403,404,429,500})check(Idas3Updates.Evaluate(current,code,json).state==Idas3Updates.CheckState.Unavailable,"HTTP/network failure "+code);
        check(Idas3Updates.Evaluate(current,200,json,true).state==Idas3Updates.CheckState.Unavailable,"Incomplete response rejected");
        foreach(string bad in new[]{"{","{}","null",new string(' ',Idas3Updates.MaximumResponseBytes+1)})
            check(Idas3Updates.Evaluate(current,200,bad).state==Idas3Updates.CheckState.Unavailable,"Malformed response rejected");
        Action<Action<Idas3Updates.Release>,string> reject=(change,reason)=>{
            var release=JsonUtility.FromJson<Idas3Updates.Release>(json);change(release);
            check(Idas3Updates.Evaluate(current,200,JsonUtility.ToJson(release)).state==Idas3Updates.CheckState.Unavailable,reason);
        };
        reject(r=>r.draft=true,"Draft ignored");reject(r=>r.prerelease=true,"Preview ignored");
        reject(r=>r.html_url="https://example.com/update","Foreign release link rejected");
        reject(r=>r.html_url=Idas3Updates.RepositoryUrl+"/releases/tag/v0.0.1","Mismatched release link rejected");
        reject(r=>r.assets=null,"Source-only release ignored");
        reject(r=>r.assets[0].state="new","Incomplete upload ignored");
        reject(r=>r.assets[0].size=0,"Empty download ignored");
        reject(r=>r.assets[0].digest=null,"Missing checksum rejected");
        reject(r=>r.assets[0].digest="sha256:not-a-hash","Invalid checksum rejected");
        reject(r=>r.assets[0].browser_download_url="file:///C:/example.exe","Unexpected asset URL rejected");
        reject(r=>r.assets[0].name="Source-Code.zip","Wrong platform asset ignored");
    }
}
