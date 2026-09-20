using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

// Windows wheel output is separate from device input and source physics.
//
// The force model is NOT here. The cabinet's own force-feedback board is ported
// in Native/src/original_ffb.cpp, and the commands sent to it come from
// Native/src/original_ffb_owner.cpp; this class only decides WHEN a wheel may be
// driven and hands the native side one frame of telemetry. That split is
// deliberate: the arcade board quantises torque to a 14-bit field, eases its
// authority in over the first 256 frames, caps it with a per-car limit read out
// of the arcade's own table, plays a fixed 31.5 Hz rumble and sends no spring
// command at all -- none of which survives being re-expressed as a single float
// here.
public sealed class Idas3WheelFeedback : IDisposable
{
    public sealed class DeviceChoice { public string id,name; internal uint vendorId,productId; }
    internal struct InputIdentity { public string key; public bool connected; public uint vendorId,productId; }

    // One frame of driving, in the units the native owner reads. This is the
    // whole contract between the game and the cabinet board.
    internal struct CabinetRequest {
        public bool driving,wallContact,invert;
        public int carIndex;
        public float speedKmh,steering,headingError,wallLateral,impact,strength,deltaSeconds;
    }

    internal interface IBackend {
        List<DeviceChoice> Discover(); bool Send(string id,CabinetRequest request); void Stop(); void Shutdown(); string Status {get;}
    }
    private sealed class NativeBackend : IBackend
    {
        private const string Library="Idas3WheelFeedback";
        [StructLayout(LayoutKind.Sequential)] private struct NativeDevice {
            public uint size;
            [MarshalAs(UnmanagedType.ByValArray,SizeConst=40)] public byte[] id;
            [MarshalAs(UnmanagedType.ByValArray,SizeConst=128)] public byte[] name;
            public uint vendorId,productId;
        }
        // Mirrors Idas3WheelRaceState in wheel_feedback_backend.h, 52 bytes.
        [StructLayout(LayoutKind.Sequential)] private struct NativeRaceState {
            public uint size,version,driving,carIndex;
            public float speedKmh,steering,headingError,wallLateral,impact;
            public uint wallContact;
            public float strength;
            public uint invert;
            public float deltaSeconds;
        }
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3WheelRefreshDevices();
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3WheelGetDevice(int index,ref NativeDevice device);
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3WheelUpdateCabinet([MarshalAs(UnmanagedType.LPUTF8Str)]string id,ref NativeRaceState state);
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern void Idas3WheelStop();
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern void Idas3WheelShutdown();
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3WheelCopyStatus([Out]byte[] text,int capacity);
        private static string Utf8(byte[] bytes){int length=Array.IndexOf(bytes,(byte)0);return Encoding.UTF8.GetString(bytes,0,length<0?bytes.Length:length);}
        public List<DeviceChoice> Discover(){
            int count=Idas3WheelRefreshDevices();if(count<0)throw new InvalidOperationException(Status);
            if(count>64)throw new InvalidOperationException("Too many force-feedback devices.");
            var result=new List<DeviceChoice>();
            for(int i=0;i<count;++i){var value=new NativeDevice{size=(uint)Marshal.SizeOf<NativeDevice>(),id=new byte[40],name=new byte[128]};
                if(value.size!=180||Idas3WheelGetDevice(i,ref value)!=1)continue;
                result.Add(new DeviceChoice{id=Utf8(value.id),name=Utf8(value.name),vendorId=value.vendorId,productId=value.productId});}
            return result;
        }
        public bool Send(string id,CabinetRequest request){
            var state=new NativeRaceState{
                size=(uint)Marshal.SizeOf<NativeRaceState>(),version=1,
                driving=request.driving?1u:0u,
                carIndex=(uint)Math.Max(0,Math.Min(34,request.carIndex)),
                speedKmh=request.speedKmh,steering=request.steering,headingError=request.headingError,
                wallLateral=request.wallLateral,impact=request.impact,
                wallContact=request.wallContact?1u:0u,
                strength=request.strength,invert=request.invert?1u:0u,
                deltaSeconds=request.deltaSeconds};
            if(state.size!=52)throw new InvalidOperationException("Wheel race state layout changed.");
            return Idas3WheelUpdateCabinet(id,ref state)==1;
        }
        public void Stop()=>Idas3WheelStop();
        public void Shutdown()=>Idas3WheelShutdown();
        public string Status {get{var bytes=new byte[1024];Idas3WheelCopyStatus(bytes,bytes.Length);return Utf8(bytes);}}
    }

    // Packs one frame of telemetry. Nothing here shapes a force; it only refuses
    // to pass on values the native owner would have to reject anyway, so a bad
    // frame stops the wheel here rather than after crossing the boundary.
    internal static bool Pack(Idas3Native.WheelState state,int car,float dt,float strength,bool invert,out CabinetRequest request){
        request=default;
        if((state.flags&1)==0)return false;
        if(!Finite(state.speed)||!Finite(state.steering)||!Finite(state.headingError)||
           !Finite(state.wallLateral)||!Finite(state.impact)||!Finite(dt)||dt<=0||!Finite(strength)||strength<=0)return false;
        request=new CabinetRequest{
            driving=true,
            // The cabinet's force limit is per car; without it every car would
            // pull with the same weight, which the arcade's table says they do not.
            carIndex=car,
            // The native side reads km/h; WheelState.speed is metres per second.
            speedKmh=Clamp(Math.Abs(state.speed),0,200)*3.6f,
            steering=Clamp(state.steering,-1,1),
            headingError=Clamp(state.headingError,-3.2f,3.2f),
            wallLateral=Clamp(state.wallLateral,-1,1),
            impact=Clamp(state.impact,0,1),
            wallContact=(state.flags&2)!=0,
            strength=Clamp(strength,0,1),
            invert=invert,
            deltaSeconds=Clamp(dt,.001f,.05f)};
        return true;
    }

