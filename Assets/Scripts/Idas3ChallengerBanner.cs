using System;
using UnityEngine;

// Original interrupt02 geometry, UVs and camera projection. Separate from the
// network owner so frame/layout verification does not need an online service.
public sealed class Idas3ChallengerBanner : MonoBehaviour
{
    [Serializable] public sealed class Part {public int texture;public float[] vertices;public int[] triangles;}
    [Serializable] public sealed class Geometry {public Part[] parts;}
    Idas3ChallengerGraphic[] graphics;
    RenderTexture repeatedBackground;
    public void Initialize()
    {
        var source=Resources.Load<TextAsset>("Challenger/geometry");
        if(source==null)throw new InvalidOperationException("Missing original challenger geometry");
        var geometry=JsonUtility.FromJson<Geometry>(source.text);
        if(geometry?.parts==null||geometry.parts.Length!=4)throw new InvalidOperationException("Invalid challenger geometry");
        graphics=new Idas3ChallengerGraphic[4];
        for(int i=0;i<graphics.Length;i++){
            var part=geometry.parts[i];
            Texture texture=Resources.Load<Texture2D>("Challenger/interrupt_"+part.texture);
            if(texture==null)throw new InvalidOperationException("Missing challenger texture "+part.texture);
            if(part.texture==0){
                // Source U runs from 0 to 2. Avoid changing the shared asset's
                // sampler (the import pipeline intentionally clamps UI assets).
                // Player textures have no CPU pixels. Instantiate(Texture2D)
                // loses their contents there even though it works in the Editor.
                repeatedBackground=new RenderTexture(texture.width,texture.height,0,RenderTextureFormat.ARGB32){
                    name="Original challenger repeat texture",filterMode=texture.filterMode,
                    wrapModeU=TextureWrapMode.Repeat,wrapModeV=TextureWrapMode.Clamp,
                    useMipMap=false,autoGenerateMips=false,antiAliasing=1};
                if(!repeatedBackground.Create())throw new InvalidOperationException("Cannot create challenger repeat texture");
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
