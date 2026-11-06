// GLSL ES 3.00 ports of the Unity shaders that draw a native scene frame.
//
//   SCENE_*  <- Assets/Resources/Idas3SceneCommon.cginc (original, non-imported path), which
//               evaluates the source PowerVR/ELAN colour, lighting and fog model.
//   UI_*     <- Assets/Resources/Idas3Ui.shader (draw-listed menu / HUD sprites).
//   BLIT_*   <- Assets/Resources/Idas3Display.shader (fit the render target to the canvas).
//
// Conventions carried over from the D3D/HLSL originals:
//  * Matrices arrive row-major for row vectors. HLSL `mul(v, M)` with M = float4x4(r0,r1,r2,r3)
//    equals GLSL `mat4(r0,r1,r2,r3) * v`, because GLSL's constructor fills columns.
//  * Source clip depth is D3D-style 0..w. The vertex shader replaces it with reversed-Z (near=1)
//    like Unity's D3D11 path, so the renderer uses a 32-bit float depth buffer, GREATER-style
//    compares, and a depth clear of 0.
//  * The source 1/w carrier (HLSL `noperspective reciprocalDepth`) is exactly gl_FragCoord.w.
//  * The geometry shader's "flat colours from the last vertex" is GLSL `flat` (GL's default
//    provoking vertex is the last one). Its triangle rejection is done with GL cull state.

