#pragma once

#include "../Utils.h"

class ChunkMeshJobResult : public NonCopyable
{
 public:
  ChunkMeshJobResult() = default;
  ~ChunkMeshJobResult();

  ChunkMeshJobResult(ChunkMeshJobResult&& other) noexcept;
  ChunkMeshJobResult& operator=(ChunkMeshJobResult&& other) noexcept;

  void Reset();
  void ReleaseMeshData();

  bool success = false;
  int chunkX = 0;
  int chunkY = 0;
  int sectionIndex = 0;
  unsigned int revision = 0;
  BoundingBox bounds = {};
  float* vertices = nullptr;
  float* normals = nullptr;
  float* texcoords = nullptr;
  unsigned char* colors = nullptr;
  int faceCount = 0;

 private:
  void MoveFrom(ChunkMeshJobResult& other);
};
