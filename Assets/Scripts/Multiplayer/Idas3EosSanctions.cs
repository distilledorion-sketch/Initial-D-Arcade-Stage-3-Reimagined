using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Epic.OnlineServices;
using Epic.OnlineServices.Sanctions;

namespace Idas3.Multiplayer
{
    internal interface IIdas3OnlineAccessPolicy : IDisposable
    {
        bool IsBlocked(string playerId);
        bool IsClearWithin(string playerId, double age);
        Task RequireAllowed(string playerId, bool local, bool force);
        void Poll();
    }

    // Read-only enforcement of both Epic sanctions and the private player
    // directory. No administrator credential is shipped with the game.
    internal sealed class Idas3EosSanctions : IDisposable
    {
        internal const double FreshSeconds = 45, GraceSeconds = 180, RetrySeconds = 10;
        internal const double QueryTimeoutSeconds = 35;
        internal const string LocalRestriction = "Online play is restricted for this player. Contact the game administrator with your Player ID.";
        internal const string PeerRestriction = "The other driver's online access is restricted.";
        internal const string Unavailable = "Online access could not be verified. Please retry shortly.";
        internal const string ResetBattleLevel = "RESET_BATTLE_LEVEL", ResetBattleStats = "RESET_BATTLE_STATS";
        internal const string TemporaryReset = "Administrative battle resets must be permanent. Contact the game administrator with your Player ID.";
        const int CacheLimit = 32;
        sealed class Entry
        {
            internal bool Known, Blocked;
            internal string[] LevelResets = Array.Empty<string>(), FullResets = Array.Empty<string>();
            internal double CheckedAt, LastAttempt;
            internal TaskCompletionSource<bool> Pending;
        }
        readonly Dictionary<string, Entry> entries = new Dictionary<string, Entry>(StringComparer.Ordinal);
        readonly IIdas3OnlineAccessPolicy directory;
        bool disposed;

        internal Idas3EosSanctions(IIdas3OnlineAccessPolicy directory)
        { this.directory = directory ?? throw new ArgumentNullException(nameof(directory)); }

