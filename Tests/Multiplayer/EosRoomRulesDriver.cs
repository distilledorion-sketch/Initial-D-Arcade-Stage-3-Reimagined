using System;
using Idas3.Multiplayer;

internal static class EosRoomRulesDriver {
    static int checks;
    static void Check(bool condition,string reason){if(!condition)throw new Exception(reason);++checks;Console.WriteLine("PASS\t"+reason);}
    static string Validate(string local="local",string original="local",string owner="local",uint capacity=2,string[] members=null,string expected="build-1",string actual="build-1")=>
        Idas3EosRoomRules.ValidateMembership(local,original,owner,capacity,members??new[]{"local","remote"},expected,actual);
    static void Invalid(string value,string reason){Check(!string.IsNullOrEmpty(value),reason);}
    static int Main(){try{
        Check(Validate()==null,"A compatible two-player lobby admits its unchanged owner and local player");
        Check(Validate(members:new[]{"local"})==null,"A compatible empty host room remains valid while waiting");
        Check(Validate(original:null,owner:"remote")==null,"Initial guest admission accepts the actual member who owns the room");
        Invalid(Validate(capacity:1),"A one-player activity lobby cannot become a race room");
        Invalid(Validate(capacity:3),"A larger lobby cannot enter the two-player race protocol");
        Invalid(Validate(actual:"build-2"),"A different build cannot be admitted");
        Invalid(Validate(expected:""),"An unidentified local build cannot accept a room");
        Invalid(Validate(actual:"BUILD-1"),"Compatibility identity matching is ordinal and exact");
        Invalid(Validate(members:Array.Empty<string>()),"An empty membership snapshot cannot retain a route");
        Invalid(Validate(members:new[]{"local","remote","outsider"}),"A third participant cannot be admitted to a two-player route");
        Invalid(Validate(members:new[]{"local","local"}),"Duplicate product users do not count as two drivers");
        Invalid(Validate(members:new[]{"local",""}),"An empty remote identity cannot be admitted");
        Invalid(Validate(members:new[]{"local",null}),"A null remote identity cannot be admitted");
        Invalid(Validate(local:"removed",members:new[]{"local","remote"}),"Removed local membership cannot continue sending to the room");
        Invalid(Validate(owner:"outsider"),"An owner who is not in the member list cannot authorize a peer");
        Invalid(Validate(owner:""),"A missing owner cannot authorize a peer");
        Invalid(Validate(owner:"remote"),"Unexpected host migration cannot silently change the established race authority");
        Invalid(Idas3EosRoomRules.ValidateMembership("local","local","local",2,null,"build-1","build-1"),"A null membership snapshot is rejected");
        foreach(string[] pair in new[]{new[]{"ID3-A","ID3-B"},new[]{"ID3-3","ID3-1"},new[]{"ID3-X","ID3-Z"}}){
            bool a=Idas3EosRoomRules.ShouldMerge(pair[0],1,pair[1]);bool b=Idas3EosRoomRules.ShouldMerge(pair[1],1,pair[0]);
            Check(a!=b,"Crossing host searches choose exactly one merger for "+pair[0]+" and "+pair[1]);
            string retained=a?pair[1]:pair[0],moved=a?pair[0]:pair[1];
            Check(string.CompareOrdinal(retained,moved)<0,"The same lower room survives regardless of which host searches first");
            Check(!Idas3EosRoomRules.ShouldMerge(retained,2,moved),"A newly paired room never abandons its admitted driver to merge again");
        }
        Check(!Idas3EosRoomRules.ShouldMerge("ID3-A",1,"ID3-A"),"Quick Match cannot join its own room");
        Check(!Idas3EosRoomRules.ShouldMerge(null,1,"ID3-A")&&!Idas3EosRoomRules.ShouldMerge("ID3-A",1,null),"Missing room identities cannot trigger a merge");
        Check(!Idas3EosRoomRules.ShouldMerge("ID3-Z",0,"ID3-A")&&!Idas3EosRoomRules.ShouldMerge("ID3-Z",3,"ID3-A"),"Only a single waiting host is eligible to move rooms");
        Console.WriteLine("TOTAL\t"+checks);return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
