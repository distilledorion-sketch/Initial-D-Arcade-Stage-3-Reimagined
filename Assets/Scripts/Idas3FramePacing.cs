using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;
using UnityEngine.LowLevel;

// Pace presentation after this frame's scene work, before PresentAfterDraw.
// Physics retains its existing fixed 60 Hz clock. VSync and uncapped rendering
// bypass this limiter; never stack it with Unity's software targetFrameRate.
public static class Idas3FramePacing
{
    private struct FramePacingLoop { }
    internal struct Schedule
    {
        private double deadline;
        public void Reset(){deadline=0;}
        public double Target(long now,int fps,long frequency){
            if(deadline==0)deadline=now;
            return deadline;
        }
        public void Advance(long now,int fps,long frequency){
            double period=(double)frequency/fps;
            // A slow frame, breakpoint, loading stall or focus change must not
            // produce a burst of short catch-up frames.
            deadline=now>deadline+period*.25?now+period:deadline+period;
        }
    }
    private static IntPtr timer;
    private static int limit;
    private static bool installed,highResolution,periodRequested;
    private static Schedule schedule;
    public static int ActiveLimit=>timer!=IntPtr.Zero&&QualitySettings.vSyncCount==0&&Application.targetFrameRate==-1?limit:0;
    public static bool HighResolutionTimer=>highResolution;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.SubsystemRegistration)]
    private static void Reset(){
        Shutdown();installed=false;
        Application.quitting-=Shutdown;Application.quitting+=Shutdown;
        Application.focusChanged-=FocusChanged;Application.focusChanged+=FocusChanged;
    }
    public static void Configure(bool vSync,int fps){
        fps=Math.Max(0,Math.Min(360,fps));
        QualitySettings.vSyncCount=vSync?1:0;
        if(vSync||fps==0||Application.isEditor||Application.platform!=RuntimePlatform.WindowsPlayer){
            Shutdown();Application.targetFrameRate=fps==0?-1:fps;return;
        }
        if(limit==fps&&timer!=IntPtr.Zero){Application.targetFrameRate=-1;return;}
        Shutdown();
        // High-resolution waitable timers are supported by Windows 10 1803+.
        // Older Windows gets a normal timer with a scoped 1 ms timer period.
        timer=CreateWaitableTimerExW(IntPtr.Zero,null,2,0x00100002);
        highResolution=timer!=IntPtr.Zero;
        if(timer==IntPtr.Zero){
            timer=CreateWaitableTimerExW(IntPtr.Zero,null,0,0x00100002);
            if(timer!=IntPtr.Zero)periodRequested=timeBeginPeriod(1)==0;
        }
        if(timer==IntPtr.Zero){Application.targetFrameRate=fps;return;}
        if(!Install()){Shutdown();Application.targetFrameRate=fps;return;}
        limit=fps;Application.targetFrameRate=-1;schedule.Reset();
    }
    private static bool Install(){
        if(installed)return true;
        var loop=PlayerLoop.GetCurrentPlayerLoop();
        Remove(ref loop);
        if(!Insert(ref loop))return false;
        PlayerLoop.SetPlayerLoop(loop);installed=true;
        return true;
    }
    private static void Remove(ref PlayerLoopSystem loop){
        if(loop.subSystemList==null)return;
        loop.subSystemList=Array.FindAll(loop.subSystemList,node=>node.type!=typeof(FramePacingLoop));
        for(int i=0;i<loop.subSystemList.Length;++i)Remove(ref loop.subSystemList[i]);
    }
    private static bool Insert(ref PlayerLoopSystem loop){
        var children=loop.subSystemList;if(children==null)return false;
        for(int i=0;i<children.Length;++i){
            if(children[i].type==typeof(UnityEngine.PlayerLoop.PostLateUpdate.PresentAfterDraw)){
                var next=new PlayerLoopSystem[children.Length+1];
                Array.Copy(children,0,next,0,i);
                next[i]=new PlayerLoopSystem{type=typeof(FramePacingLoop),updateDelegate=WaitForFrame};
                Array.Copy(children,i,next,i+1,children.Length-i);loop.subSystemList=next;return true;
            }
            if(Insert(ref children[i]))return true;
        }
        return false;
    }
    private static void WaitForFrame(){
        int fps=ActiveLimit;
        if(fps==0){schedule.Reset();return;}
        long now=Stopwatch.GetTimestamp();double target=schedule.Target(now,fps,Stopwatch.Frequency);
        // Sleep most of the interval; spin only the last 0.3 ms on modern
        // Windows. The fallback gets a 1 ms guard for its coarser wake-up.
        double guard=Stopwatch.Frequency*(highResolution?.0003:.001);
        double remaining=target-now;
        if(remaining>guard){
            long due=-Math.Max(1,(long)((remaining-guard)*10000000.0/Stopwatch.Frequency));
            if(!SetWaitableTimer(timer,ref due,0,IntPtr.Zero,IntPtr.Zero,false)||WaitForSingleObject(timer,100)!=0){
                // Fail open to Unity's existing limiter, never hang the game.
                Shutdown();Application.targetFrameRate=fps;return;
            }
        }
        while((now=Stopwatch.GetTimestamp())<target)Thread.SpinWait(16);
        schedule.Advance(now,fps,Stopwatch.Frequency);
    }
    private static void FocusChanged(bool focused){schedule.Reset();}
    private static void Shutdown(){
        if(timer!=IntPtr.Zero){CloseHandle(timer);timer=IntPtr.Zero;}
        if(periodRequested){timeEndPeriod(1);periodRequested=false;}
        limit=0;highResolution=false;schedule.Reset();
    }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    private static extern IntPtr CreateWaitableTimerExW(IntPtr attributes,string name,uint flags,uint access);
    [DllImport("kernel32.dll",SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool SetWaitableTimer(IntPtr timer,ref long due,int period,IntPtr routine,IntPtr argument,[MarshalAs(UnmanagedType.Bool)]bool resume);
    [DllImport("kernel32.dll")] private static extern uint WaitForSingleObject(IntPtr handle,uint milliseconds);
    [DllImport("kernel32.dll")] [return:MarshalAs(UnmanagedType.Bool)] private static extern bool CloseHandle(IntPtr handle);
    [DllImport("winmm.dll")] private static extern uint timeBeginPeriod(uint milliseconds);
    [DllImport("winmm.dll")] private static extern uint timeEndPeriod(uint milliseconds);
}
