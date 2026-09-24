Shader "Idas3/Arcade HUD Preview" {
 Properties { _MainTex("Texture",2D)="white"{} _Fill("Pedal fill",Float)=-1 [HideInInspector] _DstBlend("Destination blend",Float)=10 }
 SubShader { Tags { "Queue"="Overlay" "RenderType"="Transparent" } Pass {
  Cull Off ZWrite Off ZTest Always Blend SrcAlpha [_DstBlend]
  HLSLPROGRAM
  #pragma vertex vert
  #pragma fragment frag
  #include "UnityCG.cginc"
  #include "ArcadeHudImported.cginc"
  sampler2D _MainTex;float _Fill,_Brake;
  struct app {float4 vertex:POSITION;float2 uv:TEXCOORD0;float4 color:COLOR;};
  struct fragdata {float4 pos:SV_POSITION;float2 uv:TEXCOORD0;float4 color:COLOR;};
  fragdata vert(app i){fragdata o;o.pos=UnityObjectToClipPos(i.vertex);o.uv=i.uv;o.color=i.color;return o;}
  float4 frag(fragdata i):SV_Target {
   float4 result=tex2D(_MainTex,i.uv)*i.color;
   if(_Fill>=0){float2 d=i.uv-.5;float phase=frac(atan2(d.y,d.x)/6.2831853-.25);
    float arc=_Brake>.5?(.375-phase)/.25:(phase-.625)/.25;float aa=max(fwidth(arc),.002);
    result.a*=step(.0001,_Fill)*(1-smoothstep(_Fill-aa,_Fill+aa,arc));}
   result.a*=ImportedMeterAlpha(i.uv);return result;
  }
  ENDHLSL
 } }
}
