using System;
using System.IO;
using System.IO.Compression;

// Lossless wrapper around the exact little-endian replay float bits.
public static class Idas3ReplayCodec
{
    public const int MaxRaw = 18000000, MaxStored = 18010000;
    public static byte[] Encode(byte[] raw)
    {
        if (raw == null || raw.Length < 96 || raw.Length > MaxRaw || BitConverter.ToUInt32(raw, 0) != 0x32524449) throw new InvalidDataException("A detailed recording is required.");
        using var output = new MemoryStream();
        output.Write(BitConverter.GetBytes(0x32474449u), 0, 4); output.Write(BitConverter.GetBytes(raw.Length), 0, 4);
        using (var gzip = new GZipStream(output, CompressionLevel.Optimal, true)) gzip.Write(raw, 0, raw.Length);
        return output.ToArray();
    }
    public static byte[] Decode(byte[] stored)
    {
        if (stored == null || stored.Length < 16 || stored.Length > MaxStored) throw new InvalidDataException("Invalid replay size.");
        if (BitConverter.ToUInt32(stored, 0) != 0x32474449) return stored;
        int length = BitConverter.ToInt32(stored, 4);
        if (length < 96 || length > MaxRaw) throw new InvalidDataException("Invalid expanded replay size.");
        using var input = new MemoryStream(stored, 8, stored.Length - 8, false);
        using var gzip = new GZipStream(input, CompressionMode.Decompress);
        var raw = new byte[length]; int at = 0;
        while (at < length) { int n = gzip.Read(raw, at, length - at); if (n == 0) throw new InvalidDataException("Truncated compressed replay."); at += n; }
        if (gzip.ReadByte() != -1 || BitConverter.ToUInt32(raw, 0) != 0x32524449) throw new InvalidDataException("Invalid compressed replay.");
        return raw;
    }
}
