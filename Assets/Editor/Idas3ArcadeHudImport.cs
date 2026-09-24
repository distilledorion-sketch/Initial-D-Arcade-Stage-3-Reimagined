using UnityEditor;
using UnityEngine;
public sealed class Idas3ArcadeHudImport : AssetPostprocessor {
    void OnPreprocessTexture(){
        if(!assetPath.StartsWith("Assets/Resources/ArcadeHud/",System.StringComparison.Ordinal))return;
        var t=(TextureImporter)assetImporter;t.textureType=TextureImporterType.Default;t.textureShape=TextureImporterShape.Texture2D;
        t.alphaSource=TextureImporterAlphaSource.FromInput;t.alphaIsTransparency=true;t.mipmapEnabled=false;t.isReadable=false;
        t.npotScale=TextureImporterNPOTScale.None;t.maxTextureSize=1024;t.textureCompression=TextureImporterCompression.Uncompressed;
        t.filterMode=FilterMode.Bilinear;t.wrapMode=TextureWrapMode.Clamp;t.sRGBTexture=true;
    }
}
