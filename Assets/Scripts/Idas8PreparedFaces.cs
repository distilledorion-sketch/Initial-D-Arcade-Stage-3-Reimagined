using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

// Derived, optional build content. Older/mismatched packs use the original
// classifier. Never write this data into a player's installation at runtime.
internal sealed class Idas8PreparedFaces
{
    internal const string FileName="scene.faces";
    const int Version=1,MaximumBytes=16*1024*1024;
    readonly int[] triangles;
    readonly byte[][] masks;
    Idas8PreparedFaces(int[] counts,byte[][] values){triangles=counts;masks=values;}
    internal byte[] ForShape(int shape,int indexCount){
        if(shape<0||shape>=triangles.Length||triangles[shape]!=indexCount/3||indexCount%3!=0)return null;
        return masks[shape];
    }
    internal static bool Equal(byte[] a,byte[] b){
        if(a==null||b==null||a.Length!=b.Length)return false;
        for(int i=0;i<a.Length;++i)if(a[i]!=b[i])return false;return true;
    }
    static byte[] Hash(byte[] value){using(var hash=SHA256.Create())return hash.ComputeHash(value);}
    static byte[] SourceHash(Stream source){
        long position=source.Position;
        try{source.Position=0;using(var hash=SHA256.Create())return hash.ComputeHash(source);}
        finally{source.Position=position;}
    }
    internal static Idas8PreparedFaces Read(string path,Stream scene,string manifest,string course,int expectedShapes){
        long length=new FileInfo(path).Length;
        if(length<144||length>MaximumBytes)throw new InvalidDataException("Prepared faces length");
        var file=File.ReadAllBytes(path);int payloadLength=file.Length-32;
        byte[] digest;using(var hash=SHA256.Create())digest=hash.ComputeHash(file,0,payloadLength);
        for(int i=0;i<32;++i)if(digest[i]!=file[payloadLength+i])throw new InvalidDataException("Prepared faces checksum");
        using(var reader=new BinaryReader(new MemoryStream(file,0,payloadLength,false))){
            if(Encoding.ASCII.GetString(reader.ReadBytes(4))!="IFC1"||reader.ReadInt32()!=Version)throw new InvalidDataException("Prepared faces version");
            int count=reader.ReadInt32();
            if(count!=expectedShapes||count<0||count>10000)throw new InvalidDataException("Prepared faces shape count");
            if(!Equal(reader.ReadBytes(32),SourceHash(scene))||!Equal(reader.ReadBytes(32),Hash(Encoding.UTF8.GetBytes(manifest)))||
                !Equal(reader.ReadBytes(32),Hash(Encoding.UTF8.GetBytes(course))))throw new InvalidDataException("Prepared faces source differs");
            var counts=new int[count];var values=new byte[count][];
            for(int shape=0;shape<count;++shape){
                int n=reader.ReadInt32();counts[shape]=n;
                if(n==-1)continue;
                if(n<0||n>2000000)throw new InvalidDataException("Prepared faces triangle count");
                int bytes=(n+7)/8;values[shape]=reader.ReadBytes(bytes);
                if(values[shape].Length!=bytes)throw new EndOfStreamException("Prepared faces mask");
                if(n%8!=0&&(values[shape][bytes-1]>>(n%8))!=0)throw new InvalidDataException("Prepared faces padding");
            }
            if(reader.BaseStream.Position!=reader.BaseStream.Length)throw new InvalidDataException("Trailing prepared faces data");
            return new Idas8PreparedFaces(counts,values);
        }
    }
#if UNITY_EDITOR
    internal static void Write(string path,Stream scene,string manifest,string course,int[] counts,byte[][] masks){
        using(var payload=new MemoryStream()){
            using(var writer=new BinaryWriter(payload,Encoding.UTF8,true)){
                writer.Write(Encoding.ASCII.GetBytes("IFC1"));writer.Write(Version);writer.Write(counts.Length);
                writer.Write(SourceHash(scene));writer.Write(Hash(Encoding.UTF8.GetBytes(manifest)));writer.Write(Hash(Encoding.UTF8.GetBytes(course)));
                for(int i=0;i<counts.Length;++i){writer.Write(counts[i]);if(counts[i]>=0)writer.Write(masks[i]);}
            }
            var data=payload.ToArray();if(data.Length+32>MaximumBytes)throw new InvalidDataException("Prepared faces exceed limit");
            using(var output=new FileStream(path,FileMode.CreateNew,FileAccess.Write,FileShare.None)){output.Write(data,0,data.Length);var checksum=Hash(data);output.Write(checksum,0,checksum.Length);}
        }
    }
#endif
}
