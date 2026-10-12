using System;
using System.Text;
using Epic.OnlineServices;
using Epic.OnlineServices.P2P;
using Idas3.Multiplayer;

static class EosStatsP2PDriver {
    static int checks;
    static void Check(bool value, string name) {
        if (!value) throw new Exception(name);
        ++checks; Console.WriteLine("PASS\t" + name);
    }
    static byte[] Bytes(string value) => Encoding.UTF8.GetBytes(value);
    static void Incoming(P2PInterface sdk, string hello) {
        sdk.Inject(Bytes(hello), 0); sdk.Inject(new byte[] { 1 }, 0);
    }
    static void Game(P2PInterface sdk, string text, uint id = 1) {
        byte[] data = Bytes(text);
        for (int index = 0; index < Idas3EosPackets.Count(data.Length); ++index)
            sdk.Inject(Idas3EosPackets.Fragment(data, id, index), 1);
    }
    static void Scenario(Action<Idas3EosTransport, Idas3EosP2P, P2PInterface> body, bool host = true) {
        Idas3EosPlatform.Handle = new P2PTestPlatform(); Idas3EosPlatform.Now = 100;
        var owner = new Idas3EosTransport();
        using (var route = new Idas3EosP2P(owner, new ProductUserId("peer"), "PRIVATE-PLAYER-NAME", "PRIVATE-ROOM-CODE", "compatible", host))
            body(owner, route, Idas3EosPlatform.Handle.P2P);
    }
    static int Main() {
        try {
            Scenario((owner, route, sdk) => {
                sdk.NotifyClosed(ConnectionClosedReason.ConnectionIgnored); route.Poll();
                Check(owner.Failure == null && sdk.Accepted == 1, "early missing peer listener schedules retry without closing lobby");
                Idas3EosPlatform.Now += 1.1; route.Poll();
                Check(sdk.Accepted == 2 && owner.Failure == null, "pre-admission retry explicitly accepts the verified peer again");
                Incoming(sdk, "ID3EOS2:compatible:C:peer-reset-1"); route.Poll();
                Check(route.Connected && owner.Admissions == 1, "delayed peer listener can complete the original authenticated handshake");
            });
            Scenario((owner, route, sdk) => {
                for (int step = 0; step < 10; ++step) {
                    sdk.NotifyClosed(ConnectionClosedReason.ConnectionFailed);
                    Idas3EosPlatform.Now += 4; route.Poll();
                    Check(owner.Failure == null, "transient pre-admission failure can retry within original deadline");
                }
                int accepted=sdk.Accepted;Idas3EosPlatform.Now=141;route.Poll();
                Check(owner.Failure != null && owner.Failure.Contains("timed out") && sdk.Accepted == accepted && sdk.Closed == 1, "retries never extend the original forty-second handshake deadline");
            });
            foreach (ConnectionClosedReason reason in new[] { ConnectionClosedReason.ClosedByLocalUser, ConnectionClosedReason.ClosedByPeer, ConnectionClosedReason.TimedOut, ConnectionClosedReason.TooManyConnections, ConnectionClosedReason.InvalidMessage, ConnectionClosedReason.InvalidData, ConnectionClosedReason.ConnectionClosed, ConnectionClosedReason.NegotiationFailed, ConnectionClosedReason.UnexpectedError })
                Scenario((owner, route, sdk) => {
                    sdk.NotifyClosed(reason); route.Poll();
                    Check(owner.Failure != null && sdk.Accepted == 1 && sdk.Closed == 1, "non-retryable close remains terminal: " + reason);
                });
            foreach (ConnectionClosedReason reason in new[] { ConnectionClosedReason.ConnectionIgnored, ConnectionClosedReason.ConnectionFailed })
                Scenario((owner, route, sdk) => {
                    Incoming(sdk,"ID3EOS2:compatible:C:peer-reset-1");route.Poll();
                    sdk.NotifyClosed(reason);route.Poll();
                    Check(owner.Failure != null && owner.Admissions == 1 && sdk.Accepted == 1, "admitted route never retries a close: " + reason);
                });
            Scenario((owner, route, sdk) => {
                sdk.NotifyClosed(ConnectionClosedReason.ConnectionIgnored);route.Dispose();
                Idas3EosPlatform.Now+=5;route.Poll();sdk.NotifyClosed(ConnectionClosedReason.ConnectionFailed);route.Poll();
                Check(sdk.Accepted==1&&sdk.Closed==1&&owner.Admissions==0,"cancellation prevents scheduled retries and ignores late close callbacks");
            });
            Scenario((owner, route, sdk) => {
                sdk.NotifyClosed(ConnectionClosedReason.InvalidData,"intruder");sdk.NotifyClosed(ConnectionClosedReason.InvalidData,"peer","old-room");route.Poll();
                Check(owner.Failure==null&&sdk.Accepted==1,"another identity or socket cannot fail or retry this route");
                sdk.NotifyClosed(ConnectionClosedReason.ConnectionFailed);sdk.AcceptResult=Result.InvalidAuth;Idas3EosPlatform.Now+=2;route.Poll();
                Check(owner.Failure!=null&&owner.Failure.Contains("InvalidAuth")&&sdk.Accepted==2,"retry cannot bypass failed authentication acceptance");
            });
            Scenario((owner, route, sdk) => {
                sdk.SendResult=Result.InvalidAuth;route.Poll();
                Check(owner.Failure!=null&&owner.Failure.Contains("InvalidAuth")&&sdk.Accepted==1,"invalid-auth control send fails immediately rather than retrying");
            });
            Scenario((owner, route, sdk) => {
                owner.AdmissionAllowed=false;Game(sdk,"held during access refresh");Incoming(sdk,"ID3EOS2:compatible:C:peer-reset-1");route.Poll();
                int sent=sdk.Sent.Count;route.SendGame(Bytes("not authorized"),true);
                Check(route.Connected&&owner.Admissions==0&&owner.Received.Count==0&&sdk.Sent.Count==sent,"wire handshake alone cannot send or deliver game payloads before owner admission");
                owner.OnAdmit=candidate=>candidate.SendGame(Bytes("session hello"),true);owner.AdmissionAllowed=true;route.Poll();
                Check(owner.Admissions==1&&owner.Received.Count==1&&sdk.Sent.Exists(packet=>packet.Channel==1),"confirmed admission enables synchronous session hello and buffered delivery");
            });
            Scenario((owner, route, sdk) => {
                Incoming(sdk,"ID3EOS2:compatible:C:peer-reset-1");route.KeepAliveOnly();
                sdk.NotifyClosed(ConnectionClosedReason.ConnectionFailed);route.Poll();
                Check(owner.Failure!=null&&owner.Admissions==0&&sdk.Accepted==1,"completed wire handshake is not restarted before deferred application admission");
            });
            Scenario((owner, route, sdk) => {
                route.Poll();
                Check(sdk.Sent.Count == 1 && Encoding.UTF8.GetString(sdk.Sent[0].Data) == "ID3EOS2:compatible:H:local-reset-1", "local hello includes local reset stamp");
                Check(route.LocalResetStamp == "local-reset-1" && route.RemoteResetStamp == "peer-reset-1", "route captures both independently supplied reset stamps");
                Game(sdk, "held until policy acknowledgement");
                Incoming(sdk, "ID3EOS2:compatible:C:peer-reset-old"); route.Poll();
                Check(!route.Connected && owner.Admissions == 0 && owner.Received.Count == 0, "mismatched peer reset stamp with valid ACK cannot admit or deliver buffered game data");
                Idas3EosPlatform.Now = 141; route.Poll();
                Check(owner.Failure != null && owner.Received.Count == 0 && sdk.Closed == 1, "mismatched stamp times out and retires buffered messages");
            });
            Scenario((owner, route, sdk) => {
                Game(sdk, "first");
                Incoming(sdk, "ID3EOS2:compatible:C:peer-reset-1"); route.Poll();
                Check(route.Connected && owner.Admissions == 1 && owner.Received.Count == 1 && Encoding.UTF8.GetString(owner.Received[0]) == "first", "matching peer stamp admits and releases packet that arrived before control handshake");
                Game(sdk, "second", 2); route.Poll(); route.Poll();
                Check(owner.Admissions == 1 && owner.Received.Count == 2, "matching handshake announces once and continues delivery");
            });
            Scenario((owner, route, sdk) => {
                Game(sdk, "keepalive buffered"); Incoming(sdk, "ID3EOS2:compatible:C:peer-reset-1");
                route.KeepAliveOnly();
                Check(route.Connected && route.BufferedEvents == 1 && owner.Admissions == 0 && owner.Received.Count == 0, "native-load keepalive completes wire handshake but defers admission and game delivery");
                owner.OnAdmit = candidate => candidate.Dispose(); route.Poll();
                Check(owner.Admissions == 1 && owner.Received.Count == 0 && sdk.Closed == 1 && sdk.Removed == 3, "owner rejecting admission disposes route before buffered delivery");
            });
            Scenario((owner, route, sdk) => {
                Game(sdk, "legacy"); Incoming(sdk, "ID3EOS2:compatible:C"); route.Poll();
                Check(!route.Connected && owner.Admissions == 0 && owner.Received.Count == 0, "legacy unstamped hello cannot authorize a reset-aware route");
            });
            Scenario((owner, route, sdk) => {
                Incoming(sdk, "ID3EOS2:compatible:H:peer-reset-1"); route.Poll();
                Check(!route.Connected && owner.Admissions == 0, "correct stamp with wrong peer role cannot authorize route");
            });
            Scenario((owner, route, sdk) => {
                sdk.Inject(Bytes("ID3EOS2:compatible:C:peer-reset-1"), 0, "intruder"); sdk.Inject(new byte[] { 1 }, 0);
                sdk.Inject(Bytes("ID3EOS2:compatible:C:peer-reset-1"), 0, "peer", "different-room"); route.Poll();
                Check(!route.Connected && owner.Admissions == 0, "matching reset stamp from wrong identity or room cannot authorize route");
            });
            Scenario((owner, route, sdk) => {
                sdk.Inject(Bytes("ID3EOS2:compatible:C:peer-reset-1"), 0); Game(sdk, "ack pending"); route.Poll();
                Check(!route.Connected && owner.Admissions == 0 && owner.Received.Count == 0, "matching reset stamp alone waits for acknowledgement");
                sdk.Inject(new byte[] { 1 }, 0); route.Poll();
                Check(route.Connected && owner.Admissions == 1 && owner.Received.Count == 1, "late acknowledgement releases correctly stamped buffered data");
            });
            Scenario((owner, route, sdk) => {
                Incoming(sdk, "ID3EOS2:compatible:H:peer-reset-1"); route.Poll();
                Check(route.Connected && owner.Admissions == 1, "guest route accepts matching host role and reset stamp");
            }, false);
            Scenario((owner, route, sdk) => {
                int first = UnityEngine.Debug.Messages.Count;
                sdk.SendResult = Result.NoConnection; route.Poll();
                Idas3EosPlatform.Now = 141; route.Poll(); route.Poll();
                var logs = UnityEngine.Debug.Messages.GetRange(first, UnityEngine.Debug.Messages.Count - first);
                Check(logs.FindAll(message => message.Contains("handshake-timeout")).Count == 1 && logs.Exists(message => message.Contains("routing=unknown established=False hello=False ack=False helloQueued=0 controlRx=0 rejected=0 foreign=0 lastSend=NoConnection")), "no-traffic timeout reports absent routing and handshake progress once");
                Check(owner.Admissions == 0 && owner.Failure.Contains("timed out"), "timeout diagnostics retain original no-traffic rejection");
            });
            string[] invalid = { "ID3EOS1:compatible:C:PRIVATE-PAYLOAD", "ID3EOS2:PRIVATE-BUILD:C:PRIVATE-PAYLOAD", "ID3EOS2:compatible:H:PRIVATE-PAYLOAD", "ID3EOS2:compatible:C:PRIVATE-PAYLOAD" };
            string[] categories = { "protocol", "build", "role", "reset" };
            for (int index = 0; index < invalid.Length; ++index) {
                int candidate = index;
                Scenario((owner, route, sdk) => {
                    int first = UnityEngine.Debug.Messages.Count;
                    sdk.NotifyEstablished(NetworkConnectionType.RelayedConnection);
                    sdk.Inject(Bytes(invalid[candidate]), 0); sdk.Inject(Bytes(invalid[candidate]), 0);
                    sdk.Inject(new byte[] { 1 }, 0); sdk.Inject(new byte[] { 3 }, 0);
                    sdk.Inject(Bytes("PRIVATE-FOREIGN-PAYLOAD"), 0, "PRIVATE-IDENTITY");
                    route.Poll(); Idas3EosPlatform.Now = 141; route.Poll();
                    var logs = UnityEngine.Debug.Messages.GetRange(first, UnityEngine.Debug.Messages.Count - first);
                    Check(logs.FindAll(message => message.Contains("control-rejected category=" + categories[candidate])).Count == 1 && logs.FindAll(message => message.Contains("control-rejected")).Count == 1, "repeated invalid hello logs only its fixed category once: " + categories[candidate]);
                    Check(logs.Exists(message => message.Contains("routing=relayed established=True hello=False ack=True helloQueued=1 controlRx=4 rejected=2 foreign=1 lastSend=Success")) && owner.Admissions == 0, "invalid hello timeout separates route establishment ACK and rejection: " + categories[candidate]);
                });
            }
            Scenario((owner, route, sdk) => {
                int first = UnityEngine.Debug.Messages.Count;
                sdk.Inject(new byte[0], 0); sdk.Inject(new byte[Idas3EosPackets.PacketLimit], 0);
                sdk.Inject(Bytes("ID3EOS2:"), 0); sdk.Inject(Bytes("ID3EOS2:compatible:"), 0);
                route.Poll();
                var logs = UnityEngine.Debug.Messages.GetRange(first, UnityEngine.Debug.Messages.Count - first);
                Check(owner.Failure == null && !route.Connected && logs.FindAll(message => message.Contains("control-rejected")).Count == 2, "empty truncated and maximum-size controls stay bounded and cannot admit");
            });
            Check(UnityEngine.Debug.Messages.Exists(message=>message.Contains("closed reason=ConnectionIgnored"))&&UnityEngine.Debug.Messages.Exists(message=>message.Contains("control-send result=InvalidAuth")),"route diagnostics expose fixed SDK stages and results");
            Check(!UnityEngine.Debug.Messages.Exists(message=>message.Contains("PRIVATE-")||message.Contains("peer-reset-1")||message.Contains("local-reset-1")||message.Contains("123456789012345678901234")),"route diagnostics omit player names room codes socket IDs payloads and reset policy fingerprints");
            Console.WriteLine("Completed " + checks + " actual production P2P checks."); return 0;
        } catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
