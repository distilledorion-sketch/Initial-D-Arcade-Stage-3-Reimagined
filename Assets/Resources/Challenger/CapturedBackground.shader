Shader "Hidden/InitialD/ChallengerCapturedBackground"
{
    Properties { [PerRendererData] _MainTex ("Captured scene", 2D) = "white" {} }
    SubShader
    {
        Tags { "Queue"="Transparent" "RenderType"="Transparent" "IgnoreProjector"="True" }
        Cull Off
        ZWrite Off
        ZTest [unity_GUIZTestMode]
        Blend Off
        Pass
        {
            CGPROGRAM
            #pragma vertex vert
            #pragma fragment frag
            #include "UnityCG.cginc"
            struct Input { float4 vertex:POSITION; float2 uv:TEXCOORD0; fixed4 color:COLOR; };
            struct Varying { float4 vertex:SV_POSITION; float2 uv:TEXCOORD0; fixed4 color:COLOR; };
            sampler2D _MainTex;
            Varying vert(Input v) { Varying o;o.vertex=UnityObjectToClipPos(v.vertex);o.uv=v.uv;o.color=v.color;return o; }
            fixed4 frag(Varying i):SV_Target
            {
                // Original RGB565 capture has opaque alpha. Vertex RGB and
                // source-alpha blending over black both attenuate the image.
                // Write the resulting opaque image, independent of Unity's
                // scene alpha and of whatever was behind the interrupted UI.
                return fixed4(tex2D(_MainTex,i.uv).rgb*i.color.rgb*i.color.a,1);
            }
            ENDCG
        }
    }
}
