using System;
using UnityEngine;

namespace Idas3.Multiplayer
{
    // Input is intercepted before the native game host's Update. This overlay
    // never pauses Unity or the session; closing it also blocks that frame's
    // Escape/F1 from reaching the original menus or driving controls.
    [DefaultExecutionOrder(-200)]
    public sealed class Idas3MultiplayerMenu : MonoBehaviour
    {
        private const float Width=900,Height=620;
        private static readonly Color Ink=new Color32(11,12,15,255);
        private static readonly Color Panel=new Color32(22,24,29,255);
        private static readonly Color Raised=new Color32(34,36,42,255);
        private static readonly Color Edge=new Color32(65,68,76,255);
        private static readonly Color Muted=new Color32(164,166,175,255);
        private static readonly Color Yellow=new Color32(255,216,49,255);
        private static readonly Color Cyan=new Color32(225,226,230,255);
        private static readonly Color Green=new Color32(108,225,156,255);
        private static readonly Color Red=new Color32(222,35,49,255);
        private static readonly string[] Courses={"MYOGI","USUI","AKAGI","AKINA","HAPPOGAHARA","IROHAZAKA","SHOMARU","TSUCHISAKA","AKINA SNOW","HAKONE","SADAMINE","ENNA SKYLINE"};
        private Idas3MultiplayerSession session;
        private GUIStyle title,heading,label,small,button,field,number,wrapped,tightButton;
        private string joinCode="";
        private Vector2 roomScroll;
        private bool joinEntry;
        private string lastRoomFocus;
        private float scrollDragY,scrollDragStart;
        private bool scrollDragging;
        private readonly Idas3MenuFocus controllerFocus=new Idas3MenuFocus();
        private bool codeEditing;
        private bool navigationHeld;
        private bool steeringOnly;
        private string codeDraft="";
        internal int ControllerActionCount=>controllerFocus.Count;
        internal string ControllerSelection=>controllerFocus.Selected;
        private int blockThroughFrame=-1;
        private bool previousCursorVisible;
        private bool suppressClosingControls;
        private bool previousNativeRace;
        internal bool ManagedControlInput { get; set; }
        private bool previousOnlineHeld, previousCancelHeld;
        private bool disconnectedInputArmed,disconnectedPreviousConfirm,disconnectedPreviousCancel;
        private bool disconnectedInputBlocked=true,disconnectedEntered;
        private int disconnectedOpenedFrame=-1;
        private bool resultsEntered,resultsInputArmed,resultsInputBlocked=true,resultsPreviousConfirm,resultsPreviousCancel;
        private int resultsOpenedFrame=-1;
        internal bool InputCovered { get; set; }
        internal bool RaceHudActive { get; set; }
        internal bool MusicSelectionAllowed { get; set; }
        internal string SelectedMusicTitle { get; set; } = "GAME DEFAULT";
        internal string MusicControlHint { get; set; } = "HOLD VIEW CHANGE TO SELECT MUSIC";
        internal event Action MusicSelectionRequested;
        private CursorLockMode previousCursorLock;
        private RenderTexture diagnosticTarget;
        internal bool DiagnosticCaptureReady { get; private set; }
        internal int DiagnosticRepaints { get; private set; }
        internal void RequestDiagnosticCapture(RenderTexture target)
        {
            if(!IsOpen||target==null)throw new InvalidOperationException("Open menu and a target are required for a diagnostic Repaint.");
            diagnosticTarget=target;DiagnosticCaptureReady=false;
        }
        internal void CancelDiagnosticCapture(){diagnosticTarget=null;}

        public bool IsOpen { get; private set; }
        public bool BlocksGameInput => IsOpen||suppressClosingControls||Time.frameCount<=blockThroughFrame;
        public bool DisconnectedFinishVisible=>session!=null&&session.DisconnectedFinish;
        public bool CanAcknowledgeDisconnectedFinish=>DisconnectedFinishVisible&&disconnectedInputArmed&&!disconnectedInputBlocked;
        public bool ResultsVisible=>session!=null&&!session.DisconnectedFinish&&session.IsRacing&&session.StateName=="Results";
        public bool CanAcknowledgeReturnToLobby=>ResultsVisible&&session.CanReturnToLobby&&resultsInputArmed&&!resultsInputBlocked;

