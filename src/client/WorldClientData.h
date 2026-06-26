#pragma once

#include <array>

#include "../world/WorldConfig.h"

struct ClientChunkSectionState
{
  bool uploaded = false;
  bool queued = false;
  BoundingBox bounds = {};
  Model model = {};
};

struct WorldClientData
{
  std::array<std::array<ClientChunkSectionState, CHUNK_SECTION_COUNT>, MAX_VOXEL_CHUNKS> chunkSections = {};
};
