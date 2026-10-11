using System;
using System.Collections;
using System.IO;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerSmoke
    {
        private static bool LobbyChatCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-chat-check")>=0;
        [Serializable] private sealed class LobbyChatReport {
            public string schema="idas3-lobby-chat-lan-v1",role;
            public string scope="Two real isolated Unity LAN clients send and receive pre-race chat before Ready, finish and return through the existing connected-rematch check, preserve both initial messages, and exchange a second pair before Ready again. Uses production session APIs and real TCP; no live Steam peer or real player profile.";
            public bool passed,beforeReady,historyRetained,afterReturn;
            public string[] beforeMessages,returnedMessages;
        }
        private LobbyChatReport lobbyChatReport;
        private bool HasChat(string text,bool local)
        {
            foreach(var message in session.LobbyChatMessages)if(message.Text==text&&message.IsLocal==local)return true;
            return false;
        }
        private string[] ChatTranscript()
        {
            var messages=session.LobbyChatMessages;var transcript=new string[messages.Count];
            for(int i=0;i<messages.Count;++i)transcript[i]=messages[i].Name+": "+messages[i].Text;
            return transcript;
        }
        private IEnumerator VerifyLobbyChat(bool returned)
        {
            if(lobbyChatReport==null)lobbyChatReport=new LobbyChatReport{role=role};
            string stage=returned?"after-return":"before-ready";
            string peerRole=role=="host"?"join":"host";
            string initialLocal=role+" says hello before Ready",initialPeer=peerRole+" says hello before Ready";
            Phase("lobby-chat-"+stage);
            try{
                yield return Until(()=>session.CanChat,10,"Connected lobby did not enable chat.");
                Check(!session.LocalReady&&!BothReady(),"Lobby chat check must run before either new race is readied.");
                if(returned){
                    Check(lobbyChatReport.beforeReady&&session.LobbyChatMessages.Count==2&&HasChat(initialLocal,true)&&HasChat(initialPeer,false),"Connected rematch did not retain both pre-race chat messages.");
                    lobbyChatReport.historyRetained=true;
                }else Check(session.LobbyChatMessages.Count==0,"A new room inherited an old chat conversation.");
                string localText=returned?role+" is ready to chat after the rematch":initialLocal;
                string peerText=returned?peerRole+" is ready to chat after the rematch":initialPeer;
                Check(session.SendLobbyChat(localText),"Local lobby chat send was rejected during "+stage+".");
                yield return Until(()=>HasChat(localText,true)&&HasChat(peerText,false),15,"Both-direction lobby chat did not arrive during "+stage+".");
                Check(session.LobbyChatMessages.Count==(returned?4:2),"Chat duplicated or lost a local/remote message.");
                string localName="",peerName="";
                foreach(var player in session.Players)if(player.IsLocal)localName=player.Name;else peerName=player.Name;
                foreach(var message in session.LobbyChatMessages)Check(message.Name==(message.IsLocal?localName:peerName),"Lobby chat did not use the admitted driver's displayed name.");
                File.WriteAllText(Path.Combine(root,"lobby-chat-"+stage+"-seen.txt"),"Both local and remote chat verified.\n");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"lobby-chat-"+stage+"-seen.txt")),15,"Other driver did not verify lobby chat before Ready.");
                if(!WorldOnly){menu.SetOpen(true);yield return CaptureMenu("lobby-chat-"+stage);}
                if(returned){lobbyChatReport.afterReturn=true;lobbyChatReport.returnedMessages=ChatTranscript();lobbyChatReport.passed=lobbyChatReport.beforeReady&&lobbyChatReport.historyRetained;}
                else {lobbyChatReport.beforeReady=true;lobbyChatReport.beforeMessages=ChatTranscript();}
            }finally{File.WriteAllText(Path.Combine(root,"lobby-chat-report.json"),JsonUtility.ToJson(lobbyChatReport,true));}
        }
    }
}
