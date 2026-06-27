#include "ChunkMesher.h"

#include "../Utils.h"
#include "ChunkMeshBuilder.h"

namespace
{
  struct GreedyFaceCell
  {
    bool valid = false;
    int normalSign = 0;
    Color color = {};
  };

  uint8_t GetSnapshotVoxel(const ChunkMeshJob& job, int voxelX, int voxelY, int voxelZ)
  {
    if (voxelX < -1 || voxelX > CHUNK_SIZE_XZ) return 0;
    if (voxelY < -1 || voxelY > CHUNK_SIZE_XZ) return 0;
    if (voxelZ < -1 || voxelZ > job.sectionHeight) return 0;
    return job.voxels[voxelZ + 1][voxelY + 1][voxelX + 1];
  }

  bool IsSnapshotVoxelFilled(const ChunkMeshJob& job, int voxelX, int voxelY, int voxelZ)
  {
    return GetSnapshotVoxel(job, voxelX, voxelY, voxelZ) != 0;
  }

  float ComputeSnapshotFaceAmbientOcclusion(const ChunkMeshJob& job, int voxelX, int voxelY, int voxelZ, int normalX, int normalY, int normalZ)
  {
    int occupiedNeighbors = 0;
    int sampleCount = 0;

    if (normalX != 0)
    {
      for (int offsetY = -1; offsetY <= 1; offsetY++)
      {
        for (int offsetZ = -1; offsetZ <= 1; offsetZ++)
        {
          if (offsetY == 0 && offsetZ == 0) continue;
          if (IsSnapshotVoxelFilled(job, voxelX + normalX, voxelY + offsetY, voxelZ + offsetZ)) occupiedNeighbors += 1;
          sampleCount += 1;
        }
      }
    }
    else if (normalY != 0)
    {
      for (int offsetX = -1; offsetX <= 1; offsetX++)
      {
        for (int offsetZ = -1; offsetZ <= 1; offsetZ++)
        {
          if (offsetX == 0 && offsetZ == 0) continue;
          if (IsSnapshotVoxelFilled(job, voxelX + offsetX, voxelY + normalY, voxelZ + offsetZ)) occupiedNeighbors += 1;
          sampleCount += 1;
        }
      }
    }
    else
    {
      for (int offsetX = -1; offsetX <= 1; offsetX++)
      {
        for (int offsetY = -1; offsetY <= 1; offsetY++)
        {
          if (offsetX == 0 && offsetY == 0) continue;
          if (IsSnapshotVoxelFilled(job, voxelX + offsetX, voxelY + offsetY, voxelZ + normalZ)) occupiedNeighbors += 1;
          sampleCount += 1;
        }
      }
    }

    if (sampleCount == 0) return 1.0f;

    float occlusion = (float)occupiedNeighbors / (float)sampleCount;
    return Clamp(1.0f - occlusion * 0.55f, 0.35f, 1.0f);
  }

