#if IDAS3_PORTABLE_PLAYER
using System;
using System.IO;
using System.Threading;

// Stock GDS-0033 race tracks are stereo planar SPSD with Yamaha 4-bit samples.
// Like the native race decoder, each channel has its own predictor and step.
internal sealed class Idas3DiscMusicPreview
{
    private const int HeaderSize = 64;
    private static readonly int[] Scales = {230, 230, 230, 230, 307, 409, 512, 614};
    private readonly string path;
    private readonly int channelBytes, start, frames;
    internal readonly int Rate;
    internal const int Channels = 2;
    internal float SongDuration => 2f * channelBytes / Rate;

    private Idas3DiscMusicPreview(string source, int rate, int bytes, int loop)
    {
        path = source;
        Rate = rate;
        channelBytes = bytes;
        int total = 2 * bytes;
        frames = Math.Min(total, 30 * rate);
        start = Math.Min(loop, total - frames);
    }

    internal static Idas3DiscMusicPreview Open(string directory, string key)
    {
        const string prefix = "stage3.";
        if (key == null || !key.StartsWith(prefix, StringComparison.Ordinal)) return null;
        string name = key.Substring(prefix.Length);
        if (name.Length == 0 || name.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0 || name.Contains(".")) return null;
        string file = Path.Combine(directory, name + ".bin");
        if (!File.Exists(file)) return null;
        using (var reader = new BinaryReader(File.OpenRead(file))) {
            byte[] h = reader.ReadBytes(HeaderSize);
            if (h.Length != HeaderSize || h[0] != 'S' || h[1] != 'P' || h[2] != 'S' || h[3] != 'D' ||
                h[8] != 3 || (h[9] & 3) != 1 || BitConverter.ToUInt16(h, 10) != 255) return null;
            int bytes = BitConverter.ToInt32(h, 12), rate = BitConverter.ToUInt16(h, 42);
            if (bytes <= 0 || bytes > 64 * 1024 * 1024 || bytes % Channels != 0 ||
                reader.BaseStream.Length - HeaderSize < bytes || rate < 8000 || rate > 48000) return null;
            long loop = (h[9] & 128) == 0 ? 0 : 16384L + BitConverter.ToUInt32(h, 44);
            if (loop >= bytes) return null;
            return new Idas3DiscMusicPreview(file, rate, bytes / Channels, (int)loop);
        }
    }

    internal float[] ReadSamples(CancellationToken cancellation)
    {
        var samples = new float[frames * Channels];
        int end = start + frames;
        using (var reader = new BinaryReader(File.OpenRead(path))) {
            for (int channel = 0; channel < Channels; channel++) {
                cancellation.ThrowIfCancellationRequested();
                reader.BaseStream.Position = HeaderSize + (long)channel * channelBytes;
                int count = (end + 1) / 2;
                byte[] encoded = reader.ReadBytes(count);
                if (encoded.Length != count) throw new EndOfStreamException();
                int history = 0, step = 127;
                // The predictor must advance from frame zero, even for excerpts at a loop point.
                for (int frame = 0; frame < end; frame++) {
                    if ((frame & 8191) == 0) cancellation.ThrowIfCancellationRequested();
                    int nibble = (encoded[frame / 2] >> (4 * (frame & 1))) & 15;
                    int delta = Math.Min(32767, ((nibble & 7) * 2 + 1) * step / 8);
                    history = Math.Clamp(history * 254 / 256 + ((nibble & 8) == 0 ? delta : -delta), -32768, 32767);
                    step = Math.Clamp(step * Scales[nibble & 7] / 256, 127, 24576);
                    if (frame >= start) samples[(frame - start) * Channels + channel] = history / 32768f;
                }
            }
        }
        return samples;
    }
}
#endif
