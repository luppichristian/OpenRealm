#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;
uniform mat4 mvp;
uniform mat4 matModel;

out vec3 fragLocalPosition;
out vec3 fragWorldPosition;
out vec3 fragWorldNormal;

void main() {
  vec4 worldPosition = matModel * vec4(vertexPosition, 1.0);
  fragLocalPosition = vertexPosition;
  fragWorldPosition = worldPosition.xyz;
  fragWorldNormal = normalize(mat3(matModel) * vertexNormal);
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
