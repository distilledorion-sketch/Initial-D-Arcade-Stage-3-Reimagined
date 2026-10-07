using System;
using System.Collections.Generic;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Direct two-player transport, also used by the real two-process test.
    // It carries the exact same session messages as the Steam transport.
    // On a LAN a waiting host also broadcasts a small UDP beacon so other
    // drivers can pick the room from a list instead of typing an address.
    public sealed class Idas3TcpTransport : IIdas3Transport
    {
        const int MaxMessage = 4096, MaxQueuedBytes = 131072;
        internal const int DiscoveryPort = 27045; // 27031-27036 belong to Steam Remote Play
        const string BeaconMagic = "IDAS3LAN1";
        const double BeaconInterval = 1, RoomLifetime = 4;
        readonly int port;
        readonly bool loopbackOnly;
        readonly string identity = Guid.NewGuid().ToString("N");
        readonly byte[] receive = new byte[MaxQueuedBytes];
        readonly Queue<byte[]> outbound = new Queue<byte[]>();
        readonly List<Idas3Room> rooms = new List<Idas3Room>();
        readonly Dictionary<string, double> roomSeen = new Dictionary<string, double>();
        readonly HashSet<string> incompatibleHosts = new HashSet<string>();
        TcpListener listener;
        Socket peer;
        IAsyncResult connecting;
        UdpClient discovery, beacon;
        List<IPEndPoint> beaconTargets;
        int received, sendOffset, queuedBytes;
        double connectingAt, lastBeacon;
        ulong roomOrder;
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
        // Matchmaking identity of this build; rooms with another value are not listed.
        public string BuildCompatibility { get; set; } = "";
        public event Action<byte[]> Message;
        public event Action PeerChanged;
        public event Action<string> Error;
        static string OnlineMenu => Idas3PlatformPaths.IsAndroid ? "ONLINE" : "F1";
        string ReadyStatus => discovery != null ? "Looking for rooms on this network…" : "Direct LAN is ready.";

        public Idas3TcpTransport(int port = 27035, bool loopbackOnly = false, string name = null)
        {
            if (port < 1024 || port > 65535) throw new ArgumentOutOfRangeException(nameof(port));
            this.port = port; this.loopbackOnly = loopbackOnly;
            LocalName = string.IsNullOrWhiteSpace(name) ? "Driver " + identity.Substring(0, 4) : name;
        }
        public bool Initialize() { Available = true; StartDiscovery(); Status = ReadyStatus; return true; }
        public void Host(string roomName)
        {
            Leave(); StopDiscovery();
            try {
                listener = new TcpListener(loopbackOnly ? IPAddress.Loopback : IPAddress.Any, port);
                listener.Start(2); IsHost = true;
                RoomCode = (loopbackOnly ? "127.0.0.1" : LanAddress()) + ":" + port;
                Status = "Waiting for another driver at " + RoomCode;
                StartBeacon();
                PeerChanged?.Invoke();
            } catch (Exception e) { Fail("Could not host LAN race: " + e.Message); }
        }
        public void Join(string roomCode)
        {
            Leave(); StopDiscovery();
            try {
                var parts = (roomCode ?? "").Trim().Split(':');
                if (parts.Length != 2 || !int.TryParse(parts[1], out int targetPort) || targetPort < 1024 || targetPort > 65535)
                    throw new ArgumentException("Enter the host's IPv4 address and port, for example 192.168.1.20:27035.");
                if (parts[0].Equals("localhost", StringComparison.OrdinalIgnoreCase)) parts[0] = "127.0.0.1";
                if (!IPAddress.TryParse(parts[0], out var address) || address.AddressFamily != AddressFamily.InterNetwork)
                    throw new ArgumentException("Use an IPv4 address from the host's " + OnlineMenu + " menu.");
                if (loopbackOnly && !IPAddress.IsLoopback(address)) throw new ArgumentException("This test accepts loopback connections only.");
                peer = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                peer.NoDelay = true;
                connecting = peer.BeginConnect(address, targetPort, null, null);
                connectingAt = Time.realtimeSinceStartupAsDouble;
                RoomCode = address + ":" + targetPort; Status = "Connecting to " + RoomCode;
            } catch (Exception e) { Fail("LAN connection failed: " + e.Message); }
        }
        public void Browse()
        {
            if (loopbackOnly) { Status = "For LAN, enter the address shown in the host's " + OnlineMenu + " menu."; return; }
            if (IsHost || Connected || connecting != null) return;
            rooms.Clear(); roomSeen.Clear(); incompatibleHosts.Clear();
            StartDiscovery(); Status = ReadyStatus;
        }
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
            PollDiscovery(); PollBeacon();
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
            StopBeacon();
            Connected = true; Status = "Driver connected."; PeerChanged?.Invoke();
        }
        void Fail(string message)
        {
            Leave(); Status = message; Error?.Invoke(message);
        }
        public void Leave() => Close(true);
        // Leaving a room resumes room discovery; Host/Join/Dispose do not.
        void Close(bool rediscover)
        {
            bool wasRoom = Connected || !string.IsNullOrEmpty(RoomCode);
            connecting = null; peer?.Dispose(); peer = null;
            listener?.Stop(); listener = null; StopBeacon();
            Connected = false; IsHost = false; RoomCode = "";
            received = sendOffset = queuedBytes = 0; outbound.Clear();
            if (rediscover && Available) StartDiscovery();
            Status = ReadyStatus;
            if (wasRoom) PeerChanged?.Invoke();
        }
        public void Dispose() { Available = false; Close(false); StopDiscovery(); }

        void StartDiscovery()
        {
            if (loopbackOnly || discovery != null) return;
            try {
                var socket = new UdpClient();
                socket.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                socket.Client.Bind(new IPEndPoint(IPAddress.Any, DiscoveryPort));
                socket.EnableBroadcast = true; socket.Client.Blocking = false;
                discovery = socket; MulticastLock(true);
            } catch (Exception e) { Debug.LogWarning("IDAS3 LAN discovery unavailable: " + e.Message); }
        }
        void StopDiscovery()
        {
            if (discovery == null) return;
            discovery.Dispose(); discovery = null; MulticastLock(false);
            rooms.Clear(); roomSeen.Clear(); incompatibleHosts.Clear();
        }
        void PollDiscovery()
        {
            if (discovery == null) return;
            double now = Time.realtimeSinceStartupAsDouble;
            try {
                for (int i = 0; i < 32 && discovery.Available > 0; ++i) {
                    var from = new IPEndPoint(IPAddress.Any, 0);
                    var data = discovery.Receive(ref from);
                    if (!TryReadBeacon(data, out string id, out string build, out int roomPort, out string name) || id == identity) continue;
                    string code = from.Address + ":" + roomPort;
                    if (!string.IsNullOrEmpty(BuildCompatibility) && build != BuildCompatibility) { incompatibleHosts.Add(code); continue; }
                    if (!roomSeen.ContainsKey(code))
                        rooms.Add(new Idas3Room { Code = code, Name = name, HostName = name, Members = 1, Order = ++roomOrder });
                    roomSeen[code] = now;
                }
            } catch (SocketException e) when (e.SocketErrorCode == SocketError.WouldBlock) { }
            catch (Exception e) { Debug.LogWarning("IDAS3 LAN discovery stopped: " + e.Message); StopDiscovery(); return; }
            rooms.RemoveAll(room => now - roomSeen[room.Code] > RoomLifetime && roomSeen.Remove(room.Code));
            if (!IsHost && !Connected && connecting == null)
                Status = rooms.Count > 0 ? "Tap JOIN on a room, or host your own." :
                    incompatibleHosts.Count > 0 ? "A room was found on a different game version. Both drivers need the same build." :
                    "Looking for rooms on this network…";
        }
        void StartBeacon()
        {
            if (loopbackOnly) return;
            try {
                beacon = new UdpClient { EnableBroadcast = true };
                beaconTargets = BroadcastTargets(); lastBeacon = double.NegativeInfinity;
            } catch (Exception e) { beacon = null; Debug.LogWarning("IDAS3 LAN beacon unavailable: " + e.Message); }
        }
        void StopBeacon() { beacon?.Dispose(); beacon = null; }
        void PollBeacon()
        {
            if (beacon == null || !IsHost || Connected) return;
            double now = Time.realtimeSinceStartupAsDouble;
            if (now - lastBeacon < BeaconInterval) return;
            lastBeacon = now;
            var data = WriteBeacon(identity, BuildCompatibility, port, LocalName);
            foreach (var target in beaconTargets) {
                try { beacon.Send(data, data.Length, target); } catch (Exception) { }
            }
        }

        internal static byte[] WriteBeacon(string id, string build, int roomPort, string name)
        {
            string clean = Clean(name);
            return Encoding.UTF8.GetBytes(BeaconMagic + "\n" + id + "\n" + (build ?? "") + "\n" + roomPort + "\n" + clean);
        }
        internal static bool TryReadBeacon(byte[] data, out string id, out string build, out int roomPort, out string name)
        {
            id = build = name = ""; roomPort = 0;
            if (data == null || data.Length < BeaconMagic.Length || data.Length > 512) return false;
            string[] parts;
            try { parts = new UTF8Encoding(false, true).GetString(data).Split('\n'); } catch (ArgumentException) { return false; }
            if (parts.Length != 5 || parts[0] != BeaconMagic || parts[1].Length != 32 || parts[2].Length > 128) return false;
            if (!int.TryParse(parts[3], out roomPort) || roomPort < 1024 || roomPort > 65535) return false;
            id = parts[1]; build = parts[2]; name = Clean(parts[4]);
            return name.Length > 0;
        }
        static string Clean(string name)
        {
            var text = new StringBuilder();
            foreach (char c in name ?? "") if (!char.IsControl(c) && text.Length < 32) text.Append(c);
            return text.ToString().Trim();
        }
        // Limited broadcast can leave through mobile data instead of a phone's
        // own hotspot, so also send to each private interface's subnet broadcast.
        static List<IPEndPoint> BroadcastTargets()
        {
            var targets = new List<IPEndPoint> { new IPEndPoint(IPAddress.Broadcast, DiscoveryPort) };
            try {
                foreach (var nic in NetworkInterface.GetAllNetworkInterfaces()) {
                    if (nic.OperationalStatus != OperationalStatus.Up || nic.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
                    foreach (var address in nic.GetIPProperties().UnicastAddresses) {
                        if (address.Address.AddressFamily != AddressFamily.InterNetwork || !IsPrivate(address.Address)) continue;
                        IPAddress mask; try { mask = address.IPv4Mask; } catch (Exception) { continue; }
                        var target = SubnetBroadcast(address.Address, mask);
                        if (target != null && !targets.Exists(t => t.Address.Equals(target))) targets.Add(new IPEndPoint(target, DiscoveryPort));
                    }
                }
            } catch (Exception) { }
            return targets;
        }
        internal static IPAddress SubnetBroadcast(IPAddress address, IPAddress mask)
        {
            if (mask == null || address.AddressFamily != AddressFamily.InterNetwork || mask.AddressFamily != AddressFamily.InterNetwork) return null;
            var a = address.GetAddressBytes(); var m = mask.GetAddressBytes();
            if (m[0] == 0 || (m[0] & m[1] & m[2] & m[3]) == 255) return null;
            for (int i = 0; i < 4; ++i) a[i] = (byte)(a[i] | ~m[i]);
            return new IPAddress(a);
        }
        internal static bool IsPrivate(IPAddress address)
        {
            var b = address.GetAddressBytes();
            return b.Length == 4 && (b[0] == 10 || (b[0] == 172 && b[1] >= 16 && b[1] <= 31) || (b[0] == 192 && b[1] == 168));
        }
        // Android receives broadcasts on many devices only while a Wi-Fi
        // multicast lock is held (CHANGE_WIFI_MULTICAST_STATE).
#if UNITY_ANDROID && !UNITY_EDITOR
        AndroidJavaObject multicastLock;
        void MulticastLock(bool held)
        {
            try {
                if (held && multicastLock == null) {
                    using (var player = new AndroidJavaClass("com.unity3d.player.UnityPlayer"))
                    using (var activity = player.GetStatic<AndroidJavaObject>("currentActivity"))
                    using (var context = activity.Call<AndroidJavaObject>("getApplicationContext"))
                    using (var wifi = context.Call<AndroidJavaObject>("getSystemService", "wifi")) {
                        if (wifi == null) return;
                        multicastLock = wifi.Call<AndroidJavaObject>("createMulticastLock", "idas3-lan-discovery");
                        multicastLock.Call("setReferenceCounted", false); multicastLock.Call("acquire");
                    }
                } else if (!held && multicastLock != null) {
                    multicastLock.Call("release"); multicastLock.Dispose(); multicastLock = null;
                }
            } catch (Exception e) { Debug.LogWarning("IDAS3 Wi-Fi multicast lock unavailable: " + e.Message); }
        }
#else
        void MulticastLock(bool held) { }
#endif
        // Prefer the routed private address. Android 10+ hides gateways
        // (/proc/net/route), so a private Wi-Fi or hotspot address still wins
        // over virtual adapters; the OS route to a public address is the fallback.
        static string LanAddress()
        {
            string best = null; int bestScore = -1;
            try {
                foreach (var nic in NetworkInterface.GetAllNetworkInterfaces()) {
                    if (nic.OperationalStatus != OperationalStatus.Up || nic.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;
                    var properties = nic.GetIPProperties();
                    bool gateway; try { gateway = properties.GatewayAddresses.Count > 0; } catch (Exception) { gateway = false; }
                    foreach (var address in properties.UnicastAddresses) {
                        if (address.Address.AddressFamily != AddressFamily.InterNetwork || IPAddress.IsLoopback(address.Address)) continue;
                        int score = AddressScore(gateway, IsPrivate(address.Address), nic.Name);
                        if (score > bestScore) { bestScore = score; best = address.Address.ToString(); }
                    }
                }
            } catch { }
            if (best == null || bestScore < 4) {
                try {
                    using (var probe = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp)) {
                        probe.Connect(new IPEndPoint(IPAddress.Parse("192.0.2.1"), 9)); // no packet is sent
                        var routed = ((IPEndPoint)probe.LocalEndPoint).Address;
                        if (best == null || IsPrivate(routed) && bestScore < 2) best = routed.ToString();
                    }
                } catch { }
            }
            return best ?? "127.0.0.1";
        }
        internal static int AddressScore(bool gateway, bool privateAddress, string name)
        {
            name = (name ?? "").ToLowerInvariant();
            bool wireless = name.StartsWith("wlan") || name.StartsWith("swlan") || name.StartsWith("ap") || name.StartsWith("softap") || name.StartsWith("eth");
            return (gateway ? 4 : 0) + (privateAddress ? 2 : 0) + (wireless ? 1 : 0);
        }

        internal static int RunLanSelfTests()
        {
            int checks = 0; void Check(bool ok, string why) { if (!ok) throw new Exception("LAN discovery: " + why); ++checks; }
            string id = Guid.NewGuid().ToString("N");
            var data = WriteBeacon(id, "idas3-build1-abc", 27035, "Takumi\nFujiwara\u0007");
            Check(TryReadBeacon(data, out var readId, out var build, out int roomPort, out var name), "beacon round-trips");
            Check(readId == id && build == "idas3-build1-abc" && roomPort == 27035 && name == "TakumiFujiwara", "beacon fields and name sanitising");
            Check(Clean(new string('x', 80)).Length == 32, "names are capped");
            Check(!TryReadBeacon(Encoding.UTF8.GetBytes("IDAS3LAN0\n" + id + "\nb\n27035\nA"), out _, out _, out _, out _), "wrong magic rejected");
            Check(!TryReadBeacon(Encoding.UTF8.GetBytes("IDAS3LAN1\n" + id + "\nb\n80\nA"), out _, out _, out _, out _), "privileged port rejected");
            Check(!TryReadBeacon(Encoding.UTF8.GetBytes("IDAS3LAN1\nshort\nb\n27035\nA"), out _, out _, out _, out _), "bad identity rejected");
            Check(!TryReadBeacon(new byte[600], out _, out _, out _, out _) && !TryReadBeacon(new byte[] { 0xC3, 0x28 }, out _, out _, out _, out _), "oversized and invalid UTF-8 rejected");
            Check(!TryReadBeacon(Encoding.UTF8.GetBytes("IDAS3LAN1\n" + id + "\nb\n27035\n\u0001"), out _, out _, out _, out _), "empty name rejected");
            Check(SubnetBroadcast(IPAddress.Parse("192.168.43.17"), IPAddress.Parse("255.255.255.0")).ToString() == "192.168.43.255", "hotspot /24 broadcast");
            Check(SubnetBroadcast(IPAddress.Parse("10.12.3.4"), IPAddress.Parse("255.255.240.0")).ToString() == "10.12.15.255", "/20 broadcast");
            Check(SubnetBroadcast(IPAddress.Parse("10.0.0.1"), IPAddress.Parse("255.255.255.255")) == null && SubnetBroadcast(IPAddress.Parse("10.0.0.1"), IPAddress.Any) == null, "no broadcast for /32 or empty mask");
            Check(IsPrivate(IPAddress.Parse("172.20.1.1")) && IsPrivate(IPAddress.Parse("192.168.0.156")) && !IsPrivate(IPAddress.Parse("100.64.0.1")) && !IsPrivate(IPAddress.Parse("8.8.8.8")), "private ranges");
            Check(AddressScore(false, true, "wlan0") > AddressScore(false, false, "rmnet_data0"), "Android Wi-Fi beats mobile data without gateway info");
            Check(AddressScore(true, true, "Ethernet") > AddressScore(false, true, "vEthernet (WSL)"), "Windows routed adapter beats virtual adapter");
            // Real sockets: a beacon arriving on the discovery port becomes a joinable room.
            var browser = new Idas3TcpTransport(name: "Browser") { BuildCompatibility = "idas3-build1-same" };
            try {
                Check(browser.Initialize() && browser.discovery != null, "discovery socket opens");
                using (var sender = new UdpClient()) {
                    var target = new IPEndPoint(IPAddress.Loopback, DiscoveryPort);
                    void Send(byte[] packet) => sender.Send(packet, packet.Length, target);
                    Send(WriteBeacon(Guid.NewGuid().ToString("N"), "idas3-build1-other", 27035, "Old build"));
                    Send(WriteBeacon(browser.identity, "idas3-build1-same", 27035, "Myself"));
                    Send(WriteBeacon(Guid.NewGuid().ToString("N"), "idas3-build1-same", 27040, "Keiichi"));
                    var deadline = DateTime.UtcNow.AddSeconds(2);
                    while (browser.rooms.Count == 0 && DateTime.UtcNow < deadline) { browser.PollDiscovery(); System.Threading.Thread.Sleep(20); }
                    browser.PollDiscovery();
                }
                Check(browser.rooms.Count == 1 && browser.rooms[0].Code == "127.0.0.1:27040" && browser.rooms[0].HostName == "Keiichi", "compatible room listed with the sender's address");
                Check(browser.incompatibleHosts.Count == 1, "different build is reported, not listed; own beacon ignored");
                browser.Join("127.0.0.1:27999");
                Check(browser.discovery == null && browser.rooms.Count == 0, "joining a room stops discovery");
            } finally { browser.Dispose(); }
            Check(browser.discovery == null && !browser.Available, "dispose closes the discovery socket");
            return checks;
        }
    }
}
