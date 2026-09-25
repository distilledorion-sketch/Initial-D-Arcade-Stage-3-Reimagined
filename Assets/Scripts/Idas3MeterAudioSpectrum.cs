using System;
using System.Threading;

// The audio callback only copies the actual device PCM into an SPSC ring.
// Analysis and publication run on the main thread; no microphone or vehicle
// telemetry enters the spectrum. Per-band attack/release uses PCM sample time.
internal sealed class Idas3MeterAudioSpectrum
{
    internal const int BandCount=32;
    const int Capacity=32768,Mask=Capacity-1;
    static Idas3MeterAudioSpectrum active;
    static long revision;
    readonly float[] left=new float[Capacity],right=new float[Capacity],published=new float[BandCount];
    readonly Processor processor;
    long written,read,analyzed;
    float capturedPeak;
    bool silent=true;
    int running=1;

    internal Idas3MeterAudioSpectrum(int sampleRate){processor=new Processor(sampleRate);}
    internal static Idas3MeterAudioSpectrum Start(int sampleRate){
        var next=new Idas3MeterAudioSpectrum(sampleRate);
        Volatile.Write(ref active,next);Interlocked.Increment(ref revision);return next;
    }
    internal static long Revision=>Interlocked.Read(ref revision);
    internal static long CapturedFrames {get {var current=Volatile.Read(ref active);return current==null?0:Volatile.Read(ref current.written);}}
    internal static bool Available {get {var current=Volatile.Read(ref active);return current!=null&&current.processor.HasFrame;}}
    internal static bool IsSilent {get {var current=Volatile.Read(ref active);return current==null||current.silent;}}
    internal static bool CopyBands(float[] destination){
        if(destination==null||destination.Length<BandCount)throw new ArgumentException("A meter spectrum needs 32 bands.",nameof(destination));
        var current=Volatile.Read(ref active);
        if(current==null){Array.Clear(destination,0,BandCount);return false;}
        return current.CopySnapshot(destination);
    }
    internal bool CopySnapshot(float[] destination){Array.Copy(published,destination,BandCount);return processor.HasFrame;}
    internal void Stop(){
        Volatile.Write(ref running,0);
        Interlocked.CompareExchange(ref active,null,this);
        Clear();Interlocked.Increment(ref revision);
    }
    // Called from OnAudioFilterRead after the existing native mixer/resampler.
    // Retain both stereo channels so opposite-phase audio cannot cancel the
    // visualizer. Extra device channels are averaged into their stereo side.
    internal void Capture(float[] samples,int channels){
        if(Volatile.Read(ref running)==0||samples==null||channels<=0)return;
        int frames=samples.Length/channels;if(frames==0)return;
        long cursor=Volatile.Read(ref written);float peak=0;
        for(int frame=0;frame<frames;++frame){
            float l=0,r=0;int lc=0,rc=0;
            for(int channel=0;channel<channels;++channel){
                float value=samples[frame*channels+channel];
                if(float.IsNaN(value)||float.IsInfinity(value))value=0;
                value=Math.Max(-1,Math.Min(1,value));
                if((channel&1)==0){l+=value;++lc;}else{r+=value;++rc;}
            }
            l/=Math.Max(1,lc);r=rc==0?l:r/rc;
            int slot=(int)(cursor&Mask);left[slot]=l;right[slot]=r;++cursor;
            peak=Math.Max(peak,Math.Max(Math.Abs(l),Math.Abs(r)));
        }
        Volatile.Write(ref capturedPeak,peak);
        Volatile.Write(ref written,cursor);
    }
    // Main thread only. 'gain' includes Unity source/listener gain, which is
    // applied after OnAudioFilterRead and is therefore absent from tapped PCM.
    internal void Pump(float gain){
        long end=Volatile.Read(ref written);
        if(Volatile.Read(ref running)==0||float.IsNaN(gain)||float.IsInfinity(gain)||gain<=0||
            (end!=read&&Volatile.Read(ref capturedPeak)<=.0000001f)){
            read=end;Clear();return;
        }
        if(end==read)return;
        // A stalled main thread must not spend frames catching up old sound.
        // Keep two fresh FFT windows; this is presentation-only discarded PCM.
        if(end-read>8192){read=end-Processor.WindowSize*2;processor.Reset();}
        while(read<end){int slot=(int)(read&Mask);processor.Push(left[slot]*gain,right[slot]*gain);++read;}
        if(processor.Generation==analyzed)return;
        analyzed=processor.Generation;processor.Copy(published);silent=true;
        for(int band=0;band<BandCount;++band)if(published[band]>0){silent=false;break;}
        Interlocked.Increment(ref revision);
    }
    void Clear(){
        bool changed=processor.HasFrame||!silent;
        processor.Reset();analyzed=processor.Generation;Array.Clear(published,0,BandCount);silent=true;
        if(changed)Interlocked.Increment(ref revision);
    }

