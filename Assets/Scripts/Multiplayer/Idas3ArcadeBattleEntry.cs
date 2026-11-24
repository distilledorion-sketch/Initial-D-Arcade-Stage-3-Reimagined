using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // The course selector and waiting cabinet can each be the network host or guest.
    internal sealed class Idas3ArcadeBattleEntry
    {
        internal enum Page { Closed=0, Course=6, Route, Surface, Time, Waiting, Selected, Loading, AcceptChallengers }
        [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
        static extern int Idas3SceneBattleEntry(int[] data,int count,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string driver,[MarshalAs(UnmanagedType.LPUTF8Str)] string musicHint,
            [MarshalAs(UnmanagedType.LPUTF8Str)] string caption,[MarshalAs(UnmanagedType.LPUTF8Str)] string music);
        // Negative menu token only. Network choices, times and replays always use a real course.
        const int ModsTab=-1;
        static readonly int[] ArcadeCourseOrder={0,1,2,3,5,8,4,6,7};
        enum ModFocus { Closed, Tracks, Direction, Surface, Time, Start }
        readonly List<int> tiles=new List<int>(),mods=new List<int>();
        ModFocus modFocus;
        int modMask,lastMod;
        internal bool ModsOpen=>Current==Page.Course&&modFocus!=ModFocus.Closed;
        internal static readonly string[] Music={"SPEEDY SPEED BOY","REMEMBER ME","SAVE ME","OVER THE RAINBOW",
            "STOP YOUR SELF CONTROL","CRAZY FOR LOVE","EXPRESS LOVE","BLACK OUT","FALL IN THE WEB OF DESIRE","PAMELA","FIGHT FOR LOVE TONIGHT"};
        readonly Idas3MultiplayerSession session;
        const int CardProfileStart=55;
        const int MotionStart=CardProfileStart+307;
        readonly int[] data=new int[MotionStart+8];
        readonly Func<string,bool> playSong;
        readonly Func<string> currentSong;
        string songTitle;
        internal Page Current {get;private set;}
        internal bool Active=>Current!=Page.Closed;
        internal bool ChoosingChallengers=>Current==Page.AcceptChallengers;
        internal bool AcceptSelected {get;private set;}
        internal event Action<bool> ChallengersChosen;
        internal bool CanSelectMusic=>!locked&&next==Page.Closed&&(Current==Page.Waiting||(Current>=Page.Course&&Current<=Page.Time));
        int infoPage,music,directionHeld;
        double began,deadline,repeatAt,musicOpenedAt=-1;
        Page next=Page.Closed;
        double confirmedAt,fadeOutAt,switchAt,fadeInSeconds;
        int courseSlot=2,scrollDirection;
        double scrollAt;
        internal bool ChangingPage=>next!=Page.Closed;
        internal int FadeAlpha(double now){
            if(next!=Page.Closed){
                if(double.IsPositiveInfinity(fadeOutAt))return 0;
                return (int)(255*Math.Clamp((now-fadeOutAt)/.24,0,1));
            }
            return fadeInSeconds<=0?0:(int)(255*Math.Clamp(1-(now-began)/fadeInSeconds,0,1));
        }
        void Transition(Page target,double now){
            if(ChangingPage||target==Current)return;
            next=target;confirmedAt=now;
            bool conditions=Current>=Page.Course&&Current<=Page.Time&&target>=Page.Course&&target<=Page.Time;
            if(conditions){
                // Keep the course painting on screen while the selected widget flashes.
                fadeOutAt=double.PositiveInfinity;switchAt=now+31/60.0;
                return;
            }
            double black=target==Page.Selected?1.0:target==Page.Loading?.10:.12;
            fadeOutAt=now;switchAt=now+.24+black;
        }
        void ReadyAndStart(){
            if(session.CanReady&&!session.LocalReady)session.SetReady(true);
            if(session.CanStart)session.StartRace();
        }
        bool confirmHeld,backHeld,startHeld,locked;
        public Idas3ArcadeBattleEntry(Idas3MultiplayerSession room,Func<string,bool> song,Func<string> selected){session=room;playSong=song;currentSong=selected;}
        static int Wrap(int i,int count)=>(i%count+count)%count;
        static double Now=>Time.realtimeSinceStartupAsDouble;
        void Enter(Page p,double now,bool fade=true){
            next=Page.Closed;Current=p;began=now;fadeInSeconds=fade?.24:0;
            if(p==Page.Course){courseSlot=2;scrollDirection=0;}
            modFocus=ModFocus.Closed;
            int seconds=p==Page.Course?20:10;
            deadline=now+seconds;
        }
        internal void Begin(){
            if(ChoosingChallengers&&session.HandshakeComplete)Close();
            if(Active||!session.HandshakeComplete||session.IsRacing)return;
            if(session.Garage.Count==0)session.RefreshGarage();
            tiles.Clear();mods.Clear();modMask=0;
            foreach(int id in ArcadeCourseOrder)if(session.CourseAvailable(id))tiles.Add(id);
            for(int id=9;id<Idas3CourseCatalog.Count;++id){
                if(!session.CourseAvailable(id))continue;
                mods.Add(id);modMask|=1<<id;
            }
            if(mods.Count>0){
                tiles.Add(ModsTab);
                lastMod=mods.Contains(session.LocalChoice.Course)?session.LocalChoice.Course:mods[0];
            }
            infoPage=0;locked=false;
            directionHeld=0;confirmHeld=backHeld=startHeld=true;musicOpenedAt=-1;
            RefreshSong();
            if(session.LocalPicksCourse&&!session.IsRematch){
                session.SetRaceOptions(0,false,false,false);
                if(songTitle==null)ChooseMusic(0);
            }
            Enter(session.LocalPicksCourse?Page.Course:Page.Waiting,Now);Present(Now);
        }
        internal void AskAboutChallengers(bool alreadySearching){
            if(Active||session.HandshakeComplete||session.IsRacing)return;
            double now=Now;
            Array.Clear(data,0,data.Length);
            Enter(Page.AcceptChallengers,now);
            deadline=now+15;AcceptSelected=alreadySearching;locked=false;
            confirmHeld=backHeld=startHeld=true;directionHeld=0;
            PresentChoice(now);
        }
        void PresentChoice(double now){
            data[0]=(int)Current;data[2]=Math.Max(0,(int)Math.Ceiling(deadline-now));
            data[3]=(int)((now-began)*60);data[7]=AcceptSelected?16:0;
            data[MotionStart]=locked?(int)(255*Math.Clamp((now-confirmedAt)/.24,0,1)):FadeAlpha(now);
            data[MotionStart+1]=locked?(int)((now-confirmedAt)*60):-1;
            Publish("","","","");
        }
        internal void Close(){
            if(!Active)return;
            Current=next=Page.Closed;data[0]=0;data[MotionStart]=data[MotionStart+6]=0;
            Publish("","","","");
        }
        internal void Tick(double now){
            if(!Active)return;
            if(ChoosingChallengers){
                if(session.ChallengerPending||session.HandshakeComplete||session.IsRacing){Close();return;}
                if(!locked&&now>=deadline){locked=true;confirmedAt=now;}
                if(locked&&now>=confirmedAt+.24){
                    Close();ChallengersChosen?.Invoke(AcceptSelected);
                }else PresentChoice(now);
                return;
            }
            if(!session.HandshakeComplete||!session.InLobby||session.IsRacing){Close();return;}
            if(session.ShowingLoadArtwork&&Current==Page.Selected)Transition(Page.Loading,now);
            else if(session.ShowingCourseSelected&&Current!=Page.Selected)Transition(Page.Selected,now);
            if(ChangingPage){
                if(now>=switchAt){
                    if(next==Page.Selected&&!session.HasCourseDraw)ReadyAndStart();
                    if(next!=Page.Selected||session.HasCourseDraw)Enter(next,now,!double.IsPositiveInfinity(fadeOutAt));
                }
            }else if(Current==Page.Waiting)ReadyAndStart();
            else if(session.LocalPicksCourse&&!locked&&Current!=Page.Selected&&Current!=Page.Loading&&musicOpenedAt<0&&now>=deadline){if(ModsOpen)Lock(now);else Confirm(now);}
            Present(now);
        }
        internal void Input(int horizontal,int vertical,bool confirm,bool back,bool start,bool brake,bool blocked,double now){
            if(!Active)return;
            int direction=horizontal!=0?horizontal:vertical;
            bool accept=confirm&&!confirmHeld,previous=back&&!backHeld,pressStart=start&&!startHeld;
            bool move=direction!=0&&(direction!=directionHeld||now>=repeatAt);
            if(move)repeatAt=now+(direction==directionHeld?.17:.4);
            directionHeld=direction;confirmHeld=confirm;backHeld=back;startHeld=start;
            if(blocked||ChangingPage)return;
            if(ChoosingChallengers){
                if(locked)return;
                if(move)AcceptSelected=direction>0;
                if(previous)AcceptSelected=false;
                if(accept||previous){locked=true;confirmedAt=now;}
                else if(pressStart)AcceptSelected=!AcceptSelected;
                return;
            }
            session.SampleBoostBrake(brake);
            if(Current==Page.Selected||Current==Page.Loading||locked)return;
            if(previous){Back(now);return;}
            if(Current==Page.Waiting)return;
            if(move){
                if(ModsOpen){MoveMod(direction,horizontal==0,now);}
                else Move(direction);
            }
            if(accept||pressStart)Confirm(now);
        }
        void ChooseMusic(int index){
            if(playSong(Music[index])){music=index;songTitle=Music[index];}
        }
        void RefreshSong(){
            songTitle=currentSong();
            music=Array.FindIndex(Music,title=>string.Equals(title,songTitle,StringComparison.OrdinalIgnoreCase));
        }
        internal void TapView(){
            if(!CanSelectMusic)return;
            if(Current==Page.Waiting)infoPage=(infoPage+1)%3;
            else ChooseMusic((music+1)%Music.Length);
        }
        internal void MusicVisibilityChanged(bool open){
            if(open){
                if(CanSelectMusic&&session.LocalPicksCourse)musicOpenedAt=Now;
                return;
            }
            if(musicOpenedAt>=0){deadline+=Now-musicOpenedAt;musicOpenedAt=-1;}
            if(Active)RefreshSong();
        }
        void Move(int sign){
            var c=session.LocalChoice;
            switch(Current){
                case Page.Course:
                    int current=c.Course>=9?ModsTab:c.Course;
                    int tile=tiles[Wrap(tiles.IndexOf(current)+sign,tiles.Count)];
                    int slot=Math.Clamp(courseSlot+sign,0,4);
                    scrollDirection=slot==courseSlot?sign:0;scrollAt=Now;courseSlot=slot;
                    SelectCourse(tile==ModsTab?lastMod:tile);
                    break;
                case Page.Route: session.SetRaceOptions(c.Course,!c.Reverse,c.Wet,c.Night);break;
                case Page.Surface: session.SetRaceOptions(c.Course,c.Reverse,!c.Wet,c.Night);break;
                case Page.Time: session.SetRaceOptions(c.Course,c.Reverse,c.Wet,!c.Night);break;
            }
        }
        void SelectCourse(int id){
            if(id>=9)lastMod=id;
            session.SetRaceOptions(id,false,id==8,Idas3CourseCatalog.RequiresNight(id));
        }
        void StepModFocus(int direction){
            int value=(int)modFocus+direction;
            if(value==(int)ModFocus.Time&&Idas3CourseCatalog.RequiresNight(session.LocalChoice.Course))value+=direction;
            modFocus=(ModFocus)Math.Clamp(value,(int)ModFocus.Tracks,(int)ModFocus.Start);
        }
        void MoveMod(int direction,bool vertical,double now){
            deadline=now+20;
            var c=session.LocalChoice;
            if(modFocus==ModFocus.Tracks){
                SelectCourse(mods[Wrap(mods.IndexOf(c.Course)+direction,mods.Count)]);
            }else if(vertical)StepModFocus(direction);
            else if(modFocus==ModFocus.Direction)session.SetRaceOptions(c.Course,!c.Reverse,c.Wet,c.Night);
            else if(modFocus==ModFocus.Surface)session.SetRaceOptions(c.Course,c.Reverse,!c.Wet,c.Night);
            else if(modFocus==ModFocus.Time)session.SetRaceOptions(c.Course,c.Reverse,c.Wet,!c.Night);
        }
        void Confirm(double now){
            var c=session.LocalChoice;
            switch(Current){
                case Page.Course:
                    if(c.Course<9){Transition(Page.Route,now);break;}
                    deadline=now+20;
                    if(!ModsOpen)modFocus=ModFocus.Tracks;
                    else if(modFocus==ModFocus.Start)Lock(now);
                    else StepModFocus(1);
                    break;
                case Page.Route: if(c.Course==8)Lock(now);else Transition(Page.Surface,now);break;
                case Page.Surface: if(Idas3CourseCatalog.RequiresNight(c.Course))Lock(now);else Transition(Page.Time,now);break;
                case Page.Time: Lock(now);break;
            }
        }
        void Lock(double now){locked=true;Transition(Page.Selected,now);}
        void Back(double now){
            switch(Current){
                case Page.Course:
                    if(ModsOpen){modFocus=ModFocus.Closed;deadline=now+20;return;}
                    goto case Page.Waiting;
                case Page.Waiting: session.LeaveRoom();Close();return;
                case Page.Route: Transition(Page.Course,now);break;
                case Page.Surface: Transition(Page.Route,now);break;
                case Page.Time: Transition(Page.Surface,now);break;
            }
        }
        void Present(double now){
            bool wait=Current==Page.Waiting;
            var car=wait?session.RemoteSavedCar:session.LocalSavedCar;
            var rec=wait?session.RemoteRecord:default;
            var c=session.HasCourseDraw?session.SelectedChoice:session.LocalChoice;
            data[0]=(int)Current;data[1]=wait?infoPage:music;
            data[2]=Math.Max(0,(int)Math.Ceiling(deadline-now));data[3]=(int)((now-began)*60);
            data[4]=0;data[5]=car?.Car??0;data[6]=c.Course;
            data[7]=(c.Reverse?1:0)|(c.Wet?2:0)|(c.Night?4:0)|(car?.Automatic==true?8:0)|((music+1)<<8);
            data[8]=(int)(car?.Words[18]??0);data[9]=(int)rec.wins;data[10]=(int)rec.battles;data[11]=(int)rec.streak;data[12]=(int)rec.level;
            data[MotionStart]=FadeAlpha(now);
            data[MotionStart+1]=ChangingPage?(int)((now-confirmedAt)*60):-1;
            data[MotionStart+2]=courseSlot;
            data[MotionStart+3]=scrollDirection*Math.Max(0,5-(int)((now-scrollAt)*60));
            if(Current==Page.Course){
                int selected=tiles.IndexOf(c.Course>=9?ModsTab:c.Course);
                int Tile(int offset){
                    int value=tiles[Wrap(selected+offset-courseSlot,tiles.Count)];
                    return value==ModsTab?lastMod:value;
                }
                for(int i=0;i<5;++i)data[49+i]=Tile(i);
                data[MotionStart+4]=Tile(-1);data[MotionStart+5]=Tile(5);
            }
            data[MotionStart+6]=ModsOpen?(int)modFocus:0;
            data[MotionStart+7]=modMask;
            data[54]=(int)(car?.Words[16]??0);
            if(wait&&car!=null)Buffer.BlockCopy(car.Words,176,data,13*4,36*4);
            if(wait&&car!=null)Buffer.BlockCopy(car.Words,0,data,CardProfileStart*sizeof(int),car.Words.Length*sizeof(uint));
            string caption=Current==Page.Selected?Idas3CourseCatalog.DirectionLabel(c.Course,c.Reverse):"";
            Publish(wait?session.OpponentName:"","",caption,songTitle??"GAME DEFAULT");
        }
        void Publish(string driver,string musicHint,string caption,string song){
            if(Idas3SceneBattleEntry(data,data.Length,driver,musicHint,caption,song)!=1)throw new InvalidOperationException(Idas3Native.Error());
        }
    }
}
