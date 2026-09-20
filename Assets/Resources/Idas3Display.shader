Shader "Hidden/IDAS3/Display"
{
    Properties { _MainTex ("Native frame", 2D) = "black" {} }
    SubShader
    {
        Cull Off ZWrite Off ZTest Always
        Pass
        {
            CGPROGRAM
            #pragma vertex vert_img
            #pragma fragment frag
            #include "UnityCG.cginc"
            sampler2D _MainTex;
            float _FlipY, _SourceAspect, _OutputAspect;
            fixed4 frag(v2f_img input) : SV_Target
            {
                float2 uv = input.uv;
                if (_OutputAspect > _SourceAspect)
                    uv.x = (uv.x - 0.5) * (_OutputAspect / _SourceAspect) + 0.5;
                else
                    uv.y = (uv.y - 0.5) * (_SourceAspect / _OutputAspect) + 0.5;
                if (any(uv < 0) || any(uv > 1)) return fixed4(0, 0, 0, 1);
                if (_FlipY > 0.5) uv.y = 1 - uv.y;
                return fixed4(tex2D(_MainTex, uv).rgb, 1);
            }
            ENDCG
        }
    }
}
