using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// The host supplies the original catalog, owns eligibility/input and commits
// selections through native validation. Preview playback never commits a race choice.
public sealed class Idas3RaceMusicMenu : MonoBehaviour
{
    public struct Entry
    {
        public int id;
        public string key,title,artist,collection,previewPath,coverPath,pcmPath;
        public float duration;
        public float[] waveform;
        public int pcmRate,pcmChannels;
        public int stage;
    }
    private const float Width=1120,Height=664;
    private const int VisibleRows=8;
    // Display order is independent of persisted/native category IDs.
    private static readonly int[] StageIds={0,1,2,10,3,4,5,6,7,8,11,9};
    private static readonly string[] StageLabels={"All songs","Arcade Stage 1","Arcade Stage 2","Special Stage","Arcade Stage 3","Arcade Stage 4","Arcade Stage 5","Arcade Stage 6","Arcade Stage 7","Arcade Stage 8","The Arcade · S5","Custom music"};
    private static readonly Color Ink=new Color32(11,12,15,255),Panel=new Color32(22,24,29,255);
    private static readonly Color Edge=new Color32(54,57,65,255),White=new Color32(245,245,248,255);
    private static readonly Color Yellow=new Color32(246,98,109,255),Red=new Color32(222,31,52,255);
    private static readonly Color Muted=new Color32(154,159,170,255),Blue=new Color32(46,30,37,255);
    private Entry[] entries=Array.Empty<Entry>();
    private readonly List<int> visible=new List<int>();
    private int selectedId=-1,selection,firstRow,stageFilter,blockThroughFrame=-1;
    private int deleteId=-1;
    private bool deleteYes;
    private string deleteTitle="";
    private bool showHint,previousCursorVisible;
    internal bool WheelNavigation {get;set;}
    private CursorLockMode previousCursorLock;
    private float holdProgress;
    private string viewChangeLabel="VIEW CHANGE",notice="";
    private GUIStyle titleStyle,label,small,artistStyle,button,stageButton,numberStyle,confirmationStyle,songStyle;
    private GUIStyle searchStyle;
    private Font uiFont;
    internal Font DiagnosticFont=>uiFont;
    private string query="",collectionFilter="",preferencesPath;
    private bool favoritesOnly,sortByTitle,clearSearchFocus;
    private readonly HashSet<string> favorites=new HashSet<string>();
    private readonly Dictionary<string,Texture2D> covers=new Dictionary<string,Texture2D>();
    [Serializable] private sealed class Preferences {public string[] favorites;public float volume=.7f;}
    private Idas3MusicPreview preview;
    internal Idas3MusicPreview Preview=>preview;
    internal bool SearchFocused {get;private set;}
    internal string SearchQuery=>query;
    internal void BlurSearch(){clearSearchFocus=true;SearchFocused=false;}
    internal void Search(string value){query=value??"";RebuildList(false);}
    internal void ConfigurePlayer(string saveRoot)
    {
        var child=new GameObject("Sound Room Preview");child.transform.SetParent(transform,false);preview=child.AddComponent<Idas3MusicPreview>();
        preferencesPath=Path.Combine(saveRoot,"sound-room.json");
        try{if(File.Exists(preferencesPath)&&new FileInfo(preferencesPath).Length<128*1024){var p=JsonUtility.FromJson<Preferences>(File.ReadAllText(preferencesPath));if(p!=null){preview.Gain=float.IsNaN(p.volume)||float.IsInfinity(p.volume)?.7f:Mathf.Clamp(p.volume,0,Idas3GameOptions.MaximumVolume);if(p.favorites!=null)foreach(var key in p.favorites)if(!string.IsNullOrEmpty(key))favorites.Add(key);}}}catch(Exception){SetNotice("Sound Room preferences could not be loaded.");}
    }
    private void SavePreferences(){if(preferencesPath==null)return;try{var data=new Preferences{favorites=new List<string>(favorites).ToArray(),volume=preview.Gain};var temp=preferencesPath+".tmp";File.WriteAllText(temp,JsonUtility.ToJson(data));if(File.Exists(preferencesPath))File.Replace(temp,preferencesPath,null);else File.Move(temp,preferencesPath);}catch(Exception){SetNotice("Sound Room preferences could not be saved.");}}
    internal void TogglePreview(){if(!IsOpen||Busy||DeleteConfirmationOpen||visible.Count==0||preview==null)return;preview.Toggle(entries[visible[selection]]);}
    internal void StopPreview(){if(preview!=null)preview.Stop();}
    internal void ToggleFavorite(){if(visible.Count==0)return;var e=entries[visible[selection]];if(e.id<0||string.IsNullOrEmpty(e.key))return;if(!favorites.Add(e.key))favorites.Remove(e.key);SavePreferences();if(favoritesOnly)RebuildList(false);}
    private RenderTexture diagnosticTarget;
    public bool IsOpen {get;private set;}
    internal int OpenVersion {get;private set;}
    public bool Busy {get;set;}
    public bool DeleteConfirmationOpen=>deleteId>=1000;
    public bool DeleteYesSelected=>DeleteConfirmationOpen&&deleteYes;
    public int DeleteTrackId=>deleteId;
    public bool BlocksGameInput=>IsOpen||Time.frameCount<=blockThroughFrame;
    public bool HintVisible=>showHint&&!IsOpen;
    public float HoldProgress=>holdProgress;
    public int HighlightedTrackId=>visible.Count==0?-1:entries[visible[selection]].id;
    public int SelectedTrackId=>selectedId;
    public int StageFilter=>stageFilter;
    public int VisibleTrackCount=>visible.Count;
    public event Action<int> Selected;
    public event Action<int> DeleteRequested;
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
        CancelDelete();entries=(Entry[])catalog.Clone();selectedId=ids.Contains(currentId)?currentId:entries[0].id;stageFilter=0;RebuildList(true);
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
        ++OpenVersion;blockThroughFrame=Time.frameCount+1;IsOpen=open;notice="";CancelDelete();
        if(open){
            previousCursorVisible=Cursor.visible;previousCursorLock=Cursor.lockState;
            Cursor.lockState=CursorLockMode.None;Cursor.visible=true;
            stageFilter=0;query="";collectionFilter="";favoritesOnly=false;BlurSearch();RebuildList(true);
        }else{StopPreview();SavePreferences();BlurSearch();Cursor.lockState=previousCursorLock;Cursor.visible=previousCursorVisible;}
        OpenChanged?.Invoke(open);
    }
    public void Navigate(int delta)
    {
        if(!IsOpen||Busy||delta==0)return;
        if(DeleteConfirmationOpen){deleteYes=!deleteYes;return;}
        if(visible.Count==0)return;
        StopPreview();selection=Wrap(selection+Math.Sign(delta),visible.Count);EnsureVisible();notice="";
    }
    public void NavigateHorizontal(int stageDelta)
    {
        if(!IsOpen||Busy||stageDelta==0)return;
        if(DeleteConfirmationOpen){deleteYes=!deleteYes;return;}
        ChangeStage(StageIds[Wrap(Array.IndexOf(StageIds,stageFilter)+Math.Sign(stageDelta),StageIds.Length)]);
    }
    internal void NavigateDevice(int horizontal,int vertical,bool wheel){
        WheelNavigation=wheel;
        if(wheel){if(horizontal!=0)Navigate(horizontal);else if(vertical!=0)NavigateHorizontal(vertical);}
        else if(vertical!=0)Navigate(vertical);else if(horizontal!=0)NavigateHorizontal(horizontal);
    }
    public void Activate()
    {
        if(!IsOpen||Busy)return;
        if(DeleteConfirmationOpen){
            int id=deleteId;bool confirmed=deleteYes;CancelDelete();
            if(confirmed)DeleteRequested?.Invoke(id);
            return;
        }
        if(visible.Count==0)return;
        // The host closes only after its native selection setter succeeds.
        StopPreview();Selected?.Invoke(entries[visible[selection]].id);
    }
    public void RequestDelete(){
        if(!IsOpen||Busy||DeleteConfirmationOpen||HighlightedTrackId<1000||HighlightedTrackId>=2000)return;
        StopPreview();
        deleteId=HighlightedTrackId;deleteTitle=entries[visible[selection]].title;deleteYes=false;
    }
    private void CancelDelete(){deleteId=-1;deleteYes=false;deleteTitle="";}
    public void Back(){if(DeleteConfirmationOpen)CancelDelete();else if(SearchFocused)BlurSearch();else if(IsOpen)SetOpen(false);}
    private static int Wrap(int value,int count)=>(value%count+count)%count;
    private void ChangeStage(int stage)
    {
        if(stage==stageFilter&&!favoritesOnly&&collectionFilter.Length==0)return;
        StopPreview();stageFilter=stage;favoritesOnly=false;collectionFilter="";RebuildList(true);notice="";
    }
    private void RebuildList(bool preferCurrent)
    {
        StopPreview();visible.Clear();
        for(int i=0;i<entries.Length;++i){var e=entries[i];if((stageFilter==0||e.stage==stageFilter)&&
            (!favoritesOnly||(!string.IsNullOrEmpty(e.key)&&favorites.Contains(e.key)))&&
            (collectionFilter.Length==0||e.collection==collectionFilter)&&
            (query.Length==0||(e.title+" "+e.artist+" "+e.collection).IndexOf(query,StringComparison.OrdinalIgnoreCase)>=0))visible.Add(i);}
        if(sortByTitle)visible.Sort((a,b)=>string.Compare(entries[a].title,entries[b].title,StringComparison.CurrentCultureIgnoreCase));
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
        StopPreview();foreach(var cover in covers.Values)if(cover!=null)Destroy(cover);
        // Resources owns the shared font, including its runtime glyph atlas.
        // Closing/recreating this menu must not destroy it for the next instance.
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
        if(label!=null&&uiFont==Idas3MenuLocalization.Font)return;
        // Windows font families may not exist in a clean Wine/Proton prefix.
        // Ship the Japanese/Latin glyph data instead of relying on OS fallback.
        uiFont=Idas3MenuLocalization.Font;
        if(uiFont==null){
            Debug.LogError("Sound Room's bundled font is missing; using the built-in fallback.");
            uiFont=Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        }
        label=new GUIStyle(GUI.skin.label){font=uiFont,fontSize=18,padding=new RectOffset(0,0,0,0),clipping=TextClipping.Clip};label.normal.textColor=White;
        small=new GUIStyle(label){fontSize=12};artistStyle=new GUIStyle(label){fontSize=13};
        titleStyle=new GUIStyle(label){fontSize=35,fontStyle=FontStyle.BoldAndItalic};
        button=new GUIStyle(label){fontSize=15,fontStyle=FontStyle.Bold,alignment=TextAnchor.MiddleCenter};
        stageButton=new GUIStyle(button){fontSize=14,wordWrap=false};
        numberStyle=new GUIStyle(label){fontSize=12,fontStyle=FontStyle.Bold,alignment=TextAnchor.MiddleCenter};
        confirmationStyle=new GUIStyle(label){fontSize=26,fontStyle=FontStyle.Bold};
        songStyle=new GUIStyle(label){fontSize=17,wordWrap=true};
        searchStyle=new GUIStyle(GUI.skin.textField){font=uiFont,fontSize=16,padding=new RectOffset(10,10,7,7)};
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
    private void Text(Rect rect,string text,GUIStyle style,Color? color=null,bool translate=true)
    {
        var before=GUI.contentColor;GUI.contentColor=color??White;Idas3MenuLocalization.Label(rect,text,style,translate);GUI.contentColor=before;
    }
    private bool Button(Rect rect,string text,bool active=false,GUIStyle style=null)
    {
        bool enabled=GUI.enabled,hover=enabled&&rect.Contains(Event.current.mousePosition);
        Fill(rect,active&&enabled?Red:hover?Blue:Panel);Frame(rect,active&&enabled?Red:Edge);
        Text(rect,text,style??button,!enabled?Muted:White);return GUI.Button(rect,GUIContent.none,GUIStyle.none);
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
        // Depth belongs to this GUI behaviour. Restoring it would reset the
        // component's sorting order and let the lobby cover the picker.
        Styles();var beforeMatrix=GUI.matrix;GUI.depth=-12500;
        bool diagnostic=diagnosticTarget!=null&&Event.current.type==EventType.Repaint;
        var beforeTarget=RenderTexture.active;
        if(Event.current.type==EventType.Repaint)++DiagnosticRepaints;
        if(diagnostic){RenderTexture.active=diagnosticTarget;GL.PushMatrix();GL.LoadPixelMatrix(0,Screen.width,Screen.height,0);}
        try{
            if(IsOpen)DrawPicker();else DrawHint();
            if(IsOpen&&(Event.current.type==EventType.KeyDown||Event.current.type==EventType.KeyUp))Event.current.Use();
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
        Text(new Rect(8,1,width-16,height-2),Idas3MenuLocalization.Format("HOLD {0}  /  MUSIC SELECT",viewChangeLabel.ToUpperInvariant()),button,Yellow);
        if(holdProgress>0){Fill(new Rect(0,height-2,width,2),Edge);Fill(new Rect(0,height-2,width*holdProgress,2),Yellow);}
    }
    private void DrawPicker()
    {
        var safe=SafeRect();float scale=Mathf.Min(1.5f,Mathf.Min(safe.width/(Width+40),safe.height/(Height+40)));
        Fill(new Rect(0,0,Screen.width,Screen.height),new Color(0,0,0,.75f));
        GUI.matrix=Matrix4x4.TRS(new Vector3(safe.x+(safe.width-Width*scale)*.5f,safe.y+(safe.height-Height*scale)*.5f,0),Quaternion.identity,new Vector3(scale,scale,1));
        Fill(new Rect(0,0,Width,Height),Ink);Frame(new Rect(0,0,Width,Height),Edge);Fill(new Rect(0,0,Width,4),Red);
        Text(new Rect(26,17,500,48),"SOUND ROOM",titleStyle);
        Text(new Rect(28,64,650,20),"RACE MUSIC",small,Muted);
        if(clearSearchFocus){GUI.FocusControl(null);clearSearchFocus=false;}
        bool beforeEnabled=GUI.enabled;GUI.enabled=beforeEnabled&&!DeleteConfirmationOpen;
        if(Button(new Rect(1054,24,40,36),"×"))SetOpen(false);
        GUI.enabled=beforeEnabled&&!Busy&&!DeleteConfirmationOpen;
        Fill(new Rect(24,96,1072,1),Edge);
        Text(new Rect(28,110,166,20),"LIBRARY",small,Muted);
        DiagnosticStageLabelsFit=true;
        for(int i=0;i<StageIds.Length;++i){
            var rect=new Rect(24,141+i*31,172,29);bool active=stageFilter==StageIds[i]&&!favoritesOnly;
            Fill(rect,active?Panel:Ink);if(active)Fill(new Rect(rect.x,rect.y,3,rect.height),Red);
            Text(new Rect(rect.x+12,rect.y+6,rect.width-20,20),StageLabels[i],artistStyle,active?White:Muted);
            DiagnosticStageLabelsFit&=Idas3MenuLocalization.Style(artistStyle).CalcSize(new GUIContent(Idas3MenuLocalization.T(StageLabels[i]))).x<rect.width-20;
            if(GUI.Button(rect,GUIContent.none,GUIStyle.none)){BlurSearch();ChangeStage(StageIds[i]);}
        }
        if(Button(new Rect(24,522,172,32),"FAVORITES",favoritesOnly)){BlurSearch();favoritesOnly=!favoritesOnly;stageFilter=0;collectionFilter="";RebuildList(false);}
        if(Button(new Rect(24,566,172,34),"+ ADD MUSIC")){BlurSearch();StopPreview();Selected?.Invoke(Idas3CustomRaceMusic.AddId);}
        Fill(new Rect(212,110,1,490),Edge);
        GUI.SetNextControlName("SoundRoomSearch");
        var changed=GUI.TextField(new Rect(230,110,389,36),query,120,searchStyle);
        SearchFocused=GUI.GetNameOfFocusedControl()=="SoundRoomSearch";
        if(changed!=query)Search(changed);
        if(query.Length==0&&!SearchFocused)Text(new Rect(241,119,368,23),"Search songs or artists",artistStyle,Muted);
        if(Button(new Rect(629,110,137,36),sortByTitle?"TITLE A–Z":"TRACK ORDER")){BlurSearch();sortByTitle=!sortByTitle;RebuildList(true);}
        string heading=collectionFilter.Length>0?collectionFilter:favoritesOnly?"Favorites":StageLabels[Array.IndexOf(StageIds,stageFilter)];
        Text(new Rect(232,159,380,22),(collectionFilter.Length>0?heading:Idas3MenuLocalization.T(heading))+"  /  "+visible.Count,artistStyle,Muted);
        if(stageFilter==11&&Button(new Rect(614,153,152,30),"COLLECTION ›")){
            var names=new List<string>{""};foreach(var entry in entries)if(entry.stage==11&&!names.Contains(entry.collection))names.Add(entry.collection);
            collectionFilter=names[Wrap(names.IndexOf(collectionFilter)+1,names.Count)];RebuildList(false);BlurSearch();
        }
        var listRect=new Rect(230,192,550,400);
        if(GUI.enabled&&Event.current.type==EventType.ScrollWheel&&listRect.Contains(Event.current.mousePosition)){
            firstRow=Mathf.Clamp(firstRow+(int)Mathf.Sign(Event.current.delta.y)*3,0,Math.Max(0,visible.Count-VisibleRows));Event.current.Use();
        }
        for(int row=0;row<VisibleRows&&firstRow+row<visible.Count;++row)DrawRow(firstRow+row,row);
        if(visible.Count==0)Text(new Rect(240,315,510,55),"No songs found",button,Muted);
        // Always visible: a full thumb for one page, proportionally shorter for longer lists.
        firstRow=Mathf.RoundToInt(GUI.VerticalScrollbar(new Rect(773,192,10,398),firstRow,VisibleRows,0,Math.Max(VisibleRows,visible.Count)));
        Fill(new Rect(800,110,1,490),Edge);
        DrawPlayer();
        GUI.enabled=beforeEnabled;
        Fill(new Rect(24,614,1072,1),Edge);
        string message=Busy?"Loading music…":!string.IsNullOrEmpty(notice)?notice:preview!=null&&preview.Error!=null?preview.Error:
            WheelNavigation?"STEERING SELECT   ACCEL CONFIRM   BRAKE BACK":"↑ ↓ SONG   ← → COLLECTION   ENTER / A SELECT   P / Y PREVIEW   ESC / B BACK";
        Text(new Rect(28,631,1056,23),message,small,string.IsNullOrEmpty(notice)?Muted:Yellow);
        if(DeleteConfirmationOpen)DrawDeleteConfirmation();
    }
    private static string DurationText(float seconds)=>seconds<=0?"—":((int)seconds/60)+":"+((int)seconds%60).ToString("00");
    private bool Previewable(Entry entry)=>!string.IsNullOrEmpty(entry.previewPath)||!string.IsNullOrEmpty(entry.pcmPath);
    private Texture2D Cover(string path)
    {
        if(string.IsNullOrEmpty(path))return null;if(covers.TryGetValue(path,out var cached))return cached;
        Texture2D texture=null;
        try{if(File.Exists(path)&&new FileInfo(path).Length<8*1024*1024){texture=new Texture2D(2,2,TextureFormat.RGBA32,false);if(!texture.LoadImage(File.ReadAllBytes(path))){Destroy(texture);texture=null;}}}catch(Exception){if(texture!=null)Destroy(texture);texture=null;}
        covers[path]=texture;return texture;
    }
    private void DrawPlayer()
    {
        if(visible.Count==0){Text(new Rect(820,155,276,40),"Choose a song",button,Muted);return;}
        var e=entries[visible[selection]];var artwork=new Rect(850,111,216,216);var cover=Cover(e.coverPath);
        Fill(artwork,Panel);
        if(cover!=null)GUI.DrawTexture(artwork,cover,ScaleMode.ScaleToFit,true);
        else{
            Fill(new Rect(850,111,216,4),Red);
            Text(new Rect(870,157,176,76),e.id==-1?"AUTOMATIC":e.id==Idas3CustomRaceMusic.AddId?"ADD MUSIC":e.stage==9?"CUSTOM\nMUSIC":e.stage==10?"SPECIAL\nSTAGE":Idas3MenuLocalization.Format("ARCADE\nSTAGE {0}",e.stage),confirmationStyle);
            Text(new Rect(870,288,176,24),"INITIAL D",small,Muted);
        }
        Text(new Rect(822,339,270,46),e.id<0||e.id==Idas3CustomRaceMusic.AddId?Idas3MenuLocalization.T(e.title):e.title,songStyle,translate:false);
        Text(new Rect(822,387,270,20),e.id<0||e.id==Idas3CustomRaceMusic.AddId?Idas3MenuLocalization.T(e.artist):e.artist,artistStyle,Muted,false);
        bool own=preview!=null&&preview.TrackId==e.id;
        float duration=own?preview.Duration:0,position=own?preview.Position:0;
        var waveform=own&&preview.Waveform!=null?preview.Waveform:e.waveform;
        var waveRect=new Rect(822,419,270,44);Fill(waveRect,Panel);
        if(waveform!=null&&waveform.Length>0){float step=waveRect.width/waveform.Length;for(int i=0;i<waveform.Length;++i){float h=Mathf.Clamp(waveform[i],.055f,1)*36;Fill(new Rect(waveRect.x+i*step,waveRect.center.y-h/2,Math.Max(1,step-1),h),duration>0&&i/(float)waveform.Length<=position/duration?Red:Muted);}}
        if(GUI.enabled&&duration>0&&Event.current.type==EventType.MouseDown&&waveRect.Contains(Event.current.mousePosition)){preview.Seek(duration*(Event.current.mousePosition.x-waveRect.x)/waveRect.width);Event.current.Use();BlurSearch();}
        Text(new Rect(822,468,138,20),(position>0?DurationText(position):"0:00")+" / "+(duration>0?DurationText(duration):Idas3MenuLocalization.T("PREVIEW")),small,Muted);
        Text(new Rect(981,468,111,20),Idas3MenuLocalization.Format("{0} FULL",DurationText(e.duration)),small,Muted);
        bool enabled=GUI.enabled;
        GUI.enabled=enabled&&Previewable(e);
        if(Button(new Rect(822,494,42,34),"‹")){Navigate(-1);TogglePreview();BlurSearch();}
        if(Button(new Rect(872,494,170,34),own&&preview.Loading?"LOADING…":own&&preview.Playing?"PAUSE":"PLAY PREVIEW",own&&preview.Playing)){TogglePreview();BlurSearch();}
        if(Button(new Rect(1050,494,42,34),"›")){Navigate(1);TogglePreview();BlurSearch();}
        GUI.enabled=enabled;
        if(preview!=null){Text(new Rect(822,540,67,22),"VOLUME",small,Muted);preview.Gain=GUI.HorizontalSlider(new Rect(893,545,137,18),preview.Gain,0,Idas3GameOptions.MaximumVolume);Text(new Rect(1040,540,52,22),Mathf.RoundToInt(preview.Gain*100)+"%",small,Muted);}
        bool custom=e.id>=1000&&e.id<2000;
        if(custom&&Button(new Rect(822,566,86,34),"DELETE")){BlurSearch();RequestDelete();}
        if(Button(new Rect(custom?918:822,566,custom?174:270,34),e.id==Idas3CustomRaceMusic.AddId?"ADD MUSIC":"USE FOR RACE",true)){BlurSearch();Activate();}
    }
    private void DrawDeleteConfirmation()
    {
        Fill(new Rect(1,5,Width-2,Height-6),new Color(0,0,0,.8f));
        const float x=318,y=220,width=484,height=220;
        Fill(new Rect(x,y,width,height),Ink);Frame(new Rect(x,y,width,height),Edge);Fill(new Rect(x,y,width,3),Red);
        Text(new Rect(x+22,y+20,width-44,36),"Delete song?",confirmationStyle);
        Text(new Rect(x+22,y+69,width-44,50),deleteTitle,songStyle,Muted,false);
        bool beforeEnabled=GUI.enabled;GUI.enabled=beforeEnabled&&!Busy;
        if(Button(new Rect(x+126,y+156,104,40),"NO",!deleteYes)){deleteYes=false;Activate();}
        if(Button(new Rect(x+244,y+156,104,40),"YES",deleteYes)){deleteYes=true;Activate();}
        GUI.enabled=beforeEnabled;
    }
    private void DrawRow(int index,int row)
    {
        var e=entries[visible[index]];bool focus=index==selection,current=e.id==selectedId;
        var rect=new Rect(230,192+row*50,536,48);
        Fill(rect,focus?Blue:Panel);if(focus)Fill(new Rect(rect.x,rect.y,3,rect.height),Red);
        if(GUI.Button(new Rect(278,rect.y,440,rect.height),GUIContent.none,GUIStyle.none)){if(selection!=index)StopPreview();selection=index;BlurSearch();}
        bool enabled=GUI.enabled;GUI.enabled=enabled&&Previewable(e);
        if(Button(new Rect(238,rect.y+9,30,30),preview!=null&&preview.TrackId==e.id&&preview.Playing?"Ⅱ":"▶")){selection=index;TogglePreview();BlurSearch();}
        GUI.enabled=enabled;
        Text(new Rect(278,rect.y+3,388,25),e.id<0||e.id==Idas3CustomRaceMusic.AddId?Idas3MenuLocalization.T(e.title):e.title,label,translate:false);
        Text(new Rect(279,rect.y+28,342,19),e.id<0||e.id==Idas3CustomRaceMusic.AddId?Idas3MenuLocalization.T(e.artist):e.artist,small,Muted,false);
        Text(new Rect(650,rect.y+28,74,19),current?"SELECTED":DurationText(e.duration),small,current?Yellow:Muted);
        if(e.id>=0&&!string.IsNullOrEmpty(e.key)&&Button(new Rect(727,rect.y+9,30,30),favorites.Contains(e.key)?"★":"☆")){if(selection!=index)StopPreview();selection=index;ToggleFavorite();BlurSearch();}
    }
}
