using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;
using Unity.Services.Core;
using Unity.Services.Lobbies;
using Unity.Services.Multiplayer;
using Unity.Services.Relay;

namespace Idas3.Multiplayer
{
    // Gates are shared by rooms and presence: UGS limits apply to the signed-in
    // player, not individual SDK session handles. Waiting consumes no request,
    // and cancellation releases the gate before any replacement operation runs.
    internal static class Idas3UnityServiceRequests
    {
        internal enum Kind { Create, Join, Query, Get, Joined, Update, Delete, Leave }
        internal const string AttemptKey = "idas3.attempt";
        sealed class Gate
        {
            internal readonly SemaphoreSlim Mutex = new SemaphoreSlim(1, 1);
            internal double Next;
        }
        static readonly Stopwatch Clock = Stopwatch.StartNew();
        static readonly Gate[] Gates = { new Gate(), new Gate(), new Gate(), new Gate(), new Gate(), new Gate(), new Gate(), new Gate() };
        static readonly double[] Intervals = { 3.1, 1.1, 1.1, 1.1, 1.1, 1.1, .55, .22 };

        internal static async Task WaitAsync(Kind kind, CancellationToken cancel)
        {
            var gate = Gates[(int)kind];
            await gate.Mutex.WaitAsync(cancel);
            try {
                double wait;
                while ((wait = gate.Next - Clock.Elapsed.TotalSeconds) > 0)
                    await Task.Delay(Math.Max(1, (int)Math.Ceiling(wait * 1000)), cancel);
                cancel.ThrowIfCancellationRequested();
                gate.Next = Clock.Elapsed.TotalSeconds + Intervals[(int)kind];
            } finally { gate.Mutex.Release(); }
        }
        internal static Task BackoffAsync(int retry, CancellationToken cancel) => Task.Delay((int)(2000 * Math.Pow(2, Math.Min(retry, 2))), cancel);
        internal static bool RateLimited(Exception error)
        {
            if (error == null) return false;
            if (error is SessionException session && session.Error == SessionError.RateLimitExceeded ||
                error is LobbyServiceException lobby && lobby.Reason == LobbyExceptionReason.RateLimited ||
                error is RelayServiceException relay && relay.Reason == RelayExceptionReason.RateLimited ||
                error is RequestFailedException request && request.ErrorCode == CommonErrorCodes.TooManyRequests) return true;
            if (error is AggregateException aggregate) foreach (var inner in aggregate.InnerExceptions) if (RateLimited(inner)) return true;
            return RateLimited(error.InnerException);
        }
        // Reads and idempotent cleanup can safely replay an explicit 429. Create
        // and join calls use their own attempt cleanup before deciding to retry.
        internal static async Task<T> RunAsync<T>(Kind kind, Func<Task<T>> request, CancellationToken cancel)
        {
            for (int retry = 0; ; ++retry) {
                await WaitAsync(kind, cancel);
                try { return await request(); }
                catch (Exception error) when (retry < 3 && RateLimited(error)) { await BackoffAsync(retry, cancel); }
            }
        }
        internal static async Task RunAsync(Kind kind, Func<Task> request, CancellationToken cancel)
        {
            await RunAsync(kind, async () => { await request(); return true; }, cancel);
        }

        // MPS can fail after adding membership but before returning a session
        // handle. Confirm and retire only the exact attempt's authenticated
        // player property before retrying. Never delete an unrelated session.
        internal static async Task<bool> RetireAttemptAsync(string attempt, string playerId)
        {
            using (var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(20))) {
                try {
                    var ids = await RunAsync(Kind.Joined, () => LobbyService.Instance.GetJoinedLobbiesAsync(), timeout.Token);
                    foreach (string id in ids) {
                        Unity.Services.Lobbies.Models.Lobby lobby;
                        try { lobby = await RunAsync(Kind.Get, () => LobbyService.Instance.GetLobbyAsync(id), timeout.Token); }
                        catch (LobbyServiceException error) when (error.Reason == LobbyExceptionReason.LobbyNotFound || error.Reason == LobbyExceptionReason.Forbidden) { continue; }
                        bool belongs = false;
                        if (lobby.Players != null) foreach (var player in lobby.Players)
                            if (player.Id == playerId && player.Data != null && player.Data.TryGetValue(AttemptKey, out var property) && property.Value == attempt) { belongs = true; break; }
                        if (!belongs) continue;
                        try {
                            if (lobby.HostId == playerId) await RunAsync(Kind.Delete, () => LobbyService.Instance.DeleteLobbyAsync(id), timeout.Token);
                            else await RunAsync(Kind.Leave, () => LobbyService.Instance.RemovePlayerAsync(id, playerId), timeout.Token);
                        } catch (LobbyServiceException error) when (error.Reason == LobbyExceptionReason.LobbyNotFound || error.Reason == LobbyExceptionReason.PlayerNotFound) { }
                    }
                    return true;
                } catch (Exception) { return false; }
            }
        }
    }
}
