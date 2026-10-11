using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using Unity.Networking.Transport;
using Unity.Networking.Transport.Relay;
using Unity.Networking.Transport.Utilities;
using Unity.Services.Multiplayer;

namespace Idas3.Multiplayer
{
    // Relay allocates an encrypted route, but a claimed player ID on that route
    // is not authentication. Admission proves possession of the exact member's
    // per-session secret fetched from authenticated MPS membership. Secrets are
    // Member-visible (never public query metadata), fresh for each join, and
    // never transmitted on the route. Both directions prove fresh challenges.
    internal sealed class Idas3UnityNetworkHandler : INetworkHandler, IDisposable
    {
        const byte Hello = 1, Proof = 2, Game = 3;
        const int MaxPendingBytes = 256 * 1024, MaxPendingMessages = 128;
        const int MaxDeferredEvents = 1024, MaxDeferredBytes = 1024 * 1024;
        const double AdmissionTimeout = 20, ReliableTimeout = 10;
        struct DeferredEvent
        {
            public bool Accepted;
            public NetworkEvent.Type Type;
            public NetworkConnection Sender;
            public NetworkPipeline Pipeline;
            public byte[] Data;
        }
        readonly Idas3UnityTransport owner;
        readonly long generation;
        readonly Queue<byte[]> reliable = new Queue<byte[]>();
        readonly Queue<DeferredEvent> deferred = new Queue<DeferredEvent>();
        NetworkDriver driver;
        NetworkConnection connection;
        NetworkPipeline reliablePipeline, unreliablePipeline;
        TaskCompletionSource<bool> connecting;
        ISession session;
        bool disposed, host, connected, helloSent, proofSent, pumping;
        string nonce, remoteNonce, claimedId, deferredFailure;
        byte[] pendingProof, nativeLoadBlockedHead;
        int queuedBytes, deferredBytes;
        uint sendSequence, receiveSequence = 1;
        double connectionStarted, blockedSince, lastReceive, retryAt, retryDelay = .025;
        double nativeLoadStarted = -1;
        public string Secret { get; }
        public bool StartRequested { get; private set; }
        public long NativeLoadBufferedEvents { get; private set; }
        public bool Authenticated { get; private set; }
        public string RemoteId { get; private set; } = "";
        public string RemoteName { get; private set; } = "";
        bool Current => !disposed && owner.Current(generation);

        public Idas3UnityNetworkHandler(Idas3UnityTransport owner, long generation, string secret)
        {
            this.owner = owner; this.generation = generation; Secret = secret;
        }
        public Task StartAsync(NetworkConfiguration configuration) => StartAsync(configuration, CancellationToken.None);
        public async Task StartAsync(NetworkConfiguration configuration, CancellationToken cancellationToken)
        {
            StartRequested = true;
            cancellationToken.ThrowIfCancellationRequested();
            if (!Current) throw new OperationCanceledException();
            if (driver.IsCreated) throw new InvalidOperationException("The network route has already started.");
            if (configuration.Type != NetworkType.Relay || configuration.Role == NetworkRole.Server)
                throw new InvalidOperationException("This build requires a two-player Relay session.");
            host = configuration.Role == NetworkRole.Host;
            // MPS 2.4 supplies the local join allocation here for BOTH roles.
            // RelayClientData is populated only with ENTITIES_NETCODE_AVAILABLE;
            // its public documentation is misleading for custom UTP handlers.
            var relay = configuration.RelayServerData;
            if (relay.Endpoint == default || relay.AllocationId == default || relay.IsSecure != 1)
                throw new InvalidOperationException("The service did not provide valid encrypted Relay credentials.");
            var settings = new NetworkSettings();
            try {
                settings.WithNetworkConfigParameters(connectTimeoutMS: 1000, maxConnectAttempts: 20,
                    disconnectTimeoutMS: 75000, heartbeatTimeoutMS: 1000, receiveQueueCapacity: 256, sendQueueCapacity: 256);
                settings.WithRelayParameters(ref relay);
                settings.WithFragmentationStageParameters(payloadCapacity: Idas3UnityTransport.MaxPayloadBytes + 5);
                settings.WithReliableStageParameters(windowSize: 128);
                driver = relay.IsWebSocket == 1 ? NetworkDriver.Create(new WebSocketNetworkInterface(), settings) : NetworkDriver.Create(settings);
            } finally { settings.Dispose(); }
            reliablePipeline = driver.CreatePipeline(typeof(FragmentationPipelineStage), typeof(ReliableSequencedPipelineStage));
            unreliablePipeline = driver.CreatePipeline(typeof(FragmentationPipelineStage));
            if (driver.Bind(NetworkEndpoint.AnyIpv4) != 0) { Dispose(); throw new InvalidOperationException("Could not bind the Relay transport."); }
            if (host) {
                if (driver.Listen() != 0) { Dispose(); throw new InvalidOperationException("Could not listen on the Relay transport."); }
                return;
            }
            connecting = new TaskCompletionSource<bool>();
            connection = driver.Connect(relay.Endpoint);
            connectionStarted = Idas3UnityTransport.Now;
            using (cancellationToken.Register(() => connecting.TrySetCanceled())) await connecting.Task;
            cancellationToken.ThrowIfCancellationRequested();
            if (!Current) throw new OperationCanceledException();
        }
        public Task StopAsync() { Dispose(); return Task.CompletedTask; }
        public void Attach(ISession target) { if (Current) session = target; }

