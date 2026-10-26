using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEditor.Profiling;
using UnityEditorInternal;
using UnityEngine;

// Offline analysis of our private, opt-in development player. Only complete
// frames between explicit markers count; the probes must prove byte metadata.
public static class Idas3AllocationTraceChecks
{
    [Serializable] private sealed class Probe { public string name; public int occurrences,count; public long bytes; }
    [Serializable] private sealed class Frame { public int index,mainCount,allCount; public long mainBytes,allBytes; }
    [Serializable] private sealed class Site { public string thread,stack; public long bytes; public int count; }
    [Serializable] private sealed class Report
    {
        public bool passed;
        public string error,trace;
        public string scope="GC.Alloc byte metadata from complete development-player frames. Includes diagnostic overhead; not release timings or native/GPU memory.";
        public int first,last,start=-1,end=-1,unresolvedStacks;
        public Probe[] probes;
        public Frame[] frames;
        public Site[] sites;
    }
    private static void Require(bool condition,string message) { if (!condition) throw new InvalidOperationException(message); }
    private static long AllocationBytes(RawFrameDataView data,int sample)
    {
        Require(data.GetSampleMetadataCount(sample)==1,"Unexpected GC.Alloc metadata count");
        long bytes=data.GetSampleMetadataAsLong(sample,0);
        Require(bytes>0,"GC.Alloc is missing positive allocation bytes");
        return bytes;
    }
    public static void Run()
    {
        string trace=Environment.GetEnvironmentVariable("IDAS3_ALLOCATION_TRACE");
        Require(!string.IsNullOrWhiteSpace(trace)&&Path.IsPathFullyQualified(trace)&&File.Exists(trace),"An existing absolute trace path is required");
        string output=Path.Combine(Path.GetDirectoryName(trace),"allocation-analysis.json");
        Require(!File.Exists(output),"Preserve the existing analysis; use a new trace directory");
        var report=new Report{trace=trace,probes=new[]{new Probe{name="Empty"},new Probe{name="64K"},new Probe{name="256K"}}};
        try
        {
            Require(ProfilerDriver.LoadProfile(trace,false),"Unity could not load the profiler trace");
            report.first=ProfilerDriver.firstFrameIndex;report.last=ProfilerDriver.lastFrameIndex;
            int starts=0,ends=0;
            for(int frame=report.first;frame<=report.last;++frame)
            using(var data=ProfilerDriver.GetRawFrameDataView(frame,0))
            {
                if(!data.valid)continue;
                int gc=data.GetMarkerId("GC.Alloc");
                for(int i=0;i<data.sampleCount;++i)
                {
                    string name=data.GetSampleName(i);
                    if(name=="IDAS3.MeasureWindowStart"){report.start=frame;++starts;}
                    if(name=="IDAS3.MeasureWindowEnd"){report.end=frame;++ends;}
                    foreach(var probe in report.probes)
                    {
                        if(name!="IDAS3.AllocationProbe."+probe.name)continue;
                        ++probe.occurrences;
                        int end=i+data.GetSampleChildrenCountRecursive(i);
                        for(int child=i+1;child<=end;++child)
                            if(data.GetSampleMarkerId(child)==gc){probe.bytes+=AllocationBytes(data,child);++probe.count;}
                    }
                }
            }
            Require(starts==1&&ends==1&&report.end>report.start+1,"Missing or ambiguous measurement boundaries");
            var empty=report.probes[0];var small=report.probes[1];var large=report.probes[2];
            Require(report.probes.All(p=>p.occurrences==1)&&empty.count==0&&empty.bytes==0&&small.count==1&&large.count==1&&
                small.bytes>=65536&&small.bytes<=65792&&large.bytes>=262144&&large.bytes<=262400&&large.bytes-small.bytes==196608,
                "Known allocations did not calibrate GC.Alloc byte metadata");
            var frames=new List<Frame>();var sites=new Dictionary<string,Site>();var names=new Dictionary<ulong,string>();var addresses=new List<ulong>();
            for(int frame=report.start+1;frame<report.end;++frame)
            {
                var totals=new Frame{index=frame};bool mainFound=false;
                for(int thread=0;;++thread)
                using(var data=ProfilerDriver.GetRawFrameDataView(frame,thread))
                {
                    if(!data.valid)break;
                    bool main=data.threadName=="Main Thread";mainFound|=main;
                    int gc=data.GetMarkerId("GC.Alloc");
                    for(int i=0;i<data.sampleCount;++i)
                    {
                        if(data.GetSampleMarkerId(i)!=gc)continue;
                        long bytes=AllocationBytes(data,i);totals.allBytes+=bytes;++totals.allCount;
                        if(main){totals.mainBytes+=bytes;++totals.mainCount;}
                        addresses.Clear();data.GetSampleCallstack(i,addresses);
                        var stack=new List<string>();
                        foreach(ulong address in addresses)
                        {
                            if(!names.TryGetValue(address,out string name))
                            {
                                name=data.ResolveMethodInfo(address).methodName;
                                names[address]=name??"";
                            }
                            if(!string.IsNullOrEmpty(name))stack.Add(name);
                        }
                        string callstack=string.Join("\n",stack);
                        if(callstack.Length==0){callstack="[unresolved]";++report.unresolvedStacks;}
                        string key=data.threadName+"\n"+callstack;
                        if(!sites.TryGetValue(key,out Site site)){site=new Site{thread=data.threadName,stack=callstack};sites.Add(key,site);}
                        site.bytes+=bytes;++site.count;
                    }
                }
                Require(mainFound,"Complete frame has no Main Thread");frames.Add(totals);
            }
            report.frames=frames.ToArray();report.sites=sites.Values.OrderByDescending(s=>s.bytes).ToArray();
            Require(report.frames.Length>=30,"Too few complete measurement frames");report.passed=true;
        }
        catch(Exception error){report.error=error.ToString();throw;}
        finally{File.WriteAllText(output,JsonUtility.ToJson(report,true));}
    }
}
