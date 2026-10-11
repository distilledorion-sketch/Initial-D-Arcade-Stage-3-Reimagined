using System;
using System.Linq;
using Idas3.Multiplayer;

static class EosPacketsDriver
{
    static int checks;
    static void Check(bool value, string name)
    {
        if (!value) throw new Exception(name);
        ++checks; Console.WriteLine("PASS\t" + name);
    }
    static void Reject(Action action, string name)
    {
        bool rejected = false;
        try { action(); } catch (InvalidOperationException) { rejected = true; }
        Check(rejected, name);
    }
    static byte[] Data(int count) => Enumerable.Range(0, count).Select(x => (byte)(x * 73 + 19)).ToArray();
    static byte[][] Fragments(byte[] data, uint id) => Enumerable.Range(0, Idas3EosPackets.Count(data.Length)).Select(x => Idas3EosPackets.Fragment(data, id, x)).ToArray();
    static byte[] Add(Idas3EosPackets.Reassembler rx, byte[] packet, double time = 0) => rx.Add(packet, packet.Length, time);

    static int Main()
    {
        try {
            var ordered = new Idas3EosPackets.Reassembler(true);
            uint id = 1;
            foreach (int size in new[] { 1, 1153, 1154, 1155, 4096, 60000, 65536 }) {
                var data = Data(size); var packets = Fragments(data, id++); byte[] result = null;
                foreach (var packet in packets) {
                    Check(packet.Length <= 1170, "wire packet fits EOS limit: " + size + "/" + packet[12]);
                    result = Add(ordered, packet);
                }
                Check(result != null && result.SequenceEqual(data), "ordered round trip " + size);
                Check(ordered.BufferedBytes == 0, "completed message releases buffer " + size);
            }
            var rx = new Idas3EosPackets.Reassembler(false);
            var large = Data(65536); var shuffled = Fragments(large, 100).Reverse().ToArray();
            Add(rx, shuffled[0]); Add(rx, shuffled[0]);
            Check(rx.BufferedBytes == large.Length, "duplicate fragment does not allocate another message");
            byte[] rebuilt = null;
            foreach (var packet in shuffled.Skip(1)) rebuilt = Add(rx, packet);
            Check(rebuilt != null && rebuilt.SequenceEqual(large), "unreliable out-of-order fragments reassemble exactly");
            Check(Add(rx, shuffled[0]) == null && rx.BufferedBytes == 0, "completed duplicate ignored");
            Check(Add(rx, Fragments(Data(1), 99)[0]) == null, "old unreliable message ignored");
            Check(Add(rx, Fragments(Data(1), 101)[0]) != null, "new unreliable message accepted");

            rx = new Idas3EosPackets.Reassembler(false);
            Add(rx, Fragments(Data(2000), 1)[0]);
            Check(Add(rx, Fragments(Data(1), 2)[0], 3) != null && rx.BufferedBytes == 0, "expired unreliable fragment reclaimed");
            rx = new Idas3EosPackets.Reassembler(true);
            var partial = Fragments(Data(2000), 1); Add(rx, partial[0]);
            Reject(() => Add(rx, partial[1], 61), "incomplete reliable message times out");
            rx = new Idas3EosPackets.Reassembler(true);
            Reject(() => Add(rx, Fragments(Data(1), 2)[0]), "reliable sequence gap rejected");
            rx = new Idas3EosPackets.Reassembler(false);
            Add(rx, Fragments(Data(2000), 1)[0]);
            Reject(() => Add(rx, Fragments(Data(3000), 1)[0]), "conflicting total size rejected");

            rx = new Idas3EosPackets.Reassembler(false);
            for (uint n = 1; n <= 16; ++n) Add(rx, Fragments(Data(65536), n)[0]);
            Check(rx.BufferedBytes == 1024 * 1024, "reassembly byte budget enforced at boundary");
            Reject(() => Add(rx, Fragments(Data(65536), 17)[0]), "reassembly byte budget rejects overflow");
            rx = new Idas3EosPackets.Reassembler(false);
            for (uint n = 1; n <= 32; ++n) Add(rx, Fragments(Data(2000), n)[0]);
            Reject(() => Add(rx, Fragments(Data(2000), 33)[0]), "reassembly message count bounded");

            var valid = Fragments(Data(32), 1)[0];
            foreach (int offset in new[] { 0, 1, 2, 3, 8, 9, 10, 11, 12, 13, 14, 15 }) {
                var bad = (byte[])valid.Clone(); bad[offset] ^= 0x7f;
                Reject(() => Add(new Idas3EosPackets.Reassembler(false), bad), "malformed header byte " + offset + " rejected");
            }
            var zero = (byte[])valid.Clone(); Array.Clear(zero, 4, 4);
            Reject(() => Add(new Idas3EosPackets.Reassembler(false), zero), "zero sequence rejected");
            Reject(() => new Idas3EosPackets.Reassembler(false).Add(valid, valid.Length - 1, 0), "truncated fragment rejected");
            Reject(() => new Idas3EosPackets.Reassembler(false).Add(valid, valid.Length + 1, 0), "declared length beyond buffer rejected");
            Reject(() => new Idas3EosPackets.Reassembler(false).Add(null, 100, 0), "null packet rejected");
            Check(Idas3EosPackets.Next(uint.MaxValue) == 1, "sequence wraps without zero");
            rx = new Idas3EosPackets.Reassembler(false);
            Check(Add(rx, Fragments(Data(1), uint.MaxValue)[0]) != null && Add(rx, Fragments(Data(1), 1)[0]) != null, "unreliable sequence accepts wrap");

            var random = new Random(314159);
            for (int n = 0; n < 10000; ++n) {
                var bad = new byte[random.Next(0, 1300)]; random.NextBytes(bad);
                try { new Idas3EosPackets.Reassembler(false).Add(bad, bad.Length, n); }
                catch (InvalidOperationException) { }
            }
            Check(true, "10000 arbitrary malformed packets produce no indexing or allocation failures");
            Console.WriteLine("Completed " + checks + " checks."); return 0;
        } catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
