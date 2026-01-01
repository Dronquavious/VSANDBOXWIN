#include "chunk_manager.h"
#include "world_generator.h"
#include "raymath.h"
#include "rlgl.h"
#include "../blocks/block_types.h"
#include <cmath>
#include <vector>
#include <cstring>

void ChunkManager::Init() {}

void ChunkManager::UnloadAll() {
    for (auto& pair : chunks) {
        UnloadChunkModels(pair.second);
    }
    chunks.clear();
}

void ChunkManager::UnloadChunkModels(Chunk& chunk) {
    for (int i = 0; i < 13; i++) {
        if (chunk.layers[i].meshCount > 0) {
            UnloadModel(chunk.layers[i]);
            chunk.layers[i] = { 0 };
        }
    }
    chunk.meshReady = false;
}

int ChunkManager::GetBlock(int x, int y, int z, bool createIfMissing) {
    if (y < 0 || y >= CHUNK_SIZE) return 0;

    int cx = (int)floor((float)x / CHUNK_SIZE);
    int cz = (int)floor((float)z / CHUNK_SIZE);
    ChunkCoord coord = { cx, cz };

    if (chunks.find(coord) == chunks.end()) {
        if (createIfMissing) GenerateChunk(chunks[coord], cx, cz);
        else return 0;
    }

    int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    return chunks[coord].blocks[lx][y][lz];
}

void ChunkManager::SetBlock(int x, int y, int z, int type) {
    if (y < 0 || y >= CHUNK_SIZE) return;

    int cx = (int)floor((float)x / CHUNK_SIZE);
    int cz = (int)floor((float)z / CHUNK_SIZE);
    ChunkCoord coord = { cx, cz };

    if (chunks.find(coord) == chunks.end()) GenerateChunk(chunks[coord], cx, cz);

    int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    chunks[coord].blocks[lx][y][lz] = type;
    chunks[coord].meshReady = false;

    chunks[coord].shouldStep = true;
}

bool ChunkManager::IsBlockSolid(int x, int y, int z) {
    return GetBlock(x, y, z) != 0;
}

void ChunkManager::GenerateChunk(Chunk& chunk, int chunkX, int chunkZ) {
    WorldGenerator::GenerateChunk(chunk, chunkX, chunkZ);
    ComputeChunkLighting(chunk);

    // wake up the chunk so floating sand can settle
    chunk.shouldStep = true;
}

void ChunkManager::ComputeChunkLighting(Chunk& chunk) {
    // collum lighting alg
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            bool inShadow = false;
            // scan from top to bottom
            for (int y = CHUNK_SIZE - 1; y >= 0; y--) {
                if (inShadow) {
                    chunk.light[x][y][z] = 0; // darkness
                } else {
                    chunk.light[x][y][z] = 15; // sunlight
                }
                int block = chunk.blocks[x][y][z];
                if (block != 0 && block != BLOCK_LEAVES) { 
                    inShadow = true;
                }
            }
        }
    }
}

