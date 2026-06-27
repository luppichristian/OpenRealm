#pragma once

#include "../world/PlayerSystem.h"
#include "../world/VoxelWorld.h"
#include "AssetManager.h"
#include "WorldClientData.h"

void RenderWorld(const VoxelWorld& voxelWorld, const PlayerSystem& playerSystem, const WorldClientData& clientData, AssetManager& assetManager, int playerId);
