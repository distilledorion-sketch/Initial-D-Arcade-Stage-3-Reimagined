// Shared by in-race and IMGUI rendering. Atlas coordinates are converted back
// to layer coordinates before applying source-layout clipping and gauge masks.
float _GaugeMode,_ClipEnabled;
float4 _Gauge,_Atlas,_SourceSize,_ClipRect,_LocalToMeter0,_LocalToMeter1;
sampler2D _MaskTex;float _MaskEnabled;float4 _MaskTransform0,_MaskTransform1,_Radial;
float4 _SampleMotion;
float _MaterialEffect;
sampler2D _EffectTex1,_EffectTex2,_EffectTex3;
float4 _EffectParams,_EffectParams2,_EffectColor1,_EffectColor2,_EffectColor3;
float MeterGlowLobe(float radius,float density,float distanceFromCenter){
 return radius>0&&density>0?pow(saturate(1-distanceFromCenter/max(radius,.0001)),max(.1,density)*3):0;
}
// Cooked exports retain material families, textures and parameters, but not
// executable shader graphs. These adapters reconstruct their visual function.
float4 ImportedMeterEffect(float2 uv,float4 sampled,float4 tint){
 float2 localUv=(uv-_Atlas.xy)/max(_Atlas.zw,float2(.0001,.0001));
 if(_MaterialEffect>.5&&_MaterialEffect<1.5){
  float2 d=localUv-.5;float r=length(d);
  float glow=MeterGlowLobe(_EffectParams.x,_EffectParams2.x,r)+
   MeterGlowLobe(_EffectParams.y,_EffectParams2.y,r)+MeterGlowLobe(_EffectParams.z,_EffectParams2.z,r);
  float diamond=pow(saturate(1-(abs(d.x)+abs(d.y))*2),max(.1,_EffectParams.w));
  return float4(tint.rgb,tint.a*saturate(glow*.65+diamond*.35)*_EffectParams2.w);
 }
 if(_MaterialEffect>1.5&&_MaterialEffect<2.5){
  float time=_EffectParams.x,angle=sin(time*.73)*_EffectParams.y;
  float2 d=localUv-.5;
  float2 spun=float2(cos(angle)*d.x-sin(angle)*d.y,sin(angle)*d.x+cos(angle)*d.y)+.5;
  float n1=tex2D(_EffectTex2,frac(spun*1.9+float2(time*.09,-time*.22))).r;
  float n2=tex2D(_EffectTex3,frac(localUv*2.4+float2(-time*.13,-time*.34))).r;
  float silhouette=max(sampled.r,tex2D(_EffectTex1,localUv).r*.75);
  float turbulence=saturate(n1*.8+n2*.55);
  float flame=smoothstep(.12,.85,turbulence+silhouette*.24);
  float heat=saturate(turbulence+sin(time*1.7)*_EffectParams.z);
  float3 color=lerp(_EffectColor1.rgb,_EffectColor2.rgb,saturate(heat*2));
  color=lerp(color,_EffectColor3.rgb,smoothstep(.48,1,heat));
  return float4(color*tint.rgb,tint.a*silhouette*(.22+.78*flame));
 }
 if(_MaterialEffect>2.5&&_MaterialEffect<3.5){
  float2 d=localUv-.5;float radius=length(d);
  float phase=frac(atan2(d.y,d.x)/6.2831853+.25),band=floor(phase*32);
  float level=tex2D(_EffectTex1,float2((band+.5)/32,.5)).r;
  float inner=.235,outer=inner+level*.235,aa=max(fwidth(radius),.0015);
  float bars=smoothstep(inner-aa,inner+aa,radius)*(1-smoothstep(outer-aa,outer+aa,radius));
  float gap=smoothstep(.06,.17,frac(phase*32))*(1-smoothstep(.83,.94,frac(phase*32)));
  float ring=1-smoothstep(.007,.014,abs(radius-inner));
  float noise=.8+.2*tex2D(_EffectTex2,localUv).r;
  float3 color=lerp(_EffectColor1.rgb,_EffectColor2.rgb,saturate((radius-inner)/.235));
  return float4(color*tint.rgb,tint.a*level*saturate(bars*gap+ring*.65)*noise);
 }
 if(_MaterialEffect>3.5&&_MaterialEffect<4.5){
  // The authored dot grid and aperture are opaque RGB masks, not alpha images.
  sampled.a*=tex2D(_EffectTex1,localUv).r*tex2D(_EffectTex2,localUv).r;
 }
 return sampled;
}
float2 ImportedMeterSampleUv(float2 uv,out float visible){
 float angle=_SampleMotion.z*6.2831853,c=cos(angle),s=sin(angle);
 float2 samplePoint=uv-.5;
 samplePoint=float2(c*samplePoint.x-s*samplePoint.y,s*samplePoint.x+c*samplePoint.y)+.5+_SampleMotion.xy;
 visible=_SampleMotion.w>.5?1:step(0,samplePoint.x)*step(0,samplePoint.y)*step(samplePoint.x,1)*step(samplePoint.y,1);
 return _SampleMotion.w>.5?frac(samplePoint):samplePoint;
}
float ImportedMeterAlpha(float2 uv){
 float2 localUv=(uv-_Atlas.xy)/max(_Atlas.zw,float2(.0001,.0001));float alpha=1;
 float3 localPoint=float3(localUv.x*_SourceSize.x,(1-localUv.y)*_SourceSize.y,1);
 if(_GaugeMode>.5&&_GaugeMode<1.5){
  float2 d=localUv-.5;float phase=frac((_Gauge.x-(atan2(d.y,d.x)/6.2831853-.25))*_Gauge.w);
  float arc=phase/max(_Gauge.y,.0001);float aa=max(fwidth(arc),.002);
  alpha=step(.0001,_Gauge.z)*(1-smoothstep(_Gauge.z-aa,_Gauge.z+aa,arc));
 }else if(_GaugeMode>1.5){float aa=max(fwidth(localUv.x),.002);alpha=step(.0001,_Gauge.z)*(1-smoothstep(_Gauge.z-aa,_Gauge.z+aa,localUv.x));}
 if(_ClipEnabled>.5){
  float2 meter=float2(dot(_LocalToMeter0.xyz,localPoint),dot(_LocalToMeter1.xyz,localPoint));
  alpha*=step(_ClipRect.x,meter.x)*step(_ClipRect.y,meter.y)*step(meter.x,_ClipRect.z)*step(meter.y,_ClipRect.w);
 }
 if(_MaskEnabled>.5){
  float2 maskUv=float2(dot(_MaskTransform0.xyz,localPoint),dot(_MaskTransform1.xyz,localPoint));
  alpha*=step(0,maskUv.x)*step(0,maskUv.y)*step(maskUv.x,1)*step(maskUv.y,1)*tex2D(_MaskTex,float2(maskUv.x,1-maskUv.y)).a;
 }
 if(_Radial.x>0){
  // Reconstruct the stripped source radial gradient from its radius/density.
  float radius=length(localUv-.5);float aa=max(fwidth(radius),.001);
  alpha*=pow(saturate(radius/_Radial.x),max(1,_Radial.y))*(1-smoothstep(_Radial.x-aa,_Radial.x+aa,radius))*_Radial.z;
 }
 return alpha;
}
