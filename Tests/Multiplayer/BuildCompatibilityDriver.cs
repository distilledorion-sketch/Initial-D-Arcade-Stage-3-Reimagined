using System;
using System.Collections.Generic;
using System.IO;
using Idas3.Multiplayer;

// Compile together with the production helper, without Unity or Steam dependencies.
internal static class BuildCompatibilityDriver
{
    private static int checks;

    private static void Check(bool result, string description)
    {
        if (!result) throw new InvalidOperationException(description);
        ++checks;
        Console.WriteLine("PASS\t" + description);
    }

    private static string Identity(string native, string managed, IList<string> courses, string mode)
    {
        string identity = "idas3-mp9-" + native + "-" + managed;
        for (int i = 0; i < courses.Count; ++i)
            identity += "-" + (i + 11) + "-" + courses[i];
        return identity + "-" + mode;
    }

    private static void InvalidInput(string value, string description)
    {
        bool rejected = false;
        try
        {
            Idas3BuildCompatibility.ForMatchmaking(value);
        }
        catch (ArgumentException error)
        {
            rejected = error.ParamName == "fullIdentity";
        }
        Check(rejected, description);
    }

    public static int Main(string[] args)
    {
        try
        {
            string[] fixture = File.ReadAllLines(args[0]);
            Check(fixture.Length == 7, "fixture contains native, managed, four course hashes and expected key");
            var courses = new List<string> { fixture[2], fixture[3], fixture[4], fixture[5] };
            string full = Identity(fixture[0], fixture[1], courses, "authority1");
            string key = Idas3BuildCompatibility.ForMatchmaking(full);
            Check(full.Length > 256, "current fifteen-course identity reproduces the Steam value limit");
            Check(!Idas3BuildCompatibility.IsValidMatchmakingKey(full), "uncompressed current identity is rejected by the transport guard");
            Check(key == fixture[6], "actual helper produces the independently calculated fixture key");
            Check(key.Length == 57, "matchmaking key has fixed 57-character length");
            Check(Idas3BuildCompatibility.IsValidMatchmakingKey(key), "current build can pass the transport guard");
            Check(key == Idas3BuildCompatibility.ForMatchmaking(full), "same identity gives identical matchmaking key");
            Check(key != Idas3BuildCompatibility.ForMatchmaking(Identity("changed-native", fixture[1], courses, "authority1")), "native binary changes separate matchmaking pools");
            Check(key != Idas3BuildCompatibility.ForMatchmaking(Identity(fixture[0], "changed-managed", courses, "authority1")), "managed binary changes separate matchmaking pools");
            Check(key != Idas3BuildCompatibility.ForMatchmaking(Identity(fixture[0], fixture[1], courses, "pose1")), "simulation mode changes separate matchmaking pools");
            for (int i = 0; i < courses.Count; ++i)
            {
                var changed = new List<string>(courses);
                changed[i] = "changed-course-fingerprint";
                Check(key != Idas3BuildCompatibility.ForMatchmaking(Identity(fixture[0], fixture[1], changed, "authority1")), "course " + (11 + i) + " fingerprint changes separate matchmaking pools");
                changed[i] = "absent";
                string missing = Idas3BuildCompatibility.ForMatchmaking(Identity(fixture[0], fixture[1], changed, "authority1"));
                Check(key != missing, "course " + (11 + i) + " removal separates matchmaking pools");
                changed[i] = "changed-course-fingerprint";
                Check(missing != Idas3BuildCompatibility.ForMatchmaking(Identity(fixture[0], fixture[1], changed, "authority1")), "course " + (11 + i) + " absent-to-present change separates matchmaking pools");
            }

            string scope = "-quick-smoke-" + new Guid("01234567-89ab-cdef-0123-456789abcdef").ToString("N");
            string otherScope = "-quick-smoke-" + new Guid("fedcba98-7654-3210-fedc-ba9876543210").ToString("N");
            string scoped = key + scope;
            Check(Idas3BuildCompatibility.IsValidMatchmakingKey(scoped), "quick-smoke key plus full 32-character GUID passes the transport guard");
            Check(scoped.IndexOf("-quick-smoke-", StringComparison.Ordinal) == key.Length, "quick-smoke scope retains the literal marker for lobby filtering");
            Check(scoped != key && scoped != key + otherScope, "smoke scopes remain distinct from public and other smoke rooms");
            Check(scoped.Length == 102, "quick-smoke scope stays below the Steam value limit");

            var future = new List<string>(courses);
            for (int i = 0; i < 500; ++i) future.Add(fixture[2]);
            string futureIdentity = Identity(fixture[0], fixture[1], future, "authority1");
            string futureKey = Idas3BuildCompatibility.ForMatchmaking(futureIdentity);
            Check(futureIdentity.Length > 20000, "future fixture exceeds twenty thousand characters");
            Check(futureKey.Length == key.Length && Idas3BuildCompatibility.IsValidMatchmakingKey(futureKey), "hundreds of future courses retain bounded valid metadata");
            Check(futureKey != key, "adding courses changes the compatibility key");

            Check(Idas3BuildCompatibility.ForMatchmaking("abc") == "idas3-build1-ungWv48Bz+pBQUDeXa4iI7ADYaOWF3qctBD/YfIAFa0=", "standard SHA-256 vector uses the versioned base64 encoding");
            Check(Idas3BuildCompatibility.ForMatchmaking("\u30b3\u30fc\u30b9\u00e9") == "idas3-build1-oESxJ8M3xrXNIdNrbqtD6wHbG9cUUBh/LB9hFF0T2Cc=", "non-ASCII identity uses UTF-8 encoding");
            InvalidInput(null, "null identity cannot create a valid matchmaking key");
            InvalidInput("", "empty identity cannot create a valid matchmaking key");
            InvalidInput(" \t\r\n", "whitespace identity cannot create a valid matchmaking key");
            InvalidInput("idas3\0build", "NUL identity cannot create a valid matchmaking key");
            Check(!Idas3BuildCompatibility.IsValidMatchmakingKey(null), "transport rejects null");
            Check(!Idas3BuildCompatibility.IsValidMatchmakingKey(""), "transport rejects empty strings");
            Check(!Idas3BuildCompatibility.IsValidMatchmakingKey(" \t\r\n"), "transport rejects whitespace strings");
            Check(!Idas3BuildCompatibility.IsValidMatchmakingKey("key\0value"), "transport rejects embedded NUL");
            Check(Idas3BuildCompatibility.IsValidMatchmakingKey(new string('a', 256)), "transport accepts exactly 256 characters");
            Check(!Idas3BuildCompatibility.IsValidMatchmakingKey(new string('a', 257)), "transport rejects 257 characters");
            Console.WriteLine("TOTAL\t" + checks);
            Console.WriteLine("IDENTITY_LENGTH\t" + full.Length);
            Console.WriteLine("KEY_LENGTH\t" + key.Length);
            return 0;
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error);
            return 1;
        }
    }
}
