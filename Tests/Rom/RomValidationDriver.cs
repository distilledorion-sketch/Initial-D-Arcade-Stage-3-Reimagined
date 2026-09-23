using System;
using System.Collections.Generic;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

// Compiled with the production validator, without Unity or a native game host.
internal static class RomValidationDriver
{
    private static int checks;
    private const long ChdSize = 236940011;
    private const string AbcHash = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

    private static void Check(bool result, string description)
    {
        if (!result) throw new InvalidOperationException(description);
        ++checks;
        Console.WriteLine("PASS\t" + description);
    }

    private static string Fixture(string root, string name)
    {
        string path = Path.Combine(root, name);
        Directory.CreateDirectory(path);
        return path;
    }

    private static void Reject(string root, string description)
    {
        var result = Idas3RomValidation.Validate(root);
        Check(!result.Verified, description);
        Check(!string.IsNullOrWhiteSpace(result.Message), description + " has an actionable result message");
    }

    private static string HexHash(byte[] bytes)
    {
        using (var sha = SHA256.Create())
            return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
    }

    private sealed class ShortReadStream : Stream
    {
        private readonly MemoryStream inner;
        private readonly int chunk;
        public ShortReadStream(byte[] bytes, int chunkSize) { inner = new MemoryStream(bytes); chunk = chunkSize; }
        public override bool CanRead { get { return true; } }
        public override bool CanSeek { get { return false; } }
        public override bool CanWrite { get { return false; } }
        public override long Length { get { throw new NotSupportedException(); } }
        public override long Position { get { throw new NotSupportedException(); } set { throw new NotSupportedException(); } }
        public override int Read(byte[] buffer, int offset, int count) { return inner.Read(buffer, offset, Math.Min(count, chunk)); }
        public override void Flush() { }
        public override long Seek(long offset, SeekOrigin origin) { throw new NotSupportedException(); }
        public override void SetLength(long length) { throw new NotSupportedException(); }
        public override void Write(byte[] buffer, int offset, int count) { throw new NotSupportedException(); }
        protected override void Dispose(bool disposing) { if (disposing) inner.Dispose(); base.Dispose(disposing); }
    }

    private static void StreamChecks()
    {
        byte[] abc = Encoding.ASCII.GetBytes("abc");
        using (var stream = new MemoryStream(abc))
            Check(Idas3RomValidation.DigestMatches(stream, abc.Length, AbcHash, null, CancellationToken.None), "standard SHA-256 vector verifies");
        using (var stream = new MemoryStream(abc))
            Check(!Idas3RomValidation.DigestMatches(stream, abc.Length, new string('0', 64), null, CancellationToken.None), "wrong digest is rejected");
        using (var stream = new MemoryStream(Encoding.ASCII.GetBytes("abd")))
            Check(!Idas3RomValidation.DigestMatches(stream, abc.Length, AbcHash, null, CancellationToken.None), "one changed byte is rejected");
        using (var stream = new MemoryStream(abc))
            Check(!Idas3RomValidation.DigestMatches(stream, 4, AbcHash, null, CancellationToken.None), "truncated stream is rejected even when its available bytes have the expected digest");
        using (var stream = new MemoryStream(abc))
            Check(!Idas3RomValidation.DigestMatches(stream, 2, AbcHash, null, CancellationToken.None), "extra bytes are rejected");
        using (var stream = new ShortReadStream(abc, 1))
            Check(Idas3RomValidation.DigestMatches(stream, abc.Length, AbcHash, null, CancellationToken.None), "nonseekable one-byte reads verify without assuming a full buffer read");

        byte[] large = new byte[3 * 1024 * 1024 + 17];
        for (int i = 0; i < large.Length; ++i) large[i] = (byte)(i * 31 + 7);
        var progress = new List<float>();
        using (var stream = new ShortReadStream(large, 32749))
            Check(Idas3RomValidation.DigestMatches(stream, large.Length, HexHash(large), progress.Add, CancellationToken.None), "multi-buffer nonseekable stream verifies");
        Check(progress.Count > 0 && progress[progress.Count - 1] == 1f, "successful hashing reports completion");
        float previous = 0;
        foreach (float value in progress)
        {
            Check(!float.IsNaN(value) && value >= previous && value <= 1, "hash progress stays bounded and monotonic");
            previous = value;
        }
        bool cancelled = false;
        using (var cancel = new CancellationTokenSource())
        using (var stream = new MemoryStream(abc))
        {
            cancel.Cancel();
            try { Idas3RomValidation.DigestMatches(stream, abc.Length, AbcHash, null, cancel.Token); }
            catch (OperationCanceledException) { cancelled = true; }
        }
        Check(cancelled, "pre-cancelled hashing stops instead of accepting a file");
        cancelled = false;
        using (var cancel = new CancellationTokenSource())
        using (var stream = new ShortReadStream(large, 32749))
        {
            try { Idas3RomValidation.DigestMatches(stream, large.Length, HexHash(large), value => { if (value > 0) cancel.Cancel(); }, cancel.Token); }
            catch (OperationCanceledException) { cancelled = true; }
        }
        Check(cancelled, "cancellation during a multi-buffer hash stops validation");
    }