        // Called only on the Unity main thread while native loading owns its
        // stack. UTP clears its event queue and readers on every update, so copy
        // both the existing and new events without dispatching game or SDK work.
        public bool KeepAliveOnly()
        {
            if (!Current || !driver.IsCreated || pumping) return false;
            pumping = true;
            try {
                if (nativeLoadStarted < 0) {
                    nativeLoadStarted = Idas3UnityTransport.Now;
                    nativeLoadBlockedHead = reliable.Count == 0 ? null : reliable.Peek();
                }
                CaptureEvents();
                driver.ScheduleUpdate().Complete();
                CaptureEvents();
                if (driver.GetRelayConnectionStatus() == RelayConnectionStatus.AllocationInvalid)
                    deferredFailure = "The Relay connection expired. Please reconnect.";
                return deferredFailure == null;
            } catch (Exception ex) { deferredFailure = "Online transport failed: " + ex.Message; }
            finally { pumping = false; }
            return false;
        }

        public void Poll()
        {
            if (!Current || !driver.IsCreated || pumping) return;
            try {
                ResumeAfterNativeLoad();
                CaptureEvents();
                DispatchEvents();
                if (!Current || !driver.IsCreated) return;
                driver.ScheduleUpdate().Complete();
                if (driver.GetRelayConnectionStatus() == RelayConnectionStatus.AllocationInvalid) {
                    Fail("The Relay connection expired. Please reconnect."); return;
                }
                CaptureEvents();
                DispatchEvents();
                if (!Current || !driver.IsCreated) return;
                if (connection.IsCreated && !Authenticated && Idas3UnityTransport.Now - connectionStarted > AdmissionTimeout) { Reject(); return; }
                if (Authenticated && Idas3UnityTransport.Now - lastReceive > 75) { Fail("The other driver stopped responding."); return; }
                if (connected && session != null) Handshake();
                Flush();
            } catch (Exception ex) { if (Current) Fail("Online transport failed: " + ex.Message); }
        }

        void CaptureEvents()
        {
            NetworkConnection incoming;
            // Accept first: UTP discards Data events for unaccepted connections.
            while (host && (incoming = driver.Accept()).IsCreated) {
                if (ReserveDeferred(0, false)) deferred.Enqueue(new DeferredEvent { Accepted = true, Sender = incoming });
            }
            NetworkEvent.Type type;
            while ((type = driver.PopEvent(out var sender, out var reader, out var pipeline)) != NetworkEvent.Type.Empty) {
                bool expendable = type == NetworkEvent.Type.Data && pipeline == unreliablePipeline;
                if (!ReserveDeferred(reader.Length, expendable)) continue;
                var data = new byte[reader.Length];
                for (int i = 0; i < data.Length; ++i) data[i] = reader.ReadByte();
                deferred.Enqueue(new DeferredEvent { Type = type, Sender = sender, Pipeline = pipeline, Data = data });
                deferredBytes += data.Length;
                if (pumping) ++NativeLoadBufferedEvents;
            }
        }

