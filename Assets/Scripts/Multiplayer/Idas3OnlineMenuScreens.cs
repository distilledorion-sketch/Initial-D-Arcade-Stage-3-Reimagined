using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;
namespace Idas3.Multiplayer {
// Explicit isolated controller diagnostic only; this transport never makes network calls.
internal static class Idas3OnlineMenuScreens {
    sealed class RoomsTransport:IIdas3Transport {
        internal readonly List<Idas3Room> items=new List<Idas3Room>();
        public string Kind=>"Diagnostic";public bool Available=>true;public bool Connected=>false;public bool IsHost=>false;
        public string LocalId=>"1";public string LocalName=>"Chris";public string RemoteId=>"";public string RemoteName=>"";
        public string RoomCode=>"";public string Status=>"";public IReadOnlyList<Idas3Room> Rooms=>items;
        public event Action<byte[]> Message {add{} remove{}}public event Action PeerChanged {add{} remove{}}public event Action<string> Error {add{} remove{}}
        public bool Initialize()=>true;public void Host(string s){}public void Join(string s){}public void Browse(){}public void Send(byte[] data,bool reliable){}public void Poll(){}public void Leave(){}public void Dispose(){}
    }
    static readonly BindingFlags Private=BindingFlags.Instance|BindingFlags.NonPublic;
    internal static IEnumerator Run(string root){
        var transport=new RoomsTransport();var session=new Idas3MultiplayerSession(0,Path.Combine(root,"menu-only-saves"));
        typeof(Idas3MultiplayerSession).GetMethod("SetTransport",Private).Invoke(session,new object[]{transport});
        var go=new GameObject("Isolated online menu render checks");var menu=go.AddComponent<Idas3MultiplayerMenu>();menu.Initialize(session);menu.ManagedControlInput=true;menu.SetOpen(true);
        try{
            foreach(int count in new[]{0,3,4,8}){
                transport.items.Clear();for(int i=0;i<count;++i)transport.items.Add(new Idas3Room{Code="diagnostic-"+i,HostName=i==0?"Chris":"Driver "+(i+1),Members=1,Capacity=2});
                yield return Capture(menu,Path.Combine(root,"online-rooms-"+count+".png"));
            }
            // Controller navigation must scroll to an offscreen room, then wrap
            // back without clipping focus or losing the distinct room identity.
            menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);
            for(int i=0;i<60&&menu.ControllerSelection!="room:diagnostic-7";++i){
                menu.ProcessMenuNavigation(0,1,false,false,false,Time.realtimeSinceStartupAsDouble);yield return null;
                menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);yield return null;
            }
            if(menu.ControllerSelection!="room:diagnostic-7")throw new Exception("Controller cannot reach eighth room");
            yield return Capture(menu,Path.Combine(root,"online-rooms-8-scrolled.png"));
            var scroll=(Vector2)typeof(Idas3MultiplayerMenu).GetField("roomScroll",Private).GetValue(menu);
            if(scroll.y<330)throw new Exception("Focused eighth room did not scroll fully into view");
            transport.items.RemoveRange(1,7);yield return Capture(menu,Path.Combine(root,"online-rooms-shrunk.png"));
            scroll=(Vector2)typeof(Idas3MultiplayerMenu).GetField("roomScroll",Private).GetValue(menu);if(scroll.y!=0)throw new Exception("Shrinking room list left empty overscroll");
            typeof(Idas3MultiplayerMenu).GetField("joinEntry",Private).SetValue(menu,true);
            yield return Capture(menu,Path.Combine(root,"online-code.png"));
            typeof(Idas3MultiplayerMenu).GetField("codeEditing",Private).SetValue(menu,true);
            yield return Capture(menu,Path.Combine(root,"online-keyboard.png"));
            File.WriteAllText(Path.Combine(root,"menu-screens-passed.txt"),"PASS actual Unity browser/code renders; 0, 3, 4, 8 rooms; controller scroll to eighth row; shrinking list clamp. Isolated fake room catalog; no network or live population claimed.\n");
        }finally{menu.SetOpen(false);session.Dispose();UnityEngine.Object.Destroy(go);}
    }
    static IEnumerator Capture(Idas3MultiplayerMenu menu,string file){
        for(int i=0;i<4;++i)yield return null;
        var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){antiAliasing=1};target.Create();
        var old=RenderTexture.active;RenderTexture.active=target;GL.Clear(true,true,Color.black);RenderTexture.active=old;
        menu.RequestDiagnosticCapture(target);double until=Time.realtimeSinceStartupAsDouble+5;
        while(!menu.DiagnosticCaptureReady&&Time.realtimeSinceStartupAsDouble<until)yield return null;
        if(!menu.DiagnosticCaptureReady){menu.CancelDiagnosticCapture();target.Release();UnityEngine.Object.Destroy(target);throw new Exception("No diagnostic online menu repaint");}
        old=RenderTexture.active;RenderTexture.active=target;var image=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();RenderTexture.active=old;
        File.WriteAllBytes(file,image.EncodeToPNG());UnityEngine.Object.Destroy(image);target.Release();UnityEngine.Object.Destroy(target);
    }
}}
