using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Idas3.Multiplayer;

// Runs the production room-chat model without Unity, Steam, sockets or saves.
internal static class LobbyChatDriver
{
    static int checks;
    static void Check(bool condition,string description)
    {
        if(!condition)throw new InvalidOperationException(description);
        ++checks;Console.WriteLine("PASS\t"+description);
    }

    static void TextBoundaries()
    {
        Check(Idas3LobbyChat.CleanText(null)=="","Null draft is empty");
        Check(Idas3LobbyChat.CleanText(" \t\r\n\u3000")=="","Whitespace-only draft cannot produce a message");
        Check(Idas3LobbyChat.CleanText("  Ready\t for\r\n Akina?  ")=="Ready for Akina?","Pasted multiline chat stays on one clean line");
        Check(Idas3LobbyChat.CleanText("A\0B\u0007C\u202ED\u2066E\u2069")=="ABCDE","Control and bidi formatting characters cannot alter surrounding UI");
        Check(!Idas3LobbyChat.CleanText("<size=900>hi</size>").Contains("<")&&!Idas3LobbyChat.CleanText("<size=900>hi</size>").Contains(">"),"Rich-text delimiters cannot style chat UI");
        const string multilingual="Ready? 日本語 简体中文 café 🚗";
        Check(Idas3LobbyChat.CleanText(multilingual)==multilingual,"Latin, Japanese, Chinese and a valid supplementary character survive");
        Check(Idas3LobbyChat.CleanText("a\uD800b\uDC00c")=="abc","Unpaired UTF-16 surrogates are removed");
        Check(Idas3LobbyChat.CleanText(new string('x',160)).Length==160,"Exactly 160 UTF-16 units are accepted");
        Check(Idas3LobbyChat.CleanText(new string('x',10000))==new string('x',160),"An oversized paste is bounded to 160 units");
        Check(Idas3LobbyChat.CleanText(new string('x',158)+"🚗")==new string('x',158)+"🚗","A surrogate pair that fits is kept intact at the limit");
        Check(Idas3LobbyChat.CleanText(new string('x',159)+"🚗")==new string('x',159),"A surrogate pair cannot be split at the limit");
        Check(Idas3LobbyChat.CleanText(new string('x',159)+" y")==new string('x',159),"Limit handling does not leave a trailing separator");
        var strictUtf8=new UTF8Encoding(false,true);
        foreach(string candidate in new[]{new string('界',1000),string.Concat(Enumerable.Repeat("🚗",1000)),"\uD800\uDC00\uD800"}){
            string clean=Idas3LobbyChat.CleanText(candidate);
            byte[] bytes=strictUtf8.GetBytes(clean);
            Check(bytes.Length<=Idas3LobbyChat.MaxUtf8Bytes&&strictUtf8.GetString(bytes)==clean,"Sanitized Unicode fits the wire limit and round-trips through strict UTF-8");
        }
    }

    static void LocalFlow()
    {
        var chat=new Idas3LobbyChat();string clean;
        Check(!chat.TryLocal("Host"," \t",0,out clean)&&chat.LocalSequence==0&&chat.Messages.Count==0,"Blank send consumes neither history nor sequence");
        Check(chat.TryLocal("Host","  Hello  ",0,out clean)&&clean=="Hello","Accepted send returns its cleaned wire text");
        Check(chat.Messages[0].Name=="Host"&&chat.Messages[0].Text=="Hello"&&chat.Messages[0].IsLocal,"Local history stamps the admitted local name and direction");
        Check(chat.TryLocal("Host","Second",0,out clean)&&chat.TryLocal("Host","Third",0,out clean),"A short three-message local burst is accepted");
        Check(!chat.TryLocal("Host","Fourth",0,out clean)&&chat.LocalSequence==3&&chat.Messages.Count==3,"Local flooding is bounded without creating unsent history");
        Check(!chat.TryLocal("Host","Too soon",.749,out clean),"No local token is restored before 750 ms");
        Check(chat.TryLocal("Host","Recovered",.75,out clean)&&chat.LocalSequence==4,"One local send recovers at 750 ms");
        Check(!chat.TryLocal("Host","Still empty",.7501,out clean),"A restored token cannot be reused immediately");
        Check(chat.TryLocal("Host","One",100,out clean)&&chat.TryLocal("Host","Two",100,out clean)&&chat.TryLocal("Host","Three",100,out clean)&&!chat.TryLocal("Host","Four",100,out clean),"Long idle time restores only the bounded burst");
        foreach(double now in new[]{double.NaN,double.PositiveInfinity,double.NegativeInfinity,-1d}){
            var invalid=new Idas3LobbyChat();
            Check(!invalid.TryLocal("Host","Message",now,out clean)&&invalid.LocalSequence==0&&invalid.Messages.Count==0,"Invalid local clocks cannot publish a message");
        }
    }

