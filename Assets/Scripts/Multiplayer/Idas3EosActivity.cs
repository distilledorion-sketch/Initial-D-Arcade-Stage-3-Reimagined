using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Epic.OnlineServices;
using Epic.OnlineServices.Lobby;

namespace Idas3.Multiplayer
{
    // A separate, full one-member lobby advertises an authenticated owner.
    // It never participates in matchmaking, joins, or peer-to-peer traffic.
    internal sealed class Idas3EosActivity : IDisposable
    {
        internal const string Bucket = "idas3-activity-v1";
        internal const string SchemaKey = "activity_schema", StateKey = "activity_state", TimeKey = "activity_time";
        internal const int ResultLimit = 200, LifetimeSeconds = 90, SnapshotSeconds = 45;
        readonly Idas3EosTransport owner;
        readonly LobbyInterface lobby;
        readonly ProductUserId user;
        readonly Func<double> observedNow;
        readonly string bucket;
        static Task retirement = Task.CompletedTask;
        TaskCompletionSource<bool> retired;
        string presenceId, desiredState = "online";
        bool disposed, publish, writing, querying, firstPublished;
        double nextWrite, nextQuery, observedAt;
        Idas3OnlineActivity snapshot;
        static double Now => Idas3EosPlatform.Now;
        bool CanRequest => !disposed && !Idas3EosPlatform.IsStopping && Idas3EosPlatform.SignedIn;
        internal Idas3OnlineActivity Snapshot => snapshot.Available && Now - observedAt <= SnapshotSeconds ? snapshot : default;

