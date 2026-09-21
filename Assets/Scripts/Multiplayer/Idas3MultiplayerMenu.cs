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
        private const float Width=1120,Height=660;
        private static readonly Color Ink=new Color32(7,14,27,255);
        private static readonly Color Panel=new Color32(14,29,49,255);
        private static readonly Color Raised=new Color32(22,43,67,255);
        private static readonly Color Edge=new Color32(49,77,106,255);
        private static readonly Color Muted=new Color32(157,179,201,255);
        private static readonly Color Yellow=new Color32(255,216,49,255);
        private static readonly Color Cyan=new Color32(94,205,242,255);
        private static readonly Color Green=new Color32(108,225,156,255);
        private static readonly Color Red=new Color32(255,128,124,255);
        private static readonly string[] Courses={"MYOGI","USUI","AKAGI","AKINA","HAPPOGAHARA","IROHAZAKA","SHOMARU","TSUCHISAKA","AKINA SNOW","HAKONE","SADAMINE","ENNA SKYLINE"};
        private Idas3MultiplayerSession session;
        private GUIStyle title,heading,label,small,button,field,number,wrapped;
        private string joinCode="";
        private Vector2 roomScroll;
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
            controllerFocus.Reset();codeEditing=false;
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
            else if(IsOpen&&Input.GetKeyDown(KeyCode.Escape))SetOpen(false);
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
            else if(IsOpen&&cancelPressed){if(codeEditing){codeEditing=false;controllerFocus.Reset();}else SetOpen(false);}
        }
        internal void ProcessMenuNavigation(int horizontal,int vertical,bool confirm,bool back,bool blocked,double now,bool wheel=false){
            steeringOnly=wheel;
            if(wheel&&horizontal!=0){vertical=horizontal;horizontal=0;}
            navigationHeld=confirm||back;
            if(!IsOpen)return;
            if(controllerFocus.Poll(horizontal,vertical,confirm,back,blocked||InputCovered||Idas3MenuPointer.Active,now)){
                if(codeEditing){codeEditing=false;controllerFocus.Reset();}else SetOpen(false);
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
        private void Styles()
        {
            if(label!=null)return;
            label=new GUIStyle(GUI.skin.label){fontSize=18,clipping=TextClipping.Clip,padding=new RectOffset(0,0,0,0)};
            label.normal.textColor=Color.white;
            small=new GUIStyle(label){fontSize=13};
            heading=new GUIStyle(label){fontSize=21,fontStyle=FontStyle.Bold};
            title=new GUIStyle(heading){fontSize=34,fontStyle=FontStyle.BoldAndItalic};
            number=new GUIStyle(title){fontSize=48};
            button=new GUIStyle(label){fontSize=16,fontStyle=FontStyle.Bold,alignment=TextAnchor.MiddleCenter};
            field=new GUIStyle(GUI.skin.textField){fontSize=19,padding=new RectOffset(12,12,10,10),alignment=TextAnchor.MiddleLeft};
            field.normal.textColor=Color.white;field.focused.textColor=Color.white;
            field.normal.background=Texture2D.whiteTexture;field.focused.background=Texture2D.whiteTexture;
            wrapped=new GUIStyle(small){wordWrap=true,clipping=TextClipping.Clip};
        }
        private static void Fill(Rect rect,Color color)
        {
            Color before=GUI.color;GUI.color=color;GUI.DrawTexture(rect,Texture2D.whiteTexture);GUI.color=before;
        }
        private static void Frame(Rect rect,Color color)
        {
            Fill(new Rect(rect.x,rect.y,rect.width,1),color);Fill(new Rect(rect.x,rect.yMax-1,rect.width,1),color);
            Fill(new Rect(rect.x,rect.y,1,rect.height),color);Fill(new Rect(rect.xMax-1,rect.y,1,rect.height),color);
        }
        private static string Safe(string text,string fallback="")=>string.IsNullOrWhiteSpace(text)?fallback:text;
        private static string Track(int course)=>course>=0&&course<Courses.Length?Courses[course]:"SELECT COURSE";
        private static string Direction(int course,bool reverse)=>course>=9?(reverse?"UPHILL":"DOWNHILL"):(reverse?"REVERSE":"FORWARD");
        private static string Conditions(int course,bool reverse,bool wet,bool night)=>
            Direction(course,reverse)+"  /  "+(course==8?"SNOW":wet?"WET":"DRY")+"  /  "+(night?"NIGHT":"DAY");
        private static string PickSummary(Idas3RaceChoice choice)=>Track(choice.Course)+"  /  "+Conditions(choice.Course,choice.Reverse,choice.Wet,choice.Night);
        private void Text(Rect rect,string text,GUIStyle style,Color? color=null)
        {
            Color before=GUI.contentColor;GUI.contentColor=color??(style==small||style==wrapped?Muted:Color.white);GUI.Label(rect,text??"",style);GUI.contentColor=before;
        }
        private bool ActionButton(Rect rect,string text,bool enabled=true,bool primary=false,string identity=null)
        {
            string id=identity??rect.x+":"+rect.y+":"+text;
            if(enabled&&GUI.enabled&&Event.current.type==EventType.MouseDown&&rect.Contains(Event.current.mousePosition))controllerFocus.Pointer(id);
            bool controllerClick=IsOpen&&controllerFocus.Control(id,rect,enabled&&GUI.enabled,Event.current.type==EventType.Repaint);
            bool hover=enabled&&rect.Contains(Event.current.mousePosition);
            Fill(rect,!enabled?new Color32(25,36,48,255):primary?(hover?new Color32(255,233,107,255):Yellow):(hover?new Color32(38,67,96,255):Raised));
            Frame(rect,enabled?(primary?Yellow:Edge):new Color32(39,52,65,255));
            Text(rect,text,button,!enabled?new Color32(102,122,140,255):primary?Ink:Color.white);
            if(IsOpen&&enabled&&controllerFocus.Focused(id))Frame(new Rect(rect.x-3,rect.y-3,rect.width+6,rect.height+6),Color.white);
            bool before=GUI.enabled;GUI.enabled=before&&enabled;
            bool clicked=GUI.Button(rect,GUIContent.none,GUIStyle.none);GUI.enabled=before;return clicked||controllerClick;
        }
        private void Section(Rect rect,string titleText,string detail=null)
        {
            Fill(rect,Panel);Frame(rect,Edge);Fill(new Rect(rect.x,rect.y,4,42),Cyan);
            Text(new Rect(rect.x+18,rect.y+12,rect.width-36,29),titleText,heading);
            if(detail!=null)Text(new Rect(rect.x+18,rect.y+45,rect.width-36,34),detail,wrapped);
        }
        private void OnGUI()
        {
            if(session==null||session.ChallengerPending)return;
            SynchronizeRaceVisibility();
            Styles();GUI.depth=DisconnectedFinishVisible?-13000:-10000;
            if(IsOpen&&!InputCovered&&(Event.current.type==EventType.MouseDown||Event.current.type==EventType.MouseDrag||Event.current.type==EventType.MouseUp))controllerFocus.Pointer();
            bool oldEnabled=GUI.enabled;GUI.enabled=oldEnabled&&(DisconnectedFinishVisible||!InputCovered);
            Matrix4x4 oldMatrix=GUI.matrix;
            bool diagnostic=IsOpen&&diagnosticTarget!=null&&Event.current.type==EventType.Repaint;
            RenderTexture oldTarget=RenderTexture.active;
            if(IsOpen&&Event.current.type==EventType.Repaint)++DiagnosticRepaints;
            if(diagnostic){
                RenderTexture.active=diagnosticTarget;GL.PushMatrix();
                GL.LoadPixelMatrix(0,Screen.width,Screen.height,0);
            }
            try{
                if(IsOpen){controllerFocus.SpatialVertical=codeEditing&&!steeringOnly;controllerFocus.Begin();}
                if(DisconnectedFinishVisible){
                    // The original FINISH logo remains in the upper race view.
                    // Do not draw a lobby, fullscreen dimmer, or race controls.
                    if(Event.current.type==EventType.KeyDown||Event.current.type==EventType.KeyUp)Event.current.Use();
                    DisconnectedFinishView();return;
                }
                if(!IsOpen){
                    if(RaceHudActive||session.IsRacing)return;
                    if(!session.InLobby&&!session.IsQuickMatching)return;
                    float scale=Mathf.Clamp(Screen.height/900f,.75f,1.25f);
                    GUI.matrix=Matrix4x4.TRS(new Vector3(Screen.width-278*scale,18*scale,0),Quaternion.identity,new Vector3(scale,scale,1));
                    string state=session.IsQuickMatching?"  /  SEARCHING":session.InLobby?(session.PingMilliseconds>=0?"  /  "+session.PingMilliseconds+" ms":"  /  CONNECTED"):"";
                    if(ActionButton(new Rect(0,0,258,38),"F1   ONLINE BATTLE"+state))SetOpen(true);
                    if(session.HasCourseDraw){
                        Fill(new Rect(0,38,258,29),Panel);Frame(new Rect(0,38,258,29),Edge);
                        Text(new Rect(10,43,238,22),"RACE COURSE  /  "+Track(session.Course),small,Yellow);
                    }
                    return;
                }
                Fill(new Rect(0,0,Screen.width,Screen.height),new Color(0.01f,.025f,.05f,.90f));
                float factor=Mathf.Min(1.5f,Mathf.Min(Screen.width/(Width+48),Screen.height/(Height+40)));
                GUI.matrix=Matrix4x4.TRS(new Vector3((Screen.width-Width*factor)*.5f,(Screen.height-Height*factor)*.5f,0),Quaternion.identity,new Vector3(factor,factor,1));
                Fill(new Rect(0,0,Width,Height),Ink);Frame(new Rect(0,0,Width,Height),Edge);
                for(int x=0;x<Width;x+=40)Fill(new Rect(x,0,1,Height),new Color(0.3f,.5f,.7f,.025f));
                Fill(new Rect(0,0,Width,4),Yellow);Fill(new Rect(0,96,Width,2),Edge);
                Text(new Rect(24,15,520,43),"ONLINE BATTLE",title);
                Text(new Rect(26,61,650,22),"INITIAL D  /  TWO DRIVERS  /  ONE MOUNTAIN PASS",small);
                HeaderStatus();
                if(!codeEditing&&!ResultsVisible&&session.StateName!="Returning"&&ActionButton(new Rect(1048,22,48,40),"X"))SetOpen(false);
                if(codeEditing)CodeEntryView();else{
                    if(session.InLobby)RoomView();else BrowserView();
                    StatusLine();Footer();
                }
                if(!session.InLobby&&Event.current.type==EventType.KeyDown&&Event.current.keyCode==KeyCode.Return&&
                    GUI.GetNameOfFocusedControl()=="idas3-room-code"&&session.Available&&!session.Busy&&!session.IsQuickMatching&&!string.IsNullOrWhiteSpace(joinCode)){
                    session.JoinRoom(joinCode.Trim());Event.current.Use();
                }
            }finally{
                if(IsOpen)controllerFocus.End();
                GUI.enabled=oldEnabled;
                GUI.matrix=oldMatrix;
                if(diagnostic){GL.PopMatrix();RenderTexture.active=oldTarget;diagnosticTarget=null;DiagnosticCaptureReady=true;}
            }
        }
        private void DisconnectedFinishView()
        {
            var safe=Screen.safeArea;
            if(safe.width<=0||safe.height<=0)safe=new Rect(0,0,Screen.width,Screen.height);
            const float cardWidth=700,cardHeight=146;
            float factor=Mathf.Min(1.5f,Mathf.Min(safe.width/(cardWidth+40),safe.height/720f));
            float x=safe.x+(safe.width-cardWidth*factor)*.5f;
            float y=Screen.height-safe.y-(cardHeight+22)*factor;
            GUI.matrix=Matrix4x4.TRS(new Vector3(x,y,0),Quaternion.identity,new Vector3(factor,factor,1));
            Fill(new Rect(0,0,cardWidth,cardHeight),new Color32(5,10,18,246));Frame(new Rect(0,0,cardWidth,cardHeight),Color.white);
            Fill(new Rect(0,0,cardWidth,5),new Color32(188,13,13,255));
            Text(new Rect(20,14,658,42),"CONNECTION LOST",title,Color.white);
            Text(new Rect(23,60,653,26),"Race ended. No result recorded.",label,Color.white);
            Text(new Rect(23,104,345,25),"0 POINTS AWARDED",heading,Yellow);
            if(ActionButton(new Rect(423,93,253,37),"RETURN TO MENU",CanAcknowledgeDisconnectedFinish,true))AcknowledgeDisconnectedFinish();
        }
        private void HeaderStatus()
        {
            string status=session.StateName=="Returning"?"RETURNING TO LOBBY":ResultsVisible?"RACE FINISHED":!string.IsNullOrEmpty(session.CountdownText)?session.CountdownText:session.IsRacing?"BATTLE IN PROGRESS":session.IsQuickMatching?"FINDING A DRIVER":session.InLobby?"ROOM CONNECTED":session.Available?"LINK READY":"LINK OFFLINE";
            Color tone=!string.IsNullOrEmpty(session.CountdownText)||session.IsQuickMatching?Yellow:session.Available?Green:Muted;
            Fill(new Rect(778,26,8,8),tone);Text(new Rect(800,19,238,28),status,button,tone);
            Text(new Rect(778,57,255,22),Safe(session.TransportName,"ONLINE")+(session.InLobby&&session.PingMilliseconds>=0?"   /   "+session.PingMilliseconds+" ms":""),small);
        }
        private void BrowserView()
        {
            bool steam=session.TransportIndex==0;
            bool searching=session.IsQuickMatching;
            Section(new Rect(24,116,660,394),steam?"FIND A BATTLE":"DIRECT CONNECTION");
            if(steam){
                if(ActionButton(new Rect(546,128,120,32),session.Available?"REFRESH":"RETRY STEAM",!session.Busy&&!searching))session.RefreshRooms();
                if(ActionButton(new Rect(42,176,264,54),searching?"CANCEL SEARCH":"QUICK MATCH",searching||session.Available&&!session.Busy,true)){
                    if(searching)session.CancelQuickMatch();else {session.QuickMatch();if(session.IsQuickMatching)SetOpen(false);}
                }
                Text(new Rect(324,179,330,49),searching?"Finding another driver.\nYou choose when to get ready.":"Join an open room automatically,\nor host and wait for another driver.",wrapped);
                Fill(new Rect(42,246,624,1),Edge);
                Text(new Rect(42,261,624,23),"OPEN ROOMS  /  MANUAL JOIN",small);
                var rooms=session.Rooms;int count=rooms==null?0:rooms.Count;
                if(count==0){
                    Text(new Rect(66,337,576,40),session.Busy?"SEARCHING FOR ROOMS…":"NO OPEN ROOMS",heading,session.Busy?Cyan:Color.white);
                    Text(new Rect(66,381,552,67),searching?"Quick Match will open a room if none are available.":session.Busy?"Waiting for the room list.":"Use Quick Match to find a driver, host your own battle, or join with a room code.",wrapped);
                }else{
                    roomScroll=GUI.BeginScrollView(new Rect(42,294,624,197),roomScroll,new Rect(0,0,602,count*78),false,false);
                    for(int i=0;i<count;++i){
                        var room=rooms[i];float y=i*78;Fill(new Rect(0,y,600,68),Raised);
                        Text(new Rect(14,y+9,390,27),Safe(room.Name,"MOUNTAIN PASS BATTLE"),heading);
                        Text(new Rect(14,y+40,370,20),Safe(room.HostName,"HOST")+"   /   "+room.Members+" OF 2 DRIVERS",small);
                        bool canJoin=room.Members<2&&session.Available&&!session.Busy&&!searching;
                        string roomLabel=room.Members<2?"JOIN":"FULL";
                        string roomId="room:"+room.Code;
                        if(controllerFocus.Focused(roomId))roomScroll.y=Mathf.Clamp(y-65,0,Mathf.Max(0,rooms.Count*78-214));
                        if(ActionButton(new Rect(459,y+14,125,40),roomLabel,canJoin,false,roomId))session.JoinRoom(room.Code);
                    }
                    GUI.EndScrollView();
                }
            }else{
                Text(new Rect(54,207,588,36),"RACE ON YOUR LOCAL NETWORK",heading,Cyan);
                Text(new Rect(54,256,560,95),"Host a LAN room and share the address shown in the lobby. The second driver enters that address to join.\n\nBoth games must be reachable on the same network.",wrapped);
                Fill(new Rect(54,374,576,1),Edge);
                Text(new Rect(54,395,560,44),"LAN rooms use direct addresses and do not appear in the Steam room list.",wrapped);
            }
            Section(new Rect(704,116,392,394),"YOUR CONNECTION");
            if(ActionButton(new Rect(722,172,172,38),"STEAM ONLINE",!session.Busy&&!searching,steam))session.SelectTransport(0);
            if(ActionButton(new Rect(904,172,174,38),"LAN DIRECT",!session.Busy&&!searching,!steam))session.SelectTransport(1);
            Text(new Rect(722,229,350,21),"OPEN A ROOM",small);
            if(ActionButton(new Rect(722,258,356,49),"HOST A BATTLE",session.Available&&!session.Busy&&!searching,!steam))session.HostRoom();
            Fill(new Rect(722,330,356,1),Edge);
            Text(new Rect(722,348,350,23),steam?"JOIN WITH A ROOM CODE":"JOIN WITH A HOST ADDRESS",small);
            GUI.SetNextControlName("idas3-room-code");
            Color before=GUI.backgroundColor;GUI.backgroundColor=Raised;
            joinCode=GUI.TextField(new Rect(722,382,306,45),joinCode,128,field);GUI.backgroundColor=before;
            if(ActionButton(new Rect(1032,382,46,45),"EDIT",!session.Busy&&!searching)){codeDraft=joinCode;codeEditing=true;controllerFocus.Reset();GUI.FocusControl(null);}
            if(string.IsNullOrEmpty(joinCode)&&GUI.GetNameOfFocusedControl()!="idas3-room-code")
                Text(new Rect(734,395,324,25),steam?"Enter room code":"IP address:port",small);
            if(ActionButton(new Rect(722,444,356,46),session.Busy?"CONNECTING…":"JOIN BATTLE",session.Available&&!session.Busy&&!searching&&!string.IsNullOrWhiteSpace(joinCode)))session.JoinRoom(joinCode.Trim());
        }
        private void CodeEntryView(){
            Section(new Rect(24,116,1072,394),"ENTER ROOM CODE / HOST ADDRESS");
            Text(new Rect(50,177,1020,50),codeDraft+"_",heading,Yellow);
            const string characters="1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz.:/-_";
            for(int i=0;i<characters.Length;++i){char c=characters[i];
                if(ActionButton(new Rect(50+(i%13)*78,245+(i/13)*43,68,36),c.ToString())&&codeDraft.Length<128)codeDraft+=c;
            }
            if(ActionButton(new Rect(50,540,230,48),"DELETE",codeDraft.Length>0))codeDraft=codeDraft.Substring(0,codeDraft.Length-1);
            if(ActionButton(new Rect(300,540,230,48),"CLEAR",codeDraft.Length>0))codeDraft="";
            if(ActionButton(new Rect(550,540,230,48),"CANCEL")){codeEditing=false;controllerFocus.Reset();}
            if(ActionButton(new Rect(800,540,230,48),"DONE",true,true)){joinCode=codeDraft;codeEditing=false;controllerFocus.Reset();}
            Text(new Rect(50,611,980,24),"D-PAD / STICK: SELECT    A / ACCEL: CONFIRM    B / BRAKE: BACK",small);
        }
        private void RoomView()
        {
            Section(new Rect(24,116,660,394),session.StateName=="Returning"?"RETURNING TO LOBBY":ResultsVisible?"RACE RESULTS":session.IsQuickMatching?"QUICK MATCH  /  WAITING FOR A DRIVER":"THE STARTING LINE");
            Text(new Rect(43,160,514,24),"ROOM   "+Safe(session.RoomCode,"CONNECTING"),small,Cyan);
            if(ActionButton(new Rect(570,151,96,30),"COPY",!string.IsNullOrEmpty(session.RoomCode)))GUIUtility.systemCopyBuffer=session.RoomCode;
            var players=session.Players;
            for(int slot=0;slot<2;++slot){
                Idas3PlayerInfo player=players!=null&&slot<players.Count?players[slot]:null;
                float y=slot==0?197:346;var rect=new Rect(42,y,624,138);
                Fill(rect,Raised);Frame(rect,Edge);
                bool connected=player!=null&&player.Connected;
                Fill(new Rect(rect.x,rect.y,4,rect.height),connected?(player.Ready?Green:Cyan):Edge);
                bool selected=session.HasCourseDraw&&session.CourseWinnerSlot==slot;
                if(selected)Frame(rect,Yellow);
                Text(new Rect(56,y+21,79,68),(slot+1).ToString("00"),number,connected?Yellow:Edge);
                if(!connected){
                    Text(new Rect(140,y+27,465,34),"WAITING FOR A DRIVER",heading,Muted);
                    Text(new Rect(140,y+71,474,45),session.IsQuickMatching?"Your room is open. Waiting for another Quick Match driver.":"Share the room code to invite your opponent.",wrapped);
                    continue;
                }
                string tag=(player.IsLocal?"YOU":"OPPONENT")+(player.IsHost?"  /  HOST":"");
                Text(new Rect(140,y+13,347,24),tag,small,Cyan);
                Text(new Rect(140,y+35,345,34),Safe(player.Name,"DRIVER"),heading);
                Text(new Rect(511,y+19,137,30),session.StateName=="Returning"?"RETURNING":ResultsVisible?"FINISHED":session.IsRacing?"RACING":player.Ready?"READY":"NOT READY",button,player.Ready?Green:Muted);
                if(player.IsLocal){
                    bool auto=session.LocalSavedCar?.Automatic??true;
                    if(ActionButton(new Rect(511,y+48,137,24),auto?"AT  /  CHANGE":"MT  /  CHANGE",!session.IsRacing&&!session.HasCourseDraw&&!session.Busy))session.SetAutomatic(!auto);
                }else Text(new Rect(511,y+50,137,22),(session.RemoteSavedCar?.Automatic??true?"AT":"MT")+(player.PingMilliseconds>=0?"  /  "+player.PingMilliseconds+" ms":""),small);
                if(player.IsLocal){
                    string[] cars=Idas3MultiplayerSession.CarNames;int count=cars==null?0:cars.Length;
                    bool edit=!session.IsRacing&&!session.Busy&&count>0;
                    if(ActionButton(new Rect(140,y+72,34,30),"<",edit))session.CycleCar(-1);
                    Text(new Rect(185,y+75,416,28),Safe(session.LocalSavedCar?.Label??player.CarName,count>0?cars[Mathf.Clamp(session.LocalCar,0,count-1)]:"SELECT CAR"),label);
                    if(ActionButton(new Rect(613,y+72,34,30),">",edit))session.CycleCar(1);
                }else Text(new Rect(140,y+75,490,28),Safe(session.RemoteSavedCar?.Label??player.CarName,"CHOOSING A CAR"),label);
                var pick=player.IsLocal?session.LocalChoice:session.RemoteChoice;
                Text(new Rect(56,y+112,594,21),(player.IsLocal?"YOUR PICK  /  ":"OPPONENT PICK  /  ")+PickSummary(pick),small,selected?Yellow:Muted);
            }
            if(session.HasCourseDraw){SelectedCourse();return;}
            Section(new Rect(704,116,392,394),"YOUR COURSE PICK");
            Text(new Rect(722,162,356,34),"Both ready: each driver's complete pick has a 50% chance when the battle starts.",wrapped);
            var local=session.LocalChoice;
            bool canEdit=!session.IsRacing&&!session.Busy;
            if(ActionButton(new Rect(722,207,38,51),"<",canEdit))ChangeOptions((local.Course+session.AvailableCourseCount-1)%session.AvailableCourseCount,local.Reverse,local.Wet,local.Night);
            Text(new Rect(767,207,266,51),Track(local.Course),button,Yellow);
            if(ActionButton(new Rect(1040,207,38,51),">",canEdit))ChangeOptions((local.Course+1)%session.AvailableCourseCount,local.Reverse,local.Wet,local.Night);
            OptionRow(264,"DIRECTION",Direction(local.Course,local.Reverse),canEdit,()=>ChangeOptions(local.Course,!local.Reverse,local.Wet,local.Night));
            OptionRow(309,"SURFACE",local.Course==8?"SNOW":local.Wet?"WET":"DRY",canEdit&&local.Course!=8,()=>ChangeOptions(local.Course,local.Reverse,!local.Wet,local.Night));
            OptionRow(354,"TIME",local.Night?"NIGHT":"DAY",canEdit&&local.Course!=4&&local.Course!=8&&local.Course!=11,()=>ChangeOptions(local.Course,local.Reverse,local.Wet,!local.Night));
            OptionRow(399,"BOOST",session.BoostEnabled?"ON":"OFF",canEdit&&session.IsHost,()=>session.SetBoost(!session.BoostEnabled));
            OptionRow(444,"CAR COLLISIONS",session.CollisionsEnabled?"ON":"OFF",canEdit&&session.IsHost,()=>session.SetCollisions(!session.CollisionsEnabled));
            Text(new Rect(722,489,356,20),"Host controls rules; changes reset READY.",small);
        }
        private void SelectedCourse()
        {
            Section(new Rect(704,116,392,394),"RACE COURSE");
            var players=session.Players;int slot=session.CourseWinnerSlot;
            Idas3PlayerInfo winner=players!=null&&slot>=0&&slot<players.Count?players[slot]:null;
            string owner=winner==null?"DRIVER'S PICK":winner.IsLocal?"YOUR PICK SELECTED":"OPPONENT PICK SELECTED";
            Text(new Rect(722,165,356,24),owner,small,Yellow);
            Fill(new Rect(722,206,356,66),Raised);Frame(new Rect(722,206,356,66),Yellow);
            Text(new Rect(734,217,332,43),Track(session.Course),heading,Yellow);
            Text(new Rect(722,295,356,49),Conditions(session.Course,session.Reverse,session.Wet,session.Night),wrapped,Color.white);
            Text(new Rect(722,355,356,49),"PICKED BY\n"+Safe(winner?.Name,"DRIVER"),wrapped);
            Text(new Rect(722,402,356,20),"BOOST: "+(session.BoostEnabled?"ON":"OFF")+"   CAR COLLISIONS: "+(session.CollisionsEnabled?"ON":"OFF"),small,Yellow);
            Fill(new Rect(722,422,356,1),Edge);
            Text(new Rect(722,441,356,52),Safe(session.CourseDrawText,"Chosen at random from both drivers' complete picks."),wrapped);
        }
        private void OptionRow(float y,string name,string value,bool enabled,Action action)
        {
            Text(new Rect(722,y+8,140,26),name,small);
            if(ActionButton(new Rect(872,y,206,40),value,enabled,identity:"course-option:"+name))action();
        }
        private void ChangeOptions(int course,bool reverse,bool wet,bool night)
        {
            if(course==4||course==11)night=true;if(course==8){wet=true;night=true;}
            session.SetRaceOptions(course,reverse,wet,night);
        }
        private void StatusLine()
        {
            bool error=!string.IsNullOrEmpty(session.ErrorText);
            string message=error?session.ErrorText:!string.IsNullOrEmpty(session.ResultText)?session.ResultText:session.StatusText;
            if(!error&&session.ExperimentalAuthority&&session.HandshakeComplete&&!session.IsRacing&&string.IsNullOrEmpty(session.ResultText))message=session.ConnectionQuality+". "+message;
            Fill(new Rect(24,526,1072,53),error?new Color32(56,25,31,255):Panel);
            Fill(new Rect(24,526,4,53),error?Red:Cyan);
            Text(new Rect(42,535,error?990:1036,40),Safe(message,"Ready to connect."),wrapped,error?Red:Muted);
            if(error&&ActionButton(new Rect(1049,536,32,31),"X"))session.ClearError();
        }
        private void Footer()
        {
            if(MusicSelectionAllowed){
                Text(new Rect(26,585,470,20),MusicControlHint,small,Yellow);
                if(ActionButton(new Rect(26,608,470,34),"MUSIC  /  "+SelectedMusicTitle,true))MusicSelectionRequested?.Invoke();
            }else Text(new Rect(26,613,470,23),ResultsVisible?"ENTER / A  RETURN TO LOBBY   •   LEAVE ROOM DISCONNECTS":session.StateName=="Returning"?"WAITING FOR THE OTHER DRIVER":session.IsQuickMatching?"F1 / ESC  CLOSE MENU   •   SEARCH CONTINUES":"F1 / ESC  CLOSE MENU   •   RACE CONTINUES WHILE OPEN",small);
            if(session.IsQuickMatching){
                if(ActionButton(new Rect(504,599,224,43),"CANCEL SEARCH",true,true))session.CancelQuickMatch();
                if(session.InLobby)Text(new Rect(748,599,348,43),"WAITING FOR AN OPPONENT",button,Muted);
                else if(ActionButton(new Rect(850,599,246,43),"BACK TO GAME"))SetOpen(false);
                return;
            }
            if(session.StateName=="Returning"){
                ActionButton(new Rect(698,599,398,43),"RETURNING TO LOBBY…",false,true);
                return;
            }
            if(!session.InLobby){
                if(ActionButton(new Rect(850,599,246,43),"BACK TO GAME"))SetOpen(false);
                return;
            }
            if(ActionButton(new Rect(504,599,174,43),"LEAVE ROOM",!session.Busy))session.LeaveRoom();
            if(session.IsRacing){
                if(session.CanReturnToLobby){
                    if(ActionButton(new Rect(698,599,398,43),"RETURN TO LOBBY",CanAcknowledgeReturnToLobby,true))RequestReturnToLobby();
                }else if(session.StateName=="Results"||session.StateName=="Returning"){
                    ActionButton(new Rect(698,599,398,43),"RETURNING TO LOBBY…",false,true);
                }else if(ActionButton(new Rect(698,599,398,43),"RETURN TO RACE",true,true))SetOpen(false);
            }else{
                bool syncingPicks=session.HandshakeComplete&&!session.Busy&&!session.CanReady;
                string readyText=syncingPicks?"SYNCING PICKS":session.LocalReady?"CANCEL READY":"READY";
                if(ActionButton(new Rect(698,599,180,43),readyText,session.CanReady,!session.LocalReady))session.SetReady(!session.LocalReady);
                if(session.IsHost){
                    if(ActionButton(new Rect(898,599,198,43),"START BATTLE",session.CanStart&&!session.Busy,true))session.StartRace();
                }else Text(new Rect(898,599,198,43),session.LocalReady?"WAITING FOR HOST":"GET READY",button,Muted);
            }
        }
    }
}
