using System;
using System.Collections.Generic;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Direct two-player transport, also used by the real two-process test.
    // It carries the exact same session messages as the Steam transport.
    public sealed class Idas3TcpTransport : IIdas3Transport
    {
        const int MaxMessage = 4096, MaxQueuedBytes = 131072;
        readonly int port;
        readonly bool loopbackOnly;
        readonly string identity = Guid.NewGuid().ToString("N");
        readonly byte[] receive = new byte[MaxQueuedBytes];
        readonly Queue<byte[]> outbound = new Queue<byte[]>();
        readonly List<Idas3Room> rooms = new List<Idas3Room>();
        TcpListener listener;
        Socket peer;
        IAsyncResult connecting;
        int received, sendOffset, queuedBytes;
        double connectingAt;
        public string Kind => "Direct LAN";
        public bool Available { get; private set; }
        public bool Connected { get; private set; }
        public bool IsHost { get; private set; }
        public string LocalId => identity;
        public string LocalName { get; private set; }
        public string RemoteId => Connected ? "tcp-peer" : "";
        public string RemoteName => Connected ? "Other driver" : "";
        public string RoomCode { get; private set; } = "";
        public string Status { get; private set; } = "Direct LAN is ready.";
        public IReadOnlyList<Idas3Room> Rooms => rooms;
        public event Action<byte[]> Message;
        public event Action PeerChanged;
        public event Action<string> Error;

        public Idas3TcpTransport(int port = 27035, bool loopbackOnly = false, string name = null)
        {
            if (port < 1024 || port > 65535) throw new ArgumentOutOfRangeException(nameof(port));
            this.port = port; this.loopbackOnly = loopbackOnly;
            LocalName = string.IsNullOrWhiteSpace(name) ? "Driver " + identity.Substring(0, 4) : name;
        }
        public bool Initialize() { Available = true; return true; }
        public void Host(string roomName)
        {
            Leave();
            try {
                listener = new TcpListener(loopbackOnly ? IPAddress.Loopback : IPAddress.Any, port);
                listener.Start(2); IsHost = true;
                RoomCode = (loopbackOnly ? "127.0.0.1" : LanAddress()) + ":" + port;
                Status = "Waiting for another driver at " + RoomCode;
                PeerChanged?.Invoke();
            } catch (Exception e) { Fail("Could not host LAN race: " + e.Message); }
        }
        public void Join(string roomCode)
        {
            Leave();
            try {
                var parts = (roomCode ?? "").Trim().Split(':');
                if (parts.Length != 2 || !int.TryParse(parts[1], out int targetPort) || targetPort < 1024 || targetPort > 65535)
                    throw new ArgumentException("Enter the host's IPv4 address and port, for example 192.168.1.20:27035.");
                if (parts[0].Equals("localhost", StringComparison.OrdinalIgnoreCase)) parts[0] = "127.0.0.1";
                if (!IPAddress.TryParse(parts[0], out var address) || address.AddressFamily != AddressFamily.InterNetwork)
                    throw new ArgumentException("Use an IPv4 address from the host's F1 menu.");
                if (loopbackOnly && !IPAddress.IsLoopback(address)) throw new ArgumentException("This test accepts loopback connections only.");
                peer = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                peer.NoDelay = true;
                connecting = peer.BeginConnect(address, targetPort, null, null);
                connectingAt = Time.realtimeSinceStartupAsDouble;
                RoomCode = address + ":" + targetPort; Status = "Connecting to " + RoomCode;
            } catch (Exception e) { Fail("LAN connection failed: " + e.Message); }
        }
        public void Browse() { Status = "For LAN, enter the address shown in the host's F1 menu."; }
        public void Send(byte[] data, bool reliable)
        {
            if (!Connected || data == null) return;
            if (data.Length == 0 || data.Length > MaxMessage) { Fail("Network packet exceeds the game limit."); return; }
            if (queuedBytes + data.Length + 4 > MaxQueuedBytes) { Fail("The other driver stopped receiving data."); return; }
            var packet = new byte[data.Length + 4];
            packet[0] = (byte)data.Length; packet[1] = (byte)(data.Length >> 8);
            packet[2] = (byte)(data.Length >> 16); packet[3] = (byte)(data.Length >> 24);
            Buffer.BlockCopy(data, 0, packet, 4, data.Length);
            outbound.Enqueue(packet); queuedBytes += packet.Length;
        }
        public void Poll()
        {
            try {
                if (listener != null && listener.Pending()) {
                    var incoming = listener.AcceptSocket();
                    if (peer != null) incoming.Dispose();
                    else { peer = incoming; Admit(); }
                }
                if (connecting != null) {
                    if (connecting.IsCompleted) { peer.EndConnect(connecting); connecting = null; Admit(); }
                    else if (Time.realtimeSinceStartupAsDouble - connectingAt > 10) Fail("LAN connection timed out. Check the address and host firewall.");
                }
                if (!Connected || peer == null) return;
                int operations = 0;
                while (outbound.Count > 0 && ++operations <= 64 && peer.Poll(0, SelectMode.SelectWrite)) {
                    var head = outbound.Peek();
                    int sent = peer.Send(head, sendOffset, head.Length - sendOffset, SocketFlags.None);
                    if (sent == 0) { Fail("The other driver disconnected."); return; }
                    sendOffset += sent;
                    if (sendOffset == head.Length) { queuedBytes -= head.Length; outbound.Dequeue(); sendOffset = 0; }
                }
                operations = 0;
                while (Connected && peer != null && ++operations <= 64 && peer.Poll(0, SelectMode.SelectRead)) {
                    int count = peer.Receive(receive, received, receive.Length - received, SocketFlags.None);
                    if (count == 0) { Fail("The other driver disconnected."); return; }
                    received += count;
                    while (received >= 4) {
                        int length = receive[0] | (receive[1] << 8) | (receive[2] << 16) | (receive[3] << 24);
                        if (length <= 0 || length > MaxMessage) { Fail("Rejected an invalid network packet."); return; }
                        if (received < length + 4) break;
                        var packet = new byte[length]; Buffer.BlockCopy(receive, 4, packet, 0, length);
                        received -= length + 4; Buffer.BlockCopy(receive, length + 4, receive, 0, received);
                        Message?.Invoke(packet);
                        if (!Connected || peer == null) return;
                    }
                }
            } catch (SocketException e) {
                if (e.SocketErrorCode != SocketError.WouldBlock && e.SocketErrorCode != SocketError.IOPending)
                    Fail("LAN connection lost: " + e.SocketErrorCode);
            } catch (Exception e) { Fail("LAN connection failed: " + e.Message); }
        }
        void Admit()
        {
            peer.NoDelay = true; peer.Blocking = false;
            peer.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
            Connected = true; Status = "Driver connected."; PeerChanged?.Invoke();
        }
        void Fail(string message)
        {
            Leave(); Status = message; Error?.Invoke(message);
        }
        public void Leave()
        {
            bool wasRoom = Connected || !string.IsNullOrEmpty(RoomCode);
            connecting = null; peer?.Dispose(); peer = null;
            listener?.Stop(); listener = null;
            Connected = false; IsHost = false; RoomCode = "";
            received = sendOffset = queuedBytes = 0; outbound.Clear();
            Status = "Direct LAN is ready.";
            if (wasRoom) PeerChanged?.Invoke();
        }
        public void Dispose() { Leave(); Available = false; }
        static string LanAddress()
        {
            try {
                foreach (var nic in NetworkInterface.GetAllNetworkInterfaces()) {
                    if (nic.OperationalStatus != OperationalStatus.Up || nic.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
                    var properties = nic.GetIPProperties();
                    if (properties.GatewayAddresses.Count == 0) continue;
                    foreach (var address in properties.UnicastAddresses)
                        if (address.Address.AddressFamily == AddressFamily.InterNetwork) return address.Address.ToString();
                }
            } catch { }
            return "127.0.0.1";
        }
    }
}