        public void Initialize(Idas3MultiplayerSession owner)
        {
            if(session!=null){session.RaceDisconnected-=OnRaceDisconnected;session.ReturnedToLobby-=OnReturnedToLobby;}
            session=owner??throw new ArgumentNullException(nameof(owner));
            previousNativeRace=session.IsRacing;
            session.RaceDisconnected+=OnRaceDisconnected;
            session.ReturnedToLobby+=OnReturnedToLobby;
            if(session.DisconnectedFinish)OnRaceDisconnected();
        }
        public void Toggle(){SetOpen(!IsOpen);}
        public void SetOpen(bool open)
        {
            // A terminal disconnect can only be dismissed by leaving the room.
            if(DisconnectedFinishVisible||ResultsVisible||session!=null&&session.StateName=="Returning")open=true;
            blockThroughFrame=Time.frameCount;
            if(open==IsOpen)return;
            IsOpen=open;
            controllerFocus.Reset();codeEditing=false;scrollDragging=false;
            if(open){
                if(!DisconnectedFinishVisible)session?.OpenMenu();
                previousCursorVisible=Cursor.visible;previousCursorLock=Cursor.lockState;
                Cursor.lockState=CursorLockMode.None;Cursor.visible=true;
            }else{
                suppressClosingControls=true;blockThroughFrame=Time.frameCount+1;
                Cursor.lockState=previousCursorLock;Cursor.visible=previousCursorVisible;
            }
        }
        private void Update()
        {
            if(session==null)return;
            session.ActivityRequested=IsOpen&&!DisconnectedFinishVisible;
            SynchronizeRaceVisibility();
            if(ManagedControlInput)return;
            if(DisconnectedFinishVisible){
                ProcessDisconnectedInput(Input.GetKey(KeyCode.Return),Input.GetKey(KeyCode.Escape)||Input.GetKey(KeyCode.Backspace),!Application.isFocused);return;
            }
            if(ResultsVisible){
                ProcessResultsInput(Input.GetKey(KeyCode.Return),Input.GetKey(KeyCode.Escape)||Input.GetKey(KeyCode.Backspace),!Application.isFocused);return;
            }
            // Focus loss releases all game input. Do not retain an old
            // mouse/key latch from closing this menu across that release.
            if(!Application.isFocused){suppressClosingControls=false;return;}
            if(suppressClosingControls&&!Input.GetKey(KeyCode.F1)&&!Input.GetKey(KeyCode.Escape)&&!Input.GetMouseButton(0))suppressClosingControls=false;
            if(Input.GetKeyDown(KeyCode.F1))Toggle();
            else if(IsOpen&&Input.GetKeyDown(KeyCode.Escape)){if(codeEditing){codeEditing=false;controllerFocus.Reset();}else if(joinEntry){joinEntry=false;controllerFocus.Reset();}else SetOpen(false);}
        }
        internal void ProcessControlInput(bool onlineHeld,bool cancelHeld,bool blocked)
        {
            if(!ManagedControlInput)return;
            SynchronizeRaceVisibility();
            bool onlinePressed=onlineHeld&&!previousOnlineHeld;
            bool cancelPressed=cancelHeld&&!previousCancelHeld;
            previousOnlineHeld=onlineHeld;previousCancelHeld=cancelHeld;
            if(DisconnectedFinishVisible||ResultsVisible||session!=null&&session.StateName=="Returning")return;
            // Managed input has one focus owner: the host supplies `blocked`
            // from the same focus state used to sample controls and drive.
            if(suppressClosingControls&&!onlineHeld&&!cancelHeld&&!navigationHeld&&!Input.GetMouseButton(0))suppressClosingControls=false;
            if(blocked)return;
            if(onlinePressed)Toggle();
            else if(IsOpen&&cancelPressed){if(codeEditing){codeEditing=false;controllerFocus.Reset();}else if(joinEntry){joinEntry=false;controllerFocus.Reset();}else SetOpen(false);}
        }
        internal void ProcessMenuNavigation(int horizontal,int vertical,bool confirm,bool back,bool blocked,double now,bool wheel=false){
            steeringOnly=wheel;
            if(wheel&&horizontal!=0){vertical=horizontal;horizontal=0;}
            navigationHeld=confirm||back;
            if(!IsOpen)return;
            if(controllerFocus.Poll(horizontal,vertical,confirm,back,blocked||InputCovered||Idas3MenuPointer.Active,now)){
                if(codeEditing){codeEditing=false;controllerFocus.Reset();}else if(joinEntry){joinEntry=false;controllerFocus.Reset();}else SetOpen(false);
            }
        }
        private void SynchronizeRaceVisibility()
        {
            bool racing=session!=null&&session.IsRacing;
            bool starting=racing&&!previousNativeRace;
            previousNativeRace=racing;
            // Close once when native loading succeeds, before the showcase.
            // A later F1 press may reopen this same race's overlay normally.
            if(starting&&!DisconnectedFinishVisible&&!ResultsVisible&&session.StateName!="Returning")SetOpen(false);
        }
        private void OnDestroy()
        {
            if(session!=null){session.RaceDisconnected-=OnRaceDisconnected;session.ReturnedToLobby-=OnReturnedToLobby;}
            if(IsOpen){Cursor.lockState=previousCursorLock;Cursor.visible=previousCursorVisible;}
        }
        private void OnReturnedToLobby()
        {
            if(session==null||session.DisconnectedFinish)return;
            previousNativeRace=false;
            disconnectedEntered=disconnectedInputArmed=false;
            disconnectedInputBlocked=true;resultsEntered=resultsInputArmed=false;resultsInputBlocked=true;
            SetOpen(true);
        }
        public void RequestReturnToLobby()
        {
            if(!CanAcknowledgeReturnToLobby)return;
            resultsInputArmed=false;resultsInputBlocked=true;
            session.ReturnToLobby();
        }
        public void ProcessResultsInput(bool confirmHeld,bool cancelHeld,bool blocked=false)
        {
            if(!ResultsVisible){resultsEntered=resultsInputArmed=false;return;}
            if(!resultsEntered){
                resultsEntered=true;resultsInputArmed=false;resultsPreviousConfirm=resultsPreviousCancel=true;
                resultsOpenedFrame=Time.frameCount;SetOpen(true);
            }
            resultsInputBlocked=blocked;
            bool otherHeld=previousOnlineHeld||Input.GetKey(KeyCode.F1)||Input.GetMouseButton(0);
            if(blocked||Time.frameCount<=resultsOpenedFrame){
                resultsInputArmed=false;resultsPreviousConfirm=confirmHeld;resultsPreviousCancel=cancelHeld;return;
            }
            if(!resultsInputArmed){
                if(!confirmHeld&&!cancelHeld&&!otherHeld){resultsInputArmed=true;resultsPreviousConfirm=resultsPreviousCancel=false;}
                return;
            }
            bool pressed=confirmHeld&&!resultsPreviousConfirm||cancelHeld&&!resultsPreviousCancel;
            resultsPreviousConfirm=confirmHeld;resultsPreviousCancel=cancelHeld;
            if(pressed&&!otherHeld)RequestReturnToLobby();
        }
        private void OnRaceDisconnected()
        {
            if(!DisconnectedFinishVisible||disconnectedEntered)return;
            disconnectedEntered=true;disconnectedInputArmed=false;disconnectedInputBlocked=true;
            disconnectedPreviousConfirm=disconnectedPreviousCancel=true;disconnectedOpenedFrame=Time.frameCount;
            SetOpen(true);
        }
        public void ProcessDisconnectedInput(bool confirmHeld,bool cancelHeld,bool blocked=false)
        {
            if(!DisconnectedFinishVisible){disconnectedEntered=false;disconnectedInputArmed=false;return;}
            if(!disconnectedEntered)OnRaceDisconnected();
            disconnectedInputBlocked=blocked;
            bool otherHeld=previousOnlineHeld||Input.GetKey(KeyCode.F1)||Input.GetMouseButton(0);
            if(blocked||Time.frameCount<=disconnectedOpenedFrame){
                disconnectedInputArmed=false;disconnectedPreviousConfirm=confirmHeld;disconnectedPreviousCancel=cancelHeld;return;
            }
            if(!disconnectedInputArmed){
                if(!confirmHeld&&!cancelHeld&&!otherHeld){disconnectedInputArmed=true;disconnectedPreviousConfirm=disconnectedPreviousCancel=false;}
                return;
            }
            bool pressed=confirmHeld&&!disconnectedPreviousConfirm||cancelHeld&&!disconnectedPreviousCancel;
            disconnectedPreviousConfirm=confirmHeld;disconnectedPreviousCancel=cancelHeld;
            if(pressed&&!otherHeld)AcknowledgeDisconnectedFinish();
        }
        public void AcknowledgeDisconnectedFinish()
        {
            if(!CanAcknowledgeDisconnectedFinish)return;
            // Invalidate before the callback so a repeated click/input cannot
            // emit another LeaveRoom or reopen the transport browser.
            disconnectedInputArmed=false;disconnectedInputBlocked=true;
            session.LeaveRoom();
            if(!session.DisconnectedFinish){disconnectedEntered=false;SetOpen(false);}
        }
        private void Styles(){
            if(label!=null)return;
            label=new GUIStyle(GUI.skin.label){fontSize=16,clipping=TextClipping.Clip,padding=new RectOffset(0,0,0,0)};label.normal.textColor=Color.white;
            small=new GUIStyle(label){fontSize=12};heading=new GUIStyle(label){fontSize=20,fontStyle=FontStyle.Bold};
            title=new GUIStyle(heading){fontSize=34,fontStyle=FontStyle.BoldAndItalic};number=new GUIStyle(title){fontSize=28};
            button=new GUIStyle(label){fontSize=13,fontStyle=FontStyle.Bold,alignment=TextAnchor.MiddleCenter,wordWrap=false};
            tightButton=new GUIStyle(button){fontSize=11};
            field=new GUIStyle(GUI.skin.textField){fontSize=18,padding=new RectOffset(12,12,10,10),alignment=TextAnchor.MiddleLeft};
            field.normal.textColor=field.focused.textColor=Color.white;field.normal.background=field.focused.background=Texture2D.whiteTexture;
            wrapped=new GUIStyle(small){wordWrap=true,clipping=TextClipping.Clip};
        }
        private static void Fill(Rect r,Color c){var old=GUI.color;GUI.color=c;GUI.DrawTexture(r,Texture2D.whiteTexture);GUI.color=old;}
        private static void Frame(Rect r,Color c){Fill(new Rect(r.x,r.y,r.width,1),c);Fill(new Rect(r.x,r.yMax-1,r.width,1),c);Fill(new Rect(r.x,r.y,1,r.height),c);Fill(new Rect(r.xMax-1,r.y,1,r.height),c);}
        private static string Safe(string text,string fallback="")=>string.IsNullOrWhiteSpace(text)?fallback:text;
        private static string Track(int course)=>course>=0&&course<Courses.Length?Courses[course]:"SELECT COURSE";
        private static string Direction(int course,bool reverse)=>course>=9?(reverse?"UPHILL":"DOWNHILL"):(reverse?"REVERSE":"FORWARD");
        private static string Conditions(int course,bool reverse,bool wet,bool night)=>Direction(course,reverse)+" / "+(course==8?"SNOW":wet?"WET":"DRY")+" / "+(night?"NIGHT":"DAY");
        private static string PickSummary(Idas3RaceChoice c)=>Track(c.Course)+" / "+Conditions(c.Course,c.Reverse,c.Wet,c.Night);
        private void Text(Rect r,string text,GUIStyle style,Color? color=null){var old=GUI.contentColor;GUI.contentColor=color??(style==small||style==wrapped?Muted:Color.white);GUI.Label(r,text??"",style);GUI.contentColor=old;}
        private bool ActionButton(Rect r,string text,bool enabled=true,bool primary=false,string identity=null){
            string id=identity??r.x+":"+r.y+":"+text;
            if(enabled&&GUI.enabled&&Event.current.type==EventType.MouseDown&&r.Contains(Event.current.mousePosition))controllerFocus.Pointer(id);
            bool controllerClick=IsOpen&&controllerFocus.Control(id,r,enabled&&GUI.enabled,Event.current.type==EventType.Repaint);
            bool hover=enabled&&r.Contains(Event.current.mousePosition);
            Fill(r,enabled&&primary?Red:hover?Raised:Panel);Frame(r,enabled&&primary?Red:Edge);
            Text(r,text,button.CalcSize(new GUIContent(text)).x>r.width-6?tightButton:button,enabled?Color.white:Muted*.6f);
            if(IsOpen&&enabled&&controllerFocus.Focused(id))Frame(new Rect(r.x-2,r.y-2,r.width+4,r.height+4),Color.white);
            bool old=GUI.enabled;GUI.enabled=old&&enabled;bool click=GUI.Button(r,GUIContent.none,GUIStyle.none);GUI.enabled=old;return click||controllerClick;
        }
        private void Section(Rect r,string text){Fill(r,Panel);Fill(new Rect(r.x,r.y,4,43),Red);Text(new Rect(r.x+18,r.y+12,r.width-36,28),text,heading);}
        private void OnGUI(){
            if(session==null||session.ChallengerPending)return;
            SynchronizeRaceVisibility();Styles();GUI.depth=DisconnectedFinishVisible?-13000:-10000;
            if(IsOpen&&!InputCovered&&(Event.current.type==EventType.MouseDown||Event.current.type==EventType.MouseDrag||Event.current.type==EventType.MouseUp))controllerFocus.Pointer();
            bool enabled=GUI.enabled;GUI.enabled=enabled&&(DisconnectedFinishVisible||!InputCovered);var matrix=GUI.matrix;
            bool diagnostic=IsOpen&&diagnosticTarget!=null&&Event.current.type==EventType.Repaint;var target=RenderTexture.active;
            if(IsOpen&&Event.current.type==EventType.Repaint)++DiagnosticRepaints;
            if(diagnostic){RenderTexture.active=diagnosticTarget;GL.PushMatrix();GL.LoadPixelMatrix(0,Screen.width,Screen.height,0);}
            try{
                if(IsOpen){controllerFocus.SpatialVertical=codeEditing&&!steeringOnly;controllerFocus.Begin();}
                if(DisconnectedFinishVisible){if(Event.current.isKey)Event.current.Use();DisconnectedFinishView();return;}
                if(!IsOpen){
                    if(RaceHudActive||session.IsRacing||!session.InLobby&&!session.IsQuickMatching)return;
                    float scale=Mathf.Clamp(Screen.height/900f,.75f,1.25f);
                    GUI.matrix=Matrix4x4.TRS(new Vector3(Screen.width-278*scale,18*scale,0),Quaternion.identity,new Vector3(scale,scale,1));
                    string state=session.IsQuickMatching?" / SEARCHING":session.InLobby?(session.PingMilliseconds>=0?" / "+session.PingMilliseconds+" ms":" / CONNECTED"):"";
                    if(ActionButton(new Rect(0,0,258,38),"F1  ONLINE BATTLE"+state))SetOpen(true);
                    if(session.HasCourseDraw){Fill(new Rect(0,38,258,29),Panel);Text(new Rect(10,43,238,22),"RACE COURSE / "+Track(session.Course),small);}
                    return;
                }
                Fill(new Rect(0,0,Screen.width,Screen.height),new Color(0,0,0,.82f));
                float factor=Mathf.Min(1.5f,Mathf.Min(Screen.width/(Width+40),Screen.height/(Height+32)));
                GUI.matrix=Matrix4x4.TRS(new Vector3((Screen.width-Width*factor)*.5f,(Screen.height-Height*factor)*.5f,0),Quaternion.identity,new Vector3(factor,factor,1));
                Fill(new Rect(0,0,Width,Height),Ink);Frame(new Rect(0,0,Width,Height),Edge);Fill(new Rect(0,0,Width,4),Red);Fill(new Rect(0,98,Width,1),Edge);
                string screen=codeEditing||joinEntry?"JOIN A BATTLE":session.StateName=="Returning"||ResultsVisible?"RACE RESULTS":session.IsRacing?"RACE MENU":session.IsQuickMatching?"QUICK MATCH":session.InLobby?session.LobbyName:"ONLINE BATTLE";
                Text(new Rect(26,18,560,43),screen,title);Text(new Rect(28,69,560,20),"INITIAL D / ONLINE BATTLE",small);HeaderStatus();
                if(!codeEditing&&!ResultsVisible&&session.StateName!="Returning"&&ActionButton(new Rect(841,26,32,32),"×")){if(joinEntry){joinEntry=false;controllerFocus.Reset();}else SetOpen(false);}
                if(codeEditing)CodeEntryView();else{
                    if(joinEntry&&!session.InLobby)JoinView();else if(session.IsQuickMatching)SearchView();else if(session.InLobby)RoomView();else BrowserView();
                    StatusLine();Footer();
                }
                if(Event.current.type==EventType.KeyDown&&Event.current.keyCode==KeyCode.Return&&GUI.GetNameOfFocusedControl()=="idas3-room-code"&&session.Available&&!session.Busy&&!string.IsNullOrWhiteSpace(joinCode)){session.JoinRoom(joinCode.Trim());joinEntry=false;Event.current.Use();}
                if(Event.current.type==EventType.KeyDown||Event.current.type==EventType.KeyUp)Event.current.Use();
            }finally{if(IsOpen)controllerFocus.End();GUI.enabled=enabled;GUI.matrix=matrix;if(diagnostic){GL.PopMatrix();RenderTexture.active=target;diagnosticTarget=null;DiagnosticCaptureReady=true;}}
        }
        private void HeaderStatus(){
            string status=session.StateName=="Returning"?"RETURNING TO LOBBY":ResultsVisible?"RACE FINISHED":session.IsRacing?"LIVE RACE CONTINUES":session.IsQuickMatching?"FINDING A DRIVER":session.InLobby?"ROOM CONNECTED":session.Available?"LINK READY":"LINK OFFLINE";
            Fill(new Rect(619,30,6,6),session.Available?Green:Muted);Text(new Rect(637,23,196,26),status,small);
            Text(new Rect(619,65,214,24),session.TransportName.ToUpperInvariant()+(session.InLobby&&session.PingMilliseconds>=0?" / "+session.PingMilliseconds+" ms":""),small);
        }
        private void Activity(float y){
            Text(new Rect(28,y+10,210,24),"ONLINE ACTIVITY",small);
            var a=session.Activity;
            string[] values={a.Format(a.Online),a.Format(a.Queuing),a.Format(a.Racing)},names={"ONLINE","QUEUING","RACING"};
            for(int i=0;i<3;++i){float x=421+i*153;if(i>0)Fill(new Rect(x-15,y+5,1,31),Edge);Text(new Rect(x,y,70,38),values[i],number);Text(new Rect(x+72,y+13,79,22),names[i],small);}
            Fill(new Rect(26,y+52,848,1),Edge);
        }
        private void BrowserView(){
            Activity(116);bool steam=session.TransportIndex==0;bool idle=!session.Busy;float x=26,y=190;
            if(ActionButton(new Rect(x,y,232,47),"QUICK MATCH",steam&&session.Available&&idle,true)){session.QuickMatch();if(session.IsQuickMatching)SetOpen(false);}
            if(ActionButton(new Rect(x,y+58,232,47),"HOST A BATTLE",session.Available&&idle))session.HostRoom();
            if(ActionButton(new Rect(x,y+116,232,47),steam?"JOIN WITH CODE":"JOIN WITH ADDRESS",idle)){joinEntry=true;controllerFocus.Reset();}
            if(ActionButton(new Rect(x,y+174,232,47),"BACK TO GAME"))SetOpen(false);
            if(ActionButton(new Rect(x,438,112,33),"STEAM",idle,steam))session.SelectTransport(0);
            if(ActionButton(new Rect(x+120,438,112,33),"LAN DIRECT",idle,!steam))session.SelectTransport(1);
            if(steam){
                Section(new Rect(280,190,594,286),"OPEN ROOMS");
                if(ActionButton(new Rect(736,201,120,28),session.Available?"REFRESH":"RETRY STEAM",idle))session.RefreshRooms();
                RoomsList(new Rect(299,246,552,198));
            }else{
                Section(new Rect(280,190,594,286),"DIRECT CONNECTION");
                Text(new Rect(300,255,530,70),"Host a LAN room or enter the host's address.",wrapped);
                Text(new Rect(300,325,530,50),"Both drivers must be reachable on the same network.",wrapped);
            }
        }
        internal static float ScrollThumbHeight(float track,float viewport,float content)=>Mathf.Min(track,Mathf.Max(24,track*viewport/Mathf.Max(viewport,content)));
        private void RoomsList(Rect view){
            var rooms=session.Rooms;int count=rooms==null?0:rooms.Count;float content=Mathf.Max(view.height,count*66),max=content-view.height;
            string focus=controllerFocus.Selected;
            if(focus!=lastRoomFocus){for(int i=0;i<count;++i)if(focus=="room:"+rooms[i].Code){float top=i*66;if(top<roomScroll.y)roomScroll.y=top;else if(top+66>roomScroll.y+view.height)roomScroll.y=top+66-view.height;}lastRoomFocus=focus;}
            roomScroll.y=Mathf.Clamp(roomScroll.y,0,max);
            var track=new Rect(view.xMax-8,view.y,8,view.height);float thumbHeight=ScrollThumbHeight(track.height,view.height,content);
            var thumb=new Rect(track.x,track.y+(max>0?roomScroll.y/max*(track.height-thumbHeight):0),track.width,thumbHeight);var e=Event.current;
            if(GUI.enabled&&e.type==EventType.ScrollWheel&&view.Contains(e.mousePosition)){roomScroll.y=Mathf.Clamp(roomScroll.y+e.delta.y*22,0,max);e.Use();}
            if(GUI.enabled&&e.type==EventType.MouseDown&&e.button==0&&track.Contains(e.mousePosition)){
                if(thumb.Contains(e.mousePosition)&&max>0){scrollDragging=true;scrollDragY=e.mousePosition.y;scrollDragStart=roomScroll.y;}
                else roomScroll.y=Mathf.Clamp(roomScroll.y+(e.mousePosition.y<thumb.y?-view.height:view.height),0,max);e.Use();
            }
            if(scrollDragging&&e.type==EventType.MouseDrag){if(max>0)roomScroll.y=Mathf.Clamp(scrollDragStart+(e.mousePosition.y-scrollDragY)*max/Mathf.Max(1,track.height-thumbHeight),0,max);e.Use();}
            if(e.type==EventType.MouseUp)scrollDragging=false;
            GUI.BeginGroup(new Rect(view.x,view.y,view.width-22,view.height));
            for(int i=0;i<count;++i){var room=rooms[i];float y=i*66-roomScroll.y;
                Fill(new Rect(0,y,view.width-22,1),Edge);Text(new Rect(0,y+11,view.width-121,22),Idas3LobbyNames.ForHost(room.HostName),label);
                Text(new Rect(0,y+37,view.width-121,19),Safe(room.HostName,"DRIVER")+" / "+room.Members+" OF 2 DRIVERS",small);
                if(ActionButton(new Rect(view.width-106,y+16,78,33),room.Members<2?"JOIN":"FULL",room.Members<2&&session.Available&&!session.Busy,false,"room:"+room.Code))session.JoinRoom(room.Code);
            }
            if(count==0){Text(new Rect(8,61,view.width-40,30),session.Busy?"SEARCHING…":"NO OPEN ROOMS",heading);}
            GUI.EndGroup();Fill(track,Ink);Frame(track,Edge);thumb.y=track.y+(max>0?roomScroll.y/max*(track.height-thumbHeight):0);Fill(thumb,scrollDragging?Red:Muted);
        }
        private void JoinView(){
            Section(new Rect(26,126,848,350),session.TransportIndex==0?"ROOM CODE":"HOST ADDRESS");
            Text(new Rect(48,195,790,24),session.TransportIndex==0?"ROOM CODE":"IP ADDRESS:PORT",small);
            GUI.SetNextControlName("idas3-room-code");var old=GUI.backgroundColor;GUI.backgroundColor=Raised;
            joinCode=GUI.TextField(new Rect(48,235,657,48),joinCode,128,field);GUI.backgroundColor=old;
            if(ActionButton(new Rect(719,235,132,48),"EDIT",!session.Busy)){codeDraft=joinCode;codeEditing=true;controllerFocus.Reset();GUI.FocusControl(null);}
            if(ActionButton(new Rect(48,316,803,47),"JOIN BATTLE",session.Available&&!session.Busy&&!string.IsNullOrWhiteSpace(joinCode),true)){session.JoinRoom(joinCode.Trim());joinEntry=false;}
        }
        private void CodeEntryView(){
            Section(new Rect(26,116,848,400),session.TransportIndex==0?"ENTER ROOM CODE":"ENTER HOST ADDRESS");
            // Long addresses remain visible at the insertion point.
            Text(new Rect(48,174,805,35),codeDraft.Length>58?"…"+codeDraft.Substring(codeDraft.Length-58)+"_":codeDraft+"_",heading);
            const string characters="1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.:/-_";
            for(int i=0;i<characters.Length;++i){char c=characters[i];if(ActionButton(new Rect(48+(i%13)*62,224+(i/13)*42,54,34),c.ToString())&&codeDraft.Length<128)codeDraft+=c;}
            if(ActionButton(new Rect(26,558,194,42),"DELETE",codeDraft.Length>0))codeDraft=codeDraft.Substring(0,codeDraft.Length-1);
            if(ActionButton(new Rect(244,558,194,42),"CLEAR",codeDraft.Length>0))codeDraft="";
            if(ActionButton(new Rect(462,558,194,42),"CANCEL")){codeEditing=false;controllerFocus.Reset();}
            if(ActionButton(new Rect(680,558,194,42),"DONE",true,true)){joinCode=codeDraft;codeEditing=false;joinEntry=true;controllerFocus.Reset();}
        }
        private void SearchView(){
            Activity(116);Section(new Rect(26,190,848,286),"QUICK MATCH");
            Text(new Rect(76,266,748,43),"FINDING A DRIVER",title);
            Text(new Rect(78,332,710,64),session.StatusText,wrapped);
            if(session.InLobby)Text(new Rect(78,414,710,23),"ROOM / "+session.RoomCode,small);
        }
        private int ConnectedPlayers(){int count=0;foreach(var p in session.Players)if(p!=null&&p.Connected)++count;return count;}
        private void RoomView(){
            joinEntry=false;
            Text(new Rect(28,115,590,25),"ROOM / "+session.RoomCode,small);
            if(ActionButton(new Rect(680,110,83,29),"COPY",!string.IsNullOrEmpty(session.RoomCode)))GUIUtility.systemCopyBuffer=session.RoomCode;
            Text(new Rect(782,116,94,24),ConnectedPlayers()+" / 2 DRIVERS",small);
            bool terminal=ResultsVisible||session.StateName=="Returning";
            Section(new Rect(26,150,452,340),terminal?"BATTLE FINISHED":"DRIVERS");
            var players=session.Players;
            for(int slot=0;slot<2;++slot){var p=slot<players.Count?players[slot]:null;float y=204+slot*140;
                Fill(new Rect(44,y-4,416,1),Edge);
                if(p==null||!p.Connected){Text(new Rect(44,y+34,414,33),"WAITING FOR A DRIVER",heading,Muted);continue;}
                Text(new Rect(44,y+3,231,21),(p.IsLocal?"YOU":"OPPONENT")+(p.IsHost?" / HOST":""),small);
                Text(new Rect(44,y+27,255,28),Safe(p.Name,"DRIVER"),heading);
                string status=terminal?"FINISHED":session.IsRacing?"RACING":p.Ready?"READY":"NOT READY";
                Text(new Rect(323,y+27,137,26),status,button,p.Ready?Green:Muted);
                var carRect=new Rect(78,y+68,252,26);
                if(p.IsLocal){bool edit=!session.IsRacing&&!session.Busy&&!session.HasCourseDraw;
                    if(ActionButton(new Rect(44,y+65,26,29),"<",edit))session.CycleCar(-1);
                    Text(carRect,Safe(session.LocalSavedCar?.Label??p.CarName,"SELECT CAR"),small);
                    if(ActionButton(new Rect(325,y+65,26,29),">",edit))session.CycleCar(1);
                    if(ActionButton(new Rect(362,y+65,98,29),(session.LocalSavedCar?.Automatic??true)?"AT  /  CHANGE":"MT  /  CHANGE",edit))session.SetAutomatic(!(session.LocalSavedCar?.Automatic??true));
                }else{Text(new Rect(44,y+68,301,26),Safe(session.RemoteSavedCar?.Label??p.CarName),small);Text(new Rect(359,y+68,104,23),(session.RemoteSavedCar?.Automatic??true?"AT":"MT")+" / "+p.PingMilliseconds+" ms",small);}
                Text(new Rect(44,y+106,416,24),PickSummary(p.IsLocal?session.LocalChoice:session.RemoteChoice),small);
            }
            if(session.HasCourseDraw||session.IsRacing||terminal){SelectedCourse();return;}
            Section(new Rect(498,150,376,340),"YOUR COURSE PICK");Text(new Rect(791,164,63,23),"50/50",small);
            var c=session.LocalChoice;bool change=!session.IsRacing&&!session.Busy;
            if(ActionButton(new Rect(516,205,29,35),"<",change))ChangeOptions((c.Course+session.AvailableCourseCount-1)%session.AvailableCourseCount,c.Reverse,c.Wet,c.Night);
            Text(new Rect(550,205,266,35),Track(c.Course),button);
            if(ActionButton(new Rect(825,205,29,35),">",change))ChangeOptions((c.Course+1)%session.AvailableCourseCount,c.Reverse,c.Wet,c.Night);
            OptionRow(249,"DIRECTION",Direction(c.Course,c.Reverse),change,()=>ChangeOptions(c.Course,!c.Reverse,c.Wet,c.Night));
            OptionRow(294,"SURFACE",c.Course==8?"SNOW":c.Wet?"WET":"DRY",change&&c.Course!=8,()=>ChangeOptions(c.Course,c.Reverse,!c.Wet,c.Night));
            OptionRow(339,"TIME",c.Night?"NIGHT":"DAY",change&&c.Course!=4&&c.Course!=8&&c.Course!=11,()=>ChangeOptions(c.Course,c.Reverse,c.Wet,!c.Night));
            OptionRow(384,"BOOST",session.BoostEnabled?"ON":"OFF",change&&session.IsHost,()=>session.SetBoost(!session.BoostEnabled));
            OptionRow(429,"CAR COLLISIONS",session.CollisionsEnabled?"ON":"OFF",change&&session.IsHost,()=>session.SetCollisions(!session.CollisionsEnabled));
        }
        private void SelectedCourse(){
            Section(new Rect(498,150,376,340),ResultsVisible||session.StateName=="Returning"?"RESULT":"RACE COURSE");
            if(ResultsVisible||session.StateName=="Returning")Text(new Rect(517,214,337,74),Safe(session.ResultText,"RETURNING TO LOBBY…"),heading);
            else Text(new Rect(517,213,337,43),session.CourseWinnerSlot==(session.IsHost?0:1)?"YOUR PICK SELECTED":"OPPONENT PICK SELECTED",small);
            Text(new Rect(517,288,337,36),Track(session.Course),heading);
            Text(new Rect(517,337,337,46),Conditions(session.Course,session.Reverse,session.Wet,session.Night),wrapped,Color.white);
            Text(new Rect(517,398,337,41),"BOOST "+(session.BoostEnabled?"ON":"OFF")+" / CAR COLLISIONS "+(session.CollisionsEnabled?"ON":"OFF"),wrapped);
            Text(new Rect(517,451,337,28),session.CountdownText,small);
        }
        private void OptionRow(float y,string name,string value,bool enabled,Action action){Fill(new Rect(516,y-3,338,1),Edge);Text(new Rect(516,y+8,155,25),name,small);if(ActionButton(new Rect(679,y,175,35),value,enabled,false,"course-option:"+name))action();}
        private void ChangeOptions(int course,bool reverse,bool wet,bool night){if(course==4||course==11)night=true;if(course==8){wet=true;night=true;}session.SetRaceOptions(course,reverse,wet,night);}
        private void StatusLine(){
            bool error=!string.IsNullOrEmpty(session.ErrorText);string text=error?session.ErrorText:session.StateName=="Returning"?"WAITING FOR THE OTHER DRIVER…":session.ResultText;
            if(string.IsNullOrEmpty(text)&&session.Busy&&!session.IsQuickMatching)text=session.StatusText;
            if(string.IsNullOrEmpty(text)&&session.ExperimentalAuthority&&session.HandshakeComplete&&!session.IsRacing)text=session.ConnectionQuality;
            if(string.IsNullOrEmpty(text))return;
            Fill(new Rect(26,502,848,44),Panel);Fill(new Rect(26,502,3,44),error?Red:Edge);
            Text(new Rect(40,511,error?774:813,34),text,wrapped,error?new Color32(255,128,124,255):Muted);
            if(error&&ActionButton(new Rect(831,509,29,29),"X"))session.ClearError();
        }
        private void Footer(){
            Fill(new Rect(26,553,848,1),Edge);
            if(joinEntry){if(ActionButton(new Rect(26,570,220,35),"BACK")){joinEntry=false;controllerFocus.Reset();}return;}
            if(session.IsQuickMatching){if(ActionButton(new Rect(26,570,232,35),"CANCEL SEARCH",true,true))session.CancelQuickMatch();if(ActionButton(new Rect(642,570,232,35),"BACK TO GAME"))SetOpen(false);return;}
            if(MusicSelectionAllowed){if(ActionButton(new Rect(26,567,348,35),"MUSIC  / "+SelectedMusicTitle))MusicSelectionRequested?.Invoke();}
            else Text(new Rect(28,578,370,23),"↑ ↓ SELECT   ENTER / A CONFIRM   ESC / B BACK",small);
            if(!session.InLobby)return;
            if(session.StateName=="Returning"){ActionButton(new Rect(498,567,376,35),"RETURNING TO LOBBY…",false);return;}
            if(ActionButton(new Rect(393,567,126,35),"LEAVE ROOM",!session.Busy))session.LeaveRoom();
            if(session.IsRacing){
                if(session.CanReturnToLobby){if(ActionButton(new Rect(537,567,337,35),"RETURN TO LOBBY",CanAcknowledgeReturnToLobby,true))RequestReturnToLobby();}
                else if(session.StateName=="Results")ActionButton(new Rect(537,567,337,35),"RETURNING TO LOBBY…",false);
                else if(ActionButton(new Rect(537,567,337,35),"RETURN TO RACE",true,true))SetOpen(false);
            }else{
                bool syncing=session.HandshakeComplete&&!session.Busy&&!session.CanReady;
                if(ActionButton(new Rect(537,567,153,35),syncing?"SYNCING PICKS":session.LocalReady?"CANCEL READY":"READY",session.CanReady,!session.LocalReady))session.SetReady(!session.LocalReady);
                if(session.IsHost){if(ActionButton(new Rect(708,567,166,35),"START BATTLE",session.CanStart,true))session.StartRace();}
                else Text(new Rect(708,567,166,35),session.LocalReady?"WAITING FOR HOST":"GET READY",button,Muted);
            }
        }
        private void DisconnectedFinishView(){
            var safe=Screen.safeArea;if(safe.width<=0||safe.height<=0)safe=new Rect(0,0,Screen.width,Screen.height);
            const float width=700,height=146;float factor=Mathf.Min(1.5f,Mathf.Min(safe.width/(width+40),safe.height/720f));
            GUI.matrix=Matrix4x4.TRS(new Vector3(safe.x+(safe.width-width*factor)*.5f,Screen.height-safe.y-(height+22)*factor,0),Quaternion.identity,new Vector3(factor,factor,1));
            Fill(new Rect(0,0,width,height),Ink);Frame(new Rect(0,0,width,height),Edge);Fill(new Rect(0,0,width,4),Red);
            Text(new Rect(20,14,658,42),"CONNECTION LOST",title);Text(new Rect(23,60,653,26),"Race ended. No result recorded.",label);
            Text(new Rect(23,104,345,25),"0 POINTS AWARDED",heading);
            if(ActionButton(new Rect(423,93,253,37),"RETURN TO MENU",CanAcknowledgeDisconnectedFinish,true))AcknowledgeDisconnectedFinish();
        }
    }
}
