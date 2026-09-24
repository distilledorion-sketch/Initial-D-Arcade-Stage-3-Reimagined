using System;
using System.IO;
using UnityEditor;
using UnityEngine;

public static class Idas3MeterCatalogBuild
{
    public static void VerifyOnly(){
        Idas3MeterLayoutBounds.RecalculateForVerification();
        Idas3HudCustomizationChecks.Run();
        foreach(string name in new[]{"ArcadeHud","ArcadeHudPreview"}){
            var shader=Resources.Load<Shader>(name);
            if(shader==null)throw new InvalidOperationException("HUD shader resource is missing: "+name);
            bool supported=shader.isSupported,errors=ShaderUtil.ShaderHasError(shader);
            if(!supported||errors){
                string details="";
                foreach(var message in ShaderUtil.GetShaderMessages(shader))details+="\n"+message.severity+": "+message.message+" ("+message.file+":"+message.line+")";
                throw new InvalidOperationException("Invalid HUD shader: "+name+"; supported="+supported+"; compilerErrors="+errors+details);
            }
        }
        string output=Idas3MeterCatalogChecks.Run();
        const string bakedPath="Assets/Resources/ArcadeHud/Catalog/layout-bounds.json";
        File.WriteAllText(bakedPath,Idas3MeterLayoutBounds.ExportVerifiedBake());
        AssetDatabase.ImportAsset(bakedPath,ImportAssetOptions.ForceSynchronousImport|ImportAssetOptions.ForceUpdate);
        Idas3MeterLayoutBounds.VerifyBakedCache();
        File.WriteAllText(Path.Combine(output,"bounds-cache.txt"),"PASS: sampled bounds baked with catalog/alpha SHA256 and rendererVersion; runtime cache reproduced all layouts without sampling.\n");
        Debug.Log("Meter layout bounds baked and runtime cache verified: "+bakedPath);
    }
    public static void VerifyAndBuild(){VerifyOnly();Idas3Build.RebuildWindowsPlayer();}
}
