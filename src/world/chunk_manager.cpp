#include "chunk_manager.h"
#include "world_generator.h"
#include "raymath.h"
#include "rlgl.h"
#include "../blocks/block_types.h"
#include <cmath>
#include <vector>
#include <cstring>
#include <queue>

void ChunkManager::Init() {}

void ChunkManager::UnloadAll() {
    for (auto& pair : chunks) {
        UnloadChunkModels(pair.second);
    }
    chunks.clear();
}

void ChunkManager::UnloadChunkModels(Chunk& chunk) {
    for (int i = 0; i < 15; i++) {
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

    // update the Block
    chunks[coord].blocks[lx][y][lz] = type;

    // RECALCULATE LIGHTING
    // Now that a block changed (maybe a torch placed, or stone broken opening a skylight),
    // re-run the BFS to spread the light.
    ComputeChunkLighting(chunks[coord]);

    // rebuild Mesh
    chunks[coord].meshReady = false;
    chunks[coord].shouldStep = true;

    // TODO MAYBE OPTIONAL: If we are on the edge of a chunk, we should technically 
    // update the neighbor chunks too, but lets stick to this for now 
    // to keep performance high.
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
    // CLEAR LIGHTING (Reset to 0)
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int y = 0; y < CHUNK_SIZE; y++) {
            for (int z = 0; z < CHUNK_SIZE; z++) {
                chunk.light[x][y][z] = 0;
            }
        }
    }

    std::queue<LightNode> sunQueue;
    std::queue<LightNode> torchQueue;

    // INITIALIZE SOURCES
    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {

            // SUNLIGHT (Column Scan)
            bool sunBlocked = false;
            for (int y = CHUNK_SIZE - 1; y >= 0; y--) {
                int block = chunk.blocks[x][y][z];
                // Light passes through Air, Leaves, and Torches
                bool solid = (block != 0 && block != 7 && block != 12 && block != 13);

                if (!sunBlocked) {
                    if (solid) {
                        sunBlocked = true;
                    }
                    else {
                        // Set Sun Bit (High Nibble) to 15 -> (15 << 4) = 240
                        chunk.light[x][y][z] |= (15 << 4);
                        sunQueue.push({ x, y, z, 15 });
                    }
                }
            }

            // TORCHLIGHT (Scan for emitters)
            for (int y = 0; y < CHUNK_SIZE; y++) {
                int block = chunk.blocks[x][y][z];
                if (block == BLOCK_TORCH || block == BLOCK_GLOWSTONE) {
                    // Set Torch Bit (Low Nibble) to 14 (Torches aren't fully 15 bright usually)
                    chunk.light[x][y][z] |= 14;
                    torchQueue.push({ x, y, z, 14 });
                }
            }
        }
    }

    // SPREAD SUNLIGHT (BFS)
    while (!sunQueue.empty()) {
        LightNode node = sunQueue.front();
        sunQueue.pop();

        int neighbors[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

        for (int i = 0; i < 6; i++) {
            int nx = node.x + neighbors[i][0];
            int ny = node.y + neighbors[i][1];
            int nz = node.z + neighbors[i][2];

            if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                int block = chunk.blocks[nx][ny][nz];
                bool solid = (block != 0 && block != 7 && block != 12 && block != 13);

                if (!solid) {
                    int currentSun = (chunk.light[nx][ny][nz] >> 4) & 0xF;
                    if (currentSun < node.val - 1) {
                        int newSun = node.val - 1;
                        // Write back sun (preserve existing torch)
                        int existingTorch = chunk.light[nx][ny][nz] & 0xF;
                        chunk.light[nx][ny][nz] = (unsigned char)((newSun << 4) | existingTorch);
                        sunQueue.push({ nx, ny, nz, newSun });
                    }
                }
            }
        }
    }

    // SPREAD TORCHLIGHT (BFS)
    while (!torchQueue.empty()) {
        LightNode node = torchQueue.front();
        torchQueue.pop();

        int neighbors[6][3] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };

        for (int i = 0; i < 6; i++) {
            int nx = node.x + neighbors[i][0];
            int ny = node.y + neighbors[i][1];
            int nz = node.z + neighbors[i][2];

            if (nx >= 0 && nx < CHUNK_SIZE && ny >= 0 && ny < CHUNK_SIZE && nz >= 0 && nz < CHUNK_SIZE) {
                int block = chunk.blocks[nx][ny][nz];
                bool solid = (block != 0 && block != 7 && block != 12 && block != 13);

                if (!solid) {
                    int currentTorch = chunk.light[nx][ny][nz] & 0xF;
                    if (currentTorch < node.val - 1) {
                        int newTorch = node.val - 1;
                        // Write back torch (preserve existing sun)
                        int existingSun = chunk.light[nx][ny][nz] & 0xF0;
                        chunk.light[nx][ny][nz] = (unsigned char)(existingSun | newTorch);
                        torchQueue.push({ nx, ny, nz, newTorch });
                    }
                }
            }
        }
    }
}

void ChunkManager::BuildChunkMesh(Chunk& chunk, int cx, int cz, Texture2D* textures) {
    UnloadChunkModels(chunk);

    std::vector<float> vertices[15];
    std::vector<float> texcoords[15];
    std::vector<unsigned char> colors[15];

    auto getLight = [&](int x, int y, int z) {
        // convert local chunk coords to global world coords
        int globalX = (cx * CHUNK_SIZE) + x;
        int globalY = y;
        int globalZ = (cz * CHUNK_SIZE) + z;

        // find real light level
        return GetLightLevel(globalX, globalY, globalZ);
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

                    // DECODE LIGHT (Packed Byte)
                    int sun = (lightLevel >> 4) & 0xF; // High 4 bits
                    int torch = lightLevel & 0xF;      // Low 4 bits

                    // convert 0-15 int to 0-255 byte
                    int r = (int)((float)sun / 15.0f * 255.0f);
                    int g = (int)((float)torch / 15.0f * 255.0f);

                    // PACK INTO COLOR
                    // R = Sun Level
                    // G = Torch Level
                    // B = 0 (Unused for now)
                    // A = 255
                    Color c = { (unsigned char)r, (unsigned char)g, 0, 255 };

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

    for (int i = 1; i <= 14; i++) {
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
            for (int i = 1; i <= 14; i++) {
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

int ChunkManager::GetLightLevel(int x, int y, int z) {
    if (y < 0 || y >= CHUNK_SIZE) return 15; // Sky is 15

    int cx = (int)floor((float)x / CHUNK_SIZE);
    int cz = (int)floor((float)z / CHUNK_SIZE);
    ChunkCoord coord = { cx, cz };

    // If chunk doesn't exist, return 15 (Sun) or 0 (Darkness)
    // returning 0 is safer for preventing underground grid lines
    if (chunks.find(coord) == chunks.end()) return 0;

    int lx = ((x % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;
    int lz = ((z % CHUNK_SIZE) + CHUNK_SIZE) % CHUNK_SIZE;

    return (int)chunks[coord].light[lx][y][lz];
}