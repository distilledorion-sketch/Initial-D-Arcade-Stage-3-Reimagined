using System;
using UnityEngine;

// Appearance has a private working copy. Previewing and cancelling never alter
// the live race, the main settings draft, or the saved options file.
public sealed class Idas3HudCustomization : MonoBehaviour
{
    const float Width=1040,Height=680;
    const int VisibleChoices=9,ChoiceRows=5,LayoutRow=8;
    static readonly int[] NavigationRows={0,1,2,3,4,5,LayoutRow,6,7};
    static readonly int[] ActionRows={5,LayoutRow,6,7};
    static readonly Color Ink=new Color32(11,12,15,255),Panel=new Color32(22,24,29,255);
    static readonly Color Raised=new Color32(34,36,42,255),Edge=new Color32(65,68,76,255);
    static readonly Color Red=new Color32(222,35,49,255),Muted=new Color32(164,166,175,255);
    static readonly string[] Labels={"METER","SHIFT LIGHTS","PEDAL INPUTS","DRIVER PLATE","ORNAMENT"};
    static readonly string[] Help={
        "Choose a meter style.",
        "Show a warning as the engine approaches its rev limit.",
        "Show accelerator and brake inputs.",
        "Show your driver name above the meter.",
        "Choose a hanging ornament."
    };
    Idas3GameOptions options;
    Idas3PauseMenu menu;
    Idas3GameOptions.Values working;
    GUIStyle title,label,small,wrapped,button,pickerLabel;
    string error="";
    int selection,previewRow;
    bool layoutEdited;
    bool pickerOpen;
    int pickerIndex;
    float pickerScroll;
    float previewStart;
    public bool IsOpen {get;private set;}
    internal Idas3GameOptions.Values Draft=>working;
    internal int SelectedRow=>selection;
    internal bool PickerOpen=>pickerOpen;
    internal int PickerIndex=>pickerIndex;
    internal float PickerScroll=>pickerScroll;
    internal bool OrnamentPicker=>pickerOpen&&selection==4;
    bool OrnamentSelected=>selection==4||(selection>=ChoiceRows&&previewRow==4);
    int PickerCount=>OrnamentSelected?Idas3OrnamentCatalog.Count:Idas3ArcadeMeterCatalog.Count;
    int CurrentIndex=>OrnamentSelected?Idas3OrnamentCatalog.IndexOf(working.hudOrnamentId):Idas3ArcadeMeterCatalog.IndexOfStyle(working.hudMeterStyle);
    string PickerName(int index)=>OrnamentSelected?Idas3OrnamentCatalog.Name(Idas3OrnamentCatalog.IdAt(index)):Idas3ArcadeMeterCatalog.Name(Idas3ArcadeMeterCatalog.StyleAt(index));

