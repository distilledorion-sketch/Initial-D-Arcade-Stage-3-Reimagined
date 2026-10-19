using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Idas3.Multiplayer
{
    // A separate policy cache: EOS sanctions cannot grant access while this
    // service's ban status is unknown, stale or blocked (and vice versa).
    internal sealed class Idas3OnlinePlayerDirectory : IIdas3OnlineAccessPolicy
    {
        [Serializable] internal sealed class Reply
        {
            public int version;
            public string playerId, status, targetPlayerId, targetStatus;
            public long checkedAt;
        }
        sealed class Entry
        {
            internal bool Known, Blocked;
            internal double CheckedAt, LastAttempt;
            internal long Sequence;
            internal string Name, AttemptName;
            internal TaskCompletionSource<bool> Pending;
        }
        readonly Dictionary<string, Entry> entries = new Dictionary<string, Entry>(StringComparer.Ordinal);
        readonly Func<string, string, string, Task<Reply>> request;
        readonly Func<double> clock;
        readonly Func<string> identity, username;
        readonly Idas3OnlinePlayerDirectoryHttp http;
        string owner;
        long sequence;
        bool disposed;
        internal Idas3OnlinePlayerDirectory()
        {
            http = new Idas3OnlinePlayerDirectoryHttp(); request = http.Query;
            clock = () => Idas3EosPlatform.Now;
            identity = () => Idas3EosPlatform.SignedIn ? Idas3EosPlatform.User.ToString() : "";
            username = () => Idas3EosOnlineConfig.DiagnosticRun ? "" : Idas3OnlineUsername.Saved;
        }
        internal Idas3OnlinePlayerDirectory(Func<string, string, string, Task<Reply>> request,
            Func<double> clock, Func<string> identity, Func<string> username)
        { this.request = request; this.clock = clock; this.identity = identity; this.username = username; }

        internal static bool ValidId(string value)
        {
            if (value == null || value.Length != 32) return false;
            foreach (char c in value) if (!(c >= '0' && c <= '9' || c >= 'a' && c <= 'f')) return false;
            return true;
        }
        public bool IsBlocked(string playerId) => !disposed && playerId != null &&
            entries.TryGetValue(playerId, out var entry) && entry.Known && entry.Blocked;
        public bool IsClearWithin(string playerId, double age) => !disposed && owner == identity() &&
            playerId != null && entries.TryGetValue(playerId, out var entry) && entry.Known && !entry.Blocked &&
            entry.Name == username() && clock() - entry.CheckedAt < age;

        public async Task RequireAllowed(string playerId, bool local, bool force)
        {
            await Query(playerId, force);
            if (IsBlocked(owner)) throw new Idas3EosRestrictedException(Idas3EosSanctions.LocalRestriction);
            if (IsBlocked(playerId)) throw new Idas3EosRestrictedException(local ? Idas3EosSanctions.LocalRestriction : Idas3EosSanctions.PeerRestriction);
            // The first-use name prompt can finish while initial access is in
            // flight. Register the newly chosen name before granting access.
            if (entries.TryGetValue(playerId, out var entry) && entry.Name != username()) {
                await Query(playerId, force);
                if (IsBlocked(owner)) throw new Idas3EosRestrictedException(Idas3EosSanctions.LocalRestriction);
                if (IsBlocked(playerId)) throw new Idas3EosRestrictedException(local ? Idas3EosSanctions.LocalRestriction : Idas3EosSanctions.PeerRestriction);
            }
            if (!IsClearWithin(playerId, Idas3EosSanctions.FreshSeconds)) throw Unavailable();
        }
        Task Query(string target, bool force)
        {
            string self = identity(), name = username();
            if (disposed || !ValidId(self) || !ValidId(target) || owner != null && owner != self) throw Unavailable();
            owner = self;
            if (!entries.ContainsKey(self)) entries.Add(self, new Entry());
            if (!entries.TryGetValue(target, out var entry)) {
                if (entries.Count >= 32) {
                    string oldest = null; double oldestTime = double.MaxValue;
                    foreach (var item in entries) if (item.Key != self && item.Value.Pending == null && item.Value.CheckedAt < oldestTime) {
                        oldest = item.Key; oldestTime = item.Value.CheckedAt;
                    }
                    if (oldest == null) throw Unavailable();
                    entries.Remove(oldest);
                }
                entries.Add(target, entry = new Entry());
            }
            Expire(entry);
            if (entry.Pending != null) return entry.AttemptName == name ? entry.Pending.Task : AfterOldName(entry.Pending.Task, target, force);
            double now = clock();
            if (entry.Known && entry.Name == name && now - entry.CheckedAt <
                (force ? Idas3EosSanctions.RetrySeconds : Idas3EosSanctions.FreshSeconds)) return Task.CompletedTask;
            if (entry.LastAttempt > 0 && entry.AttemptName == name && now - entry.LastAttempt < Idas3EosSanctions.RetrySeconds) throw Unavailable();
            var complete = new TaskCompletionSource<bool>();
            entry.Pending = complete; entry.LastAttempt = now; entry.AttemptName = name;
            _ = Fetch(target, self, name, entry, complete, now, ++sequence);
            return complete.Task;
        }
        async Task AfterOldName(Task previous, string target, bool force)
        {
            try { await previous; } catch (Exception) { /* The new name needs its own authenticated attempt. */ }
            await Query(target, force);
        }
        async Task Fetch(string target, string self, string name, Entry entry, TaskCompletionSource<bool> complete, double started, long stamp)
        {
            try {
                var reply = await request(self, target == self ? "" : target, name);
                if (disposed || !ReferenceEquals(entry.Pending, complete)) return;
                if (identity() != self || clock() - started >= Idas3EosSanctions.QueryTimeoutSeconds || reply == null ||
                    reply.version != 1 || reply.playerId != self || !ValidStatus(reply.status) || reply.checkedAt <= 0 ||
                    (target == self ? !string.IsNullOrEmpty(reply.targetPlayerId) || !string.IsNullOrEmpty(reply.targetStatus) :
                    reply.targetPlayerId != target || !ValidStatus(reply.targetStatus))) throw Unavailable();
                // Request order prevents a delayed older response from clearing
                // a newer known ban. Never refresh clearance on a failed read.
                Publish(entry, target == self ? reply.status : reply.targetStatus, name, started, stamp);
                if (target != self) Publish(entries[self], reply.status, name, started, stamp);
                entry.Pending = null; complete.TrySetResult(true);
            } catch (Exception) {
                if (!ReferenceEquals(entry.Pending, complete)) return;
                entry.Pending = null; complete.TrySetException(Unavailable());
            }
        }
        static bool ValidStatus(string status) => status == "allowed" || status == "blocked";
        static void Publish(Entry entry, string status, string name, double started, long stamp)
        {
            if (entry.Sequence > stamp) return;
            entry.Known = true; entry.Blocked = status == "blocked"; entry.Name = name;
            entry.CheckedAt = started; entry.Sequence = stamp;
        }
        static InvalidOperationException Unavailable() => new InvalidOperationException(Idas3EosSanctions.Unavailable);
        void Expire(Entry entry)
        {
            if (entry.Pending == null || clock() - entry.LastAttempt < Idas3EosSanctions.QueryTimeoutSeconds) return;
            var pending = entry.Pending; entry.Pending = null; pending.TrySetException(Unavailable());
        }
        public void Poll() { if (!disposed) foreach (var entry in new List<Entry>(entries.Values)) Expire(entry); }
        public void Dispose()
        {
            disposed = true;
            http?.Dispose();
            foreach (var entry in new List<Entry>(entries.Values)) entry.Pending?.TrySetCanceled();
            entries.Clear();
        }
    }
}
