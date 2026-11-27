using System;
using UnityEngine;

// Original interrupt02 geometry, UVs and camera projection. Separate from the
// network owner so frame/layout verification does not need an online service.
public sealed class Idas3ChallengerBanner : MonoBehaviour
{
    [Serializable] public sealed class Part {public int texture;public bool flash;public float[] vertices;public int[] triangles;}
    [Serializable] public sealed class Geometry {public Part[] parts;public int badgeTexture;}
    internal int BadgeTexture {get; private set;}
    Idas3ChallengerGraphic[] graphics;
    RenderTexture repeatedBackground;
#if IDAS3_PORTABLE_PLAYER
    static readonly Texture2D[] discTextures = new Texture2D[7];
    static string DiscFile(string filename) => System.IO.Path.Combine(Application.streamingAssetsPath,
        "IDAS3", "data", "unity", "Challenger", filename);
#endif
    internal static Texture2D LoadTexture(int index)
    {
#if IDAS3_PORTABLE_PLAYER
        if (discTextures[index]) return discTextures[index];
        var pixels = System.IO.File.ReadAllBytes(DiscFile("interrupt_" + index + ".png"));
        var texture = new Texture2D(2, 2, TextureFormat.RGBA32, false);
        if (!texture.LoadImage(pixels)) { Destroy(texture); throw new InvalidOperationException("Invalid challenger texture"); }
        texture.wrapMode = TextureWrapMode.Clamp;
        return discTextures[index] = texture;
#else
        return Resources.Load<Texture2D>("Challenger/interrupt_" + index);
#endif
    }
    public void Initialize()
    {
#if IDAS3_PORTABLE_PLAYER
        string json = System.IO.File.ReadAllText(DiscFile("geometry.json"));
#else
        var source=Resources.Load<TextAsset>("Challenger/geometry");
        if(source==null)throw new InvalidOperationException("Missing original challenger geometry");
        string json = source.text;
#endif
        var geometry=JsonUtility.FromJson<Geometry>(json);
        if(geometry?.parts==null)throw new InvalidOperationException("Missing challenger geometry");
        if(geometry.parts.Length==4 && geometry.badgeTexture==0){
            BadgeTexture=4;
            geometry.parts[3].flash=true;
        }else if(geometry.parts.Length==6 && geometry.badgeTexture==6){
            BadgeTexture=geometry.badgeTexture;
        }else throw new InvalidOperationException("Invalid challenger geometry");
        graphics=new Idas3ChallengerGraphic[geometry.parts.Length];
        for(int i=0;i<graphics.Length;i++){
            var part=geometry.parts[i];
            Texture texture=LoadTexture(part.texture);
            if(texture==null)throw new InvalidOperationException("Missing challenger texture "+part.texture);
            if(part.texture==0){
                // Source U runs from 0 to 2; mirror the half-plate so its
                // outside border stays at the two ends, not across the middle.
                // Avoid changing the shared asset's
                // sampler (the import pipeline intentionally clamps UI assets).
                // Player textures have no CPU pixels. Instantiate(Texture2D)
                // loses their contents there even though it works in the Editor.
                repeatedBackground=new RenderTexture(texture.width,texture.height,0,RenderTextureFormat.ARGB32){
                    name="Original challenger mirrored plate",filterMode=texture.filterMode,
                    wrapModeU=TextureWrapMode.Mirror,wrapModeV=TextureWrapMode.Clamp,
                    useMipMap=false,autoGenerateMips=false,antiAliasing=1};
                if(!repeatedBackground.Create())throw new InvalidOperationException("Cannot create challenger mirrored plate");
                var previous=RenderTexture.active;
                try{Graphics.Blit(texture,repeatedBackground);}finally{RenderTexture.active=previous;}
                texture=repeatedBackground;
            }
            var go=new GameObject("Original interrupt part "+part.texture,typeof(RectTransform),typeof(CanvasRenderer),typeof(Idas3ChallengerGraphic));
            go.layer=gameObject.layer;go.transform.SetParent(transform,false);
            var rt=go.GetComponent<RectTransform>();rt.anchorMin=rt.anchorMax=rt.pivot=new Vector2(0,1);rt.sizeDelta=new Vector2(640,480);
            graphics[i]=go.GetComponent<Idas3ChallengerGraphic>();graphics[i].Initialize(part,texture);
        }
        ApplyFrame(0);
    }
    public void ApplyFrame(int frame){if(graphics!=null)foreach(var graphic in graphics)graphic.ApplyFrame(frame);}
    void OnDestroy(){if(repeatedBackground!=null){repeatedBackground.Release();if(Application.isPlaying)Destroy(repeatedBackground);else DestroyImmediate(repeatedBackground);}}
}
