using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;

// Only these original GDS-0033 bytes unlock startup. No saved approval flag,
// filename-only check, command-line bypass, or dependency on extracted assets.
public static class Idas3RomValidation
{
    internal const string ChdSha256 = "9be8db03c0f75415373937545786ac6f6b9d12627f0f0584ff9f2f5cd414cf8e";
    internal const long ChdBytes = 236940011;
    public const string ReadmeText = "GDS-0033 REQUIRED\n\n" +
        "Place your original gds-0033.chd in this rom folder beside InitialDUnity.exe.\n" +
        "Supported CHD: 236940011 bytes\nSHA256: " + ChdSha256 + "\n\n" +
        "Alternatively, place all four extracted disc files here:\n" +
        "gds-0033.cue\ngds-0033-track1.bin\ngds-0033-track2.bin\ngds-0033-track3.bin\n\n" +
        "The game verifies the original disc before starting. Missing, incomplete or different files cannot start the game.\n" +
        "ROM files are supplied by you and are not included in game downloads. Updates and Full Repair preserve this folder.\n";

    internal sealed class Result
    {
        public bool Verified { get; }
        public string Message { get; }
        public string Format { get; }
        internal Result(bool verified, string message, string format = null)
        { Verified = verified; Message = message; Format = format; }
    }

    private static readonly string[] TrackNames = { "gds-0033-track1.bin", "gds-0033-track2.bin", "gds-0033-track3.bin" };
    private static readonly long[] TrackBytes = { 1058400, 5049744, 1185760800 };
    private static readonly string[] TrackHashes = {
        "e96f586ec0f6c47533b4856761894d0a7538ba3d50f88c6262083d04309127d1",
        "8e047c4648d21506cf545a60ced9a62774d63ea8f076d1cae38a2088106779bc",
        "0ca6f50ed433d92ea365222e0759c84952c84f35b8d43d6244717882ed0526d1"
    };
    private const string CueLayout = "FILE \"GDS-0033-TRACK1.BIN\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n" +
        "FILE \"GDS-0033-TRACK2.BIN\" BINARY\nTRACK 02 AUDIO\nPREGAP 00:02:00\nINDEX 01 00:00:00\n" +
        "FILE \"GDS-0033-TRACK3.BIN\" BINARY\nTRACK 03 MODE1/2352\nINDEX 01 00:00:00\n";

    internal static Result Validate(string gameRoot, Action<float> progress = null, CancellationToken cancellation = default)
    {
        cancellation.ThrowIfCancellationRequested();
        try
        {
            string folder = Path.Combine(Path.GetFullPath(gameRoot), "rom");
            string chd = Path.Combine(folder, "gds-0033.chd");
            bool foundChd = File.Exists(chd);
            bool unreadableChd = false;
            if (foundChd)
            {
                try
                {
                    if (MatchesFile(chd, ChdBytes, ChdSha256, progress, cancellation))
                        return new Result(true, "GDS-0033 verified.", "CHD");
                }
                catch (Exception error) when (error is IOException || error is UnauthorizedAccessException || error is System.Security.SecurityException)
                { unreadableChd = true; } // The complete CUE/BIN dump is an independent valid input.
            }

            string cue = Path.Combine(folder, "gds-0033.cue");
            if (!File.Exists(cue)) return new Result(false, foundChd
                ? unreadableChd ? "Could not read gds-0033.chd. Check its permissions and try again." : "gds-0033.chd does not match the required GDS-0033 disc."
                : "Place gds-0033.chd in the rom folder.");
            if (!ValidCue(cue)) return new Result(false, "gds-0033.cue does not describe the required three-track disc.");
            const long total = 1058400L + 5049744L + 1185760800L;
            long completed = 0;
            for (int i = 0; i < TrackNames.Length; ++i)
            {
                cancellation.ThrowIfCancellationRequested();
                string path = Path.Combine(folder, TrackNames[i]);
                if (!File.Exists(path)) return new Result(false, "Missing " + TrackNames[i] + ".");
                long prior = completed, size = TrackBytes[i];
                if (!MatchesFile(path, size, TrackHashes[i], value => progress?.Invoke((prior + value * size) / total), cancellation))
                    return new Result(false, TrackNames[i] + " does not match the required GDS-0033 disc.");
                completed += size;
            }
            return new Result(true, "GDS-0033 verified.", "CUE/BIN");
        }
        catch (OperationCanceledException) { throw; }
        catch (Exception error) when (error is IOException || error is UnauthorizedAccessException || error is ArgumentException || error is NotSupportedException || error is System.Security.SecurityException)
        { return new Result(false, "Could not read GDS-0033. Check that the rom files are accessible."); }
    }

    private static bool ValidCue(string path)
    {
        // Never follow paths supplied by a cue sheet. Only the fixed original
        // layout is supported; harmless whitespace, comments and case may vary.
        using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
        {
            if (stream.Length > 16384) return false;
            using (var reader = new StreamReader(stream, Encoding.UTF8, true))
            {
                var layout = new StringBuilder();
                string line;
                while ((line = reader.ReadLine()) != null)
                {
                    line = Regex.Replace(line.Trim(), @"\s+", " ").ToUpperInvariant();
                    if (line.Length == 0 || line.StartsWith("REM ", StringComparison.Ordinal)) continue;
                    layout.Append(line).Append('\n');
                }
                return layout.ToString() == CueLayout;
            }
        }
    }

    private static bool MatchesFile(string path, long size, string digest, Action<float> progress, CancellationToken cancellation)
    {
        using (var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 1024 * 1024, FileOptions.SequentialScan))
            return DigestMatches(stream, size, digest, progress, cancellation);
    }

    internal static bool DigestMatches(Stream input, long expectedSize, string expectedSha256, Action<float> progress, CancellationToken cancellation)
    {
        cancellation.ThrowIfCancellationRequested();
        if (expectedSize < 0 || (input.CanSeek && input.Length - input.Position != expectedSize)) return false;
        using (var sha = SHA256.Create())
        {
            var buffer = new byte[1024 * 1024];
            long total = 0;
            for (;;)
            {
                cancellation.ThrowIfCancellationRequested();
                int count = input.Read(buffer, 0, buffer.Length);
                if (count == 0) break;
                total += count;
                if (total > expectedSize) return false;
                sha.TransformBlock(buffer, 0, count, buffer, 0);
                progress?.Invoke(expectedSize == 0 ? 1 : (float)((double)total / expectedSize));
            }
            cancellation.ThrowIfCancellationRequested();
            if (total != expectedSize) return false;
            sha.TransformFinalBlock(Array.Empty<byte>(), 0, 0);
            string actual = BitConverter.ToString(sha.Hash).Replace("-", "").ToLowerInvariant();
            return string.Equals(actual, expectedSha256, StringComparison.OrdinalIgnoreCase);
        }
    }
}
