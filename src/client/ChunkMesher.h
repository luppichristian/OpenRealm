#pragma once

#include "ChunkMeshJobResult.h"

struct ChunkMeshJob
{
  int chunkX = 0;
  int chunkY = 0;
  int sectionIndex = 0;
  int baseVoxelZ = 0;
  int sectionHeight = 0;
  unsigned int revision = 0;
  uint8_t voxels[SECTION_SNAPSHOT_SIZE_Z][SECTION_SNAPSHOT_SIZE_Y][SECTION_SNAPSHOT_SIZE_X] = {};
};

ChunkMeshJobResult BuildChunkMesh(const ChunkMeshJob& job);
