using System;
using System.Collections.Generic;

public sealed partial class Idas3ControlBindings
{
    sealed class RigState { public bool seen; public int blocked; public readonly Dictionary<string,Idas3ControllerControl> controls=new Dictionary<string,Idas3ControllerControl>(StringComparer.Ordinal); }
    readonly Dictionary<string,RigState> rigStates=new Dictionary<string,RigState>(StringComparer.Ordinal);
    readonly List<string> expiredRigStates=new List<string>();
    readonly float[] rigAmounts=new float[ActionCount];

    // Profiles remain independent in the editor. Only explicitly saved generic
    // device bindings join the active wheel rig; gamepads and keyboard-only
    // selection retain their single-source behavior.
    void PollRig(IReadOnlyList<Idas3ControllerDevices.RigSample> samples)
    {
        Array.Clear(rigAmounts,0,rigAmounts.Length);
        foreach(var state in rigStates.Values)state.seen=false;
        if(genericProfile&&samples!=null)foreach(var sample in samples){
            if(sample==null||string.IsNullOrEmpty(sample.key)||sample.profile==activeProfileKey||
                !savedProfiles.TryGetValue(sample.profile,out var profile)||!profile.generic)continue;
            int matches=0;foreach(var other in samples)if(other.profile==sample.profile)++matches;
            if(matches!=1)continue; // Serial-less identical devices are ambiguous.
            bool fresh=!rigStates.TryGetValue(sample.key,out var state);
            if(fresh){state=new RigState();rigStates.Add(sample.key,state);}
            state.seen=true;state.controls.Clear();
            if(sample.controls!=null)foreach(var control in sample.controls)
                if(control!=null&&!string.IsNullOrEmpty(control.path)&&Finite(control.value))state.controls[control.path]=control;
            for(int i=0;i<rigAmounts.Length;++i){
                var binding=profile.actions[i];float amount=0;
                if(!string.IsNullOrEmpty(binding.controlPath)&&state.controls.TryGetValue(binding.controlPath,out var control)){
                    float extent=binding.controlDirection>0?binding.controlMax-binding.controlRest:binding.controlRest-binding.controlMin;
                    amount=binding.controlButton?(control.value>.5f?1:0):extent>.0001f?Math.Max(0,Math.Min(1,(control.value-binding.controlRest)*binding.controlDirection/extent)):0;
                }
                int bit=1<<i;
                if(fresh&&amount>.15f)state.blocked|=bit;
                if(amount<=.15f)state.blocked&=~bit;
                if((state.blocked&bit)==0)rigAmounts[i]=Math.Max(rigAmounts[i],amount);
            }
        }
        expiredRigStates.Clear();foreach(var pair in rigStates)if(!pair.Value.seen)expiredRigStates.Add(pair.Key);
        foreach(var key in expiredRigStates)rigStates.Remove(key);
    }
}
