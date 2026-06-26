#include "WorldMeshSystem.h"

WorldMeshSystem::~WorldMeshSystem()
{
  Stop();
}

bool WorldMeshSystem::Start()
{
  return workerPool.Start();
}

void WorldMeshSystem::Stop()
{
  workerPool.Stop();
}

void WorldMeshSystem::UnloadChunkRenderData(VoxelChunkSection* section) const
{
  if (!section->uploaded) return;
  UnloadModel(section->model);
  section->uploaded = false;
}

ChunkMeshJob WorldMeshSystem::CaptureChunkSectionMeshJob(const VoxelWorld& voxelWorld, const VoxelChunk& chunk, int sectionIndex) const
{
  int chunkBaseX = chunk.chunkX * CHUNK_SIZE_XZ;
  int chunkBaseY = chunk.chunkY * CHUNK_SIZE_XZ;
  int baseVoxelZ = VoxelWorld::GetChunkSectionBaseVoxelZ(sectionIndex);
  int sectionHeight = VoxelWorld::GetChunkSectionVoxelHeight(sectionIndex);

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

void WorldMeshSystem::QueueDirtyChunkSectionMeshes(VoxelWorld& voxelWorld, int maxSectionsToQueue)
{
  int queuedSections = 0;

  for (int chunkIndex = 0; chunkIndex < voxelWorld.GetChunkCount(); chunkIndex++)
  {
    VoxelChunk* chunk = voxelWorld.GetChunkByIndex(chunkIndex);
    for (int sectionIndex = 0; sectionIndex < CHUNK_SECTION_COUNT; sectionIndex++)
    {
      VoxelChunkSection* section = &chunk->sections[sectionIndex];
      if (!section->dirty || section->queued) continue;
      if (queuedSections >= maxSectionsToQueue) return;
      if (!EnqueueChunkMeshJob(voxelWorld, *chunk, sectionIndex)) return;

      section->queued = true;
      queuedSections += 1;
    }
  }
}

void WorldMeshSystem::ApplyCompletedChunkMesh(VoxelWorld& voxelWorld, const WorldAssets& assets, ChunkMeshJobResult* result)
{
  VoxelChunkSection* section = voxelWorld.FindChunkSectionByCoords(result->chunkX, result->chunkY, result->sectionIndex);
  if (section == nullptr) return;

  section->queued = false;

  if (!result->success) return;
  if (result->revision != section->revision) return;

  UnloadChunkRenderData(section);
  section->bounds = result->bounds;

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
  section->model = LoadModelFromMesh(mesh);
  section->model.materials[0].shader = assets.GetVoxelShader();
  section->uploaded = true;
  section->dirty = false;
  result->ReleaseMeshData();
}

void WorldMeshSystem::ProcessCompletedChunkMeshes(VoxelWorld& voxelWorld, const WorldAssets& assets, int maxUploads)
{
  for (int uploadIndex = 0; uploadIndex < maxUploads; uploadIndex++)
  {
    ChunkMeshJobResult result = {};
    if (!workerPool.PopCompleted(&result)) break;
    ApplyCompletedChunkMesh(voxelWorld, assets, &result);
  }
}