export const SCENE_VS = `#version 300 es
precision highp float;
precision highp int;

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec2 aUv;
layout(location = 4) in vec4 aOffset;

layout(std140) uniform FrameBlock { vec4 uF[46]; };   // 2 views x 23 float4
layout(std140) uniform LightBlock { uvec4 uL[312]; }; // 2 views x 4 scopes x 39 uint4
layout(std140) uniform FogBlock { uvec4 uG[34]; };    // 136 words

uniform int uView;
uniform uint uPcw, uTsp, uGmp;
uniform int uOriginal, uEmissive, uBillboard, uLightScope, uCourseLightRange, uNativeSource;
uniform vec4 uLightOverride;   // xyz: showroom light direction override, w: enabled
uniform float uGloss;
uniform vec2 uDepth;           // reversed-Z coefficients A, B for the current camera

out vec3 vWorld;
out vec3 vNormal;
out vec4 vColor;
out vec2 vUv;
out vec4 vOffset;
flat out vec4 fColor;
flat out vec4 fOffset;

vec4 F(int i) { return uF[uView * 23 + i]; }
uvec4 L(int i) { return uL[(uView * 4 + uLightScope) * 39 + i]; }

vec3 courseRgb(uint word) { return vec3(float((word >> 16) & 255u), float((word >> 8) & 255u), float(word & 255u)) / 255.0; }
vec3 idasNativeNightAmbient() { return vec3(.22, .25, .34); }

// Bounded point-light evaluator shared by fallback scenery and imported-course car lighting.
vec3 idasPointLampLight(vec4 lamp, vec3 world, vec3 normal) {
  vec3 toLamp = lamp.xyz - world;
  float d2 = dot(toLamp, toLamp), invD = inversesqrt(max(d2, 1e-5));
  vec3 direction = toLamp * invD;
  float edge = clamp(1.0 - d2 * lamp.w, 0.0, 1.0);
  float down = smoothstep(.05, .40, direction.y);
  float diffuse = .12 + .88 * clamp(dot(normal, direction), 0.0, 1.0);
  float strength = lamp.w > 0.0 ? 1.6 * edge * edge * down * diffuse / (1.0 + .018 * d2) : 0.0;
  return vec3(1.0, .91, .76) * strength;
}
vec3 idasCourseLampLight(vec3 world, vec3 normal) {
  vec3 result = vec3(0.0);
  for (int i = 0; i < 8; ++i) result += idasPointLampLight(F(14 + i), world, normal);
  return result;
}

// Decode the source ELAN light words: quantized directions, separate diffuse/specular masks,
// material routing and attenuation.
void courseColors(inout vec4 base, inout vec4 offset, vec3 viewPosition, vec3 viewNormal, float gloss) {
  vec3 n = normalize(viewNormal), reflection = reflect(normalize(viewPosition), n);
  vec3 diffuse = vec3(0.0), specular = vec3(0.0);
  float diffuseAlpha = 0.0, specularAlpha = 0.0;
  uint masks = L(4).z, flags = L(4).y;
  uint count = L(38).y;
  for (uint i = 0u; i < count; ++i) {
    uvec4 a = L(6 + int(i) * 2), b = L(7 + int(i) * 2);
    uint id = a.y & 15u, route = (a.z >> 24) & 15u;
    bool useDiffuse = (masks & (1u << id)) != 0u, useSpecular = (masks & (1u << (id + 16u))) != 0u;
    if (!useDiffuse && !useSpecular) continue;
    // 8-bit signed fields, sign-extended by hand.
    int h0 = int((a.z >> 16) & 255u), h1 = int((a.z >> 8) & 255u), h2 = int(a.z & 255u);
    if (h0 > 127) h0 -= 256;
    if (h1 > 127) h1 -= 256;
    if (h2 > 127) h2 -= 256;
    ivec3 high = ivec3(h0, h1, h2);
    ivec3 low = ivec3(int((a.x >> 16) & 15u), int((a.x >> 4) & 15u), int(a.x & 15u));
    vec3 incoming = -vec3((high * 16) | low) / 2047.0;
    vec3 position = uintBitsToFloat(uvec3(a.w, b.x, b.y));
    bool actualParallel = (a.x & (1u << 20)) != 0u;
    bool parallel = actualParallel || (position == vec3(0.0) && b.z == 0u && b.w == 0u);
    uint diffuseMode = actualParallel ? ((a.z >> 28) & 3u) : ((a.y >> 5) & 7u);
    uint specularMode = actualParallel ? 0u : ((a.z >> 28) & 3u);
    vec3 color = courseRgb(a.y >> 8), toLight;
    if (parallel) toLight = normalize(incoming);
    else {
      vec3 delta = position - viewPosition;
      float distance = length(delta);
      toLight = normalize(delta);
      float da = uintBitsToFloat((b.z & 65535u) << 16), db = uintBitsToFloat(b.z & 0xffff0000u);
      float aa = uintBitsToFloat((b.w & 65535u) << 16), ab = uintBitsToFloat(b.w & 0xffff0000u);
      if (da != 1.0 || db != 0.0) {
        float d = (a.z & (1u << 31)) != 0u ? distance : 1.0 / distance;
        color *= clamp(db * d + da, 0.0, 1.0);
      }
      if (aa != 1.0 || ab != 0.0) color *= clamp((1.0 - max(0.0, dot(toLight, incoming))) * ab + aa, 0.0, 1.0);
    }
    float factor = (route & 8u) != 0u ? -2.0 : 2.0;
    if (useDiffuse) {
      float amount = factor;
      if (diffuseMode == 0u) amount *= max(dot(n, toLight), 0.0);
      else if (diffuseMode == 1u) amount *= abs(dot(n, toLight));
      if ((route & 4u) != 0u) diffuseAlpha += color.r * amount;
      else if ((route & 2u) != 0u) specular += color * amount * base.rgb;
      else diffuse += color * amount * base.rgb;
    }
    if (useSpecular) {
      float amount = factor;
      if (specularMode == 0u) amount *= clamp(pow(max(dot(toLight, reflection), 0.0), gloss), 0.0, 1.0);
      else if (specularMode == 1u) amount *= clamp(pow(abs(dot(toLight, reflection)), gloss), 0.0, 1.0);
      if ((route & 4u) != 0u) specularAlpha += color.r * amount;
      else if ((route & 1u) != 0u) specular += color * amount * offset.rgb;
      else diffuse += color * amount * offset.rgb;
    }
  }
  diffuse += courseRgb(L(4).w) * ((flags & (1u << 5)) != 0u ? base.rgb : vec3(1.0));
  specular += courseRgb(L(5).x) * ((flags & (1u << 6)) != 0u ? offset.rgb : vec3(1.0));
  base = vec4(diffuse, base.a + diffuseAlpha);
  offset = vec4(specular, offset.a + specularAlpha);
  if ((flags & (1u << 9)) != 0u) offset += max(base - 1.0, 0.0);
  base = clamp(base, 0.0, 1.0);
  offset = clamp(offset, 0.0, 1.0);
}

void main() {
  vec4 eye = F(4), atmosphere = F(5), lampDirection = F(7), cameraRight = F(8), cameraUp = F(9);
  mat4 viewProjection = mat4(F(0), F(1), F(2), F(3));
  mat4 sourceLightView = mat4(uintBitsToFloat(L(0)), uintBitsToFloat(L(1)), uintBitsToFloat(L(2)), uintBitsToFloat(L(3)));
  bool native = uNativeSource != 0;
  bool original = native || uOriginal != 0;
  bool courseLightRange = native || uCourseLightRange != 0;
  bool sourceLight = native || L(38).x != 0u;
  vec4 showroomLight = native ? vec4(0.0) : (uLightOverride.w != 0.0 ? vec4(uLightOverride.xyz, F(10).w) : F(10));
  vec4 showroomTerms = F(11);

  vec3 world = aPos;
  vec3 normal = aNormal;
  vec4 clip = viewProjection * vec4(aPos, 1.0);
  vec4 color = aColor, offsetColor = aOffset;
  vec2 uv = aUv;

  if (uBillboard != 0) {
    vec3 facing = cross(cameraRight.xyz, cameraUp.xyz);
    float localZ = aPos.z;
    vec3 localNormal = vec3(0.0, 0.0, 1.0);
    if ((uGmp & 512u) == 0u) {
      // Lit source cards are planar. LocalZ carries their exact source normal bytes (signed
      // byte / 127), not a displaced coordinate.
      uint packedBytes = uint(aPos.z * 16777216.0);
      int b0 = int(packedBytes & 255u), b1 = int((packedBytes >> 8) & 255u), b2 = int((packedBytes >> 16) & 255u);
      if (b0 > 127) b0 -= 256;
      if (b1 > 127) b1 -= 256;
      if (b2 > 127) b2 -= 256;
      localNormal = vec3(float(b0), float(b1), float(b2)) / 127.0;
      localZ = 0.0;
    }
    world = aNormal + cameraRight.xyz * aPos.x + cameraUp.xyz * aPos.y + facing * localZ;
    clip = viewProjection * vec4(world, 1.0);
    normal = cameraRight.xyz * localNormal.x + cameraUp.xyz * localNormal.y + facing * localNormal.z;
  }

  bool courseLit = sourceLight && courseLightRange && showroomLight.w == 0.0 && original;
  if (courseLit) {
    if ((uGmp & 512u) == 0u)
      courseColors(color, offsetColor, (sourceLightView * vec4(world, 1.0)).xyz, (sourceLightView * vec4(normal, 0.0)).xyz, uGloss);
    // ELAN adds untextured offset RGBA before pixel alpha selection.
    if ((uPcw & 8u) == 0u) { color += offsetColor; offsetColor = vec4(0.0); }
  }

  // Original 112260/053480 showroom GLM: one white parallel light, ambient multiplied by base
  // material, specular routed to offset. The b0 material bit bypasses the light model.
  if (showroomLight.w != 0.0 && original && (uGmp & 512u) == 0u) {
    vec3 n = aNormal * inversesqrt(max(dot(aNormal, aNormal), 1e-10));
    vec3 toLight = normalize(showroomLight.xyz);
    vec3 incident = aPos - eye.xyz;
    incident *= inversesqrt(max(dot(incident, incident), 1e-10));
    float diffuse = showroomTerms.y * max(dot(n, toLight), 0.0);
    float specular = showroomTerms.y * pow(max(dot(toLight, reflect(incident, n)), 0.0), uGloss);
    vec3 sceneLightColor = F(22).rgb;
    color = clamp(vec4(aColor.rgb * (showroomTerms.x + diffuse * sceneLightColor), aColor.a), 0.0, 1.0);
    offsetColor = clamp(vec4(aOffset.rgb * specular * sceneLightColor, aOffset.a), 0.0, 1.0);
  }

  // Imported courses supplement a car's retained source light array with street lamps.
  if (lampDirection.w > 0.0 && atmosphere.w > 0.0 && uLightScope >= 2 && original && uEmissive == 0 && (uGmp & 512u) == 0u) {
    vec3 n = normal * inversesqrt(max(dot(normal, normal), 1e-10));
    color.rgb = clamp(color.rgb + aColor.rgb * idasCourseLampLight(world, n), 0.0, 1.0);
  }

  // ELAN's environment map adds view-normal XY/2+.5 to the authored UV.
  if (original && (uGmp & (1u << 11)) != 0u) {
    vec3 n = normalize(aNormal);
    uv = clamp(aUv + vec2(dot(n, cameraRight.xyz), dot(n, cameraUp.xyz)) * .5 + .5, 0.0, 1.0);
  }

  vWorld = world;
  vNormal = normal;
  vColor = color;
  vUv = uv;
  vOffset = offsetColor;
  fColor = color;
  fOffset = offsetColor;
  // Reversed-Z, formed from clip W exactly as the Unity shader does (A - B*w, with the
  // coefficients computed in double precision per camera). Subtracting forward clip Z from W
  // would lose the tiny near-plane term. GL window depth = (z/w + 1)/2, so z = (2r - 1) * w
  // where r*w = A - B*w is the reversed depth times w.
  gl_Position = vec4(clip.xy, 2.0 * (uDepth.x - uDepth.y * clip.w) - clip.w, clip.w);
}
`;

