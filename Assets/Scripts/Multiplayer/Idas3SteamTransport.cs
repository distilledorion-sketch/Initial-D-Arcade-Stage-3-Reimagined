using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using Steamworks;

namespace Idas3.Multiplayer
{
    // Steam App 480 is Valve's Spacewar development application. This adapter
    // uses the real Steam client/API and isolates its rooms from other tests.
    // All entry points and callbacks belong to the Unity main thread.
    public sealed class Idas3SteamTransport : IIdas3MatchmakingTransport, IIdas3RegionalMatchmakingTransport, IIdas3MatchmakingServiceState
    {
        public const uint DevelopmentAppId = 480;
        public const string GameNamespace = "idas3-unity-recompiled-p2p-20260909";
        public const string TransportProtocol = "1";
        public const int MaxPayloadBytes = 65536;
        private const string GameKey = "idas3.game", ProtocolKey = "idas3.protocol";
        private const string BuildKey = "idas3.build", QuickMatchKey = "idas3.quick";
        private const string HostKey = "idas3.host", NameKey = "idas3.name", HostNameKey = "idas3.hostName";
        private const int Channel = 193031, HeaderBytes = 17;
        private const ulong Magic = 0x3154454E334449UL;
        // The game session owns its tighter15s live/60s loading deadlines.
        // Native course loading can block one main thread for many seconds.
        private const double OperationTimeout = 25, PeerTimeout = 75;
        private static Idas3SteamTransport steamOwner;
        private readonly Idas3SteamInputConfig controllerConfig = new Idas3SteamInputConfig();
        private readonly Stopwatch clock = Stopwatch.StartNew();
        private readonly Func<double> nowSeconds;
        private double Now => nowSeconds();
        private readonly Queue<byte[]> pendingReliable = new Queue<byte[]>();
        private int pendingReliableBytes;
        private double reliableBlockedAt, nextReliableRetry;
        private const int MaxPendingReliableBytes = 256 * 1024, MaxPendingReliableMessages = 128;
        private const double ReliableRetryTimeout = 10;
        private readonly List<IDisposable> callbacks = new List<IDisposable>();
        private readonly List<IDisposable> calls = new List<IDisposable>();
        private IDisposable discoveryCall;
        private readonly List<Idas3Room> rooms = new List<Idas3Room>();
        private readonly IntPtr[] received = new IntPtr[32];
        private CSteamID lobby, pendingLobby;
        private ulong local, host, peer;
        private long operation;
        private double deadline, lastReceive, lastHeartbeat, lastMembershipCheck;
        private string pendingKind;
        private string buildCompatibility = "";
        private bool polling;
        private Idas3SteamActivity activity;
        public bool ActivityRequested {get;set;}
        public bool PublishActivity {get;set;}
        public string ActivityState {get;set;}="online";
        public Idas3OnlineActivity Activity=>Available?activity?.Get(UnityEngine.Time.realtimeSinceStartupAsDouble)??default:default;

        public string Kind => "Steam";
        public bool Available { get; private set; }
        public bool ServiceConnected => Available && SteamUser.BLoggedOn();
        public bool Connected => Available && peer != 0;
        public bool IsHost => InLobby && host == local;
        public string LocalId => local == 0 ? "" : local.ToString(CultureInfo.InvariantCulture);
        public string LocalName { get; private set; } = "";
        public string RemoteId => peer == 0 ? "" : peer.ToString(CultureInfo.InvariantCulture);
        public string RemoteName { get; private set; } = "";
        public string RoomCode => InLobby ? EncodeRoom(lobby.m_SteamID) : "";
        public string Status { get; private set; } = "Steam is not initialized.";
        public IReadOnlyList<Idas3Room> Rooms => rooms;
        public bool IsBusy => pendingKind != null;
        public bool InLobby => lobby.m_SteamID != 0;
        public ulong RoomOrder => lobby.m_SteamID;
        public int RoomMembers => InLobby ? SteamMatchmaking.GetNumLobbyMembers(lobby) : 0;
        public string BuildCompatibility {
            get => buildCompatibility;
            set {
                string next = value ?? "";
                if ((InLobby || IsBusy) && !string.Equals(next, buildCompatibility, StringComparison.Ordinal))
                    throw new InvalidOperationException("Leave the current room request before changing the game build.");
                buildCompatibility = next;
            }
        }
        public event Action<byte[]> Message;
        public event Action PeerChanged;
        public event Action<string> Error;

