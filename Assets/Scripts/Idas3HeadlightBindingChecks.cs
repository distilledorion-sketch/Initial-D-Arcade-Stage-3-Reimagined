using System;
using System.IO;
using UnityEngine;
public static class Idas3HeadlightBindingChecks {
    static int checks;static void Check(bool ok,string message){checks++;if(!ok)throw new Exception(message);}
    static bool H(Idas3Native.FrameInput f)=>(f.key2&(1u<<8))!=0;
    public static void Run(){
        string root="Verification/effects-headlights-20260922/bindings-"+Guid.NewGuid().ToString("N");Directory.CreateDirectory(root);
        Idas3Native.FrameInput Read(Idas3ControlBindings m,KeyCode key=KeyCode.None,ushort buttons=0){m.Poll(k=>k==key,new Idas3ControlBindings.PadState{connected=true,buttons=buttons},0);var f=new Idas3Native.FrameInput();m.ApplyDriving(ref f);return f;}
        var map=new Idas3ControlBindings();map.Initialize(root);
        Check(H(Read(map,KeyCode.H)),"Default H did not reach native H");Check(H(Read(map,KeyCode.None,0x80)),"Right-stick click did not reach native H");Check(!H(Read(map)),"Released headlights remained held");
        map.BeginEdit();Check(map.TrySetDraftKey(Idas3ControlBindings.ActionId.Headlights,Idas3ControlBindings.Slot.Primary,KeyCode.L)&&map.ApplyDraft(),"Rebind headlights");Read(map);
        Check(H(Read(map,KeyCode.L))&&!H(Read(map,KeyCode.H)),"Rebind retained old H shortcut");
        var loaded=new Idas3ControlBindings();loaded.Initialize(root);Check(H(Read(loaded,KeyCode.L)),"Saved headlight binding lost");
        foreach(int version in new[]{1,2})foreach(bool conflict in new[]{false,true}){
            string path=Path.Combine(root,"legacy"+version+conflict);Directory.CreateDirectory(path);
            var old=Idas3ControlBindings.Defaults();old.version=version;Array.Resize(ref old.actions,9);
            if(conflict){old.actions[6].key1=KeyCode.H;old.actions[6].pad=Idas3ControlBindings.PadInput.RightThumb;}
            File.WriteAllText(Path.Combine(path,"controls.json"),JsonUtility.ToJson(old));var m=new Idas3ControlBindings();m.Initialize(path);
            Check(m.LastError==null&&m.Current.version==3&&m.Current.actions.Length==10,"Legacy controls did not migrate");
            for(int i=0;i<9;i++)Check(JsonUtility.ToJson(m.Current.actions[i])==JsonUtility.ToJson(old.actions[i]),"Migration changed an existing action");
            Check(H(Read(m,KeyCode.H))==!conflict,"Migration overwrote a key assignment");Check(H(Read(m,KeyCode.None,0x80))==!conflict,"Migration overwrote a controller assignment");
        }
        foreach(bool generic in new[]{false,true})foreach(bool conflict in new[]{false,true}){
            string path=Path.Combine(root,"profile"+generic+conflict);Directory.CreateDirectory(path);
            var old=Idas3ControlBindings.Defaults();old.version=2;Array.Resize(ref old.actions,9);
            var actions=Idas3ControlBindings.Defaults().actions;Array.Resize(ref actions,9);
            if(conflict)actions[6].pad=Idas3ControlBindings.PadInput.RightThumb;
            old.controllerProfiles=new[]{new Idas3ControlBindings.ControllerProfile{key="saved-device",label="Saved device",generic=generic,actions=actions}};
            File.WriteAllText(Path.Combine(path,"controls.json"),JsonUtility.ToJson(old));var m=new Idas3ControlBindings();m.Initialize(path);
            Check(m.LastError==null,"Device profile migration failed");m.SelectControllerProfile("saved-device","Saved device",generic);
            for(int i=0;i<9;i++)Check(JsonUtility.ToJson(m.Current.actions[i])==JsonUtility.ToJson(actions[i]),"Migration changed a saved device action");
            Check(m.Current.actions[9].key1==KeyCode.H,"Device selection lost headlight keyboard binding");
            Check(m.Current.actions[9].pad==(!generic&&!conflict?Idas3ControlBindings.PadInput.RightThumb:Idas3ControlBindings.PadInput.None),"Device migration stole a button or assigned an unknown wheel input");
            m.BeginEdit();Check(m.ApplyDraft(),"Could not save migrated profile");
            var reload=new Idas3ControlBindings();reload.Initialize(path);reload.SelectControllerProfile("saved-device","Saved device",generic);
            Check(Idas3ControlBindings.Equivalent(reload.Current,m.Current),"Saved device migration did not survive reload");
        }
        File.WriteAllText("Verification/effects-headlights-20260922/binding-checks.txt","PASS "+checks+" headlight binding and migration checks\n");
        Idas3ControllerMenuChecks.Run();
    }
}
