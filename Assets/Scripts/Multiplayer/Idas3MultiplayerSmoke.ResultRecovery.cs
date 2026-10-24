using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;

namespace Idas3.Multiplayer
{
    public sealed partial class Idas3MultiplayerSmoke
    {
        private static bool ScoredRecoveryCheck=>Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-multiplayer-result-recovery-check")>=0;
        private bool scoredRecoveryInput,scoredRecordDirectory;
        private string scoredRecordBlock;
        private FileStream scoredRecordLock;
        private readonly List<ResultRecoveryCheck> scoredRecoveries=new List<ResultRecoveryCheck>();
        [Serializable] private class ResultRecoveryCheck
        {
            public int cycle,clicks;
            public string input;
            public bool passed,durableIntent,historyBlocked,focusGuard,heldGuard,retried;
            public string[] captures;
            public List<RecoveryInputObservation> inputTrace=new List<RecoveryInputObservation>();
        }
        [Serializable] private class RecoveryInputObservation
        {
            public string stage,selected,key;
            public int frame,axis,pad;
            public double time,nextRepeat;
            public bool armed,pointerActive,pending,visible,inLobby,racing,focused;
        }
        private void TraceRecoveryInput(string stage)
        {
            if(scoredRecoveries.Count==0)return;
            var flags=BindingFlags.Instance|BindingFlags.NonPublic;
            var focus=(Idas3MenuFocus)typeof(Idas3MultiplayerMenu).GetField("controllerFocus",flags).GetValue(menu);
            scoredRecoveries[scoredRecoveries.Count-1].inputTrace.Add(new RecoveryInputObservation{
                stage=stage,selected=focus.Selected,key=manualKey.ToString(),pad=manualPad,frame=Time.frameCount,
                time=Time.realtimeSinceStartupAsDouble,
                nextRepeat=(double)typeof(Idas3MenuFocus).GetField("nextRepeat",flags).GetValue(focus),
                axis=(int)typeof(Idas3MenuFocus).GetField("axis",flags).GetValue(focus),
                armed=(bool)typeof(Idas3MenuFocus).GetField("armed",flags).GetValue(focus),
                pointerActive=Idas3MenuPointer.Active,pending=session.ResultSavePending,visible=menu.ResultRecoveryVisible,
                inLobby=session.InLobby,racing=session.IsRacing,focused=host.DiagnosticFocusOverride??Application.isFocused});
        }
        private IEnumerator RecoveryFrames(string stage,int count)
        {
            TraceRecoveryInput(stage+"-before");
            for(int i=0;i<count;++i){yield return null;TraceRecoveryInput(stage);}
        }
        [Serializable] private class ResultRecoveryReport
        {
            public string schema="idas3-scored-result-recovery-v1",role,applicationVersion=Application.version;
            public string scope="Actual scored LAN results with private blocked history writes. Production keyboard/controller menu routing and real OnGUI rendering. No EOS, physical hardware or power-loss claim.";
            public bool passed;
            public ResultRecoveryCheck[] cases;
        }
        private void SaveRecoveryReport()
        {
            File.WriteAllText(Path.Combine(root,"result-recovery-report.json"),JsonUtility.ToJson(new ResultRecoveryReport{
                role=role,passed=scoredRecoveries.Count==2&&scoredRecoveries.TrueForAll(x=>x.passed),cases=scoredRecoveries.ToArray()},true));
        }
        private void BlockScoredRecord()
        {
            Check(File.Exists(Path.Combine(root,"ISOLATED_SCORED_RACE_TEST.txt"))&&!session.IsRacing,"Only a marked isolated pre-race save may be obstructed.");
            Check(scoredRecordBlock==null,"Previous private record obstruction remains.");
            scoredRecordBlock=Path.Combine(root,"userdata","online_records_v1","car_"+session.LocalCar.ToString("D2")+".json");
            Directory.CreateDirectory(Path.GetDirectoryName(scoredRecordBlock));
            if(File.Exists(scoredRecordBlock))scoredRecordLock=new FileStream(scoredRecordBlock,FileMode.Open,FileAccess.Read,FileShare.Read);
            else{
                Check(!Directory.Exists(scoredRecordBlock),"Unexpected existing obstruction.");
                Directory.CreateDirectory(scoredRecordBlock);scoredRecordDirectory=true;
            }
        }
        private void CloseScoredRecordBlock()
        {
            scoredRecordLock?.Dispose();scoredRecordLock=null;
            if(scoredRecordDirectory){
                string expected=Path.GetFullPath(Path.Combine(root,"userdata","online_records_v1"));
                Check(Path.GetDirectoryName(Path.GetFullPath(scoredRecordBlock))==expected&&
                    (File.GetAttributes(scoredRecordBlock)&FileAttributes.ReparsePoint)==0,"Unsafe private obstruction path.");
                Directory.Delete(scoredRecordBlock,false);scoredRecordDirectory=false;
            }
            scoredRecordBlock=null;
        }
        private void RecoveryButton(bool pad,KeyCode key,ushort buttons)
        {manualKey=pad?KeyCode.None:key;manualPad=pad?buttons:(ushort)0;}
        private IEnumerator CaptureRecoveryPanel(string name,int width,int height)
        {
            Screen.SetResolution(width,height,FullScreenMode.Windowed);
            yield return Until(()=>Screen.width==width&&Screen.height==height,8,"Recovery viewport did not reach requested size.");
            yield return Frames(5);
            // Capture the actual menu's own OnGUI Repaint. The black surrounding
            // area is deliberate; this checks the modal, not the native backdrop.
            var target=new RenderTexture(width,height,0,RenderTextureFormat.ARGB32){antiAliasing=1};
            Texture2D picture=null;
            Check(target.Create(),"Recovery capture target failed.");
            try{
                var old=RenderTexture.active;RenderTexture.active=target;GL.Clear(true,true,Color.black);RenderTexture.active=old;
                int repaint=menu.DiagnosticRepaints;menu.RequestDiagnosticCapture(target);
                yield return Until(()=>menu.DiagnosticCaptureReady,5,"Recovery modal received no actual Repaint.");
                Check(menu.ResultRecoveryVisible&&menu.DiagnosticRepaints>repaint,"Recovery modal disappeared before capture.");
                old=RenderTexture.active;RenderTexture.active=target;
                picture=new Texture2D(width,height,TextureFormat.RGB24,false);
                picture.ReadPixels(new Rect(0,0,width,height),0,0);picture.Apply();RenderTexture.active=old;
                var pixels=picture.GetPixels32();int red=0,light=0;
                foreach(var p in pixels){if(p.r>150&&p.g<80&&p.b<100)++red;if(p.r>180&&p.g>180&&p.b>180)++light;}
                Check(red>200&&light>100,"Recovery panel capture is missing its controls or text.");
                File.WriteAllBytes(Path.Combine(root,name+".png"),picture.EncodeToPNG());
            }finally{
                menu.CancelDiagnosticCapture();if(picture!=null)Destroy(picture);target.Release();Destroy(target);
            }
        }
        private IEnumerator RecoverScoredRecord(int cycle,Idas3MultiplayerRecords records,Idas3BattleRecord before,uint balance)
        {
            bool pad=(cycle==0)==(role=="join");
            var result=new ResultRecoveryCheck{cycle=cycle,input=pad?"controller":"keyboard"};scoredRecoveries.Add(result);SaveRecoveryReport();
            int language=Idas3MenuLocalization.Language;var captures=new List<string>();
            scoredRecoveryInput=true;manualKey=KeyCode.None;manualPad=0;
            try{
                Phase("scored-save-recovery-"+cycle);
                yield return Until(()=>menu.ResultRecoveryVisible&&menu.IsOpen,5,"Failed save did not expose the recovery panel.");
                yield return Frames(6);
                result.durableIntent=File.Exists(Path.Combine(root,"userdata","online_result_v1.json"));
                result.historyBlocked=records.Read(session.LocalCar).Equals(before)&&ReadScoredBalance()==balance;
                Check(result.durableIntent&&result.historyBlocked&&!session.CanReturnToLobby,"Blocked history lost its durable reward or allowed Continue.");
                if(cycle==0){
                    var sizes=new[]{new Vector2Int(1024,768),new Vector2Int(1280,720),new Vector2Int(1920,1080),new Vector2Int(2560,1080)};
                    for(int lang=0;lang<3;++lang){
                        Idas3MenuLocalization.SetLanguage(lang);
                        foreach(var size in sizes){
                            string name="result-recovery-"+lang+"-"+size.x+"x"+size.y;
                            yield return CaptureRecoveryPanel(name,size.x,size.y);captures.Add(name+".png");
                        }
                    }
                }
                Idas3MenuLocalization.SetLanguage(language);Screen.SetResolution(1200,720,FullScreenMode.Windowed);yield return Frames(8);
                var focus=(Idas3MenuFocus)typeof(Idas3MultiplayerMenu).GetField("controllerFocus",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(menu);
                Check(focus.Count==2&&focus.Selected!=null&&focus.Selected.EndsWith(":RETRY RESULT",StringComparison.Ordinal),"Recovery controls were not focusable.");
                // One consumed poll represents a tap. Several slow frames can
                // legitimately cross the production hold-repeat threshold.
                RecoveryButton(pad,KeyCode.RightArrow,8);yield return RecoveryFrames("right",1);
                Check(focus.Selected.EndsWith(":EXIT GAME",StringComparison.Ordinal),"Right did not select Exit.");
                RecoveryButton(pad,KeyCode.None,0);yield return RecoveryFrames("right-release",3);
                RecoveryButton(pad,KeyCode.LeftArrow,4);yield return RecoveryFrames("left",1);
                Check(focus.Selected.EndsWith(":RETRY RESULT",StringComparison.Ordinal),"Left did not select Retry.");
                RecoveryButton(pad,KeyCode.None,0);yield return RecoveryFrames("left-release",3);
                int clicks=menu.DiagnosticResultRetryClicks;
                host.DiagnosticFocusOverride=false;RecoveryButton(pad,KeyCode.Return,0x1000);yield return Frames(4);
                host.DiagnosticFocusOverride=true;yield return Frames(4);
                result.focusGuard=menu.DiagnosticResultRetryClicks==clicks&&session.ResultSavePending;
                Check(result.focusGuard,"Held confirmation crossed the focus-loss guard.");
                RecoveryButton(pad,KeyCode.None,0);yield return Frames(5);
                RecoveryButton(pad,KeyCode.Return,0x1000);
                yield return Until(()=>menu.DiagnosticResultRetryClicks==clicks+1,3,"Production input did not activate Retry.");
                yield return Frames(10);
                result.heldGuard=menu.DiagnosticResultRetryClicks==clicks+1&&session.ResultSavePending;
                Check(result.heldGuard,"Held input repeated Retry or escaped a blocked save.");
                RecoveryButton(pad,KeyCode.None,0);yield return Frames(5);
                RecoveryButton(pad,KeyCode.Backspace,0x2000);yield return RecoveryFrames("cancel",3);
                Check(menu.ResultRecoveryVisible&&session.InLobby&&session.IsRacing,"Cancel left an unfinished result.");
                RecoveryButton(pad,KeyCode.None,0);yield return Frames(5);
                // Start the automatic retry's one-second interval with a known
                // failed attempt, then let the actual button perform the retry.
                session.RetryResultSave();Check(session.ResultSavePending,"Private write obstruction stopped working.");
                CloseScoredRecordBlock();RecoveryButton(pad,KeyCode.Return,0x1000);
                yield return Until(()=>menu.DiagnosticResultRetryClicks==clicks+2,0.75,"Retry was not activated before automatic retry.");
                RecoveryButton(pad,KeyCode.None,0);
                yield return Until(()=>!session.ResultSavePending&&session.CanReturnToLobby,3,"Retry did not finish both reward stores.");
                result.clicks=menu.DiagnosticResultRetryClicks-clicks;
                result.retried=!menu.ResultRecoveryVisible&&ReadScoredBalance()==balance&&records.Read(session.LocalCar).battles==before.battles+1;
                Check(result.retried,"Retry duplicated tuning or failed to save history.");result.passed=true;
                yield return ScoredBarrier("scored-recovered-"+cycle,90);
            }finally{
                TraceRecoveryInput("finally");
                host.DiagnosticFocusOverride=true;scoredRecoveryInput=false;manualKey=KeyCode.None;manualPad=0;
                Idas3MenuLocalization.SetLanguage(language);result.captures=captures.ToArray();SaveRecoveryReport();
            }
        }
    }
}