        public Idas3SteamTransport() : this(null) { }
        internal Idas3SteamTransport(Func<double> now) { nowSeconds = now ?? (() => clock.Elapsed.TotalSeconds); }

        public bool Initialize()
        {
            if (Available) return true;
            if (steamOwner != null && steamOwner != this) { Fail("Steam is already owned by another multiplayer session."); return false; }
            try {
                var result = SteamAPI.InitEx(out string detail);
                if (result != ESteamAPIInitResult.k_ESteamAPIInitResult_OK) {
                    Fail("Steam could not start. Open Steam and sign in, then select Retry Steam. " + detail); return false;
                }
                Available = true; steamOwner = this;
                if (SteamUtils.GetAppID().m_AppId != DevelopmentAppId)
                    throw new InvalidOperationException("This test build requires steam_appid.txt containing 480 beside the game executable.");
                controllerConfig.Initialize();
                if (!SteamUser.BLoggedOn()) throw new InvalidOperationException("Steam is offline. Sign in before using online races.");
                local = SteamUser.GetSteamID().m_SteamID;
                LocalName = CleanName(SteamFriends.GetPersonaName(), "Driver");
                activity=new Idas3SteamActivity(local,GameNamespace+"-activity-v1"+(buildCompatibility.Contains("-quick-smoke-")?buildCompatibility:""));
                callbacks.Add(Callback<LobbyDataUpdate_t>.Create(OnLobbyData));
                callbacks.Add(Callback<LobbyChatUpdate_t>.Create(OnLobbyMembers));
                callbacks.Add(Callback<SteamNetworkingMessagesSessionRequest_t>.Create(OnSessionRequest));
                callbacks.Add(Callback<SteamNetworkingMessagesSessionFailed_t>.Create(OnSessionFailed));
                callbacks.Add(Callback<SteamServersDisconnected_t>.Create(OnSteamDisconnected));
                callbacks.Add(Callback<SteamServersConnected_t>.Create(_ => {
                    lastMembershipCheck = -100;
                    UnityEngine.Debug.Log("IDAS3 Steam services reconnected; peer session retained.");
                }));
                SteamNetworkingUtils.InitRelayNetworkAccess();
                Status = "Steam ready. Host a room or enter a room code.";
                return true;
            }
            catch (Exception e) { Dispose(); Fail("Steam initialization failed: " + e.Message); return false; }
        }

