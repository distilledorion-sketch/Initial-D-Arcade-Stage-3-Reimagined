using System;
using System.Security.Cryptography;
using System.Text;

namespace Idas3.Multiplayer
{
    internal static class Idas3BuildCompatibility
    {
        internal static bool IsValidMatchmakingKey(string value) =>
            !string.IsNullOrWhiteSpace(value) && value.Length <= 256 && value.IndexOf('\0') < 0;

        internal static string ForMatchmaking(string fullIdentity)
        {
            if (string.IsNullOrWhiteSpace(fullIdentity) || fullIdentity.IndexOf('\0') >= 0)
                throw new ArgumentException("The game build identity is invalid.", nameof(fullIdentity));
            // Course fingerprints grow as maps are added. Keep Steam metadata
            // bounded without dropping any code, course or simulation identity.
            // The session still compares the complete identity in its handshake.
            using (var hash = SHA256.Create())
                return "idas3-build1-" + Convert.ToBase64String(hash.ComputeHash(Encoding.UTF8.GetBytes(fullIdentity)));
        }
    }
}
