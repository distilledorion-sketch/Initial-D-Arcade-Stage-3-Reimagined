using System;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Epic.OnlineServices;
using Epic.OnlineServices.Lobby;
using Epic.OnlineServices.P2P;

namespace Idas3.Multiplayer
{
    public sealed class Idas3EosTransport : IIdas3MatchmakingTransport, IIdas3PrivateRoomTransport,
        IIdas3MatchmakingServiceState, IIdas3QueuedMatchmakingTransport, IIdas3OnlineActivityTransport
    {
        internal const string Protocol = "idas3-eos-4-player-directory", BuildKey = "build", KindKey = "kind", NameKey = "name", HostKey = "host";
        internal const string StatsResetFailed = "Online battle stats could not be reset. Online play is unavailable until the reset can be saved.";
        internal const string PeerStatsReset = "The other driver's battle stats were reset. Please rejoin the room.";
        const int MatchmakingProgressTimeoutSeconds = 180;
        // Only the authenticated local player's record owner may write saves.
        // The transport/probe never acquires any administrator capability.
        internal Func<string, string[], string[], bool> ApplyStatsResets { get; set; }
        string appliedResetStamp;
        public const int MaxPayloadBytes = Idas3EosPackets.MessageLimit;
        readonly List<Idas3Room> rooms = new List<Idas3Room>();
        static readonly List<Idas3EosTransport> live = new List<Idas3EosTransport>();
        static readonly Dictionary<string, double> nextRequests = new Dictionary<string, double>();
        static string cleanupFailure;
        static double nextNativePump;
        LobbyInterface lobby;
        P2PInterface p2p;
        Idas3EosP2P route;
        Idas3EosP2P pendingAdmission;
        Idas3EosSanctions sanctions;
        Idas3EosActivity activity;
        string lobbyId, originalOwner, busy, build = "";
        string checkingPeer, verifiedPeer;
        long generation;
        bool disposed, initializing, privateRoom, peerAdmitted, admissionAnnounced, admissionAccessReady, checkingAccess;
        int members;
        double deadline, nextMembership;
        double nextAccessRefresh;
        ulong memberNotify, updateNotify, memberUpdateNotify, incomingRequestNotify;
        string diagnosticStage = "idle";
        Task pendingRequest = Task.CompletedTask, cleanupBarrier = Task.CompletedTask;
        CancellationTokenSource cancellation;
        public static long NativeLoadKeepAlivePumps { get; private set; }
        public static long NativeLoadBufferedEvents { get; private set; }
        static double Now => Idas3EosPlatform.Now;

        public Idas3EosTransport() { live.Add(this); }
        internal static void ShutdownAll()
        {
            foreach (var transport in live.ToArray()) transport.Dispose();
        }
        public static void KeepAliveDuringNativeLoad()
        {
            if (Now < nextNativePump) return;
            nextNativePump = Now + .1;
            // SDK callbacks only queue work. Normal Poll dispatches membership,
            // application events and native game messages after loading returns.
            Idas3EosPlatform.Tick(false);
            foreach (var transport in live) {
                if (transport.disposed || transport.route == null) continue;
                long before = transport.route.BufferedEvents;
                if (transport.route.KeepAliveOnly()) ++NativeLoadKeepAlivePumps;
                NativeLoadBufferedEvents += transport.route.BufferedEvents - before;
            }
        }
        public string Kind => "EOS P2P";
        public bool Available { get; private set; }
        public bool ServiceConnected => Available && Idas3EosPlatform.SignedIn;
        public bool Connected => peerAdmitted && lobbyId != null && route != null && route.Connected;
        public bool IsHost => lobbyId != null && originalOwner == LocalId;
        public string LocalId { get; private set; } = "";
        public string LocalName { get; private set; } = "Driver";
        internal void RefreshPlayerName()
        {
            if (InLobby || IsMatching) throw new InvalidOperationException("Leave the room or queue before changing your online username.");
            LocalName = Idas3EosOnlineConfig.PlayerName();
        }
        public string RemoteId => Connected ? route.RemoteId : "";
        public string RemoteName => Connected ? route.RemoteName : "";
        public string ConnectionType => route?.ConnectionType ?? "unknown";
        public string RoomCode => lobbyId ?? "";
        public string Status { get; private set; } = "Online services are not initialized.";
        public IReadOnlyList<Idas3Room> Rooms => rooms;
        public bool IsBusy => initializing || busy != null;
        public bool IsMatching { get; private set; }
        public bool InLobby => lobbyId != null;
        public bool PrivateRoom => InLobby && privateRoom;
        public ulong RoomOrder => lobbyId == null ? 0 : Order(lobbyId);
        public int RoomMembers => members;
        public bool ActivityRequested { get; set; }
        public string ActivityState { get; set; } = "online";
        public bool PublishActivity { get; set; }
        public Idas3OnlineActivity Activity => activity?.Snapshot ?? default;
        public string BuildCompatibility {
            get => build;
            set {
                string next = value ?? "";
                if ((InLobby || IsBusy) && build != next) throw new InvalidOperationException("Leave the room or queue before changing the game build.");
                build = next;
            }
        }
        internal string CompatibilityHash => Hash(Protocol + "|" + build);
        // Manual public hosts and Quick Match are one pool of compatible
        // battles. Kind stays an attribute for browser/merge behavior.
        string BattleBucket => "id3-" + CompatibilityHash.Substring(0, 32) + "-battle";
        public event Action<byte[]> Message;
        public event Action PeerChanged;
        public event Action<string> Error;

        public bool Initialize()
        {
            if (disposed) return false;
            if (Available) return true;
            if (initializing) return false;
            initializing = true; deadline = Now + 60;
            Stage("initialize");
            Status = "Connecting to online services…";
            _ = InitializeAsync(++generation);
            return false;
        }
        async Task InitializeAsync(long token)
        {
            try {
                await Idas3EosPlatform.InitializeAsync(Idas3EosOnlineConfig.Load());
                if (!Current(token)) return;
                LocalId = Idas3EosPlatform.User.ToString(); LocalName = Idas3EosOnlineConfig.PlayerName();
                ListenForPeerRequests();
                if (sanctions == null) sanctions = new Idas3EosSanctions(new Idas3OnlinePlayerDirectory());
                Stage("local-access");
                Status = "Checking online access…";
                await sanctions.RequireAllowed(Idas3EosPlatform.User, true);
                if (!Current(token)) return;
                ApplyLocalStatResets();
                if (!Current(token)) return;
                lobby = Idas3EosPlatform.Handle.GetLobbyInterface();
                if (activity == null && !Idas3EosOnlineConfig.DiagnosticRun) activity = new Idas3EosActivity(this, lobby, Idas3EosPlatform.User, () => UnityEngine.Time.realtimeSinceStartupAsDouble);
                Subscribe(); Available = true; initializing = false;
                nextAccessRefresh = Now + Idas3EosSanctions.FreshSeconds;
                Stage("ready");
                Status = "Online ready. Find a match, host a room, or enter a room code.";
            } catch (Exception ex) { if (Current(token)) { initializing = false; Available = false; DiagnosticFailure(ex); Fail(AccessFailure(ex, "Online initialization failed: ")); } }
        }
        void ListenForPeerRequests()
        {
            if (incomingRequestNotify != 0) return;
            Stage("peer-listener");
            p2p = Idas3EosPlatform.Handle.GetP2PInterface();
            // EOS silently ignores an early connection request when there is
            // no listener. Keep one installed before asynchronous access checks
            // and between rooms. Observing a request never authorizes it: only
            // the verified route may accept its exact peer and socket.
            var options = new AddNotifyPeerConnectionRequestOptions { LocalUserId = Idas3EosPlatform.User, SocketId = null };
            incomingRequestNotify = p2p.AddNotifyPeerConnectionRequest(ref options, null, (ref OnIncomingConnectionRequestInfo info) => { });
            if (incomingRequestNotify == 0) throw new InvalidOperationException("Could not register online connection notifications.");
        }
        void Subscribe()
        {
            if (memberNotify != 0) return;
            var status = new AddNotifyLobbyMemberStatusReceivedOptions();
            memberNotify = lobby.AddNotifyLobbyMemberStatusReceived(ref status, null, (ref LobbyMemberStatusReceivedCallbackInfo info) => {
                string room = info.LobbyId.ToString(), user = info.TargetUserId?.ToString(); var state = info.CurrentStatus;
                Idas3EosPlatform.Defer(() => {
                    if (disposed || lobbyId != room) return;
                    if (state == LobbyMemberStatus.Closed) { FailRoom("The online room has closed."); return; }
                    // A delayed Left notification can belong to a canceled join
                    // followed by a rejoin to the same host. Read the current
                    // authoritative cache instead of trusting its older event.
                    RefreshMembership();
                });
            });
            var updated = new AddNotifyLobbyUpdateReceivedOptions();
            updateNotify = lobby.AddNotifyLobbyUpdateReceived(ref updated, null, (ref LobbyUpdateReceivedCallbackInfo info) => {
                string room = info.LobbyId.ToString(); Idas3EosPlatform.Defer(() => { if (!disposed && lobbyId == room) RefreshMembership(); });
            });
            var member = new AddNotifyLobbyMemberUpdateReceivedOptions();
            memberUpdateNotify = lobby.AddNotifyLobbyMemberUpdateReceived(ref member, null, (ref LobbyMemberUpdateReceivedCallbackInfo info) => {
                string room = info.LobbyId.ToString(); Idas3EosPlatform.Defer(() => { if (!disposed && lobbyId == room) RefreshMembership(); });
            });
            if (memberNotify == 0 || updateNotify == 0 || memberUpdateNotify == 0) {
                Unsubscribe();
                throw new InvalidOperationException("Could not subscribe to online rooms.");
            }
        }
        public void Host(string roomName) => StartRoom(roomName, false, false, null);
        public void HostPrivate(string roomName) => StartRoom(roomName, true, false, null);
        public void HostQuickMatch(string roomName) => StartMatchmaking();
        public void StartMatchmaking() => StartRoom("Quick Match", false, true, null);
        public void CancelMatchmaking() { if (IsMatching) Leave(); }
        public void Join(string roomCode)
        {
            string code = (roomCode ?? "").Trim().ToUpperInvariant();
            if (!ValidCode(code)) { Fail("Enter a valid room code."); return; }
            StartRoom(null, false, false, code);
        }
        static bool ValidCode(string code)
        {
            if (code.Length != 28 || !code.StartsWith("ID3-", StringComparison.Ordinal)) return false;
            for (int i = 4; i < code.Length; ++i) if (!(code[i] >= '0' && code[i] <= '9') && !(code[i] >= 'A' && code[i] <= 'F')) return false;
            return true;
        }
        void StartRoom(string name, bool isPrivate, bool match, string code)
        {
            if (!CanStart()) return;
            Leave();
            long token = Begin(match ? "Finding another driver" : code == null ? "Creating room" : "Joining room", match ? MatchmakingProgressTimeoutSeconds : 60);
            Stage(match ? "matchmaking" : code == null ? "room-create" : "room-join");
            IsMatching = match;
            pendingRequest = StartRoomAsync(token, name, isPrivate, match, code, cancellation.Token, cleanupBarrier);
        }
        async Task StartRoomAsync(long token, string name, bool isPrivate, bool match, string code, CancellationToken cancel, Task prior)
        {
            try {
                await prior; EnsureCurrent(token, cancel);
                if (cleanupFailure != null) throw new Idas3EosCleanupException(cleanupFailure);
                await RequireLocalAccess(token, cancel);
                if (match) await MatchLoop(token, cancel);
                else if (code != null) await JoinRoom(token, code, cancel);
                else await CreateRoom(token, name, isPrivate, "room", cancel);
            } catch (OperationCanceledException) { }
            catch (Idas3EosCleanupException ex) {
                if (Current(token)) { Leave(); Fail(ex.Message); }
                // This pending request participates in the cleanup barrier even
                // after cancellation. Never turn failed retirement into success.
                throw;
            }
            catch (Exception ex) { if (Current(token)) { DiagnosticFailure(ex); Leave(); Fail(AccessFailure(ex, "Could not connect online: ")); } }
        }
        async Task MatchLoop(long token, CancellationToken cancel)
        {
            while (Current(token)) {
                cancel.ThrowIfCancellationRequested();
                if (Connected) return;
                if (lobbyId == null || members == 1) {
                    var candidates = await Search("queue", cancel);
                    EnsureCurrent(token, cancel);
                    RefreshMembership();
                    EnsureCurrent(token, cancel);
                    if (Connected || members > 1) { await Task.Delay(500, cancel); continue; }
                    // A healthy empty queue can wait indefinitely. Only stalled
                    // requests expire; a peer's admission deadline is separate.
                    deadline = Now + MatchmakingProgressTimeoutSeconds;
                    Idas3Room target = null;
                    foreach (var item in candidates) {
                        // Manual hosts stay put, so any of them can receive a
                        // queued driver. Ordering is only needed when both
                        // waiting hosts can leave to merge their queue rooms.
                        if (!item.QuickMatch) { target = item; break; }
                        if (target == null && (lobbyId == null || Idas3EosRoomRules.ShouldMerge(lobbyId, members, item.Code))) target = item;
                    }
                    if (target != null) {
                        if (lobbyId != null) await RetireCurrentRoom();
                        EnsureCurrent(token, cancel);
                        try { await JoinRoom(token, target.Code, cancel); }
                        catch (Idas3EosServiceException ex) when (ex.ResultCode == Result.LobbyTooManyPlayers || ex.ResultCode == Result.NotFound || ex.ResultCode == Result.LobbyInvalidSession || ex.ResultCode == Result.TooManyRequests) {
                            if (!Current(token)) return; // another driver won this vacancy; search again
                        }
                    } else if (lobbyId == null) await CreateRoom(token, "Quick Match", false, "queue", cancel);
                }
                // The total lobby-ID ordering merges simultaneous hosts without
                // both drivers abandoning rooms to join one another.
                await Task.Delay(2500 + (int)(Order(LocalId) % 700), cancel);
            }
        }
        async Task CreateRoom(long token, string name, bool isPrivate, string kind, CancellationToken cancel)
        {
            await Pace("create", 3.1, cancel); EnsureCurrent(token, cancel);
            await RequireLocalAccess(token, cancel);
            string requested = RandomRoomCode();
            var complete = new TaskCompletionSource<CreateLobbyCallbackInfo>();
            var options = new CreateLobbyOptions { LocalUserId = Idas3EosPlatform.User, MaxLobbyMembers = 2,
                // Unlisted/code-only rooms use Joinviapresence with presence
                // disabled. Inviteonly requires an EOS invitation and would
                // reject a player entering our shareable room code.
                PermissionLevel = isPrivate ? LobbyPermissionLevel.Joinviapresence : LobbyPermissionLevel.Publicadvertised,
                BucketId = BattleBucket, DisableHostMigration = true, EnableJoinById = true, LobbyId = requested,
                PresenceEnabled = false, EnableRTCRoom = false, AllowInvites = false, RejoinAfterKickRequiresInvite = true };
            lobby.CreateLobby(ref options, null, (ref CreateLobbyCallbackInfo info) => { var copy = info; Idas3EosPlatform.Defer(() => complete.TrySetResult(copy)); });
            var result = await complete.Task;
            Idas3EosPlatform.Check(result.ResultCode, "Creating room");
            string created = result.LobbyId.ToString();
            if (!Current(token) || cancel.IsCancellationRequested) { await Retire(created, true); return; }
            try {
                await SetAttributes(created, true, name, kind, cancel);
                if (!Current(token) || cancel.IsCancellationRequested) { await Retire(created, true); return; }
                lobbyId = created; originalOwner = LocalId; privateRoom = isPrivate; members = 1;
                busy = IsMatching ? "Finding another driver" : null;
                Status = "Room " + RoomCode + " — waiting for another driver.";
                RefreshMembership();
            } catch { await Retire(created, true); throw; }
        }
        async Task JoinRoom(long token, string code, CancellationToken cancel)
        {
            await Pace("join", 2.1, cancel); EnsureCurrent(token, cancel);
            await RequireLocalAccess(token, cancel);
            var complete = new TaskCompletionSource<JoinLobbyByIdCallbackInfo>();
            var options = new JoinLobbyByIdOptions { LobbyId = code, LocalUserId = Idas3EosPlatform.User, PresenceEnabled = false };
            lobby.JoinLobbyById(ref options, null, (ref JoinLobbyByIdCallbackInfo info) => { var copy = info; Idas3EosPlatform.Defer(() => complete.TrySetResult(copy)); });
            var result = await complete.Task;
            Idas3EosPlatform.Check(result.ResultCode, "Joining room");
            if (!Current(token) || cancel.IsCancellationRequested) { await Retire(code, false); return; }
            try {
                var details = Details(code);
                try {
                    var info = Info(details);
                    if (info.MaxMembers != 2 || Get(details, BuildKey) != CompatibilityHash || info.LobbyOwnerUserId?.ToString() == LocalId)
                        throw new InvalidOperationException("This room uses a different game build or online identity.");
                    originalOwner = info.LobbyOwnerUserId.ToString();
                    privateRoom = info.PermissionLevel != LobbyPermissionLevel.Publicadvertised;
                } finally { details.Release(); }
                await SetAttributes(code, false, null, null, cancel);
                if (!Current(token) || cancel.IsCancellationRequested) { await Retire(code, false); return; }
                lobbyId = code; members = 2; busy = "Connecting to driver";
                RefreshMembership();
            } catch { await Retire(code, false); throw; }
        }
        async Task SetAttributes(string id, bool host, string name, string kind, CancellationToken cancel)
        {
            await Pace("update", 2.1, cancel);
            var options = new UpdateLobbyModificationOptions { LobbyId = id, LocalUserId = Idas3EosPlatform.User };
            Idas3EosPlatform.Check(lobby.UpdateLobbyModification(ref options, out LobbyModification edit), "Preparing room data");
            try {
                if (host) {
                    Add(edit, BuildKey, CompatibilityHash, false); Add(edit, KindKey, kind, false);
                    Add(edit, NameKey, Clean(name, "Race room"), false); Add(edit, HostKey, LocalName, false);
                }
                Add(edit, NameKey, LocalName, true); Add(edit, BuildKey, CompatibilityHash, true);
                var complete = new TaskCompletionSource<Result>();
                var update = new UpdateLobbyOptions { LobbyModificationHandle = edit };
                lobby.UpdateLobby(ref update, null, (ref UpdateLobbyCallbackInfo info) => { var result = info.ResultCode; Idas3EosPlatform.Defer(() => complete.TrySetResult(result)); });
                Idas3EosPlatform.Check(await complete.Task, "Updating room data");
            } finally { edit.Release(); }
        }
        static void Add(LobbyModification edit, string key, string value, bool member)
        {
            var data = new AttributeData { Key = key, Value = new AttributeDataValue { AsUtf8 = value } };
            Result result;
            if (member) { var options = new LobbyModificationAddMemberAttributeOptions { Attribute = data, Visibility = LobbyAttributeVisibility.Public }; result = edit.AddMemberAttribute(ref options); }
            else { var options = new LobbyModificationAddAttributeOptions { Attribute = data, Visibility = LobbyAttributeVisibility.Public }; result = edit.AddAttribute(ref options); }
            Idas3EosPlatform.Check(result, "Preparing room attribute");
        }
        public void Browse()
        {
            if (!CanStart() || InLobby) return;
            rooms.Clear(); long token = Begin("Finding rooms", 40);
            pendingRequest = BrowseAsync(token, cancellation.Token);
        }
        async Task BrowseAsync(long token, CancellationToken cancel)
        {
            try {
                await RequireLocalAccess(token, cancel);
                var result = await Search("room", cancel);
                if (!Current(token)) return;
                rooms.AddRange(result); busy = null;
                Status = rooms.Count == 0 ? "No open rooms found. You can host one." : "Found " + rooms.Count + " open room(s).";
            } catch (OperationCanceledException) { }
            catch (Exception ex) { if (Current(token)) { busy = null; Fail(AccessFailure(ex, "Room search failed: ")); } }
        }
        async Task<List<Idas3Room>> Search(string kind, CancellationToken cancel)
        {
            for (int retry = 0; ; ++retry) {
                try { return await SearchOnce(kind, cancel); }
                catch (Idas3EosServiceException ex) when (retry < 3 && (ex.ResultCode == Result.TooManyRequests || ex.ResultCode == Result.TimedOut || ex.ResultCode == Result.ServiceFailure)) {
                    await Task.Delay(2000 << retry, cancel);
                }
            }
        }
        async Task<List<Idas3Room>> SearchOnce(string kind, CancellationToken cancel)
        {
            await Pace("search", 2.1, cancel);
            var create = new CreateLobbySearchOptions { MaxResults = 50 };
            Idas3EosPlatform.Check(lobby.CreateLobbySearch(ref create, out LobbySearch search), "Creating room search");
            try {
                var bucket = new LobbySearchSetParameterOptions { Parameter = new AttributeData { Key = LobbyInterface.SEARCH_BUCKET_ID, Value = new AttributeDataValue { AsUtf8 = BattleBucket } }, ComparisonOp = ComparisonOp.Equal };
                Idas3EosPlatform.Check(search.SetParameter(ref bucket), "Setting room search");
                var slots = new LobbySearchSetParameterOptions { Parameter = new AttributeData { Key = LobbyInterface.SEARCH_MINSLOTSAVAILABLE, Value = new AttributeDataValue { AsInt64 = 1 } }, ComparisonOp = ComparisonOp.Greaterthanorequal };
                Idas3EosPlatform.Check(search.SetParameter(ref slots), "Setting room vacancies");
                if (kind == "room") {
                    // Filter before EOS applies its result limit, so waiting
                    // queue lobbies cannot crowd manual hosts out of Browse.
                    var manual = new LobbySearchSetParameterOptions { Parameter = new AttributeData { Key = KindKey, Value = new AttributeDataValue { AsUtf8 = "room" } }, ComparisonOp = ComparisonOp.Equal };
                    Idas3EosPlatform.Check(search.SetParameter(ref manual), "Setting room type");
                }
                var done = new TaskCompletionSource<Result>();
                var find = new LobbySearchFindOptions { LocalUserId = Idas3EosPlatform.User };
                search.Find(ref find, null, (ref LobbySearchFindCallbackInfo info) => { var result = info.ResultCode; Idas3EosPlatform.Defer(() => done.TrySetResult(result)); });
                Idas3EosPlatform.Check(await done.Task, "Finding rooms"); cancel.ThrowIfCancellationRequested();
                var roomsFound = new List<Idas3Room>();
                var countOptions = new LobbySearchGetSearchResultCountOptions();
                uint count = search.GetSearchResultCount(ref countOptions);
                for (uint i = 0; i < count && i < 50; ++i) {
                    var copy = new LobbySearchCopySearchResultByIndexOptions { LobbyIndex = i };
                    if (search.CopySearchResultByIndex(ref copy, out LobbyDetails details) != Result.Success) continue;
                    try {
                        var info = Info(details);
                        string id = info.LobbyId.ToString();
                        string roomKind = Get(details, KindKey);
                        bool acceptedKind = roomKind == "room" || kind == "queue" && roomKind == "queue";
                        if (!ValidCode(id) || info.MaxMembers != 2 || info.AvailableSlots != 1 || info.PermissionLevel != LobbyPermissionLevel.Publicadvertised ||
                            info.LobbyOwnerUserId?.ToString() == LocalId || Get(details, BuildKey) != CompatibilityHash || !acceptedKind) continue;
                        roomsFound.Add(new Idas3Room { Code = id, Name = Clean(Get(details, NameKey), "Race room"), HostName = Clean(Get(details, HostKey), "Driver"), Members = 1, Capacity = 2, Order = Order(id), QuickMatch = roomKind == "queue" });
                    } finally { details.Release(); }
                }
                roomsFound.Sort((a, b) => string.CompareOrdinal(a.Code, b.Code));
                return roomsFound;
            } finally { search.Release(); }
        }
        LobbyDetails Details(string id)
        {
            var options = new CopyLobbyDetailsHandleOptions { LocalUserId = Idas3EosPlatform.User, LobbyId = id };
            Idas3EosPlatform.Check(lobby.CopyLobbyDetailsHandle(ref options, out LobbyDetails value), "Reading room membership");
            return value;
        }
        static LobbyDetailsInfo Info(LobbyDetails details)
        {
            var options = new LobbyDetailsCopyInfoOptions();
            Idas3EosPlatform.Check(details.CopyInfo(ref options, out LobbyDetailsInfo? value), "Reading room data");
            if (!value.HasValue) throw new InvalidOperationException("The room is no longer available.");
            return value.Value;
        }
        static string Get(LobbyDetails details, string key, ProductUserId user = null)
        {
            Epic.OnlineServices.Lobby.Attribute? value;
            Result result;
            if (user == null) { var options = new LobbyDetailsCopyAttributeByKeyOptions { AttrKey = key }; result = details.CopyAttributeByKey(ref options, out value); }
            else { var options = new LobbyDetailsCopyMemberAttributeByKeyOptions { AttrKey = key, TargetUserId = user }; result = details.CopyMemberAttributeByKey(ref options, out value); }
            return result == Result.Success && value.HasValue && value.Value.Data.HasValue ? value.Value.Data.Value.Value.AsUtf8?.ToString() ?? "" : "";
        }
        void RefreshMembership()
        {
            if (lobbyId == null || disposed) return;
            try {
                var details = Details(lobbyId);
                try {
                    var info = Info(details);
                    var count = new LobbyDetailsGetMemberCountOptions(); members = (int)details.GetMemberCount(ref count);
                    if (members > 2) throw new InvalidOperationException("Online room has an invalid player list.");
                    var memberIds = new List<string>();
                    ProductUserId peer = null;
                    for (uint i = 0; i < members; ++i) {
                        var index = new LobbyDetailsGetMemberByIndexOptions { MemberIndex = i };
                        var member = details.GetMemberByIndex(ref index);
                        memberIds.Add(member?.ToString());
                        if (member?.ToString() != LocalId) peer = member;
                    }
                    string invalid = Idas3EosRoomRules.ValidateMembership(LocalId, originalOwner, info.LobbyOwnerUserId?.ToString(), info.MaxMembers, memberIds, CompatibilityHash, Get(details, BuildKey));
                    if (invalid != null) throw new InvalidOperationException(invalid);
                    if (route != null && (peer == null || peer.ToString() != route.RemoteId)) throw new InvalidOperationException("The other driver left the room.");
                    if (route == null && peer != null) {
                        string peerBuild = Get(details, BuildKey, peer);
                        if (peerBuild.Length == 0) return; // Join has completed; member attributes are still arriving.
                        if (peerBuild != CompatibilityHash) throw new InvalidOperationException("The other driver is using a different game build.");
                        if (verifiedPeer != peer.ToString() || !sanctions.IsClearWithin(peer, Idas3EosSanctions.FreshSeconds)) {
                            if (checkingPeer != peer.ToString()) {
                                checkingPeer = peer.ToString(); busy = "Checking driver access";
                                Stage("peer-access");
                                deadline = Now + 60;
                                _ = VerifyPeerAccess(generation, lobbyId, peer);
                            }
                            return;
                        }
                        if (!sanctions.IsClearWithin(Idas3EosPlatform.User, Idas3EosSanctions.FreshSeconds)) {
                            if (!checkingAccess) _ = RefreshAccess(generation);
                            return;
                        }
                        ApplyLocalStatResets();
                        if (lobbyId == null) return;
                        route = new Idas3EosP2P(this, peer, Clean(Get(details, NameKey, peer), "Driver"), lobbyId, CompatibilityHash, IsHost);
                        Stage("peer-connect");
                        busy = "Connecting to driver";
                        deadline = Now + 45;
                    }
                } finally { details.Release(); }
            } catch (Exception ex) { DiagnosticFailure(ex); FailRoom(ex.Message); }
        }
        public void Poll()
        {
            if (disposed) return;
            Idas3EosPlatform.Tick(true);
            sanctions?.Poll();
            if ((initializing || busy != null) && Now > deadline) { string description = initializing ? "Online initialization" : busy; Diagnostic("timeout"); Leave(); Fail(description + " timed out. Please retry."); }
            if (Idas3EosPlatform.Failure != null && InLobby) { FailRoom(Idas3EosPlatform.Failure); return; }
            PollAccess();
            if (InLobby && Now >= nextMembership) { nextMembership = Now + 1; RefreshMembership(); }
            route?.Poll();
            activity?.Poll(PublishActivity, ActivityRequested, ActivityState);
        }
        public void Send(byte[] data, bool reliable)
        {
            if (!Connected || data == null) return;
            if (data.Length < 1 || data.Length > MaxPayloadBytes) { FailRoom("A game message exceeded the online packet limit."); return; }
            route.SendGame(data, reliable);
        }
        internal bool Admitted(Idas3EosP2P source)
        {
            if (route != source || lobbyId == null || !source.Connected) return false;
            if (peerAdmitted) return true;
            if (pendingAdmission != null && !admissionAccessReady) return false;
            if (!sanctions.IsClearWithin(Idas3EosPlatform.User, Idas3EosSanctions.FreshSeconds) ||
                !sanctions.IsClearWithin(ProductUserId.FromString(source.RemoteId), Idas3EosSanctions.FreshSeconds)) {
                if (pendingAdmission == null) {
                    pendingAdmission = source; admissionAccessReady = false;
                    busy = "Checking driver access"; deadline = Now + 60;
                    Stage("admission-access");
                    _ = VerifyAdmissionAccess(generation, lobbyId, source);
                } else FailRoom(Idas3EosSanctions.Unavailable);
                return false;
            }
            if (source.LocalResetStamp != ResetStamp(Idas3EosPlatform.User) ||
                source.RemoteResetStamp != ResetStamp(ProductUserId.FromString(source.RemoteId))) {
                FailRoom(PeerStatsReset); return false;
            }
            peerAdmitted = true; pendingAdmission = null;
            busy = null; IsMatching = false; Status = "Connected to " + source.RemoteName + ".";
            Stage("admitted");
            return true;
        }
        internal void AnnounceAdmission(Idas3EosP2P source)
        {
            if (route != source || !Connected || admissionAnnounced) return;
            // The route enables game sending before this callback, so the
            // session's synchronous first hello is retained after admission.
            admissionAnnounced = true;
            PeerChanged?.Invoke();
        }
        async Task VerifyAdmissionAccess(long token, string room, Idas3EosP2P source)
        {
            try {
                // Parallel verification avoids aging the local snapshot while
                // waiting for a second sequential service request. Existing
                // in-flight queries are shared by the sanctions adapter.
                await Task.WhenAll(sanctions.RequireAllowed(Idas3EosPlatform.User, true, true),
                    sanctions.RequireAllowed(ProductUserId.FromString(source.RemoteId), false, true));
                if (!Current(token) || lobbyId != room || route != source || pendingAdmission != source) return;
                ApplyLocalStatResets();
                if (!Current(token) || route != source) return;
                admissionAccessReady = true;
                nextAccessRefresh = Now + Idas3EosSanctions.FreshSeconds;
                // Normal Poll rechecks current membership and reset stamps
                // before authorizing the route or delivering buffered data.
            } catch (Exception ex) {
                if (!Current(token) || lobbyId != room || route != source || pendingAdmission != source) return;
                DiagnosticFailure(ex);
                if (ex is Idas3EosStatsResetException || sanctions.IsBlocked(Idas3EosPlatform.User))
                    BlockLocalAccess(ex is Idas3EosStatsResetException ? StatsResetFailed : Idas3EosSanctions.LocalRestriction);
                else FailRoom(AccessFailure(ex, "Could not verify connection: "));
            }
        }
        internal void Deliver(Idas3EosP2P source, byte[] data) { if (route == source && Connected) Message?.Invoke(data); }
        internal void RouteFailed(Idas3EosP2P source, string detail) { if (route == source) FailRoom(detail); }
        async Task RetireCurrentRoom()
        {
            string old = lobbyId; bool host = IsHost;
            lobbyId = null; originalOwner = null; members = 0;
            checkingPeer = verifiedPeer = null;
            pendingAdmission = null; admissionAccessReady = false;
            route?.Dispose(); route = null;
            if (old != null) await Retire(old, host);
        }
        async Task Retire(string id, bool host)
        {
            if (lobby == null || string.IsNullOrEmpty(id) || Idas3EosPlatform.IsStopping) return;
            for (int retry = 0; retry < 3; ++retry) {
                await Pace("leave", 2.1, CancellationToken.None);
                if (Idas3EosPlatform.IsStopping) return;
                var done = new TaskCompletionSource<Result>();
                if (host) {
                    var options = new DestroyLobbyOptions { LobbyId = id, LocalUserId = Idas3EosPlatform.User };
                    lobby.DestroyLobby(ref options, null, (ref DestroyLobbyCallbackInfo info) => { var result = info.ResultCode; Idas3EosPlatform.Defer(() => done.TrySetResult(result)); });
                } else {
                    var options = new LeaveLobbyOptions { LobbyId = id, LocalUserId = Idas3EosPlatform.User };
                    lobby.LeaveLobby(ref options, null, (ref LeaveLobbyCallbackInfo info) => { var result = info.ResultCode; Idas3EosPlatform.Defer(() => done.TrySetResult(result)); });
                }
                var status = await done.Task;
                if (status == Result.Success || status == Result.NotFound || status == Result.NoChange) return;
                await Task.Delay(1000 * (retry + 1));
            }
            // Do not let an old per-player Leave race a new join with this same
            // identity. A failed cleanup is visible on the next room request.
            cleanupFailure = "The previous online room could not be closed. Restart the game to reconnect.";
            throw new Idas3EosCleanupException(cleanupFailure);
        }
        public void Leave()
        {
            // A closed route has already changed Connected to false by the
            // time its failure reaches us. Remember admission so every peer
            // departure still notifies the session exactly once.
            bool notify = admissionAnnounced; peerAdmitted = admissionAnnounced = false;
            ++generation; cancellation?.Cancel(); cancellation?.Dispose(); cancellation = null;
            initializing = false; busy = null; IsMatching = false;
            checkingAccess = false;
            Task oldRequest = pendingRequest; pendingRequest = Task.CompletedTask;
            Task retire = RetireCurrentRoom();
            cleanupBarrier = CleanupAll(cleanupBarrier, oldRequest, retire);
            // Awaiting the barrier still propagates failure to new requests.
            // Observe it even if the player never opens Online again.
            _ = cleanupBarrier.ContinueWith(task => { var observed = task.Exception; }, TaskContinuationOptions.OnlyOnFaulted);
            Status = Available ? "Online ready." : "Online services are not connected.";
            if (notify) PeerChanged?.Invoke();
        }
        static async Task CleanupAll(Task prior, Task request, Task retire)
        {
            // Task exceptions are retained by the barrier and observed before
            // the next operation; an unrelated new room can never be detached.
            await Task.WhenAll(prior, request, retire);
        }
        public void Dispose()
        {
            if (disposed) return;
            Leave(); disposed = true; Available = false; live.Remove(this);
            sanctions?.Dispose();
            activity?.Dispose();
            Unsubscribe();
            if (incomingRequestNotify != 0) p2p.RemoveNotifyPeerConnectionRequest(incomingRequestNotify);
            incomingRequestNotify = 0;
        }
        void Unsubscribe()
        {
            if (lobby != null) {
                if (memberNotify != 0) lobby.RemoveNotifyLobbyMemberStatusReceived(memberNotify);
                if (updateNotify != 0) lobby.RemoveNotifyLobbyUpdateReceived(updateNotify);
                if (memberUpdateNotify != 0) lobby.RemoveNotifyLobbyMemberUpdateReceived(memberUpdateNotify);
            }
            memberNotify = updateNotify = memberUpdateNotify = 0;
        }
        bool CanStart()
        {
            if (disposed) return false;
            if (!Available) { Initialize(); return false; }
            if (cleanupFailure != null) { Fail(cleanupFailure); return false; }
            if (Idas3EosPlatform.Failure != null) { Fail(Idas3EosPlatform.Failure); return false; }
            if (!ServiceConnected) { Fail("Online sign-in is reconnecting. Please retry shortly."); return false; }
            if (IsBusy) return false;
            if (string.IsNullOrWhiteSpace(build)) { Fail("The game build identity is not ready."); return false; }
            return true;
        }
        async Task RequireLocalAccess(long token, CancellationToken cancel)
        {
            try {
                await sanctions.RequireAllowed(Idas3EosPlatform.User, true);
                EnsureCurrent(token, cancel);
                ApplyLocalStatResets();
                EnsureCurrent(token, cancel);
            } catch (Idas3EosRestrictedException) {
                if (Current(token)) Available = false;
                throw;
            }
        }
        async Task VerifyPeerAccess(long token, string room, ProductUserId peer)
        {
            try {
                await sanctions.RequireAllowed(peer, false, true);
                if (!Current(token) || lobbyId != room || checkingPeer != peer.ToString()) return;
                checkingPeer = null; verifiedPeer = peer.ToString();
                // Re-read the current member list after the asynchronous check.
                // A result for a departed/replaced driver cannot admit a peer.
                RefreshMembership();
            } catch (Exception ex) {
                if (Current(token) && lobbyId == room && checkingPeer == peer.ToString()) { DiagnosticFailure(ex); FailRoom(ex.Message); }
            }
        }
        void PollAccess()
        {
            if (!Available || sanctions == null) return;
            if (sanctions.IsBlocked(Idas3EosPlatform.User)) { BlockLocalAccess(Idas3EosSanctions.LocalRestriction); return; }
            ProductUserId peer = route == null ? null : ProductUserId.FromString(route.RemoteId);
            if (peer != null && sanctions.IsBlocked(peer)) { FailRoom(Idas3EosSanctions.PeerRestriction); return; }
            if (peer != null && sanctions.IsClearWithin(peer, Idas3EosSanctions.GraceSeconds) &&
                route.RemoteResetStamp != ResetStamp(peer)) { FailRoom(PeerStatsReset); return; }
            // Only an already-admitted connection may ride out a short service
            // outage. No new room/peer uses this extended cache allowance.
            double age = Connected ? Idas3EosSanctions.GraceSeconds : Idas3EosSanctions.FreshSeconds;
            if (Connected && (!sanctions.IsClearWithin(Idas3EosPlatform.User, age) || !sanctions.IsClearWithin(peer, age))) {
                BlockLocalAccess(Idas3EosSanctions.Unavailable); return;
            }
            if (pendingAdmission == null && !checkingAccess && Now >= nextAccessRefresh) _ = RefreshAccess(generation);
        }
        async Task RefreshAccess(long token)
        {
            checkingAccess = true;
            string room = lobbyId; var previousRoute = route;
            try {
                await sanctions.RequireAllowed(Idas3EosPlatform.User, true, true);
                if (!Current(token)) return;
                ApplyLocalStatResets();
                if (!Current(token)) return;
                if (previousRoute != null && route == previousRoute)
                    await sanctions.RequireAllowed(ProductUserId.FromString(previousRoute.RemoteId), false, true);
                if (!Current(token)) return;
                nextAccessRefresh = Now + Idas3EosSanctions.FreshSeconds;
            } catch (Exception ex) {
                if (!Current(token)) return;
                if (ex is Idas3EosStatsResetException) BlockLocalAccess(StatsResetFailed);
                else if (sanctions.IsBlocked(Idas3EosPlatform.User)) BlockLocalAccess(Idas3EosSanctions.LocalRestriction);
                else if (ex is Idas3EosRestrictedException && lobbyId == room && route == previousRoute) FailRoom(ex.Message);
                else if (!Connected || !sanctions.IsClearWithin(Idas3EosPlatform.User, Idas3EosSanctions.GraceSeconds) ||
                    !sanctions.IsClearWithin(ProductUserId.FromString(route.RemoteId), Idas3EosSanctions.GraceSeconds)) BlockLocalAccess(Idas3EosSanctions.Unavailable);
                else nextAccessRefresh = Now + Idas3EosSanctions.RetrySeconds;
            } finally { if (Current(token)) checkingAccess = false; }
        }
        internal string ResetStamp(ProductUserId user)
        {
            // Each side obtains these IDs independently from EOS. The peer's
            // P2P hello must acknowledge that same policy before game packets.
            return Hash(user.ToString() + "|level:" + string.Join(",", sanctions.LevelResetIds(user)) +
                "|full:" + string.Join(",", sanctions.FullResetIds(user)));
        }
        void ApplyLocalStatResets()
        {
            var user = Idas3EosPlatform.User;
            string stamp = ResetStamp(user);
            if (appliedResetStamp == stamp) return;
            string[] level = sanctions.LevelResetIds(user), full = sanctions.FullResetIds(user);
            try {
                if (level.Length != 0 || full.Length != 0) {
                    if (ApplyStatsResets == null) throw new InvalidOperationException("No local battle record owner is available.");
                    ApplyStatsResets(user.ToString(), level, full);
                }
                appliedResetStamp = stamp;
            } catch (Exception) {
                Available = false;
                throw new Idas3EosStatsResetException();
            }
        }
        void BlockLocalAccess(string message)
        {
            Available = false; Leave(); Fail(message);
        }
        static string AccessFailure(Exception error, string prefix)
        {
            // Keep known user-facing messages intact so the custom-menu
            // language catalog can translate the complete restriction text.
            return error is Idas3EosRestrictedException || error is Idas3EosStatsResetException || error.Message == Idas3EosSanctions.Unavailable
                ? error.Message : prefix + error.Message;
        }
        long Begin(string description, int seconds)
        {
            cancellation?.Cancel(); cancellation?.Dispose(); cancellation = new CancellationTokenSource();
            // An older background check may complete after this generation.
            // Its finally block must not leave the new operation marked busy.
            checkingAccess = false;
            busy = description; deadline = Now + seconds; Status = description + "…"; return ++generation;
        }
        bool Current(long token) => !disposed && generation == token;
        void Stage(string stage) { diagnosticStage = stage; Diagnostic("started"); }
        void Diagnostic(string outcome) => UnityEngine.Debug.Log("[IDAS3 EOS] stage=" + diagnosticStage + " outcome=" + outcome);
        void DiagnosticFailure(Exception error) => Diagnostic(error is Idas3EosServiceException service ? service.ResultCode.ToString() :
            error is Idas3EosRestrictedException ? "restricted" : error is Idas3EosStatsResetException ? "reset-failed" : "failed");
        void EnsureCurrent(long token, CancellationToken cancel) { cancel.ThrowIfCancellationRequested(); if (!Current(token)) throw new OperationCanceledException(); }
        void FailRoom(string detail) { Leave(); Fail(detail); }
        void Fail(string detail) { Status = detail; Error?.Invoke(detail); }
        internal static bool TryPace(string kind, double interval)
        {
            double now = Now;
            if (nextRequests.TryGetValue(kind, out double next) && next > now) return false;
            nextRequests[kind] = now + interval;
            return true;
        }
        internal static async Task Pace(string kind, double interval, CancellationToken cancel)
        {
            double now = Now;
            double start = nextRequests.TryGetValue(kind, out double next) ? Math.Max(now, next) : now;
            nextRequests[kind] = start + interval;
            if (start > now) await Task.Delay((int)Math.Ceiling((start - now) * 1000), cancel);
            cancel.ThrowIfCancellationRequested();
        }
        internal static string Clean(string value, string fallback, int length = 48)
        {
            var clean = new StringBuilder(); foreach (char c in (value ?? "").Trim()) if (!char.IsControl(c) && c != '<' && c != '>' && clean.Length < length) clean.Append(c);
            return clean.Length == 0 ? fallback : clean.ToString();
        }
        internal static string Hash(string value) { using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(value))).Replace("-", "").ToLowerInvariant(); }
        static ulong Order(string value) { using (var sha = SHA256.Create()) return BitConverter.ToUInt64(sha.ComputeHash(Encoding.UTF8.GetBytes(value)), 0); }
        static string RandomRoomCode() { var bytes = new byte[12]; using (var rng = RandomNumberGenerator.Create()) rng.GetBytes(bytes); return "ID3-" + BitConverter.ToString(bytes).Replace("-", ""); }
    }
    internal sealed class Idas3EosStatsResetException : InvalidOperationException
    {
        internal Idas3EosStatsResetException() : base(Idas3EosTransport.StatsResetFailed) { }
    }
    internal sealed class Idas3EosCleanupException : InvalidOperationException
    {
        internal Idas3EosCleanupException(string message) : base(message) { }
    }
}
