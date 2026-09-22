using System;
using System.IO;
using System.Reflection;
using UnityEngine;
public static class Idas3SponsorChecks {
    static void Require(bool condition,string message){if(!condition)throw new Exception(message);}
    static Vector3 V(BinaryReader r)=>new Vector3(r.ReadSingle(),r.ReadSingle(),r.ReadSingle());
    public static void Build(){Run();Idas3HudSizeBuild.Build();}
    public static void Run(){
        var method=typeof(Idas8HakoneCourse).GetMethod("SponsorFaceTags",BindingFlags.Static|BindingFlags.NonPublic);
        string summary="";
        foreach(string course in new[]{"SADAMINE","HAKONE"})
        foreach(string variant in new[]{"","day_wet","night_dry","night_wet"}){
            string root=Path.Combine("RuntimeAssets",course,variant);
            var manifest=JsonUtility.FromJson<Idas8HakoneCourse.Manifest>(File.ReadAllText(Path.Combine(root,"scene.json")));
            int tagged=0,checkedCorners=0;
            using(var r=new BinaryReader(File.OpenRead(Path.Combine(root,"scene.bin")))){
                r.ReadBytes(4);int count=r.ReadInt32();
                for(int shape=0;shape<count;shape++){
                    int material=r.ReadInt32(),nv=r.ReadInt32(),nt=r.ReadInt32();r.ReadBytes(48);
                    if(!(bool)typeof(Idas8HakoneCourse).GetMethod("IsSponsorMaterial",BindingFlags.Static|BindingFlags.NonPublic).Invoke(null,new object[]{course,manifest.materials[material].name})){r.BaseStream.Position+=nv*44L+nt*4L;continue;}
                    var v=new Vector3[nv];var n=new Vector3[nv];var uv=new Vector2[nv];var uv2=new Vector2[nv];var c=new Color32[nv];var ix=new int[nt];
                    for(int i=0;i<nv;i++){v[i]=V(r);n[i]=V(r);uv[i]=new Vector2(r.ReadSingle(),r.ReadSingle());uv2[i]=new Vector2(r.ReadSingle(),r.ReadSingle());c[i]=new Color32(r.ReadByte(),r.ReadByte(),r.ReadByte(),r.ReadByte());}
                    for(int i=0;i<nt;i++)ix[i]=r.ReadInt32();var old=(int[])ix.Clone();
                    object[] args={v,n,uv,uv2,c,ix,course=="SADAMINE"&&variant=="night_wet"?.25f:0,course=="HAKONE"};var tags=(Vector2[])method.Invoke(null,args);if(tags==null)continue;
                    var vv=(Vector3[])args[0];var nn=(Vector3[])args[1];var tt=(Vector2[])args[2];var tt2=(Vector2[])args[3];var cc=(Color32[])args[4];
                    for(int i=0;i<nt;i++){
                        int a=old[i],b=ix[i];Require(v[a]==vv[b]&&n[a]==nn[b]&&uv[a]==tt[b]&&uv2[a]==tt2[b]&&c[a].Equals(cc[b]),"Geometry or texture data changed");checkedCorners++;
                    }
                    for(int i=0;i<nt;i+=3){
                        float axis=tags[ix[i]].y;Require(axis==tags[ix[i+1]].y&&axis==tags[ix[i+2]].y,"Tag leaked into neighboring triangle");
                        if(axis==0)continue;tagged++;if(course=="HAKONE"){
                            Require(axis==-.25f,"Wrong Hakone reflection axis");
                            for(int k=0;k<3;k++){var t=tt[ix[i+k]];Require(t.x>=0&&t.x<=7/16f+.00001f&&t.y>=0&&t.y<=.25001f,"Non-logo Hakone region tagged");}
                            continue;
                        }
                        Require(axis==.125f||axis==.375f||axis==.625f||axis==.875f,"Wrong sponsor tile");
                        for(int k=0;k<3;k++){var t=tt[ix[i+k]];Require(t.x>=axis/2-.06251f&&t.x<=axis/2+.06251f&&t.y>=0&&t.y<=.25001f,"Non-logo region tagged");}
                    }
                }
            }
            Debug.Log($"Sponsor check {course} {variant}: {tagged} / {checkedCorners}");Require(tagged>100,"Sponsor panels missing: "+variant);summary+=$"PASS {course} {variant}: {tagged} tagged triangles; {checkedCorners} corners preserve positions/normals/colors/UVs; no neighboring triangle tag leaks.\n";
        }
        Directory.CreateDirectory("Verification/hakone-signs-20260922");File.WriteAllText("Verification/hakone-signs-20260922/mesh-checks.txt",summary);Debug.Log(summary);
    }
}
