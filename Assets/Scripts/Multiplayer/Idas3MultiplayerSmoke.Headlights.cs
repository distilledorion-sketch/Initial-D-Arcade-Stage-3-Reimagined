using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerSmoke
    {
        private static bool HeadlightsCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-headlights-check")>=0;
        private static bool HeadlightsNight=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-headlights-night")>=0;
        [Serializable] private class HeadlightStage {
            public string name;public bool localOn,remoteOn;public uint localVisible,remoteVisible,localPhase,remotePhase;
            public ulong localTicks,remoteTicks;
        }
        [Serializable] private class HeadlightReport {
            public string schema="idas3-opponent-headlights-lan-v1",role;
            public string scope="Two isolated real Unity LAN clients. Host car0 popup lamps and guest car8 fixed lamps independently toggle the mapped H action. Both clients observe absolute on/off flags and native lamp geometry; no Steam peer is contacted.";
            public bool passed,night,savesUnchanged;public HeadlightStage[] stages;
        }
        private IEnumerator VerifyOpponentHeadlights()
        {
            Phase("opponent-headlights");var records=new List<HeadlightStage>();string saveBefore=SaveFingerprint();
            bool initial=session.Night;
            for(int stage=0;stage<5;++stage){
                // Every stage is a barrier, so one driver's next toggle cannot
                // hide the previous state before the other peer observes it.
                File.WriteAllText(Path.Combine(root,"headlights-ready-"+stage),"ready");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"headlights-ready-"+stage)),15,"Peer did not enter headlight stage.");
                string actor=stage==1||stage==3?"host":"join";
                if(stage>0&&role==actor){
                    bool before=(session.LocalSnapshot.flags&16u)!=0;
                    manualKey=KeyCode.H;
                    yield return Until(()=>((session.LocalSnapshot.flags&16u)!=0)!=before,5,"Mapped H action did not toggle local headlights.");
                    manualKey=KeyCode.None;yield return Frames(3);
                }
                bool hostOn=stage==1||stage==2?!initial:initial;
                bool joinOn=stage==2||stage==3?!initial:initial;
                bool localOn=role=="host"?hostOn:joinOn,remoteOn=role=="host"?joinOn:hostOn;
                yield return Until(()=>((session.LocalSnapshot.flags&16u)!=0)==localOn&&
                    ((session.RemoteSnapshot.flags&16u)!=0)==remoteOn&&
                    (session.LocalSnapshot.headlightVisible!=0)==localOn&&
                    (session.RemoteSnapshot.headlightVisible!=0)==remoteOn,8,
                    "Headlights or lamp geometry did not synchronize independently at stage "+stage);
                var local=session.LocalSnapshot;var remote=session.RemoteSnapshot;
                records.Add(new HeadlightStage{name="stage-"+stage,localOn=localOn,remoteOn=remoteOn,
                    localVisible=local.headlightVisible,remoteVisible=remote.headlightVisible,
                    localPhase=local.headlightPhase,remotePhase=remote.headlightPhase,localTicks=local.raceTicks,remoteTicks=remote.raceTicks});
                File.WriteAllText(Path.Combine(root,"headlights-seen-"+stage),"seen");
                yield return Until(()=>File.Exists(Path.Combine(peerRoot,"headlights-seen-"+stage)),15,"Peer did not observe headlight stage.");
            }
            Check(SaveFingerprint()==saveBefore,"Headlight changes wrote private saves or records.");
            File.WriteAllText(Path.Combine(root,"headlights-report.json"),JsonUtility.ToJson(new HeadlightReport{
                role=role,passed=true,night=initial,savesUnchanged=true,stages=records.ToArray()},true));
            Phase("racing");
        }
    }
}