        public void Host(string roomName) => HostRoom(roomName, false);
        public void HostQuickMatch(string roomName) => HostRoom(roomName, true);
        private void HostRoom(string roomName, bool quickMatch)
        {
            if (!CanStart()) return;
            Leave(); long token = Begin("Creating room");
            string title = CleanName(roomName, LocalName + "'s race");
            string build = buildCompatibility;
            Track<LobbyCreated_t>(SteamMatchmaking.CreateLobby(ELobbyType.k_ELobbyTypePublic, 2), (result, failed) => {
                var created = new CSteamID(result.m_ulSteamIDLobby);
                if (token != operation) { if (!failed && result.m_eResult == EResult.k_EResultOK) SteamMatchmaking.LeaveLobby(created); return; }
                EndOperation();
                if (failed || result.m_eResult != EResult.k_EResultOK) { Fail("Could not create Steam room: " + result.m_eResult); return; }
                lobby = created; host = local;
                bool ok = SteamMatchmaking.SetLobbyJoinable(lobby, false);
                ok &= SteamMatchmaking.SetLobbyData(lobby, GameKey, GameNamespace);
                ok &= SteamMatchmaking.SetLobbyData(lobby, ProtocolKey, TransportProtocol);
                ok &= SteamMatchmaking.SetLobbyData(lobby, BuildKey, build);
                ok &= SteamMatchmaking.SetLobbyData(lobby, QuickMatchKey, quickMatch ? "1" : "0");
                ok &= SteamMatchmaking.SetLobbyData(lobby, HostKey, LocalId);
                ok &= SteamMatchmaking.SetLobbyData(lobby, NameKey, title);
                ok &= SteamMatchmaking.SetLobbyData(lobby, HostNameKey, LocalName);
                if (!ok) { FailRoom("Could not publish the room's game and protocol metadata."); return; }
                SteamMatchmaking.SetLobbyMemberData(lobby, ProtocolKey, TransportProtocol);
                if (!SteamMatchmaking.SetLobbyJoinable(lobby, true)) { FailRoom("Steam could not open the room."); return; }
                Status = "Room " + RoomCode + " — waiting for another driver.";
                PeerChanged?.Invoke();
            });
        }

        public void Join(string roomCode)
        {
            if (!CanStart()) return;
            if (!TryDecodeRoom(roomCode, out ulong id) || !new CSteamID(id).IsLobby()) {
                Fail("Enter a valid ID3- room code."); return;
            }
            Leave(); Begin("Checking room"); pendingLobby = new CSteamID(id);
            // Fetch metadata before joining: never join an unrelated App 480 lobby.
            if (!SteamMatchmaking.RequestLobbyData(pendingLobby)) { EndOperation(); Fail("The Steam room could not be found."); }
        }

        public void Browse() => BrowseDistance(ELobbyDistanceFilter.k_ELobbyDistanceFilterWorldwide);
        public void BrowseQuickMatch(int distance) => BrowseDistance(distance<=0?ELobbyDistanceFilter.k_ELobbyDistanceFilterClose:distance==1?ELobbyDistanceFilter.k_ELobbyDistanceFilterDefault:ELobbyDistanceFilter.k_ELobbyDistanceFilterWorldwide);
        void BrowseDistance(ELobbyDistanceFilter distance)
        {
            if (!CanStart()) return;
            activity?.CancelSearch();
            rooms.Clear(); long token = Begin("Finding rooms");
            SteamMatchmaking.AddRequestLobbyListStringFilter(GameKey, GameNamespace, ELobbyComparison.k_ELobbyComparisonEqual);
            SteamMatchmaking.AddRequestLobbyListStringFilter(ProtocolKey, TransportProtocol, ELobbyComparison.k_ELobbyComparisonEqual);
            SteamMatchmaking.AddRequestLobbyListStringFilter(BuildKey, buildCompatibility, ELobbyComparison.k_ELobbyComparisonEqual);
            SteamMatchmaking.AddRequestLobbyListFilterSlotsAvailable(1);
            SteamMatchmaking.AddRequestLobbyListDistanceFilter(distance);
            SteamMatchmaking.AddRequestLobbyListResultCountFilter(50);
            discoveryCall = Track<LobbyMatchList_t>(SteamMatchmaking.RequestLobbyList(), (result, failed) => {
                if (token != operation) return;
                EndOperation();
                // Lobby callbacks and discovery results can arrive in either
                // order. Once a driver arrives, discovery cannot fail the match.
                if (InLobby && SteamMatchmaking.GetNumLobbyMembers(lobby) > 1) { RefreshPeer(); return; }
                if (failed) { Fail("Steam room search failed. Please retry."); return; }
                for (int i = 0; i < result.m_nLobbiesMatching && i < 50; ++i) {
                    var candidate = SteamMatchmaking.GetLobbyByIndex(i);
                    if (!CompatibleRoom(candidate) || candidate == lobby) continue;
                    int members = SteamMatchmaking.GetNumLobbyMembers(candidate);
                    if (members < 1 || members >= 2) continue;
                    rooms.Add(new Idas3Room { Code = EncodeRoom(candidate.m_SteamID),
                        Name = CleanName(SteamMatchmaking.GetLobbyData(candidate, NameKey), "Race room"),
                        HostName = CleanName(SteamMatchmaking.GetLobbyData(candidate, HostNameKey), "Driver"), Members = members, Capacity = 2,
                        Order = candidate.m_SteamID, QuickMatch = SteamMatchmaking.GetLobbyData(candidate, QuickMatchKey) == "1" });
                }
                Status = rooms.Count == 0 ? "No open rooms found. You can host one." : "Found " + rooms.Count + " open room(s).";
            });
        }

