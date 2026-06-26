#pragma once

#include "ChunkMeshJobResult.h"

class ChunkMeshBuilder
{
 public:
  ~ChunkMeshBuilder();

  bool AppendFace(Vector3 corners[4], Vector3 normal, Color color, Vector2 texcoordScale);
  void ReleaseTo(ChunkMeshJobResult* result);

 private:
  bool EnsureFaceCapacity(int requiredFaces);
  void WriteVertex(int vertexIndex, Vector3 position, Vector3 normal, Color color, Vector2 texcoord);

  float* vertices = nullptr;
  float* normals = nullptr;
  float* texcoords = nullptr;
  unsigned char* colors = nullptr;
  int faceCount = 0;
  int faceCapacity = 0;
};
