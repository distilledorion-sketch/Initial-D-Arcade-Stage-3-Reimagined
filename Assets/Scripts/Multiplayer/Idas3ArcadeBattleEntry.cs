using System;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Offline card setup still offers the original Accept / Refuse Challengers prompt.
    internal sealed class Idas3ArcadeBattleEntry
    {
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        static extern int Idas3SceneBattleEntry(int[] data,int count,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string driver,[MarshalAs(UnmanagedType.LPUTF8Str)] string musicHint,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string caption,[MarshalAs(UnmanagedType.LPUTF8Str)] string music);
        const int AcceptChallengersPage = 13;
        const int MotionStart = 55 + 307;
        const int ConfirmationFrames = 31, FadeFrames = 8;
        readonly int[] data = new int[MotionStart + 8];
        readonly Idas3MultiplayerSession session;
        bool confirmHeld,backHeld,startHeld,locked;
        int? directionHeld;
        double began,deadline,confirmedAt;
        internal bool Active {get;private set;}
        internal bool ChoosingChallengers => Active;
        internal bool AcceptSelected {get;private set;}
        internal event Action<bool> ChallengersChosen;
        internal Idas3ArcadeBattleEntry(Idas3MultiplayerSession session){this.session=session;}
        internal void AskAboutChallengers(bool alreadySearching){
            if(Active||session.HandshakeComplete||session.IsRacing)return;
            double now=Time.realtimeSinceStartupAsDouble;
            Array.Clear(data,0,data.Length);
            Active=true;began=now;
            deadline=now+15;AcceptSelected=alreadySearching;locked=false;
            confirmHeld=backHeld=startHeld=true;directionHeld=null;
            PresentChoice(now);
        }
        void PresentChoice(double now){
            data[0]=AcceptChallengersPage;data[2]=Math.Max(0,(int)Math.Ceiling(deadline-now));
            data[3]=(int)((now-began)*60);data[7]=AcceptSelected?16:0;
            int confirmFrame=locked?Math.Max(0,(int)((now-confirmedAt)*60)):-1;
            double fade=locked?(confirmFrame-(ConfirmationFrames-FadeFrames)+1)/(double)FadeFrames:1-(now-began)/.24;
            data[MotionStart]=(int)(255*Math.Clamp(fade,0,1));
            data[MotionStart+1]=confirmFrame;
            Publish();
        }
        internal void Close(){
            if(!Active)return;
            Active=false;data[0]=0;data[MotionStart]=data[MotionStart+6]=0;
            Publish();
        }
        internal void Tick(double now){
            if(!Active)return;
            if(session.ChallengerPending||session.HandshakeComplete||session.IsRacing){Close();return;}
            if(!locked&&now>=deadline){locked=true;confirmedAt=now;}
            if(locked&&now>=confirmedAt+ConfirmationFrames/60d){
                Close();ChallengersChosen?.Invoke(AcceptSelected);
            }else PresentChoice(now);
        }
        internal void Input(int horizontal,int vertical,bool confirm,bool back,bool start,bool blocked,double now){
            if(!Active)return;
            int direction=horizontal!=0?horizontal:vertical;
            bool accept=confirm&&!confirmHeld,previous=back&&!backHeld,pressStart=start&&!startHeld;
            bool move=directionHeld.HasValue&&direction!=0&&direction!=directionHeld;
            directionHeld=direction;confirmHeld=confirm;backHeld=back;startHeld=start;
            if(blocked||locked||session.ChallengerPending)return;
            bool wasAccepted=AcceptSelected;
            if(previous)AcceptSelected=false;
            if(accept||previous){locked=true;confirmedAt=now;}
            else if(pressStart)AcceptSelected=!AcceptSelected;
            else if(move)AcceptSelected=direction>0;
            if(locked||wasAccepted!=AcceptSelected)PresentChoice(now);
        }
        void Publish(){
            if(Idas3SceneBattleEntry(data,data.Length,"","","","")!=1)throw new InvalidOperationException(Idas3Native.Error());
        }
    }
}
