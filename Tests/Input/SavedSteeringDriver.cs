using System;
using System.Reflection;
using System.Runtime.InteropServices;
public static class Idas3Native {
    public struct WheelState {
        public uint size,version,flags;public ulong simulationTicks;
        public float speed,steering,headingError,wallLateral,impact;
    }
}
public static class Idas3GameOptions {
    public class Values { public bool wheelForceFeedback,wheelFeedbackInvert;public float wheelFeedbackStrength;public string wheelFeedbackDevice; }
}
static class SavedSteeringDriver {
    static int count;
    static void Check(bool value,string label){if(!value)throw new Exception(label);++count;Console.WriteLine("PASS\t"+label);}
    static int Main(){try{
        var identity=new Idas3WheelFeedback.InputIdentity{key="test",connected=false,vendorId=0,productId=0};
        Check(!identity.connected&&identity.key=="test"&&identity.vendorId==0&&identity.productId==0,"Test identity has no physical device");
        var state=new Idas3Native.WheelState{size=40,version=1,flags=1,speed=20};
        Idas3WheelFeedback.CabinetRequest request;
        Check(Idas3WheelFeedback.Pack(state,0,.016f,.5f,false,out request)&&request.cardSteeringLevel==-1,"Legacy scene telemetry retains legacy steering dial");
        for(uint level=0;level<9;++level){
            state.flags=1u|4u|(level<<8);
            Check(Idas3WheelFeedback.Pack(state,13,.016f,.5f,false,out request)&&request.cardSteeringLevel==(int)level,"Saved dial crosses managed request: "+level);
            Check(request.carIndex==13&&request.strength==.5f&&!request.wallContact,"Saved dial preserves car, strength and contact: "+level);
        }
        for(uint level=9;level<16;++level){state.flags=1u|4u|(level<<8);Check(!Idas3WheelFeedback.Pack(state,0,.016f,.5f,false,out request),"Invalid dial stops output: "+level);}
        state.flags=4;
        Check(!Idas3WheelFeedback.Pack(state,0,.016f,.5f,false,out request),"Inactive driving never sends saved steering output");
        var backend=typeof(Idas3WheelFeedback).GetNestedType("NativeBackend",BindingFlags.NonPublic);
        var wire=backend.GetNestedType("NativeRaceState",BindingFlags.NonPublic);
        Check(Marshal.SizeOf(wire)==56,"Native v2 wire is 56 bytes");
        Check(Marshal.OffsetOf(wire,"cardSteeringLevel").ToInt32()==52,"Saved dial follows unchanged legacy wire prefix");
        Check(Marshal.OffsetOf(wire,"deltaSeconds").ToInt32()==48,"Legacy delta-time offset is preserved");
        Console.WriteLine("PASS "+count+" saved steering transport checks; no hardware calls");return 0;
    }catch(Exception error){Console.Error.WriteLine(error);return 1;}}
}