        bool ReserveDeferred(int bytes, bool expendable)
        {
            if (deferredFailure != null) return false;
            if (deferred.Count < MaxDeferredEvents && deferredBytes + bytes <= MaxDeferredBytes) return true;
            if (expendable) return false;
            // Preserve control/reliable FIFO order while discarding old optional
            // snapshots if they occupy the capacity required for reliable data.
            int count = deferred.Count;
            for (int i = 0; i < count; ++i) {
                var item = deferred.Dequeue();
                if ((deferred.Count >= MaxDeferredEvents - 1 || deferredBytes + bytes > MaxDeferredBytes) &&
                    !item.Accepted && item.Type == NetworkEvent.Type.Data && item.Pipeline == unreliablePipeline)
                    deferredBytes -= item.Data.Length;
                else deferred.Enqueue(item);
            }
            if (deferred.Count < MaxDeferredEvents && deferredBytes + bytes <= MaxDeferredBytes) return true;
            deferredFailure = "The incoming online message buffer is full. Please reconnect.";
            return false;
        }

        void DispatchEvents()
        {
            // Remove each record before dispatch. A native load invoked by a
            // message can reenter KeepAliveOnly and append to this same FIFO.
            while (Current && driver.IsCreated) {
                if (deferredFailure != null) { Fail(deferredFailure); return; }
                if (deferred.Count == 0) return;
                var item = deferred.Dequeue();
                deferredBytes -= item.Data?.Length ?? 0;
                if (item.Accepted) {
                    // An admitted peer is never replaced by another connection.
                    if (connection.IsCreated) driver.Disconnect(item.Sender);
                    else { connection = item.Sender; ConnectedRoute(); }
                    continue;
                }
                if (item.Sender != connection) continue;
                if (item.Type == NetworkEvent.Type.Connect) { ConnectedRoute(); connecting?.TrySetResult(true); }
                else if (item.Type == NetworkEvent.Type.Disconnect) {
                    if (host && !Authenticated) { ResetConnection(); continue; }
                    Fail("The other driver disconnected."); return;
                } else if (item.Type == NetworkEvent.Type.Data) {
                    bool ordered = item.Pipeline == reliablePipeline;
                    // Unreliable game traffic can overtake our reliable
                    // admission proof. Drop it until admission completes.
                    if (!Authenticated && item.Pipeline == unreliablePipeline) continue;
                    byte[] data = item.Data;
                    if ((!ordered && item.Pipeline != unreliablePipeline) || data.Length < (ordered ? 5 : 1) ||
                        data.Length > Idas3UnityTransport.MaxPayloadBytes + (ordered ? 5 : 1) || (!Authenticated && data.Length > 1028)) { Reject(); continue; }
                    if (ordered) {
                        uint sequence = data[0] | ((uint)data[1] << 8) | ((uint)data[2] << 16) | ((uint)data[3] << 24);
                        // A pipeline send can fail after storing fragments;
                        // whole-message retries use the same envelope ID.
                        if (sequence < receiveSequence) continue;
                        if (sequence != receiveSequence) { Reject(); continue; }
                        ++receiveSequence;
                    }
                    var bytes = new byte[data.Length - (ordered ? 4 : 0)];
                    Buffer.BlockCopy(data, ordered ? 4 : 0, bytes, 0, bytes.Length);
                    if (!ordered && bytes[0] != Game) { Reject(); continue; }
                    Receive(bytes);
                }
            }
        }
        void ConnectedRoute()
        {
            connected = true; connectionStarted = lastReceive = Idas3UnityTransport.Now;
            nonce = Idas3UnityTransport.RandomToken();
        }
        void Receive(byte[] bytes)
        {
            if (bytes[0] == Game) {
                if (!Authenticated) { Reject(); return; }
                lastReceive = Idas3UnityTransport.Now;
                var data = new byte[bytes.Length - 1]; Buffer.BlockCopy(bytes, 1, data, 0, data.Length);
                owner.Deliver(this, data); return;
            }
            if (Authenticated || bytes.Length > 1024) { Reject(); return; }
            try {
                using (var stream = new MemoryStream(bytes, false))
                using (var reader = new BinaryReader(stream, Encoding.UTF8)) {
                    byte kind = reader.ReadByte();
                    if (kind == Hello) {
                        string sessionId = reader.ReadString(), playerId = reader.ReadString(), compatibility = reader.ReadString(), challenge = reader.ReadString();
                        if (stream.Position != stream.Length || sessionId.Length > 80 || playerId.Length > 80 || compatibility != owner.CompatibilityHash || !ValidSecret(challenge)) { Reject(); return; }
                        if (session == null) {
                            // Service completion can lag the UTP Connect event.
                            // Save only a bounded hello; no game data is delivered.
                            pendingSessionId = sessionId;
                        } else if (sessionId != session.Id) { Reject(); return; }
                        if (claimedId != null && (claimedId != playerId || remoteNonce != challenge)) { Reject(); return; }
                        claimedId = playerId; remoteNonce = challenge;
                    } else if (kind == Proof) {
                        if (bytes.Length != 33 || pendingProof != null) { Reject(); return; }
                        pendingProof = reader.ReadBytes(32);
                    } else { Reject(); return; }
                }
                Handshake();
            } catch (Exception) { Reject(); }
        }
        string pendingSessionId;
        void Handshake()
        {
            if (!Current || !connected || session == null || Authenticated) return;
            if (pendingSessionId != null && pendingSessionId != session.Id) { Reject(); return; }
            if (!helloSent) {
                Enqueue(Packet(writer => { writer.Write(Hello); writer.Write(session.Id); writer.Write(owner.LocalId); writer.Write(owner.CompatibilityHash); writer.Write(nonce); }));
                helloSent = true;
            }
            if (claimedId == null || remoteNonce == null) return;
            if (claimedId == owner.LocalId || (!host && claimedId != session.Host) || session.PlayerCount > 2) { Reject(); return; }
            var peer = Idas3UnityTransport.FindPlayer(session, claimedId);
            if (peer == null) return; // Wait for authenticated lobby membership events/refresh.
            string secret = Idas3UnityTransport.Get(peer.Properties, Idas3UnityTransport.SecretKey);
            if (!ValidSecret(secret) || Idas3UnityTransport.Get(peer.Properties, Idas3UnityTransport.CompatibilityKey) != owner.CompatibilityHash) { Reject(); return; }
            if (!proofSent) {
                byte[] proof = Prove(Secret, owner.LocalId, claimedId, nonce, remoteNonce);
                Enqueue(Packet(writer => { writer.Write(Proof); writer.Write(proof); })); proofSent = true;
            }
            if (pendingProof == null) return;
            byte[] expected = Prove(secret, claimedId, owner.LocalId, remoteNonce, nonce);
            int difference = 0; for (int i = 0; i < 32; ++i) difference |= expected[i] ^ pendingProof[i];
            if (difference != 0) { Reject(); return; }
            RemoteId = claimedId; RemoteName = Idas3UnityTransport.Clean(Idas3UnityTransport.Get(peer.Properties, Idas3UnityTransport.NameKey), "Driver");
            Authenticated = true; lastReceive = Idas3UnityTransport.Now;
            owner.Admitted(this);
        }
        byte[] Prove(string key, string sender, string receiver, string senderNonce, string receiverNonce)
        {
            using (var hmac = new HMACSHA256(Convert.FromBase64String(key)))
                return hmac.ComputeHash(Packet(writer => { writer.Write(Idas3UnityTransport.Protocol); writer.Write(session.Id); writer.Write(owner.CompatibilityHash); writer.Write(sender); writer.Write(receiver); writer.Write(senderNonce); writer.Write(receiverNonce); }));
        }
        static bool ValidSecret(string value)
        {
            if (value == null || value.Length != 44) return false;
            try { return Convert.FromBase64String(value).Length == 32; } catch (FormatException) { return false; }
        }
        static byte[] Packet(Action<BinaryWriter> write)
        {
            using (var stream = new MemoryStream()) {
                using (var writer = new BinaryWriter(stream, Encoding.UTF8, true)) write(writer);
                return stream.ToArray();
            }
        }
        public void SendGame(byte[] data, bool ordered)
        {
            if (!Current || !Authenticated) return;
            var packet = new byte[data.Length + 1]; packet[0] = Game; Buffer.BlockCopy(data, 0, packet, 1, data.Length);
            if (ordered) { Enqueue(packet); Flush(); } else TrySend(packet, unreliablePipeline);
        }
        void Enqueue(byte[] packet)
        {
            if (!Current) return;
            if (reliable.Count >= MaxPendingMessages || queuedBytes + packet.Length + 4 > MaxPendingBytes || sendSequence == uint.MaxValue) { Fail("The reliable online message buffer is full. Please reconnect."); return; }
            if (reliable.Count == 0) blockedSince = Idas3UnityTransport.Now;
            var envelope = new byte[packet.Length + 4];
            uint sequence = ++sendSequence;
            for (int i = 0; i < 4; ++i) envelope[i] = (byte)(sequence >> (8 * i));
            Buffer.BlockCopy(packet, 0, envelope, 4, packet.Length);
            reliable.Enqueue(envelope); queuedBytes += envelope.Length;
        }
        void Flush()
        {
            if (!Current || !driver.IsCreated || !connected) return;
            ResumeAfterNativeLoad();
            if (reliable.Count != 0 && Idas3UnityTransport.Now - blockedSince > ReliableTimeout) {
                Fail("Reliable online messages timed out. Please reconnect."); return;
            }
            if (Idas3UnityTransport.Now < retryAt) return;
            while (reliable.Count != 0) {
                byte[] packet = reliable.Peek();
                if (!TrySend(packet, reliablePipeline)) {
                    // Partial fragmented sends occupy the reliable window. Give
                    // their ACKs time to drain before retrying the whole message;
                    // refilling every freed slot each frame can starve its tail.
                    retryDelay = Math.Min(4, retryDelay * 2); retryAt = Idas3UnityTransport.Now + retryDelay;
                    break;
                }
                reliable.Dequeue(); queuedBytes -= packet.Length; blockedSince = Idas3UnityTransport.Now;
                retryAt = 0; retryDelay = .025;
            }
        }
        void ResumeAfterNativeLoad()
        {
            if (nativeLoadStarted < 0) return;
            double duration = Math.Max(0, Idas3UnityTransport.Now - nativeLoadStarted);
            // The application deliberately paused sending while the loader kept
            // UTP alive. Preserve the old head's stalled age, without extending
            // the timeout of a new message enqueued after loading finished.
            if (reliable.Count != 0 && ReferenceEquals(reliable.Peek(), nativeLoadBlockedHead)) blockedSince += duration;
            if (!Authenticated && connection.IsCreated) connectionStarted += duration;
            nativeLoadStarted = -1; nativeLoadBlockedHead = null;
        }
        bool TrySend(byte[] bytes, NetworkPipeline pipeline)
        {
            if (!Current || !driver.IsCreated || !connection.IsCreated) return false;
            int result = driver.BeginSend(pipeline, connection, out var writer, bytes.Length);
            if (result != 0) return false;
            for (int i = 0; i < bytes.Length; ++i) writer.WriteByte(bytes[i]);
            return driver.EndSend(writer) >= 0;
        }
        void Reject()
        {
            if (!Current) return;
            if (!host || Authenticated) { Fail("The remote driver could not be authenticated for this room."); return; }
            if (driver.IsCreated && connection.IsCreated) driver.Disconnect(connection);
            ResetConnection();
        }
        void ResetConnection()
        {
            connection = default; connected = false; Authenticated = false; helloSent = proofSent = false;
            claimedId = remoteNonce = nonce = pendingSessionId = null; pendingProof = null;
            RemoteId = RemoteName = ""; reliable.Clear(); queuedBytes = 0; sendSequence = 0; receiveSequence = 1; retryAt = 0; retryDelay = .025;
        }
        void Fail(string detail)
        {
            connecting?.TrySetException(new InvalidOperationException(detail));
            owner.RouteFailed(this, detail);
        }
        public void Dispose()
        {
            if (disposed) return;
            disposed = true; connecting?.TrySetCanceled();
            if (driver.IsCreated) driver.Dispose();
            session = null; ResetConnection();
            deferred.Clear(); deferredBytes = 0; deferredFailure = null;
        }
    }
}