        private void OnLobbyData(LobbyDataUpdate_t update)
        {
            if (pendingKind == "Checking room" && update.m_ulSteamIDLobby == pendingLobby.m_SteamID && update.m_ulSteamIDMember == update.m_ulSteamIDLobby) {
                var candidate = pendingLobby; long token = operation;
                if (update.m_bSuccess == 0 || !CompatibleRoom(candidate)) { EndOperation(); Fail("That code is not a compatible Initial D room."); return; }
                if (SteamMatchmaking.GetNumLobbyMembers(candidate) >= 2) { EndOperation(); Fail("That room is full."); return; }
                pendingKind = "Joining room"; Status = pendingKind + "…";
                Track<LobbyEnter_t>(SteamMatchmaking.JoinLobby(candidate), (result, failed) => {
                    var entered = new CSteamID(result.m_ulSteamIDLobby);
                    bool success = !failed && result.m_EChatRoomEnterResponse == (uint)EChatRoomEnterResponse.k_EChatRoomEnterResponseSuccess;
                    if (token != operation) { if (success) SteamMatchmaking.LeaveLobby(entered); return; }
                    EndOperation();
                    if (!success) { Fail("Could not join Steam room: " + (EChatRoomEnterResponse)result.m_EChatRoomEnterResponse); return; }
                    lobby = entered;
                    if (entered != candidate || !CompatibleRoom(lobby)) { FailRoom("The room's game or protocol changed while joining."); return; }
                    host = SteamMatchmaking.GetLobbyOwner(lobby).m_SteamID;
                    SteamMatchmaking.SetLobbyMemberData(lobby, ProtocolKey, TransportProtocol);
                    Status = "Joined room " + RoomCode + "."; RefreshPeer();
                });
            }
            else if (InLobby && update.m_ulSteamIDLobby == lobby.m_SteamID) RefreshPeer();
        }

        private void OnLobbyMembers(LobbyChatUpdate_t update)
        {
            if (!InLobby || update.m_ulSteamIDLobby != lobby.m_SteamID) return;
            const uint departed = (uint)(EChatMemberStateChange.k_EChatMemberStateChangeLeft |
                EChatMemberStateChange.k_EChatMemberStateChangeDisconnected |
                EChatMemberStateChange.k_EChatMemberStateChangeKicked |
                EChatMemberStateChange.k_EChatMemberStateChangeBanned);
            if ((update.m_rgfChatMemberStateChange & departed) != 0) {
                ulong who = update.m_ulSteamIDUserChanged;
                if (who == local || who == host) { FailRoom("The host left or your Steam room membership ended."); return; }
                if (who == peer) { ClosePeer(); PeerChanged?.Invoke(); return; }
            }
            RefreshPeer();
        }

        private bool CompatibleRoom(CSteamID room) => room.IsLobby() &&
            SteamMatchmaking.GetLobbyData(room, GameKey) == GameNamespace &&
            SteamMatchmaking.GetLobbyData(room, ProtocolKey) == TransportProtocol &&
            !string.IsNullOrWhiteSpace(buildCompatibility) && SteamMatchmaking.GetLobbyData(room, BuildKey) == buildCompatibility &&
            SteamMatchmaking.GetLobbyMemberLimit(room) == 2 &&
            ulong.TryParse(SteamMatchmaking.GetLobbyData(room, HostKey), NumberStyles.None, CultureInfo.InvariantCulture, out ulong owner) && owner != 0;

