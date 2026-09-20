Shader "Idas3/Original UI"
{
 Properties { _MainTex ("Source sprite", 2D) = "white" {} _SrcBlend ("Source blend", Int) = 5 _DstBlend ("Destination blend", Int) = 10 }
 SubShader {
  Tags { "Queue"="Overlay" "RenderType"="Transparent" }
  Pass {
   Cull Off ZWrite Off ZTest Always
   Blend [_SrcBlend] [_DstBlend]
   HLSLPROGRAM
   #pragma vertex vert
   #pragma fragment frag
   #pragma target 4.0
   #include "UnityCG.cginc"
   sampler2D _MainTex;
   float4 _MainTex_TexelSize, _Canvas, _Clip, _Tint;
   float _Opacity;
   uint _Tsp, _Pcw, _Original, _SourceIsOne;
   struct Input { float4 position:POSITION; float2 uv:TEXCOORD0; float4 color:COLOR; float4 offset:TEXCOORD1; };
   struct Output { float4 position:SV_POSITION; float2 uv:TEXCOORD0; float4 color:COLOR; float4 offset:TEXCOORD1; float2 pixel:TEXCOORD2; };
   Output vert(Input i) { Output o; o.position=float4(2*i.position.x/_Canvas.x-1,1-2*i.position.y/_Canvas.y,0,1); o.position.y*=_ProjectionParams.x;
    o.uv=i.uv; o.color=i.color; o.offset=i.offset; o.pixel=i.position.xy; return o; }
   float coord(float uv,float size,bool clampUv,bool mirrorUv) {
    if(clampUv) return (clamp(floor(uv*size),0,size-1)+.5)/size;
    float base=floor(uv), part=uv-base; if(mirrorUv && (int(base)&1))part=1-part;
    return (clamp(floor(part*size),0,size-1)+.5)/size;
   }
   float4 frag(Output i):SV_TARGET {
    clip(i.pixel-_Clip.xy); clip(_Clip.zw-i.pixel);
    float2 uv=float2(coord(i.uv.x,_MainTex_TexelSize.z,(_Tsp&(1u<<16))!=0,(_Tsp&(1u<<18))!=0),coord(i.uv.y,_MainTex_TexelSize.w,(_Tsp&(1u<<15))!=0,(_Tsp&(1u<<17))!=0));
    float4 sample=tex2Dlod(_MainTex,float4(uv,0,0)); if(_Tsp&(1u<<19))sample.a=1;
    float4 vertex=i.color;if(_Original && !(_Tsp&(1u<<20)))vertex.a=1;
    float4 value=vertex;
    if(_Pcw&8) { uint env=(_Tsp>>6)&3;
     if(env==0)value=sample;
     else if(env==1)value=float4(vertex.rgb*sample.rgb,sample.a);
     else if(env==2)value=float4(lerp(vertex.rgb,sample.rgb,sample.a),vertex.a);
     else value=vertex*sample;
    }
    if((_Pcw&4)||!(_Pcw&8))value.rgb+=i.offset.rgb;
    value=saturate(value)*_Tint;value.a*=_Opacity;if(_SourceIsOne)value.rgb*=_Opacity;
    return value;
   }
   ENDHLSL
  }
 }
}
