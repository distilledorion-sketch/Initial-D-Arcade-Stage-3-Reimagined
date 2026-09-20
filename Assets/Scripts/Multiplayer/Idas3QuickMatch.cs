using System;
using System.Collections.Generic;

namespace Idas3.Multiplayer
{
    // Discovery owns no gameplay state. The normal authenticated room handshake
    // finishes matchmaking; choosing cars, Ready and Start remain player actions.
    public sealed class Idas3QuickMatch
    {
        enum Stage { Idle, Searching, Joining, Hosting, Waiting, WaitingSearch, Retry }
        readonly IIdas3MatchmakingTransport transport;
        readonly Func<double> now, jitter;
        readonly string roomName;
        readonly HashSet<string> attempted = new HashSet<string>(StringComparer.Ordinal);
        Stage stage;
        double deadline, nextSearch, searchBegan, occupiedAt = -1;
        int joinFailures;
        public bool IsActive => stage != Stage.Idle;
        public string Status { get; private set; } = "";
        public event Action Matched;
        public event Action<string> Failed;

        public Idas3QuickMatch(IIdas3MatchmakingTransport transport, Func<double> now, Func<double> jitter, string roomName)
        {
            this.transport=transport??throw new ArgumentNullException(nameof(transport));
            this.now=now??throw new ArgumentNullException(nameof(now));
            this.jitter=jitter??throw new ArgumentNullException(nameof(jitter));
            this.roomName=roomName;
        }
        public void Start()
        {
            if(IsActive || transport.InLobby || transport.IsBusy)return;
            if(!transport.Available){Failed?.Invoke("Steam is unavailable. Sign in, then try Quick Match again.");return;}
            attempted.Clear();joinFailures=0;occupiedAt=-1;searchBegan=now();
            Search(false);
        }
        void Search(bool waiting)
        {
            stage=waiting?Stage.WaitingSearch:Stage.Searching;deadline=now()+30;
            Status=waiting?"Waiting for another driver — checking other open rooms…":"Finding a compatible open battle…";
            int distance=now()-searchBegan<15?0:now()-searchBegan<30?1:2;
            if(transport is IIdas3RegionalMatchmakingTransport regional){
                Status=distance==0?"Finding nearby drivers…":distance==1?"Searching the surrounding region…":"Searching all regions…";
                regional.BrowseQuickMatch(distance);
            }else transport.Browse();
        }
        void Host()
        {
            stage=Stage.Hosting;deadline=now()+30;
            Status="No open battle found. Opening a room for another driver…";
            transport.HostQuickMatch(roomName);
        }
        void Join(Idas3Room room)
        {
            // A deterministic room order makes two independently created quick
            // rooms converge without both owners leaving to join one another.
            stage=Stage.Joining;deadline=now()+40;occupiedAt=-1;
            attempted.Add(room.Code);
            Status="Joining an open battle…";
            if(transport.InLobby)transport.Leave();
            transport.Join(room.Code);
        }
        Idas3Room Candidate(bool alreadyHosting)
        {
            Idas3Room selected=null;
            foreach(var room in transport.Rooms) {
                if(room==null || string.IsNullOrEmpty(room.Code) || room.Code==transport.RoomCode || room.Order==0 ||
                    room.Capacity!=2 || room.Members!=1 || attempted.Contains(room.Code))continue;
                if(alreadyHosting && room.QuickMatch && room.Order>=transport.RoomOrder)continue;
                if(selected==null || room.Order<selected.Order)selected=room;
            }
            return selected;
        }
        void WaitForDriver()
        {
            stage=Stage.Waiting;nextSearch=now()+3+Math.Max(0,Math.Min(1,jitter()))*2;
            Status="Waiting for another driver. Quick Match will connect you automatically.";
        }
        public void Tick(bool handshakeComplete)
        {
            if(!IsActive)return;
            if(handshakeComplete) {stage=Stage.Idle;Status="Opponent found. Choose your car and select Ready.";Matched?.Invoke();return;}
            if(!transport.Available){Stop("Steam disconnected. Sign in and try Quick Match again.");return;}
            double time=now();
            // Lobby membership precedes the admitted peer/protocol callback.
            // Keep an arriving player's host in place throughout that gap.
            if(transport.InLobby && (transport.Connected || transport.RoomMembers>1)) {
                if(occupiedAt<0)occupiedAt=time;
                Status="Driver found — connecting both games…";
                if(time-occupiedAt>40)HandleTransportError("The other driver did not complete the connection.");
                return;
            }
            occupiedAt=-1;
            switch(stage) {
            case Stage.Searching:
                if(transport.IsBusy){if(time>deadline)Stop("Room search timed out. Please try Quick Match again.");break;}
                var available=Candidate(false);if(available!=null)Join(available);else Host();break;
            case Stage.Joining:
                if(time>deadline)HandleTransportError("That driver could not be reached.");break;
            case Stage.Hosting:
                if(transport.IsBusy){if(time>deadline)Stop("Creating the room timed out. Please try again.");break;}
                if(transport.InLobby&&transport.IsHost)WaitForDriver();
                else Stop("Steam could not open the Quick Match room.");break;
            case Stage.Waiting:
                if(!transport.InLobby||!transport.IsHost){Stop("The Quick Match room closed. Please try again.");break;}
                if(time>=nextSearch&&!transport.IsBusy)Search(true);break;
            case Stage.WaitingSearch:
                if(transport.IsBusy){if(time>deadline)Stop("Room search timed out. Please try again.");break;}
                if(!transport.InLobby||!transport.IsHost){Stop("The Quick Match room closed. Please try again.");break;}
                var other=Candidate(true);if(other!=null)Join(other);else WaitForDriver();break;
            case Stage.Retry:
                if(time>=nextSearch&&!transport.IsBusy)Search(false);break;
            }
        }
        public bool HandleTransportError(string message)
        {
            if(!IsActive)return false;
            if(stage==Stage.Joining && transport.Available && ++joinFailures<=5) {
                stage=Stage.Retry;occupiedAt=-1;
                nextSearch=now()+.4+Math.Max(0,Math.Min(1,jitter()));
                transport.Leave();
                Status="That room filled or closed. Looking for another battle…";
            } else Stop(message);
            return true;
        }
        void Stop(string error)
        {
            stage=Stage.Idle;Status=error;transport.Leave();Failed?.Invoke(error);
        }
        public void Cancel()
        {
            if(!IsActive)return;
            stage=Stage.Idle;Status="Quick Match cancelled.";transport.Leave();
        }
    }
}