        private void RefreshPeer()
        {
            if (!InLobby || !ServiceConnected) return;
            lastMembershipCheck = Now;
            // GetLobbyOwner is available only after joining. Validate the
            // advertised owner against Steam here, never during discovery.
            if (!CompatibleRoom(lobby) || SteamMatchmaking.GetLobbyOwner(lobby).m_SteamID != host ||
                SteamMatchmaking.GetLobbyData(lobby, HostKey) != host.ToString(CultureInfo.InvariantCulture)) {
                FailRoom("The host left or the room changed. Create or join another room."); return;
            }
            int count = SteamMatchmaking.GetNumLobbyMembers(lobby);
            if (count < 1 || count > 2) { FailRoom("The room no longer has a valid two-driver membership."); return; }
            if (count > 1) CancelDiscovery();
            bool containsSelf = false; ulong admitted = 0;
            for (int i = 0; i < count; ++i) {
                var member = SteamMatchmaking.GetLobbyMemberByIndex(lobby, i);
                if (member.m_SteamID == local) containsSelf = true;
                else {
                    string protocol = SteamMatchmaking.GetLobbyMemberData(lobby, member, ProtocolKey);
                    // Missing cached metadata is not a departure. New members
                    // still require protocol admission; explicit changes revoke it.
                    if (protocol == TransportProtocol || member.m_SteamID == peer && string.IsNullOrEmpty(protocol)) admitted = member.m_SteamID;
                }
            }
            if (!containsSelf) { FailRoom("You are no longer in the Steam room."); return; }
            if (admitted == peer) return;
            ClosePeer(); peer = admitted;
            RemoteName = peer == 0 ? "" : CleanName(SteamFriends.GetFriendPersonaName(new CSteamID(peer)), "Driver");
            lastReceive = Now; lastHeartbeat = -100;
            Status = peer == 0 ? "Waiting for another driver in " + RoomCode + "." : "Connected to " + RemoteName + ".";
            PeerChanged?.Invoke();
        }

        private bool Allowed(ulong id)
        {
            // RefreshPeer admits only a member of this validated two-driver
            // room. Steam callbacks run before packet delivery and revoke that
            // identity on membership changes. Do not repeat synchronous Steam
            // membership queries for every frame, send and received packet.
            return InLobby && id != 0 && id == peer && id != local;
        }

        internal double[] DiagnosticMembershipCost()
        {
            if(!Available||!InLobby||peer!=0)throw new InvalidOperationException("Membership benchmark requires an isolated one-member Steam lobby.");
            // Measure only read-only calls against our own diagnostic room.
            // A real two-member check performs these calls once or twice more
            // per packet/frame depending on the member's list position.
            const int iterations=4096;
            ulong sink=0;var timer=Stopwatch.StartNew();
            for(int i=0;i<iterations;++i){sink+=(ulong)SteamMatchmaking.GetNumLobbyMembers(lobby);sink^=SteamMatchmaking.GetLobbyMemberByIndex(lobby,0).m_SteamID;}
            double queries=timer.Elapsed.TotalMilliseconds;timer.Restart();
            for(int i=0;i<iterations;++i)if(Allowed(local))++sink;
            double cached=timer.Elapsed.TotalMilliseconds;
            GC.KeepAlive(sink);return new[]{queries/iterations,cached/iterations,(double)iterations};
        }

        private void OnSessionRequest(SteamNetworkingMessagesSessionRequest_t request)
        {
            RefreshPeer();
            if (Allowed(request.m_identityRemote.GetSteamID64())) SteamNetworkingMessages.AcceptSessionWithUser(ref request.m_identityRemote);
            // Ignoring unrelated identities does not contact them or accept a session.
        }

