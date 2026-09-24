Shader "Idas3/Ornament Overlay" {
 Properties { _MainTex("Texture",2D)="white"{} _Screen("Screen coordinates",Float)=0 }
 SubShader { Tags {"Queue"="Overlay"} Pass {
  Cull Off ZWrite Off ZTest Always Blend One OneMinusSrcAlpha
  HLSLPROGRAM
  #pragma vertex vert
  #pragma fragment frag
  #include "UnityCG.cginc"
  sampler2D _MainTex;float _Screen;float4 _Canvas,_Rect;
  struct app {float4 vertex:POSITION;float2 uv:TEXCOORD0;float4 color:COLOR;};
  struct data {float4 pos:SV_POSITION;float2 uv:TEXCOORD0;};
  data vert(app v){data o;o.pos=UnityObjectToClipPos(v.vertex);
   if(_Screen>.5){float2 p=_Rect.xy+v.vertex.xy*_Rect.zw;o.pos=float4(2*p.x/_Canvas.x-1,1-2*p.y/_Canvas.y,0,1);o.pos.y*=_ProjectionParams.x;}
   o.uv=v.uv;return o;}
  float4 frag(data i):SV_Target{return tex2D(_MainTex,i.uv);}
  ENDHLSL
 } }
}
