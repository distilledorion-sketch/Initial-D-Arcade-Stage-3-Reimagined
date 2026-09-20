Shader "InitialD/HakonePrototype" {
 Properties {
  _MainTex("Base",2D)="white" {} _ShadowTex("Road shadow",2D)="white" {}
  _Cutoff("Cutoff",Float)=0 _HasShadow("Shadow",Float)=0
  _SrcBlend("Source blend",Float)=1 _DstBlend("Dest blend",Float)=0 _ZWrite("Depth write",Float)=1
 }
 SubShader { Tags { "RenderType"="Opaque" } Pass {
  Cull Off ZWrite [_ZWrite] Blend [_SrcBlend] [_DstBlend]
  CGPROGRAM
  #pragma vertex vert
  #pragma fragment frag
  #include "UnityCG.cginc"
  sampler2D _MainTex, _ShadowTex; float _Cutoff,_HasShadow;
  struct vIn { float4 vertex:POSITION; float2 uv:TEXCOORD0; float2 uv2:TEXCOORD1; fixed4 color:COLOR; };
  struct vOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float2 uv2:TEXCOORD1; fixed4 color:COLOR; };
  vOut vert(vIn v) { vOut o; o.pos=UnityObjectToClipPos(v.vertex); o.uv=v.uv; o.uv2=v.uv2; o.color=v.color; return o; }
  fixed4 frag(vOut i):SV_Target {
   fixed4 c=tex2D(_MainTex,i.uv); clip(c.a-_Cutoff);
   c.rgb*=i.color.rgb;
   // Prototype ambient floor; Stage 8's lighting constants are not imported yet.
   fixed4 s=tex2D(_ShadowTex,i.uv2); c.rgb*=lerp(1,max(s.rgb,.5),s.a*_HasShadow);
   return c;
  }
  ENDCG
 } }
}
