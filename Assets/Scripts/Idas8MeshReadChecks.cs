#if UNITY_EDITOR
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

public static class Idas8MeshReadChecks
{
    [Serializable] private class AssetResult {public string path;public int meshes;public long vertices,indices;}
    [Serializable] private class Result {public bool passed;public int checks;public List<AssetResult> assets=new List<AssetResult>();}
    private static int checks;
    private static void Check(bool condition,string message){if(!condition)throw new InvalidOperationException(message);checks++;}
    private static void Equal<T>(T[] a,T[] b) where T:struct {Check(MemoryMarshal.AsBytes(a.AsSpan()).SequenceEqual(MemoryMarshal.AsBytes(b.AsSpan())),"Decoded mesh differs bit-for-bit: "+typeof(T).Name);}
    private static void Compare(BinaryReader a,BinaryReader b,int vertices,int indices,Matrix4x4 matrix,ref byte[] slowScratch,ref byte[] fastScratch,Action heartbeat){
        var p=new Vector3[vertices];var n=new Vector3[vertices];var uv=new Vector2[vertices];var uv2=new Vector2[vertices];var c=new Color32[vertices];
        var p2=new Vector3[vertices];var n2=new Vector3[vertices];var t2=new Vector2[vertices];var t22=new Vector2[vertices];var c2=new Color32[vertices];
        Idas8HakoneCourse.ReadMeshVertices(a,matrix,p,n,uv,uv2,c,ref slowScratch,null,true);
        Idas8HakoneCourse.ReadMeshVertices(b,matrix,p2,n2,t2,t22,c2,ref fastScratch,heartbeat);
        Equal(p,p2);Equal(n,n2);Equal(uv,t2);Equal(uv2,t22);Equal(c,c2);
        var ix=new int[indices];var ix2=new int[indices];
        Idas8HakoneCourse.ReadMeshIndices(a,ix,vertices,ref slowScratch,null,true);
        Idas8HakoneCourse.ReadMeshIndices(b,ix2,vertices,ref fastScratch,heartbeat);Equal(ix,ix2);
    }
    private sealed class Fragmented:Stream {
        private readonly MemoryStream source;public Fragmented(byte[] bytes){source=new MemoryStream(bytes);}
        public override bool CanRead=>true;public override bool CanWrite=>false;public override bool CanSeek=>false;
        public override long Length=>throw new NotSupportedException();public override long Position{get=>throw new NotSupportedException();set=>throw new NotSupportedException();}
        public override int Read(byte[] b,int o,int n)=>source.Read(b,o,Math.Min(7,n));public override int ReadByte()=>source.ReadByte();
        public override void Flush(){}public override long Seek(long a,SeekOrigin b)=>throw new NotSupportedException();public override void SetLength(long a)=>throw new NotSupportedException();public override void Write(byte[] b,int o,int n)=>throw new NotSupportedException();
        protected override void Dispose(bool disposing){if(disposing)source.Dispose();base.Dispose(disposing);}
    }
    private static void Reject(Action action,Type expected){try{action();}catch(Exception ex){Check(expected.IsInstanceOfType(ex),"Wrong rejection: "+ex);return;}throw new InvalidOperationException("Invalid mesh payload accepted");}
    public static void Run(){
        checks=0;string input=Environment.GetEnvironmentVariable("IDAS3_MESH_READ_INPUT"),output=Environment.GetEnvironmentVariable("IDAS3_MESH_READ_REPORT");
        if(string.IsNullOrEmpty(input)||string.IsNullOrEmpty(output))throw new ArgumentException("Explicit mesh input/report paths required");
        var report=new Result();
        using(var bytes=new MemoryStream()){
            const int count=5000;using(var w=new BinaryWriter(bytes,System.Text.Encoding.UTF8,true)){
                for(int i=0;i<count;i++){for(int j=0;j<10;j++)w.Write((i-2317)*.13f+j*.00017f);w.Write((byte)i);w.Write((byte)(i>>1));w.Write((byte)(255-i));w.Write((byte)(i^0x55));}
                for(int i=0;i<17001;i++)w.Write(i%count);
            }
            var data=bytes.ToArray();byte[] slow=null,fast=null;int heartbeats=0;
            using(var a=new BinaryReader(new MemoryStream(data)))using(var b=new BinaryReader(new Fragmented(data)))
                Compare(a,b,count,17001,Matrix4x4.TRS(new Vector3(12,-5,.3f),Quaternion.Euler(17,53,-11),new Vector3(2,.5f,3)),ref slow,ref fast,()=>heartbeats++);
            Check(heartbeats>4,"Short reads and network heartbeat not exercised");
            Check(fast.Length==65536,"Reader scratch must stay bounded to 64 KiB across vertex and index chunks");
            byte[] buffer=null;var positions=new Vector3[1];var normals=new Vector3[1];var uv=new Vector2[1];var colors=new Color32[1];
            Reject(()=>{using(var r=new BinaryReader(new MemoryStream(new byte[43])))Idas8HakoneCourse.ReadMeshVertices(r,Matrix4x4.identity,positions,normals,uv,uv,colors,ref buffer,null);},typeof(EndOfStreamException));
            Reject(()=>{using(var r=new BinaryReader(new Fragmented(new byte[43])))Idas8HakoneCourse.ReadMeshVertices(r,Matrix4x4.identity,positions,normals,uv,uv,colors,ref buffer,null);},typeof(EndOfStreamException));
            foreach(int invalid in new[]{-1,1})Reject(()=>{using(var r=new BinaryReader(new MemoryStream(BitConverter.GetBytes(invalid))))Idas8HakoneCourse.ReadMeshIndices(r,new int[1],1,ref buffer,null);},typeof(InvalidDataException));
            Reject(()=>{using(var r=new BinaryReader(new MemoryStream(new byte[3])))Idas8HakoneCourse.ReadMeshIndices(r,new int[1],1,ref buffer,null);},typeof(EndOfStreamException));
            using(var r=new BinaryReader(new MemoryStream())){Idas8HakoneCourse.ReadMeshVertices(r,Matrix4x4.identity,new Vector3[0],new Vector3[0],new Vector2[0],new Vector2[0],new Color32[0],ref buffer,null);Idas8HakoneCourse.ReadMeshIndices(r,new int[0],0,ref buffer,null);}
        }
        foreach(string path in Directory.GetFiles(input,"scene.bin",SearchOption.AllDirectories)){
            var data=File.ReadAllBytes(path);if(data.Length<8||System.Text.Encoding.ASCII.GetString(data,0,4)!="HKN2")continue;
            using(var a=new BinaryReader(new MemoryStream(data)))using(var b=new BinaryReader(new MemoryStream(data))){
                a.ReadInt32();int count=a.ReadInt32();b.BaseStream.Position=8;byte[] slow=null,fast=null;
                var row=new AssetResult{path=Path.GetRelativePath(input,path),meshes=count};
                for(int i=0;i<count;i++){
                    a.ReadInt32();int nv=a.ReadInt32(),nt=a.ReadInt32();Check(nv>=0&&nv<=2000000&&nt>=0&&nt<=6000000&&nt%3==0,"Invalid source mesh header");
                    Matrix4x4 matrix=Matrix4x4.identity;for(int y=0;y<3;y++)for(int x=0;x<4;x++)matrix[y,x]=a.ReadSingle();b.BaseStream.Position=a.BaseStream.Position;
                    Compare(a,b,nv,nt,matrix,ref slow,ref fast,null);Check(a.BaseStream.Position==b.BaseStream.Position,"Reader consumed different bytes");row.vertices+=nv;row.indices+=nt;
                }
                Check(a.BaseStream.Position==data.Length,"Unconsumed source bytes");report.assets.Add(row);
            }
        }
        Check(report.assets.Count>=4,"Imported asset coverage is missing");report.checks=checks;report.passed=true;File.WriteAllText(output,JsonUtility.ToJson(report,true));
        Debug.Log("PASS mesh block reader: "+report.assets.Count+" source files, "+checks+" checks");
    }
}

#endif
