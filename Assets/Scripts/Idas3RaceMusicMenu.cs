using System;
using System.Collections.Generic;
using UnityEngine;

// The host supplies the original catalog, owns eligibility/input and commits
// selections through native validation. This view never starts audio itself.
public sealed class Idas3RaceMusicMenu : MonoBehaviour
{
    public struct Entry
    {
        public int id;
        public string title,artist;
        public int stage;
    }
    private const float Width=800,Height=592;
    private const int VisibleRows=8;
    private static readonly string[] StageLabels={"ALL","STAGE 1","STAGE 2","STAGE 3","STAGE 4","STAGE 5","STAGE 6","STAGE 7","STAGE 8","CUSTOM"};
    private static readonly Color Ink=new Color32(5,10,18,255),Panel=new Color32(13,24,35,255);
    private static readonly Color Edge=new Color32(94,126,142,255),White=new Color32(245,245,238,255);
    private static readonly Color Yellow=new Color32(255,221,44,255),Red=new Color32(188,13,13,255);
    private static readonly Color Muted=new Color32(174,188,195,255),Blue=new Color32(28,67,91,255);
    private Entry[] entries=Array.Empty<Entry>();
    private readonly List<int> visible=new List<int>();
    private int selectedId=-1,selection,firstRow,stageFilter,blockThroughFrame=-1;
    private bool showHint,previousCursorVisible;
    internal bool WheelNavigation {get;set;}
    private CursorLockMode previousCursorLock;
    private float holdProgress;
    private string viewChangeLabel="VIEW CHANGE",notice="";
    private GUIStyle titleStyle,label,small,artistStyle,button,stageButton,numberStyle;
    private RenderTexture diagnosticTarget;
    public bool IsOpen {get;private set;}
    public bool BlocksGameInput=>IsOpen||Time.frameCount<=blockThroughFrame;
    public bool HintVisible=>showHint&&!IsOpen;
    public float HoldProgress=>holdProgress;
    public int HighlightedTrackId=>visible.Count==0?-1:entries[visible[selection]].id;
    public int SelectedTrackId=>selectedId;
    public int StageFilter=>stageFilter;
    public int VisibleTrackCount=>visible.Count;
    public event Action<int> Selected;
    public event Action<bool> OpenChanged;
    public bool DiagnosticCaptureReady {get;private set;}
    public int DiagnosticRepaints {get;private set;}
    public bool DiagnosticStageLabelsFit {get;private set;}

