using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using Idas3.Multiplayer;

internal static class LobbyChatSessionDriver
{
    const uint Magic=0x504D3349;
    const ushort Protocol=11;
    const byte Chat=19;
    const ulong LocalNonce=101,RemoteNonce=202;
    const BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    static int checks;
    static void Check(bool value,string why){if(!value)throw new InvalidOperationException(why);++checks;Console.WriteLine("PASS\t"+why);}
    static void Set(Idas3MultiplayerSession session,string field,object value)=>typeof(Idas3MultiplayerSession).GetField(field,Private).SetValue(session,value);

    sealed class Fixture:IDisposable {
        internal readonly ChatFixtureTransport Transport;
        internal readonly Idas3MultiplayerSession Session;
        internal Fixture(bool host=true){
            UnityEngine.Time.realtimeSinceStartupAsDouble=10;
            Transport=new ChatFixtureTransport{IsHost=host};
            Session=new Idas3MultiplayerSession(8,Path.Combine(Path.GetTempPath(),"idas3-chat-fixture-unavailable"));
            typeof(Idas3MultiplayerSession).GetMethod("SetTransport",Private).Invoke(Session,new object[]{Transport});
            Set(Session,"state","Lobby");Set(Session,"<HandshakeComplete>k__BackingField",true);
            Set(Session,"localNonce",LocalNonce);Set(Session,"remoteNonce",RemoteNonce);Set(Session,"remoteName","Admitted rival");
        }
        public void Dispose(){Set(Session,"nativeRace",false);Session.Dispose();}
    }