export const SCENE_FS = `#version 300 es
precision highp float;
precision highp int;

in vec3 vWorld;
in vec3 vNormal;
in vec4 vColor;
in vec2 vUv;
in vec4 vOffset;
flat in vec4 fColor;
flat in vec4 fOffset;
out vec4 outColor;

layout(std140) uniform FrameBlock { vec4 uF[46]; };
layout(std140) uniform LightBlock { uvec4 uL[312]; };
layout(std140) uniform FogBlock { uvec4 uG[34]; };

uniform sampler2D uTex;
uniform int uView;
uniform uint uPcw, uTsp, uGmp, uViewMask;
uniform int uOriginal, uEmissive, uLightScope, uCourseLightRange, uNativeSource, uFlatShade;
uniform vec4 uLightOverride;
uniform float uTrackSurface, uTrackLighting;

vec4 F(int i) { return uF[uView * 23 + i]; }
uvec4 L(int i) { return uL[(uView * 4 + uLightScope) * 39 + i]; }

vec3 idasNativeNightAmbient() { return vec3(.22, .25, .34); }
vec3 idasPointLampLight(vec4 lamp, vec3 world, vec3 normal) {
  vec3 toLamp = lamp.xyz - world;
  float d2 = dot(toLamp, toLamp), invD = inversesqrt(max(d2, 1e-5));
  vec3 direction = toLamp * invD;
  float edge = clamp(1.0 - d2 * lamp.w, 0.0, 1.0);
  float down = smoothstep(.05, .40, direction.y);
  float diffuse = .12 + .88 * clamp(dot(normal, direction), 0.0, 1.0);
  float strength = lamp.w > 0.0 ? 1.6 * edge * edge * down * diffuse / (1.0 + .018 * d2) : 0.0;
  return vec3(1.0, .91, .76) * strength;
}
vec3 idasCourseLampLight(vec3 world, vec3 normal) {
  vec3 result = vec3(0.0);
  for (int i = 0; i < 8; ++i) result += idasPointLampLight(F(14 + i), world, normal);
  return result;
}
float idasNativeDiffuse(vec3 n, vec3 world, vec3 camera, vec3 toLight) {
  n *= inversesqrt(max(dot(n, n), 1e-10));
  if (dot(n, world - camera) > 0.0) n = -n;
  return .32 + .68 * clamp(dot(n, normalize(toLight)), 0.0, 1.0);
}
vec3 idasNativeAtmosphere(vec3 color, vec3 fogColor, float distance, float start, float range) {
  float fog = clamp((distance - start) / max(range, .001), 0.0, 1.0);
  return mix(color, fogColor, fog * fog);
}
// A small hue-preserving exposure shoulder for shaded scenery; applied before fog, never to
// skies or multiplicative light/shadow passes.
vec3 idasTrackPresentation(vec3 color, float night) {
  float gain = mix(.22, .14, clamp(night, 0.0, 1.0)) * clamp(uTrackLighting, 0.0, 1.0);
  float peak = max(color.r, max(color.g, color.b));
  return color * (1.0 + gain) / (1.0 + gain * clamp(peak, 0.0, 1.0));
}
float sourceFogCoefficient(float reciprocalDepth, float density) {
  float z = clamp(density * reciprocalDepth, 1.0, 255.9999);
  uint bits = floatBitsToUint(z);
  uint exponent = (bits >> 23) - 127u;
  uint index = exponent * 16u + ((bits >> 19) & 15u);
  float fraction = float(bits & 0x7ffffu) / 524288.0;
  uint pair = uG[2 + int(index >> 2)][int(index & 3u)];
  return mix(float(pair >> 8), float(pair & 255u), fraction) / 255.0;
}

void main() {
  bool native = uNativeSource != 0;
  bool original = native || uOriginal != 0;
  bool courseLightRange = native || uCourseLightRange != 0;
  vec4 eye = F(4), atmosphere = F(5), lampPosition = F(6), lampDirection = F(7);
  vec4 opponentPosition = F(12), opponentDirection = F(13);
  vec4 showroomLight = native ? vec4(0.0) : (uLightOverride.w != 0.0 ? vec4(uLightOverride.xyz, F(10).w) : F(10));
  float alphaReference = F(11).w;
  bool sourceLight = native || L(38).x != 0u;
  vec4 fogColorDensity = uintBitsToFloat(uvec4(uG[0]));
  vec4 vertexFog = uintBitsToFloat(uvec4(uG[1]));

  // The no-clip course variant skips view masking and the punch-through alpha test.
  if (uNativeSource != 2 && (uViewMask & (1u << uint(uView))) == 0u) discard;

  vec3 normal = vNormal * inversesqrt(max(dot(vNormal, vNormal), 1e-10));
  // Face the fallback lighting normal toward the viewer for remaining two-sided geometry.
  if (dot(normal, vWorld - eye.xyz) > 0.0) normal = -normal;
  float light = idasNativeDiffuse(normal, vWorld, eye.xyz, vec3(-.35, .85, .4));

  bool flatColors = uFlatShade != 0;
  vec4 color = flatColors ? fColor : vColor;
  vec4 offsetColor = flatColors ? fOffset : vOffset;

  if (!original) {
    vec4 texel = texture(uTex, vUv).bgra;
    if (texel.a * vColor.a - .05 < 0.0) discard;
    color *= texel;
    color.rgb *= light;
  } else {
    // Source course and showroom colours were already lit per vertex.
    if (showroomLight.w == 0.0 && !(sourceLight && courseLightRange) && (uGmp & 512u) == 0u && uEmissive == 0) {
      color.rgb *= light;
      offsetColor = vec4(0.0);
    }
    if ((uTsp & (1u << 20)) == 0u) color.a = 1.0;
    // PVR fog mode 3 replaces the lit base before texture shading.
    if (vertexFog.w != 0.0 && ((uTsp >> 22) & 3u) == 3u)
      color = vec4(fogColorDensity.rgb, sourceFogCoefficient(gl_FragCoord.w, fogColorDensity.w));
    if ((uPcw & 8u) != 0u) {
      vec4 texel = texture(uTex, vUv).bgra;
      if ((uTsp & (1u << 19)) != 0u) texel.a = 1.0;
      uint mode = (uTsp >> 6) & 3u;
      if (mode == 0u) color = texel;
      else if (mode == 1u) color = vec4(color.rgb * texel.rgb, texel.a);
      else if (mode == 2u) color.rgb = mix(color.rgb, texel.rgb, texel.a);
      else color *= texel;
      if ((uPcw & 4u) != 0u) color.rgb += offsetColor.rgb;
    } else color += offsetColor;
    if (uNativeSource != 2 && ((uPcw >> 24) & 7u) == 4u) {
      if (floor(clamp(color.a, 0.0, 1.0) * 255.0 + .5) - alphaReference < 0.0) discard;
      color.a = 1.0;
    }
    color = clamp(color, 0.0, 1.0);
  }

  // Original races use separate projected headlight geometry; native illumination remains for
  // development/attract geometry and car ambient.
  if (showroomLight.w == 0.0 && !(sourceLight && courseLightRange) && atmosphere.w > 0.0 && uEmissive == 0) {
    vec3 delta = vWorld - lampPosition.xyz;
    float ahead = dot(delta, lampDirection.xyz);
    float lateral = dot(delta, vec3(lampDirection.z, 0.0, -lampDirection.x));
    float beam = (1.0 - smoothstep(.55, 1.0, abs(lateral) / (2.0 + max(ahead, 0.0) * .13))) * smoothstep(0.0, 3.0, ahead) * (1.0 - smoothstep(70.0, 155.0, ahead));
    float vertical = 1.0 - smoothstep(2.0, 11.0, abs(delta.y + ahead * .018));
    float illumination = beam * vertical * lampPosition.w;
    if (opponentPosition.w > 0.0) {
      delta = vWorld - opponentPosition.xyz;
      ahead = dot(delta, opponentDirection.xyz);
      lateral = dot(delta, vec3(opponentDirection.z, 0.0, -opponentDirection.x));
      beam = (1.0 - smoothstep(.55, 1.0, abs(lateral) / (2.0 + max(ahead, 0.0) * .13))) * smoothstep(0.0, 3.0, ahead) * (1.0 - smoothstep(70.0, 155.0, ahead));
      vertical = 1.0 - smoothstep(2.0, 11.0, abs(delta.y + ahead * .018));
      illumination = max(illumination, beam * vertical);
    }
    vec3 streetLight = idasCourseLampLight(vWorld, normal);
    color.rgb *= min(idasNativeNightAmbient() + illumination * vec3(.94, .86, .70) + streetLight, vec3(1.30, 1.24, 1.16));
  }
  if (uTrackSurface != 0.0 && showroomLight.w == 0.0) color.rgb = idasTrackPresentation(color.rgb, atmosphere.w);
  if (vertexFog.w != 0.0 && original) {
    uint fogMode = (uTsp >> 22) & 3u;
    if (fogMode == 0u) color.rgb = mix(color.rgb, fogColorDensity.rgb, sourceFogCoefficient(gl_FragCoord.w, fogColorDensity.w));
    else if (fogMode == 1u && (uPcw & 4u) != 0u) color.rgb = mix(color.rgb, vertexFog.rgb, vOffset.a);
    outColor = color;
    return;
  }
  // Development geometry and attract owners without a recovered fog setup keep their native
  // atmosphere; this is not used for source race meshes.
  if (showroomLight.w != 0.0 || (original && ((uTsp >> 22) & 3u) == 2u)) { outColor = color; return; }
  outColor = vec4(idasNativeAtmosphere(color.rgb, atmosphere.rgb, length(vWorld - eye.xyz), 85.0, 410.0), color.a);
}
`;

