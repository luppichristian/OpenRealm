#include "ChunkMeshBuilder.h"

ChunkMeshBuilder::~ChunkMeshBuilder()
{
  free(vertices);
  free(normals);
  free(texcoords);
  free(colors);
}

bool ChunkMeshBuilder::AppendFace(Vector3 corners[4], Vector3 normal, Color color, Vector2 texcoordScale)
{
  if (!EnsureFaceCapacity(faceCount + 1)) return false;

  static const int triangleOrder[FACE_VERTEX_COUNT] = {0, 1, 2, 0, 2, 3};
  Vector2 texcoords[4] = {
      {           0.0f,            0.0f},
      {texcoordScale.x,            0.0f},
      {texcoordScale.x, texcoordScale.y},
      {           0.0f, texcoordScale.y}
  };

  int baseVertex = faceCount * FACE_VERTEX_COUNT;
  for (int i = 0; i < FACE_VERTEX_COUNT; i++)
  {
    int cornerIndex = triangleOrder[i];
    WriteVertex(baseVertex + i, corners[cornerIndex], normal, color, texcoords[cornerIndex]);
  }

  faceCount += 1;
  return true;
}

void ChunkMeshBuilder::ReleaseTo(ChunkMeshJobResult* result)
{
  result->faceCount = faceCount;
  result->vertices = vertices;
  result->normals = normals;
  result->texcoords = texcoords;
  result->colors = colors;

  faceCount = 0;
  faceCapacity = 0;
  vertices = nullptr;
  normals = nullptr;
  texcoords = nullptr;
  colors = nullptr;
}

bool ChunkMeshBuilder::EnsureFaceCapacity(int requiredFaces)
{
  if (requiredFaces <= faceCapacity) return true;

  int newCapacity = faceCapacity > 0 ? faceCapacity * 2 : 256;
  while (newCapacity < requiredFaces) newCapacity *= 2;

  int vertexCapacity = newCapacity * FACE_VERTEX_COUNT;
  float* newVertices = (float*)realloc(vertices, (size_t)vertexCapacity * FACE_FLOATS_PER_VERTEX * sizeof(float));
  if (newVertices == nullptr) return false;

  float* newNormals = (float*)realloc(normals, (size_t)vertexCapacity * FACE_FLOATS_PER_VERTEX * sizeof(float));
  if (newNormals == nullptr)
  {
    vertices = newVertices;
    return false;
  }

  float* newTexcoords = (float*)realloc(texcoords, (size_t)vertexCapacity * FACE_TEXCOORDS_PER_VERTEX * sizeof(float));
  if (newTexcoords == nullptr)
  {
    vertices = newVertices;
    normals = newNormals;
    return false;
  }

  unsigned char* newColors = (unsigned char*)realloc(colors, (size_t)vertexCapacity * FACE_COLOR_BYTES_PER_VERTEX * sizeof(unsigned char));
  if (newColors == nullptr)
  {
    vertices = newVertices;
    normals = newNormals;
    texcoords = newTexcoords;
    return false;
  }

  vertices = newVertices;
  normals = newNormals;
  texcoords = newTexcoords;
  colors = newColors;
  faceCapacity = newCapacity;
  return true;
}

void ChunkMeshBuilder::WriteVertex(int vertexIndex, Vector3 position, Vector3 normal, Color color, Vector2 texcoord)
{
  int vertexFloatIndex = vertexIndex * 3;
  int texcoordFloatIndex = vertexIndex * 2;
  int colorByteIndex = vertexIndex * 4;

  vertices[vertexFloatIndex + 0] = position.x;
  vertices[vertexFloatIndex + 1] = position.y;
  vertices[vertexFloatIndex + 2] = position.z;
  normals[vertexFloatIndex + 0] = normal.x;
  normals[vertexFloatIndex + 1] = normal.y;
  normals[vertexFloatIndex + 2] = normal.z;
  texcoords[texcoordFloatIndex + 0] = texcoord.x;
  texcoords[texcoordFloatIndex + 1] = texcoord.y;
  colors[colorByteIndex + 0] = color.r;
  colors[colorByteIndex + 1] = color.g;
  colors[colorByteIndex + 2] = color.b;
  colors[colorByteIndex + 3] = color.a;
}
