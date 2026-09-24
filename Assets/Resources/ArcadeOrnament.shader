Shader "Idas3/Actual Ornament" {
 Properties { _MainTex("Texture",2D)="white"{} _Color("Tint",Color)=(1,1,1,1) _Cutoff("Alpha cutoff",Float)=0 _Cull("Cull",Float)=2 _ZWrite("Depth",Float)=1 }
 SubShader { Tags {"RenderType"="Transparent"} Pass {
  Cull [_Cull] ZWrite [_ZWrite] ZTest LEqual Blend SrcAlpha OneMinusSrcAlpha, One OneMinusSrcAlpha
  HLSLPROGRAM
  #pragma vertex vert
  #pragma fragment frag
  #include "UnityCG.cginc"
  sampler2D _MainTex;float4 _Color;float _Cutoff;
  struct app {float4 vertex:POSITION;float3 normal:NORMAL;float2 uv:TEXCOORD0;};
  struct data {float4 pos:SV_POSITION;float3 normal:TEXCOORD1;float2 uv:TEXCOORD0;};
  data vert(app v){data o;o.pos=UnityObjectToClipPos(v.vertex);o.normal=UnityObjectToWorldNormal(v.normal);o.uv=v.uv;return o;}
  float4 frag(data i):SV_Target {float4 c=tex2D(_MainTex,i.uv)*_Color;clip(c.a-max(_Cutoff,.002));
   float lighting=.72+.28*abs(dot(normalize(i.normal),normalize(float3(-.4,.65,-1))));c.rgb*=lighting;return c;}
  ENDHLSL
 } }
}