        internal Idas3EosActivity(Idas3EosTransport owner, LobbyInterface lobby, ProductUserId user, Func<double> observedNow, string diagnosticBucket = null)
        {
            this.owner = owner; this.lobby = lobby; this.user = user; this.observedNow = observedNow;
            // Only explicitly isolated diagnostics use another bucket.
            bucket = diagnosticBucket ?? Bucket;
        }
        internal void Poll(bool shouldPublish, bool requested, string state)
        {
            if (disposed) return;
            bool nextPublish = shouldPublish && owner.ServiceConnected;
            if (publish != nextPublish) nextWrite = Math.Min(nextWrite, Now);
            publish = nextPublish;
            string nextState = ValidState(state) ? state : "online";
            if (nextState != desiredState) { desiredState = nextState; nextWrite = Math.Min(nextWrite, Now + 5); }
            if (!CanRequest) return;
            if (!writing && Now >= nextWrite && (publish || presenceId != null)) {
                writing = true; _ = WritePresence();
            }
            if (!querying && (requested || publish) && Now >= nextQuery) {
                querying = true; _ = Query();
            }
        }
        async Task WritePresence()
        {
            try {
                if (!publish) { await RemovePresence(); nextWrite = Now + 5; return; }
                if (presenceId == null) {
                    // Retired publishers must finish cleanup before a new one
                    // consumes another lobby slot after an Online/LAN switch.
                    await retirement;
                    if (!Idas3EosTransport.TryPace("create", 3.1)) { nextWrite = Now + .25; return; }
                    if (!CanRequest || !publish) return;
                    var complete = new TaskCompletionSource<string>();
                    var options = new CreateLobbyOptions { LocalUserId = user, MaxLobbyMembers = 1,
                        PermissionLevel = LobbyPermissionLevel.Publicadvertised, BucketId = bucket,
                        DisableHostMigration = true, EnableJoinById = false, PresenceEnabled = false,
                        EnableRTCRoom = false, AllowInvites = false, RejoinAfterKickRequiresInvite = true };
                    lobby.CreateLobby(ref options, null, (ref CreateLobbyCallbackInfo info) => {
                        var result = info.ResultCode; string id = info.LobbyId?.ToString();
                        Idas3EosPlatform.Defer(() => {
                            if (result == Result.Success && !string.IsNullOrEmpty(id)) complete.TrySetResult(id);
                            else complete.TrySetException(new Idas3EosServiceException(result, "Publishing online activity"));
                        });
                    });
                    try { await Finish(complete.Task); }
                    catch (TimeoutException) {
                        retirement = Task.WhenAll(retirement, RetireLateCreation(complete.Task));
                        throw;
                    }
                    presenceId = await complete.Task;
                    firstPublished = false;
                }
                if (disposed || !publish) { await RemovePresence(); return; }
                if (!Idas3EosTransport.TryPace("update", 2.1)) { nextWrite = Now + .25; return; }
                if (!CanRequest || !publish) return;
                string state = desiredState;
                var editOptions = new UpdateLobbyModificationOptions { LobbyId = presenceId, LocalUserId = user };
                Idas3EosPlatform.Check(lobby.UpdateLobbyModification(ref editOptions, out LobbyModification edit), "Preparing online activity");
                Task pending = null;
                try {
                    Add(edit, SchemaKey, new AttributeDataValue { AsInt64 = 1 });
                    Add(edit, StateKey, new AttributeDataValue { AsUtf8 = state });
                    Add(edit, TimeKey, new AttributeDataValue { AsInt64 = DateTimeOffset.UtcNow.ToUnixTimeSeconds() });
                    var done = new TaskCompletionSource<Result>();
                    var update = new UpdateLobbyOptions { LobbyModificationHandle = edit };
                    lobby.UpdateLobby(ref update, null, (ref UpdateLobbyCallbackInfo info) => {
                        var result = info.ResultCode; Idas3EosPlatform.Defer(() => done.TrySetResult(result));
                    });
                    pending = done.Task; await Finish(pending);
                    Idas3EosPlatform.Check(await done.Task, "Updating online activity");
                    nextWrite = Now + (state == desiredState ? 30 : 5);
                    if (!firstPublished) { firstPublished = true; nextQuery = Math.Min(nextQuery, Now + 4); }
                } finally { ReleaseAfter(pending, edit.Release); }
                if (disposed || !publish) await RemovePresence();
            } catch (Idas3EosServiceException error) {
                if (error.ResultCode == Result.NotFound || error.ResultCode == Result.LobbyInvalidSession) presenceId = null;
                nextWrite = Now + 30;
            } catch (Exception) { nextWrite = Now + 30; }
            finally { writing = false; if (disposed) { writing = true; _ = DisposePresence(); } }
        }
        static void Add(LobbyModification edit, string key, AttributeDataValue value)
        {
            var options = new LobbyModificationAddAttributeOptions { Attribute = new AttributeData { Key = key, Value = value }, Visibility = LobbyAttributeVisibility.Public };
            Idas3EosPlatform.Check(edit.AddAttribute(ref options), "Preparing online activity attribute");
        }
        async Task Query()
        {
            nextQuery = Now + 30;
            try {
                // Join the normal search budget at most once per refresh, so a
                // continuously active Quick Match cannot starve the census.
                await Idas3EosTransport.Pace("search", 2.1, CancellationToken.None);
                if (!CanRequest) return;
                var create = new CreateLobbySearchOptions { MaxResults = ResultLimit };
                Idas3EosPlatform.Check(lobby.CreateLobbySearch(ref create, out LobbySearch search), "Creating activity search");
                Task pending = null;
                try {
                    Parameter(search, LobbyInterface.SEARCH_BUCKET_ID, new AttributeDataValue { AsUtf8 = bucket }, ComparisonOp.Equal);
                    // Explicitly include full one-person presence lobbies.
                    Parameter(search, LobbyInterface.SEARCH_MINSLOTSAVAILABLE, new AttributeDataValue { AsInt64 = 0 }, ComparisonOp.Greaterthanorequal);
                    var done = new TaskCompletionSource<Result>(); var find = new LobbySearchFindOptions { LocalUserId = user };
                    search.Find(ref find, null, (ref LobbySearchFindCallbackInfo info) => {
                        var result = info.ResultCode; Idas3EosPlatform.Defer(() => done.TrySetResult(result));
                    });
                    pending = done.Task;
                    await Finish(pending);
                    Idas3EosPlatform.Check(await done.Task, "Finding online activity");
                    if (disposed) return;
                    var countOptions = new LobbySearchGetSearchResultCountOptions();
                    uint raw = search.GetSearchResultCount(ref countOptions);
                    var entries = new List<Entry>();
                    for (uint i = 0; i < raw && i < ResultLimit; ++i) {
                        var copy = new LobbySearchCopySearchResultByIndexOptions { LobbyIndex = i };
                        if (search.CopySearchResultByIndex(ref copy, out LobbyDetails details) != Result.Success) continue;
                        try {
                            var infoOptions = new LobbyDetailsCopyInfoOptions();
                            if (details.CopyInfo(ref infoOptions, out LobbyDetailsInfo? info) != Result.Success || !info.HasValue) continue;
                            var value = info.Value;
                            if (value.MaxMembers != 1 || value.AvailableSlots != 0 || value.PermissionLevel != LobbyPermissionLevel.Publicadvertised || value.BucketId?.ToString() != bucket || value.LobbyOwnerUserId == null || !value.LobbyOwnerUserId.IsValid()) continue;
                            if (LongAttribute(details, SchemaKey) != 1) continue;
                            entries.Add(new Entry { Owner = value.LobbyOwnerUserId.ToString(), State = StringAttribute(details, StateKey), At = LongAttribute(details, TimeKey) });
                        } finally { details.Release(); }
                    }
                    snapshot = Summarize(entries, raw, DateTimeOffset.UtcNow.ToUnixTimeSeconds(), observedNow());
                    observedAt = Now;
                } finally { ReleaseAfter(pending, search.Release); }
            } catch (Exception) { nextQuery = Now + 30; }
            finally { querying = false; }
        }
        static void Parameter(LobbySearch search, string key, AttributeDataValue value, ComparisonOp comparison)
        {
            var options = new LobbySearchSetParameterOptions { Parameter = new AttributeData { Key = key, Value = value }, ComparisonOp = comparison };
            Idas3EosPlatform.Check(search.SetParameter(ref options), "Preparing activity search");
        }
        static AttributeDataValue? Attribute(LobbyDetails details, string key)
        {
            var options = new LobbyDetailsCopyAttributeByKeyOptions { AttrKey = key };
            return details.CopyAttributeByKey(ref options, out Epic.OnlineServices.Lobby.Attribute? value) == Result.Success && value.HasValue && value.Value.Data.HasValue ? value.Value.Data.Value.Value : (AttributeDataValue?)null;
        }
        static long LongAttribute(LobbyDetails details, string key) { var value = Attribute(details, key); return value.HasValue ? value.Value.AsInt64 ?? 0 : 0; }
        static string StringAttribute(LobbyDetails details, string key) { var value = Attribute(details, key); return value.HasValue ? value.Value.AsUtf8?.ToString() ?? "" : ""; }
        internal struct Entry { internal string Owner, State; internal long At; }
        internal static bool ValidState(string value) => value == "online" || value == "queuing" || value == "racing";
        internal static Idas3OnlineActivity Summarize(IEnumerable<Entry> entries, uint raw, long utcNow, double observedNow)
        {
            var owners = new Dictionary<string, Entry>(StringComparer.Ordinal);
            foreach (var item in entries) {
                if (string.IsNullOrEmpty(item.Owner) || !ValidState(item.State) || item.At < utcNow - LifetimeSeconds || item.At > utcNow + 30) continue;
                if (!owners.TryGetValue(item.Owner, out Entry prior) || prior.At < item.At) owners[item.Owner] = item;
            }
            var result = new Idas3OnlineActivity { Available = true, Limited = raw >= ResultLimit, ObservedAt = observedNow };
            foreach (var entry in owners.Values) { if (result.Online == ResultLimit) { result.Limited = true; break; } ++result.Online; if (entry.State == "queuing") ++result.Queuing; else if (entry.State == "racing") ++result.Racing; }
            return result;
        }
        async Task RemovePresence()
        {
            if (presenceId == null || Idas3EosPlatform.IsStopping) return;
            string id = presenceId;
            await RemoveLobby(id);
            if (presenceId == id) presenceId = null;
        }
        async Task RemoveLobby(string id)
        {
            await Idas3EosTransport.Pace("leave", 2.1, CancellationToken.None);
            if (Idas3EosPlatform.IsStopping) return;
            var done = new TaskCompletionSource<Result>(); var options = new DestroyLobbyOptions { LobbyId = id, LocalUserId = user };
            lobby.DestroyLobby(ref options, null, (ref DestroyLobbyCallbackInfo info) => { var result = info.ResultCode; Idas3EosPlatform.Defer(() => done.TrySetResult(result)); });
            await Finish(done.Task);
            var status = await done.Task;
            Idas3EosPlatform.Check(status, "Removing online activity", Result.NotFound);
        }
        static async Task Finish(Task pending)
        {
            if (await Task.WhenAny(pending, Task.Delay(35000)) != pending) throw new TimeoutException("Online activity request timed out.");
            await pending;
        }
        static void ReleaseAfter(Task pending, Action release)
        {
            if (pending == null || pending.IsCompleted) release();
            else _ = pending.ContinueWith(_ => Idas3EosPlatform.Defer(() => { if (!Idas3EosPlatform.IsStopping) release(); }));
        }
        async Task RetireLateCreation(Task<string> pending)
        {
            try {
                string id = await pending;
                while (!Idas3EosPlatform.IsStopping) {
                    try { await RemoveLobby(id); return; } catch (Exception) { }
                    await Task.Delay(30000);
                }
            } catch (Exception) { }
        }
        public void Dispose()
        {
            if (disposed) return; disposed = true; publish = false; snapshot = default;
            retired = new TaskCompletionSource<bool>(); retirement = Task.WhenAll(retirement, retired.Task);
            if (!writing) { writing = true; _ = DisposePresence(); }
        }
        async Task DisposePresence()
        {
            try {
                while (presenceId != null && !Idas3EosPlatform.IsStopping) {
                    try { await RemovePresence(); } catch (Exception) { }
                    if (presenceId != null && !Idas3EosPlatform.IsStopping) await Task.Delay(30000);
                }
            } finally { writing = false; retired?.TrySetResult(true); }
        }
    }
}
