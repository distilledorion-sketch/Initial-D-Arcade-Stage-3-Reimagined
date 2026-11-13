using System;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine.InputSystem;

// Output only: consumes the original producer's current/undelivered amplitude.
// Flycast's SDL fallback sends its sine amplitude to both gamepad motors. A pad
// cannot reproduce steering torque or the cabinet's exact 31.5 Hz wheel motion.
public sealed class Idas3ControllerVibration : IDisposable
{
    internal interface ITarget : IDisposable { bool Connected {get;} void Set(float low,float high); }
    internal sealed class UnityTarget : ITarget {
        private readonly Gamepad pad;
        private bool active;
        internal UnityTarget(Gamepad pad){this.pad=pad;}
        public bool Connected=>pad!=null&&pad.added&&pad.enabled;
        public void Set(float low,float high){active=low>0||high>0;if(Connected)pad.SetMotorSpeeds(low,high);}
        public void Dispose(){if(active)Set(0,0);}
    }
    [StructLayout(LayoutKind.Sequential)] private struct Motors {public ushort low,high;}
    [DllImport("xinput9_1_0.dll",EntryPoint="XInputSetState")]
    private static extern uint SetXInput(uint slot,ref Motors motors);
    internal sealed class XboxTarget : ITarget {
        private readonly uint slot;
        private readonly Func<bool> connected;
        private readonly Action<uint,float,float> writer;
        internal uint Slot=>slot;
        private readonly object gate=new object();
        private Timer watchdog;
        private long expires;
        private bool disposed,active;
        internal XboxTarget(uint slot,Func<bool> connected,Action<uint,float,float> writer=null){this.slot=slot;this.connected=connected;this.writer=writer??WriteNative;}
        public bool Connected=>!disposed&&connected();
        private static void WriteNative(uint slot,float low,float high){
            var motors=new Motors{low=(ushort)(low*65535),high=(ushort)(high*65535)};
            if(SetXInput(slot,ref motors)!=0&&(low!=0||high!=0))throw new InvalidOperationException("Controller vibration unavailable.");
        }
        public void Set(float low,float high){lock(gate){
            if(disposed)return;
            active=low>0||high>0;
            // XInput has no duration parameter. A render-thread stall must not
            // leave a motor latched on; fresh driving renews this short lease.
            expires=active?System.Diagnostics.Stopwatch.GetTimestamp()+System.Diagnostics.Stopwatch.Frequency/10:0;
            if(active&&watchdog==null)watchdog=new Timer(Expire,null,25,25);
            writer(slot,low,high);
        }}
        private void Expire(object unused){lock(gate){
            if(disposed||!active||System.Diagnostics.Stopwatch.GetTimestamp()<expires)return;
            active=false;expires=0;try{writer(slot,0,0);}catch(Exception){}
        }}
        public void Dispose(){lock(gate){
            if(disposed)return;
            try{if(active)writer(slot,0,0);}catch(Exception){}
            active=false;disposed=true;watchdog?.Dispose();watchdog=null;
        }}
    }
    private readonly bool disableOutput;
    private ITarget target;
    private bool disposed,haveTick,haveTime,sending;
    private ulong lastTick;
    private double tickChangedAt,lastFrame,nextSend,retryAt,pendingRumbleAt;
    private int pendingRumble;
    public Idas3ControllerVibration(bool disableOutput=false){this.disableOutput=disableOutput;}
    internal void Update(Idas3GameOptions.Values settings,ITarget device,Idas3Native.FeedbackState state,bool allowed,double now){
        if(disposed)return;
        if(!ReferenceEquals(target,device)){Stop();target=device;retryAt=0;}
        if(disableOutput||settings==null||!settings.controllerVibration||!Finite(settings.controllerVibrationStrength)||
            settings.controllerVibrationStrength<=0||!allowed||target==null||!target.Connected||!Idas3WheelFeedback.Valid(state)||
            double.IsNaN(now)||double.IsInfinity(now)||now<0){Stop();return;}
        if(haveTime&&now<lastFrame){Stop();tickChangedAt=now-.101;lastFrame=now;return;}
        lastFrame=now;haveTime=true;
        if(now-pendingRumbleAt>.1)pendingRumble=0;
        bool fresh=!haveTick||state.simulationTicks!=lastTick;
        if(fresh){lastTick=state.simulationTicks;tickChangedAt=now;haveTick=true;
            if(state.peakRumbleAmplitude>pendingRumble){pendingRumble=state.peakRumbleAmplitude;pendingRumbleAt=now;}}
        else if(now-tickChangedAt>.1){Stop();return;}
        if(now<retryAt){pendingRumble=0;return;}
        if(now<nextSend)return;
        int amplitude=Math.Max(state.rumbleAmplitude,pendingRumble);
        float motor=Idas3WheelFeedback.Rumble(amplitude)*Math.Min(settings.controllerVibrationStrength,1);
        pendingRumble=0;
        if(motor==0){StopMotors();return;}
        try{sending=true;target.Set(motor,motor);nextSend=now+1.0/60;}
        catch(Exception){Stop();retryAt=now+1;}
    }
    private static bool Finite(float v)=>!float.IsNaN(v)&&!float.IsInfinity(v);
    private void StopMotors(){if(sending){try{target?.Set(0,0);}catch(Exception){}sending=false;}nextSend=0;}
    public void Stop(){StopMotors();pendingRumble=0;}
    public void Dispose(){if(disposed)return;Stop();target=null;disposed=true;}
}