void ChunkManager::BuildChunkMesh(Chunk& chunk, int cx, int cz, Texture2D* textures) {
    UnloadChunkModels(chunk);

    std::vector<float> vertices[13];
    std::vector<float> texcoords[13];
    std::vector<unsigned char> colors[13];

    auto getLight = [&](int x, int y, int z) {
        if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE) {
            return 15;
        }
        return (int)chunk.light[x][y][z];
    };

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                int blockID = chunk.blocks[x][y][z];
                if (blockID == 0) continue;

                // culling checks
                bool top = (y == CHUNK_SIZE - 1) || (chunk.blocks[x][y + 1][z] == 0);
                bool bottom = (y > 0) && (chunk.blocks[x][y - 1][z] == 0);
                bool left = (x == 0) || (chunk.blocks[x - 1][y][z] == 0);
                bool right = (x == CHUNK_SIZE - 1) || (chunk.blocks[x + 1][y][z] == 0);
                bool front = (z == CHUNK_SIZE - 1) || (chunk.blocks[x][y][z + 1] == 0);
                bool back = (z == 0) || (chunk.blocks[x][y][z - 1] == 0);

                if (!top && !bottom && !left && !right && !front && !back) continue;

                float gx = (float)(cx * CHUNK_SIZE + x);
                float gy = (float)y;
                float gz = (float)(cz * CHUNK_SIZE + z);

                auto pushFace = [&](int renderID, float* vData, float* uvData, int lightLevel) {
                    int b = (int)((float)lightLevel / 15.0f * 255.0f);
                    if (b < 60) b = 60; 
                    Color c = { (unsigned char)b, (unsigned char)b, (unsigned char)b, 255 };

                    for (int k = 0; k < 18; k++) vertices[renderID].push_back(vData[k]);
                    for (int k = 0; k < 12; k++) texcoords[renderID].push_back(uvData[k]);
                    for (int k = 0; k < 6; k++) {
                        colors[renderID].push_back(c.r);
                        colors[renderID].push_back(c.g);
                        colors[renderID].push_back(c.b);
                        colors[renderID].push_back(c.a);
                    }
                };

                auto getRenderID = [&](bool isTop, bool isBottom) {
                    if (blockID == BLOCK_GRASS) { 
                        if (isTop) return (int)BLOCK_GRASS;
                        if (isBottom) return (int)BLOCK_DIRT;
                        return (int)BLOCK_GRASS_SIDE;
                    }
                    if (blockID == BLOCK_SNOW) { 
                        if (isTop) return (int)BLOCK_SNOW;
                        if (isBottom) return (int)BLOCK_DIRT;
                        return (int)BLOCK_SNOW_SIDE;
                    }
                    if (blockID == BLOCK_SNOW_LEAVES) {
                        if (isTop) return (int)BLOCK_SNOW;    // reuse snow top
                        if (isBottom) return (int)BLOCK_LEAVES; // reuse leaf bottom
                        return (int)BLOCK_SNOW_LEAVES;        // side with drip
                    }
                    return blockID;
                };

                float fFront[] = { gx, gy, gz + 1, gx + 1, gy, gz + 1, gx + 1, gy + 1, gz + 1, gx, gy, gz + 1, gx + 1, gy + 1, gz + 1, gx, gy + 1, gz + 1 };
                float uvFront[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };
                float fBack[] = { gx + 1, gy, gz, gx, gy, gz, gx, gy + 1, gz, gx + 1, gy, gz, gx, gy + 1, gz, gx + 1, gy + 1, gz };
                float uvBack[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };
                float fTop[] = { gx, gy + 1, gz + 1, gx + 1, gy + 1, gz + 1, gx + 1, gy + 1, gz, gx, gy + 1, gz + 1, gx + 1, gy + 1, gz, gx, gy + 1, gz };
                float uvTop[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };
                float fBottom[] = { gx, gy, gz, gx + 1, gy, gz, gx, gy, gz + 1, gx, gy, gz + 1, gx + 1, gy, gz, gx + 1, gy, gz + 1 };
                float uvBottom[] = { 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 0 };
                float fRight[] = { gx + 1, gy, gz + 1, gx + 1, gy, gz, gx + 1, gy + 1, gz, gx + 1, gy, gz + 1, gx + 1, gy + 1, gz, gx + 1, gy + 1, gz + 1 };
                float uvRight[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };
                float fLeft[] = { gx, gy, gz, gx, gy, gz + 1, gx, gy + 1, gz + 1, gx, gy, gz, gx, gy + 1, gz + 1, gx, gy + 1, gz };
                float uvLeft[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

                if (front)  pushFace(getRenderID(false, false), fFront, uvFront, getLight(x, y, z + 1));
                if (back)   pushFace(getRenderID(false, false), fBack, uvBack, getLight(x, y, z - 1));
                if (top)    pushFace(getRenderID(true, false), fTop, uvTop, getLight(x, y + 1, z));
                if (bottom) pushFace(getRenderID(false, true), fBottom, uvBottom, getLight(x, y - 1, z));
                if (right)  pushFace(getRenderID(false, false), fRight, uvRight, getLight(x + 1, y, z));
                if (left)   pushFace(getRenderID(false, false), fLeft, uvLeft, getLight(x - 1, y, z));
            }
        }
    }

    for (int i = 1; i <= 12; i++) {
        if (vertices[i].empty()) continue;
        Mesh mesh = { 0 };
        mesh.vertexCount = (int)vertices[i].size() / 3;
        mesh.triangleCount = mesh.vertexCount / 3;
        mesh.vertices = (float*)MemAlloc((unsigned int)(vertices[i].size() * sizeof(float)));
        mesh.texcoords = (float*)MemAlloc((unsigned int)(texcoords[i].size() * sizeof(float)));
        mesh.colors = (unsigned char*)MemAlloc((unsigned int)(colors[i].size() * sizeof(unsigned char)));
        memcpy(mesh.vertices, vertices[i].data(), vertices[i].size() * sizeof(float));
        memcpy(mesh.texcoords, texcoords[i].data(), texcoords[i].size() * sizeof(float));
        memcpy(mesh.colors, colors[i].data(), colors[i].size() * sizeof(unsigned char));
        UploadMesh(&mesh, false);
        chunk.layers[i] = LoadModelFromMesh(mesh);
        chunk.layers[i].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = textures[i];
    }
    chunk.meshReady = true;
}

