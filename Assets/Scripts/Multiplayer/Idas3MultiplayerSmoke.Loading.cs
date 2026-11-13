using System;
using System.Collections;
using System.IO;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerSmoke
    {
        private bool LoadingCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-loading-check")>=0;
        private bool loadingDelayApplied;
        private double loadingDelaySeconds;
        private ulong loadingDelayRace;

        [Serializable] private class LoadingReport {
            public string scope="Two isolated local TCP Unity players; an optional main-thread LateUpdate delay models scenery loading. Real imported scenery and native driving; not EOS or Internet traversal.";
            public string role,course,variant;
            public bool passed,delayApplied,sceneryLoaded;
            public int requestedDelayMs;
            public double delaySeconds;
            public ulong race,confirmed,verified;
            public float localTravel,remoteTravel;
        }

        private void LateUpdate()
        {
            if(!LoadingCheck||finished||loadingDelayApplied||!session.IsRacing||session.RaceReleased||
                (host.Status.flags&16384u)==0||(host.Status.flags&1u)!=0)return;
            try {
                Check(!onlineCheck&&!P2pRaceCheck&&peerRoot.Length>0,"Loading checks require isolated local peers.");
                int delay=IntegerArgument(Environment.GetCommandLineArgs(),"-idas3-multiplayer-loading-delay-ms",0,0,25000);
                loadingDelayApplied=true;loadingDelayRace=session.CurrentRaceId;
                // This runs after the game submitted its first race frame, in
                // the same Unity phase as the synchronous scenery loaders.
                // A premature Loaded packet lets the peer choose a start time
                // that has already expired when this frame finally completes.
                var elapsed=System.Diagnostics.Stopwatch.StartNew();
                File.WriteAllText(Path.Combine(root,"loading-delay-started.txt"),delay.ToString());
                while(elapsed.ElapsedMilliseconds<delay){
                    Idas3OnlineTransportPump.KeepAliveDuringNativeLoad();
                    System.Threading.Thread.Sleep(5);
                }
                loadingDelaySeconds=elapsed.Elapsed.TotalSeconds;
                Check(!session.RaceReleased&&!session.DisconnectedFinish,"Race released or disconnected inside its first presentation frame.");
                File.WriteAllText(Path.Combine(root,"loading-delay-finished.txt"),loadingDelaySeconds.ToString("R",System.Globalization.CultureInfo.InvariantCulture));
            }catch(Exception error){Finish(false,error.ToString());}
        }

        private IEnumerator VerifyRaceLoading()
        {
            Check(loadingDelayApplied&&loadingDelayRace==session.CurrentRaceId,"LateUpdate loading fixture did not run on this race.");
            int delay=IntegerArgument(Environment.GetCommandLineArgs(),"-idas3-multiplayer-loading-delay-ms",0,0,25000);
            Check(loadingDelaySeconds>=delay/1000.0,"Requested scenery-loading delay did not occur.");
            yield return Until(()=>session.AuthorityStatus.confirmed>=120&&localTravel>1&&remoteTravel>1,12,"Loaded race did not advance both cars.");
            var scenery=FindAnyObjectByType<Idas8HakoneCourse>();
            Check(scenery!=null&&scenery.LoadedCourse==Idas8HakoneCourse.CourseName(host.Status.flags)&&
                scenery.LoadedVariant==Idas8HakoneCourse.Variant(host.Status.flags),"Race released without its selected imported scenery.");
            Check(session.RaceReleased&&!session.DisconnectedFinish&&!menu.IsOpen,"Completed loading did not enter a healthy race.");
            var report=new LoadingReport{passed=true,role=role,course=scenery.LoadedCourse,variant=scenery.LoadedVariant,
                delayApplied=loadingDelayApplied,sceneryLoaded=true,requestedDelayMs=delay,delaySeconds=loadingDelaySeconds,
                race=session.CurrentRaceId,confirmed=session.AuthorityStatus.confirmed,verified=session.AuthorityStatus.verified,
                localTravel=localTravel,remoteTravel=remoteTravel};
            File.WriteAllText(Path.Combine(root,"race-loading.json"),JsonUtility.ToJson(report,true));
            yield return Until(()=>File.Exists(Path.Combine(peerRoot,"race-loading.json")),8,"Peer did not complete the loading check.");
            driving=false;session.LeaveRoom();yield return Frames(8);
            Check(!session.IsRacing&&!session.InLobby,"Loading fixture could not leave cleanly.");
        }
    }
}
