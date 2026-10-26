using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Profiling;

// Explicit private diagnostic. Enumeration and JSON work are outside measured
// frames; nothing is unloaded, collected or changed to obtain these estimates.
internal static class Idas3ResourceCensus
{
    internal static readonly bool Requested=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-resource-census")>=0;
    static readonly bool DumpUi=Array.IndexOf(Environment.GetCommandLineArgs(),"-idas3-resource-ui-dump")>=0;
    [DllImport("Idas3Unity",CallingConvention=CallingConvention.Cdecl)]
    static extern int Idas3UiCopyTextureRGBA(uint id,[Out] byte[] rgba,int capacity);
    [Serializable] sealed class Entry {
        public string kind,name,format,id;
        public int width,height,mips,samples,vertices,channels,frequency;
        public ulong indices;
        public long unityNativeBytes;
        public bool readable;
    }
    [Serializable] sealed class Report {
        public string schema="idas3-resource-census-v1",label;
        public string scope="Private Development player. Unity native-object memory estimates are not measured VRAM, process RAM or additive ownership accounting. Driver/heap counters are separate and can overlap. No forced GC, unload, quality or simulation changes. Snapshot enumeration/serialization is excluded from performance timing; runs with this option are for memory attribution.";
        public bool developmentBuild;
        public long unityAllocated,unityReserved,unityUnusedReserved,monoUsed,monoHeap,graphicsDriverBytes;
        public int unityFrame;
        public List<Entry> resources=new List<Entry>();
    }
    internal static void Capture(string directory,string label)
    {
        if(!Requested)return;
        if(!Debug.isDebugBuild)throw new InvalidOperationException("Resource census requires a private Development player.");
        var report=new Report{label=label,developmentBuild=Debug.isDebugBuild,unityFrame=Time.frameCount,
            unityAllocated=Profiler.GetTotalAllocatedMemoryLong(),unityReserved=Profiler.GetTotalReservedMemoryLong(),
            unityUnusedReserved=Profiler.GetTotalUnusedReservedMemoryLong(),monoUsed=Profiler.GetMonoUsedSizeLong(),
            monoHeap=Profiler.GetMonoHeapSizeLong(),graphicsDriverBytes=Profiler.GetAllocatedMemoryForGraphicsDriver()};
        if(report.unityAllocated<=0||report.unityReserved<report.unityAllocated)throw new InvalidOperationException("Unity memory counters unavailable.");
        foreach(var texture in Resources.FindObjectsOfTypeAll<Texture>()){
            var row=ObjectEntry(texture);row.width=texture.width;row.height=texture.height;
            if(texture is Texture2D t){row.mips=t.mipmapCount;row.format=t.format.ToString();row.readable=t.isReadable;}
            else if(texture is RenderTexture rt){row.format=rt.graphicsFormat.ToString();row.samples=rt.antiAliasing;row.mips=rt.mipmapCount;}
            report.resources.Add(row);
        }
        foreach(var mesh in Resources.FindObjectsOfTypeAll<Mesh>()){
            var row=ObjectEntry(mesh);row.vertices=mesh.vertexCount;row.readable=mesh.isReadable;
            for(int i=0;i<mesh.subMeshCount;++i)row.indices+=mesh.GetIndexCount(i);
            report.resources.Add(row);
        }
        foreach(var material in Resources.FindObjectsOfTypeAll<Material>())report.resources.Add(ObjectEntry(material));
        foreach(var font in Resources.FindObjectsOfTypeAll<Font>())report.resources.Add(ObjectEntry(font));
        foreach(var shader in Resources.FindObjectsOfTypeAll<Shader>())report.resources.Add(ObjectEntry(shader));
        foreach(var asset in Resources.FindObjectsOfTypeAll<TextAsset>())report.resources.Add(ObjectEntry(asset));
        foreach(var clip in Resources.FindObjectsOfTypeAll<AudioClip>()){
            var row=ObjectEntry(clip);row.samples=clip.samples;row.channels=clip.channels;row.frequency=clip.frequency;row.format=clip.loadType.ToString();report.resources.Add(row);
        }
        report.resources.Sort((a,b)=>{int bytes=b.unityNativeBytes.CompareTo(a.unityNativeBytes);return bytes!=0?bytes:string.CompareOrdinal(a.id,b.id);});
        string path=Path.Combine(directory,"resources-"+label+".json");
        if(File.Exists(path))throw new IOException("Refusing to overwrite a resource census.");
        File.WriteAllText(path,JsonUtility.ToJson(report,true));
        if(DumpUi){
            string folder=Path.Combine(directory,"ui-textures");Directory.CreateDirectory(folder);
            foreach(var row in report.resources){
                const string prefix="Original UI sprite ";
                if(row.kind!="Texture2D"||row.width!=512||row.height!=512||!row.name.StartsWith(prefix,StringComparison.Ordinal)||
                    !uint.TryParse(row.name.Substring(prefix.Length),out uint id))continue;
                string output=Path.Combine(folder,id+".png");if(File.Exists(output))continue;
                byte[] rgba=new byte[checked(row.width*row.height*4)];
                if(Idas3UiCopyTextureRGBA(id,rgba,rgba.Length)!=rgba.Length)throw new InvalidOperationException("UI diagnostic texture export failed.");
                var image=new Texture2D(row.width,row.height,TextureFormat.RGBA32,false,true);
                try{image.LoadRawTextureData(rgba);image.Apply(false,false);File.WriteAllBytes(output,image.EncodeToPNG());}
                finally{UnityEngine.Object.Destroy(image);}
            }
        }
    }
    static Entry ObjectEntry(UnityEngine.Object value)=>new Entry{kind=value.GetType().Name,name=value.name,id=value.GetEntityId().ToString(),unityNativeBytes=Profiler.GetRuntimeMemorySizeLong(value)};
}
