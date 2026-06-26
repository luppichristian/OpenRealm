#include "WorldRenderer.h"

static bool IsChunkVisible(const BoundingBox* bounds, Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp, float aspectRatio)
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

static void DrawVoxelWorld(const VoxelWorld& voxelWorld, Vector3 cameraPosition, Vector3 cameraForward, Vector3 cameraRight, Vector3 cameraUp)
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

void RenderWorld(const VoxelWorld& voxelWorld, const PlayerSystem& playerSystem, AssetManager& assetManager, int playerId)
{
  const PlayerState* player = playerSystem.FindPlayer(playerId);
  if (player == nullptr) return;

  Vector3 cameraPosition;
  Vector3 cameraForward;
  Vector3 cameraRight;
  Vector3 cameraUp;
  playerSystem.GetCameraVectors(player, &cameraPosition, &cameraForward, &cameraRight, &cameraUp);
  Shader floorShader = assetManager.FetchShader("infinite_floor.fs");
  Shader voxelShader = assetManager.FetchShader("voxel_chunk.vs", "voxel_chunk.fs");
  Vector2 resolution = {(float)GetScreenWidth(), (float)GetScreenHeight()};
  float aspectRatio = resolution.x / resolution.y;
  float timeSeconds = (float)GetTime();
  float chunkGridSize = BLOCK_GRID_SIZE * (float)CHUNK_SIZE_XZ;
  float verticalFovRadians = VERTICAL_FOV_RADIANS;
  float blockGridSize = BLOCK_GRID_SIZE;
  int floorCameraPositionLoc = assetManager.FetchShaderLocation(floorShader, "cameraPosition");
  int floorCameraForwardLoc = assetManager.FetchShaderLocation(floorShader, "cameraForward");
  int floorCameraRightLoc = assetManager.FetchShaderLocation(floorShader, "cameraRight");
  int floorCameraUpLoc = assetManager.FetchShaderLocation(floorShader, "cameraUp");
  int floorResolutionLoc = assetManager.FetchShaderLocation(floorShader, "resolution");
  int floorVerticalFovLoc = assetManager.FetchShaderLocation(floorShader, "verticalFovRadians");
  int floorAspectRatioLoc = assetManager.FetchShaderLocation(floorShader, "aspectRatio");
  int floorTimeSecondsLoc = assetManager.FetchShaderLocation(floorShader, "timeSeconds");
  int floorBlockGridSizeLoc = assetManager.FetchShaderLocation(floorShader, "blockGridSize");
  int floorChunkGridSizeLoc = assetManager.FetchShaderLocation(floorShader, "chunkGridSize");
  int voxelCameraPositionLoc = assetManager.FetchShaderLocation(voxelShader, "cameraPosition");
  int voxelBlockGridSizeLoc = assetManager.FetchShaderLocation(voxelShader, "blockGridSize");
  int voxelTimeSecondsLoc = assetManager.FetchShaderLocation(voxelShader, "timeSeconds");
  SetShaderValue(floorShader, floorCameraPositionLoc, &cameraPosition, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraForwardLoc, &cameraForward, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraRightLoc, &cameraRight, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraUpLoc, &cameraUp, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorResolutionLoc, &resolution, SHADER_UNIFORM_VEC2);
  SetShaderValue(floorShader, floorVerticalFovLoc, &verticalFovRadians, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorAspectRatioLoc, &aspectRatio, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorTimeSecondsLoc, &timeSeconds, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorBlockGridSizeLoc, &blockGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorChunkGridSizeLoc, &chunkGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(voxelShader, voxelCameraPositionLoc, &cameraPosition, SHADER_UNIFORM_VEC3);
  SetShaderValue(voxelShader, voxelBlockGridSizeLoc, &blockGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(voxelShader, voxelTimeSecondsLoc, &timeSeconds, SHADER_UNIFORM_FLOAT);

  ClearBackground(BLACK);
  BeginShaderMode(floorShader);
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
