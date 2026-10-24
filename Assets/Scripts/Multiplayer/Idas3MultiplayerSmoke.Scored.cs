using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerSmoke
    {
        private static bool ScoredCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-scored-check")>=0;
        private const string ScoredScope="Two isolated packaged LAN clients; ordinary generated wheel input, unchanged wall-clock delta and original shared physics/rules. Host then guest crosses the normal Myogi finish. Other driver remains stationary and times out. Actual native tuning saves, managed history, duplicate delivery, native points/Continue and connected rematch are checked. Not EOS/NAT, human input, power-loss atomicity or two moving finishers.";
        private int scoredDriver;
        private readonly float[] scoredGuidance=new float[6];
        private long scoredInputSamples;
        private string scoredProfilePath;
        private uint[] scoredInitialProfile;
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        private static extern int Idas3MultiplayerScoredDiagnosticControls([Out] float[] values,int count);

        [Serializable] private class ScoredRace
        {
            public int winner,tuningPoints,battlePoints;
            public ulong raceId,hostTicks,guestTicks,authorityFrame,verifiedFrame;
            public double seconds;
            public uint balance;
            public string result,saveFingerprint;
            public bool naturalFinish,duplicatePreserved,continued;
            public Idas3BattleRecord before,after;
        }
        [Serializable] private class ScoredReport
        {
            public string schema="idas3-scored-race-cycle-v1",applicationVersion=Application.version,role,phase,scope=ScoredScope;
            public bool passed,connectedRematch,persistedAfterLeaving,unrelatedProfilePreserved;
            public int checks;public long inputSamples;
            public ScoredRace[] races;
        }

        private Idas3ControlBindings.PadState ScoredPad()
        {
            var pad=new Idas3ControlBindings.PadState{connected=true};
            if(finished||frozen||!driving||!session.IsRacing||!session.RaceReleased||session.StateName=="Results"||
                scoredDriver!=(role=="host"?0:1))return pad;
            if(Idas3MultiplayerScoredDiagnosticControls(scoredGuidance,scoredGuidance.Length)!=1){
                Finish(false,"Read-only scored guidance unavailable: "+Idas3Native.Error());return pad;
            }
            // Invert the documented Flycast wheel ADC mapping. The resulting
            // axis still passes through production bindings and native driver().
            int adc=128+(int)Math.Round(scoredGuidance[0]*80,MidpointRounding.AwayFromZero);
            pad.thumbLX=(short)Math.Max(-32768,Math.Min(32767,(adc<<8)+128-32768));
            pad.rightTrigger=255;++scoredInputSamples;return pad;
        }

        private static uint ProfileHash(byte[] bytes)
        {
            uint hash=2166136261;
            unchecked{for(int i=0;i<bytes.Length;++i)if(i<20||i>=24){hash^=bytes[i];hash*=16777619;}}
            return hash;
        }
        private void WriteScoredProfile(string path,uint[] words)
        {
            using(var stream=new MemoryStream()){
                using(var writer=new BinaryWriter(stream,System.Text.Encoding.UTF8,true)){
                    writer.Write(new byte[]{73,68,51,80,82,70,49,0});writer.Write(1u);writer.Write(words[4]);writer.Write(307u);writer.Write(0u);
                    foreach(uint word in words)writer.Write(word);
                }
                var bytes=stream.ToArray();Array.Copy(BitConverter.GetBytes(ProfileHash(bytes)),0,bytes,20,4);
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                using(var target=new FileStream(path,FileMode.CreateNew,FileAccess.Write))target.Write(bytes,0,bytes.Length);
            }
        }
        private uint ReadScoredBalance()
        {
            var bytes=File.ReadAllBytes(scoredProfilePath);
            Check(bytes.Length==1252&&BitConverter.ToUInt32(bytes,20)==ProfileHash(bytes),"Tuning save is missing or invalid on disk.");
            for(int i=0;i<307;++i)if(i!=18)Check(BitConverter.ToUInt32(bytes,24+i*4)==scoredInitialProfile[i],"Reward changed an unrelated profile word: "+i);
            return BitConverter.ToUInt32(bytes,24+72);
        }
        private string PrepareScoredProfile()
        {
            Check(File.Exists(Path.Combine(root,"ISOLATED_SCORED_RACE_TEST.txt"))&&!session.IsRacing,"Fixture requires a marked private pre-race profile.");
            int car=role=="host"?0:8;
            scoredInitialProfile=new uint[307];
            Check(Idas3MultiplayerNative.Idas3MultiplayerReadCar(5+car,scoredInitialProfile,307)>0,"Stock profile cannot be read for private fixture.");
            scoredInitialProfile[41]=(scoredInitialProfile[41]&0xffffff00u)|63u;
            scoredInitialProfile[17]=0;scoredInitialProfile[18]=321;
            string slot=Path.Combine(root,"userdata","saves","slot_1");
            scoredProfilePath=Path.Combine(slot,"driver_profiles_v1","car_"+car.ToString("D2")+".profile");
            WriteScoredProfile(scoredProfilePath,scoredInitialProfile);
            File.WriteAllText(Path.Combine(slot,"save.txt"),"idas3-save-slot-v1 "+car+" 0 0 0 220 220 220 220 220 -\n");
            var other=(uint[])scoredInitialProfile.Clone();other[4]=7;other[18]=777;
            string unrelated=Path.Combine(slot,"driver_profiles_v1","car_07.profile");WriteScoredProfile(unrelated,other);
            Check(ReadScoredBalance()==321,"Private baseline balance differs.");return unrelated;
        }
        private IEnumerator ScoredBarrier(string name,double seconds=30)
        {
            File.WriteAllText(Path.Combine(root,name+".txt"),"ready\n");
            yield return Until(()=>File.Exists(Path.Combine(peerRoot,name+".txt")),seconds,"Peer did not reach "+name);
        }
        private IEnumerator DriveScoredRace(int cycle)
        {
            double deadline=Time.realtimeSinceStartupAsDouble+280,next=0;
            using(var trace=new StreamWriter(Path.Combine(root,"scored-"+cycle+"-trajectory.csv"))){
                trace.WriteLine("seconds,localTicks,remoteTicks,speed,progress,authorityFrame,verified,winner,inputSamples");
                while(!session.CanReturnToLobby&&Time.realtimeSinceStartupAsDouble<deadline){
                    if(Time.realtimeSinceStartupAsDouble>=next){
                        var local=session.LocalSnapshot;var authority=session.AuthorityStatus;
                        trace.WriteLine(string.Format(System.Globalization.CultureInfo.InvariantCulture,
                            "{0:F3},{1},{2},{3:F3},{4:F3},{5},{6},{7},{8}",
                            Time.realtimeSinceStartupAsDouble,local.raceTicks,session.RemoteSnapshot.raceTicks,
                            local.speed,local.progress,authority.frame,authority.verified,authority.winner,scoredInputSamples));
                        trace.Flush();next=Time.realtimeSinceStartupAsDouble+1;
                    }
                    Check(!session.DisconnectedFinish&&session.IsRacing,"Scored race disconnected before its result.");
                    yield return null;
                }
            }
            Check(session.CanReturnToLobby,"Ordinary driving failed to produce a saved scored result.");
        }
        private IEnumerator RunScored()
        {
            var report=new ScoredReport{role=role};var races=new List<ScoredRace>();int firstCheck=checks;
            try{
                yield return Frames(5);Check(host.Ready,"Native game did not initialize.");
                menu=FindFirstObjectByType<Idas3MultiplayerMenu>();Check(menu!=null,"Online menu missing.");
                string unrelated=PrepareScoredProfile();byte[] unrelatedBefore=File.ReadAllBytes(unrelated);
                host.GameOptions.BeginEdit();var options=host.GameOptions.Draft;
                options.controllerResponse=2;options.SteeringDeadzone=0;options.steeringSmoothing=0;
                options.vSync=false;options.frameRateLimit=60;options.antiAliasing=1;
                Check(host.GameOptions.ApplyDraft(),"Private wheel settings could not be applied.");
                session.ConfigureLocalTest(port,"SCORED "+role.ToUpperInvariant());session.RefreshGarage();session.SetCar(role=="host"?0:8);session.SetAutomatic(true);
                Check(session.LocalSavedCar.Saved&&session.LocalSavedCar.SaveSlot==0&&(session.LocalSavedCar.Words[41]&255)==63,"Private tuned save not selected.");
                session.SetRaceOptions(0,false,false,false);
                if(role=="host"){
                    Phase("hosting");session.HostRoom();
                    yield return Until(()=>session.InLobby&&!string.IsNullOrEmpty(session.RoomCode),20,"Scored host did not open.");
                    roomCode=session.RoomCode;
                    File.WriteAllText(Path.Combine(root,"room.json"),JsonUtility.ToJson(new RoomNotice{role=role,code=roomCode,port=port,course=0,phase=phase},true));
                }else{Phase("joining");session.JoinRoom(address);}
                yield return Until(()=>session.HandshakeComplete&&PeerConnected()&&session.CanReady,60,"Scored handshake did not finish.");
                roomCode=session.RoomCode;session.SetRaceOptions(0,false,false,false);
                if(role=="host"){session.SetBoost(false);session.SetCollisions(false);}
                yield return Until(()=>CarSelectionAgrees()&&session.RemoteChoice.Equals(session.LocalChoice)&&!session.BoostEnabled&&!session.CollisionsEnabled&&session.CanReady,20,"Scored profiles/rules did not agree.");
                object transport=SessionField<object>("transport");ulong previousRace=0;uint expectedBalance=321;
                var records=new Idas3MultiplayerRecords(Path.Combine(root,"userdata"));
                Check(records.Read(session.LocalCar).Equals(Idas3BattleRecord.Fresh),"Scored test did not start with fresh history.");
                for(int cycle=0;cycle<2;++cycle){
                    scoredDriver=cycle;var item=new ScoredRace{winner=cycle,before=records.Read(session.LocalCar)};races.Add(item);
                    Phase("scored-ready-"+cycle);driving=false;menu.SetOpen(true);
                    yield return ScoredBarrier("scored-ready-"+cycle);
                    yield return Until(()=>session.CanReady,15,"Rematch selection was not acknowledged.");
                    session.SetReady(true);
                    if(role=="host"){yield return Until(()=>session.CanStart,20,"Both drivers did not ready.");session.StartRace();}
                    yield return Until(()=>session.IsRacing&&session.RaceReleased,60,"Scored race did not start.");
                    item.raceId=SessionField<ulong>("raceId");Check(item.raceId!=0&&item.raceId!=previousRace,"Rematch reused a race ID.");previousRace=item.raceId;
                    Check(session.ExperimentalAuthority&&!menu.IsOpen&&ReferenceEquals(transport,SessionField<object>("transport")),"Scored race lost authority, menu closure or transport.");
                    Check(Idas3Native.Idas3SceneGetControllerResponse()==2&&Idas3Native.Idas3SceneGetSteeringDeadzone()==0&&Idas3Native.Idas3SceneGetSteeringSmoothing()==0,"Private wheel configuration changed.");
                    Phase("scored-driving-"+cycle);double beganRace=Time.realtimeSinceStartupAsDouble;driving=true;
                    yield return DriveScoredRace(cycle);
                    driving=false;item.seconds=Time.realtimeSinceStartupAsDouble-beganRace;lastAuthority=session.AuthorityStatus;
                    item.authorityFrame=lastAuthority.frame;item.verifiedFrame=lastAuthority.verified;
                    item.hostTicks=lastAuthority.hostFinishTicks;item.guestTicks=lastAuthority.clientFinishTicks;
                    item.naturalFinish=lastAuthority.winner==cycle&&item.verifiedFrame>10000;
                    Check(item.naturalFinish&&!session.DisconnectedFinish&&!session.ResultSavePending,"Wrong winner or unverified native finish.");
                    Check(item.seconds>150,"Scored race bypassed the normal wall clock.");
                    bool won=cycle==(role=="host"?0:1);item.result=session.ResultText;item.tuningPoints=session.TuningPointsEarned;item.battlePoints=session.BattlePointsEarned;
                    Check(item.result==(won?"YOU WIN":"YOU LOSE")&&item.tuningPoints==(won?2000:1000),"Incorrect win/loss or tuning award.");
                    expectedBalance+=(uint)item.tuningPoints;item.balance=ReadScoredBalance();item.after=records.Read(session.LocalCar);
                    Check(item.balance==expectedBalance&&item.after.battles==item.before.battles+1&&item.after.wins==item.before.wins+(won?1u:0u),"Persisted result differs from earned result.");
                    item.saveFingerprint=SaveFingerprint();
                    Check(Idas3MultiplayerNative.Idas3MultiplayerSetResult(cycle)==1,"Duplicate native result rejected.");
                    var show=typeof(Idas3MultiplayerSession).GetMethod("ShowResult",BindingFlags.Instance|BindingFlags.NonPublic);
                    Check(show!=null,"Managed result entry point missing.");show.Invoke(session,new object[]{cycle,item.hostTicks,item.guestTicks});
                    yield return Frames(5);item.duplicatePreserved=SaveFingerprint()==item.saveFingerprint;
                    Check(item.duplicatePreserved&&ReadScoredBalance()==expectedBalance&&records.Read(session.LocalCar).Equals(item.after),"Duplicate result repeated an award or save.");
                    yield return CaptureMenu("scored-"+cycle+"-finish");
                    yield return ScoredBarrier("scored-results-"+cycle);
                    yield return Until(()=>menu.ResultsPage==1&&menu.CanAcknowledgeReturnToLobby,30,"Native result did not reach points.");
                    yield return Until(()=>Idas3MultiplayerNative.Idas3MultiplayerResultScreen(5)>=1,5,"Native points reveal did not complete.");
                    yield return CaptureMenu("scored-"+cycle+"-points");pulse=13;
                    yield return Until(()=>menu.ResultsPage==2&&menu.CanAcknowledgeReturnToLobby,8,"Points did not reach Continue.");
                    yield return CaptureMenu("scored-"+cycle+"-continue");yield return ScoredBarrier("scored-continue-"+cycle);
                    if(cycle==0){
                        pulse=13;
                        yield return Until(()=>session.InLobby&&session.HandshakeComplete&&!session.IsRacing,30,"Both Yes did not keep the room for rematch.");
                        yield return Frames(10);
                        item.continued=host.Status.frontendStage==5&&menu.IsOpen&&session.RoomCode==roomCode&&ReferenceEquals(transport,SessionField<object>("transport"));
                        Check(item.continued&&!session.LocalReady&&session.LocalSavedCar.SaveSlot==0&&session.LocalSavedCar.Words[18]==expectedBalance,"Lobby retained stale readiness, points or selected car.");
                        yield return Until(()=>session.RemoteRecord.battles==1,10,"Peer battle history did not refresh.");
                        report.connectedRematch=true;
                    }else{
                        // Both take the real No path; each must restore mode select.
                        pulse=27;yield return Until(()=>!session.IsRacing&&!session.InLobby,15,"No failed to leave race and room.");
                        yield return Frames(10);Check(host.Status.frontendStage==5,"No did not restore mode select.");
                    }
                }
                Check(ReadScoredBalance()==3321,"Final tuning balance differs after leaving.");
                var reloaded=new Idas3MultiplayerRecords(Path.Combine(root,"userdata")).Read(role=="host"?0:8);
                report.persistedAfterLeaving=reloaded.battles==2&&reloaded.wins==1&&reloaded.Equals(races[1].after);
                report.unrelatedProfilePreserved=Convert.ToBase64String(File.ReadAllBytes(unrelated))==Convert.ToBase64String(unrelatedBefore);
                Check(report.persistedAfterLeaving&&report.unrelatedProfilePreserved,"Reloaded history or unrelated car changed.");
                yield return ScoredBarrier("scored-complete");report.passed=true;
            }finally{
                driving=false;report.phase=phase;report.checks=checks-firstCheck;report.inputSamples=scoredInputSamples;report.races=races.ToArray();
                File.WriteAllText(Path.Combine(root,"scored-report.json"),JsonUtility.ToJson(report,true));
            }
            Finish(true,null);
        }
    }
}
