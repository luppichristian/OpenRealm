#include "WorldRenderer.h"

bool WorldRenderer::IsChunkVisible(const BoundingBox* bounds, Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp, float aspectRatio) const
{
  if (bounds->min.x == bounds->max.x && bounds->min.y == bounds->max.y && bounds->min.z == bounds->max.z) return false;

  if (cameraPosition.x >= bounds->min.x && cameraPosition.x <= bounds->max.x && cameraPosition.y >= bounds->min.y && cameraPosition.y <= bounds->max.y &&
      cameraPosition.z >= bounds->min.z && cameraPosition.z <= bounds->max.z)
    return true;

  Vector3 center = {(bounds->min.x + bounds->max.x) * 0.5f, (bounds->min.y + bounds->max.y) * 0.5f, (bounds->min.z + bounds->max.z) * 0.5f};
  Vector3 extents = {(bounds->max.x - bounds->min.x) * 0.5f, (bounds->max.y - bounds->min.y) * 0.5f, (bounds->max.z - bounds->min.z) * 0.5f};
  Vector3 toCenter = Vector3Subtract(center, cameraPosition);
  float centerForward = Vector3DotProduct(toCenter, cameraForward);
  float centerRight = Vector3DotProduct(toCenter, cameraRight);
  float centerUp = Vector3DotProduct(toCenter, cameraUp);
  float radiusForward = extents.x * fabsf(cameraForward.x) + extents.y * fabsf(cameraForward.y) + extents.z * fabsf(cameraForward.z);
  float radiusRight = extents.x * fabsf(cameraRight.x) + extents.y * fabsf(cameraRight.y) + extents.z * fabsf(cameraRight.z);
  float radiusUp = extents.x * fabsf(cameraUp.x) + extents.y * fabsf(cameraUp.y) + extents.z * fabsf(cameraUp.z);
  const float nearPlane = 0.01f;
  const float farPlane = 1000.0f;

  if (centerForward + radiusForward < nearPlane) return false;
  if (centerForward - radiusForward > farPlane) return false;

  float halfVertical = tanf(VERTICAL_FOV_RADIANS * 0.5f);
  float halfHorizontal = halfVertical * aspectRatio;
  float forwardMax = fmaxf(centerForward + radiusForward, nearPlane);
  if (fabsf(centerRight) - radiusRight > forwardMax * halfHorizontal) return false;
  if (fabsf(centerUp) - radiusUp > forwardMax * halfVertical) return false;

  return true;
}

void WorldRenderer::DrawVoxelWorld(const VoxelWorld& voxelWorld, Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp) const
{
  float aspectRatio = (float)GetScreenWidth() / (float)GetScreenHeight();

  for (int chunkIndex = 0; chunkIndex < voxelWorld.GetChunkCount(); chunkIndex++)
  {
    const VoxelChunk* chunk = voxelWorld.GetChunkByIndex(chunkIndex);
    for (int sectionIndex = 0; sectionIndex < CHUNK_SECTION_COUNT; sectionIndex++)
    {
      const VoxelChunkSection* section = &chunk->sections[sectionIndex];
      if (!section->uploaded) continue;
      if (!IsChunkVisible(&section->bounds, cameraPosition, cameraForward, cameraRight, cameraUp, aspectRatio)) continue;
      DrawModel(section->model, Vector3Zero(), 1.0f, WHITE);
    }
  }
}

void WorldRenderer::Render(const VoxelWorld& voxelWorld, const PlayerSystem& playerSystem, const WorldAssets& assets, int playerId) const
{
  const PlayerState* player = playerSystem.FindPlayer(playerId);
  if (player == nullptr) return;

  Vector3 cameraPosition;
  Vector3 cameraForward;
  Vector3 cameraRight;
  Vector3 cameraUp;
  playerSystem.GetCameraVectors(player, &cameraPosition, &cameraForward, &cameraRight, &cameraUp);
  assets.UpdateShaders(cameraPosition, cameraForward, cameraRight, cameraUp);

  ClearBackground(BLACK);
  BeginShaderMode(assets.GetFloorShader());
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
  EndShaderMode();

  Camera3D camera = {};
  camera.position = cameraPosition;
  camera.target = Vector3Add(cameraPosition, cameraForward);
  camera.up = WORLD_UP;
  camera.fovy = VERTICAL_FOV_DEGREES;
  camera.projection = CAMERA_PERSPECTIVE;

  BeginMode3D(camera);
  DrawVoxelWorld(voxelWorld, cameraPosition, cameraForward, cameraRight, cameraUp);
  playerSystem.DrawOtherPlayers(playerId);
  EndMode3D();
}