    public void Open(Idas3GameOptions owner,Idas3PauseMenu parent)
    {
        if(IsOpen)return;
        options=owner??throw new ArgumentNullException(nameof(owner));
        menu=parent??throw new ArgumentNullException(nameof(parent));
        working=Idas3GameOptions.Normalize(owner.Draft);selection=0;previewRow=0;layoutEdited=false;error="";pickerOpen=false;pickerScroll=0;
        previewStart=Time.unscaledTime;IsOpen=true;
    }
    bool Available(int row)=>row==0||row>=4||working.hudMeterStyle!=0;
    public void Navigate(int direction)
    {
        if(!IsOpen||direction==0)return;
        if(pickerOpen){BrowseChoice(direction);return;}
        int at=Array.IndexOf(NavigationRows,selection);
        do{at=(at+Math.Sign(direction)+NavigationRows.Length)%NavigationRows.Length;selection=NavigationRows[at];}while(!Available(selection));
        if(selection==0||selection==4)previewRow=selection;
    }
    public void NavigateHorizontal(int direction)
    {
        if(!IsOpen||direction==0)return;
        if(pickerOpen){BrowseChoice(direction);return;}
        if(selection>=ChoiceRows){int at=Array.IndexOf(ActionRows,selection);selection=ActionRows[(at+Math.Sign(direction)+ActionRows.Length)%ActionRows.Length];return;}
        Adjust(selection,direction);
    }
    public void Activate()
    {
        if(!IsOpen)return;
        if(pickerOpen){pickerOpen=false;return;}
        if(selection==0||selection==4)OpenPicker(selection);
        else if(selection<ChoiceRows)Adjust(selection,1);
        else if(selection==ChoiceRows){Idas3GameOptions.CopyHudCustomization(new Idas3GameOptions.Values(),working);error="";}
        else if(selection==LayoutRow)OpenLayout();
        else Close(selection==ChoiceRows+1);
    }
    public void Back(){if(pickerOpen)pickerOpen=false;else Close(false);}
    void OpenLayout()
    {
        pickerOpen=false;
        menu.OpenHudEditor(working,(saved,layout)=>{
            if(!IsOpen||!saved)return;
            Idas3GameOptions.CopyHudLayout(layout,working);layoutEdited=true;error="";
        },OrnamentSelected?9:1);
    }
    void OpenPicker(int row)
    {
        selection=row;previewRow=row;pickerOpen=true;pickerIndex=Math.Max(0,CurrentIndex);pickerScroll=0;
        RevealChoice();
    }
    void BrowseChoice(int direction)
    {
        int count=PickerCount;
        if(count==0)return;
        SelectChoice((pickerIndex+Math.Sign(direction)+count)%count);RevealChoice();
    }
    void SelectChoice(int index)
    {
        pickerIndex=index;
        if(OrnamentSelected)working.hudOrnamentId=Idas3OrnamentCatalog.IdAt(index);
        else working.hudMeterStyle=Idas3ArcadeMeterCatalog.StyleAt(index);
        error="";
    }
    void RevealChoice()
    {
        int first=Mathf.FloorToInt(pickerScroll);
        if(pickerIndex<first)first=pickerIndex;
        else if(pickerIndex>=first+VisibleChoices)first=pickerIndex-VisibleChoices+1;
        pickerScroll=Mathf.Clamp(first,0,Math.Max(0,PickerCount-VisibleChoices));
    }
    void Adjust(int row,int direction)
    {
        if(!Available(row))return;
        selection=row;if(row==0||row==4)previewRow=row;error="";
        if(row==0||row==4){pickerIndex=Math.Max(0,CurrentIndex);BrowseChoice(direction);}
        else if(row==1)working.hudShiftLights=!working.hudShiftLights;
        else if(row==2)working.hudPedalIndicators=!working.hudPedalIndicators;
        else if(row==3)working.hudNameplateStyle=1-working.hudNameplateStyle;
    }
    public void Close(bool save)
    {
        if(!IsOpen)return;
        if(save&&!options.ApplyHudCustomization(working,layoutEdited)){error=options.LastError??"Could not save HUD customization.";return;}
        IsOpen=false;pickerOpen=false;Idas3ArcadeHud.ReleasePreview();Idas3OrnamentRenderer.ReleasePreview();menu?.HudCustomizationClosed(save);
    }
    void OnDestroy(){Close(false);}

