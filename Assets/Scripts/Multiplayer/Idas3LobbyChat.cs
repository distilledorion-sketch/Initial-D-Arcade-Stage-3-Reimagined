using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text;

namespace Idas3.Multiplayer
{
    public sealed class Idas3LobbyChatMessage
    {
        public string Name { get; }
        public string Text { get; }
        public bool IsLocal { get; }
        internal Idas3LobbyChatMessage(string name,string text,bool isLocal)
        { Name=name;Text=text;IsLocal=isLocal; }
    }

    // Main-thread, room-local history. Names come from the admitted session,
    // never from the chat payload. Keep this independent of Unity for checks.
    internal sealed class Idas3LobbyChat
    {
        internal const int MaxCharacters=160,MaxHistory=40,MaxUtf8Bytes=MaxCharacters*4;
        internal const double MessageInterval=.75;
        const int LocalBurst=3,RemoteBurst=6;
        readonly List<Idas3LobbyChatMessage> messages=new List<Idas3LobbyChatMessage>();
        readonly IReadOnlyList<Idas3LobbyChatMessage> view;
        double localTokens=LocalBurst,remoteTokens=RemoteBurst,localAt,remoteAt;
        ulong remoteSequence;
        internal ulong LocalSequence { get; private set; }
        internal IReadOnlyList<Idas3LobbyChatMessage> Messages=>view;
        internal Idas3LobbyChat() {view=messages.AsReadOnly();}

        internal static string CleanText(string text)
        {
            if(string.IsNullOrEmpty(text))return "";
            var clean=new StringBuilder(Math.Min(text.Length,MaxCharacters));
            bool space=false;
            for(int i=0;i<text.Length&&clean.Length<MaxCharacters;++i){
                char c=text[i];
                if(char.IsWhiteSpace(c)){space=clean.Length>0;continue;}
                // Remove formatting controls (including bidi overrides), rich
                // text delimiters, and unpaired surrogates from both peers.
                if(c=='<'||c=='>'||char.IsControl(c)||CharUnicodeInfo.GetUnicodeCategory(c)==UnicodeCategory.Format)continue;
                bool pair=char.IsHighSurrogate(c)&&i+1<text.Length&&char.IsLowSurrogate(text[i+1]);
                if(char.IsSurrogate(c)&&!pair)continue;
                int needed=(space?1:0)+(pair?2:1);
                if(clean.Length+needed>MaxCharacters)break;
                if(space){clean.Append(' ');space=false;}
                clean.Append(c);if(pair)clean.Append(text[++i]);
            }
            return clean.ToString();
        }

        static bool TakeToken(ref double tokens,ref double last,double now,int burst)
        {
            if(double.IsNaN(now)||double.IsInfinity(now)||now<0)return false;
            tokens=Math.Min(burst,tokens+Math.Max(0,now-last)/MessageInterval);last=now;
            if(tokens<1)return false;
            tokens-=1;return true;
        }

        internal bool TryLocal(string name,string text,double now,out string clean)
        {
            clean=CleanText(text);
            if(clean.Length==0||!TakeToken(ref localTokens,ref localAt,now,LocalBurst))return false;
            ++LocalSequence;Append(name,clean,true);return true;
        }

        internal bool TryRemote(string name,string text,ulong sequence,double now)
        {
            if(sequence==0||sequence<=remoteSequence)return false;
            // Reliable transport preserves order. Consume each identity even
            // when filtered so delayed duplicates cannot reappear later.
            remoteSequence=sequence;
            string clean=CleanText(text);
            // A little receive headroom tolerates reliable delivery batching.
            if(clean.Length==0||!TakeToken(ref remoteTokens,ref remoteAt,now,RemoteBurst))return false;
            Append(name,clean,false);return true;
        }

        void Append(string name,string text,bool local)
        {
            if(messages.Count==MaxHistory)messages.RemoveAt(0);
            messages.Add(new Idas3LobbyChatMessage(name,text,local));
        }

        internal void Clear()
        {
            messages.Clear();LocalSequence=remoteSequence=0;
            localTokens=LocalBurst;remoteTokens=RemoteBurst;localAt=remoteAt=0;
        }
    }
}