        internal bool IsBlocked(ProductUserId user) => user != null && (directory.IsBlocked(user.ToString()) ||
            entries.TryGetValue(user.ToString(), out Entry entry) && entry.Known && entry.Blocked);
        internal string[] LevelResetIds(ProductUserId user) => ResetIds(user, false);
        internal string[] FullResetIds(ProductUserId user) => ResetIds(user, true);
        string[] ResetIds(ProductUserId user, bool full)
        {
            if (disposed || user == null || !entries.TryGetValue(user.ToString(), out Entry entry) || !entry.Known)
                throw new InvalidOperationException(Unavailable);
            // Keep callers from changing the policy snapshot shared by access
            // verification and administrative record-reset enforcement.
            return (string[])(full ? entry.FullResets : entry.LevelResets).Clone();
        }
        internal bool IsClearWithin(ProductUserId user, double age)
        {
            return !disposed && user != null && entries.TryGetValue(user.ToString(), out Entry entry) &&
                entry.Known && !entry.Blocked && Idas3EosPlatform.Now - entry.CheckedAt < age && directory.IsClearWithin(user.ToString(), age);
        }
        internal async Task RequireAllowed(ProductUserId user, bool local, bool force = false)
        {
            var epic = Query(user, force);
            await Task.WhenAll(epic, directory.RequireAllowed(user?.ToString(), local, force));
            if (epic.Result) throw new Idas3EosRestrictedException(local ? LocalRestriction : PeerRestriction);
        }
        internal void Poll()
        {
            if (disposed) return;
            directory.Poll();
            // Completion may synchronously leave/dispose the transport, so use
            // a bounded snapshot rather than enumerating a mutable collection.
            foreach (Entry entry in new List<Entry>(entries.Values)) {
                if (disposed) return;
                ExpirePending(entry);
            }
        }
        static void ExpirePending(Entry entry)
        {
            if (entry.Pending == null || Idas3EosPlatform.Now - entry.LastAttempt < QueryTimeoutSeconds) return;
            var expired = entry.Pending; entry.Pending = null;
            expired.TrySetException(new InvalidOperationException(Unavailable));
        }
        Task<bool> Query(ProductUserId user, bool force)
        {
            if (disposed || Idas3EosPlatform.IsStopping) throw new OperationCanceledException();
            if (user == null || !user.IsValid() || !Idas3EosPlatform.SignedIn)
                throw new InvalidOperationException(Unavailable);
            string id = user.ToString();
            if (!entries.TryGetValue(id, out Entry entry)) {
                if (entries.Count >= CacheLimit) {
                    string oldest = null; double time = double.PositiveInfinity;
                    foreach (var item in entries)
                        if (item.Value.Pending == null && item.Value.LastAttempt < time) { oldest = item.Key; time = item.Value.LastAttempt; }
                    if (oldest == null) throw new InvalidOperationException(Unavailable);
                    entries.Remove(oldest);
                }
                entries.Add(id, entry = new Entry());
            }
            ExpirePending(entry);
            if (disposed || Idas3EosPlatform.IsStopping) throw new OperationCanceledException();
            if (entry.Pending != null) return entry.Pending.Task;
            double now = Idas3EosPlatform.Now;
            if (!force && entry.Known && now - entry.CheckedAt < FreshSeconds) return Task.FromResult(entry.Blocked);
            if (force && entry.Known && entry.LastAttempt == entry.CheckedAt && now - entry.CheckedAt < RetrySeconds)
                return Task.FromResult(entry.Blocked);
            if (entry.LastAttempt > 0 && now - entry.LastAttempt < RetrySeconds)
                throw new InvalidOperationException(Unavailable);
            var complete = new TaskCompletionSource<bool>();
            entry.Pending = complete; entry.LastAttempt = now;
            try {
                var api = Idas3EosPlatform.Handle.GetSanctionsInterface();
                var localUser = Idas3EosPlatform.User;
                var options = new QueryActivePlayerSanctionsOptions { LocalUserId = localUser, TargetUserId = user };
                api.QueryActivePlayerSanctions(ref options, null, (ref QueryActivePlayerSanctionsCallbackInfo info) => {
                    if (!Common.IsOperationComplete(info.ResultCode)) return;
                    var result = info.ResultCode;
                    string target = info.TargetUserId?.ToString(), requester = info.LocalUserId?.ToString();
                    // Never complete tasks or inspect the SDK's cache inside its
                    // Tick callback: a synchronous native scene load may own it.
                    Idas3EosPlatform.Defer(() => {
                        if (disposed || Idas3EosPlatform.IsStopping) { complete.TrySetCanceled(); return; }
                        if (!ReferenceEquals(entry.Pending, complete)) return;
                        try {
                            if (Idas3EosPlatform.Now - now >= QueryTimeoutSeconds) throw new InvalidOperationException(Unavailable);
                            if (result != Result.Success) throw new Idas3EosServiceException(result, "Verifying online access");
                            if (target != id || requester != localUser.ToString()) throw new InvalidOperationException(Unavailable);
                            var countOptions = new GetPlayerSanctionCountOptions { TargetUserId = user };
                            uint count = api.GetPlayerSanctionCount(ref countOptions);
                            if (count > 1024) throw new InvalidOperationException(Unavailable);
                            bool blocked = false;
                            var levelResets = new SortedSet<string>(StringComparer.Ordinal);
                            var fullResets = new SortedSet<string>(StringComparer.Ordinal);
                            for (uint i = 0; i < count; ++i) {
                                var copy = new CopyPlayerSanctionByIndexOptions { TargetUserId = user, SanctionIndex = i };
                                // The official C# wrapper copies into a managed
                                // value and releases its native allocation itself.
                                Result copied = api.CopyPlayerSanctionByIndex(ref copy, out PlayerSanction? sanction);
                                if (copied != Result.Success || !sanction.HasValue) throw new InvalidOperationException(Unavailable);
                                string action = sanction.Value.Action?.ToString();
                                if (string.IsNullOrEmpty(action)) throw new InvalidOperationException(Unavailable);
                                if (RestrictsOnline(action)) blocked = true;
                                if (action == ResetBattleLevel || action == ResetBattleStats) {
                                    // Active-only queries cannot retain a reset floor
                                    // after its sanction expires. Administrators must
                                    // create a new permanent sanction for each reset.
                                    if (sanction.Value.TimeExpires != 0) throw new InvalidOperationException(TemporaryReset);
                                    string reference = sanction.Value.ReferenceId?.ToString();
                                    if (!ValidResetId(reference)) throw new InvalidOperationException(Unavailable);
                                    (action == ResetBattleStats ? fullResets : levelResets).Add(reference);
                                }
                            }
                            // QueryActive is authoritative. Never expire a ban
                            // using the player's adjustable wall clock.
                            // Request-start time is conservative if loading has
                            // deferred processing of an older service response.
                            // A timestamp has only second precision; every unique
                            // reset ID matters even when several share a second.
                            // Publish the complete validated snapshot atomically.
                            entry.LevelResets = CopyIds(levelResets); entry.FullResets = CopyIds(fullResets);
                            entry.Known = true; entry.Blocked = blocked; entry.CheckedAt = now;
                            entry.Pending = null; complete.TrySetResult(blocked);
                        } catch (Exception ex) { entry.Pending = null; complete.TrySetException(ex); }
                    });
                });
            } catch (Exception ex) { entry.Pending = null; complete.TrySetException(ex); }
            return complete.Task;
        }
        internal static bool RestrictsOnline(string action) =>
            string.Equals(action, "RESTRICT_GAME_ACCESS", StringComparison.Ordinal) ||
            string.Equals(action, "ONLINE_PLAY", StringComparison.Ordinal);
        static bool ValidResetId(string reference)
        {
            if (string.IsNullOrEmpty(reference) || reference.Length > 128) return false;
            foreach (char value in reference)
                if (!(value >= 'a' && value <= 'z' || value >= 'A' && value <= 'Z' ||
                    value >= '0' && value <= '9' || value == '-' || value == '_')) return false;
            return true;
        }
        static string[] CopyIds(SortedSet<string> ids)
        {
            var copy = new string[ids.Count]; ids.CopyTo(copy); return copy;
        }
        public void Dispose()
        {
            disposed = true;
            directory.Dispose();
            foreach (var entry in entries.Values) entry.Pending?.TrySetCanceled();
            entries.Clear();
        }
    }
    internal sealed class Idas3EosRestrictedException : InvalidOperationException
    {
        internal Idas3EosRestrictedException(string message) : base(message) { }
    }
}
