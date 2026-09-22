Shader "Idas3/HUD editor car" {
 Properties { _MainTex("Texture",2D)="white"{} }
 SubShader { Tags { "RenderType"="Opaque" } Pass {
 Cull Off
 CGPROGRAM
 #pragma vertex vert
 #pragma fragment frag
 #include "UnityCG.cginc"
 sampler2D _MainTex;
 struct input { float4 vertex:POSITION; float3 normal:NORMAL; float2 uv:TEXCOORD0; };
 struct output { float4 position:SV_POSITION; float2 uv:TEXCOORD0; float light:TEXCOORD1; };
 output vert(input v){output o;o.position=UnityObjectToClipPos(v.vertex);o.uv=v.uv;o.light=.5+.5*abs(dot(UnityObjectToWorldNormal(v.normal),normalize(float3(.3,1,-.5))));return o;}
 fixed4 frag(output i):SV_Target {fixed4 color=tex2D(_MainTex,i.uv);clip(color.a-.05);return fixed4(color.rgb*i.light,1);}
 ENDCG
 } }
}