  bool AppendGreedyQuad(ChunkMeshBuilder* builder, int axisIndex, int normalSign, int planeIndex, int startA, int startB, int width, int height, int chunkBaseX, int chunkBaseY, int sectionBaseZ, Color color)
  {
    Vector3 corners[4] = {};
    Vector3 normal = {0.0f, 0.0f, 0.0f};

    if (axisIndex == 0)
    {
      float planeX = (float)(chunkBaseX + planeIndex) * BLOCK_GRID_SIZE;
      float y0 = (float)(chunkBaseY + startA) * BLOCK_GRID_SIZE;
      float y1 = (float)(chunkBaseY + startA + width) * BLOCK_GRID_SIZE;
      float z0 = (float)(sectionBaseZ + startB) * BLOCK_GRID_SIZE;
      float z1 = (float)(sectionBaseZ + startB + height) * BLOCK_GRID_SIZE;
      normal = {(float)normalSign, 0.0f, 0.0f};

      if (normalSign > 0)
      {
        corners[0] = {planeX, y0, z0};
        corners[1] = {planeX, y1, z0};
        corners[2] = {planeX, y1, z1};
        corners[3] = {planeX, y0, z1};
      }
      else
      {
        corners[0] = {planeX, y1, z0};
        corners[1] = {planeX, y0, z0};
        corners[2] = {planeX, y0, z1};
        corners[3] = {planeX, y1, z1};
      }
    }
    else if (axisIndex == 1)
    {
      float planeY = (float)(chunkBaseY + planeIndex) * BLOCK_GRID_SIZE;
      float x0 = (float)(chunkBaseX + startA) * BLOCK_GRID_SIZE;
      float x1 = (float)(chunkBaseX + startA + width) * BLOCK_GRID_SIZE;
      float z0 = (float)(sectionBaseZ + startB) * BLOCK_GRID_SIZE;
      float z1 = (float)(sectionBaseZ + startB + height) * BLOCK_GRID_SIZE;
      normal = {0.0f, (float)normalSign, 0.0f};

      if (normalSign > 0)
      {
        corners[0] = {x1, planeY, z0};
        corners[1] = {x0, planeY, z0};
        corners[2] = {x0, planeY, z1};
        corners[3] = {x1, planeY, z1};
      }
      else
      {
        corners[0] = {x0, planeY, z0};
        corners[1] = {x1, planeY, z0};
        corners[2] = {x1, planeY, z1};
        corners[3] = {x0, planeY, z1};
      }
    }
    else
    {
      float planeZ = (float)(sectionBaseZ + planeIndex) * BLOCK_GRID_SIZE;
      float x0 = (float)(chunkBaseX + startA) * BLOCK_GRID_SIZE;
      float x1 = (float)(chunkBaseX + startA + width) * BLOCK_GRID_SIZE;
      float y0 = (float)(chunkBaseY + startB) * BLOCK_GRID_SIZE;
      float y1 = (float)(chunkBaseY + startB + height) * BLOCK_GRID_SIZE;
      normal = {0.0f, 0.0f, (float)normalSign};

      if (normalSign > 0)
      {
        corners[0] = {x0, y0, planeZ};
        corners[1] = {x1, y0, planeZ};
        corners[2] = {x1, y1, planeZ};
        corners[3] = {x0, y1, planeZ};
      }
      else
      {
        corners[0] = {x0, y1, planeZ};
        corners[1] = {x1, y1, planeZ};
        corners[2] = {x1, y0, planeZ};
        corners[3] = {x0, y0, planeZ};
      }
    }

    return builder->AppendFace(corners, normal, color, {(float)width, (float)height});
  }

  bool BuildGreedyMeshPlane(ChunkMeshBuilder* builder, GreedyFaceCell* mask, int cellsWide, int cellsHigh, int axisIndex, int planeIndex, int chunkBaseX, int chunkBaseY, int sectionBaseZ)
  {
    for (int row = 0; row < cellsHigh; row++)
    {
      for (int column = 0; column < cellsWide; column++)
      {
        GreedyFaceCell cell = mask[row * cellsWide + column];
        if (!cell.valid) continue;

        int width = 1;
        while (column + width < cellsWide)
        {
          GreedyFaceCell next = mask[row * cellsWide + column + width];
          if (!next.valid || next.normalSign != cell.normalSign || !AreColorsEqual(next.color, cell.color)) break;
          width += 1;
        }

        int height = 1;
        bool canGrow = true;
        while (row + height < cellsHigh && canGrow)
        {
          for (int widthOffset = 0; widthOffset < width; widthOffset++)
          {
            GreedyFaceCell next = mask[(row + height) * cellsWide + column + widthOffset];
            if (!next.valid || next.normalSign != cell.normalSign || !AreColorsEqual(next.color, cell.color))
            {
              canGrow = false;
              break;
            }
          }
          if (canGrow) height += 1;
        }

        if (!AppendGreedyQuad(builder, axisIndex, cell.normalSign, planeIndex, column, row, width, height, chunkBaseX, chunkBaseY, sectionBaseZ, cell.color)) return false;

        for (int clearRow = 0; clearRow < height; clearRow++)
        {
          for (int clearColumn = 0; clearColumn < width; clearColumn++)
          {
            mask[(row + clearRow) * cellsWide + column + clearColumn].valid = false;
          }
        }
      }
    }

    return true;
  }

  BoundingBox ComputeBounds(const ChunkMeshJob& job)
  {
    float minX = (float)(job.chunkX * CHUNK_SIZE_XZ) * BLOCK_GRID_SIZE;
    float minY = (float)(job.chunkY * CHUNK_SIZE_XZ) * BLOCK_GRID_SIZE;
    float minZ = (float)job.baseVoxelZ * BLOCK_GRID_SIZE;
    float maxX = minX + (float)CHUNK_SIZE_XZ * BLOCK_GRID_SIZE;
    float maxY = minY + (float)CHUNK_SIZE_XZ * BLOCK_GRID_SIZE;
    float maxZ = minZ;

    for (int z = job.sectionHeight - 1; z >= 0; z--)
    {
      bool found = false;
      for (int localY = 0; localY < CHUNK_SIZE_XZ && !found; localY++)
      {
        for (int localX = 0; localX < CHUNK_SIZE_XZ; localX++)
        {
          if (GetSnapshotVoxel(job, localX, localY, z) == 0) continue;
          maxZ = ((float)(job.baseVoxelZ + z) + 1.0f) * BLOCK_GRID_SIZE;
          found = true;
          break;
        }
      }
      if (found) break;
    }

    return BoundingBox {
        .min = {minX, minY, minZ},
        .max = {maxX, maxY, maxZ}
    };
  }
}  // namespace

