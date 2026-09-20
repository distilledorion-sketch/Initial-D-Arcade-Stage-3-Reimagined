// Shared original color/alpha evaluation for scene color and opaque-alpha depth.
#ifndef IDAS3_SCENE_COMMON_INCLUDED
#define IDAS3_SCENE_COMMON_INCLUDED
   #include "UnityCG.cginc"
   // Shared native presentation lighting for meshes without recovered ELAN words.
   // Keep the existing Stage 3 defaults at its call site; imported courses can
   // supply their own authored direction and atmosphere to this same evaluator.
   float idasNativeDiffuse(float3 n,float3 world,float3 camera,float3 toLight){
    n*=rsqrt(max(dot(n,n),1e-10));
    if(dot(n,world-camera)>0)n=-n;
    return .32+.68*saturate(dot(n,normalize(toLight)));
   }
   float3 idasNativeAtmosphere(float3 color,float3 fogColor,float distance,float start,float range){
    float fog=saturate((distance-start)/max(range,.001));
    return lerp(color,fogColor,fog*fog);
   }
   #if defined(IDAS_IMPORTED_COURSE)
   Texture2D _ImportedShadowTex; SamplerState sampler_ImportedShadowTex;
   float _ImportedCoverage,_ImportedCutoff,_ImportedHasShadow,_ImportedSky,_ImportedNight;
   float4 _ImportedSunDirection,_ImportedFogColor,_ImportedFogRange;
   #endif
   StructuredBuffer<float4> _IdasFrameWords;
   StructuredBuffer<uint4> _IdasLightWords;
   StructuredBuffer<uint4> _IdasFogWords;
   uint _IdasView;
   float4 _IdasDepthProjection;
   uint pcw,tsp,gmp,original,emissive,billboard,courseLightRange,viewMask,_LightScope,courseCullMode;
   float glossCoefficient;
   float4 _LightOverride;
   float4 F(uint i){return _IdasFrameWords[_IdasView*23+i];}
   uint4 L(uint i){return _IdasLightWords[(_IdasView*4+_LightScope)*39+i];}
   #define viewProjection float4x4(F(0),F(1),F(2),F(3))
   #define eye F(4)
   #define atmosphere F(5)
   #define lampPosition F(6)
   #define lampDirection F(7)
   #define cameraRight F(8)
   #define cameraUp F(9)
   #define showroomLight (_LightOverride.w!=0?float4(_LightOverride.xyz,F(10).w):F(10))
   #define showroomTerms F(11)
   #define alphaReference F(11).w
   #define opponentPosition F(12)
   #define opponentDirection F(13)
   #define sceneLightColor F(22)
   #define sourceLightView float4x4(asfloat(L(0)),asfloat(L(1)),asfloat(L(2)),asfloat(L(3)))
   #define sourceLightInfo L(38)
   #define sourceFogColorDensity asfloat(_IdasFogWords[0])
   #define sourceVertexFogColorEnabled asfloat(_IdasFogWords[1])

