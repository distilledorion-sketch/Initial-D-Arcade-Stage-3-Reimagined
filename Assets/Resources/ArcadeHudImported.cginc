// Shared by in-race and IMGUI rendering. Atlas coordinates are converted back
// to layer coordinates before applying source-layout clipping and gauge masks.
float _GaugeMode,_ClipEnabled;
float4 _Gauge,_Atlas,_SourceSize,_ClipRect,_LocalToMeter0,_LocalToMeter1;
sampler2D _MaskTex;float _MaskEnabled;float4 _MaskTransform0,_MaskTransform1,_Radial;
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
