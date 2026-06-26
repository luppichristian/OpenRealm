#version 330

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec4 vertexColor;
in vec2 vertexTexCoord;

uniform mat4 mvp;
uniform mat4 matModel;

out vec3 fragWorldPosition;
out vec3 fragWorldNormal;
out vec4 fragColor;
out vec2 fragTexCoord;

void main() {
  vec4 worldPosition = matModel * vec4(vertexPosition, 1.0);
  fragWorldPosition = worldPosition.xyz;
  fragWorldNormal = normalize(mat3(matModel) * vertexNormal);
  fragColor = vertexColor;
  fragTexCoord = vertexTexCoord;
  gl_Position = mvp * vec4(vertexPosition, 1.0);
}
