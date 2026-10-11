using System;
using System.Collections.Generic;

// SDK/platform/owner test boundary. The actual P2P lifecycle and packet
// reassembly implementation compile unchanged into this executable.
namespace Epic.OnlineServices {
    public enum Result { Success, NoChange, NotFound, LimitExceeded, TooManyRequests, ServiceFailure }
    public sealed class ProductUserId {
        readonly string value;
        public ProductUserId(string value) { this.value = value; }
        public override string ToString() => value;
    }
}
namespace Epic.OnlineServices.P2P {
    public enum RelayControl { ForceRelays, AllowRelays }
    public enum PacketReliability { ReliableOrdered, UnreliableUnordered }
    public enum NetworkConnectionType { DirectConnection, RelayedConnection }
    public struct SocketId { public string SocketName; }
    public struct SetRelayControlOptions { public RelayControl RelayControl; }
    public struct SetPacketQueueSizeOptions { public ulong IncomingPacketQueueMaxSizeBytes, OutgoingPacketQueueMaxSizeBytes; }
    public struct AddNotifyPeerConnectionRequestOptions { public ProductUserId LocalUserId; public SocketId SocketId; }
    public struct AddNotifyPeerConnectionClosedOptions { public ProductUserId LocalUserId; public SocketId SocketId; }
    public struct AddNotifyPeerConnectionEstablishedOptions { public ProductUserId LocalUserId; public SocketId SocketId; }
    public struct OnIncomingConnectionRequestInfo { public ProductUserId RemoteUserId; public SocketId? SocketId; }
    public struct OnRemoteConnectionClosedInfo { public ProductUserId RemoteUserId; public SocketId? SocketId; public string Reason; }
    public struct OnPeerConnectionEstablishedInfo { public ProductUserId RemoteUserId; public SocketId? SocketId; public NetworkConnectionType NetworkType; }
    public struct AcceptConnectionOptions { public ProductUserId LocalUserId, RemoteUserId; public SocketId SocketId; }
    public struct CloseConnectionOptions { public ProductUserId LocalUserId, RemoteUserId; public SocketId SocketId; }
    public struct SendPacketOptions {
        public ProductUserId LocalUserId, RemoteUserId;
        public SocketId SocketId;
        public byte Channel;
        public ArraySegment<byte> Data;
        public PacketReliability Reliability;
        public bool AllowDelayedDelivery, DisableAutoAcceptConnection;
    }
    public struct ReceivePacketOptions { public ProductUserId LocalUserId; public uint MaxDataSizeBytes; }
    public delegate void RequestCallback(ref OnIncomingConnectionRequestInfo info);
    public delegate void ClosedCallback(ref OnRemoteConnectionClosedInfo info);
    public delegate void EstablishedCallback(ref OnPeerConnectionEstablishedInfo info);
    public sealed class P2PInterface {
        internal sealed class Packet {
            internal string User, Socket;
            internal byte Channel;
            internal byte[] Data;
        }
        readonly Queue<Packet> incoming = new Queue<Packet>();
        internal readonly List<Packet> Sent = new List<Packet>();
        internal string Socket;
        internal int Closed, Removed;
        internal void Inject(byte[] data, byte channel, string user = "peer", string socket = null) {
            incoming.Enqueue(new Packet { User = user, Socket = socket ?? Socket, Channel = channel, Data = data });
        }
        public Result SetRelayControl(ref SetRelayControlOptions options) => Result.Success;
        public Result SetPacketQueueSize(ref SetPacketQueueSizeOptions options) => Result.Success;
        public ulong AddNotifyPeerConnectionRequest(ref AddNotifyPeerConnectionRequestOptions options, object ignored, RequestCallback callback) => 1;
        public ulong AddNotifyPeerConnectionClosed(ref AddNotifyPeerConnectionClosedOptions options, object ignored, ClosedCallback callback) => 2;
        public ulong AddNotifyPeerConnectionEstablished(ref AddNotifyPeerConnectionEstablishedOptions options, object ignored, EstablishedCallback callback) => 3;
        public void RemoveNotifyPeerConnectionRequest(ulong id) { ++Removed; }
        public void RemoveNotifyPeerConnectionClosed(ulong id) { ++Removed; }
        public void RemoveNotifyPeerConnectionEstablished(ulong id) { ++Removed; }
        public Result AcceptConnection(ref AcceptConnectionOptions options) { Socket = options.SocketId.SocketName; return Result.Success; }
        public Result CloseConnection(ref CloseConnectionOptions options) { ++Closed; return Result.Success; }
        public Result SendPacket(ref SendPacketOptions options) {
            var bytes = new byte[options.Data.Count];
            Array.Copy(options.Data.Array, options.Data.Offset, bytes, 0, bytes.Length);
            Sent.Add(new Packet { User = options.RemoteUserId.ToString(), Socket = options.SocketId.SocketName, Channel = options.Channel, Data = bytes });
            return Result.Success;
        }
        public Result ReceivePacket(ref ReceivePacketOptions options, ref ProductUserId sender, ref SocketId socket, out byte channel, ArraySegment<byte> buffer, out uint length) {
            channel = 0; length = 0;
            if (incoming.Count == 0) return Result.NotFound;
            var next = incoming.Dequeue();
            sender = new ProductUserId(next.User); socket = new SocketId { SocketName = next.Socket }; channel = next.Channel;
            Array.Copy(next.Data, 0, buffer.Array, buffer.Offset, next.Data.Length); length = (uint)next.Data.Length;
            return Result.Success;
        }
    }
}
namespace Idas3.Multiplayer {
    internal sealed class Idas3EosTransport {
        internal readonly Dictionary<string, string> Stamps = new Dictionary<string, string> { { "local", "local-reset-1" }, { "peer", "peer-reset-1" } };
        internal readonly List<byte[]> Received = new List<byte[]>();
        internal int Admissions;
        internal string Failure;
        internal Action<Idas3EosP2P> OnAdmit;
        internal string ResetStamp(Epic.OnlineServices.ProductUserId user) => Stamps[user.ToString()];
        internal static string Hash(string value) => "12345678901234567890123456789012";
        internal void Admitted(Idas3EosP2P route) { ++Admissions; OnAdmit?.Invoke(route); }
        internal void Deliver(Idas3EosP2P route, byte[] data) { Received.Add(data); }
        internal void RouteFailed(Idas3EosP2P route, string detail) { Failure = detail; route.Dispose(); }
    }
    internal static class Idas3EosOnlineConfig { internal static bool ForceRelay => false; }
    internal sealed class P2PTestPlatform {
        internal readonly Epic.OnlineServices.P2P.P2PInterface P2P = new Epic.OnlineServices.P2P.P2PInterface();
        internal Epic.OnlineServices.P2P.P2PInterface GetP2PInterface() => P2P;
    }
    internal static class Idas3EosPlatform {
        internal static readonly Epic.OnlineServices.ProductUserId User = new Epic.OnlineServices.ProductUserId("local");
        internal static P2PTestPlatform Handle;
        internal static double Now;
        internal static void Check(Epic.OnlineServices.Result result, string message) { if (result != Epic.OnlineServices.Result.Success) throw new Exception(message); }
        internal static void Defer(Action action) { throw new NotSupportedException("No notification callback is used by these scenarios."); }
    }
}