// ---- UI sprites (pixel-space triangle lists) -----------------------------------------------
export const UI_VS = `#version 300 es
precision highp float;
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec4 aColor;   // 4 x UNSIGNED_BYTE normalised, memory order B,G,R,A of 0xAARRGGBB
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec4 aOffset;  // same packing
uniform vec2 uCanvas;
out vec4 vColor;
out vec4 vOffset;
out vec2 vUv;
out vec2 vPixel;
void main() {
  gl_Position = vec4(2.0 * aPos.x / uCanvas.x - 1.0, 1.0 - 2.0 * aPos.y / uCanvas.y, 0.0, 1.0);
  vColor = aColor.bgra; vOffset = aOffset.bgra; vUv = aUv; vPixel = aPos;
}
`;

export const UI_FS = `#version 300 es
precision highp float;
precision highp int;
in vec4 vColor;
in vec4 vOffset;
in vec2 vUv;
in vec2 vPixel;
out vec4 outColor;
uniform sampler2D uTex;
uniform vec4 uClip;        // left, top, right, bottom in canvas pixels
uniform vec4 uTint;
uniform float uOpacity;
uniform uint uTsp, uPcw;
uniform int uOriginal, uSourceIsOne;

// Point sampling with the source wrap rules, by integer texel fetch.
int coord(float uv, int size, bool clampUv, bool mirrorUv) {
  if (clampUv) return clamp(int(floor(uv * float(size))), 0, size - 1);
  float base = floor(uv), part = uv - base;
  if (mirrorUv && (int(base) & 1) != 0) part = 1.0 - part;
  return clamp(int(floor(part * float(size))), 0, size - 1);
}
void main() {
  if (vPixel.x < uClip.x || vPixel.y < uClip.y || vPixel.x > uClip.z || vPixel.y > uClip.w) discard;
  ivec2 size = textureSize(uTex, 0);
  ivec2 texel = ivec2(coord(vUv.x, size.x, (uTsp & (1u << 16)) != 0u, (uTsp & (1u << 18)) != 0u),
                      coord(vUv.y, size.y, (uTsp & (1u << 15)) != 0u, (uTsp & (1u << 17)) != 0u));
  vec4 sampleColor = texelFetch(uTex, texel, 0);
  if ((uTsp & (1u << 19)) != 0u) sampleColor.a = 1.0;
  vec4 vertex = vColor;
  if (uOriginal != 0 && (uTsp & (1u << 20)) == 0u) vertex.a = 1.0;
  vec4 value = vertex;
  if ((uPcw & 8u) != 0u) {
    uint env = (uTsp >> 6) & 3u;
    if (env == 0u) value = sampleColor;
    else if (env == 1u) value = vec4(vertex.rgb * sampleColor.rgb, sampleColor.a);
    else if (env == 2u) value = vec4(mix(vertex.rgb, sampleColor.rgb, sampleColor.a), vertex.a);
    else value = vertex * sampleColor;
  }
  if ((uPcw & 4u) != 0u || (uPcw & 8u) == 0u) value.rgb += vOffset.rgb;
  value = clamp(value, 0.0, 1.0) * uTint;
  value.a *= uOpacity;
  if (uSourceIsOne != 0) value.rgb *= uOpacity;
  outColor = value;
}
`;

// ---- Present the render target to the canvas, letterboxed to the source aspect ---------------
export const BLIT_VS = `#version 300 es
precision highp float;
out vec2 vUv;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vUv = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
`;

export const BLIT_FS = `#version 300 es
precision highp float;
in vec2 vUv;
out vec4 outColor;
uniform sampler2D uTex;
void main() { outColor = vec4(texture(uTex, vUv).rgb, 1.0); }
`;
