#pragma once

#include "../Base.h"

inline constexpr int CHUNK_SIZE_XZ = 16;
inline constexpr int CHUNK_MAX_HEIGHT = 1020;
inline constexpr int CHUNK_SECTION_HEIGHT = 32;
inline constexpr int CHUNK_SECTION_COUNT = (CHUNK_MAX_HEIGHT + CHUNK_SECTION_HEIGHT - 1) / CHUNK_SECTION_HEIGHT;
inline constexpr int MAX_VOXEL_CHUNKS = 64;
inline constexpr int MIN_CHUNK_COORD = -30000;
inline constexpr int MAX_CHUNK_COORD = 30000;
inline constexpr int CHUNK_LOOKUP_CAPACITY = 256;
inline constexpr int MAX_WORLD_EVENTS = 64;
inline constexpr int MAX_PLAYERS = 32;
inline constexpr int CHUNK_MESH_QUEUE_BUDGET = 16;
inline constexpr int CHUNK_MESH_UPLOAD_BUDGET = 2;
inline constexpr int MESH_WORKER_COUNT = 2;
inline constexpr int MAX_MESH_JOBS = MAX_VOXEL_CHUNKS * CHUNK_SECTION_COUNT;
inline constexpr int FOOTSTEP_VOICE_COUNT = 4;
inline constexpr int LANDING_VOICE_COUNT = 2;
inline constexpr float MOUSE_SENSITIVITY = 0.003f;
inline constexpr float MOVE_SPEED = 6.0f;
inline constexpr float FOOTSTEP_INTERVAL_SECONDS = 0.45f;
inline constexpr float FOOTSTEP_VOLUME = 0.55f;
inline constexpr float FOOTSTEP_PITCH = 1.0f;
inline constexpr float LANDING_VOLUME = 0.65f;
inline constexpr float LANDING_PITCH = 1.0f;
inline constexpr float GRAVITY = 24.0f;
inline constexpr float JUMP_VELOCITY = 9.5f;
inline constexpr float GROUND_HEIGHT = 0.0f;
inline constexpr float EYE_HEIGHT = 3.0f;
inline constexpr float PLAYER_RADIUS = 0.45f;
inline constexpr float PLAYER_HEIGHT = 3.4f;
inline constexpr float BLOCK_GRID_SIZE = 0.375f;
inline constexpr float VOXEL_EDIT_DISTANCE = 14.0f;
inline constexpr float MAX_ABS_PITCH = PI / 2.0f - 0.01f;
inline constexpr float VERTICAL_FOV_DEGREES = 90.0f;
inline constexpr float VERTICAL_FOV_RADIANS = VERTICAL_FOV_DEGREES * DEG2RAD;
inline constexpr int FACE_VERTEX_COUNT = 6;
inline constexpr int FACE_FLOATS_PER_VERTEX = 3;
inline constexpr int FACE_TEXCOORDS_PER_VERTEX = 2;
inline constexpr int FACE_COLOR_BYTES_PER_VERTEX = 4;
inline constexpr int SECTION_SNAPSHOT_SIZE_X = CHUNK_SIZE_XZ + 2;
inline constexpr int SECTION_SNAPSHOT_SIZE_Y = CHUNK_SIZE_XZ + 2;
inline constexpr int SECTION_SNAPSHOT_SIZE_Z = CHUNK_SECTION_HEIGHT + 2;
inline constexpr Vector3 WORLD_UP = {0.0f, 0.0f, 1.0f};