    public static int Main(string[] args)
    {
        try
        {
            if (args.Length == 3 && (args[0] == "--validate-path" || args[0] == "--validate-locked-chd"))
            {
                string game = Path.GetFullPath(args[1]);
                Idas3RomValidation.Result actual;
                if (args[0] == "--validate-locked-chd")
                {
                    // CreateNew prevents touching any previously installed ROM.
                    using (var locked = new FileStream(Path.Combine(game, "rom", "gds-0033.chd"), FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None))
                    {
                        locked.WriteByte(1); locked.Flush();
                        actual = Idas3RomValidation.Validate(game);
                    }
                }
                else actual = Idas3RomValidation.Validate(game);
                bool expected = bool.Parse(args[2]);
                Check(actual.Verified == expected, "explicit CUE fixture validation result matches expectation");
                if (args[0] == "--validate-locked-chd" && expected)
                    Check(actual.Format == "CUE/BIN", "locked CHD falls through to the valid CUE/BIN set");
                Console.WriteLine("FORMAT\t" + (actual.Format ?? "rejected"));
                Console.WriteLine("TOTAL\t" + checks);
                return 0;
            }
            if (args.Length < 1 || args.Length > 2) throw new ArgumentException("Expected new fixture directory and optional actual game root.");
            string root = Path.GetFullPath(args[0]);
            if (Directory.Exists(root) || File.Exists(root)) throw new IOException("Use a new isolated fixture directory.");
            Directory.CreateDirectory(root);
            StreamChecks();
            Reject(Path.Combine(root, "absent-root"), "missing game root cannot pass");
            Reject(Fixture(root, "missing-rom-folder"), "missing rom folder cannot pass");
            string empty = Fixture(root, "empty-rom-folder");
            Fixture(empty, "rom"); Reject(empty, "empty rom folder cannot pass");
            string wrong = Fixture(root, "wrong-filename");
            File.WriteAllText(Path.Combine(Fixture(wrong, "rom"), "another-game.chd"), "MComprHD");
            Reject(wrong, "unrecognized ROM filename cannot pass");
            string fake = Fixture(root, "renamed-fake-chd");
            File.WriteAllText(Path.Combine(Fixture(fake, "rom"), "gds-0033.chd"), "this is not a ROM");
            Reject(fake, "renamed unrelated file cannot pass as CHD");
            string zero = Fixture(root, "empty-chd");
            File.WriteAllBytes(Path.Combine(Fixture(zero, "rom"), "gds-0033.chd"), new byte[0]);
            Reject(zero, "empty canonical CHD filename cannot pass");
            string lockedOnly = Fixture(root, "locked-chd-only");
            using (var locked = new FileStream(Path.Combine(Fixture(lockedOnly, "rom"), "gds-0033.chd"), FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None))
            {
                locked.WriteByte(1); locked.Flush();
                Reject(lockedOnly, "locked CHD without a valid alternate disc cannot pass");
            }
            string truncated = Fixture(root, "truncated-chd");
            var header = new byte[124]; Encoding.ASCII.GetBytes("MComprHD").CopyTo(header, 0); header[11] = 124; header[15] = 5;
            File.WriteAllBytes(Path.Combine(Fixture(truncated, "rom"), "gds-0033.chd"), header);
            Reject(truncated, "CHD signature with truncated content cannot pass");
            string fullFake = Fixture(root, "correct-size-fake-chd");
            using (var stream = File.Create(Path.Combine(Fixture(fullFake, "rom"), "gds-0033.chd")))
            {
                stream.Write(header, 0, header.Length); stream.SetLength(ChdSize);
            }
            Reject(fullFake, "matching CHD filename, signature and size still require the canonical digest");
            string incomplete = Fixture(root, "incomplete-bin-set");
            string binRoot = Fixture(incomplete, "rom");
            File.WriteAllText(Path.Combine(binRoot, "gds-0033.cue"), "FILE \"gds-0033-track1.bin\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n");
            File.WriteAllBytes(Path.Combine(binRoot, "gds-0033-track1.bin"), new byte[2352]);
            Reject(incomplete, "partial or truncated BIN set cannot pass");
            string noCue = Fixture(root, "bin-without-cue");
            string noCueRom = Fixture(noCue, "rom");
            for (int track = 1; track <= 3; ++track)
                File.WriteAllBytes(Path.Combine(noCueRom, "gds-0033-track" + track + ".bin"), new byte[2352]);
            Reject(noCue, "BIN filenames without the required CUE cannot pass");
            int unsafeIndex = 0;
            foreach (string target in new[] { "../outside.bin", "..\\outside.bin", "/outside.bin", "C:\\outside.bin" })
            {
                string unsafeRoot = Fixture(root, "unsafe-cue-" + (++unsafeIndex));
                File.WriteAllText(Path.Combine(Fixture(unsafeRoot, "rom"), "gds-0033.cue"), "FILE \"" + target + "\" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n");
                Reject(unsafeRoot, "CUE outside the rom folder cannot pass: " + target);
            }
            string metadata = Fixture(root, "metadata-only");
            string metadataRom = Fixture(metadata, "rom");
            File.WriteAllText(Path.Combine(metadataRom, "README.md"), "gds-0033 verified");
            File.WriteAllText(Path.Combine(metadataRom, "manifest.json"), "{\"verified\":true}");
            File.WriteAllText(Path.Combine(metadataRom, "gds-0033.chd.sha256"), "9be8db03c0f75415373937545786ac6f6b9d12627f0f0584ff9f2f5cd414cf8e");
            Reject(metadata, "metadata and claimed hashes cannot stand in for the ROM");
            if (args.Length == 2)
            {
                var verified = Idas3RomValidation.Validate(Path.GetFullPath(args[1]));
                Check(verified.Verified, "explicit actual game root validates its installed canonical ROM");
                Check(!string.IsNullOrWhiteSpace(verified.Format), "verified actual ROM reports its format");
                Console.WriteLine("FORMAT\t" + verified.Format);
            }
            Console.WriteLine("TOTAL\t" + checks);
            return 0;
        }
        catch (Exception error) { Console.Error.WriteLine(error); return 1; }
    }
}
