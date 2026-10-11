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
        using (var route = new Idas3EosP2P(owner, new ProductUserId("peer"), "Peer", "room", "compatible", host))
            body(owner, route, Idas3EosPlatform.Handle.P2P);
    }
    static int Main() {
        try {
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
            Console.WriteLine("Completed " + checks + " actual production P2P checks."); return 0;
        } catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
