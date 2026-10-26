using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

public sealed partial class Idas8HakoneCourse
{
    // HKN2 stores ten little-endian floats followed by four colour bytes.
    // Reuse one load-local buffer rather than making a stream call per scalar.
    [StructLayout(LayoutKind.Sequential, Pack=1)]
    private struct DiskVertex
    {
        public Vector3 position,normal;
        public Vector2 uv,uv2;
        public Color32 color;
    }
    private static readonly bool PackedVertexLayout = BitConverter.IsLittleEndian && Marshal.SizeOf<DiskVertex>()==44;
    private static readonly Action MeshReadHeartbeat=Idas3.Multiplayer.Idas3OnlineTransportPump.KeepAliveDuringNativeLoad;
    private const int MeshReadBufferBytes=65536;

    private static Span<byte> ReadMeshPayload(BinaryReader reader,int bytes,ref byte[] scratch,Action heartbeat)
    {
        if(bytes<0||bytes>MeshReadBufferBytes)throw new InvalidDataException("Mesh payload size");
        if(reader.BaseStream.CanSeek&&bytes>reader.BaseStream.Length-reader.BaseStream.Position)
            throw new EndOfStreamException("Truncated mesh payload");
        if(scratch==null||scratch.Length<MeshReadBufferBytes)scratch=new byte[MeshReadBufferBytes];
        int offset=0;
        while(offset<bytes){
            // Service the online transport even while reading a large mesh.
            heartbeat?.Invoke();
            int got=reader.Read(scratch,offset,bytes-offset);
            if(got<=0)throw new EndOfStreamException("Truncated mesh payload");
            offset+=got;
        }
        return new Span<byte>(scratch,0,bytes);
    }

    internal static void ReadMeshVertices(BinaryReader reader,Matrix4x4 matrix,Vector3[] positions,Vector3[] normals,
        Vector2[] uv,Vector2[] uv2,Color32[] colors,ref byte[] scratch,Action heartbeat,bool legacy=false)
    {
        int count=positions.Length;
        if(normals.Length!=count||uv.Length!=count||uv2.Length!=count||colors.Length!=count)
            throw new ArgumentException("Mesh vertex arrays differ in length");
        if(legacy||!PackedVertexLayout){
            for(int i=0;i<count;i++){
                if((i&4095)==0)heartbeat?.Invoke();
                positions[i]=matrix.MultiplyPoint3x4(Vec(reader));normals[i]=matrix.MultiplyVector(Vec(reader));
                uv[i]=new Vector2(reader.ReadSingle(),reader.ReadSingle());uv2[i]=new Vector2(reader.ReadSingle(),reader.ReadSingle());
                colors[i]=new Color32(reader.ReadByte(),reader.ReadByte(),reader.ReadByte(),reader.ReadByte());
            }
            return;
        }
        for(int offset=0;offset<count;){
            int n=Math.Min(MeshReadBufferBytes/44,count-offset);
            var source=MemoryMarshal.Cast<byte,DiskVertex>(ReadMeshPayload(reader,n*44,ref scratch,heartbeat));
            for(int j=0;j<n;j++){
                int i=offset+j;ref readonly var vertex=ref source[j];
                positions[i]=matrix.MultiplyPoint3x4(vertex.position);normals[i]=matrix.MultiplyVector(vertex.normal);
                uv[i]=vertex.uv;uv2[i]=vertex.uv2;colors[i]=vertex.color;
            }
            offset+=n;
        }
    }

    internal static void ReadMeshIndices(BinaryReader reader,int[] indices,int vertexCount,ref byte[] scratch,Action heartbeat,bool legacy=false)
    {
        if(legacy||!BitConverter.IsLittleEndian){
            for(int i=0;i<indices.Length;i++){
                int index=reader.ReadInt32();if(index<0||index>=vertexCount)throw new InvalidDataException("Vertex index");indices[i]=index;
            }
            return;
        }
        for(int offset=0;offset<indices.Length;){
            int n=Math.Min(MeshReadBufferBytes/4,indices.Length-offset);
            var source=MemoryMarshal.Cast<byte,int>(ReadMeshPayload(reader,n*4,ref scratch,heartbeat));
            for(int j=0;j<n;j++){
                int index=source[j];if(index<0||index>=vertexCount)throw new InvalidDataException("Vertex index");indices[offset+j]=index;
            }
            offset+=n;
        }
    }
}
