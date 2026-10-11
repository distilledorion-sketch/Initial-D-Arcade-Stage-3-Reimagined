Shader "IDAS3/Projected Headlight"
{
 Properties {
  _MainTex("Original beam texture",2D)="white"{}
  _TrackSurface("Track surface lighting",Float)=0
  _SrcBlend("Source RGB",Float)=2
  _DstBlend("Destination RGB",Float)=1
  _SrcBlendAlpha("Source alpha",Float)=0
  _DstBlendAlpha("Destination alpha",Float)=1
  _ZWrite("Depth write",Float)=0
  _ZTest("Depth comparison",Float)=4
  _ImportedHeadlightFill("Imported road beam fill",Range(0,1))=.35
 }
 SubShader {
  Tags { "RenderType"="Transparent" }
  HLSLINCLUDE
   #include "Idas3SceneCommon.cginc"
   float _ImportedHeadlightFill;
   float4 importedFillPS(P v):SV_TARGET {
    // Native frame flag is set only for imported night races/replays.
    // The original projector still owns on/off, texture, road contact,
    // depth and player/opponent view masks through mainPS.
    clip(lampDirection.w-.5);clip(_ImportedHeadlightFill-.001);
    return float4(mainPS(v).rgb*saturate(_ImportedHeadlightFill),0);
   }
  ENDHLSL
  Pass {
   Name "SOURCE"
   Cull Off ZWrite [_ZWrite] ZTest [_ZTest]
   Blend [_SrcBlend] [_DstBlend], [_SrcBlendAlpha] [_DstBlendAlpha]
   HLSLPROGRAM
   #pragma target 4.5
   #pragma vertex mainVS
   #pragma fragment mainPS
   ENDHLSL
  }
  Pass {
   Name "IMPORTED_FILL"
   Cull Off ZWrite Off ZTest [_ZTest] ColorMask RGB
   // Lift a dark road inside the existing beam, tapering the added light
   // as the destination approaches white. A uniform exposure change would
   // brighten the sky and everything outside the headlights as well.
   Blend OneMinusDstColor One, Zero One
   HLSLPROGRAM
   #pragma target 4.5
   #pragma vertex mainVS
   #pragma fragment importedFillPS
   ENDHLSL
  }
 }
 Fallback Off
}