    private readonly IBackend backend;
    private readonly bool disableOutput;
    private readonly List<DeviceChoice> choices=new List<DeviceChoice>();
    private string sendingDevice,desiredDevice,inputKey;
    private bool sending,disposed,haveTick;
    private ulong lastTick;
    private double tickChangedAt,nextScan,retryAt,lastSendAt;
    public IReadOnlyList<DeviceChoice> Choices=>choices;
    public string StatusText {get;private set;}="Force feedback is off.";
    public Idas3WheelFeedback(bool disableOutput=false):this(new NativeBackend(),disableOutput){}
    internal Idas3WheelFeedback(IBackend backend,bool disableOutput=false){
        this.backend=backend??throw new ArgumentNullException(nameof(backend));this.disableOutput=disableOutput;
        choices.Add(new DeviceChoice{id="",name="AUTOMATIC (ACTIVE WHEEL)"});
    }
    internal static bool Finite(float value)=>!float.IsNaN(value)&&!float.IsInfinity(value);
    internal static float Clamp(float value,float min,float max)=>Math.Max(min,Math.Min(max,value));
    public void RefreshDevices(){
        if(disposed)return;
        try{
            var found=backend.Discover();choices.RemoveRange(1,choices.Count-1);choices.AddRange(found);
            StatusText=found.Count==0?"No compatible force-feedback wheel found. Check its Windows driver.":found.Count+" force-feedback device(s) available.";
        }catch(Exception error){choices.RemoveRange(1,choices.Count-1);Stop();StatusText="Wheel feedback unavailable: "+error.Message;}
    }
    private string Resolve(string preference,InputIdentity input){
        if(!string.IsNullOrEmpty(preference)){
            for(int i=1;i<choices.Count;++i)if(string.Equals(choices[i].id,preference,StringComparison.OrdinalIgnoreCase))return choices[i].id;
            StatusText="Selected feedback wheel is disconnected. Refresh or choose a connected wheel.";return null;
        }
        string match=null;int count=0;
        if(input.vendorId!=0&&input.productId!=0)for(int i=1;i<choices.Count;++i)
            if(choices[i].vendorId==input.vendorId&&choices[i].productId==input.productId){match=choices[i].id;++count;}
        if(count==1)return match;
        StatusText=count>1?"Several matching wheels found. Choose the feedback wheel explicitly.":"Select your feedback wheel here, and bind its steering in CONTROLS.";
        return null;
    }
    internal void Update(Idas3GameOptions.Values settings,InputIdentity input,Idas3Native.WheelState state,
        int car,bool allowed,double now)
    {
        if(disposed)return;
        if(inputKey!=input.key){Stop();inputKey=input.key;desiredDevice=null;retryAt=0;}
        if(settings==null||!settings.wheelForceFeedback||!Finite(settings.wheelFeedbackStrength)||settings.wheelFeedbackStrength<=0){Stop();StatusText="Force feedback is off.";return;}
        if(disableOutput){Stop();StatusText="Hardware output is disabled during automated testing.";return;}
        if(!allowed||!input.connected||state.size!=40||state.version!=1||(state.flags&1)==0){Stop();StatusText="Feedback rests in menus, while paused or unfocused, and outside driving.";return;}
        if(double.IsNaN(now)||double.IsInfinity(now)){Stop();return;}
        if(now>=nextScan){RefreshDevices();nextScan=now+2;}
        string selected=Resolve(settings.wheelFeedbackDevice,input);
        if(selected==null){Stop();return;}
        if(desiredDevice!=selected){Stop();desiredDevice=selected;retryAt=0;}
        if(!haveTick||state.simulationTicks!=lastTick){lastTick=state.simulationTicks;tickChangedAt=now;haveTick=true;}
        else if(now-tickChangedAt>.1){Stop();StatusText="Feedback stopped: driving updates are stale.";return;}
        if(now<retryAt||sending&&now-lastSendAt<1.0/60)return;
        float dt=sending?(float)(now-lastSendAt):1f/60;
        if(!Pack(state,car,dt,settings.wheelFeedbackStrength,settings.wheelFeedbackInvert,out var request)){
            Stop();StatusText="Feedback stopped: invalid driving data.";return;
        }
        try{
            sendingDevice=selected;
            if(!backend.Send(selected,request)){string error=backend.Status;Stop();retryAt=now+1;StatusText=error;return;}
            sending=true;lastSendAt=now;StatusText="Feedback active — cabinet board: torque, centring, damper and rumble.";
        }catch(Exception error){Stop();retryAt=now+1;StatusText="Wheel feedback unavailable: "+error.Message;}
    }
    public void Stop(){
        // The native Stop also resets the board and its owner, so the next race
        // starts from the centre and eases in again rather than resuming a force.
        try{if(sending||sendingDevice!=null)backend.Stop();}catch(Exception){}
        sending=false;sendingDevice=null;
    }
    public void Dispose(){if(disposed)return;Stop();try{backend.Shutdown();}catch(Exception){}disposed=true;}
}