    internal sealed class Processor
    {
        internal const int WindowSize=2048,HopSize=512;
        readonly int rate;
        readonly float[] inputL=new float[WindowSize],inputR=new float[WindowSize];
        readonly double[] realL=new double[WindowSize],imagL=new double[WindowSize],realR=new double[WindowSize],imagR=new double[WindowSize];
        readonly double[] window=new double[WindowSize],cosine=new double[WindowSize/2],sine=new double[WindowSize/2];
        readonly int[] reverse=new int[WindowSize],firstBin=new int[BandCount],lastBin=new int[BandCount];
        readonly float[] levels=new float[BandCount];
        readonly double attack,release;
        int cursor,filled,hop;
        internal bool HasFrame {get;private set;}
        internal long Generation {get;private set;}
        internal Processor(int sampleRate){
            if(sampleRate<8000||sampleRate>192000)throw new ArgumentOutOfRangeException(nameof(sampleRate));
            rate=sampleRate;attack=1-Math.Exp(-HopSize/(rate*.035));release=1-Math.Exp(-HopSize/(rate*.18));
            for(int i=0;i<WindowSize;++i){
                window[i]=.5-.5*Math.Cos(2*Math.PI*i/WindowSize);
                int value=i,reversed=0;for(int bit=0;bit<11;++bit){reversed=(reversed<<1)|(value&1);value>>=1;}reverse[i]=reversed;
            }
            for(int i=0;i<WindowSize/2;++i){double angle=-2*Math.PI*i/WindowSize;cosine[i]=Math.Cos(angle);sine[i]=Math.Sin(angle);}
            double top=Math.Min(16000,rate*.5);
            for(int band=0;band<BandCount;++band){
                double low=60*Math.Pow(top/60,band/(double)BandCount),high=60*Math.Pow(top/60,(band+1d)/BandCount);
                firstBin[band]=Math.Max(1,Math.Min(WindowSize/2-1,(int)Math.Ceiling(low*WindowSize/rate)));
                lastBin[band]=Math.Max(firstBin[band],Math.Min(WindowSize/2-1,(int)Math.Floor(high*WindowSize/rate)));
            }
        }
        internal void Reset(){
            cursor=filled=hop=0;HasFrame=false;Array.Clear(inputL,0,WindowSize);Array.Clear(inputR,0,WindowSize);Array.Clear(levels,0,BandCount);++Generation;
        }
        internal void Copy(float[] target)=>Array.Copy(levels,target,BandCount);
        internal static int BandForFrequency(float frequency,int sampleRate){
            double top=Math.Min(16000,sampleRate*.5);
            return Math.Max(0,Math.Min(BandCount-1,(int)Math.Floor(Math.Log(Math.Max(60,frequency)/60)/Math.Log(top/60)*BandCount)));
        }
        static float Sample(float value)=>float.IsNaN(value)||float.IsInfinity(value)?0:Math.Max(-1,Math.Min(1,value));
        internal void Push(float l,float r){
            inputL[cursor]=Sample(l);inputR[cursor]=Sample(r);cursor=(cursor+1)&(WindowSize-1);
            if(filled<WindowSize){if(++filled==WindowSize){Analyze();hop=0;}return;}
            if(++hop==HopSize){Analyze();hop=0;}
        }
        void Analyze(){
            for(int i=0;i<WindowSize;++i){int source=(cursor+i)&(WindowSize-1),target=reverse[i];
                realL[target]=inputL[source]*window[i];realR[target]=inputR[source]*window[i];imagL[target]=imagR[target]=0;}
            Transform(realL,imagL);Transform(realR,imagR);
            for(int band=0;band<BandCount;++band){
                double peak=0;
                for(int bin=firstBin[band];bin<=lastBin[band];++bin){
                    double power=(realL[bin]*realL[bin]+imagL[bin]*imagL[bin]+realR[bin]*realR[bin]+imagR[bin]*imagR[bin])*.5;
                    peak=Math.Max(peak,power);
                }
                double amplitude=Math.Sqrt(peak)*4/WindowSize;
                double target=amplitude<=.001?0:Math.Max(0,Math.Min(1,(20*Math.Log10(amplitude)+60)/60));
                double blend=!HasFrame?1:target>levels[band]?attack:release;
                double value=levels[band]+(target-levels[band])*blend;
                levels[band]=(float)(target==0&&value<.0001?0:value);
            }
            HasFrame=true;++Generation;
        }
        void Transform(double[] real,double[] imaginary){
            for(int length=2;length<=WindowSize;length<<=1){int half=length>>1,step=WindowSize/length;
                for(int start=0;start<WindowSize;start+=length)for(int j=0;j<half;++j){
                    int a=start+j,b=a+half,twiddle=j*step;double c=cosine[twiddle],s=sine[twiddle];
                    double r=real[b]*c-imaginary[b]*s,i=real[b]*s+imaginary[b]*c;
                    real[b]=real[a]-r;imaginary[b]=imaginary[a]-i;real[a]+=r;imaginary[a]+=i;
                }
            }
        }
    }
}
