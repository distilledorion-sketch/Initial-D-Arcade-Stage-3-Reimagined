using System;
using System.Collections.Generic;
using System.Text;
using Epic.OnlineServices;
using Epic.OnlineServices.P2P;

namespace Idas3.Multiplayer
{
    internal sealed class Idas3EosP2P : IDisposable
    {
        sealed class Pending { internal byte[] Data; internal uint Id; internal int Fragment; }
        sealed class Received { internal byte[] Data; internal bool Ordered; }
        readonly Idas3EosTransport owner;
        readonly P2PInterface p2p;
        readonly ProductUserId local, remote;
        readonly SocketId socket;
        readonly string socketName;
        readonly byte[] hello, expectedHello;
        readonly byte[] receive = new byte[Idas3EosPackets.PacketLimit];
        readonly Queue<Pending> outgoing = new Queue<Pending>();
        readonly LinkedList<Received> incoming = new LinkedList<Received>();
        readonly Idas3EosPackets.Reassembler reliable = new Idas3EosPackets.Reassembler(true), unreliable = new Idas3EosPackets.Reassembler(false);
        ulong requestNotify, closedNotify, establishedNotify;
        int outgoingBytes, incomingBytes;
        uint nextReliable = 1, nextUnreliable = 1;
        bool disposed, announced, helloReceived, ackReceived, pumping;
        double started, nextHello, nextHeartbeat, lastReceive, sendBlockedSince;
        string failure;
        internal bool Connected => !disposed && failure == null && helloReceived && ackReceived;
        internal string RemoteId { get; }
        internal string RemoteName { get; }
        internal string LocalResetStamp { get; }
        internal string RemoteResetStamp { get; }
        internal string ConnectionType { get; private set; } = "unknown";
        internal long BufferedEvents { get; private set; }

