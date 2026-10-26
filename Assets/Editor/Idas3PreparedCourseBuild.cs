using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public static class Idas3PreparedCourseBuild
{
    [Serializable] sealed class Report { public bool passed;public string error;public int formatChecks;public List<Idas8HakoneCourse.PreparedFaceBuildReport> packs=new List<Idas8HakoneCourse.PreparedFaceBuildReport>(); }
    public static List<Idas8HakoneCourse.PreparedFaceBuildReport> Stage(string course,string source,string destination){
        var reports=new List<Idas8HakoneCourse.PreparedFaceBuildReport>();
        if(!Idas8HakoneCourse.UsesPreparedFaces(course))return reports;
        var scenes=Directory.GetFiles(source,"scene.bin",SearchOption.AllDirectories);Array.Sort(scenes,StringComparer.Ordinal);
        foreach(string scene in scenes){
            string folder=Path.GetDirectoryName(scene),relative=Path.GetRelativePath(source,folder);
            var result=Idas8HakoneCourse.BakePreparedFaces(course,folder,Path.Combine(destination,relative,"scene.faces"));
            reports.Add(result);Debug.Log("PREPARED_COURSE: "+JsonUtility.ToJson(result));
        }
        return reports;
    }
    // Private build investigation. Does not write RuntimeAssets or any game install.
    public static void Run(){
        string requested=Environment.GetEnvironmentVariable("IDAS3_PREPARED_FACES_OUTPUT");
        if(string.IsNullOrWhiteSpace(requested)||!Path.IsPathFullyQualified(requested))throw new ArgumentException("An absolute new prepared-face output is required.");
        string output=Path.GetFullPath(requested),root=Path.GetFullPath(Path.Combine(Application.dataPath,".."));
        if(!output.StartsWith(Path.Combine(root,"Verification")+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase)||Directory.Exists(output)||File.Exists(output))throw new IOException("Use a NEW output directory inside Verification.");
        Directory.CreateDirectory(output);var report=new Report();
        try{
            report.formatChecks=Idas8HakoneCourse.VerifyPreparedFaceFormat(Path.Combine(output,"format-checks"));
            foreach(string course in Idas3CourseCatalog.Packs)report.packs.AddRange(Stage(course,Path.Combine(root,"RuntimeAssets",course),Path.Combine(output,course)));
            if(report.packs.Count==0)throw new InvalidDataException("No imported packs prepared");
            report.passed=true;
        }catch(Exception error){report.error=error.ToString();throw;}
        finally{File.WriteAllText(Path.Combine(output,"report.json"),JsonUtility.ToJson(report,true));}
    }
}
