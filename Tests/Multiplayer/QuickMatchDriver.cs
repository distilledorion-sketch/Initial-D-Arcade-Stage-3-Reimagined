using System;
using System.Collections.Generic;
using Idas3.Multiplayer;

// The discovery fallback remains available to non-cloud transports. Exercise
// its real state machine without creating service accounts or public rooms.
static class QuickMatchDriver {
    sealed class FixtureRooms:IIdas3MatchmakingTransport,IIdas3RegionalMatchmakingTransport,IIdas3MatchmakingServiceState {
        public bool Available {get;set;}=true;
        public bool ServiceConnected {get;set;}=true;
        public bool Connected {get;set;}
        public bool IsHost {get;set;}
        public bool InLobby {get;set;}
        public bool IsBusy {get;set;}
        public int RoomMembers {get;set;}
        public ulong RoomOrder {get;set;}
        public string RoomCode {get;set;}="";
        public string BuildCompatibility {get;set;}="fixture";
        public string Kind=>"Fixture";
        public string LocalId=>"local";
        public string LocalName=>"Driver";
        public string RemoteId=>Connected?"peer":"";
        public string RemoteName=>Connected?"Opponent":"";
        public string Status=>"";
        public readonly List<Idas3Room> Entries=new List<Idas3Room>();
        public IReadOnlyList<Idas3Room> Rooms=>Entries;
        public int Searches,Hosts,Leaves,Distance;
        public string Joined;
        public event Action<byte[]> Message {add{}remove{}}
        public event Action PeerChanged {add{}remove{}}
        public event Action<string> Error {add{}remove{}}
        public bool Initialize()=>true;
        public void Host(string name){++Hosts;IsBusy=true;}
        public void HostQuickMatch(string name)=>Host(name);
        public void Join(string code){Joined=code;IsBusy=true;}
        public void Browse(){++Searches;IsBusy=true;}
        public void BrowseQuickMatch(int distance){Distance=distance;Browse();}
        public void CompleteSearch(){IsBusy=false;}
        public void CompleteHost(ulong order=20){IsBusy=false;InLobby=IsHost=true;RoomMembers=1;RoomOrder=order;RoomCode="own";}
        public void Send(byte[] data,bool reliable)=>throw new InvalidOperationException("Discovery must not send game packets");
        public void Poll(){}
        public void Leave(){++Leaves;IsBusy=InLobby=IsHost=Connected=false;RoomMembers=0;RoomOrder=0;RoomCode="";}
        public void Dispose()=>Leave();
    }
    static int checks;
    static double now;
    static void Check(bool value,string label){if(!value)throw new Exception(label);++checks;Console.WriteLine("PASS\t"+label);}
    static Idas3QuickMatch Start(FixtureRooms rooms){now=0;var q=new Idas3QuickMatch(rooms,()=>now,()=>0,"Test");q.Start();return q;}
    static void Waiting(FixtureRooms rooms,Idas3QuickMatch q){rooms.CompleteSearch();q.Tick(false);rooms.CompleteHost();q.Tick(false);}
    static Idas3Room Room(string code,ulong order,bool privateRoom=false,bool quick=true,int members=1)=>new Idas3Room{Code=code,Order=order,Private=privateRoom,QuickMatch=quick,Members=members,Capacity=2};

    static void RecoverSearch(){
        using(var t=new FixtureRooms()){
            var q=Start(t);t.IsBusy=false;q.HandleTransportError("Temporary search failure");
            Check(q.IsActive&&!t.InLobby,"Initial discovery failure keeps the search active");
            now=3;q.Tick(false);Waiting(t,q);
            Check(q.IsActive&&t.InLobby&&t.IsHost,"A retry can create an advertised waiting room");
            now=7;q.Tick(false);int left=t.Leaves;t.IsBusy=false;q.HandleTransportError("Temporary search failure");
            Check(q.IsActive&&t.InLobby&&t.Leaves==left,"Waiting-room search failure preserves its room");
            now=10;q.Tick(false);t.Connected=true;t.RoomMembers=2;q.Tick(true);
            Check(!q.IsActive&&t.Connected&&t.Leaves==left,"A completed handshake stops searching without dropping the peer");
        }
        using(var t=new FixtureRooms()){
            var q=Start(t);
            for(int i=0;i<6;++i){t.IsBusy=false;q.HandleTransportError("Search failed");now+=11;q.Tick(false);}
            Check(!q.IsActive&&!t.InLobby,"Repeated discovery failures stop after the retry budget");
        }
    }
    static void RecoverService(){
        using(var t=new FixtureRooms()){
            var q=Start(t);t.ServiceConnected=false;q.Tick(false);now=45;q.Tick(false);
            Check(q.IsActive,"Temporary service interruption preserves the search");
            t.ServiceConnected=true;t.CompleteSearch();q.Tick(false);t.CompleteHost();q.Tick(false);
            Check(q.IsActive&&t.InLobby,"Search resumes after the service reconnects");
            q.Cancel();Check(!q.IsActive&&!t.InLobby,"Cancellation closes a recovered waiting room");
        }
        using(var t=new FixtureRooms()){
            var q=Start(t);Waiting(t,q);int left=t.Leaves;
            t.ServiceConnected=false;q.Tick(false);now=35;q.Tick(false);t.ServiceConnected=true;q.Tick(false);
            Check(q.IsActive&&t.InLobby&&t.Leaves==left,"Service recovery preserves an existing waiting room");
        }
        using(var t=new FixtureRooms()){
            var q=Start(t);t.ServiceConnected=false;q.Tick(false);now=61;q.Tick(false);
            Check(!q.IsActive&&!t.InLobby,"Service recovery has a bounded deadline");
        }
    }
    static void SelectionAndConvergence(){
        using(var t=new FixtureRooms()){
            t.Entries.Add(Room("private",1,true));t.Entries.Add(Room("full",2,false,true,2));
            t.Entries.Add(Room("later",6));t.Entries.Add(Room("earlier",5));
            var q=Start(t);t.CompleteSearch();q.Tick(false);
            Check(t.Joined=="earlier","Discovery skips private/full rooms and deterministically chooses the first open room");
            t.IsBusy=false;q.HandleTransportError("Room filled");now=1;q.Tick(false);t.CompleteSearch();q.Tick(false);
            Check(t.Joined=="later","A failed join moves on to another compatible room");
            q.Cancel();Check(!q.IsActive&&!t.InLobby,"Cancel during joining ends discovery");
        }
        using(var t=new FixtureRooms()){
            var q=Start(t);Waiting(t,q);t.Entries.Add(Room("newer",30));
            now=4;q.Tick(false);t.CompleteSearch();q.Tick(false);
            Check(t.Joined==null&&t.InLobby,"A waiting host keeps its room when only a newer quick room exists");
            t.Entries.Add(Room("older",10));now=8;q.Tick(false);t.CompleteSearch();q.Tick(false);
            Check(t.Joined=="older"&&!t.InLobby,"A waiting host yields only to an older quick room");
        }
        using(var t=new FixtureRooms()){
            var q=Start(t);Waiting(t,q);now=4;q.Tick(false);int left=t.Leaves;
            t.RoomMembers=2;t.IsBusy=false;q.HandleTransportError("Late discovery failed");q.Tick(false);
            Check(q.IsActive&&t.InLobby&&t.Leaves==left,"A late search failure preserves an arriving peer before handshake");
        }
    }
    static int Main(){try{RecoverSearch();RecoverService();SelectionAndConvergence();Console.WriteLine("TOTAL\t"+checks);return 0;}catch(Exception e){Console.Error.WriteLine(e);return 1;}}
}