        internal Idas3EosP2P(Idas3EosTransport owner, ProductUserId other, string name, string lobbyId, string compatibility, bool host)
        {
            this.owner = owner; local = Idas3EosPlatform.User; remote = other;
            RemoteId = other.ToString(); RemoteName = name;
            if (RemoteId == local.ToString()) throw new InvalidOperationException("Both game clients are using the same online identity. Use different devices or operating-system accounts.");
            p2p = Idas3EosPlatform.Handle.GetP2PInterface();
            socketName = "id3" + Idas3EosTransport.Hash(lobbyId).Substring(0, 24);
            socket = new SocketId { SocketName = socketName };
            LocalResetStamp = owner.ResetStamp(local); RemoteResetStamp = owner.ResetStamp(remote);
            hello = Encoding.UTF8.GetBytes("ID3EOS2:" + compatibility + ":" + (host ? "H" : "C") + ":" + LocalResetStamp);
            expectedHello = Encoding.UTF8.GetBytes("ID3EOS2:" + compatibility + ":" + (host ? "C" : "H") + ":" + RemoteResetStamp);
            started = lastReceive = Idas3EosPlatform.Now;
            var relay = new SetRelayControlOptions { RelayControl = Idas3EosOnlineConfig.ForceRelay ? RelayControl.ForceRelays : RelayControl.AllowRelays };
            Idas3EosPlatform.Check(p2p.SetRelayControl(ref relay), "Configuring peer routing");
            var queues = new SetPacketQueueSizeOptions { IncomingPacketQueueMaxSizeBytes = 2 * 1024 * 1024, OutgoingPacketQueueMaxSizeBytes = 2 * 1024 * 1024 };
            Idas3EosPlatform.Check(p2p.SetPacketQueueSize(ref queues), "Configuring peer packet queues");
            var request = new AddNotifyPeerConnectionRequestOptions { LocalUserId = local, SocketId = socket };
            requestNotify = p2p.AddNotifyPeerConnectionRequest(ref request, null, (ref OnIncomingConnectionRequestInfo info) => {
                var copy = info;
                Idas3EosPlatform.Defer(() => { if (!disposed && Same(copy.RemoteUserId, copy.SocketId)) Accept(); });
            });
            var closed = new AddNotifyPeerConnectionClosedOptions { LocalUserId = local, SocketId = socket };
            closedNotify = p2p.AddNotifyPeerConnectionClosed(ref closed, null, (ref OnRemoteConnectionClosedInfo info) => {
                var copy = info;
                Idas3EosPlatform.Defer(() => { if (!disposed && Same(copy.RemoteUserId, copy.SocketId)) failure = "The peer connection closed (" + copy.Reason + ")."; });
            });
            var established = new AddNotifyPeerConnectionEstablishedOptions { LocalUserId = local, SocketId = socket };
            establishedNotify = p2p.AddNotifyPeerConnectionEstablished(ref established, null, (ref OnPeerConnectionEstablishedInfo info) => {
                var copy = info;
                Idas3EosPlatform.Defer(() => {
                    if (!disposed && Same(copy.RemoteUserId, copy.SocketId)) ConnectionType = copy.NetworkType == NetworkConnectionType.DirectConnection ? "direct" : copy.NetworkType == NetworkConnectionType.RelayedConnection ? "relayed" : "unknown";
                });
            });
            if (requestNotify == 0 || closedNotify == 0 || establishedNotify == 0) { Dispose(); throw new InvalidOperationException("Could not register online connection notifications."); }
            Accept();
        }
        bool Same(ProductUserId user, SocketId? id) => user != null && user.ToString() == RemoteId && id.HasValue && id.Value.SocketName.ToString() == socketName;
        void Accept()
        {
            // `remote` comes only from the exact compatible lobby's membership;
            // no request can authorize a different PUID or a previous room.
            var options = new AcceptConnectionOptions { LocalUserId = local, RemoteUserId = remote, SocketId = socket };
            var result = p2p.AcceptConnection(ref options);
            if (result != Result.Success && result != Result.NoChange) failure = "Could not accept the other driver (" + result + ").";
        }
        internal void SendGame(byte[] data, bool ordered)
        {
            if (!Connected) return;
            if (ordered) {
                if (outgoing.Count >= 256 || outgoingBytes + data.Length > 1024 * 1024) { failure = "The reliable online send queue is full."; return; }
                outgoing.Enqueue(new Pending { Data = (byte[])data.Clone(), Id = nextReliable }); nextReliable = Idas3EosPackets.Next(nextReliable); outgoingBytes += data.Length;
                FlushOutgoing();
            } else {
                uint id = nextUnreliable; nextUnreliable = Idas3EosPackets.Next(nextUnreliable);
                for (int i = 0; i < Idas3EosPackets.Count(data.Length); ++i) {
                    var packet = Idas3EosPackets.Fragment(data, id, i);
                    if (Send(packet, 2, false) != Result.Success) break; // stale race snapshots may be dropped under congestion
                }
            }
        }
        Result Send(byte[] bytes, byte channel, bool ordered)
        {
            var options = new SendPacketOptions { LocalUserId = local, RemoteUserId = remote, SocketId = socket,
                Channel = channel, Data = new ArraySegment<byte>(bytes), Reliability = ordered ? PacketReliability.ReliableOrdered : PacketReliability.UnreliableUnordered,
                AllowDelayedDelivery = ordered, DisableAutoAcceptConnection = true };
            return p2p.SendPacket(ref options);
        }
        void FlushOutgoing()
        {
            int budget = 256;
            while (!disposed && outgoing.Count != 0 && budget-- > 0) {
                var item = outgoing.Peek();
                var result = Send(Idas3EosPackets.Fragment(item.Data, item.Id, item.Fragment), 1, true);
                if (result != Result.Success) {
                    if (result != Result.LimitExceeded && result != Result.TooManyRequests) failure = "Sending an online message failed (" + result + ").";
                    if (sendBlockedSince == 0) sendBlockedSince = Idas3EosPlatform.Now;
                    if (Idas3EosPlatform.Now - sendBlockedSince > 15) failure = "The online connection stopped accepting messages.";
                    return;
                }
                sendBlockedSince = 0;
                if (++item.Fragment == Idas3EosPackets.Count(item.Data.Length)) { outgoing.Dequeue(); outgoingBytes -= item.Data.Length; }
            }
        }
        internal bool KeepAliveOnly()
        {
            if (disposed || pumping) return false;
            pumping = true;
            try { PumpPackets(); return true; }
            catch (Exception ex) { failure = "Online packet processing failed: " + ex.Message; return false; }
            finally { pumping = false; }
        }
        void PumpPackets()
        {
            double now = Idas3EosPlatform.Now;
            if (!Connected && now >= nextHello) { Send(hello, 0, true); nextHello = now + 1; }
            if (Connected && now >= nextHeartbeat) { Send(new byte[] { 3 }, 0, false); nextHeartbeat = now + 1; }
            FlushOutgoing();
            var options = new ReceivePacketOptions { LocalUserId = local, MaxDataSizeBytes = (uint)receive.Length };
            ProductUserId sender = null;
            SocketId source = new SocketId();
            for (int i = 0; i < 4096; ++i) {
                var result = p2p.ReceivePacket(ref options, ref sender, ref source, out byte channel, new ArraySegment<byte>(receive), out uint length);
                if (result == Result.NotFound) break;
                if (result != Result.Success) { failure = "Receiving an online packet failed (" + result + ")."; break; }
                if (!Same(sender, source)) continue;
                lastReceive = now;
                if (channel == 0) {
                    if (length == expectedHello.Length && Equal(receive, expectedHello, (int)length)) { helloReceived = true; Send(new byte[] { 1 }, 0, true); }
                    else if (length == 1 && receive[0] == 1) ackReceived = true;
                    else if (length == 1 && receive[0] == 2) { failure = "The other driver left the room."; break; }
                    // Channel zero also carries liveness packets. Invalid control
                    // data never authorizes a game payload or changes the peer.
                    continue;
                }
                // EOS channels need not arrive in handshake order. The sender
                // is already the verified lobby member; hold its game packets
                // until both handshake directions complete instead of dropping
                // reliable data that overtook the channel-zero acknowledgement.
                if (channel != 1 && channel != 2) continue;
                byte[] data = (channel == 1 ? reliable : unreliable).Add(receive, (int)length, now);
                if (data == null) continue;
                BufferMessage(data, channel == 1);
                if (failure != null) break;
            }
            if (!Connected && now - started > 40) failure = "Connecting to the other driver timed out.";
            if (Connected && now - lastReceive > 45) failure = "The other driver stopped responding.";
        }
        internal void Poll()
        {
            KeepAliveOnly();
            if (disposed) return;
            if (failure != null) { owner.RouteFailed(this, failure); return; }
            if (Connected && !announced) { announced = true; owner.Admitted(this); }
            while (!disposed && Connected && incoming.Count != 0) { var data = incoming.First.Value.Data; incoming.RemoveFirst(); incomingBytes -= data.Length; owner.Deliver(this, data); }
        }
        void BufferMessage(byte[] data, bool ordered)
        {
            while (incoming.Count >= 1024 || incomingBytes + data.Length > 1024 * 1024) {
                var stale = incoming.First;
                while (stale != null && stale.Value.Ordered) stale = stale.Next;
                if (stale == null) {
                    if (ordered) failure = "The reliable online receive buffer is full. Please reconnect.";
                    return;
                }
                incomingBytes -= stale.Value.Data.Length; incoming.Remove(stale);
            }
            incoming.AddLast(new Received { Data = data, Ordered = ordered }); incomingBytes += data.Length; ++BufferedEvents;
        }
        static bool Equal(byte[] a, byte[] b, int length) { for (int i = 0; i < length; ++i) if (a[i] != b[i]) return false; return true; }
        public void Dispose()
        {
            if (disposed) return;
            if (Connected) Send(new byte[] { 2 }, 0, true);
            disposed = true;
            if (requestNotify != 0) p2p.RemoveNotifyPeerConnectionRequest(requestNotify);
            if (closedNotify != 0) p2p.RemoveNotifyPeerConnectionClosed(closedNotify);
            if (establishedNotify != 0) p2p.RemoveNotifyPeerConnectionEstablished(establishedNotify);
            var close = new CloseConnectionOptions { LocalUserId = local, RemoteUserId = remote, SocketId = socket };
            p2p.CloseConnection(ref close);
            incoming.Clear(); outgoing.Clear(); incomingBytes = outgoingBytes = 0;
        }
    }
}
