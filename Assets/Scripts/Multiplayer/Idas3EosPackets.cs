using System;
using System.Collections.Generic;

namespace Idas3.Multiplayer
{
    // EOS caps each packet at 1170 bytes. Application framing keeps the existing
    // 64 KiB game-message contract without exceeding that limit. This class has
    // no SDK or Unity dependencies so malformed fragments can be tested offline.
    internal static class Idas3EosPackets
    {
        internal const int PacketLimit = 1170, Header = 16, Chunk = PacketLimit - Header, MessageLimit = 65536;
        internal static int Count(int size) => (size + Chunk - 1) / Chunk;
        internal static byte[] Fragment(byte[] message, uint id, int index)
        {
            if (message == null || message.Length < 1 || message.Length > MessageLimit || id == 0 || index < 0 || index >= Count(message.Length)) throw new ArgumentOutOfRangeException();
            int count = Math.Min(Chunk, message.Length - index * Chunk);
            var data = new byte[Header + count];
            data[0] = (byte)'I'; data[1] = (byte)'D'; data[2] = (byte)'P'; data[3] = 1;
            Write(data, 4, id); Write(data, 8, (uint)message.Length);
            data[12] = (byte)index; data[13] = (byte)(index >> 8);
            int fragments = Count(message.Length); data[14] = (byte)fragments; data[15] = (byte)(fragments >> 8);
            Buffer.BlockCopy(message, index * Chunk, data, Header, count);
            return data;
        }
        static uint Read(byte[] data, int offset) => (uint)(data[offset] | data[offset + 1] << 8 | data[offset + 2] << 16 | data[offset + 3] << 24);
        static void Write(byte[] data, int offset, uint value) { for (int i = 0; i < 4; ++i) data[offset + i] = (byte)(value >> (8 * i)); }
        internal static uint Next(uint value) => value == uint.MaxValue ? 1 : value + 1;

        internal sealed class Reassembler
        {
            sealed class Assembly { internal byte[] Data; internal bool[] Seen; internal int Received; internal double Expires; }
            readonly Dictionary<uint, Assembly> pending = new Dictionary<uint, Assembly>();
            readonly List<uint> expired = new List<uint>();
            readonly bool reliable;
            int bytes;
            uint nextReliable = 1, lastUnreliable;
            internal int BufferedBytes => bytes;
            internal Reassembler(bool ordered) { reliable = ordered; }
            internal byte[] Add(byte[] packet, int length, double now)
            {
                if (packet == null || length < Header + 1 || length > PacketLimit || length > packet.Length || packet[0] != 'I' || packet[1] != 'D' || packet[2] != 'P' || packet[3] != 1)
                    throw new InvalidOperationException("Invalid online packet header.");
                uint id = Read(packet, 4), total = Read(packet, 8);
                int index = packet[12] | packet[13] << 8, count = packet[14] | packet[15] << 8;
                if (id == 0 || total < 1 || total > MessageLimit || count != Count((int)total) || index >= count || length - Header != Math.Min(Chunk, (int)total - index * Chunk))
                    throw new InvalidOperationException("Invalid online packet size.");
                if (reliable && id != nextReliable) throw new InvalidOperationException("Reliable online messages arrived out of order.");
                if (!reliable && lastUnreliable != 0 && unchecked((int)(id - lastUnreliable)) <= 0) return null;
                expired.Clear();
                foreach (var pair in pending) if (pair.Value.Expires < now) expired.Add(pair.Key);
                foreach (uint key in expired) { if (reliable) throw new InvalidOperationException("An online message could not be completed."); bytes -= pending[key].Data.Length; pending.Remove(key); }
                if (!pending.TryGetValue(id, out var item)) {
                    if (pending.Count >= 32 || bytes + total > 1024 * 1024) throw new InvalidOperationException("Online packet reassembly buffer is full.");
                    item = new Assembly { Data = new byte[total], Seen = new bool[count], Expires = now + (reliable ? 60 : 2) };
                    pending.Add(id, item); bytes += (int)total;
                }
                if (item.Data.Length != total || item.Seen.Length != count) throw new InvalidOperationException("Online packet fragments disagree.");
                if (item.Seen[index]) return null;
                Buffer.BlockCopy(packet, Header, item.Data, index * Chunk, length - Header);
                item.Seen[index] = true;
                if (++item.Received < count) return null;
                pending.Remove(id); bytes -= item.Data.Length;
                if (reliable) nextReliable = Next(nextReliable); else lastUnreliable = id;
                return item.Data;
            }
        }
    }
}
