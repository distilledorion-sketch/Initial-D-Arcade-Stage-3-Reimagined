using System;

// High-resolution HID pedals stay separate from the stable byte-trigger ABI.
// The original native conditioner still owns calibration and pedal response.
internal static class Idas3PrecisePedals
{
    internal delegate int LegacyStep(ref Idas3Native.FrameInput frame);
    internal delegate int PreciseStep(ref Idas3Native.FrameInput frame,ref Idas3Native.PedalInput pedals);
    internal struct Sample
    {
        internal Idas3Native.PedalInput pedals;
        internal uint legacyThrottle,legacyBrake;
    }
    internal static Idas3Native.PedalInput Empty => new Idas3Native.PedalInput{size=16,version=1};
    internal static ushort Adc(float amount)
    {
        if(float.IsNaN(amount)||float.IsInfinity(amount))throw new ArgumentException("Pedal must be finite");
        int value=(int)Math.Round(Math.Max(0,Math.Min(1,amount))*32767f)*2;
        // Flycast's JVS midpoint notch and final signed-byte wire range.
        if(value>=0x8000&&value<0x8100)value=0x8100;
        return (ushort)Math.Min(value,0xff7f);
    }
    internal static Sample Mapped(Idas3Native.FrameInput frame,bool throttleValid,float throttle,bool brakeValid,float brake)
    {
        var result=new Sample{pedals=Empty,legacyThrottle=frame.rightTrigger,legacyBrake=frame.leftTrigger};
        if(throttleValid){result.pedals.validMask|=1;result.pedals.throttleAdc=Adc(throttle);}
        if(brakeValid){result.pedals.validMask|=2;result.pedals.brakeAdc=Adc(brake);}
        return result;
    }
    internal static Sample Physical(Idas3Native.FrameInput frame,Idas3ControlBindings.PadState pad) =>
        Mapped(frame,pad.connected&&pad.preciseTriggers,pad.preciseRightTrigger,pad.connected&&pad.preciseTriggers,pad.preciseLeftTrigger);
    internal static Idas3Native.PedalInput ForSubmission(Idas3Native.FrameInput frame,Sample sample,bool diagnostic)
    {
        // Run after all menu/focus/diagnostic routing. A later neutralization
        // or replacement packet must never retain a previous precise value.
        if(diagnostic||(frame.flags&3u)!=1u||frame.padConnected==0||
           frame.rightTrigger!=sample.legacyThrottle||frame.leftTrigger!=sample.legacyBrake)return Empty;
        return sample.pedals.size==16?sample.pedals:Empty;
    }
    internal static int Submit(ref Idas3Native.FrameInput frame,Sample sample,bool diagnostic,
        ref bool unavailable,LegacyStep legacy,PreciseStep precise)
    {
        if(!unavailable){
            var pedals=ForSubmission(frame,sample,diagnostic);
            try{return precise(ref frame,ref pedals);}
            catch(EntryPointNotFoundException){unavailable=true;}
        }
        return legacy(ref frame);
    }
}
