using System;
using System.Collections.Generic;

namespace Idas3.Multiplayer
{
    public sealed class Idas3Room
    {
        public string Code, Name, HostName;
        public int Members, Capacity = 2;
        public ulong Order;
        public bool QuickMatch;
    }

    // All events and methods run on Unity's main thread. Implementations must
    // accept game payloads only from the one authenticated/admitted room peer.
    public interface IIdas3Transport : IDisposable
    {
        string Kind { get; }
        bool Available { get; }
        bool Connected { get; }
        bool IsHost { get; }
        string LocalId { get; }
        string LocalName { get; }
        string RemoteId { get; }
        string RemoteName { get; }
        string RoomCode { get; }
        string Status { get; }
        IReadOnlyList<Idas3Room> Rooms { get; }
        event Action<byte[]> Message;
        event Action PeerChanged;
        event Action<string> Error;
        bool Initialize();
        void Host(string roomName);
        void Join(string roomCode);
        void Browse();
        void Send(byte[] data, bool reliable);
        void Poll();
        void Leave();
    }

    // Optional lobby discovery features. The session owns queue decisions and
    // supplies the exact native + managed build identity before room requests.
    public interface IIdas3MatchmakingTransport : IIdas3Transport
    {
        bool IsBusy { get; }
        bool InLobby { get; }
        ulong RoomOrder { get; }
        int RoomMembers { get; }
        string BuildCompatibility { get; set; }
        void HostQuickMatch(string roomName);
    }
    // Quick Match can widen its search without changing manual room browsing.
    public interface IIdas3RegionalMatchmakingTransport
    {
        void BrowseQuickMatch(int distance);
    }

}
