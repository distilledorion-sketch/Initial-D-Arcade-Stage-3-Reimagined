using UnityEngine;
using UnityEngine.UI;

public sealed class Idas3ChallengerGraphic : MaskableGraphic
{
    Idas3ChallengerBanner.Part part;
    Texture texture;
    int frame=-1;
    public override Texture mainTexture=>texture!=null?texture:Texture2D.whiteTexture;
    public void Initialize(Idas3ChallengerBanner.Part data,Texture image)
    {
        part=data;texture=image;raycastTarget=false;SetAllDirty();
    }
    public void ApplyFrame(int value){if(frame==value)return;frame=value;SetVerticesDirty();}
    protected override void OnPopulateMesh(VertexHelper mesh)
    {
        mesh.Clear();if(part==null)return;
        var tint=new Color32(255,255,255,part.texture==3?Idas3ChallengerAnimation.FlashAlpha(frame):(byte)255);
        for(int i=0;i<part.vertices.Length;i+=5){
            Idas3ChallengerAnimation.Project(frame,part.vertices[i],part.vertices[i+1],part.vertices[i+2],out float x,out float y);
            // The decoded PNG retains the source's inverted rows. At the
            // authored top V=0, sample Unity V=0 (the PNG's bottom row).
            mesh.AddVert(new Vector3(x,-y,0),tint,new Vector2(part.vertices[i+3],-part.vertices[i+4]));
        }
        for(int i=0;i<part.triangles.Length;i+=3)mesh.AddTriangle(part.triangles[i],part.triangles[i+1],part.triangles[i+2]);
    }
}
