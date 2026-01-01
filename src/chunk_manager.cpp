#include "chunk_manager.h"
#include "raymath.h"
#include "rlgl.h"
#include "world_gen.h"
#include <cmath>
#include <vector>

void ChunkManager::Init() {}

void ChunkManager::UnloadAll() {
	for (auto& pair : chunks) {
		UnloadChunkModels(pair.second);
	}
	chunks.clear();
}

void ChunkManager::UnloadChunkModels(Chunk& chunk) {
	for (int i = 0; i < 12; i++) {
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
}

bool ChunkManager::IsBlockSolid(int x, int y, int z) {
	return GetBlock(x, y, z) != 0;
}

void ChunkManager::ComputeChunkLighting(Chunk& chunk) {
	// collum lighting alg
	for (int x = 0; x < CHUNK_SIZE; x++) {
		for (int z = 0; z < CHUNK_SIZE; z++) {

			bool inShadow = false;

			// scan from top to bottom
			for (int y = CHUNK_SIZE - 1; y >= 0; y--) {

				// apply Light based on what is ABOVE us
				if (inShadow) {
					chunk.light[x][y][z] = 0; // Darkness
				}
				else {
					chunk.light[x][y][z] = 15; // Sunlight
				}

				// check if THIS block blocks light for the one below it
				int block = chunk.blocks[x][y][z];
				if (block != 0 && block != 7) { // 7 = Leaves (transparent-ish)
					inShadow = true;
				}
			}
		}
	}
}

// --- MESH GENERATION ---
void ChunkManager::BuildChunkMesh(Chunk& chunk, int cx, int cz, Texture2D* textures) {
	UnloadChunkModels(chunk);

	std::vector<float> vertices[12];
	std::vector<float> texcoords[12];
	std::vector<unsigned char> colors[12];

	// helper to get light level safely
	auto getLight = [&](int x, int y, int z) {
		// if neighbor is outside this chunk, assume it's bright (Sunlight)
		// fixes the "Black Lines" at chunk borders
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

				// culling Checks
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

					// convert Light Level (0-15) to Brightness (60-255)
					int b = (int)((float)lightLevel / 15.0f * 255.0f);
					if (b < 60) b = 60; // shadows shouldnt be pitch black
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
					if (blockID == 4) { // Grass
						if (isTop) return 4;
						if (isBottom) return 1;
						return 8;
					}
					if (blockID == 9) { // Snow
						if (isTop) return 9;
						if (isBottom) return 1;
						return 11;
					}
					return blockID;
					};

				float fFront[] = { gx, gy, gz + 1, gx + 1, gy, gz + 1, gx + 1, gy + 1, gz + 1, gx, gy, gz + 1, gx + 1, gy + 1, gz + 1, gx, gy + 1, gz + 1 };
				float uvFront[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

				float fBack[] = { gx + 1, gy, gz, gx, gy, gz, gx, gy + 1, gz, gx + 1, gy, gz, gx, gy + 1, gz, gx + 1, gy + 1, gz };
				float uvBack[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

				float fTop[] = { gx, gy + 1, gz + 1, gx + 1, gy + 1, gz + 1, gx + 1, gy + 1, gz, gx, gy + 1, gz + 1, gx + 1, gy + 1, gz, gx, gy + 1, gz };
				float uvTop[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

				float fBottom[] = { gx, gy, gz, gx + 1, gy, gz, gx, gy, gz + 1,   gx, gy, gz + 1, gx + 1, gy, gz, gx + 1, gy, gz + 1 };
				float uvBottom[] = { 0, 1, 1, 1, 0, 0,   0, 0, 1, 1, 1, 0 };

				float fRight[] = { gx + 1, gy, gz + 1, gx + 1, gy, gz, gx + 1, gy + 1, gz, gx + 1, gy, gz + 1, gx + 1, gy + 1, gz, gx + 1, gy + 1, gz + 1 };
				float uvRight[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

				float fLeft[] = { gx, gy, gz, gx, gy, gz + 1, gx, gy + 1, gz + 1, gx, gy, gz, gx, gy + 1, gz + 1, gx, gy + 1, gz };
				float uvLeft[] = { 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0 };

				// use Neighbors Light Level for each face
				if (front)  pushFace(getRenderID(false, false), fFront, uvFront, getLight(x, y, z + 1));
				if (back)   pushFace(getRenderID(false, false), fBack, uvBack, getLight(x, y, z - 1));
				if (top)    pushFace(getRenderID(true, false), fTop, uvTop, getLight(x, y + 1, z));
				if (bottom) pushFace(getRenderID(false, true), fBottom, uvBottom, getLight(x, y - 1, z));
				if (right)  pushFace(getRenderID(false, false), fRight, uvRight, getLight(x + 1, y, z));
				if (left)   pushFace(getRenderID(false, false), fLeft, uvLeft, getLight(x - 1, y, z));
			}
		}
	}

	// upload Meshes
	for (int i = 1; i <= 11; i++) {
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

// --- UPDATE AND DRAW ---
void ChunkManager::UpdateAndDraw(Vector3 playerPos, Texture2D* textures, Shader shader, Color tint) {
	int playerCX = (int)floor(playerPos.x / CHUNK_SIZE);
	int playerCZ = (int)floor(playerPos.z / CHUNK_SIZE);
	int renderDist = 12;

	for (int cx = playerCX - renderDist; cx <= playerCX + renderDist; cx++) {
		for (int cz = playerCZ - renderDist; cz <= playerCZ + renderDist; cz++) {
			ChunkCoord coord = { cx, cz };
			if (chunks.find(coord) == chunks.end()) {
				GenerateChunk(chunks[coord], cx, cz);
			}
			Chunk& chunk = chunks[coord];
			if (!chunk.meshReady) {
				BuildChunkMesh(chunk, cx, cz, textures);
			}
			for (int i = 1; i <= 11; i++) {
				if (chunk.layers[i].meshCount > 0) {
					chunk.layers[i].materials[0].shader = shader;
					DrawModel(chunk.layers[i], { 0,0,0 }, 1.0f, tint);
				}
			}
		}
	}
}

// --- TREE GENERATION ---
void ChunkManager::CreateTree(Chunk& chunk, int x, int y, int z)
{
	int height = GetRandomValue(4, 6);

	// Trunk
	for (int i = 0; i < height; i++) {
		if (y + i < CHUNK_SIZE) chunk.blocks[x][y + i][z] = 3;
	}

	// Leaves
	int leafStart = height - 2;
	int leafEnd = height + 1;

	for (int ly = leafStart; ly <= leafEnd; ly++) {
		int radius = (ly == leafEnd) ? 1 : 2;
		for (int lx = -radius; lx <= radius; lx++) {
			for (int lz = -radius; lz <= radius; lz++) {
				if (abs(lx) == radius && abs(lz) == radius && (ly != leafEnd)) continue;
				int finalX = x + lx;
				int finalY = y + ly;
				int finalZ = z + lz;

				if (finalX >= 0 && finalX < CHUNK_SIZE &&
					finalY >= 0 && finalY < CHUNK_SIZE &&
					finalZ >= 0 && finalZ < CHUNK_SIZE)
				{
					if (chunk.blocks[finalX][finalY][finalZ] == 0) {
						chunk.blocks[finalX][finalY][finalZ] = 7;
					}
				}
			}
		}
	}
}

// --- GENERATION (NOW WITH CAVES) ---
void ChunkManager::GenerateChunk(Chunk& chunk, int chunkX, int chunkZ) {
	WorldGen::GenerateChunk(chunk, chunkX, chunkZ);
	ComputeChunkLighting(chunk);
}
