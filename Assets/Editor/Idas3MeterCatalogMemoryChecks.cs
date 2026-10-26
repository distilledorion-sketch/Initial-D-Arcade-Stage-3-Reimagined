using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Text;
using UnityEngine;

// Real catalogue equivalence and ownership checks. No artwork regeneration,
// player-data writes, or changes to the baked layout catalogue.
public static class Idas3MeterCatalogMemoryChecks
{
    [Serializable] sealed class Report {
        public bool passed;public int entries,checks,retainedWithOneActive;
        public double longestEntryReadMs;
    }
    static readonly BindingFlags Hidden=BindingFlags.Static|BindingFlags.NonPublic;
    static int LoadedCount=>(int)typeof(Idas3ArcadeMeterCatalog).GetProperty("LoadedMeterCount",Hidden).GetValue(null);
    static void Check(bool value,string message,Report report){++report.checks;if(!value)throw new InvalidOperationException(message);}
    public static void Run(){
        var report=new Report();
        int count=Idas3ArcadeMeterCatalog.Count;
        for(int index=0;index<count;++index){
            int style=Idas3ArcadeMeterCatalog.StyleAt(index);
            Check(Idas3ArcadeMeterCatalog.IsValidStyle(style),"Style rejected: "+style,report);
            Check(!string.IsNullOrEmpty(Idas3ArcadeMeterCatalog.Name(style)),"Style has no name: "+style,report);
            if(style>1)Check(Idas3ArcadeMeterCatalog.IsAvailable(style),"Style unavailable: "+style,report);
        }
        Check(LoadedCount==0,"Listing meters loaded detailed metadata",report);
        Idas3MeterLayoutBounds.VerifyBakedCache();
        Check(LoadedCount==0,"Checking baked layouts loaded detailed metadata",report);
        VerifyEntries(report);
        var active=Idas3ArcadeMeterCatalog.Get(8);
        GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();
        report.retainedWithOneActive=LoadedCount;
        Check(report.retainedWithOneActive==1,"Unused detailed meter catalogues remain strongly held",report);
        Check(ReferenceEquals(active,Idas3ArcadeMeterCatalog.Get(8)),"Active meter identity changed",report);
        GC.KeepAlive(active);
        VerifyReader(report);
        report.passed=true;
        string path=Environment.GetEnvironmentVariable("IDAS3_METER_CATALOG_REPORT");
        if(string.IsNullOrEmpty(path)||File.Exists(path))throw new IOException("A fresh catalogue-check output path is required");
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path)));
        File.WriteAllText(path,JsonUtility.ToJson(report,true));
        UnityEngine.Debug.Log("PASS meter catalogue equivalence and ownership: "+report.entries+" entries; "+report.checks+" checks");
    }
    [MethodImpl(MethodImplOptions.NoInlining)]
    static void VerifyEntries(Report report){
        var asset=Resources.Load<TextAsset>("ArcadeHud/Catalog/catalog");
        Idas3ArcadeMeterCatalog.Catalog original;
        try{original=JsonUtility.FromJson<Idas3ArcadeMeterCatalog.Catalog>(asset.text);}
        finally{Resources.UnloadAsset(asset);}
        foreach(var meter in original.meters){
            if(meter==null||meter.id<0||meter.id>117||meter.id==113||meter.id==114||meter.layers==null||meter.layers.Length==0)continue;
            int style=meter.id==31?1:meter.id+2;
            var watch=Stopwatch.StartNew();var actual=Idas3ArcadeMeterCatalog.Get(style);watch.Stop();
            report.longestEntryReadMs=Math.Max(report.longestEntryReadMs,watch.Elapsed.TotalMilliseconds);
            Check(JsonUtility.ToJson(meter)==JsonUtility.ToJson(actual),"Meter metadata differs: "+meter.id,report);
            Check(ReferenceEquals(actual,Idas3ArcadeMeterCatalog.Get(style)),"Repeated read recreated active meter: "+meter.id,report);
            Check(Idas3ArcadeMeterCatalog.Name(style)==(style==1?"Stuttgart":meter.name),"Index name differs: "+meter.id,report);
            ++report.entries;
        }
        Check(Idas3ArcadeMeterCatalog.Count==report.entries+1,"Selectable meter count changed",report);
        Check(!Idas3ArcadeMeterCatalog.IsValidStyle(115)&&!Idas3ArcadeMeterCatalog.IsValidStyle(116),"Excluded meter IDs became selectable",report);
    }
    static void VerifyReader(Report report){
        const string json="{\"meters\":[{\"id\":0,\"name\":\"nested [ { \\\" } ]\",\"layers\":[{\"name\":\"日付\",\"curves\":[{\"values\":[1e-7,-123.125]}]}]},{\"name\":\"2026-10-24T00:00:00Z\",\"sourceNameJapanese\":\"日本語\",\"id\":6,\"layers\":[{}]}]}";
        var reference=JsonUtility.FromJson<Idas3ArcadeMeterCatalog.Catalog>(json);
        var read=typeof(Idas3ArcadeMeterCatalog).GetMethod("ReadMeter",Hidden);
        foreach(var fixture in new[]{json,"\uFEFF"+json,"{\"version\":1,"+json.Substring(1),json.Replace("},{","},\r\n {"),json.Replace("},{","},\n {"),json.Replace("},{","},\r {")})
            for(int i=0;i<reference.meters.Length;++i){
                var actual=(Idas3ArcadeMeterCatalog.Meter)read.Invoke(null,new object[]{Encoding.UTF8.GetBytes(fixture),i});
                Check(JsonUtility.ToJson(actual)==JsonUtility.ToJson(reference.meters[i]),"Indexed reader changed nested/string/numeric data",report);
            }
        bool rejected=false;
        try{read.Invoke(null,new object[]{Encoding.UTF8.GetBytes(json),2});}
        catch(TargetInvocationException error){rejected=error.InnerException is InvalidDataException;}
        Check(rejected,"Missing ordinal was accepted",report);
        var nullInput=Encoding.UTF8.GetBytes("{\"meters\":[null,{\"id\":6,\"name\":\"日本語 😀\",\"layers\":[{}]}]}");
        Check(read.Invoke(null,new object[]{nullInput,0})==null,"Null entry changed index positions",report);
        var afterNull=(Idas3ArcadeMeterCatalog.Meter)read.Invoke(null,new object[]{nullInput,1});
        Check(afterNull.id==6&&afterNull.name=="日本語 😀","UTF-8/null entry indexing differs",report);
        foreach(var invalid in new[]{"{\"meters\":[{} ,]}","{\"meters\":[{\"layers\":[}]}]}","{\"meters\":[{\"name\":\"unfinished}]}","{\"version\":2,\"meters\":[{}]}","{\"meters\":[{}]} extra"}){
            rejected=false;
            try{read.Invoke(null,new object[]{Encoding.UTF8.GetBytes(invalid),0});}
            catch(TargetInvocationException error){rejected=error.InnerException is InvalidDataException;}
            Check(rejected,"Malformed catalogue index was accepted",report);
        }
    }
}