ChunkMeshJobResult BuildChunkMesh(const ChunkMeshJob& job)
{
  ChunkMeshJobResult result = {};
  result.chunkX = job.chunkX;
  result.chunkY = job.chunkY;
  result.sectionIndex = job.sectionIndex;
  result.revision = job.revision;

  ChunkMeshBuilder builder = {};
  int maxMaskCells = CHUNK_SIZE_XZ * CHUNK_SECTION_HEIGHT;
  GreedyFaceCell* mask = (GreedyFaceCell*)malloc((size_t)maxMaskCells * sizeof(GreedyFaceCell));
  if (mask == nullptr) return result;

  int chunkBaseX = job.chunkX * CHUNK_SIZE_XZ;
  int chunkBaseY = job.chunkY * CHUNK_SIZE_XZ;

  for (int axisIndex = 0; axisIndex < 3; axisIndex++)
  {
    int planes = axisIndex == 2 ? job.sectionHeight : CHUNK_SIZE_XZ;
    int cellsWide = CHUNK_SIZE_XZ;
    int cellsHigh = axisIndex == 2 ? CHUNK_SIZE_XZ : job.sectionHeight;

    for (int planeIndex = 0; planeIndex <= planes; planeIndex++)
    {
      memset(mask, 0, (size_t)cellsWide * (size_t)cellsHigh * sizeof(GreedyFaceCell));

      for (int row = 0; row < cellsHigh; row++)
      {
        for (int column = 0; column < cellsWide; column++)
        {
          int leftX = 0;
          int leftY = 0;
          int leftZ = 0;
          int rightX = 0;
          int rightY = 0;
          int rightZ = 0;

          if (axisIndex == 0)
          {
            leftX = planeIndex - 1;
            leftY = column;
            leftZ = row;
            rightX = planeIndex;
            rightY = leftY;
            rightZ = leftZ;
          }
          else if (axisIndex == 1)
          {
            leftX = column;
            leftY = planeIndex - 1;
            leftZ = row;
            rightX = leftX;
            rightY = planeIndex;
            rightZ = leftZ;
          }
          else
          {
            leftX = column;
            leftY = row;
            leftZ = planeIndex - 1;
            rightX = leftX;
            rightY = leftY;
            rightZ = planeIndex;
          }

          uint8_t leftVoxel = GetSnapshotVoxel(job, leftX, leftY, leftZ);
          uint8_t rightVoxel = GetSnapshotVoxel(job, rightX, rightY, rightZ);
          if ((leftVoxel != 0) == (rightVoxel != 0)) continue;

          int normalSign = leftVoxel != 0 ? 1 : -1;
          int voxelX = normalSign > 0 ? leftX : rightX;
          int voxelY = normalSign > 0 ? leftY : rightY;
          int voxelZ = normalSign > 0 ? leftZ : rightZ;
          uint8_t voxelValue = normalSign > 0 ? leftVoxel : rightVoxel;
          int normalX = axisIndex == 0 ? normalSign : 0;
          int normalY = axisIndex == 1 ? normalSign : 0;
          int normalZ = axisIndex == 2 ? normalSign : 0;
          float shade = ComputeSnapshotFaceAmbientOcclusion(job, voxelX, voxelY, voxelZ, normalX, normalY, normalZ);
          shade *= GetFaceLightFactor(normalX, normalY, normalZ);
          Color baseColor = GetColorFromUint8(voxelValue);

          mask[row * cellsWide + column] = GreedyFaceCell {
              .valid = true,
              .normalSign = normalSign,
              .color = {baseColor.r, baseColor.g, baseColor.b, (unsigned char)Clamp(shade * 255.0f, 0.0f, 255.0f)},
          };
        }
      }

      if (!BuildGreedyMeshPlane(&builder, mask, cellsWide, cellsHigh, axisIndex, planeIndex, chunkBaseX, chunkBaseY, job.baseVoxelZ))
      {
        free(mask);
        return result;
      }
    }
  }

  free(mask);

  result.success = true;
  result.bounds = ComputeBounds(job);
  builder.ReleaseTo(&result);
  return result;
}
