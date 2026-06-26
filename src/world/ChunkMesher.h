#pragma once

#include "ChunkMeshJobResult.h"
#include "ChunkMeshWorkerPool.h"

class ChunkMesher
{
 public:
  static ChunkMeshJobResult Build(const ChunkMeshJob& job);
};
