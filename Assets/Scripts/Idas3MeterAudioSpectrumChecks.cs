using System;
using UnityEngine;

public static class Idas3MeterAudioSpectrumChecks
{
    public static void Run()=>Debug.Log(RunChecks());
    public static string RunChecks(){
        int checks=0;
        void Check(bool condition,string message){++checks;if(!condition)throw new InvalidOperationException("Meter audio spectrum: "+message);}
        void Equal(float[] actual,float[] expected,string message){for(int i=0;i<actual.Length;++i)Check(Math.Abs(actual[i]-expected[i])<.00001f,message+" band "+i);}
        float[] bands=new float[Idas3MeterAudioSpectrum.BandCount];
        foreach(int rate in new[]{44100,48000,96000}){
            float frequency=64f*rate/Idas3MeterAudioSpectrum.Processor.WindowSize;
            var processor=new Idas3MeterAudioSpectrum.Processor(rate);
            processor.Copy(bands);foreach(float value in bands)Check(value==0,"unprimed output is not silent");
            for(int i=0;i<rate/2;++i){float value=(float)(.5*Math.Sin(2*Math.PI*frequency*i/rate));processor.Push(value,value);}
            Check(processor.HasFrame,"PCM did not produce an FFT at "+rate);processor.Copy(bands);
            int peak=0;for(int i=0;i<bands.Length;++i){Check(bands[i]>=0&&bands[i]<=1&&!float.IsNaN(bands[i]),"invalid normalized band");if(bands[i]>bands[peak])peak=i;}
            int wanted=Idas3MeterAudioSpectrum.Processor.BandForFrequency(frequency,rate);
            Check(Math.Abs(peak-wanted)<=1,"known sine appeared at the wrong frequency band");Check(bands[peak]>.8f,"known sine energy missing");
            for(int i=0;i<rate*3;++i)processor.Push(0,0);
            processor.Copy(bands);foreach(float value in bands)Check(value==0,"silence does not settle to zero");
            processor.Reset();Check(!processor.HasFrame,"reset retained a published window");
        }
        var normal=new Idas3MeterAudioSpectrum.Processor(48000);var opposite=new Idas3MeterAudioSpectrum.Processor(48000);
        var louder=new Idas3MeterAudioSpectrum.Processor(48000);var quiet=new Idas3MeterAudioSpectrum.Processor(48000);
        for(int i=0;i<24000;++i){float wave=(float)Math.Sin(2*Math.PI*1500*i/48000);normal.Push(wave*.5f,wave*.5f);opposite.Push(wave*.5f,-wave*.5f);louder.Push(wave*.8f,wave*.8f);quiet.Push(wave*.05f,wave*.05f);}
        float[] reference=new float[bands.Length];normal.Copy(reference);opposite.Copy(bands);Equal(bands,reference,"opposite-phase stereo cancelled");
        louder.Copy(reference);quiet.Copy(bands);int target=Idas3MeterAudioSpectrum.Processor.BandForFrequency(1500,48000);
        Check(reference[target]>bands[target]+.2f,"output gain does not affect magnitude");
        var invalid=new Idas3MeterAudioSpectrum.Processor(48000);
        for(int i=0;i<4096;++i)invalid.Push(float.NaN,float.PositiveInfinity);
        invalid.Copy(bands);foreach(float value in bands)Check(value==0,"invalid PCM contaminated output");

        // Same two-tone PCM reaches the same sample-time filter state when
        // Update drains it at different game/render rates. These detached
        // sessions never replace the production audio source or global bands.
        float[] expected=null;
        foreach(int fps in new[]{30,60,144,240}){
            var session=new Idas3MeterAudioSpectrum(48000);var chunk=new float[3200];int position=0;
            for(int frame=0;frame<fps*2;++frame){
                int end=(int)((frame+1L)*96000/(fps*2));int count=end-position;
                // Reuse the buffer for the fixed-size divisors. At 144 Hz
                // exact-length scratch buffers are test-only allocations.
                var pcm=count*2==chunk.Length?chunk:new float[count*2];
                for(int i=0;i<count;++i){double time=(position+i)/48000d;pcm[i*2]=(float)(.45*Math.Sin(2*Math.PI*750*time));pcm[i*2+1]=(float)(.3*Math.Sin(2*Math.PI*4000*time));}
                session.Capture(pcm,2);session.Pump(1);position=end;
            }
            Check(session.CopySnapshot(bands),"capture pipeline did not publish at "+fps+" Hz");
            if(expected==null)expected=(float[])bands.Clone();else Equal(bands,expected,"render rate changed sample-time smoothing");
            var zeros=new float[1024];session.Capture(zeros,2);session.Pump(1);
            Check(!session.CopySnapshot(bands),"silent PCM did not clear retained FFT");foreach(float value in bands)Check(value==0,"silent source retained bars");
            var tone=new float[8192];for(int i=0;i<tone.Length/2;++i)tone[i*2]=tone[i*2+1]=(float)(.5*Math.Sin(2*Math.PI*750*i/48000));
            session.Capture(tone,2);session.Pump(1);Check(session.CopySnapshot(bands),"mute setup did not have live energy");
            Check(bands[Idas3MeterAudioSpectrum.Processor.BandForFrequency(750,48000)]>.5f,"mute setup did not have audible bars");
            session.Pump(0);Check(!session.CopySnapshot(bands),"mute retained published FFT");foreach(float value in bands)Check(value==0,"external mute retained bars");
            session.Stop();
        }
        var backlog=new Idas3MeterAudioSpectrum(48000);var large=new float[80000];
        for(int i=0;i<large.Length;i++)large[i]=(float)(.4*Math.Sin(i*.13));
        backlog.Capture(large,2);backlog.Pump(1);Check(backlog.CopySnapshot(bands),"overrun did not recover from recent audio");
        foreach(float value in bands)Check(value>=0&&value<=1&&!float.IsNaN(value),"overrun corrupted spectrum");backlog.Stop();
        return "Meter audio spectrum checks passed: "+checks;
    }
}