float3 courseRgb(uint word){return float3((word>>16)&255,(word>>8)&255,word&255)/255.0;}
float3 idasNativeNightAmbient(){return float3(.22,.25,.34);}
// The same bounded point-light evaluator serves D3 fallback scenery,
// imported road surfaces, and imported-course car lighting.
float3 idasCourseLampLight(float3 world,float3 normal){
 float3 result=0;
 [unroll]for(uint i=0;i<8;++i){
  float3 toLamp=F(14+i).xyz-world;
  float d2=dot(toLamp,toLamp),invD=rsqrt(max(d2,1e-5));
  float3 direction=toLamp*invD;
  float edge=saturate(1-d2*F(14+i).w);
  float down=smoothstep(.05,.40,direction.y);
  float diffuse=.12+.88*saturate(dot(normal,direction));
  float strength=F(14+i).w>0?1.6*edge*edge*down*diffuse/(1+.018*d2):0;
  result+=float3(1,.91,.76)*strength;
 }
 return result;
}
// Decode the source ELAN words, retaining quantized directions, separate
// diffuse/specular masks, material routing and upper-floatword attenuation.
void courseColors(inout float4 base,inout float4 offset,float3 viewPosition,float3 viewNormal,float gloss){
 float3 n=normalize(viewNormal),reflection=reflect(normalize(viewPosition),n);
 float3 diffuse=0,specular=0;float diffuseAlpha=0,specularAlpha=0;
 uint masks=L(4).z,flags=L(4).y;
 [loop]for(uint i=0;i<sourceLightInfo.y;++i){
  uint4 a=L(6+i*2),b=L(7+i*2);
  uint id=a.y&15,route=(a.z>>24)&15;
  bool useDiffuse=(masks&(1u<<id))!=0,useSpecular=(masks&(1u<<(id+16)))!=0;
  if(!useDiffuse&&!useSpecular)continue;
  int3 high=int3((a.z>>16)&255,(a.z>>8)&255,a.z&255);high=(high<<24)>>24;
  int3 low=int3((a.x>>16)&15,(a.x>>4)&15,a.x&15);
  float3 incoming=-float3((high<<4)|low)/2047.0;
  float3 position=asfloat(uint3(a.w,b.x,b.y));
  bool actualParallel=(a.x&(1u<<20))!=0;
  bool parallel=actualParallel||(all(position==0)&&b.z==0&&b.w==0);
  uint diffuseMode=actualParallel?((a.z>>28)&3):((a.y>>5)&7);
  uint specularMode=actualParallel?0:((a.z>>28)&3);
  float3 color=courseRgb(a.y>>8),toLight;
  if(parallel)toLight=normalize(incoming);
  else{
   float3 delta=position-viewPosition;float distance=length(delta);toLight=normalize(delta);
   float da=asfloat((b.z&65535)<<16),db=asfloat(b.z&0xffff0000);
   float aa=asfloat((b.w&65535)<<16),ab=asfloat(b.w&0xffff0000);
   if(da!=1||db!=0){float d=(a.z&(1u<<31))!=0?distance:1/distance;color*=saturate(db*d+da);}
   if(aa!=1||ab!=0)color*=saturate((1-max(0,dot(toLight,incoming)))*ab+aa);
  }
  float factor=(route&8)!=0?-2.0:2.0;
  if(useDiffuse){
   float amount=factor;
   if(diffuseMode==0)amount*=max(dot(n,toLight),0);
   else if(diffuseMode==1)amount*=abs(dot(n,toLight));
   if((route&4)!=0)diffuseAlpha+=color.r*amount;
   else if((route&2)!=0)specular+=color*amount*base.rgb;
   else diffuse+=color*amount*base.rgb;
  }
  if(useSpecular){
   float amount=factor;
   if(specularMode==0)amount*=saturate(pow(max(dot(toLight,reflection),0),gloss));
   else if(specularMode==1)amount*=saturate(pow(abs(dot(toLight,reflection)),gloss));
   if((route&4)!=0)specularAlpha+=color.r*amount;
   else if((route&1)!=0)specular+=color*amount*offset.rgb;
   else diffuse+=color*amount*offset.rgb;
  }
 }
 diffuse+=courseRgb(L(4).w)*((flags&(1u<<5))!=0?base.rgb:float3(1,1,1));
 specular+=courseRgb(L(5).x)*((flags&(1u<<6))!=0?offset.rgb:float3(1,1,1));
 base=float4(diffuse,base.a+diffuseAlpha);offset=float4(specular,offset.a+specularAlpha);
 if((flags&(1u<<9))!=0)offset+=max(base-1,0);
 base=saturate(base);offset=saturate(offset);
}
float sourceFogCoefficient(float reciprocalDepth){
 float z=clamp(sourceFogColorDensity.w*reciprocalDepth,1.0,255.9999);
 uint bits=asuint(z),exponent=(bits>>23)-127;
 uint index=exponent*16+((bits>>19)&15);
 float fraction=float(bits&0x7ffff)/524288.0;
 uint pair=_IdasFogWords[2+(index>>2)][index&3];
 return lerp(float(pair>>8),float(pair&255),fraction)/255.0;
}
struct V{float3 p:POSITION;float3 n:NORMAL;float4 c:COLOR0;float2 uv:TEXCOORD0;float4 offsetColor:TEXCOORD1;float2 treeFace:TEXCOORD2;UNITY_VERTEX_INPUT_INSTANCE_ID};
struct P{float4 p:SV_POSITION;float3 world:TEXCOORD0;float3 n:NORMAL;float4 c:COLOR0;float2 uv:TEXCOORD1;float4 offsetColor:COLOR1;noperspective float reciprocalDepth:TEXCOORD2;float treeFace:TEXCOORD3;};
Texture2D _MainTex;SamplerState sampler_MainTex;
P mainVS(V v){UNITY_SETUP_INSTANCE_ID(v);P o;o.treeFace=v.treeFace.x;
#if defined(IDAS_IMPORTED_COURSE)
 o.world=mul(unity_ObjectToWorld,float4(v.p,1)).xyz;
 // Imported geometry shares the D3 camera's clip/depth convention with cars.
 o.p=mul(float4(o.world,1),viewProjection);o.p.y*=_ProjectionParams.x;
#if defined(UNITY_REVERSED_Z)
 o.p.z=_IdasDepthProjection.w!=0?_IdasDepthProjection.x-_IdasDepthProjection.y*o.p.w:o.p.w-o.p.z;
#endif
 o.n=mul((float3x3)unity_ObjectToWorld,v.n);o.c=v.c;o.uv=v.uv;
 o.offsetColor=v.offsetColor;o.reciprocalDepth=1/o.p.w;
 return o;
#else
 o.p=mul(float4(v.p,1),viewProjection);o.world=v.p;o.n=v.n;o.c=v.c;o.uv=v.uv;o.offsetColor=v.offsetColor;
 if(billboard!=0){float3 facing=cross(cameraRight.xyz,cameraUp.xyz);
  float localZ=v.p.z;float3 localNormal=float3(0,0,1);
  if((gmp&512)==0){
   // Lit source cards are planar. LocalZ carries their exact source normal
   // bytes, not a displaced coordinate; retain signed byte/127 semantics.
   uint packed=(uint)(v.p.z*16777216.0);
   int3 bytes=int3((int)(packed<<24),(int)(packed<<16),(int)(packed<<8))>>24;
   localNormal=float3(bytes)/127.0;localZ=0;
  }
  o.world=v.n+cameraRight.xyz*v.p.x+cameraUp.xyz*v.p.y+facing*localZ;
  o.p=mul(float4(o.world,1),viewProjection);
  o.n=cameraRight.xyz*localNormal.x+cameraUp.xyz*localNormal.y+facing*localNormal.z;}
 bool courseLit=sourceLightInfo.x!=0&&courseLightRange!=0&&showroomLight.w==0&&original!=0;
 if(courseLit){
  if((gmp&512)==0)courseColors(o.c,o.offsetColor,mul(float4(o.world,1),sourceLightView).xyz,mul(float4(o.n,0),sourceLightView).xyz,glossCoefficient);
  // ELAN adds untextured offset RGBA before pixel alpha selection.
  if((pcw&8)==0){o.c+=o.offsetColor;o.offsetColor=0;}
 }
 // Original 112260/053480 showroom GLM: one nonzero white parallel light,
 // ambient multiplied by base material, and specular routed to offset.
 // ELAN computes these colors per vertex before texturing. The b0 material
 // bit bypasses the entire light model, including ambient and highlights.
 if(showroomLight.w!=0&&original!=0&&(gmp&512)==0){
  float3 n=v.n*rsqrt(max(dot(v.n,v.n),1e-10));
  float3 toLight=normalize(showroomLight.xyz);
  float3 incident=v.p-eye.xyz;incident*=rsqrt(max(dot(incident,incident),1e-10));
  float diffuse=showroomTerms.y*max(dot(n,toLight),0);
  float specular=showroomTerms.y*pow(max(dot(toLight,reflect(incident,n)),0),glossCoefficient);
  o.c=saturate(float4(v.c.rgb*(showroomTerms.x+diffuse*sceneLightColor.rgb),v.c.a));
  o.offsetColor=saturate(float4(v.offsetColor.rgb*specular*sceneLightColor.rgb,v.offsetColor.a));
 }
 // Only imported courses supplement the car's retained D3 source light
 // array. Lamp data and the enable bit reset when leaving the course.
 if(lampDirection.w>0&&atmosphere.w>0&&_LightScope>=2&&original!=0&&emissive==0&&(gmp&512)==0){
  float3 n=o.n*rsqrt(max(dot(o.n,o.n),1e-10));
  o.c.rgb=saturate(o.c.rgb+v.c.rgb*idasCourseLampLight(o.world,n));
 }
 // ELAN's ordinary environment map adds view-normal XY/2+.5 to authored UV.
 if(original!=0&&(gmp&(1<<11))!=0){float3 n=normalize(v.n);o.uv=saturate(v.uv+float2(dot(n,cameraRight.xyz),dot(n,cameraUp.xyz))*.5+.5);}
 // Interpolate reciprocal clip depth linearly in screen space, matching the
 // PVR reciprocal-Z carrier without reapplying perspective correction.
 o.reciprocalDepth=1/o.p.w;
 // Unity flips camera targets (including the Editor Game view) when needed.
 // Source matrices remain unchanged; only the final clip-space carrier flips.
 o.p.y*=_ProjectionParams.x;
#if defined(UNITY_REVERSED_Z)
 // Source VP is forward-Z. Avoid subtracting two almost equal clip values:
 // at the opening's .01 near plane this erased separation between car panels.
 o.p.z=_IdasDepthProjection.w!=0
  ? _IdasDepthProjection.x-_IdasDepthProjection.y*o.p.w
  : o.p.w-o.p.z;
#endif
 return o;
#endif
}
// ELAN flat shading uses the final source strip vertex after lighting. D3D's
// default provoking vertex differs, so select the recovered final vertex
// explicitly for showroom and course GLM passes.
[maxvertexcount(3)]
void showroomGeometry(triangle P input[3],inout TriangleStream<P> stream){
#if defined(IDAS_IMPORTED_COURSE)
 // Paired source leaf faces occupy one plane. Keep only the camera-facing
 // side, using world space so the main camera and rear view agree.
 if(input[0].treeFace>.5){
  float3 face=cross(input[1].world-input[0].world,input[2].world-input[0].world);
  if(dot(face,eye.xyz-input[0].world)<=0)return;
 }
 for(uint k=0;k<3;++k)stream.Append(input[k]);
#else
 // Source course cards and tagged ordinary car actors retain authored face
 // orientation. Drawing hidden backs can overlap foliage or outside car trim.
 // Work before projection: this stays consistent for RH main / LH mirror
 // cameras and render textures, without relying on Unity's front-face state.
 // Modes 0/1 have no winding rejection; the source subpixel-size threshold
 // for mode 1 is intentionally not approximated here.
 if(courseCullMode>=2){
  float3 face=cross(input[1].world-input[0].world,input[2].world-input[0].world);
  float facing=dot(face,eye.xyz-input[0].world);
  // Authored ISP2 triangles use the opposite cross-product to their
  // outward normals; ISP3 reverses it. Preserve that source convention.
  if((courseCullMode==2&&facing>=0)||(courseCullMode==3&&facing<=0))return;
 }
 for(uint i=0;i<3;++i){P o=input[i];
  if(original!=0&&(pcw&2)==0){o.c=input[2].c;o.offsetColor=input[2].offsetColor;}
  stream.Append(o);
 }
#endif
}
float4 mainPS(P v):SV_TARGET{
#if defined(IDAS_IMPORTED_COURSE)
 float4 color=_MainTex.Sample(sampler_MainTex,v.uv);
 if(_ImportedCoverage!=0){
  // Derivative-scaled coverage stays approximately one pixel wide while the
  // camera moves. MSAA resolves that coverage instead of binary leaf flicker.
  color.a=saturate((color.a-_ImportedCutoff)/max(fwidth(color.a),1.0/255.0)+.5);
  clip(color.a-1.0/255.0);
 }else clip(color.a-_ImportedCutoff);
 // Stage 8 night vertices contain light contributions, often zero, rather
 // than a complete surface tint. Supply D3's existing night ambient before
 // the ordinary D3 projected-headlight pass multiplies the road beneath it.
 float3 tint=v.c.rgb;
 if(_ImportedNight!=0&&_ImportedSky==0){
  // PCT surfaces have no authored normal; recover their face normal for
  // lamp response instead of leaving the original baked road unlit.
  float3 normal=dot(v.n,v.n)>.01?normalize(v.n):normalize(cross(ddy(v.world),ddx(v.world)));
  if(dot(normal,v.world-eye.xyz)>0)normal=-normal;
  tint=max(tint,idasNativeNightAmbient())+idasCourseLampLight(v.world,normal);
 }
 color.rgb*=tint;
 // PCT meshes already contain baked lighting. Only authored normal-bearing
 // geometry uses directional lighting, avoiding double-darkened foliage.
 if(_ImportedSky==0 && _ImportedNight==0 && dot(v.n,v.n)>.01)
  color.rgb*=idasNativeDiffuse(v.n,v.world,_WorldSpaceCameraPos,_ImportedSunDirection.xyz);
 float4 shadow=_ImportedShadowTex.Sample(sampler_ImportedShadowTex,v.offsetColor.xy);
 color.rgb*=lerp(1,.32+.68*shadow.rgb,shadow.a*_ImportedHasShadow);
 if(_ImportedSky==0){
  // Night source Mie coefficients are not RGB fog (several are white/0.4).
  // Reuse D3's native night atmosphere instead of turning the horizon white.
  color.rgb=_ImportedNight!=0
   ?idasNativeAtmosphere(color.rgb,atmosphere.rgb,length(v.world-eye.xyz),85,410)
   :idasNativeAtmosphere(color.rgb,_ImportedFogColor.rgb,length(v.world-eye.xyz),_ImportedFogRange.x,_ImportedFogRange.y);
 }
 return color;
#else
 if((viewMask&(1u<<_IdasView))==0)discard;
 float3 normal=v.n*rsqrt(max(dot(v.n,v.n),1e-10));
 // Face the fallback lighting normal toward the viewer for remaining
 // two-sided development geometry. Original course lighting is evaluated
 // per vertex above with its authored normal.
 if(dot(normal,v.world-eye.xyz)>0)normal=-normal;
 float light=idasNativeDiffuse(normal,v.world,eye.xyz,float3(-.35,.85,.4));
 float4 color=v.c;float4 offsetColor=v.offsetColor;
 if(original==0){
  float4 texel=_MainTex.Sample(sampler_MainTex,v.uv);clip(texel.a*v.c.a-.05);
  color*=texel;color.rgb*=light;
 }else{
  // Source course and showroom colors were already lit per vertex.
  if(showroomLight.w==0&&!(sourceLightInfo.x!=0&&courseLightRange!=0)&&(gmp&512)==0&&emissive==0){color.rgb*=light;offsetColor=0;}
  if((tsp&(1<<20))==0)color.a=1;
  // PVR fog mode3 replaces the lit base before texture shading.
  if(sourceVertexFogColorEnabled.w!=0&&((tsp>>22)&3)==3){
   color=float4(sourceFogColorDensity.rgb,sourceFogCoefficient(v.reciprocalDepth));
  }
  if((pcw&8)!=0){
   float4 texel=_MainTex.Sample(sampler_MainTex,v.uv);
   if((tsp&(1<<19))!=0)texel.a=1;
   uint mode=(tsp>>6)&3;
   if(mode==0)color=texel;
   else if(mode==1)color=float4(color.rgb*texel.rgb,texel.a);
   else if(mode==2)color.rgb=lerp(color.rgb,texel.rgb,texel.a);
   else color*=texel;
   if((pcw&4)!=0)color.rgb+=offsetColor.rgb;
  }else color+=offsetColor;
  if(((pcw>>24)&7)==4){clip(floor(saturate(color.a)*255+.5)-alphaReference);color.a=1;}
  color=saturate(color);
 }
 // Original races use separate projected headlight geometry. Native scene
 // illumination remains for development/attract geometry and car ambient.
 if(showroomLight.w==0&&!(sourceLightInfo.x!=0&&courseLightRange!=0)&&atmosphere.w>0&&emissive==0){
  float3 delta=v.world-lampPosition.xyz;float ahead=dot(delta,lampDirection.xyz);
  float lateral=dot(delta,float3(lampDirection.z,0,-lampDirection.x));
  float beam=(1-smoothstep(.55,1,abs(lateral)/(2+max(ahead,0)*.13)))*smoothstep(0,3,ahead)*(1-smoothstep(70,155,ahead));
  float vertical=1-smoothstep(2.0,11.0,abs(delta.y+ahead*.018));
  float illumination=beam*vertical*lampPosition.w;
  if(opponentPosition.w>0){
   delta=v.world-opponentPosition.xyz;ahead=dot(delta,opponentDirection.xyz);
   lateral=dot(delta,float3(opponentDirection.z,0,-opponentDirection.x));
   beam=(1-smoothstep(.55,1,abs(lateral)/(2+max(ahead,0)*.13)))*smoothstep(0,3,ahead)*(1-smoothstep(70,155,ahead));
   vertical=1-smoothstep(2.0,11.0,abs(delta.y+ahead*.018));
   illumination=max(illumination,beam*vertical);
  }
  float3 streetLight=idasCourseLampLight(v.world,normal);
  color.rgb*=min(idasNativeNightAmbient()+illumination*float3(.94,.86,.70)+streetLight,float3(1.30,1.24,1.16));
 }
 if(sourceVertexFogColorEnabled.w!=0&&original!=0){
  uint fogMode=(tsp>>22)&3;
  if(fogMode==0)color.rgb=lerp(color.rgb,sourceFogColorDensity.rgb,sourceFogCoefficient(v.reciprocalDepth));
  else if(fogMode==1&&(pcw&4)!=0)color.rgb=lerp(color.rgb,sourceVertexFogColorEnabled.rgb,v.offsetColor.a);
  return color;
 }
 // Development geometry and attract owners without a recovered fog setup
 // retain their native atmosphere; this is not used for source race meshes.
 if(showroomLight.w!=0 || (original!=0 && ((tsp>>22)&3)==2))return color;
 return float4(idasNativeAtmosphere(color.rgb,atmosphere.rgb,length(v.world-eye.xyz),85,410),color.a);
#endif
}

#endif
