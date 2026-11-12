using System;

// Original interrupt owner 0C16C860/0C16CBA0. The first 40 X values are the
// float results of its original trigonometry, not a host easing approximation.
// Verification/challenger-animation-20261110 records the executed source.
public static class Idas3ChallengerAnimation
{
    public const int LastFrame=181;
    public const double Duration=(LastFrame+1)/60.0;
    static readonly float[] ArrivalX={
        8.60000038f,7.7820015f,6.61086607f,5.21568966f,3.72530079f,2.2562356f,
        .908790708f,-.241707072f,-1.14380717f,-1.77342224f,-2.13035631f,-2.23444963f,
        -2.12235355f,-1.84099567f,-1.44273102f,-.980177402f,-.501473606f,-.0476202555f,
        .35048002f,.672648013f,.90894407f,1.0593878f,1.13138556f,1.13815844f,
        1.09590781f,1.02187502f,.932227612f,.84066689f,.757461786f,.68915844f,
        .638629436f,.605727375f,.588008106f,.581746697f,.582791507f,.587359488f,
        .59256202f,.596651673f,.599007905f,.599883795f
    };
    public static int FrameAt(double seconds)=>!(seconds>0)?0:
        seconds>=Duration?LastFrame:(int)Math.Floor(seconds*60.0);
    public static float X(int frame)=>frame<40?ArrivalX[Math.Max(0,frame)]:.6f;
    public static byte FlashAlpha(int frame)
    {
        int phase=frame-40;
        return (byte)Math.Max(0,Math.Min(255,phase<=7?phase*32:(24-phase)*16));
    }
    public static void BackdropVertex(int frame,int index,out float x,out float y,out byte channel)
    {
        if(index<0||index>=100)throw new ArgumentOutOfRangeException(nameof(index));
        // 0C16C980: a slanted, 100-vertex screen strip. The OR is intentional:
        // replacing it with a smooth fade loses the travelling brightness bands.
        int pair=index/2;
        x=pair/50f*1024f*2f;y=(index&1)==0?0f:512f;
        if((index&1)!=0)x-=160f;
        int shade=Math.Max(0,Math.Min(60,pair+60-Math.Max(0,frame)));
        channel=(byte)((shade*255/60)|0xc0);
    }
    public static void Project(int frame,float x,float y,float z,out float screenX,out float screenY)
    {
        // Actual 145EC0 bank matrix and default 1D0940/1D0880 camera. Preserve
        // the authored per-vertex depth; the lettering is not a flat rectangle.
        const float scale=.0258883499f;
        float px=x*scale+(-3.1999998092651367f+X(frame))*scale;
        float py=y*scale+(2.3999998569488525f-1.8f)*scale;
        float depth=.15000000596046448f-z;
        screenX=320f+320f*1.8106600046157837f*px/depth;
        screenY=240f-240f*2.4142134189605713f*py/depth;
    }
}
