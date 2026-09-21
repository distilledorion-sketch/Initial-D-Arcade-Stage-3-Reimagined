Shader "IDAS3/Original Scene Material"
{
 Properties {
  _MainTex("Original texture",2D)="white"{}
  _SrcBlend("Source RGB",Float)=1
  _DstBlend("Destination RGB",Float)=0
  _SrcBlendAlpha("Source alpha",Float)=1
  _DstBlendAlpha("Destination alpha",Float)=0
  _ZWrite("Depth write",Float)=1
  _ZTest("Depth comparison",Float)=4
  _ImportedShadowTex("Imported road shadow",2D)="white"{}
  _AlphaToMask("Alpha coverage",Float)=0
  _ImportedCoverage("Imported cutout coverage",Float)=0
  _ImportedCutoff("Imported alpha cutoff",Float)=0
  _ImportedHasShadow("Imported road shadow enabled",Float)=0
  _ImportedSky("Imported sky",Float)=0
  _ImportedNight("Imported night scenery",Float)=0
  _ImportedPs2Lighting("PS2 baked color and linear fog",Float)=0
  _ImportedSkyFollowXZ("Sky follows camera XZ",Float)=0
  _ImportedBillboard("Upright spectator sprite",Float)=0
 }
 SubShader {
  Tags { "RenderType"="Opaque" }
  Pass {
   AlphaToMask [_AlphaToMask]
   Cull Off ZWrite [_ZWrite] ZTest [_ZTest]
   Blend [_SrcBlend] [_DstBlend], [_SrcBlendAlpha] [_DstBlendAlpha]
   HLSLPROGRAM
   #pragma target 5.0
   #pragma vertex mainVS
   #pragma geometry showroomGeometry
   #pragma fragment mainPS
   #pragma multi_compile_local _ IDAS_IMPORTED_COURSE
   #pragma multi_compile_instancing
   #include "Idas3SceneCommon.cginc"
   ENDHLSL
  }
 }
 Fallback Off
}