        private void OnSessionFailed(SteamNetworkingMessagesSessionFailed_t failed)
        {
            if (Allowed(failed.m_info.m_identityRemote.GetSteamID64()))
                FailRoom("Steam peer connection failed (" + failed.m_info.m_eEndReason + "): " + failed.m_info.m_szEndDebug);
        }

        private void CancelDiscovery()
        {
            if (pendingKind != "Finding rooms") return;
            ++operation; EndOperation(); rooms.Clear();
            if (discoveryCall != null) { calls.Remove(discoveryCall); discoveryCall.Dispose(); discoveryCall = null; }
        }

        private void OnSteamDisconnected(SteamServersDisconnected_t disconnected)
        {
            // Steam backend availability is separate from the existing peer
            // route. Keep pumping packets; the session still enforces liveness.
            CancelDiscovery(); activity?.CancelSearch();
            Status = "Steam is reconnecting…";
            UnityEngine.Debug.LogWarning("IDAS3 Steam services interrupted (" + disconnected.m_eResult + "); keeping the peer session.");
        }

        public void Send(byte[] data, bool reliable)
        {
            if (data == null || data.Length == 0 || data.Length > MaxPayloadBytes) { Fail("Invalid multiplayer payload length."); return; }
            if (!Available || !Allowed(peer)) return;
            SendEnvelope(data, reliable, 1);
        }

        private void SendEnvelope(byte[] data, bool reliable, byte type)
        {
            var packet = new byte[HeaderBytes + (data == null ? 0 : data.Length)];
            Write64(packet, 0, Magic); Write64(packet, 8, lobby.m_SteamID); packet[16] = type;
            if (data != null) Buffer.BlockCopy(data, 0, packet, HeaderBytes, data.Length);
            if (reliable && pendingReliable.Count > 0) { QueueReliable(packet); return; }
            var result = SendRaw(packet, reliable);
            if (result == EResult.k_EResultLimitExceeded && reliable) { QueueReliable(packet); return; }
            HandleSendResult(result, reliable);
        }

        private EResult SendRaw(byte[] packet, bool reliable)
        {
            var identity = new SteamNetworkingIdentity(); identity.SetSteamID64(peer);
            var handle = GCHandle.Alloc(packet, GCHandleType.Pinned);
            EResult result;
            try { result = SteamNetworkingMessages.SendMessageToUser(ref identity, handle.AddrOfPinnedObject(), (uint)packet.Length,
                reliable ? Constants.k_nSteamNetworkingSend_ReliableNoNagle : Constants.k_nSteamNetworkingSend_UnreliableNoDelay, Channel); }
            finally { handle.Free(); }
            return result;
        }

        private void HandleSendResult(EResult result, bool reliable)
        {
            // Unreliable snapshots can be dropped while a route is connecting
            // or congested; reliable control messages must report failures.
            if (result == EResult.k_EResultNoConnection) FailRoom("The Steam peer connection closed.");
            else if (result != EResult.k_EResultOK && reliable) Fail("Steam could not send a control message: " + result);
        }

        private void QueueReliable(byte[] packet)
        {
            if (pendingReliable.Count >= MaxPendingReliableMessages || pendingReliableBytes + packet.Length > MaxPendingReliableBytes) {
                FailRoom("The Steam connection remained congested. Please reconnect."); return;
            }
            if (pendingReliable.Count == 0) {
                reliableBlockedAt = Now; nextReliableRetry = Now + .05;
                UnityEngine.Debug.LogWarning("IDAS3 Steam send buffer full; retrying control messages in order.");
            }
            pendingReliable.Enqueue(packet); pendingReliableBytes += packet.Length;
        }

