using System;
using System.Collections.Generic;

namespace Idas3.Multiplayer
{
    // Decisions shared by live EOS lobby callbacks and deterministic checks.
    // EOS owns identities; these checks never accept a claimed wire identity.
    internal static class Idas3EosRoomRules
    {
        internal static bool ShouldMerge(string ownRoomId,int members,string candidateId)
        {
            // The caller has already searched the exact compatibility bucket
            // for an open one-player Quick Match room. Only the larger room ID
            // moves, so two waiting hosts cannot repeatedly swap rooms.
            return members==1&&!string.IsNullOrEmpty(ownRoomId)&&!string.IsNullOrEmpty(candidateId)&&
                string.Compare(candidateId,ownRoomId,StringComparison.Ordinal)<0;
        }

        internal static string ValidateMembership(string localId,string originalOwner,string roomOwner,uint maxMembers,
            IReadOnlyList<string> memberIds,string compatibilityExpected,string compatibilityActual)
        {
            if(maxMembers!=2)return "Online room has an unsupported player limit.";
            if(string.IsNullOrEmpty(compatibilityExpected)||!string.Equals(compatibilityExpected,compatibilityActual,StringComparison.Ordinal))
                return "Online room belongs to a different game build.";
            if(memberIds==null||memberIds.Count<1||memberIds.Count>2)return "Online room has an invalid player list.";
            var distinct=new HashSet<string>(StringComparer.Ordinal);
            foreach(string member in memberIds)
                if(string.IsNullOrEmpty(member)||!distinct.Add(member))return "Online room has an invalid player list.";
            if(string.IsNullOrEmpty(localId)||!distinct.Contains(localId))return "Online room no longer contains this player.";
            if(string.IsNullOrEmpty(roomOwner)||!distinct.Contains(roomOwner))return "Online room owner is missing.";
            if(!string.IsNullOrEmpty(originalOwner)&&!string.Equals(originalOwner,roomOwner,StringComparison.Ordinal))
                return "Online room owner changed. Please rejoin.";
            return null;
        }
    }
}
