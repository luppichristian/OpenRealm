#include "WorldRenderer.h"

struct ChunkVisibilityQuery
{
  const BoundingBox* bounds = nullptr;
  PlayerSystem::CameraVectors camera = {};
  float aspectRatio = 1.0f;
};

struct WorldDrawPass
{
  const VoxelWorld* voxelWorld = nullptr;
  const WorldClientData* clientData = nullptr;
  PlayerSystem::CameraVectors camera = {};
  float aspectRatio = 1.0f;
};

static bool IsChunkVisible(const ChunkVisibilityQuery& query)
{
  const BoundingBox& bounds = *query.bounds;
  const PlayerSystem::CameraVectors& camera = query.camera;
  if (bounds.min.x == bounds.max.x && bounds.min.y == bounds.max.y && bounds.min.z == bounds.max.z) return false;

  if (camera.position.x >= bounds.min.x && camera.position.x <= bounds.max.x && camera.position.y >= bounds.min.y && camera.position.y <= bounds.max.y &&
      camera.position.z >= bounds.min.z && camera.position.z <= bounds.max.z)
    return true;

  Vector3 center = {(bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f, (bounds.min.z + bounds.max.z) * 0.5f};
  Vector3 extents = {(bounds.max.x - bounds.min.x) * 0.5f, (bounds.max.y - bounds.min.y) * 0.5f, (bounds.max.z - bounds.min.z) * 0.5f};
  Vector3 toCenter = Vector3Subtract(center, camera.position);
  float centerForward = Vector3DotProduct(toCenter, camera.forward);
  float centerRight = Vector3DotProduct(toCenter, camera.right);
  float centerUp = Vector3DotProduct(toCenter, camera.up);
  float radiusForward = extents.x * fabsf(camera.forward.x) + extents.y * fabsf(camera.forward.y) + extents.z * fabsf(camera.forward.z);
  float radiusRight = extents.x * fabsf(camera.right.x) + extents.y * fabsf(camera.right.y) + extents.z * fabsf(camera.right.z);
  float radiusUp = extents.x * fabsf(camera.up.x) + extents.y * fabsf(camera.up.y) + extents.z * fabsf(camera.up.z);
  const float nearPlane = 0.01f;
  const float farPlane = 1000.0f;

  if (centerForward + radiusForward < nearPlane) return false;
  if (centerForward - radiusForward > farPlane) return false;

  float halfVertical = tanf(VERTICAL_FOV_RADIANS * 0.5f);
  float halfHorizontal = halfVertical * query.aspectRatio;
  float forwardMax = fmaxf(centerForward + radiusForward, nearPlane);
  if (fabsf(centerRight) - radiusRight > forwardMax * halfHorizontal) return false;
  if (fabsf(centerUp) - radiusUp > forwardMax * halfVertical) return false;

  return true;
}

static void DrawVoxelWorld(const WorldDrawPass& pass)
{
  // Mesh bounds are conservative, so this culling pass just avoids obviously off-screen sections.
  for (int chunkIndex = 0; chunkIndex < pass.voxelWorld->GetChunkCount(); chunkIndex++)
  {
    for (int sectionIndex = 0; sectionIndex < CHUNK_SECTION_COUNT; sectionIndex++)
    {
      const ClientChunkSectionState& sectionState = pass.clientData->chunkSections[chunkIndex][sectionIndex];
      if (!sectionState.uploaded) continue;
      if (!IsChunkVisible({.bounds = &sectionState.bounds, .camera = pass.camera, .aspectRatio = pass.aspectRatio})) continue;
      DrawModel(sectionState.model, Vector3Zero(), 1.0f, WHITE);
    }
  }
}

static void DrawOtherPlayers(const PlayerSystem& playerSystem, int cameraPlayerId)
{
  for (const PlayerState& player : playerSystem.GetPlayers())
  {
    if (!player.active || player.id == cameraPlayerId) continue;

    Vector3 center = player.position;
    center.z += PLAYER_HEIGHT * 0.5f;
    DrawCubeV(center, Vector3 {PLAYER_RADIUS * 2.0f, PLAYER_RADIUS * 2.0f, PLAYER_HEIGHT}, Color {120, 220, 255, 180});
    DrawCubeWiresV(center, {PLAYER_RADIUS * 2.0f, PLAYER_RADIUS * 2.0f, PLAYER_HEIGHT}, RAYWHITE);
  }
}

void RenderWorld(const VoxelWorld& voxelWorld, const PlayerSystem& playerSystem, const WorldClientData& clientData, AssetManager& assetManager, int playerId)
{
  const PlayerState* player = playerSystem.FindPlayer(playerId);
  if (player == nullptr) return;

  PlayerSystem::CameraVectors camera = playerSystem.BuildCameraVectors(player);
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

  // The fullscreen floor shader reconstructs world-space rays from the camera basis, so
  // we upload the full basis every frame instead of relying on raylib's fixed camera state.
  SetShaderValue(floorShader, floorCameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraForwardLoc, &camera.forward, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraRightLoc, &camera.right, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorCameraUpLoc, &camera.up, SHADER_UNIFORM_VEC3);
  SetShaderValue(floorShader, floorResolutionLoc, &resolution, SHADER_UNIFORM_VEC2);
  SetShaderValue(floorShader, floorVerticalFovLoc, &verticalFovRadians, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorAspectRatioLoc, &aspectRatio, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorTimeSecondsLoc, &timeSeconds, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorBlockGridSizeLoc, &blockGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(floorShader, floorChunkGridSizeLoc, &chunkGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(voxelShader, voxelCameraPositionLoc, &camera.position, SHADER_UNIFORM_VEC3);
  SetShaderValue(voxelShader, voxelBlockGridSizeLoc, &blockGridSize, SHADER_UNIFORM_FLOAT);
  SetShaderValue(voxelShader, voxelTimeSecondsLoc, &timeSeconds, SHADER_UNIFORM_FLOAT);

  ClearBackground(BLACK);
  BeginShaderMode(floorShader);
  // The floor is rendered as a fullscreen pass first so the chunk meshes can stay focused on voxels only.
  DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), WHITE);
  EndShaderMode();

  Camera3D renderCamera = {};
  renderCamera.position = camera.position;
  renderCamera.target = Vector3Add(camera.position, camera.forward);
  renderCamera.up = WORLD_UP;
  renderCamera.fovy = VERTICAL_FOV_DEGREES;
  renderCamera.projection = CAMERA_PERSPECTIVE;

  BeginMode3D(renderCamera);
  DrawVoxelWorld({.voxelWorld = &voxelWorld, .clientData = &clientData, .camera = camera, .aspectRatio = aspectRatio});
  DrawOtherPlayers(playerSystem, playerId);
  EndMode3D();
}