        private void FlushReliable()
        {
            if (pendingReliable.Count == 0 || Now < nextReliableRetry) return;
            if (Now - reliableBlockedAt > ReliableRetryTimeout) { FailRoom("The Steam connection remained congested for too long."); return; }
            nextReliableRetry = Now + .05;
            for (int i = 0; i < 8 && Connected && pendingReliable.Count > 0; ++i) {
                byte[] packet = pendingReliable.Peek();
                var result = SendRaw(packet, true);
                if (result == EResult.k_EResultLimitExceeded) return;
                if (result != EResult.k_EResultOK) { HandleSendResult(result, true); return; }
                pendingReliable.Dequeue(); pendingReliableBytes -= packet.Length;
            }
        }

        public void Poll()
        {
            // Callbacks can request a flush while processing a packet. Never
            // let nested polling overwrite the outstanding native batch.
            if (!Available || polling) return;
            polling = true;
            try { PollCore(); }
            finally { polling = false; }
        }

        private void PollCore()
        {
            SteamAPI.RunCallbacks();
            if (!Available) return;
            double now = Now;
            if(activity!=null){activity.Requested=ActivityRequested||PublishActivity;activity.SetState(ActivityState);activity.Poll(UnityEngine.Time.realtimeSinceStartupAsDouble,IsBusy);}
            // Callbacks normally update admission immediately. Retain a low
            // frequency audit for a missed/delayed lobby notification.
            if (InLobby && now - lastMembershipCheck >= 1) RefreshPeer();
            if (!Available) return;
            if (IsBusy && now > deadline) {
                if (pendingKind == "Finding rooms") CancelDiscovery();
                else { ++operation; EndOperation(); }
                Fail("Steam request timed out. Please retry.");
            }
            for (int batch = 0; batch < 8; ++batch) {
                if (!Available) break;
                int count = SteamNetworkingMessages.ReceiveMessagesOnChannel(Channel, received, received.Length);
                if (count <= 0) break;
                try {
                    for (int i = 0; i < count; ++i) {
                        var incoming = SteamNetworkingMessage_t.FromIntPtr(received[i]);
                        if (!Allowed(incoming.m_identityPeer.GetSteamID64()) || incoming.m_cbSize < HeaderBytes || incoming.m_cbSize > HeaderBytes + MaxPayloadBytes) continue;
                        var packet = new byte[incoming.m_cbSize]; Marshal.Copy(incoming.m_pData, packet, 0, packet.Length);
                        if (Read64(packet, 0) != Magic || Read64(packet, 8) != lobby.m_SteamID) continue;
                        if (packet[16] == 0 && packet.Length == HeaderBytes) lastReceive = now;
                        else if (packet[16] == 1 && packet.Length > HeaderBytes) {
                            lastReceive = now;
                            var data = new byte[packet.Length - HeaderBytes]; Buffer.BlockCopy(packet, HeaderBytes, data, 0, data.Length);
                            Message?.Invoke(data);
                        }
                    }
                }
                finally {
                    for (int i = 0; i < count; ++i) { SteamNetworkingMessage_t.Release(received[i]); received[i] = IntPtr.Zero; }
                }
            }
            if (!Connected) return;
            FlushReliable();
            if (!Connected) return;
            if (!Allowed(peer)) { RefreshPeer(); return; }
            if (now - lastReceive > PeerTimeout) { FailRoom("The other driver stopped responding."); return; }
            if (now - lastHeartbeat >= 1) { lastHeartbeat = now; SendEnvelope(null, false, 0); }
        }

        public void Leave()
        {
            CancelDiscovery();
            ++operation; EndOperation();
            bool notify = InLobby || peer != 0;
            if (Available) {
                ClosePeer();
                if (InLobby) {
                    if (host == local) SteamMatchmaking.SetLobbyJoinable(lobby, false);
                    SteamMatchmaking.LeaveLobby(lobby);
                }
            }
            lobby = default; host = 0; peer = 0; RemoteName = "";
            Status = Available ? "Steam ready." : "Steam is not initialized.";
            if (notify) PeerChanged?.Invoke();
        }

