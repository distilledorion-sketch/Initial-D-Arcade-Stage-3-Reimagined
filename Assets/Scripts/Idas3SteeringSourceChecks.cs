using System;
using System.IO;
using UnityEngine;

// Replay steering-source labels written to native input bits 11..13.
public static class Idas3SteeringSourceChecks
{
    public static void Run(){
        string root=Path.GetFullPath("Verification/replay-steering-source/bindings-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        int checks=0;void Check(bool ok,string why){++checks;if(!ok)throw new Exception(why);}
        var map=new Idas3ControlBindings();map.Initialize(root);
        uint Source(KeyCode key=KeyCode.None,Idas3ControlBindings.PadState pad=default,Idas3ControllerControl[] controls=null){
            map.Poll(k=>k==key,pad,0,controls);var f=new Idas3Native.FrameInput{flags=1};map.ApplyDriving(ref f);
            Check((f.flags&0x701u)==1,"Steering source disturbed focus or direct gear");return (f.flags>>11)&7;
        }
        var stick=new Idas3ControlBindings.PadState{connected=true};
        Check(Source()==0&&Source(KeyCode.None,stick)==0,"Idle steering was labelled");
        Check(Source(KeyCode.A)==1&&Source(KeyCode.RightArrow,stick)==1,"Keyboard steering was not labelled keyboard");
        stick.thumbLX=-20000;Check(Source(KeyCode.None,stick)==3,"Left stick was not labelled analog");
        stick.thumbLX=1500;Check(Source(KeyCode.D,stick)==1,"Stick drift relabelled a keyboard tick");
        stick.thumbLX=30000;Check(Source(KeyCode.A,stick)==3,"A full stick deflection lost to a held key");
        map.BeginEdit();Check(map.TrySetDraftPad(Idas3ControlBindings.ActionId.SteerLeft,Idas3ControlBindings.PadInput.DPadLeft)&&map.ApplyDraft(),"Bind D-pad steering");
        Check(Source(KeyCode.None,new Idas3ControlBindings.PadState{connected=true,buttons=4})==2,"D-pad steering was not labelled D-pad");
        var steer=new Idas3ControllerControl{path="stick/x",label="Steering",value=0};
        map.SelectControllerProfile("wheel","wheel",true);
        Check(map.TrySetDraftControl(Idas3ControlBindings.ActionId.SteerLeft,steer,-1,0)&&map.TrySetDraftControl(Idas3ControlBindings.ActionId.SteerRight,steer,1,0)&&map.ApplyDraft(),"Bind wheel steering");
        var wheel=new[]{steer};Source(KeyCode.None,default,wheel);
        Check(Source(KeyCode.None,default,wheel)==0,"Centred wheel was labelled");
        steer.value=.4f;Check(Source(KeyCode.None,default,wheel)==4,"Generic wheel steering was not labelled wheel");
        Check(Source(KeyCode.A,default,wheel)==4,"A turned wheel lost to a held key");
        File.WriteAllText(Path.Combine(root,"PASS.txt"),checks+" steering source label checks passed");Debug.Log("PASS "+checks+" steering source checks");
    }
}
