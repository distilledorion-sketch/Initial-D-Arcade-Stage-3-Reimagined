using System;
using System.Collections.Generic;
using System.Collections;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

// A separate presentation owner: editing never starts a race or changes a card.
public sealed class Idas3HudEditor : MonoBehaviour
{
    [StructLayout(LayoutKind.Sequential)] struct CarFrame { public uint size,vertices,ranges,textures; public IntPtr vertexData,rangeData,textureData; }
    [StructLayout(LayoutKind.Sequential)] struct CarVertex { public Vector3 position,normal; public Color color; public Vector2 uv; public Color offset; }
    [StructLayout(LayoutKind.Sequential)] struct CarTexture { public uint width,height; public ulong pixels; public IntPtr data; }
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneHudPreview(int mode,int width,int height,int mapSize,int mapZoom,float seconds,int messages);
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)] static extern int Idas3SceneHudCar(ref CarFrame frame);
    static readonly int[] Groups={1,2,3,6,7,4,5,8,9};
    static readonly string[] Names={"Time / sections","Speedometer / gear","Time Attack records","Legend opponent","Online opponent","Rear-view mirror","Minimap","Accepting challengers","Time Extended"};
    static readonly string[] Fields={"hudTimerSize","hudSpeedometerSize","hudRecordsSize","hudLegendSize","hudOnlineSize","hudMirrorSize","minimapSize","hudChallengersSize","hudTimeExtensionSize"};
    const int Layer=28;
    Idas3GameOptions options; Idas3PauseMenu menu;
    Idas3GameOptions.Values working;
    GameObject preview,car; Camera camera; Idas3UnityUi ui;
    readonly List<UnityEngine.Object> owned=new List<UnityEngine.Object>();
    Texture2D badge; int selected,mode; bool thirdPerson,dragging;
    Vector2 lastPointer; string error="";
    GUIStyle label,heading; Rect toolbar;
    RenderTexture diagnosticTarget;bool diagnosticReady;
    public bool IsOpen { get; private set; }
    internal Idas3GameOptions.Values Draft=>working;
    internal bool ThirdPerson=>thirdPerson;
    internal int PreviewMode=>mode;
    public void Open(Idas3GameOptions owner,Idas3PauseMenu parent)
    {
        if(IsOpen)return;
        options=owner;menu=parent;working=Idas3GameOptions.Normalize(owner.Draft);error="";
        try{
            preview=new GameObject("HUD layout preview");
            camera=preview.AddComponent<Camera>();camera.depth=20;camera.clearFlags=CameraClearFlags.SolidColor;
            camera.backgroundColor=new Color(.025f,.03f,.045f);camera.cullingMask=1<<Layer;
            camera.fieldOfView=55;camera.nearClipPlane=.01f;camera.farClipPlane=100;
            camera.transform.position=new Vector3(0,1.6f,-6.5f);camera.transform.LookAt(new Vector3(0,1.65f,0));
            ui=preview.AddComponent<Idas3UnityUi>();ui.Initialize(camera);ui.HudOptionsOverride=working;
            badge=Resources.Load<Texture2D>("Challenger/interrupt_4");
            BuildCar();car.SetActive(thirdPerson);
            IsOpen=true;Cursor.visible=true;Cursor.lockState=CursorLockMode.None;
            Refresh();
        }catch(Exception ex){error=ex.Message;Close(false);Debug.LogError("HUD editor: "+ex);}
    }
    void BuildCar()
    {
        var frame=new CarFrame{size=(uint)Marshal.SizeOf<CarFrame>()};
        if(Idas3SceneHudCar(ref frame)!=1||frame.vertices==0||frame.vertices>1000000||frame.ranges>10000||frame.textures>2048)
            throw new InvalidOperationException("Could not load preview car: "+Idas3Native.Error());
        var positions=new Vector3[frame.vertices];var normals=new Vector3[frame.vertices];var uv=new Vector2[frame.vertices];
        for(int i=0;i<positions.Length;++i){var v=Marshal.PtrToStructure<CarVertex>(IntPtr.Add(frame.vertexData,i*64));positions[i]=v.position;normals[i]=v.normal;uv[i]=v.uv;}
        var mesh=new Mesh{name="HUD editor car",indexFormat=IndexFormat.UInt32};owned.Add(mesh);
        mesh.vertices=positions;mesh.normals=normals;mesh.uv=uv;mesh.subMeshCount=(int)frame.ranges;
        var textures=new Texture2D[frame.textures];
        for(int i=0;i<textures.Length;++i){
            var source=Marshal.PtrToStructure<CarTexture>(IntPtr.Add(frame.textureData,i*24));
            if(source.width==0||source.height==0||source.width>2048||source.height>2048||source.pixels!=(ulong)source.width*source.height)throw new InvalidOperationException("Invalid preview texture");
            var argb=new int[source.pixels];Marshal.Copy(source.data,argb,0,argb.Length);var colors=new Color32[argb.Length];
            for(int j=0;j<colors.Length;++j){uint value=(uint)argb[j];colors[j]=new Color32((byte)(value>>16),(byte)(value>>8),(byte)value,(byte)(value>>24));}
            var texture=new Texture2D((int)source.width,(int)source.height,TextureFormat.RGBA32,false);texture.SetPixels32(colors);texture.Apply(false,true);textures[i]=texture;owned.Add(texture);
        }
        var shader=Resources.Load<Shader>("HudEditorCar");if(!shader)throw new InvalidOperationException("HUD preview shader missing");
        var materials=new Material[frame.ranges];
        for(int i=0;i<materials.Length;++i){
            int first=Marshal.ReadInt32(frame.rangeData,i*12),count=Marshal.ReadInt32(frame.rangeData,i*12+4),texture=Marshal.ReadInt32(frame.rangeData,i*12+8);
            if(first<0||count<0||(long)first+count>positions.Length)throw new InvalidOperationException("Invalid preview car geometry");
            var indices=new int[count];for(int j=0;j<count;++j)indices[j]=first+j;mesh.SetIndices(indices,MeshTopology.Triangles,i);
            var material=new Material(shader);material.mainTexture=texture>=0&&texture<textures.Length?textures[texture]:Texture2D.whiteTexture;owned.Add(material);materials[i]=material;
        }
        mesh.RecalculateBounds();car=new GameObject("Preview car");car.layer=Layer;car.transform.SetParent(preview.transform,false);
        car.AddComponent<MeshFilter>().sharedMesh=mesh;car.AddComponent<MeshRenderer>().sharedMaterials=materials;
        float scale=4f/Mathf.Max(.001f,mesh.bounds.size.z);car.transform.localScale=Vector3.one*scale;
        car.transform.localPosition=-new Vector3(mesh.bounds.center.x,mesh.bounds.min.y,mesh.bounds.center.z)*scale;
        // Camera is an owner, not the car's parent: keep the car in preview world space.
        car.transform.SetParent(null,true);
        car.transform.position=-new Vector3(mesh.bounds.center.x,mesh.bounds.min.y,mesh.bounds.center.z)*scale;
        car.transform.rotation=Quaternion.identity;
    }
    void LateUpdate(){if(IsOpen){if(!menu.IsOpen){Close(false);return;}Refresh();}}
    internal void Refresh()
    {
        if(!IsOpen)return;
        if(Idas3SceneHudPreview(mode,Screen.width,Screen.height,working.minimapSize,working.minimapZoom,Time.unscaledTime,0)!=1){error=Idas3Native.Error();return;}
        ui.ApplyFrame();
    }
    internal void SelectGroup(int index)
    {
        selected=Mathf.Clamp(index,0,Groups.Length-1);int group=Groups[selected];
        if(group==3)mode=0;else if(group==6)mode=1;else if(group==7)mode=2;
        dragging=false;
    }
    internal void SetThirdPerson(bool value){thirdPerson=value;if(car)car.SetActive(value);}
    Rect BadgeRect()
    {
        float w=Screen.width,h=Screen.height,fit=Mathf.Min(w/640,h/480);
        float upper=working.HudGroupScale(mode==0?3:mode==1?6:7),lower=working.HudGroupScale(2),scale=working.HudGroupScale(8);
        var size=new Vector2(114,28.5f)*fit*scale;
        var center=new Vector2(w-80*fit*Mathf.Max(upper,lower,scale),(194*fit*upper+h-161*fit*lower)*.5f);
        center+=Vector2.Scale(working.HudOffset(8),new Vector2(w,h));return new Rect(center-size*.5f,size);
    }
    internal bool Bounds(int group,out Rect bounds){if(group==8){bounds=BadgeRect();return true;}return ui.HudBounds(group,out bounds);}
    internal void MoveSelected(Vector2 delta)
    {
        int group=Groups[selected];if(!Bounds(group,out var bounds))return;
        // Keep the whole group reachable even after changing aspect ratio.
        delta.x=Mathf.Clamp(delta.x,-bounds.xMin,Mathf.Max(-bounds.xMin,Screen.width-bounds.xMax));
        delta.y=Mathf.Clamp(delta.y,-bounds.yMin,Mathf.Max(-bounds.yMin,Screen.height-bounds.yMax));
        working.SetHudOffset(group,working.HudOffset(group)+new Vector2(delta.x/Screen.width,delta.y/Screen.height));
    }
    internal void ResizeSelected(int direction)
    {
        var field=typeof(Idas3GameOptions.Values).GetField(Fields[selected]);int value=(int)field.GetValue(working);
        field.SetValue(working,Mathf.Clamp(value+direction,0,Groups[selected]==5?2:4));
    }
    internal void ResetSelected(){working.SetHudOffset(Groups[selected],Vector2.zero);typeof(Idas3GameOptions.Values).GetField(Fields[selected]).SetValue(working,Groups[selected]==5?0:2);}
    static void CopyLayout(Idas3GameOptions.Values from,Idas3GameOptions.Values to)
    {
        foreach(string name in Fields){var field=typeof(Idas3GameOptions.Values).GetField(name);field.SetValue(to,field.GetValue(from));}
        to.hudPositions=(Vector2[])from.hudPositions.Clone();to.minimapZoom=from.minimapZoom;
    }
    public void Close(bool save)
    {
        if(save&&IsOpen){
            var pending=options.Draft.Clone();options.BeginEdit();CopyLayout(working,options.Draft);
            bool applied=options.ApplyDraft();if(applied)CopyLayout(working,pending);
            JsonUtility.FromJsonOverwrite(JsonUtility.ToJson(pending),options.Draft);
            if(!applied){error=options.LastError;return;}
        }
        IsOpen=false;dragging=false;
        if(preview){preview.SetActive(false);Destroy(preview);}if(car)Destroy(car);
        foreach(var obj in owned)if(obj)Destroy(obj);owned.Clear();preview=null;ui=null;camera=null;car=null;
    }
    void OnDestroy(){Close(false);}
    void OnGUI()
    {
        if(!IsOpen)return;
        bool diagnostic=diagnosticTarget!=null&&Event.current.type==EventType.Repaint;
        var previousTarget=RenderTexture.active;
        if(diagnostic){RenderTexture.active=diagnosticTarget;GL.PushMatrix();GL.LoadPixelMatrix(0,Screen.width,Screen.height,0);}
        try{DrawEditor();}finally{if(diagnostic){GL.PopMatrix();RenderTexture.active=previousTarget;diagnosticTarget=null;diagnosticReady=true;}}
    }
    void DrawEditor()
    {
        GUI.depth=-12000;
        if(label==null){label=new GUIStyle(GUI.skin.label){fontSize=14,normal={textColor=Color.white}};heading=new GUIStyle(label){fontSize=17,fontStyle=FontStyle.Bold};}
        float toolbarScale=Mathf.Min(1f,(Screen.width-24)/930f);
        float width=930*toolbarScale;toolbar=new Rect((Screen.width-width)*.5f,Screen.height-12-114*toolbarScale,width,114*toolbarScale);
        var e=Event.current;
        if(!toolbar.Contains(e.mousePosition)){
            if(e.type==EventType.MouseDown&&e.button==0){
                for(int i=Groups.Length-1;i>=0;--i)if(Bounds(Groups[i],out var hit)&&hit.Contains(e.mousePosition)){selected=i;dragging=true;lastPointer=e.mousePosition;GUIUtility.hotControl=0;e.Use();break;}
            }else if(e.type==EventType.MouseDrag&&dragging){MoveSelected(e.mousePosition-lastPointer);lastPointer=e.mousePosition;e.Use();}
            else if(e.type==EventType.ScrollWheel){if(Bounds(Groups[selected],out var hit)&&hit.Contains(e.mousePosition)){ResizeSelected(e.delta.y<0?1:-1);e.Use();}}
        }
        if(e.type==EventType.MouseUp)dragging=false;
        if(badge)GUI.DrawTextureWithTexCoords(BadgeRect(),badge,new Rect(0,1,1,-1));
        foreach(int group in Groups)if(Bounds(group,out var bounds)){
            bool chosen=group==Groups[selected];GUI.color=chosen?new Color(.2f,.85f,1,.9f):new Color(1,1,1,.18f);
            GUI.DrawTexture(new Rect(bounds.x,bounds.y,bounds.width,1),Texture2D.whiteTexture);GUI.DrawTexture(new Rect(bounds.x,bounds.yMax-1,bounds.width,1),Texture2D.whiteTexture);
            GUI.DrawTexture(new Rect(bounds.x,bounds.y,1,bounds.height),Texture2D.whiteTexture);GUI.DrawTexture(new Rect(bounds.xMax-1,bounds.y,1,bounds.height),Texture2D.whiteTexture);
        }
        GUI.color=Color.white;GUI.Box(toolbar,"");
        var previousMatrix=GUI.matrix;GUI.matrix=Matrix4x4.TRS(new Vector3(toolbar.x,toolbar.y,0),Quaternion.identity,Vector3.one*toolbarScale);
        try{DrawToolbar();}finally{GUI.matrix=previousMatrix;}
    }
    void DrawToolbar()
    {
        GUILayout.BeginArea(new Rect(12,7,906,100));
        GUILayout.BeginHorizontal();GUILayout.Label("HUD EDITOR",heading,GUILayout.Width(125));
        if(GUILayout.Toggle(!thirdPerson,"Bumper",GUI.skin.button,GUILayout.Width(90)))SetThirdPerson(false);
        if(GUILayout.Toggle(thirdPerson,"Third person",GUI.skin.button,GUILayout.Width(105)))SetThirdPerson(true);
        GUILayout.FlexibleSpace();if(GUILayout.Button("Time Attack"))mode=0;if(GUILayout.Button("Legend"))mode=1;if(GUILayout.Button("Online"))mode=2;GUILayout.EndHorizontal();
        GUILayout.BeginHorizontal();if(GUILayout.Button("<",GUILayout.Width(28)))SelectGroup((selected+Groups.Length-1)%Groups.Length);
        GUILayout.Label(Names[selected],label,GUILayout.Width(180));if(GUILayout.Button(">",GUILayout.Width(28)))SelectGroup((selected+1)%Groups.Length);
        if(GUILayout.Button("−",GUILayout.Width(28)))ResizeSelected(-1);
        int size=(int)typeof(Idas3GameOptions.Values).GetField(Fields[selected]).GetValue(working);
        GUILayout.Label((Groups[selected]==5?100+25*size:50+25*size)+"%",label,GUILayout.Width(45));
        if(GUILayout.Button("+",GUILayout.Width(28)))ResizeSelected(1);
        if(GUILayout.Button("Reset selected"))ResetSelected();
        if(GUILayout.Button("Reset layout")){CopyLayout(new Idas3GameOptions.Values(),working);}
        if(GUILayout.Button("Cancel")){Close(false);GUILayout.EndHorizontal();GUILayout.EndArea();return;}
        if(GUILayout.Button("Save & return")){Close(true);GUILayout.EndHorizontal();GUILayout.EndArea();return;}
        GUILayout.EndHorizontal();GUILayout.Label(string.IsNullOrEmpty(error)?(Idas3GameOptions.Values.HudPositionGroup(Groups[selected])==3?"Position shared by Time Attack, Legend and Online • Mouse wheel or + / − to resize":"Drag a highlighted group • Mouse wheel or + / − to resize • Race announcements stay fixed"):error,label);GUILayout.EndArea();
    }
    internal IEnumerator Capture(string path)
    {
        Refresh();var target=new RenderTexture(Screen.width,Screen.height,24);target.Create();camera.targetTexture=target;
        try{
            camera.Render();ui.RenderOverlayForCapture();camera.targetTexture=null;
            diagnosticReady=false;diagnosticTarget=target;float deadline=Time.realtimeSinceStartup+4;
            while(!diagnosticReady&&Time.realtimeSinceStartup<deadline)yield return null;
            if(!diagnosticReady)throw new InvalidOperationException("HUD editor did not repaint");
            yield return new WaitForEndOfFrame();
            var previous=RenderTexture.active;RenderTexture.active=target;
            var texture=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);texture.ReadPixels(new Rect(0,0,target.width,target.height),0,0);texture.Apply();System.IO.File.WriteAllBytes(path,texture.EncodeToPNG());Destroy(texture);RenderTexture.active=previous;
        }finally{diagnosticTarget=null;if(camera)camera.targetTexture=null;target.Release();Destroy(target);}
    }
}
