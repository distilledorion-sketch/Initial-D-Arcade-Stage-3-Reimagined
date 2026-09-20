using System;
using System.Collections.Generic;

namespace Idas3.Multiplayer
{
    // Explicit private-process diagnostic only. Discovery is deterministic;
    // admission, hello/build checks and gameplay use real TCP/session code.
    internal sealed class Idas3ChallengerTestTransport : IIdas3MatchmakingTransport
    {
        readonly Idas3TcpTransport inner;
        readonly bool host;
        readonly int port;
        readonly List<Idas3Room> rooms=new List<Idas3Room>();
        public Idas3ChallengerTestTransport(int port,string name){
            if(Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-challenger-check")<0)throw new InvalidOperationException("Diagnostic transport requires explicit flag");
            this.port=port;host=name=="SMOKE HOST";inner=new Idas3TcpTransport(port,true,name);
        }
        public string Kind=>"Private matchmaking diagnostic";
        public bool Available=>inner.Available;
        public bool Connected=>inner.Connected;
        public bool IsHost=>inner.IsHost;
        public string LocalId=>inner.LocalId;
        public string LocalName=>inner.LocalName;
        public string RemoteId=>inner.RemoteId;
        public string RemoteName=>inner.RemoteName;
        public string RoomCode=>inner.RoomCode;
        public string Status=>inner.Status;
        public IReadOnlyList<Idas3Room> Rooms=>rooms;
        public bool IsBusy=>false;
        public bool InLobby=>!string.IsNullOrEmpty(RoomCode);
        public ulong RoomOrder=>1;
        public int RoomMembers=>Connected?2:InLobby?1:0;
        public string BuildCompatibility {get;set;}
        public event Action<byte[]> Message {add=>inner.Message+=value;remove=>inner.Message-=value;}
        public event Action PeerChanged {add=>inner.PeerChanged+=value;remove=>inner.PeerChanged-=value;}
        public event Action<string> Error {add=>inner.Error+=value;remove=>inner.Error-=value;}
        public bool Initialize()=>inner.Initialize();
        public void Host(string name)=>inner.Host(name);
        public void HostQuickMatch(string name)=>Host(name);
        public void Join(string code)=>inner.Join(code);
        public void Browse(){rooms.Clear();if(!host)rooms.Add(new Idas3Room{Code="127.0.0.1:"+port,Name="Isolated host",Members=1,Capacity=2,Order=1,QuickMatch=true});}
        public void Send(byte[] data,bool reliable)=>inner.Send(data,reliable);
        public void Poll()=>inner.Poll();
        public void Leave()=>inner.Leave();
        public void Dispose()=>inner.Dispose();
    }
}
