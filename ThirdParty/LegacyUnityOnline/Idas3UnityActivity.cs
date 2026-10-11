using System;
using System.Collections.Generic;
using System.Globalization;
using System.Threading;
using System.Threading.Tasks;
using Unity.Services.Multiplayer;

namespace Idas3.Multiplayer
{
    // One small non-networked session per signed-in client. This records menu,
    // queue, and race presence without counting race rooms twice or including
    // unrelated Unity projects/environments. Failed/stale queries show unknown.
    internal sealed class Idas3UnityActivity : IDisposable
    {
        const string OwnerKey = "idas3.owner", StateKey = "idas3.state", SeenKey = "idas3.seen";
        readonly string playerId;
        IHostSession presence;
        CancellationTokenSource cancellation = new CancellationTokenSource();
        bool disposed, publishing, querying;
        long generation, queryGeneration;
        double nextPublish, nextQuery, publishDeadline, queryDeadline;
        Idas3OnlineActivity snapshot;
        public Idas3OnlineActivity Snapshot => !disposed && Idas3UnityTransport.Now - snapshot.ObservedAt < 45 ? snapshot : default;
        public Idas3UnityActivity(string playerId) { this.playerId = playerId; }

        public void Poll(bool requested, bool publish, string state, string compatibility)
        {
            if (disposed) return;
            double now = Idas3UnityTransport.Now;
            if ((!publish && (presence != null || publishing)) || (publishing && now > publishDeadline)) StopPublishing();
            if (querying && now > queryDeadline) { ++queryGeneration; querying = false; snapshot = default; nextQuery = now + 15; }
            if (publish && !publishing && now >= nextPublish) {
                publishing = true; publishDeadline = now + 25; nextPublish = now + 30;
                _ = PublishAsync(generation, state == "racing" || state == "queuing" ? state : "online", compatibility, cancellation.Token);
            }
            if (requested && !querying && now >= nextQuery) {
                querying = true; queryDeadline = now + 25; nextQuery = now + 20;
                _ = QueryAsync(++queryGeneration, compatibility, cancellation.Token);
            }
        }
        async Task PublishAsync(long token, string state, string compatibility, CancellationToken cancel)
        {
            IHostSession created = null;
            string attempt = Guid.NewGuid().ToString("N");
            bool createRequested = false;
            try {
                var properties = new Dictionary<string, SessionProperty> {
                    { Idas3UnityTransport.CompatibilityKey, new SessionProperty(compatibility, VisibilityPropertyOptions.Public, PropertyIndex.String1) },
                    { Idas3UnityTransport.KindKey, new SessionProperty("activity", VisibilityPropertyOptions.Public, PropertyIndex.String2) },
                    { OwnerKey, new SessionProperty(playerId) },
                    { StateKey, new SessionProperty(state) },
                    { SeenKey, new SessionProperty(DateTimeOffset.UtcNow.ToUnixTimeSeconds().ToString(CultureInfo.InvariantCulture)) }
                };
                if (presence == null) {
                    await Idas3UnityServiceRequests.WaitAsync(Idas3UnityServiceRequests.Kind.Create, cancel);
                    if (disposed || token != generation) return;
                    createRequested = true;
                    created = await MultiplayerService.Instance.CreateSessionAsync(new SessionOptions {
                        Type = Idas3UnityTransport.Protocol + "-activity-" + Guid.NewGuid().ToString("N"), Name = "Online driver", MaxPlayers = 1,
                        IsLocked = true, SessionProperties = properties,
                        PlayerProperties = new Dictionary<string, PlayerProperty> { { Idas3UnityServiceRequests.AttemptKey, new PlayerProperty(attempt, VisibilityPropertyOptions.Member) } }
                    }, cancel);
                    if (disposed || token != generation) { await Idas3UnityTransport.CleanupSession(created); return; }
                    presence = created;
                } else {
                    var target = presence;
                    target.SetProperties(properties);
                    await Idas3UnityServiceRequests.RunAsync(Idas3UnityServiceRequests.Kind.Update, () => target.SavePropertiesAsync(), cancel);
                }
            } catch (Exception error) {
                if (createRequested && created == null) await Idas3UnityServiceRequests.RetireAttemptAsync(attempt, playerId);
                if (Idas3UnityServiceRequests.RateLimited(error)) return;
                if (!disposed && token == generation && presence != null) {
                    var stale = presence; presence = null; _ = Idas3UnityTransport.CleanupSession(stale);
                }
            } finally { if (!disposed && token == generation) publishing = false; }
        }
        async Task QueryAsync(long token, string compatibility, CancellationToken cancel)
        {
            try {
                var result = await Idas3UnityServiceRequests.RunAsync(Idas3UnityServiceRequests.Kind.Query, () => MultiplayerService.Instance.QuerySessionsAsync(new QuerySessionsOptions {
                    Count = 100,
                    FilterOptions = new List<FilterOption> {
                        new FilterOption(FilterField.StringIndex1, compatibility, FilterOperation.Equal),
                        new FilterOption(FilterField.StringIndex2, "activity", FilterOperation.Equal)
                    }
                }, cancel), cancel);
                if (disposed || token != queryGeneration) return;
                var next = new Idas3OnlineActivity { Available = true, Limited = result.Sessions.Count >= 100 || !string.IsNullOrEmpty(result.ContinuationToken), ObservedAt = Idas3UnityTransport.Now };
                var owners = new HashSet<string>(StringComparer.Ordinal);
                long now = DateTimeOffset.UtcNow.ToUnixTimeSeconds();
                foreach (var item in result.Sessions) {
                    string id = Idas3UnityTransport.Get(item.Properties, OwnerKey);
                    if (id != item.HostId || item.MaxPlayers != 1 || item.AvailableSlots != 0 ||
                        !long.TryParse(Idas3UnityTransport.Get(item.Properties, SeenKey), NumberStyles.Integer, CultureInfo.InvariantCulture, out long seen) ||
                        seen < now - 90 || seen > now + 30 || !owners.Add(id)) continue;
                    ++next.Online;
                    string state = Idas3UnityTransport.Get(item.Properties, StateKey);
                    if (state == "racing") ++next.Racing;
                    else if (state == "queuing") ++next.Queuing;
                }
                snapshot = next;
            } catch (Exception) { if (!disposed && token == queryGeneration) snapshot = default; }
            finally { if (!disposed && token == queryGeneration) querying = false; }
        }
        void StopPublishing()
        {
            ++generation; ++queryGeneration; cancellation.Cancel(); cancellation.Dispose(); cancellation = new CancellationTokenSource();
            publishing = querying = false; nextPublish = Idas3UnityTransport.Now + 30;
            var old = presence; presence = null; if (old != null) _ = Idas3UnityTransport.CleanupSession(old);
        }
        public void Dispose()
        {
            if (disposed) return;
            StopPublishing(); disposed = true; cancellation.Cancel(); cancellation.Dispose(); snapshot = default;
        }
    }
}
