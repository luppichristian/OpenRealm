#version 330

in vec3 fragLocalPosition;
in vec3 fragWorldPosition;
in vec3 fragWorldNormal;

out vec4 finalColor;

uniform vec3 cameraPosition;
uniform float timeSeconds;
uniform vec3 blockColor;
uniform vec3 faceOcclusionA;
uniform vec3 faceOcclusionB;

float EdgeMask(vec3 localPosition) {
  vec3 absLocal = abs(localPosition);
  vec2 faceUv;

  if (absLocal.x > absLocal.y && absLocal.x > absLocal.z) {
    faceUv = localPosition.yz;
  } else if (absLocal.y > absLocal.z) {
    faceUv = localPosition.xz;
  } else {
    faceUv = localPosition.xy;
  }

  vec2 edgeDistance = vec2(0.5) - abs(faceUv);
  float distanceToEdge = min(edgeDistance.x, edgeDistance.y);
  float aa = max(fwidth(distanceToEdge), 0.0008);
  float edgeWidth = 0.11;
  return 1.0 - smoothstep(edgeWidth, edgeWidth + aa * 2.0, distanceToEdge);
}

float FaceAmbientOcclusion(vec3 normal) {
  vec3 absNormal = abs(normal);

  if (absNormal.x > absNormal.y && absNormal.x > absNormal.z) {
    return normal.x > 0.0 ? faceOcclusionA.x : faceOcclusionA.y;
  }

  if (absNormal.y > absNormal.z) {
    return normal.y > 0.0 ? faceOcclusionA.z : faceOcclusionB.x;
  }

  return normal.z > 0.0 ? faceOcclusionB.y : faceOcclusionB.z;
}

void main() {
  vec3 normal = normalize(fragWorldNormal);
  vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
  float facing = max(dot(normal, viewDirection), 0.0);
  float topLight = max(normal.z, 0.0);
  float pulse = 0.92 + 0.08 * sin(timeSeconds * 1.4 + fragWorldPosition.x * 0.18 + fragWorldPosition.y * 0.18);
  float edge = EdgeMask(fragLocalPosition);
  float ambientOcclusion = FaceAmbientOcclusion(normal);

  vec3 baseColor = mix(blockColor * 0.08, vec3(0.01, 0.015, 0.025), 0.35);
  vec3 fillColor = blockColor * (0.42 + topLight * 0.08);
  vec3 edgeColor = mix(blockColor, vec3(1.0), 0.22);
  vec3 glowColor = blockColor * 0.25;

  vec3 color = mix(baseColor, fillColor, 0.72 + topLight * 0.18);
  color += glowColor * (0.12 + topLight * 0.08);
  color += edgeColor * edge * (0.85 + topLight * 0.25 + facing * 0.12) * pulse;
  color += edgeColor * pow(edge, 3.0) * 0.2;
  color *= ambientOcclusion;
  color += edgeColor * edge * (1.0 - ambientOcclusion) * 0.08;

  finalColor = vec4(color, 1.0);
}
