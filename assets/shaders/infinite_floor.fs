#version 330

out vec4 finalColor;

uniform vec3 cameraPosition;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec2 resolution;
uniform float verticalFovRadians;
uniform float aspectRatio;
uniform float timeSeconds;
uniform float blockGridSize;
uniform float chunkGridSize;

float Hash12(vec2 p) {
  vec3 p3 = fract(vec3(p.xyx) * 0.1031);
  p3 += dot(p3, p3.yzx + 33.33);
  return fract((p3.x + p3.y) * p3.z);
}

float SoftDot(vec2 localPosition, vec2 derivatives, float radius) {
  float dist = length(localPosition / derivatives);
  return 1.0 - smoothstep(radius, radius + 1.35, dist);
}

float DotLayer(vec2 worldPosition, float cellSize, float radius, float motionSpeed) {
  vec2 cell = worldPosition / cellSize;
  vec2 cellId = floor(cell);
  float hash = Hash12(cellId);
  vec2 local = fract(cell) - 0.5;

  float twinkle = 0.55 + 0.45 * sin(timeSeconds * motionSpeed + hash * 6.2831853);
  local.y += 0.08 * sin(timeSeconds * (motionSpeed * 0.6) + hash * 11.0);

  vec2 derivatives = max(fwidth(cell), vec2(0.0001));
  return SoftDot(local, derivatives, radius) * mix(0.45, 1.0, twinkle);
}

float GridLineLayer(vec2 worldPosition, float cellSize, float lineWidth) {
  vec2 cell = worldPosition / cellSize;
  vec2 local = min(fract(cell), 1.0 - fract(cell));
  vec2 derivatives = max(fwidth(cell), vec2(0.0001));

  float lineX = 1.0 - smoothstep(lineWidth, lineWidth + derivatives.x * 1.5, local.x);
  float lineY = 1.0 - smoothstep(lineWidth, lineWidth + derivatives.y * 1.5, local.y);
  return max(lineX, lineY);
}

float ParticleLayer(vec2 worldPosition, float cellSize) {
  vec2 cell = worldPosition / cellSize;
  vec2 cellId = floor(cell);
  vec2 local = fract(cell) - 0.5;
  vec2 derivatives = max(fwidth(cell), vec2(0.0001));

  float presence = step(0.62, Hash12(cellId + 7.31));
  vec2 offset = vec2(Hash12(cellId + 13.2), Hash12(cellId + 29.4)) - 0.5;
  offset *= 0.38;

  float phase = Hash12(cellId + 41.0) * 6.2831853;
  float twinkle = 0.55 + 0.45 * sin(timeSeconds * 3.6 + phase);
  vec2 drift = vec2(
                   sin(timeSeconds * 1.7 + phase),
                   cos(timeSeconds * 2.1 + phase * 1.3)) *
               0.08;

  return presence * SoftDot(local - offset - drift, derivatives, 0.72) * twinkle;
}

float SkyParticleLayer(vec2 skyPosition, float cellSize) {
  vec2 cell = skyPosition / cellSize;
  vec2 cellId = floor(cell);
  vec2 local = fract(cell) - 0.5;
  vec2 derivatives = max(fwidth(cell), vec2(0.0001));

  float presence = step(0.86, Hash12(cellId + 91.7));
  vec2 offset = vec2(Hash12(cellId + 17.3), Hash12(cellId + 53.8)) - 0.5;
  offset *= 0.42;

  float phase = Hash12(cellId + 67.1) * 6.2831853;
  float twinkle = 0.45 + 0.55 * sin(timeSeconds * 2.2 + phase);
  vec2 drift = vec2(
                   sin(timeSeconds * 0.75 + phase),
                   cos(timeSeconds * 0.62 + phase * 1.2)) *
               0.08;

  return presence * SoftDot(local - offset - drift, derivatives, 0.68) * twinkle;
}

void main() {
  vec2 uv = gl_FragCoord.xy / resolution;
  vec2 ndc = vec2(uv.x * 2.0 - 1.0, uv.y * 2.0 - 1.0);
  float tanHalfFov = tan(verticalFovRadians * 0.5);
  vec3 rayDirection = normalize(cameraForward + ndc.x * aspectRatio * tanHalfFov * cameraRight + ndc.y * tanHalfFov * cameraUp);

  if (rayDirection.z >= 0.0) {
    vec2 skyUv = rayDirection.xy / max(rayDirection.z + 0.35, 0.1);
    float skyParticles = SkyParticleLayer(skyUv * 10.0, 0.9);
    float skyParticlesWide = SkyParticleLayer(skyUv * 5.5 + vec2(11.0, -7.0), 1.35);
    float skyFade = smoothstep(-0.02, 0.2, rayDirection.z);

    vec3 skyBaseColor = vec3(0.004, 0.008, 0.016);
    vec3 skyParticleColor = vec3(0.22, 0.34, 0.42);
    vec3 skyColor = skyBaseColor + skyParticleColor * (skyParticles * 0.14 + skyParticlesWide * 0.08) * skyFade;
    finalColor = vec4(skyColor, 1.0);
    return;
  }

  float t = -cameraPosition.z / rayDirection.z;
  if (t <= 0.0) {
    finalColor = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }

  vec3 hitPoint = cameraPosition + rayDirection * t;

  float fineGrid = GridLineLayer(hitPoint.xy, blockGridSize, 0.04);
  float majorGrid = GridLineLayer(hitPoint.xy, chunkGridSize, 0.015);
  float nodes = DotLayer(hitPoint.xy, chunkGridSize, 1.6, 1.6);
  float particles = ParticleLayer(hitPoint.xy, 0.55);
  float majorMask = 1.0 - majorGrid;
  float fineMask = 1.0 - fineGrid;
  float finePattern = max(fineGrid * 0.65, nodes * 0.85);

  float planarDistance = length(hitPoint.xy - cameraPosition.xy);
  float farFade = 1.0 - smoothstep(55.0, 170.0, planarDistance);
  float horizonFade = clamp((-rayDirection.z - 0.015) / 0.18, 0.0, 1.0);
  float fade = farFade * horizonFade;

  vec3 baseColor = vec3(0.012, 0.02, 0.035);
  vec3 gridColor = vec3(0.0, 0.42, 0.58);
  vec3 accentColor = vec3(0.28, 0.62, 0.72);
  vec3 glowColor = vec3(0.05, 0.18, 0.42);
  vec3 particleColor = vec3(0.48, 0.66, 0.72);
  vec3 color = baseColor;
  color += glowColor * 0.05 * fade;
  color += gridColor * finePattern * 0.72 * fade * majorMask;
  color += accentColor * nodes * 0.12 * fade * majorMask;
  color += particleColor * particles * 0.14 * fade * majorMask * fineMask;
  color += gridColor * majorGrid * 0.9 * fade;

  finalColor = vec4(color, 1.0);
}
