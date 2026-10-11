Shader "IDAS3/Imported Lamp Corona"
{
 Properties { _MainTex("Source light glare atlas",2D)="black"{} }
 SubShader {
  // After opaque scenery/cars, before UI. Depth-test against the same native
  // projection as the imported scenery; never draw through hills or buildings.
  Tags { "Queue"="Transparent" "RenderType"="Transparent" }
  Pass {
   Cull Off ZWrite Off ZTest LEqual Blend One OneMinusSrcAlpha
   HLSLPROGRAM
   #pragma target 4.5
   #pragma vertex vert
   #pragma fragment frag
   #include "UnityCG.cginc"
   StructuredBuffer<float4> _IdasFrameWords;
   uint _IdasView;float4 _IdasDepthProjection;
   Texture2D _MainTex;SamplerState sampler_MainTex;
   float4 F(uint i){return _IdasFrameWords[_IdasView*23+i];}
   struct V {float3 center:POSITION;float3 direction:NORMAL;float2 uv:TEXCOORD0;float2 corner:TEXCOORD1;float4 color:COLOR;};
   struct P {float4 position:SV_POSITION;float2 uv:TEXCOORD0;float4 color:COLOR;};
   P vert(V v){
    P o;float3 toEye=F(4).xyz-v.center;float distance=length(toEye);
    float3 world=v.center+F(8).xyz*v.corner.x+F(9).xyz*v.corner.y;
    o.position=mul(float4(world,1),float4x4(F(0),F(1),F(2),F(3)));o.position.y*=_ProjectionParams.x;
    #if defined(UNITY_REVERSED_Z)
     o.position.z=_IdasDepthProjection.w!=0?_IdasDepthProjection.x-_IdasDepthProjection.y*o.position.w:o.position.w-o.position.z;
    #endif
    o.uv=v.uv;
    // Fade from above the fixture and into the existing night fog. Camera-facing
    // geometry uses each view's basis, so the mirror never sees a sideways card.
    float facing=v.uv.x<0?1:smoothstep(-.15,.2,dot(v.direction,toEye/max(distance,.001)));
    float fog=saturate((distance-85)/410);
    o.color=v.color*(facing*(1-fog*fog));return o;
   }
   float4 frag(P p):SV_TARGET{
    if(p.uv.x<0)return float4(p.color.rgb,p.color.a);
    float4 glow=_MainTex.Sample(sampler_MainTex,p.uv);
    return float4(glow.rgb*p.color.rgb,0);
   }
   ENDHLSL
  }
 }
 Fallback Off
}