    void Styles()
    {
        if(label!=null)return;
        label=new GUIStyle(GUI.skin.label){fontSize=16,fontStyle=FontStyle.Bold,padding=new RectOffset(),clipping=TextClipping.Clip};
        label.normal.textColor=Color.white;
        title=new GUIStyle(label){fontSize=40,fontStyle=FontStyle.BoldAndItalic};
        small=new GUIStyle(label){fontSize=13,fontStyle=FontStyle.Normal};
        wrapped=new GUIStyle(small){wordWrap=true};
        button=new GUIStyle(label){alignment=TextAnchor.MiddleCenter};
        pickerLabel=new GUIStyle(label){fontSize=14,alignment=TextAnchor.MiddleLeft};
    }
    // Called by the pause menu so it shares input ownership and capture output.
    internal void Draw(bool wheelNavigation)
    {
        if(!IsOpen)return;
        Styles();var previousMatrix=GUI.matrix;
        try{
            GUI.matrix=Matrix4x4.identity;
            Fill(new Rect(0,0,Screen.width,Screen.height),new Color(0,0,0,.86f));
            float scale=Mathf.Min(1.5f,Mathf.Min(Screen.width/(Width+32),Screen.height/(Height+24)));
            GUI.matrix=Matrix4x4.TRS(new Vector3((Screen.width-Width*scale)*.5f,(Screen.height-Height*scale)*.5f,0),Quaternion.identity,new Vector3(scale,scale,1));
            Fill(new Rect(0,0,Width,Height),Ink);Frame(new Rect(0,0,Width,Height),Edge);Fill(new Rect(0,0,Width,5),Red);
            Text(new Rect(30,18,900,52),"CUSTOMIZE HUD",title);
            Text(new Rect(33,77,900,23),"SETTINGS  /  HUD  /  CUSTOMIZE",small,Muted);
            if(Button(new Rect(984,22,34,35),"×",false)){Close(false);return;}
            Fill(new Rect(0,108,Width,1),Edge);
            Fill(new Rect(30,139,334,370),Panel);
            if(pickerOpen)ChoicePicker();else for(int row=0;row<ChoiceRows;++row)Choice(row);
            var previewPanel=new Rect(384,139,624,370);
            Fill(previewPanel,Panel);Frame(previewPanel,Edge);
            bool ornament=OrnamentSelected;
            bool activePreview=ornament?working.hudOrnamentId!=0:working.hudMeterStyle!=0;
            Text(new Rect(404,156,220,22),activePreview?"LIVE PREVIEW":"PREVIEW",label);
            FittedText(new Rect(648,158,340,22),ornament?Idas3OrnamentCatalog.Name(working.hudOrnamentId):Idas3ArcadeMeterCatalog.Name(working.hudMeterStyle),small,Muted);
            var previewArea=new Rect(404,192,584,268);
            if(ornament)Idas3OrnamentRenderer.DrawPreview(previewArea,working.hudOrnamentId,Time.unscaledTime-previewStart);
            else Idas3ArcadeHud.DrawPreview(previewArea,working,Time.unscaledTime-previewStart);
            Text(new Rect(404,475,584,22),ornament?(activePreview?"SWING PREVIEW":"OFF"):activePreview?"DEMO  /  Sample engine speed and pedal inputs":"The original race meter is selected.",small,Muted);
            string help=pickerOpen?(ornament?"Choose an ornament.":"Choose a meter."):selection<ChoiceRows?Help[selection]:selection==ChoiceRows?"Restore the default HUD appearance.":selection==LayoutRow?"Move and resize your meter, keychain and other HUD items.":selection==ChoiceRows+1?"Save HUD customization and return to settings.":"Return to settings without saving these changes.";
            Text(new Rect(32,525,976,40),string.IsNullOrEmpty(error)?help:error,wrapped,string.IsNullOrEmpty(error)?Muted:Color.white);
            if(Button(new Rect(30,583,218,35),"RESET DEFAULTS",selection==ChoiceRows)){pickerOpen=false;selection=ChoiceRows;Activate();}
            if(Button(new Rect(268,583,218,35),"EDIT LAYOUT",selection==LayoutRow)){pickerOpen=false;selection=LayoutRow;Activate();}
            if(Button(new Rect(544,583,218,35),"APPLY",selection==ChoiceRows+1,true)){pickerOpen=false;selection=ChoiceRows+1;Activate();}
            if(Button(new Rect(782,583,226,35),"CANCEL",selection==ChoiceRows+2)){pickerOpen=false;selection=ChoiceRows+2;Activate();}
            Fill(new Rect(24,630,Width-48,1),Edge);
            Text(new Rect(32,646,974,22),wheelNavigation?(pickerOpen?"STEERING  BROWSE    ACCELERATOR  SELECT    BRAKE  BACK":"STEERING  SELECT    ACCELERATOR  CONFIRM    BRAKE  CANCEL"):
                pickerOpen?"↑ ↓ / ← →  BROWSE    ENTER / A  SELECT    ESC / B  BACK":"↑ ↓  SELECT    ← →  CHANGE    ENTER / A  CONFIRM    ESC / B  CANCEL",small,Muted);
        }finally{GUI.matrix=previousMatrix;}
    }
    void Choice(int row)
    {
        float y=149+row*70;bool enabled=Available(row);
        var bounds=new Rect(42,y,310,66);
        if(selection==row)Frame(bounds,Red);
        Text(new Rect(54,y+4,270,22),Labels[row],label,enabled?Color.white:Muted);
        string value=row==0?Idas3ArcadeMeterCatalog.Name(working.hudMeterStyle):
            row==4?Idas3OrnamentCatalog.Name(working.hudOrnamentId):!enabled?"ARCADE ONLY":row==1?(working.hudShiftLights?"ON":"OFF"):
            row==2?(working.hudPedalIndicators?"ON":"OFF"):(working.hudNameplateStyle==1?"ON":"OFF");
        bool before=GUI.enabled;GUI.enabled=before&&enabled;
        if(Button(new Rect(54,y+30,32,29),"‹",false)){Adjust(row,-1);}
        if(Button(new Rect(94,y+30,206,29),value,false)){if(row==0||row==4)OpenPicker(row);else Adjust(row,1);}
        if(Button(new Rect(308,y+30,32,29),"›",false)){Adjust(row,1);}
        GUI.enabled=before;
    }
    void ChoicePicker()
    {
        int count=PickerCount;
        Text(new Rect(54,153,250,23),OrnamentSelected?"ORNAMENT":"METER",label);
        var viewport=new Rect(42,183,286,VisibleChoices*32);
        var e=Event.current;
        if(e.type==EventType.ScrollWheel&&new Rect(30,139,334,370).Contains(e.mousePosition)){
            pickerScroll=Mathf.Clamp(pickerScroll+e.delta.y*2,0,Math.Max(0,count-VisibleChoices));e.Use();
        }
        pickerScroll=GUI.VerticalScrollbar(new Rect(335,183,17,VisibleChoices*32),pickerScroll,Math.Min(count,VisibleChoices),0,count);
        int first=Mathf.Clamp(Mathf.FloorToInt(pickerScroll),0,Math.Max(0,count-VisibleChoices));
        for(int row=0;row<VisibleChoices&&first+row<count;++row){
            int index=first+row;bool selected=index==pickerIndex;
            var rect=new Rect(viewport.x,viewport.y+row*32,viewport.width,30);
            bool hover=rect.Contains(e.mousePosition);
            Fill(rect,selected||hover?Raised:Panel);if(selected){Frame(rect,Red);Fill(new Rect(rect.x,rect.y,4,rect.height),Red);}
            FittedText(new Rect(rect.x+12,rect.y,rect.width-24,rect.height),PickerName(index),pickerLabel);
            if(GUI.Button(rect,GUIContent.none,GUIStyle.none)){SelectChoice(index);pickerOpen=false;}
        }
        Text(new Rect(54,481,180,22),(pickerIndex+1)+" / "+count,small,Muted);
        if(Button(new Rect(252,478,100,25),"DONE",false))pickerOpen=false;
    }
    static void FittedText(Rect rect,string value,GUIStyle style,Color? color=null)
    {
        int size=style.fontSize;bool wrap=style.wordWrap;
        try{
            var content=new GUIContent(value);
            while(style.fontSize>10&&style.CalcSize(content).x>rect.width)style.fontSize--;
            style.wordWrap=style.CalcSize(content).x>rect.width;
            Text(rect,value,style,color);
        }finally{style.fontSize=size;style.wordWrap=wrap;}
    }
    static void Fill(Rect rect,Color color){var before=GUI.color;GUI.color=color;GUI.DrawTexture(rect,Texture2D.whiteTexture);GUI.color=before;}
    static void Frame(Rect rect,Color color){Fill(new Rect(rect.x,rect.y,rect.width,1),color);Fill(new Rect(rect.x,rect.yMax-1,rect.width,1),color);Fill(new Rect(rect.x,rect.y,1,rect.height),color);Fill(new Rect(rect.xMax-1,rect.y,1,rect.height),color);}
    static void Text(Rect rect,string value,GUIStyle style,Color? color=null){var before=GUI.contentColor;GUI.contentColor=color??Color.white;GUI.Label(rect,value,style);GUI.contentColor=before;}
    bool Button(Rect rect,string value,bool active,bool primary=false)
    {
        bool hover=GUI.enabled&&rect.Contains(Event.current.mousePosition);
        Fill(rect,primary?Red:hover||active?Raised:Panel);Frame(rect,active?Red:Edge);
        if(active)Fill(new Rect(rect.x,rect.y,4,rect.height),Red);
        FittedText(new Rect(rect.x+5,rect.y,rect.width-10,rect.height),value,button,GUI.enabled?Color.white:Muted);
        return GUI.Button(rect,GUIContent.none,GUIStyle.none);
    }
}
