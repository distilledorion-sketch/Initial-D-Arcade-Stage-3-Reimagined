#if UNITY_EDITOR
using System;
using System.IO;
using UnityEngine;

public sealed partial class Idas8HakoneCourse
{
    public static int VerifyPreparedFaceFormat(string folder){
        Directory.CreateDirectory(folder);int checks=0;
        void Check(bool value,string reason){++checks;if(!value)throw new InvalidOperationException(reason);}
        byte[] input={1,2,3,4,5};string valid=Path.Combine(folder,"valid.bin");
        using(var scene=new MemoryStream(input))Idas8PreparedFaces.Write(valid,scene,"manifest","course",new[]{2,-1,0},new[]{new byte[]{3},null,Array.Empty<byte>()});
        using(var scene=new MemoryStream(input)){
            scene.Position=2;var decoded=Idas8PreparedFaces.Read(valid,scene,"manifest","course",3);
            Check(scene.Position==2,"Prepared reader moved geometry cursor");
            Check(Idas8PreparedFaces.Equal(decoded.ForShape(0,6),new byte[]{3}),"Prepared flags changed");
            Check(decoded.ForShape(1,6)==null&&decoded.ForShape(0,9)==null&&decoded.ForShape(3,0)==null,"Prepared shape mismatch accepted");
            Check(decoded.ForShape(2,0)?.Length==0,"Empty mesh lost");
        }
        void Reject(string name,byte[] file,byte[] geometry,string manifest,string course,int count){
            string path=Path.Combine(folder,name+".bin");File.WriteAllBytes(path,file);bool rejected=false;
            using(var scene=new MemoryStream(geometry))try{Idas8PreparedFaces.Read(path,scene,manifest,course,count);}catch(Exception error) when(error is IOException||error is InvalidDataException){rejected=true;}
            Check(rejected,"Invalid prepared faces accepted: "+name);
        }
        var good=File.ReadAllBytes(valid);var corrupt=(byte[])good.Clone();corrupt[112]^=1;
        Reject("checksum",corrupt,input,"manifest","course",3);
        var shortFile=new byte[good.Length-1];Array.Copy(good,shortFile,shortFile.Length);Reject("truncated",shortFile,input,"manifest","course",3);
        Reject("source",good,new byte[]{5,4,3,2,1},"manifest","course",3);
        Reject("manifest",good,input,"other","course",3);Reject("course",good,input,"manifest","other",3);Reject("shapes",good,input,"manifest","course",4);
        byte[] Rehash(byte[] file){using(var hash=System.Security.Cryptography.SHA256.Create()){var sum=hash.ComputeHash(file,0,file.Length-32);Array.Copy(sum,0,file,file.Length-32,32);}return file;}
        var version=(byte[])good.Clone();version[4]=2;Reject("version",Rehash(version),input,"manifest","course",3);
        var padding=(byte[])good.Clone();padding[112]=0x83;Reject("padding",Rehash(padding),input,"manifest","course",3);
        Idas8ImportedTreeChecks.Run();Check(true,"Original foliage preparation checks");
        return checks;
    }
    [Serializable] public sealed class PreparedFaceBuildReport {
        public string course,source,output;public int shapes,preparedShapes;public long triangles,bytes;public bool verified;
    }
    public static PreparedFaceBuildReport BakePreparedFaces(string course,string source,string destination){
        string manifestText=File.ReadAllText(Path.Combine(source,"scene.json"));
        var manifest=JsonUtility.FromJson<Manifest>(manifestText);
        var report=new PreparedFaceBuildReport{course=course,source=Path.GetFullPath(source),output=Path.GetFullPath(destination)};
        using(var reader=new BinaryReader(File.OpenRead(Path.Combine(source,"scene.bin")))){
            Magic(reader,"HKN2");int count=reader.ReadInt32();
            if(count!=manifest.shapes||count<0||count>10000)throw new InvalidDataException("Bake shape count");
            report.shapes=count;var counts=new int[count];var masks=new byte[count][];
            for(int shape=0;shape<count;++shape){
                int material=reader.ReadInt32(),nv=reader.ReadInt32(),nt=reader.ReadInt32();
                if(material<0||material>=manifest.materials.Length||nv<0||nv>2000000||nt<0||nt>6000000||nt%3!=0)throw new InvalidDataException("Bake mesh header");
                Matrix4x4 matrix=Matrix4x4.identity;for(int row=0;row<3;++row)for(int col=0;col<4;++col)matrix[row,col]=reader.ReadSingle();
                long end=reader.BaseStream.Position+nv*44L+nt*4L;
                if(end>reader.BaseStream.Length)throw new EndOfStreamException("Bake mesh data");
                if(!UsesPairedFoliageFaces(course,manifest.materials[material])){counts[shape]=-1;reader.BaseStream.Position=end;continue;}
                var vertices=new Vector3[nv];var data=reader.ReadBytes(checked(nv*44));
                for(int i=0;i<nv;++i){int at=i*44;vertices[i]=matrix.MultiplyPoint3x4(new Vector3(BitConverter.ToSingle(data,at),BitConverter.ToSingle(data,at+4),BitConverter.ToSingle(data,at+8)));}
                var indices=new int[nt];for(int i=0;i<nt;++i){indices[i]=reader.ReadInt32();if(indices[i]<0||indices[i]>=nv)throw new InvalidDataException("Bake vertex index");}
                counts[shape]=nt/3;masks[shape]=ClassifyTreeFaces(vertices,indices);
                ++report.preparedShapes;report.triangles+=nt/3;
            }
            if(reader.BaseStream.Position!=reader.BaseStream.Length)throw new InvalidDataException("Trailing bake geometry");
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(destination)));
            string pending=destination+"."+Guid.NewGuid().ToString("N")+".tmp";
            try{
                Idas8PreparedFaces.Write(pending,reader.BaseStream,manifestText,course,counts,masks);
                var decoded=Idas8PreparedFaces.Read(pending,reader.BaseStream,manifestText,course,count);
                for(int i=0;i<count;++i)if(counts[i]>=0&&!Idas8PreparedFaces.Equal(masks[i],decoded.ForShape(i,counts[i]*3)))throw new InvalidDataException("Prepared face roundtrip");
                report.bytes=new FileInfo(pending).Length;
                if(File.Exists(destination))File.Replace(pending,destination,null);else File.Move(pending,destination);
                report.verified=true;
            }finally{if(File.Exists(pending))File.Delete(pending);}
        }
        return report;
    }
}
#endif