    static byte[] Packet(string text,ulong sequence=1,ulong round=0,ulong sender=RemoteNonce,ulong recipient=LocalNonce)=>RawPacket(Encoding.UTF8.GetBytes(text),sequence,round,sender,recipient);
    static byte[] RawPacket(byte[] text,ulong sequence=1,ulong round=0,ulong sender=RemoteNonce,ulong recipient=LocalNonce,int length=-1){
        using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)){
            writer.Write(Magic);writer.Write(Protocol);writer.Write(Chat);writer.Write(sender);writer.Write(recipient);
            writer.Write(round);writer.Write(sequence);writer.Write((ushort)(length<0?text.Length:length));writer.Write(text);return stream.ToArray();
        }
    }

    static void SendingAndAttribution(){
        foreach(bool host in new[]{true,false})using(var f=new Fixture(host)){
            int changes=0;f.Session.LobbyChatChanged+=()=>++changes;
            Check(f.Session.CanChat,"Admitted "+(host?"host":"guest")+" can chat in lobby");
            Check(f.Session.SendLobbyChat("  Ready\n for Akina?  "),"Lobby sender accepts a clean text message");
            Check(f.Transport.Sent.Count==1&&f.Transport.Reliable.Single(),"Session dispatches chat as one reliable packet");
            using(var reader=new BinaryReader(new MemoryStream(f.Transport.Sent[0]))){
                Check(reader.ReadUInt32()==Magic&&reader.ReadUInt16()==Protocol&&reader.ReadByte()==Chat,"Chat preserves protocol header and existing packet numbering");
                Check(reader.ReadUInt64()==LocalNonce&&reader.ReadUInt64()==RemoteNonce,"Outgoing chat binds sender and recipient session nonces");
                Check(reader.ReadUInt64()==0&&reader.ReadUInt64()==1,"Outgoing chat carries lobby generation and monotonic sequence");
                var bytes=reader.ReadBytes(reader.ReadUInt16());
                Check(Encoding.UTF8.GetString(bytes)=="Ready for Akina?"&&reader.BaseStream.Position==reader.BaseStream.Length,"Wire payload contains bounded text without an editable sender identity");
            }
            f.Transport.Deliver(Packet("Good luck!"));
            Check(f.Session.LobbyChatMessages.Count==2&&f.Session.LobbyChatMessages[1].Name=="Admitted rival"&&!f.Session.LobbyChatMessages[1].IsLocal,"Received text is attributed to the admitted session peer");
            Check(changes==2,"History events follow accepted local and peer messages");
            f.Transport.Deliver(Packet("Changed duplicate"));
            Check(f.Session.LobbyChatMessages.Count==2&&changes==2,"Packet duplicate cannot change history or trigger another notification");
            Check(!f.Session.SendLobbyChat(" \t")&&f.Transport.Sent.Count==1,"Blank chat does not dispatch a packet");
        }
    }

    static void Admission(){
        using(var f=new Fixture()){
            f.Transport.Deliver(Packet("Wrong sender",sender:999));f.Transport.Deliver(Packet("Wrong recipient",recipient:999));
            Check(f.Session.LobbyChatMessages.Count==0&&f.Session.ErrorText=="","Wrong-session packets are ignored without interrupting the room");
            Set(f.Session,"<HandshakeComplete>k__BackingField",false);f.Transport.Deliver(Packet("Before admission"));
            Check(!f.Session.CanChat&&!f.Session.SendLobbyChat("Before admission")&&f.Session.LobbyChatMessages.Count==0,"Chat is disabled before the compatibility handshake");
            Set(f.Session,"<HandshakeComplete>k__BackingField",true);
            foreach(string state in new[]{"Offline","Connecting","Matching","Loading","Countdown","Racing","Results","Returning"}){
                Set(f.Session,"state",state);f.Transport.Deliver(Packet("Outside lobby"));
                Check(!f.Session.CanChat&&!f.Session.SendLobbyChat("Outside lobby")&&f.Session.LobbyChatMessages.Count==0&&f.Session.ErrorText=="","Chat is ignored during "+state);
            }
            Set(f.Session,"state","Lobby");Set(f.Session,"nativeRace",true);f.Transport.Deliver(Packet("Native race active"));
            Check(!f.Session.CanChat&&f.Session.LobbyChatMessages.Count==0,"An active native race blocks chat even with a stale Lobby label");
            Set(f.Session,"nativeRace",false);Set(f.Session,"<CourseWinnerSlot>k__BackingField",0);f.Transport.Deliver(Packet("Selection locked"));
            Check(!f.Session.CanChat&&f.Session.LobbyChatMessages.Count==0,"Locked course selection blocks pre-race chat");
            Set(f.Session,"<CourseWinnerSlot>k__BackingField",-1);f.Transport.Connected=false;
            Check(!f.Session.CanChat&&!f.Session.SendLobbyChat("Disconnected"),"Lost transport route cannot send chat");
            f.Transport.Connected=true;f.Transport.RoomCode="";
            Check(!f.Session.CanChat&&!f.Session.SendLobbyChat("No room"),"Room membership is required for chat");
            f.Transport.RoomCode="private-chat-fixture";
            f.Transport.Deliver(Packet("Accepted after ignored attempts"));
            Check(f.Session.LobbyChatMessages.Count==1,"Ignored packets cannot consume the accepted peer sequence");
        }
    }

    static void RematchAndRoomReset(){
        using(var f=new Fixture()){
            f.Session.SendLobbyChat("First race");f.Transport.Deliver(Packet("Good luck"));
            // Mimic the already tested return handshake's generation change;
            // no native race is loaded or retired by this packet fixture.
            Set(f.Session,"returnRaceId",555UL);Set(f.Session,"state","Lobby");
            f.Transport.Deliver(Packet("Delayed prior lobby",2));
            Check(f.Session.LobbyChatMessages.Count==2,"Messages from the previous lobby generation cannot appear after a rematch");
            f.Transport.Deliver(Packet("Rematch ready",2,555));
            Check(f.Session.LobbyChatMessages.Count==3&&f.Session.LobbyChatMessages[2].Text=="Rematch ready","Same-room rematch retains history and accepts its current generation");
            Check(f.Session.SendLobbyChat("Again?"),"Local sender resumes chat in the returned lobby");
            using(var reader=new BinaryReader(new MemoryStream(f.Transport.Sent.Last()))){reader.BaseStream.Position=23;Check(reader.ReadUInt64()==555,"Returned-lobby sends use the new generation identity");}
            var view=f.Session.LobbyChatMessages;f.Session.LeaveRoom();
            Check(view.Count==0&&!f.Session.CanChat,"Leaving clears all room history and disables sending");
        }
    }

    static void MalformedMessages(){
        var cases=new[]{
            Tuple.Create("zero-length text",RawPacket(Array.Empty<byte>())),
            Tuple.Create("oversized UTF-8 bytes",RawPacket(new byte[641])),
            Tuple.Create("oversized character count",Packet(new string('x',161))),
            Tuple.Create("invalid UTF-8",RawPacket(new byte[]{0xc3,0x28})),
            Tuple.Create("truncated text",RawPacket(new byte[]{65},length:2)),
            Tuple.Create("unexpected trailing data",RawPacket(new byte[]{65,66},length:1)),
            Tuple.Create("truncated chat header",Packet("x").Take(30).ToArray())
        };
        foreach(var item in cases)using(var f=new Fixture()){
            f.Transport.Deliver(item.Item2);
            Check(!f.Session.CanChat&&f.Session.LobbyChatMessages.Count==0&&!string.IsNullOrEmpty(f.Session.ErrorText),"Malformed "+item.Item1+" cannot enter history and closes the invalid session");
        }
        using(var f=new Fixture()){
            f.Transport.Deliver(Packet(new string('界',160)));
            Check(f.Session.LobbyChatMessages.Count==1&&f.Session.LobbyChatMessages[0].Text.Length==160,"Maximum valid multibyte message is accepted by the real packet parser");
            f.Transport.Deliver(Packet("<size=900>Hi</size>\u202e",2));
            Check(!f.Session.LobbyChatMessages[1].Text.Contains("<")&&!f.Session.LobbyChatMessages[1].Text.Contains("\u202e"),"Peer text controls and rich-text delimiters are sanitized after wire decoding");
        }
    }

    static int Main(){
        try{SendingAndAttribution();Admission();RematchAndRoomReset();MalformedMessages();Console.WriteLine("TOTAL\t"+checks);return 0;}
        catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
