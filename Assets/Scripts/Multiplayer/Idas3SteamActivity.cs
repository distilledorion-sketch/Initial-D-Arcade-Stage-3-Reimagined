using System;
using System.Collections.Generic;
using System.Globalization;
using Steamworks;

namespace Idas3.Multiplayer {
    public static class Idas3LobbyNames {
        public static string ForHost(string host){
            string name=string.IsNullOrWhiteSpace(host)?"Driver":host.Trim();
            var chars=name.ToCharArray();for(int i=0;i<chars.Length;++i)if(char.IsControl(chars[i])||chars[i]=='<'||chars[i]=='>')chars[i]=' ';
            return new string(chars,0,Math.Min(chars.Length,48)).Trim()+" Lobby";
        }
    }
    public struct Idas3OnlineActivity {
        public int Online,Queuing,Racing;
        public bool Available,Limited;
        internal static string StateFor(bool racing,string state,bool quick,bool inLobby,bool connected,bool disconnected)=>
            disconnected?"online":racing&&state!="Results"&&state!="Returning"?"racing":quick||inLobby&&!connected?"queuing":"online";
        public string Format(int count)=>Available?count.ToString(CultureInfo.InvariantCulture)+(Limited?"+":""):"—";
    }
    // A separate invisible lobby per client records presence even in menus and
    // full race rooms. Never count Spacewar's app-wide population or race-lobby
    // members: App 480 is shared, and full lobbies are excluded by Steam search.
    // All calls run on the owning transport's main-thread callback pump.
    internal sealed class Idas3SteamActivity : IDisposable {
        internal const int SearchLimit=50;
        internal const uint FreshSeconds=90;
        const string ScopeKey="idas3.activity",OwnerKey="idas3.owner",StateKey="idas3.state",TimeKey="idas3.seen";
        readonly string scope;
        readonly ulong owner;
        CSteamID presence;
        CallResult<LobbyCreated_t> create;
        CallResult<LobbyMatchList_t> search;
        double nextCreate,nextPublish,nextSearch,searchDeadline,createDeadline,updated=-999;
        bool creating,searching,disposed;
        string published="",state="online";
        Idas3OnlineActivity snapshot;
        internal bool Requested;
        internal Idas3SteamActivity(ulong owner,string scope){this.owner=owner;this.scope=scope;}
        internal Idas3OnlineActivity Get(double now)=>!disposed&&now-updated<45?snapshot:default;
        internal void SetState(string value){state=value=="racing"||value=="queuing"?value:"online";}
        internal void CancelSearch(){if(!searching)return;search?.Dispose();search=null;searching=false;nextSearch=0;}
        internal void Poll(double now,bool roomBusy){
            if(disposed||!SteamUser.BLoggedOn()){snapshot=default;return;}
            if(creating&&now>createDeadline){
                // Keep the creation callback alive: an eventual success must be
                // left rather than leaking a second invisible presence lobby.
                snapshot=default;
            }
            if(searching&&now>searchDeadline){CancelSearch();nextSearch=now+15;snapshot=default;}
            if(presence.m_SteamID==0){
                if(creating||roomBusy||now<nextCreate)return;
                create?.Dispose();creating=true;createDeadline=now+25;nextCreate=now+60;
                create=CallResult<LobbyCreated_t>.Create((r,failed)=>{
                    creating=false;var id=new CSteamID(r.m_ulSteamIDLobby);
                    if(failed||r.m_eResult!=EResult.k_EResultOK)return;
                    if(disposed){SteamMatchmaking.LeaveLobby(id);return;}
                    presence=id;
                    bool ok=SteamMatchmaking.SetLobbyData(id,ScopeKey,scope);
                    ok&=SteamMatchmaking.SetLobbyData(id,OwnerKey,owner.ToString(CultureInfo.InvariantCulture));
                    ok&=SteamMatchmaking.SetLobbyJoinable(id,true);
                    if(!ok){SteamMatchmaking.LeaveLobby(id);presence=default;return;}
                    nextPublish=0;
                });
                // Invisible membership is independent of the regular race room.
                var created=SteamMatchmaking.CreateLobby(ELobbyType.k_ELobbyTypeInvisible,2);
                if(created==SteamAPICall_t.Invalid){creating=false;create.Dispose();create=null;return;}create.Set(created);return;
            }
            if((now>=nextPublish||published!=state)&&SteamMatchmaking.GetLobbyOwner(presence).m_SteamID!=owner){SteamMatchmaking.LeaveLobby(presence);presence=default;snapshot=default;nextCreate=now+15;return;}
            if(now>=nextPublish||published!=state){
                bool ok=SteamMatchmaking.SetLobbyData(presence,StateKey,state);
                ok&=SteamMatchmaking.SetLobbyData(presence,TimeKey,SteamUtils.GetServerRealTime().ToString(CultureInfo.InvariantCulture));
                published=state;nextPublish=now+(ok?15:5);
            }
            if(!Requested||roomBusy||searching||now<nextSearch)return;
            searching=true;searchDeadline=now+20;nextSearch=now+15;
            SteamMatchmaking.AddRequestLobbyListStringFilter(ScopeKey,scope,ELobbyComparison.k_ELobbyComparisonEqual);
            SteamMatchmaking.AddRequestLobbyListDistanceFilter(ELobbyDistanceFilter.k_ELobbyDistanceFilterWorldwide);
            SteamMatchmaking.AddRequestLobbyListResultCountFilter(SearchLimit);
            search?.Dispose();search=CallResult<LobbyMatchList_t>.Create((r,failed)=>{
                searching=false;if(disposed||failed){snapshot=default;return;}
                var samples=new List<Sample>();uint time=SteamUtils.GetServerRealTime();
                for(int i=0;i<r.m_nLobbiesMatching&&i<SearchLimit;++i){var id=SteamMatchmaking.GetLobbyByIndex(i);
                    if(SteamMatchmaking.GetLobbyData(id,ScopeKey)!=scope||SteamMatchmaking.GetLobbyMemberLimit(id)!=2)continue;
                    if(!ulong.TryParse(SteamMatchmaking.GetLobbyData(id,OwnerKey),out ulong who)||who==0||!uint.TryParse(SteamMatchmaking.GetLobbyData(id,TimeKey),out uint seen))continue;
                    samples.Add(new Sample{Owner=who,Seen=seen,State=SteamMatchmaking.GetLobbyData(id,StateKey)});
                }
                // Steam's index may lag our just-published record. Include self
                // once using server time; the aggregator deduplicates identity.
                samples.Add(new Sample{Owner=owner,Seen=time,State=state});
                snapshot=Aggregate(samples,time,r.m_nLobbiesMatching>=SearchLimit);updated=UnityEngine.Time.realtimeSinceStartupAsDouble;
            });
            var call=SteamMatchmaking.RequestLobbyList();if(call==SteamAPICall_t.Invalid){searching=false;snapshot=default;return;}search.Set(call);
        }
        internal struct Sample {internal ulong Owner;internal uint Seen;internal string State;}
        internal static Idas3OnlineActivity Aggregate(IEnumerable<Sample> samples,uint now,bool limited){
            var latest=new Dictionary<ulong,Sample>();
            foreach(var s in samples){if(s.Owner==0||s.Seen>now+5||now>s.Seen&&now-s.Seen>FreshSeconds||s.State!="online"&&s.State!="queuing"&&s.State!="racing")continue;
                if(!latest.TryGetValue(s.Owner,out var old)||s.Seen>=old.Seen)latest[s.Owner]=s;}
            var a=new Idas3OnlineActivity{Available=true,Limited=limited,Online=latest.Count};
            foreach(var s in latest.Values){if(s.State=="queuing")++a.Queuing;else if(s.State=="racing")++a.Racing;}return a;
        }
        public void Dispose(){if(disposed)return;disposed=true;CancelSearch();create?.Dispose();create=null;if(presence.m_SteamID!=0)SteamMatchmaking.LeaveLobby(presence);presence=default;snapshot=default;}
    }
}
