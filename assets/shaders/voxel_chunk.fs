#version 330

in vec3 fragWorldPosition;
in vec3 fragWorldNormal;
in vec4 fragColor;

out vec4 finalColor;

uniform vec3 cameraPosition;
uniform float blockGridSize;
uniform float timeSeconds;

float EdgeMask(vec3 worldPosition, vec3 normal)
{
  vec3 absNormal = abs(normal);
  vec2 uv;

  if (absNormal.x > absNormal.y && absNormal.x > absNormal.z)
  {
    uv = worldPosition.yz / blockGridSize;
  }
  else if (absNormal.y > absNormal.z)
  {
    uv = worldPosition.xz / blockGridSize;
  }
  else
  {
    uv = worldPosition.xy / blockGridSize;
  }

  vec2 tiledUv = abs(fract(uv) - 0.5);
  vec2 edgeDistance = vec2(0.5) - tiledUv;
  float distanceToEdge = min(edgeDistance.x, edgeDistance.y);
  float aa = max(fwidth(distanceToEdge), 0.0008);
  float edgeWidth = 0.11;
  return 1.0 - smoothstep(edgeWidth, edgeWidth + aa * 2.0, distanceToEdge);
}

void main()
{
  vec3 normal = normalize(fragWorldNormal);
  vec3 viewDirection = normalize(cameraPosition - fragWorldPosition);
  vec3 baseColor = fragColor.rgb;
  float ambientOcclusion = fragColor.a;
  float facing = pow(max(dot(normal, viewDirection), 0.0), 1.6);
  float topLight = max(normal.z, 0.0);
  float pulse = 0.94 + 0.06 * sin(timeSeconds * 1.4 + fragWorldPosition.x * 0.18 + fragWorldPosition.y * 0.18);
  float edge = EdgeMask(fragWorldPosition, normal);
  vec3 fillColor = baseColor * (0.42 + topLight * 0.08);
  vec3 glowColor = baseColor * 0.25;
  vec3 edgeColor = mix(baseColor, vec3(1.0), 0.22);
  vec3 edgeLift = edgeColor * edge * (0.85 + topLight * 0.25 + facing * 0.12) * pulse;
  vec3 backgroundColor = mix(baseColor * 0.08, vec3(0.01, 0.015, 0.025), 0.35);
  vec3 color = mix(backgroundColor, fillColor, 0.72 + topLight * 0.18);
  color += glowColor;
  color += edgeLift;
  color *= ambientOcclusion;
  color += edgeColor * edge * (1.0 - ambientOcclusion) * 0.08;
  color += edgeColor * pow(edge, 3.0) * 0.2;
  finalColor = vec4(color, 1.0);
}
