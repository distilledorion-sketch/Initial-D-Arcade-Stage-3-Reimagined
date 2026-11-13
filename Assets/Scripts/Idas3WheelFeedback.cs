using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

// Windows wheel output is separate from device input and source physics.
//
// The original producer runs once per native simulation tick. This adapter
// converts its completed command values to the device protocol; rendering and
// worker delivery never advance the original counter or reconstruct physics.
public sealed class Idas3WheelFeedback : IDisposable
{
    public sealed class DeviceChoice { public string id,name; internal uint vendorId,productId; }
    internal struct InputIdentity { public string key; public bool connected; public uint vendorId,productId; }

    // Normalized raw board output. The device backend applies power once.
    internal struct CabinetRequest {
        public float power,torque,damperStrength,damperParameter,rumbleIntensity,rumbleFrequencyHz;
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
        // Existing Idas3WheelCabinetState v1 ABI, 40 bytes.
        [StructLayout(LayoutKind.Sequential)] private struct NativeCabinetState {
            public uint size,version;
            public float power,torque,spring,damperStrength,damperParameter,rumbleIntensity,rumbleFrequencyHz;
            public uint active;
        }
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3WheelRefreshDevices();
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3WheelGetDevice(int index,ref NativeDevice device);
        [DllImport(Library,CallingConvention=CallingConvention.Cdecl)] private static extern int Idas3WheelSetCabinetState([MarshalAs(UnmanagedType.LPUTF8Str)]string id,ref NativeCabinetState state);
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
            var state=new NativeCabinetState{
                size=(uint)Marshal.SizeOf<NativeCabinetState>(),version=1,active=1,
                power=request.power,torque=request.torque,spring=0,
                damperStrength=request.damperStrength,damperParameter=request.damperParameter,
                rumbleIntensity=request.rumbleIntensity,rumbleFrequencyHz=request.rumbleFrequencyHz};
            if(state.size!=40)throw new InvalidOperationException("Wheel board state layout changed.");
            return Idas3WheelSetCabinetState(id,ref state)==1;
        }
        public void Stop()=>Idas3WheelStop();
        public void Shutdown()=>Idas3WheelShutdown();
        public string Status {get{var bytes=new byte[1024];Idas3WheelCopyStatus(bytes,bytes.Length);return Utf8(bytes);}}
    }

    internal static bool Valid(Idas3Native.FeedbackState state)=>state.size==40&&state.version==1&&state.flags==1&&
        state.torqueCode>=0&&state.torqueCode<=255&&state.damperParameter>=0&&state.damperParameter<=127&&
        state.rumbleAmplitude>=0&&state.rumbleAmplitude<=47&&state.peakRumbleAmplitude>=0&&state.peakRumbleAmplitude<=47&&
        state.rumbleFrequencyHalfHz<=127&&((state.rumbleAmplitude<=1&&state.peakRumbleAmplitude<=1)||state.rumbleFrequencyHalfHz>0);
    internal static float Rumble(int amplitude)=>amplitude<=1?0:(amplitude-1)/80f;
    internal static bool Pack(Idas3Native.FeedbackState state,float strength,bool invert,out CabinetRequest request){
        request=default;
        if(!Valid(state)||!Finite(strength)||strength<=0)return false;
        // Original steering has the opposite sign to the host wheel axis.
        // This is the only polarity conversion; the backend keeps this sign.
        float torque=(128-state.torqueCode)/128f;
        request=new CabinetRequest{
            power=Clamp(strength,0,1),torque=invert?-torque:torque,
            damperStrength=2f/127,damperParameter=state.damperParameter/127f,
            rumbleIntensity=Rumble(Math.Max(state.rumbleAmplitude,state.peakRumbleAmplitude)),
            rumbleFrequencyHz=state.rumbleFrequencyHalfHz*.5f};
        return true;
    }

    private readonly IBackend backend;
    private readonly bool disableOutput;
    private readonly List<DeviceChoice> choices=new List<DeviceChoice>();
    private string sendingDevice,desiredDevice,inputKey;
    private bool sending,disposed,haveTick,haveTime;
    private ulong lastTick;
    private double tickChangedAt,nextScan,retryAt,lastSendAt,lastObservedAt,pendingRumbleAt;
    private int pendingRumble;
    private uint pendingRumbleFrequencyHalfHz;
    public IReadOnlyList<DeviceChoice> Choices=>choices;
    public string StatusText {get;private set;}="Force feedback is off.";
    public Idas3WheelFeedback(bool disableOutput=false):this(disableOutput?(IBackend)new NativeBackend():new QueuedBackend(new NativeBackend()),disableOutput){}
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
    internal void Update(Idas3GameOptions.Values settings,InputIdentity input,Idas3Native.FeedbackState state,
        bool allowed,double now)
    {
        if(disposed)return;
        if(inputKey!=input.key){Stop();inputKey=input.key;desiredDevice=null;retryAt=0;}
        if(settings==null||!settings.wheelForceFeedback||!Finite(settings.wheelFeedbackStrength)||settings.wheelFeedbackStrength<=0){Stop();StatusText="Force feedback is off.";return;}
        if(disableOutput){Stop();StatusText="Hardware output is disabled during automated testing.";return;}
        if(!allowed||!input.connected||!Valid(state)){Stop();StatusText="Feedback rests in menus, while paused or unfocused, and outside driving.";return;}
        if(double.IsNaN(now)||double.IsInfinity(now)||now<0){Stop();return;}
        if(haveTime&&now<lastObservedAt){Stop();tickChangedAt=now-.101;lastObservedAt=now;return;}
        lastObservedAt=now;haveTime=true;
        // Device enumeration opens drivers and can stall for many milliseconds.
        // A healthy active wheel needs no repeated inventory scan. Disconnects
        // still fail through Send; idle/retry discovery can then recover it.
        if(!sending&&now>=nextScan){RefreshDevices();nextScan=now+2;}
        string selected=Resolve(settings.wheelFeedbackDevice,input);
        if(selected==null){Stop();return;}
        if(desiredDevice!=selected){Stop();desiredDevice=selected;retryAt=0;}
        if(now-pendingRumbleAt>.1)pendingRumble=0;
        if(!haveTick||state.simulationTicks!=lastTick){
            lastTick=state.simulationTicks;tickChangedAt=now;haveTick=true;
            if(state.peakRumbleAmplitude>pendingRumble){pendingRumble=state.peakRumbleAmplitude;pendingRumbleFrequencyHalfHz=state.rumbleFrequencyHalfHz;pendingRumbleAt=now;}
        }
        else if(now-tickChangedAt>.1){Stop();StatusText="Feedback stopped: driving updates are stale.";return;}
        if(now<retryAt){pendingRumble=0;return;}
        if(sending&&now-lastSendAt<1.0/60)return;
        state.peakRumbleAmplitude=pendingRumble;
        if(pendingRumble>state.rumbleAmplitude)state.rumbleFrequencyHalfHz=pendingRumbleFrequencyHalfHz;
        if(!Pack(state,settings.wheelFeedbackStrength,settings.wheelFeedbackInvert,out var request)){
            Stop();StatusText="Feedback stopped: invalid driving data.";return;
        }
        try{
            sendingDevice=selected;
            if(!backend.Send(selected,request)){string error=backend.Status;Stop();retryAt=now+1;StatusText=error;return;}
            pendingRumble=0;sending=true;lastSendAt=now;StatusText="Feedback active — original steering resistance and vibration.";
        }catch(Exception error){Stop();retryAt=now+1;StatusText="Wheel feedback unavailable: "+error.Message;}
    }
    public void Stop(){
        // Device release must not advance/reset the native simulation owner.
        try{if(sending||sendingDevice!=null)backend.Stop();}catch(Exception){}
        sending=false;sendingDevice=null;pendingRumble=0;
    }
    public void Dispose(){if(disposed)return;Stop();try{backend.Shutdown();}catch(Exception){}disposed=true;}

    // All driver calls have one owner outside the render thread. This mailbox
    // keeps the newest continuous state and the largest undelivered rumble.
    // The native backend still checks foreground ownership and uses 100 ms
    // effects. Stop discards queued output and takes priority over discovery.
    internal sealed class QueuedBackend : IBackend {
        private readonly IBackend driver;
        private readonly object gate=new object();
        private readonly AutoResetEvent wake=new AutoResetEvent(false);
        private readonly Thread worker;
        private List<DeviceChoice> devices=new List<DeviceChoice>();
        private bool discover,stop,shutdown,pending,failed;
        private string id,status="Discovering wheel feedback devices…";
        private CabinetRequest request;
        private double queuedAt;
        // The caller sends at most60Hz: eight entries cover every event in
        // the100ms lease, including both endpoints. Keep the latest continuous
        // state separate so an expired high peak cannot erase a newer low one.
        private struct RumbleEvent {internal double at;internal float intensity,frequency;}
        private readonly RumbleEvent[] pendingRumbles=new RumbleEvent[8];
        private int pendingRumbleCount;
        private readonly Func<double> monotonicNow;
        internal QueuedBackend(IBackend driver,Func<double> monotonicNow=null){
            this.driver=driver;this.monotonicNow=monotonicNow??(()=>System.Diagnostics.Stopwatch.GetTimestamp()/(double)System.Diagnostics.Stopwatch.Frequency);
            worker=new Thread(Run){IsBackground=true,Name="Wheel feedback output"};
            worker.Start();
        }
        public List<DeviceChoice> Discover(){lock(gate){
            if(!shutdown){discover=true;wake.Set();}
            return new List<DeviceChoice>(devices);
        }}
        public bool Send(string device,CabinetRequest value){lock(gate){
            if(shutdown)return false;
            if(failed){failed=false;return false;}
            double stamp=monotonicNow();
            if(!pending||id!=device)pendingRumbleCount=0;
            int kept=0;
            for(int i=0;i<pendingRumbleCount;++i)if(stamp-pendingRumbles[i].at<=.1)pendingRumbles[kept++]=pendingRumbles[i];
            pendingRumbleCount=kept;
            if(value.rumbleIntensity>0){
                if(pendingRumbleCount==pendingRumbles.Length){
                    Array.Copy(pendingRumbles,1,pendingRumbles,0,--pendingRumbleCount);
                }
                pendingRumbles[pendingRumbleCount++]=new RumbleEvent{at=stamp,intensity=value.rumbleIntensity,frequency=value.rumbleFrequencyHz};
            }
            id=device;request=value;queuedAt=stamp;pending=true;wake.Set();return true;
        }}
        public void Stop(){lock(gate){if(shutdown)return;pending=false;pendingRumbleCount=0;stop=true;wake.Set();}}
        public void Shutdown(){lock(gate){if(shutdown)return;pending=false;pendingRumbleCount=0;shutdown=true;wake.Set();}}
        public string Status {get{lock(gate)return status;}}
        internal bool WaitForShutdown(int milliseconds)=>worker.Join(milliseconds);
        private void Run(){
            var recentRumbles=new RumbleEvent[8];
            try{while(true){
                wake.WaitOne();
                while(true){
                    int command,recentRumbleCount=0;string device=null;CabinetRequest value=default;double stamp=0;
                    lock(gate){
                        if(shutdown)command=3;
                        else if(stop){stop=false;command=2;}
                        else if(pending){
                            pending=false;device=id;value=request;stamp=queuedAt;command=1;
                            recentRumbleCount=pendingRumbleCount;Array.Copy(pendingRumbles,recentRumbles,recentRumbleCount);pendingRumbleCount=0;
                        }
                        else if(discover){discover=false;command=0;}
                        else break;
                    }
                    if(command==3)return;
                    try{
                        if(command==2)driver.Stop();
                        else if(command==0){var found=driver.Discover();string message=driver.Status;lock(gate){devices=found;status=message;}}
                        else {
                            double now=monotonicNow(),age=now-stamp;
                            if(age<0||age>.1){driver.Stop();continue;}
                            for(int i=0;i<recentRumbleCount;++i){
                                var item=recentRumbles[i];double eventAge=now-item.at;
                                if(eventAge>=0&&eventAge<=.1&&item.intensity>value.rumbleIntensity){
                                    value.rumbleIntensity=item.intensity;value.rumbleFrequencyHz=item.frequency;
                                }
                            }
                            bool ok=driver.Send(device,value);
                            // Read driver status before taking the mailbox lock:
                            // even a slow native status call cannot block Update.
                            string message=ok?null:driver.Status;
                            if(!ok){driver.Stop();lock(gate){failed=true;pending=false;pendingRumbleCount=0;status=message;}}
                        }
                    }catch(Exception error){
                        try{driver.Stop();}catch(Exception){}
                        lock(gate){failed=true;pending=false;pendingRumbleCount=0;devices.Clear();status="Wheel feedback unavailable: "+error.Message;}
                    }
                }
            }}finally{try{driver.Stop();}catch(Exception){}try{driver.Shutdown();}catch(Exception){}wake.Dispose();}
        }
    }
}
