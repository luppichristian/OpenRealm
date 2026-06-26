#pragma once

#include "../AssetManager.h"
#include "PlayerSystem.h"
#include "VoxelWorld.h"

void RenderWorld(const VoxelWorld& voxelWorld, const PlayerSystem& playerSystem, AssetManager& assetManager, int playerId);
