#include "ChunkMeshJobResult.h"

ChunkMeshJobResult::~ChunkMeshJobResult()
{
  Reset();
}

ChunkMeshJobResult::ChunkMeshJobResult(ChunkMeshJobResult&& other) noexcept
{
  MoveFrom(other);
}

ChunkMeshJobResult& ChunkMeshJobResult::operator=(ChunkMeshJobResult&& other) noexcept
{
  if (this != &other)
  {
    Reset();
    MoveFrom(other);
  }
  return *this;
}

void ChunkMeshJobResult::Reset()
{
  free(vertices);
  free(normals);
  free(texcoords);
  free(colors);
  vertices = nullptr;
  normals = nullptr;
  texcoords = nullptr;
  colors = nullptr;
  success = false;
  chunkX = 0;
  chunkY = 0;
  sectionIndex = 0;
  revision = 0;
  bounds = {};
  faceCount = 0;
}

void ChunkMeshJobResult::ReleaseMeshData()
{
  vertices = nullptr;
  normals = nullptr;
  texcoords = nullptr;
  colors = nullptr;
  faceCount = 0;
}

void ChunkMeshJobResult::MoveFrom(ChunkMeshJobResult& other)
{
  success = other.success;
  chunkX = other.chunkX;
  chunkY = other.chunkY;
  sectionIndex = other.sectionIndex;
  revision = other.revision;
  bounds = other.bounds;
  vertices = other.vertices;
  normals = other.normals;
  texcoords = other.texcoords;
  colors = other.colors;
  faceCount = other.faceCount;
  other.ReleaseMeshData();
  other.success = false;
  other.chunkX = 0;
  other.chunkY = 0;
  other.sectionIndex = 0;
  other.revision = 0;
  other.bounds = {};
}