    static void RemoteFlow()
    {
        var chat=new Idas3LobbyChat();
        Check(!chat.TryRemote("Peer","Zero",0,0)&&chat.Messages.Count==0,"Sequence zero is not a valid peer message");
        Check(chat.TryRemote("Peer"," Hi ",1,0)&&chat.Messages[0].Text=="Hi"&&!chat.Messages[0].IsLocal&&chat.Messages[0].Name=="Peer","Remote history uses negotiated name and cleaned text");
        Check(!chat.TryRemote("Spoof","duplicate",1,0)&&chat.Messages.Count==1,"A duplicate cannot overwrite attribution or append history");
        Check(chat.TryRemote("Peer","Third",3,0)&&!chat.TryRemote("Peer","Delayed second",2,1),"Reliable sequence gaps are accepted and older messages remain stale");
        Check(!chat.TryRemote("Peer"," \n",4,1)&&!chat.TryRemote("Peer","Changed retry",4,2),"Filtered remote identity cannot reappear later with different text");
        var burst=new Idas3LobbyChat();
        for(ulong sequence=1;sequence<=6;++sequence)Check(burst.TryRemote("Peer","Buffered "+sequence,sequence,0),"Reliable delivery batching accepts message "+sequence);
        Check(!burst.TryRemote("Peer","Flood",7,0)&&burst.Messages.Count==6,"Remote flood exceeds the bounded receive allowance");
        Check(!burst.TryRemote("Peer","Delayed flood retry",7,5),"Rate-rejected remote sequence stays consumed after refill");
        Check(burst.TryRemote("Peer","New message",8,.75)&&burst.Messages.Count==7,"A fresh remote message is accepted after refill");
        string clean;
        Check(burst.TryLocal("Host","Reply",.75,out clean),"Remote activity cannot exhaust the local reply allowance");
    }

    static void HistoryAndRoomIsolation()
    {
        var chat=new Idas3LobbyChat();var view=chat.Messages;string clean;ulong remote=0;
        for(int i=0;i<46;++i){
            bool accepted=i%2==0?chat.TryLocal("Host","message-"+i,i,out clean):chat.TryRemote("Peer","message-"+i,++remote,i);
            if(!accepted)throw new InvalidOperationException("Spaced history fixture send was refused");
        }
        Check(chat.Messages.Count==40&&view.Count==40,"Shared local/remote history remains capped at 40 messages");
        Check(chat.Messages[0].Text=="message-6"&&chat.Messages[39].Text=="message-45","Oldest entries are evicted while chronological order remains intact");
        Check(chat.Messages[0].IsLocal&&!chat.Messages[39].IsLocal,"History trimming preserves message attribution");
        bool readOnly=false;
        try{((IList<Idas3LobbyChatMessage>)view).Clear();}catch(NotSupportedException){readOnly=true;}
        Check(readOnly&&chat.Messages.Count==40,"Consumers cannot mutate the exposed chat history");
        var snapshot=chat.Messages[0];chat.Clear();
        Check(chat.Messages.Count==0&&view.Count==0&&chat.LocalSequence==0,"Leaving/resetting the room clears retained views and local sequence");
        Check(snapshot.Text=="message-6"&&snapshot.Name=="Host","An existing message object remains immutable after reset");
        Check(chat.TryRemote("New peer","Fresh room",1,0),"A fresh room accepts the replacement peer's first sequence");
        Check(chat.TryLocal("Host","A",0,out clean)&&chat.TryLocal("Host","B",0,out clean)&&chat.TryLocal("Host","C",0,out clean),"A fresh room starts with its normal local send allowance");
        Check(chat.LocalSequence==3&&chat.Messages.Count==4,"Reset history contains only the new room conversation");
    }

    static int Main()
    {
        try{
            TextBoundaries();LocalFlow();RemoteFlow();HistoryAndRoomIsolation();
            Console.WriteLine("TOTAL\t"+checks);return 0;
        }catch(Exception error){Console.Error.WriteLine(error);return 1;}
    }
}
