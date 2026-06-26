#include "WorldMeshSystem.h"

#include "../Utils.h"

WorldMeshSystem::~WorldMeshSystem()
{
}

static ClientChunkSectionState* GetClientChunkSectionState(WorldClientData& clientData, int chunkIndex, int sectionIndex)
{
  return &clientData.chunkSections[chunkIndex][sectionIndex];
}

static int FindChunkIndexByCoords(const VoxelWorld& voxelWorld, int chunkX, int chunkY)
{
  for (int chunkIndex = 0; chunkIndex < voxelWorld.GetChunkCount(); chunkIndex++)
  {
    const VoxelChunk* chunk = voxelWorld.GetChunkByIndex(chunkIndex);
    if (chunk->chunkX == chunkX && chunk->chunkY == chunkY) return chunkIndex;
  }

  return -1;
}

bool WorldMeshSystem::Start()
{
  return workerPool.Start();
}

void WorldMeshSystem::Stop(WorldClientData& clientData)
{
  workerPool.Stop();
  ResetClientData(clientData);
}

void WorldMeshSystem::UnloadChunkRenderData(ClientChunkSectionState* sectionState)
{
  if (!sectionState->uploaded) return;
  UnloadModel(sectionState->model);
  *sectionState = {};
}

void WorldMeshSystem::ResetClientData(WorldClientData& clientData)
{
  for (auto& chunkSections : clientData.chunkSections)
  {
    for (ClientChunkSectionState& sectionState : chunkSections)
    {
      UnloadChunkRenderData(&sectionState);
    }
  }
}

ChunkMeshJob WorldMeshSystem::CaptureChunkSectionMeshJob(const VoxelWorld& voxelWorld, const VoxelChunk& chunk, int sectionIndex) const
{
  int chunkBaseX = chunk.chunkX * CHUNK_SIZE_XZ;
  int chunkBaseY = chunk.chunkY * CHUNK_SIZE_XZ;
  int baseVoxelZ = GetChunkSectionBaseVoxelZ(sectionIndex);
  int sectionHeight = GetChunkSectionVoxelHeight(sectionIndex);

  ChunkMeshJob job = {};
  job.chunkX = chunk.chunkX;
  job.chunkY = chunk.chunkY;
  job.sectionIndex = sectionIndex;
  job.baseVoxelZ = baseVoxelZ;
  job.sectionHeight = sectionHeight;
  job.revision = chunk.sections[sectionIndex].revision;

  for (int localZ = -1; localZ <= sectionHeight; localZ++)
  {
    for (int localY = -1; localY <= CHUNK_SIZE_XZ; localY++)
    {
      for (int localX = -1; localX <= CHUNK_SIZE_XZ; localX++)
      {
        job.voxels[localZ + 1][localY + 1][localX + 1] = voxelWorld.GetVoxel(chunkBaseX + localX, chunkBaseY + localY, baseVoxelZ + localZ);
      }
    }
  }

  return job;
}

bool WorldMeshSystem::EnqueueChunkMeshJob(const VoxelWorld& voxelWorld, const VoxelChunk& chunk, int sectionIndex)
{
  return workerPool.Enqueue(CaptureChunkSectionMeshJob(voxelWorld, chunk, sectionIndex));
}

void WorldMeshSystem::QueueDirtyChunkSectionMeshes(VoxelWorld& voxelWorld, WorldClientData& clientData, int maxSectionsToQueue)
{
  int queuedSections = 0;

  for (int chunkIndex = 0; chunkIndex < voxelWorld.GetChunkCount(); chunkIndex++)
  {
    VoxelChunk* chunk = voxelWorld.GetChunkByIndex(chunkIndex);
    for (int sectionIndex = 0; sectionIndex < CHUNK_SECTION_COUNT; sectionIndex++)
    {
      VoxelChunkSection* section = &chunk->sections[sectionIndex];
      ClientChunkSectionState* sectionState = GetClientChunkSectionState(clientData, chunkIndex, sectionIndex);
      if (!section->dirty || sectionState->queued) continue;
      if (queuedSections >= maxSectionsToQueue) return;
      if (!EnqueueChunkMeshJob(voxelWorld, *chunk, sectionIndex)) return;

      sectionState->queued = true;
      queuedSections += 1;
    }
  }
}

void WorldMeshSystem::ApplyCompletedChunkMesh(VoxelWorld& voxelWorld, WorldClientData& clientData, AssetManager& assetManager, ChunkMeshJobResult* result)
{
  VoxelChunkSection* section = voxelWorld.FindChunkSectionByCoords(result->chunkX, result->chunkY, result->sectionIndex);
  if (section == nullptr) return;
  int chunkIndex = FindChunkIndexByCoords(voxelWorld, result->chunkX, result->chunkY);
  if (chunkIndex < 0) return;
  ClientChunkSectionState* sectionState = GetClientChunkSectionState(clientData, chunkIndex, result->sectionIndex);

  sectionState->queued = false;

  if (!result->success) return;
  if (result->revision != section->revision) return;

  UnloadChunkRenderData(sectionState);
  sectionState->bounds = result->bounds;

  if (result->faceCount <= 0)
  {
    section->dirty = false;
    return;
  }

  Mesh mesh = {};
  mesh.vertexCount = result->faceCount * FACE_VERTEX_COUNT;
  mesh.triangleCount = result->faceCount * 2;
  mesh.vertices = result->vertices;
  mesh.normals = result->normals;
  mesh.texcoords = result->texcoords;
  mesh.colors = result->colors;
  UploadMesh(&mesh, false);
  sectionState->model = LoadModelFromMesh(mesh);
  sectionState->model.materials[0].shader = assetManager.FetchShader("voxel_chunk.vs", "voxel_chunk.fs");
  sectionState->uploaded = true;
  section->dirty = false;
  result->ReleaseMeshData();
}

void WorldMeshSystem::ProcessCompletedChunkMeshes(VoxelWorld& voxelWorld, WorldClientData& clientData, AssetManager& assetManager, int maxUploads)
{
  for (int uploadIndex = 0; uploadIndex < maxUploads; uploadIndex++)
  {
    ChunkMeshJobResult result = {};
    if (!workerPool.PopCompleted(&result)) break;
    ApplyCompletedChunkMesh(voxelWorld, clientData, assetManager, &result);
  }
}
