namespace Idas3.Multiplayer
{
    // Loading may service network I/O, but gameplay callbacks wait for the normal Poll.
    public static class Idas3OnlineTransportPump
    {
        public static long NativeLoadKeepAlivePumps => Idas3EosTransport.NativeLoadKeepAlivePumps;
        public static long NativeLoadBufferedEvents => Idas3EosTransport.NativeLoadBufferedEvents;

        public static void KeepAliveDuringNativeLoad()
        {
            Idas3EosTransport.KeepAliveDuringNativeLoad();
        }
    }
}
