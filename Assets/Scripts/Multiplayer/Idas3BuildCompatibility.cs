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
            // Lobby search and the handshake share the same build and race rules.
            using (var hash = SHA256.Create())
                return "idas3-build1-" + Convert.ToBase64String(hash.ComputeHash(Encoding.UTF8.GetBytes(fullIdentity)));
        }
    }
}