        private void ClosePeer()
        {
            pendingReliable.Clear(); pendingReliableBytes = 0;
            if (peer != 0 && Available) {
                var identity = new SteamNetworkingIdentity(); identity.SetSteamID64(peer);
                SteamNetworkingMessages.CloseSessionWithUser(ref identity);
            }
            peer = 0; RemoteName = "";
        }

        private bool CanStart()
        {
            if (!Idas3BuildCompatibility.IsValidMatchmakingKey(buildCompatibility)) {
                Fail("The game build could not be identified. Restart the game and try again."); return false;
            }
            if (!Available && !Initialize()) return false;
            if (!SteamUser.BLoggedOn()) { Fail("Steam is offline. Sign in and retry."); return false; }
            if (IsBusy) { Fail("A Steam room request is already in progress."); return false; }
            if (calls.Count >= 8) { Fail("Steam is still completing earlier requests. Please wait or reconnect."); return false; }
            return true;
        }
        private long Begin(string kind) { pendingKind = kind; deadline = Now + OperationTimeout; Status = kind + "…"; return ++operation; }
        private void EndOperation() { pendingKind = null; pendingLobby = default; }
        private void Fail(string reason) { Status = reason; Error?.Invoke(reason); }
        private void FailRoom(string reason) {
            UnityEngine.Debug.LogWarning("IDAS3 Steam room ended: " + reason + "; peer silence=" + (Now-lastReceive).ToString("F2",CultureInfo.InvariantCulture) + "s; pending controls=" + pendingReliable.Count);
            Leave(); Fail(reason);
        }
        private IDisposable Track<T>(SteamAPICall_t call, Action<T, bool> action)
        {
            if (call == SteamAPICall_t.Invalid) { EndOperation(); Fail("Steam rejected the room request."); return null; }
            CallResult<T> result = null;
            result = CallResult<T>.Create((value, failed) => {
                try { action(value, failed); }
                finally { calls.Remove(result); result.Dispose(); }
            });
            calls.Add(result); result.Set(call);
            return result;
        }

        public void Dispose()
        {
            activity?.Dispose();activity=null;
            Leave();
            foreach (var callback in callbacks) callback.Dispose(); callbacks.Clear();
            foreach (var call in calls) call.Dispose(); calls.Clear();
            if (Available && steamOwner == this) {
                try { controllerConfig.Dispose(); }
                finally { SteamAPI.Shutdown(); steamOwner = null; }
            }
            Available = false; local = 0; LocalName = ""; rooms.Clear(); Status = "Steam is not initialized.";
        }

        private static string CleanName(string text, string fallback)
        {
            if (string.IsNullOrWhiteSpace(text)) return fallback;
            var chars = text.Trim().ToCharArray();
            for (int i = 0; i < chars.Length; ++i) if (char.IsControl(chars[i]) || chars[i] == '<' || chars[i] == '>') chars[i] = ' ';
            return new string(chars, 0, Math.Min(chars.Length, 64));
        }
        private static void Write64(byte[] data, int at, ulong value) { for (int i = 0; i < 8; ++i) data[at + i] = (byte)(value >> (8 * i)); }
        private static ulong Read64(byte[] data, int at) { ulong v = 0; for (int i = 0; i < 8; ++i) v |= (ulong)data[at + i] << (8 * i); return v; }
        private static string EncodeRoom(ulong value)
        {
            const string alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
            string code = ""; do { code = alphabet[(int)(value % 36)] + code; value /= 36; } while (value != 0);
            return "ID3-" + code;
        }
        private static bool TryDecodeRoom(string code, out ulong value)
        {
            value = 0; code = (code ?? "").Trim().ToUpperInvariant();
            if (!code.StartsWith("ID3-", StringComparison.Ordinal) || code.Length < 5 || code.Length > 17) return false;
            try {
                checked { for (int i = 4; i < code.Length; ++i) { int d = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ".IndexOf(code[i]); if (d < 0) return false; value = value * 36 + (uint)d; } }
                return value != 0;
            }
            catch (OverflowException) { value = 0; return false; }
        }
    }
}
