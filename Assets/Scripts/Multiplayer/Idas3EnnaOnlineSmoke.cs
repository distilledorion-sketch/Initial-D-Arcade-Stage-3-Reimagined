using System;
using System.Collections;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Idas3.Multiplayer {
    public sealed partial class Idas3MultiplayerSmoke {
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        static extern int Idas3SharedReadFinish(byte[] bytes,int capacity);
        IEnumerator VerifyEnnaReplay(){
            var folder=Path.Combine(root,"userdata/replays");
            yield return Until(()=>Directory.Exists(folder)&&Directory.GetFiles(folder,"*.idreplay").Length>0,25,"Online Enna replay was not saved locally");
            var files=Directory.GetFiles(folder,"*.idreplay");Check(files.Length==1,"Expected one online recording");
            var data=Idas3ReplayData.Load(files[0]);var m=data.Metadata;var choice=session.LocalChoice;
            Check(m.mode==1&&m.condition==22+(choice.Reverse?1:0)&&m.weather==(choice.Wet?1:0)&&m.night==1,"Wrong online Enna replay identity");
            Check(data.Detailed&&data.Opponent!=null&&data.Opponent.Detailed&&data.Frames.Length>600&&data.Frames.Length==data.Opponent.Frames.Length,"Online replay lost an opponent or detailed telemetry");
            Check(m.playerName==(role=="host"?"SMOKE HOST":"SMOKE JOIN")&&m.opponentName==(role=="host"?"SMOKE JOIN":"SMOKE HOST"),"Online replay names changed");
            Check(Path.GetFileName(files[0]).Contains("_enna_")&&Path.GetFileName(files[0]).Contains("_ol"),"Online Enna filename missing course/mode");
            Check(Idas3SharedReadFinish(new byte[4096],4096)==0,"Personal online recording entered leaderboard upload queue");
            File.WriteAllText(Path.Combine(root,"enna-replay.json"),JsonUtility.ToJson(m,true));
        }
    }
}
