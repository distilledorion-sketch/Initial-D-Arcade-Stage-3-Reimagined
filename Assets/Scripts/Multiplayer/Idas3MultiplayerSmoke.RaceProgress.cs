using System;
using System.Collections;
using System.IO;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerSmoke
    {
        private bool RaceProgressCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-race-progress-check")>=0;
        [Serializable] private class RaceProgressReport
        {
            public string scope="Real muted local TCP players and shared native physics. Input packets alone are deliberately withheld; pings and room traffic continue. Not live EOS or Internet traversal.";
            public string role,mode,terminalReason;
            public bool passed,stalled,connectedWhileStalled;
            public ulong before,atStall,after,verifiedAfter;
            public int recoveryPackets,recoveries;
            public long dropped;
            public double seconds;
        }
        private IEnumerator VerifyRaceProgress()
        {
            Check(!onlineCheck&&!P2pRaceCheck&&peerRoot.Length>0,"Progress fixture requires isolated local peers.");
            Check(session.AuthorityRecoveryPacketsSent==0,"Normal showcase triggered a false input recovery.");
            bool expire=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-race-progress-timeout")>=0;
            var report=new RaceProgressReport{role=role,mode=expire?"unrecoverable":"reliable-recovery",before=session.AuthorityStatus.confirmed};
            double started=Time.realtimeSinceStartupAsDouble;
            Phase("race-input-outage");
            session.SetDiagnosticRaceInputOutage(true,true);
            File.WriteAllText(Path.Combine(root,"outage-armed.txt"),"Race inputs withheld, pings retained.");
            yield return Until(()=>File.Exists(Path.Combine(peerRoot,"outage-armed.txt")),8,"Peer did not arm its input outage.");
            yield return Until(()=>session.AuthorityStatus.stalled!=0,5,"Packet outage did not stall the native simulation.");
            report.stalled=true;report.atStall=session.AuthorityStatus.confirmed;
            yield return new WaitForSecondsRealtime(1.5f);
            report.connectedWhileStalled=session.HandshakeComplete&&session.PingMilliseconds>=0&&!session.DisconnectedFinish;
            Check(report.connectedWhileStalled,"Control route did not stay alive during the input-only outage.");
            if(expire){
                Phase("race-input-timeout");
                yield return Until(()=>session.DisconnectedFinish,18,"Healthy pings left a permanently stalled race alive.");
                Check(session.StateName=="Disconnected"&&session.BattlePointsEarned==0&&session.TuningPointsEarned==0,
                    "Input failure awarded a result or points.");
                Check(session.ResultText.Contains("NO RESULT RECORDED"),"Input failure did not show the neutral disconnect.");
                report.terminalReason=session.StatusText;
            }else{
                Phase("race-input-reliable-recovery");
                // Continue dropping every unreliable input packet. Recovery
                // must come through the ordinary reliable transport path.
                session.SetDiagnosticRaceInputOutage(true,false);
                yield return Until(()=>session.AuthorityStatus.confirmed>report.atStall+64&&session.AuthorityStatus.verified>report.atStall,12,
                    "Reliable input retransmission did not resume verified native progress.");
                Check(session.AuthorityRecoveryPacketsSent>0&&session.AuthorityRecoveries>0,"Recovery did not exercise the reliable retry.");
                session.SetDiagnosticRaceInputOutage(false,false);
                var start=session.LocalSnapshot;ulong confirmed=session.AuthorityStatus.confirmed;
                Phase("race-input-resumed");
                yield return Until(()=>session.AuthorityStatus.confirmed>=confirmed+120,8,"Normal race did not keep advancing after recovery.");
                Check(Vector3.Distance(start.actor,session.LocalSnapshot.actor)>1&&session.RemoteSnapshot.raceTicks>start.raceTicks,
                    "Confirmed recovery did not restore car movement.");
                Check(!session.DisconnectedFinish&&!menu.IsOpen,"Recovery interrupted the running race with a disconnect or menu.");
            }
            report.after=session.AuthorityStatus.confirmed;report.verifiedAfter=session.AuthorityStatus.verified;
            report.recoveryPackets=session.AuthorityRecoveryPacketsSent;report.recoveries=session.AuthorityRecoveries;
            report.dropped=session.DiagnosticDroppedPackets;report.seconds=Time.realtimeSinceStartupAsDouble-started;report.passed=true;
            File.WriteAllText(Path.Combine(root,"race-progress.json"),JsonUtility.ToJson(report,true));
            yield return Until(()=>File.Exists(Path.Combine(peerRoot,"race-progress.json")),8,"Peer did not finish its progress check.");
            driving=false;session.LeaveRoom();yield return Frames(8);
            Check(!session.IsRacing&&!session.InLobby,"Progress fixture could not leave cleanly.");
        }
    }
}