    public void Initialize(Entry[] catalog,int currentId)
    {
        if(catalog==null||catalog.Length==0)throw new ArgumentException("A race music catalog is required.",nameof(catalog));
        var ids=new HashSet<int>();
        foreach(var entry in catalog)
            if(!ids.Add(entry.id)||entry.stage<0||entry.stage>=StageLabels.Length||string.IsNullOrWhiteSpace(entry.title))
                throw new ArgumentException("The race music catalog contains an invalid or duplicate entry.",nameof(catalog));
        entries=(Entry[])catalog.Clone();selectedId=currentId;stageFilter=0;RebuildList(true);
    }
    public void SetContext(bool showOpponentHint,float progress,string controlLabel)
    {
        showHint=showOpponentHint;holdProgress=float.IsNaN(progress)?0:Mathf.Clamp01(progress);
        viewChangeLabel=string.IsNullOrWhiteSpace(controlLabel)?"VIEW CHANGE":controlLabel;
    }
    public void ReplaceCatalog(Entry[] catalog,int currentId){int filter=stageFilter;Initialize(catalog,currentId);stageFilter=filter;RebuildList(true);}
    public void ShowCustom(){ChangeStage(9);}
    public void SetSelected(int id)
    {
        selectedId=id;
        if(!IsOpen)RebuildList(true);
    }
    public void SetNotice(string text){notice=text??"";}
    public void SetOpen(bool open)
    {
        if(open==IsOpen)return;
        if(open&&entries.Length==0)return;
        blockThroughFrame=Time.frameCount+1;IsOpen=open;notice="";
        if(open){
            previousCursorVisible=Cursor.visible;previousCursorLock=Cursor.lockState;
            Cursor.lockState=CursorLockMode.None;Cursor.visible=true;
            stageFilter=0;RebuildList(true);
        }else{Cursor.lockState=previousCursorLock;Cursor.visible=previousCursorVisible;}
        OpenChanged?.Invoke(open);
    }
    public void Navigate(int delta)
    {
        if(!IsOpen||delta==0||visible.Count==0)return;
        selection=Wrap(selection+Math.Sign(delta),visible.Count);EnsureVisible();notice="";
    }
    public void NavigateHorizontal(int stageDelta)
    {
        if(!IsOpen||stageDelta==0)return;
        ChangeStage(Wrap(stageFilter+Math.Sign(stageDelta),StageLabels.Length));
    }
    internal void NavigateDevice(int horizontal,int vertical,bool wheel){
        WheelNavigation=wheel;
        if(wheel){if(horizontal!=0)Navigate(horizontal);else if(vertical!=0)NavigateHorizontal(vertical);}
        else if(vertical!=0)Navigate(vertical);else if(horizontal!=0)NavigateHorizontal(horizontal);
    }
    public void Activate()
    {
        if(!IsOpen||visible.Count==0)return;
        // The host closes only after its native selection setter succeeds.
        Selected?.Invoke(entries[visible[selection]].id);
    }
    public void Back(){if(IsOpen)SetOpen(false);}
    private static int Wrap(int value,int count)=>(value%count+count)%count;
    private void ChangeStage(int stage)
    {
        if(stage==stageFilter)return;
        stageFilter=stage;RebuildList(true);notice="";
    }
    private void RebuildList(bool preferCurrent)
    {
        visible.Clear();
        for(int i=0;i<entries.Length;++i)if(stageFilter==0||entries[i].stage==stageFilter)visible.Add(i);
        selection=0;firstRow=0;
        if(preferCurrent)for(int i=0;i<visible.Count;++i)if(entries[visible[i]].id==selectedId){selection=i;break;}
        EnsureVisible();
    }
    private void EnsureVisible()
    {
        if(selection<firstRow)firstRow=selection;
        if(selection>=firstRow+VisibleRows)firstRow=selection-VisibleRows+1;
        firstRow=Math.Max(0,Math.Min(firstRow,Math.Max(0,visible.Count-VisibleRows)));
    }
    private void OnDestroy()
    {
        if(IsOpen){Cursor.lockState=previousCursorLock;Cursor.visible=previousCursorVisible;}
    }
    public void RequestDiagnosticCapture(RenderTexture target)
    {
        if(target==null)throw new ArgumentNullException(nameof(target));
        diagnosticTarget=target;DiagnosticCaptureReady=false;
    }
    public void CancelDiagnosticCapture(){diagnosticTarget=null;}
    private void Styles()
    {
        if(label!=null)return;
        label=new GUIStyle(GUI.skin.label){fontSize=18,padding=new RectOffset(0,0,0,0),clipping=TextClipping.Clip};label.normal.textColor=White;
        small=new GUIStyle(label){fontSize=12};artistStyle=new GUIStyle(label){fontSize=13};
        titleStyle=new GUIStyle(label){fontSize=35,fontStyle=FontStyle.BoldAndItalic};
        button=new GUIStyle(label){fontSize=15,fontStyle=FontStyle.Bold,alignment=TextAnchor.MiddleCenter};
        stageButton=new GUIStyle(button){fontSize=14,wordWrap=false};
        numberStyle=new GUIStyle(label){fontSize=12,fontStyle=FontStyle.Bold,alignment=TextAnchor.MiddleCenter};
    }
    private static void Fill(Rect rect,Color color)
    {
        var before=GUI.color;GUI.color=color;GUI.DrawTexture(rect,Texture2D.whiteTexture);GUI.color=before;
    }
    private static void Frame(Rect rect,Color color)
    {
        Fill(new Rect(rect.x,rect.y,rect.width,1),color);Fill(new Rect(rect.x,rect.yMax-1,rect.width,1),color);
        Fill(new Rect(rect.x,rect.y,1,rect.height),color);Fill(new Rect(rect.xMax-1,rect.y,1,rect.height),color);
    }
    private void Text(Rect rect,string text,GUIStyle style,Color? color=null)
    {
        var before=GUI.contentColor;GUI.contentColor=color??White;GUI.Label(rect,text??"",style);GUI.contentColor=before;
    }
    private bool Button(Rect rect,string text,bool active=false,GUIStyle style=null)
    {
        bool hover=rect.Contains(Event.current.mousePosition);Fill(rect,active?Yellow:hover?Blue:Panel);Frame(rect,active?Yellow:Edge);
        Text(rect,text,style??button,active?Ink:White);return GUI.Button(rect,GUIContent.none,GUIStyle.none);
    }
    private static Rect SafeRect()
    {
        var safe=Screen.safeArea;
        if(safe.width<=0||safe.height<=0)return new Rect(0,0,Screen.width,Screen.height);
        return new Rect(safe.x,Screen.height-safe.yMax,safe.width,safe.height);
    }
    private void OnGUI()
    {
        if(entries.Length==0||(!IsOpen&&!showHint))return;
        if(IsOpen&&(Event.current.type==EventType.KeyDown||Event.current.type==EventType.KeyUp))Event.current.Use();
        // Depth belongs to this GUI behaviour. Restoring it would reset the
        // component's sorting order and let the lobby cover the picker.
        Styles();var beforeMatrix=GUI.matrix;GUI.depth=-12500;
        bool diagnostic=diagnosticTarget!=null&&Event.current.type==EventType.Repaint;
        var beforeTarget=RenderTexture.active;
        if(Event.current.type==EventType.Repaint)++DiagnosticRepaints;
        if(diagnostic){RenderTexture.active=diagnosticTarget;GL.PushMatrix();GL.LoadPixelMatrix(0,Screen.width,Screen.height,0);}
        try{
            if(IsOpen)DrawPicker();else DrawHint();
        }finally{
            GUI.matrix=beforeMatrix;
            if(diagnostic){GL.PopMatrix();RenderTexture.active=beforeTarget;diagnosticTarget=null;DiagnosticCaptureReady=true;}
        }
    }
    private void DrawHint()
    {
        var safe=SafeRect();float scale=Mathf.Min(1.5f,Mathf.Min(safe.width/1000f,safe.height/720f));
        const float width=540,height=28;
        float x=safe.x+(safe.width-width*scale)*.5f,y=safe.yMax-(height+13)*scale;
        GUI.matrix=Matrix4x4.TRS(new Vector3(x,y,0),Quaternion.identity,new Vector3(scale,scale,1));
        Fill(new Rect(0,0,width,height),new Color(0,0,0,.72f));
        Text(new Rect(8,1,width-16,height-2),"HOLD "+viewChangeLabel.ToUpperInvariant()+"  /  MUSIC SELECT",button,Yellow);
        if(holdProgress>0){Fill(new Rect(0,height-2,width,2),Edge);Fill(new Rect(0,height-2,width*holdProgress,2),Yellow);}
    }
    private void DrawPicker()
    {
        var safe=SafeRect();float scale=Mathf.Min(1.5f,Mathf.Min(safe.width/(Width+48),safe.height/(Height+48)));
        Fill(new Rect(0,0,Screen.width,Screen.height),new Color(0,0,0,.68f));
        GUI.matrix=Matrix4x4.TRS(new Vector3(safe.x+(safe.width-Width*scale)*.5f,safe.y+(safe.height-Height*scale)*.5f,0),Quaternion.identity,new Vector3(scale,scale,1));
        Fill(new Rect(0,0,Width,Height),Ink);Frame(new Rect(0,0,Width,Height),White);
        // Slanted white/red header treatment follows the opponent screen.
        Fill(new Rect(1,1,Width-2,66),Red);
        for(int i=0;i<18;++i)Fill(new Rect(Width-136+i*3,66+i,135-i*3,1),Red);
        Fill(new Rect(0,66,Width-136,2),White);
        Text(new Rect(25,9,590,48),"Select BGM",titleStyle);
        Text(new Rect(596,24,175,24),"RACE MUSIC",button,Yellow);
        float tabWidth=(Width-44-4*(StageLabels.Length-1))/StageLabels.Length;
        DiagnosticStageLabelsFit=true;
        for(int i=0;i<StageLabels.Length;++i){
            var rect=new Rect(22+i*(tabWidth+4),91,tabWidth,32);
            var content=new GUIContent(StageLabels[i]);stageButton.fontSize=14;
            while(stageButton.fontSize>10&&stageButton.CalcSize(content).x>rect.width-10)--stageButton.fontSize;
            var measured=stageButton.CalcSize(content);
            DiagnosticStageLabelsFit&=measured.x<=rect.width-10&&measured.y<=rect.height&&rect.xMin>=22&&rect.xMax<=Width-21.9f;
            if(Button(rect,StageLabels[i],stageFilter==i,stageButton))ChangeStage(i);
        }
        if(Event.current.type==EventType.ScrollWheel&&new Rect(20,134,760,384).Contains(Event.current.mousePosition)){
            Navigate(Event.current.delta.y>=0?1:-1);Event.current.Use();
        }
        for(int row=0;row<VisibleRows&&firstRow+row<visible.Count;++row)DrawRow(firstRow+row,row);
        if(visible.Count==0)Text(new Rect(30,223,735,36),"No tracks in this stage.",button,Muted);
        if(visible.Count>VisibleRows){
            Fill(new Rect(783,134,3,384),Blue);
            float thumb=384f*VisibleRows/visible.Count,offset=(384-thumb)*firstRow/(visible.Count-VisibleRows);
            Fill(new Rect(783,134+offset,3,thumb),Yellow);
        }
        Fill(new Rect(22,527,756,1),Edge);
        Text(new Rect(24,535,557,20),string.IsNullOrEmpty(notice)?"Your choice plays during the race. Each driver chooses their own music.":notice,small,string.IsNullOrEmpty(notice)?Muted:Yellow);
        if(Button(new Rect(600,539,83,34),"SELECT",true))Activate();
        if(Button(new Rect(693,539,84,34),"BACK"))Back();
        Text(new Rect(24,562,565,20),WheelNavigation?"STEERING SELECT    ACCEL CONFIRM    BRAKE BACK":"↑ ↓ SELECT    ← → STAGE    ENTER / A OK    ESC / B BACK",small,Muted);
    }
    private void DrawRow(int index,int row)
    {
        var entry=entries[visible[index]];bool focus=index==selection,current=entry.id==selectedId;
        var rect=new Rect(22,134+row*48,754,46);
        Fill(rect,focus?Blue:Panel);if(focus){Frame(rect,Yellow);Fill(new Rect(rect.x,rect.y,4,rect.height),Yellow);}
        else Fill(new Rect(rect.x,rect.yMax-1,rect.width,1),new Color32(36,54,64,255));
        Text(new Rect(30,rect.y+13,42,23),entry.stage==0?"AUTO":entry.stage==9?"USER":"S"+entry.stage,numberStyle,focus?Yellow:Muted);
        Text(new Rect(83,rect.y+4,682,23),entry.title,label,focus?Yellow:White);
        Text(new Rect(84,rect.y+27,571,18),entry.artist,artistStyle,Muted);
        if(current)Text(new Rect(661,rect.y+27,104,18),"SELECTED",numberStyle,Yellow);
        if(GUI.Button(rect,GUIContent.none,GUIStyle.none)){selection=index;EnsureVisible();Activate();}
    }
}