void ChunkManager::UpdateAndDraw(Vector3 playerPos, Texture2D* textures, Shader shader, Color tint) {
    int playerCX = (int)floor(playerPos.x / CHUNK_SIZE);
    int playerCZ = (int)floor(playerPos.z / CHUNK_SIZE);
    
    for (int cx = playerCX - RENDER_DISTANCE; cx <= playerCX + RENDER_DISTANCE; cx++) {
        for (int cz = playerCZ - RENDER_DISTANCE; cz <= playerCZ + RENDER_DISTANCE; cz++) {
            ChunkCoord coord = { cx, cz };
            if (chunks.find(coord) == chunks.end()) {
                GenerateChunk(chunks[coord], cx, cz);
            }
            Chunk& chunk = chunks[coord];
            if (!chunk.meshReady) {
                BuildChunkMesh(chunk, cx, cz, textures);
            }
            for (int i = 1; i <= 12; i++) {
                if (chunk.layers[i].meshCount > 0) {
                    chunk.layers[i].materials[0].shader = shader;
                    DrawModel(chunk.layers[i], { 0,0,0 }, 1.0f, tint);
                }
            }
        }
    }
}

void ChunkManager::UpdateChunkPhysics() {
    for (auto& pair : chunks) {
        Chunk& chunk = pair.second;

        // the Sleep Check
        // if nothing is moving, skip checks
        if (!chunk.shouldStep) continue;

        bool moved = false;

        // scan loops
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                for (int y = 0; y < CHUNK_SIZE; y++) {

                    if (chunk.blocks[x][y][z] == BLOCK_SAND) {
                        if (y > 0) {
                            if (chunk.blocks[x][y - 1][z] == BLOCK_AIR) {
                                // swap
                                chunk.blocks[x][y - 1][z] = BLOCK_SAND;
                                chunk.blocks[x][y][z] = BLOCK_AIR;
                                moved = true;
                            }
                        }
                    }
                }
            }
        }

        if (moved) {
            chunk.meshReady = false;
            // it moved, so it might need to move again next frame. Keep awake.
            chunk.shouldStep = true;
        }
        else {
            // go to sleep
            // nothing moved this entire scan. stop checking until the player touches something.
            chunk.shouldStep = false;
        }
    }
}