using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using UnityEngine;
namespace Idas3.Multiplayer {
// Explicit isolated controller diagnostic only; this transport never makes network calls.
internal static class Idas3OnlineMenuScreens {
    sealed class RoomsTransport:IIdas3Transport,IIdas3PrivateRoomTransport {
        public bool PrivateRoom=>false;public void HostPrivate(string name){}
        internal readonly List<Idas3Room> items=new List<Idas3Room>();
        internal readonly List<byte[]> sent=new List<byte[]>();
        internal bool connected,lastReliable;internal string room="";
        public string Kind=>"Online";public bool Available=>true;public bool Connected=>connected;public bool IsHost=>connected;
        public string LocalId=>"1";public string LocalName=>connected?"テスト運転手":"Chris";public string RemoteId=>connected?"2":"";public string RemoteName=>connected?"対戦相手":"";
        public string RoomCode=>room;public string Status=>"";public IReadOnlyList<Idas3Room> Rooms=>items;
        public event Action<byte[]> Message;public event Action PeerChanged {add{} remove{}}public event Action<string> Error {add{} remove{}}
        internal void Deliver(byte[] data){Message?.Invoke(data);}
        public bool Initialize()=>true;public void Host(string s){}public void Join(string s){}public void Browse(){}public void Send(byte[] data,bool reliable){sent.Add((byte[])data.Clone());lastReliable=reliable;}public void Poll(){}public void Leave(){connected=false;room="";}public void Dispose(){}
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
            typeof(Idas3MultiplayerMenu).GetField("joinCode",Private).SetValue(menu,"ID3-0123456789ABCDEF01234567");
            yield return Capture(menu,Path.Combine(root,"online-code.png"));
            typeof(Idas3MultiplayerMenu).GetField("codeEditing",Private).SetValue(menu,true);
            typeof(Idas3MultiplayerMenu).GetField("codeDraft",Private).SetValue(menu,"ID3-0123456789ABCDEF01234567");
            yield return Capture(menu,Path.Combine(root,"online-keyboard.png"));
            yield return CheckUsername(menu,session,root);
            yield return CheckLobbyChat(menu,session,transport,root);
            File.WriteAllText(Path.Combine(root,"menu-screens-passed.txt"),"PASS actual Unity browser/code/chat renders; 0, 3, 4, 8 rooms; controller scroll to eighth row; shrinking list clamp; connected lobby, Japanese names, 40-message history, unread scroll preservation, local-send auto-scroll, and text-entry input safeguards. Isolated fake transport; no network, live population, or physical keyboard/IME input claimed.\n");
        }finally{menu.SetOpen(false);session.Dispose();UnityEngine.Object.Destroy(go);}
    }
    internal static IEnumerator RunUsernameOnly(string root){
        Directory.CreateDirectory(root);Idas3OnlineUsernameChecks.Run();
        var transport=new RoomsTransport();var session=new Idas3MultiplayerSession(0,Path.Combine(root,"menu-only-saves"));
        typeof(Idas3MultiplayerSession).GetMethod("SetTransport",Private).Invoke(session,new object[]{transport});
        var go=new GameObject("Isolated username menu render checks");var menu=go.AddComponent<Idas3MultiplayerMenu>();
        menu.Initialize(session);menu.ManagedControlInput=true;OpenUsernameFixture(menu);
        try{yield return CheckUsername(menu,session,root);}
        finally{menu.SetOpen(false);session.Dispose();UnityEngine.Object.Destroy(go);}
    }
    static IEnumerator CheckUsername(Idas3MultiplayerMenu menu,Idas3MultiplayerSession session,string root){
        string stored=PlayerPrefs.GetString(Idas3OnlineUsername.PreferenceKey,"");bool existed=PlayerPrefs.HasKey(Idas3OnlineUsername.PreferenceKey);
        int language=Idas3MenuLocalization.Language;
        try{
            Set(menu,"codeEditing",false);Set(menu,"joinEntry",false);Call(menu,"BeginUsername");Set(menu,"usernameRequired",true);Set(menu,"usernameDraft","");
            foreach(int selected in new[]{0,1,2}){
                Idas3MenuLocalization.SetLanguage(selected);
                yield return Capture(menu,Path.Combine(root,"online-username-"+selected+".png"));
            }
            Idas3MenuLocalization.SetLanguage(0);
            var focus=(Idas3MenuFocus)Read(menu,"controllerFocus");
            Check(menu.ControllerActionCount>60,"username keyboard must expose controller actions");
            focus.Pointer("username-input");
            menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);yield return null;
            menu.ProcessMenuNavigation(0,0,true,false,false,Time.realtimeSinceStartupAsDouble);
            yield return Capture(menu,Path.Combine(root,"online-username-controller-focus.png"));
            Check(!(bool)Read(menu,"usernameTyping")&&menu.ControllerSelection=="username-key:0","controller field activation must focus onscreen keyboard without entering physical typing mode");
            menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);yield return null;
            menu.ProcessMenuNavigation(0,0,true,false,false,Time.realtimeSinceStartupAsDouble);
            yield return Capture(menu,Path.Combine(root,"online-username-controller.png"));
            Check((string)Read(menu,"usernameDraft")=="A","controller must insert a keyboard character once");
            menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);yield return null;
            menu.ProcessMenuNavigation(1,0,false,false,false,Time.realtimeSinceStartupAsDouble);yield return null;
            Check(menu.ControllerSelection=="username-key:1","controller keyboard must navigate to next character");
            Set(menu,"usernameDraft","日本の運転手");Set(menu,"usernameTyping",true);
            Check(menu.CapturesTextInput&&menu.BlocksGameInput,"username typing must capture input");
            menu.ProcessControlInput(true,true,false);menu.ProcessMenuNavigation(1,1,true,true,false,Time.realtimeSinceStartupAsDouble);
            Check(menu.IsOpen&&(bool)Read(menu,"usernameTyping")&&(string)Read(menu,"usernameDraft")=="日本の運転手","typing must suppress menu and driving controls");
            yield return Capture(menu,Path.Combine(root,"online-username-japanese-entry.png"),()=>Set(menu,"usernameTyping",true));
            Call(menu,"EndUsernameTyping");Check(menu.CapturesTextInput,"closing text entry must retain release guard");
            yield return null;yield return null;
            menu.ProcessControlInput(false,false,false);menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);
            Check(!menu.CapturesTextInput,"neutral controls release username typing guard");
            Call(menu,"BackFromMenu");Check(!menu.IsOpen&&!session.InLobby,"cancelling required name must return to the game");
            OpenUsernameFixture(menu);Set(menu,"playerDetails",true);Call(menu,"BeginUsername");Set(menu,"usernameRequired",false);Call(menu,"BackFromMenu");
            Check(menu.IsOpen&&(bool)Read(menu,"playerDetails")&&!(bool)Read(menu,"usernameOpen"),"cancelling a rename must retain Player Details");
            Set(menu,"playerDetails",false);
            Check(PlayerPrefs.HasKey(Idas3OnlineUsername.PreferenceKey)==existed&&PlayerPrefs.GetString(Idas3OnlineUsername.PreferenceKey,"")==stored,"menu fixtures must not change the saved username");
            File.WriteAllText(Path.Combine(root,"username-menu-passed.txt"),"PASS actual Unity EN/JP/Chinese username modal renders, controller keyboard selection/insertion, input and release guards, first-use Cancel, rename Cancel, and untouched live preferences. Fake transport; no network or physical IME automation claimed.\n");
        }finally{Idas3MenuLocalization.SetLanguage(language);Call(menu,"EndUsernameTyping");Set(menu,"usernameOpen",false);Set(menu,"usernameRequired",false);}
    }
    static void OpenUsernameFixture(Idas3MultiplayerMenu menu){
        // This presentation fixture owns a fake room provider and intentionally
        // omits the game/garage/native DLL. Session lifecycle and first-use
        // matchmaking gates have their own production-session regression suite.
        Set(menu,"previousCursorVisible",Cursor.visible);Set(menu,"previousCursorLock",Cursor.lockState);
        Set(menu,"<IsOpen>k__BackingField",true);
        ((Idas3MenuFocus)Read(menu,"controllerFocus")).Reset();
    }
    static object Read(object owner,string name)=>owner.GetType().GetField(name,Private).GetValue(owner);
    static void Set(object owner,string name,object value)=>owner.GetType().GetField(name,Private).SetValue(owner,value);
    static void Call(object owner,string name)=>owner.GetType().GetMethod(name,Private).Invoke(owner,null);
    static void Check(bool value,string message){if(!value)throw new Exception("Lobby chat: "+message);}
    static byte[] ChatPacket(string text,ulong sequence){
        using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream,Encoding.UTF8,true)){
            var bytes=Encoding.UTF8.GetBytes(text);
            writer.Write(0x504D3349u);writer.Write((ushort)11);writer.Write((byte)19);writer.Write(202UL);writer.Write(101UL);
            writer.Write(0UL);writer.Write(sequence);writer.Write((ushort)bytes.Length);writer.Write(bytes);return stream.ToArray();
        }
    }
    static IEnumerator CheckLobbyChat(Idas3MultiplayerMenu menu,Idas3MultiplayerSession session,RoomsTransport transport,string root){
        Set(menu,"joinEntry",false);Set(menu,"codeEditing",false);
        transport.connected=true;transport.room="isolated-chat";
        Set(session,"state","Lobby");Set(session,"<HandshakeComplete>k__BackingField",true);
        Set(session,"localNonce",101UL);Set(session,"remoteNonce",202UL);Set(session,"remoteName",transport.RemoteName);Set(session,"matchAnnounced",true);
        Check(session.CanChat&&session.CanReady,"connected fixture must enable both chat and Ready");
        yield return Capture(menu,Path.Combine(root,"online-chat-empty.png"));
        transport.Deliver(ChatPacket("よろしくお願いします！ Akina downhill next?",1));
        Check(session.LobbyChatMessages.Count==1&&session.LobbyChatMessages[0].Name=="対戦相手","received text must use the admitted Japanese peer name");
        var history=(Idas3LobbyChat)Read(session,"lobbyChat");
        // Virtual time fills a bounded history without waiting for 30 seconds;
        // the first and subsequent unread message use the real receive path.
        for(ulong i=2;i<=40;++i)Check(history.TryRemote(transport.RemoteName,"Message "+i+": 秋名の下りを走りましょう。 Let's race again after this round.",i,i),"history fixture");
        yield return Capture(menu,Path.Combine(root,"online-chat-history-latest.png"));
        Check(((Vector2)Read(menu,"chatScroll")).y>500,"long history should initially show the newest message");
        Set(menu,"chatScroll",Vector2.zero);
        yield return Capture(menu,Path.Combine(root,"online-chat-history-oldest.png"));
        transport.Deliver(ChatPacket("New reply while you are reading earlier messages.",41));
        Check(session.LobbyChatMessages.Count==40,"history must stay bounded");
        yield return Capture(menu,Path.Combine(root,"online-chat-history-unread.png"));
        Check(((Vector2)Read(menu,"chatScroll")).y==0,"an unread remote reply must preserve the reader's scroll position");

        const string draft="F1 / Escape / Backspace test — 入力中";
        var focus=(Idas3MenuFocus)Read(menu,"controllerFocus");
        var visible=new List<Idas3MenuFocus.Item>((List<Idas3MenuFocus.Item>)Read(focus,"visible"));
        var ready=visible.Find(item=>item.id.EndsWith(":READY",StringComparison.Ordinal));
        Check(ready.id!=null,"the real lobby repaint must include enabled Ready");
        Set(menu,"chatDraft",draft);Call(menu,"BeginChatEditing");
        // BeginChatEditing normally clears focus. Repopulate the actual lobby
        // controls and select Ready to exercise the worst possible held input.
        focus.Begin();foreach(var item in visible)focus.Control(item.id,item.rect,true,false);focus.End();focus.Pointer(ready.id);
        Check(menu.CapturesTextInput,"typing must capture controls");
        string selection=menu.ControllerSelection;int sent=transport.sent.Count;
        menu.ProcessControlInput(true,false,false);
        Check(menu.IsOpen&&(bool)Read(menu,"chatEditing"),"F1 must not close the menu while typing");
        menu.ProcessControlInput(false,true,false);
        Check(menu.IsOpen&&(bool)Read(menu,"chatEditing"),"rebound Cancel must not close text entry");
        double now=Time.realtimeSinceStartupAsDouble;
        menu.ProcessMenuNavigation(0,0,false,false,false,now);
        menu.ProcessMenuNavigation(0,0,true,false,false,now+.1);
        Check(Read(focus,"activate")==null,"controller confirm must not queue Ready while typing");
        menu.ProcessMenuNavigation(1,1,false,false,false,now+.2);
        menu.ProcessMenuNavigation(0,0,false,true,false,now+.3);
        menu.ProcessMenuNavigation(-1,0,true,true,false,now+.4,true);
        Check(menu.IsOpen&&menu.CapturesTextInput&&(string)Read(menu,"chatDraft")==draft,"navigation, Ready and Backspace/back must leave the draft intact");
        Check(menu.ControllerSelection==selection&&!session.LocalReady&&transport.sent.Count==sent,"typing must not move controller focus, toggle Ready, or send a packet");
        yield return Capture(menu,Path.Combine(root,"online-chat-editing.png"),()=>Call(menu,"BeginChatEditing"));
        Check(!session.LocalReady&&transport.sent.Count==sent,"a repaint must not activate a control queued while typing");

        Set(menu,"chatDraft","  よろしくお願いします！   Ready when you are.  ");Call(menu,"BeginChatEditing");Call(menu,"SubmitChat");
        Check((string)Read(menu,"chatDraft")==""&&transport.sent.Count==sent+1&&transport.lastReliable,"Submit must clear the draft and send one reliable message");
        Check(session.LobbyChatMessages[39].IsLocal&&session.LobbyChatMessages[39].Name=="テスト運転手","local echo must use the local driver name");
        using(var stream=new MemoryStream(transport.sent[sent]))using(var reader=new BinaryReader(stream,Encoding.UTF8)){
            Check(reader.ReadUInt32()==0x504D3349u&&reader.ReadUInt16()==11&&reader.ReadByte()==19,"wire must use the chat packet type");
            Check(reader.ReadUInt64()==101UL&&reader.ReadUInt64()==202UL&&reader.ReadUInt64()==0UL&&reader.ReadUInt64()==1UL,"wire must identify this peer, room round and message");
            int length=reader.ReadUInt16();Check(Encoding.UTF8.GetString(reader.ReadBytes(length))=="よろしくお願いします！ Ready when you are."&&stream.Position==stream.Length,"wire must contain only the sanitized UTF-8 text");
        }
        Call(menu,"SubmitChat");Check(transport.sent.Count==sent+1,"an empty draft must not resend the previous message");
        Call(menu,"EndChatEditing");
        menu.ProcessControlInput(true,true,false);menu.ProcessMenuNavigation(1,1,true,true,false,now+.4);
        Check(menu.CapturesTextInput&&menu.IsOpen&&!session.LocalReady,"held controls must remain blocked after text entry ends");
        yield return null;yield return null;
        menu.ProcessControlInput(false,false,false);menu.ProcessMenuNavigation(0,0,false,false,false,Time.realtimeSinceStartupAsDouble);
        Check(!menu.CapturesTextInput&&menu.IsOpen,"neutral controls after the release frame must release the typing gate");
        yield return Capture(menu,Path.Combine(root,"online-chat-local-sent.png"));
        Check(((Vector2)Read(menu,"chatScroll")).y>500,"sending locally must scroll to the new message");

        Set(session,"state","Loading");Set(menu,"chatDraft","Do not send while loading");Call(menu,"BeginChatEditing");Call(menu,"SubmitChat");
        Check(!session.CanChat&&!(bool)Read(menu,"chatEditing")&&transport.sent.Count==sent+1,"loading must disable both editing and sending");
        Set(session,"state","Lobby");Set(menu,"chatDraft","");
        File.WriteAllText(Path.Combine(root,"lobby-chat-menu-passed.txt"),"PASS connected fake lobby rendering; Japanese peer/local names; bounded history; remote unread preserves scroll; local send scrolls to bottom; F1/rebound Cancel/controller Ready/navigation/Backspace-back suppressed during typing; held-input release gate; reliable UTF-8 send; empty-submit and loading guards. Input method calls and real Unity repaint only, not physical keyboard or IME automation.\n");
    }
    static IEnumerator Capture(Idas3MultiplayerMenu menu,string file,Action prepare=null){
        for(int i=0;i<4;++i)yield return null;
        var target=new RenderTexture(Screen.width,Screen.height,24,RenderTextureFormat.ARGB32){antiAliasing=1};target.Create();
        var old=RenderTexture.active;RenderTexture.active=target;GL.Clear(true,true,Color.black);RenderTexture.active=old;
        menu.RequestDiagnosticCapture(target);double until=Time.realtimeSinceStartupAsDouble+5;
        // Hidden diagnostic players may have no OS focus. Apply this isolated
        // draft fixture after Update so OnGUI can render its editing state.
        while(!menu.DiagnosticCaptureReady&&Time.realtimeSinceStartupAsDouble<until){prepare?.Invoke();yield return null;}
        if(!menu.DiagnosticCaptureReady){menu.CancelDiagnosticCapture();target.Release();UnityEngine.Object.Destroy(target);throw new Exception("No diagnostic online menu repaint");}
        old=RenderTexture.active;RenderTexture.active=target;var image=new Texture2D(target.width,target.height,TextureFormat.RGB24,false);image.ReadPixels(new Rect(0,0,target.width,target.height),0,0);image.Apply();RenderTexture.active=old;
        File.WriteAllBytes(file,image.EncodeToPNG());UnityEngine.Object.Destroy(image);target.Release();UnityEngine.Object.Destroy(target);
    }
}}
