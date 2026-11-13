using System;
using System.Collections.Generic;

public sealed partial class Idas3ControlBindings
{
    sealed class RigState { public bool seen; public int blocked; public readonly Dictionary<string,Idas3ControllerControl> controls=new Dictionary<string,Idas3ControllerControl>(StringComparer.Ordinal); }
    readonly Dictionary<string,RigState> rigStates=new Dictionary<string,RigState>(StringComparer.Ordinal);
    readonly List<string> expiredRigStates=new List<string>();
    readonly float[] rigAmounts=new float[ActionCount];
    readonly HashSet<string> rigStartSources=new HashSet<string>(StringComparer.Ordinal);

    static bool GearAction(int action)=>action==(int)ActionId.ShiftUp||action==(int)ActionId.ShiftDown||action>=(int)ActionId.Gear1;
    public bool IsWheelAccessory(string key)
    {
        if(string.IsNullOrEmpty(key)||!savedProfiles.TryGetValue(key,out var profile))return false;
        if(profile.generic)return true;
        // Ordinary gamepads remain independent. A saved H-pattern binding or
        // remapped sequential shifter opts that device's gears into the rig.
        for(int i=(int)ActionId.Gear1;i<ActionCount;++i)if(ControllerIdentity(profile.actions[i])!=null)return true;
        return (ControllerIdentity(profile.actions[4])!=null&&
                (!string.IsNullOrEmpty(profile.actions[4].controlPath)||profile.actions[4].pad!=PadInput.B))||
               (ControllerIdentity(profile.actions[5])!=null&&
                (!string.IsNullOrEmpty(profile.actions[5].controlPath)||profile.actions[5].pad!=PadInput.X));
    }
    // Profiles remain independent in the editor. Saved HID bindings and the
    // gear bindings of configured Xbox-style shifters join an active wheel.
    void PollRig(IReadOnlyList<Idas3ControllerDevices.RigSample> samples)
    {
        Array.Clear(rigAmounts,0,rigAmounts.Length);
        rigStartSources.Clear();
        foreach(var state in rigStates.Values)state.seen=false;
        if(genericProfile&&samples!=null)foreach(var sample in samples){
            if(sample==null||string.IsNullOrEmpty(sample.key)||sample.profile==activeProfileKey||
                !savedProfiles.TryGetValue(sample.profile,out var profile)||!IsWheelAccessory(sample.profile))continue;
            int matches=0;foreach(var other in samples)if(other.profile==sample.profile)++matches;
            if(matches!=1)continue; // Serial-less identical devices are ambiguous.
            bool fresh=!rigStates.TryGetValue(sample.key,out var state);
            if(fresh){state=new RigState();rigStates.Add(sample.key,state);}
            state.seen=true;state.controls.Clear();
            if(sample.controls!=null)foreach(var control in sample.controls)
                if(control!=null&&!string.IsNullOrEmpty(control.path)&&Finite(control.value))state.controls[control.path]=control;
            for(int i=0;i<rigAmounts.Length;++i){
                if(!profile.generic&&!GearAction(i))continue;
                var binding=profile.actions[i];float amount=0;
                if(!string.IsNullOrEmpty(binding.controlPath)&&state.controls.TryGetValue(binding.controlPath,out var control)){
                    float extent=binding.controlDirection>0?binding.controlMax-binding.controlRest:binding.controlRest-binding.controlMin;
                    amount=binding.controlButton?(control.value>.5f?1:0):extent>.0001f?Math.Max(0,Math.Min(1,(control.value-binding.controlRest)*binding.controlDirection/extent)):0;
                }
                else if(string.IsNullOrEmpty(binding.controlPath))amount=RawMagnitude(sample.pad,binding.pad)/32768f;
                int bit=1<<i;
                if(fresh&&amount>.15f)state.blocked|=bit;
                if(amount<=.15f)state.blocked&=~bit;
                if((state.blocked&bit)==0){
                    rigAmounts[i]=Math.Max(rigAmounts[i],amount);
                    if(i==(int)ActionId.Start&&amount>=.5f)rigStartSources.Add("rig:"+sample.key+":"+binding.controlPath);
                }
            }
        }
        expiredRigStates.Clear();foreach(var pair in rigStates)if(!pair.Value.seen)expiredRigStates.Add(pair.Key);
        foreach(var key in expiredRigStates)rigStates.Remove(key);
    }
}
