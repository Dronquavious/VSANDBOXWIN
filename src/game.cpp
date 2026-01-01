#include "game.h"
#include "raymath.h"
#include <cmath>
#include <ctime>
#include "rlgl.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define RAYGUI_IMPLEMENTATION
#include "raygui.h"

// SHADER CODE (GLSL 330)
const char* fogVsCode = R"(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec4 vertexColor;
in vec3 vertexNormal;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matView;

out vec2 fragTexCoord;
out vec4 fragColor;
out float fragDist;

void main()
{
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    
    // Calculate distance from camera to vertex
    vec4 viewPos = matView * matModel * vec4(vertexPosition, 1.0);
    fragDist = length(viewPos.xyz);
    
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

const char* fogFsCode = R"(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in float fragDist;

out vec4 finalColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Fog Uniforms
uniform float fogDensity;
uniform vec3 fogColor;

void main()
{
    vec4 texColor = texture(texture0, fragTexCoord);
    vec4 baseColor = texColor * colDiffuse * fragColor;
    
    // create a "Clear Zone" of 70 units around the player
    // If distance is less than 70, fog is 0.
    float dist = max(fragDist - 70.0, 0.0);
    
    // Exponential Fog Calculation
    // We use the new 'dist' variable instead of 'fragDist'
    float fogFactor = 1.0 / exp(pow(dist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    
    // Mix the Block color with the Fog/Sky color
    finalColor = mix(vec4(fogColor, 1.0), baseColor, fogFactor);
}
)";

void Game::Init()
{
	// variables init
	handBobbing = 0.0f;
	playerPosition = Vector3{ 16.0f, 20.0f, 16.0f };
	cameraAngleX = 0.0f;
	cameraAngleY = 0.0f;
	isFlying = true;
	verticalVelocity = 0.0f;

	// INVENTORY
	inventory.selectedSlot = 0;
	inventory.slots[0] = { 1, 64 };  // Dirt
	inventory.slots[1] = { 2, 64 };  // Stone
	inventory.slots[2] = { 3, 64 };  // Wood
	inventory.slots[3] = { 4, 64 };  // Grass
	inventory.slots[4] = { 5, 64 };  // Sand
	inventory.slots[5] = { 9, 64 };  // Snow
	inventory.slots[6] = { 10, 64 }; // Cactus
	inventory.slots[7] = { 7, 64 };  // Leaves
	inventory.slots[8] = { 6, 64 };  // Bedrock

	// physics Settings
	showDebugUI = true;
	gravity = 0.015f;
	jumpForce = 0.25f;
	moveSpeed = 4.0f;
	daySpeed = 0.005f;
	timeOfDay = 0.0f;
	flySpeed = 10.0f;
	timeMode = 0;

	// setup Camera
	camera.position = Vector3{ 0.0f, 10.0f, 10.0f };
	camera.target = Vector3{ 0.0f, 0.0f, 0.0f };
	camera.up = Vector3{ 0.0f, 1.0f, 0.0f };
	camera.fovy = 45.0f;
	camera.projection = CAMERA_PERSPECTIVE;

	// load Assets
	texDirt = GenDirtTexture(BLOCK_TEX_SIZE);
	texStone = GenStoneTexture(BLOCK_TEX_SIZE);
	texWood = GenWoodTexture(BLOCK_TEX_SIZE);
	texGrass = GenGrassTexture(BLOCK_TEX_SIZE);
	texGrassSide = GenGrassSideTexture(BLOCK_TEX_SIZE);
	texSand = GenSandTexture(BLOCK_TEX_SIZE);
	texBedrock = GenBedrockTexture(BLOCK_TEX_SIZE);
	texLeaves = GenLeavesTexture(BLOCK_TEX_SIZE);
	texSnow = GenSnowTexture(BLOCK_TEX_SIZE);
	texSnowSide = GenSnowSideTexture(BLOCK_TEX_SIZE);
	texCactus = GenCactusTexture(BLOCK_TEX_SIZE);

	Mesh mesh = GenMeshCube(1.0f, 1.0f, 1.0f);
	blockModel = LoadModelFromMesh(mesh);

	// input Setup
	DisableCursor();
	HideCursor(); // Optional: Sometimes annoying during debug
	showDebugUI = false;
	messageTimer = 0.0f;
	messageText = "";

	// loading fog shader
	fogShader = LoadShaderFromMemory(fogVsCode, fogFsCode);

	// get locations of the variables inside the shader
	fogDensityLoc = GetShaderLocation(fogShader, "fogDensity");
	fogColorLoc = GetShaderLocation(fogShader, "fogColor");

	// default density (How thick the fog is)
	// Lower = Clearer, Higher = Thicker
	float density = 0.005f;
	SetShaderValue(fogShader, fogDensityLoc, &density, SHADER_UNIFORM_FLOAT);


	Mesh skyMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
	skyModel = LoadModelFromMesh(skyMesh);

	// skybox tex
	Texture2D skyTex = GenerateSkyTexture();
	skyModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = skyTex;
	SetTextureWrap(skyTex, TEXTURE_WRAP_CLAMP);

	// cloud layer
	Mesh cloudMesh = GenMeshPlane(2000.0f, 2000.0f, 1, 1);

	// scale UVs so the texture repeats many times across this giant plane
	// creates the density of the clouds
	for (int i = 0; i < cloudMesh.vertexCount; i++) {
		cloudMesh.texcoords[i * 2] *= 8.0f;     // Repeat 8 times on X
		cloudMesh.texcoords[i * 2 + 1] *= 8.0f; // Repeat 8 times on Y
	}

	cloudModel = LoadModelFromMesh(cloudMesh);
	texClouds = GenerateCloudTexture();
	cloudModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texClouds;
	cloudScroll = 0.0f;

	// haze layer
	Mesh hazeMesh = GenMeshCylinder(390.0f, 200.0f, 16);
	hazeModel = LoadModelFromMesh(hazeMesh);
	texHaze = GenerateHazeTexture();
	hazeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texHaze;

	world.Init();
}

void Game::update()
{

	// --- time of day logic ---
	if (timeMode == 0) // cycle
	{
		timeOfDay += GetFrameTime() * daySpeed;
		if (timeOfDay > 1.0f) timeOfDay = 0.0f;
	}
	else if (timeMode == 1) // always Day
	{

		timeOfDay = 0.5f; // noon (Brightest Blue)
	}
	else if (timeMode == 2) // always Night
	{
		timeOfDay = 0.0f; // midnight (Darkest)
	}


	// Update Message Timer
	if (messageTimer > 0.0f)
	{
		messageTimer -= GetFrameTime();
	}

	timeOfDay += GetFrameTime() * daySpeed;
	if (timeOfDay > 1.0f)
		timeOfDay = 0.0f;

	// toggle DEBUG Menu
	if (IsKeyPressed(KEY_TAB))
	{
		showDebugUI = !showDebugUI;

		if (showDebugUI)
		{
			EnableCursor(); // unlock it once
		}
		else
		{
			DisableCursor(); // lock it once
		}
	}

	// the "Click to Focus" Fix
	// if we are in Game Mode and you click the screen, we force a re-lock.
	// fixes the "mouse moved off screen" bug without spamming the OS.
	if (!showDebugUI && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
	{
		DisableCursor();
	}

	// logic Split
	if (showDebugUI)
	{
		// --- MENU MODE ---
		// (Do nothing, let the mouse be free)
	}
	else
	{
		// --- SAVE / LOAD INPUTS ---
		if (IsKeyPressed(KEY_P))
			SaveMap();
		if (IsKeyPressed(KEY_L))
			LoadMap();

		// --- GAME MODE ---
		UpdatePlayer();
		UpdateRaycast();
		EditMap();
	}
}

void Game::draw()
{
	// brightness based on time
	float brightness = 1.0f;
	if (timeOfDay > 0.5f) {
		float t = (timeOfDay - 0.5f) * 2.0f;
		brightness = 1.0f - t;
	}
	else {
		float t = timeOfDay * 2.0f;
		brightness = t;
	}
	if (brightness < 0.1f) brightness = 0.1f;

	Color skyTint = {
		(unsigned char)(255 * brightness),
		(unsigned char)(255 * brightness),
		(unsigned char)(255 * brightness),
		255
	};

	// animate clouds
	cloudScroll += GetFrameTime() * 0.005f;
	if (cloudScroll > 1000.0f) cloudScroll = 0.0f;

	BeginDrawing();
	ClearBackground(BLACK);

	BeginMode3D(camera);

	rlDisableDepthMask();
	rlDisableDepthTest();
	rlDisableBackfaceCulling(); // allow inside of skybox to be visible

	Vector3 skyScale = { -800.0f, 800.0f, 800.0f };
	DrawModelEx(skyModel, camera.position, Vector3{ 0.0f, 1.0f, 0.0f }, 0.0f, skyScale, skyTint);

	// sraw sun & moon (they should be visible regardless)
	float orbitRadius = 400.0f;
	float angle = (timeOfDay * 2.0f * PI) - (PI / 2.0f);
	Vector3 sunPos = { camera.position.x + cosf(angle) * orbitRadius, camera.position.y + sinf(angle) * orbitRadius, camera.position.z };
	Vector3 moonPos = { camera.position.x - cosf(angle) * orbitRadius, camera.position.y - sinf(angle) * orbitRadius, camera.position.z };
	DrawSphere(sunPos, 40.0f, Color{ 255,255,200,255 });
	DrawSphere(moonPos, 20.0f, Color{ 220,220,220,255 });

	// re-enable depth TEST before drawing any alpha layers so they are occluded by nearer geometry
	rlEnableDepthTest();

	// clouds (alpha blended dome/plane above the camera)
	// keep depth TEST on, but disable DEPTH WRITE so clouds don't block world geometry
	BeginBlendMode(BLEND_ALPHA);
	rlDisableDepthMask();

	// make clouds always centered on camera (prevents slicing and seams)
	Vector3 camPos = camera.position;
	Vector3 cloudPos = camPos;
	cloudPos.y = camPos.y + 80.0f; // relative height above camera

	// simulate movement by rotating the cloud mesh instead of texture-matrix
	static float cloudAngle = 0.0f;
	cloudAngle += GetFrameTime() * 2.0f; // degrees/sec
	if (cloudAngle > 360.0f) cloudAngle -= 360.0f;

	float sunHeight = sinf(angle); // -1 = night, +1 = noon
	float dayFactor = Clamp((sunHeight + 0.2f) / 1.2f, 0.0f, 1.0f);
	Color cloudDay = Color{ 240, 240, 240, 255 };
	Color cloudNight = Color{ 80, 90, 110, 255 };

	Color cloudTint = LerpColor(cloudNight, cloudDay, dayFactor);

	DrawModelEx(cloudModel, cloudPos, Vector3{ 0, 1, 0 }, cloudAngle, Vector3{ 1, 1, 1 }, Fade(cloudTint, 0.9f));

	rlEnableDepthMask();
	EndBlendMode();

	// haze
	BeginBlendMode(BLEND_ALPHA);
	rlDisableDepthMask();

	Color skyDay = Color{ 135, 180, 235, 255 };
	Color skyNight = Color{ 10, 15, 30, 255 };

	Color skyColor = LerpColor(skyNight, skyDay, dayFactor);

	Vector3 hazePos = camPos;
	hazePos.y = camPos.y + 50.0f;
	DrawModelEx( hazeModel, hazePos, Vector3 {0, 1, 0}, 0.0f, Vector3 {1, 1, 1}, Fade(skyColor, 0.9f));

	rlEnableDepthMask();
	EndBlendMode();

	// restore culling for world
	rlEnableBackfaceCulling();

	// --- WORLD RENDER SECTION ---

	// Update Fog Shader Variables
	float fogColor[3] = {
	skyColor.r / 255.0f,
	skyColor.g / 255.0f,
	skyColor.b / 255.0f
	};

	SetShaderValue(fogShader, fogColorLoc, fogColor, SHADER_UNIFORM_VEC3);

	Texture2D textures[12];
	textures[1] = texDirt;
	textures[2] = texStone;
	textures[3] = texWood;
	textures[4] = texGrass;
	textures[5] = texSand;
	textures[6] = texBedrock;
	textures[7] = texLeaves;
	textures[8] = texGrassSide;
	textures[9] = texSnow;
	textures[10] = texCactus;
	textures[11] = texSnowSide;

	world.UpdateAndDraw(playerPosition, textures, fogShader, skyTint);

	// Highlight Box
	if (isBlockSelected)
	{
		Vector3 center = {
			selectedBlockPos.x + 0.5f,
			selectedBlockPos.y + 0.5f,
			selectedBlockPos.z + 0.5f
		};
		DrawCubeWires(center, 1.01f, 1.01f, 1.01f, BLACK);
	}

	DrawHand(skyTint);

	EndMode3D();

	DrawUI();
	DrawDebug();

	EndDrawing();
}

void Game::shutDown()
{
	UnloadTexture(texDirt);
	UnloadTexture(texStone);
	UnloadTexture(texWood);
	UnloadTexture(texGrass);
	UnloadModel(blockModel);
	UnloadModel(skyModel);
	UnloadTexture(texSand);
	UnloadTexture(texBedrock);
	UnloadTexture(texLeaves);
	UnloadTexture(texGrassSide);
	UnloadTexture(texSnow);
	UnloadTexture(texCactus);
	UnloadTexture(texSnowSide);
	UnloadShader(fogShader);
	UnloadTexture(texClouds);
	UnloadTexture(texHaze);
	UnloadModel(cloudModel);
	UnloadModel(hazeModel);
	world.UnloadAll();
}

// --- PRIVATE HELPER FUNCTIONS ---

bool Game::IsPositionSolid(Vector3 pos)
{
	int x = (int)round(pos.x);
	int y = (int)round(pos.y);
	int z = (int)round(pos.z);

	if (x < 0 || x >= CHUNK_SIZE)
		return false;
	if (y < 0 || y >= CHUNK_SIZE)
		return false;
	if (z < 0 || z >= CHUNK_SIZE)
		return false;

	return world.GetBlock(x, y, z) != 0;
}

void Game::UpdatePlayer()
{
	float dt = GetFrameTime();

	// FIX: Clamp Delta Time
	// If the game lags, we pretend only 1/20th of a second passed.
	// prevents you from falling through the floor during lag spikes.
	if (dt > 0.05f) dt = 0.05f;

	// --- INPUT & CAMERA ---
	Vector2 mouseDelta = GetMouseDelta();
	cameraAngleX -= mouseDelta.x * 0.003f;
	cameraAngleY -= mouseDelta.y * 0.003f;
	if (cameraAngleY > 1.5f) cameraAngleY = 1.5f;
	if (cameraAngleY < -1.5f) cameraAngleY = -1.5f;

	forward = { sinf(cameraAngleX), 0.0f, cosf(cameraAngleX) };
	right = { cosf(cameraAngleX), 0.0f, -sinf(cameraAngleX) };

	float baseSpeed = isFlying ? flySpeed : moveSpeed;
	if (IsKeyDown(KEY_LEFT_SHIFT)) baseSpeed *= 2.0f;
	float step = baseSpeed * dt;

	Vector3 moveVec = { 0,0,0 };
	if (IsKeyDown(KEY_W)) { moveVec.x += forward.x; moveVec.z += forward.z; }
	if (IsKeyDown(KEY_S)) { moveVec.x -= forward.x; moveVec.z -= forward.z; }
	if (IsKeyDown(KEY_D)) { moveVec.x -= right.x; moveVec.z -= right.z; }
	if (IsKeyDown(KEY_A)) { moveVec.x += right.x; moveVec.z += right.z; }

	if (Vector3Length(moveVec) > 0) {
		moveVec = Vector3Normalize(moveVec);
		moveVec = Vector3Scale(moveVec, step);
	}

	if (IsKeyPressed(KEY_F)) isFlying = !isFlying;

	// --- PHYSICS ENGINE ---
	if (isFlying)
	{
		playerPosition.x += moveVec.x;
		playerPosition.z += moveVec.z;
		if (IsKeyDown(KEY_SPACE)) playerPosition.y += step;
		if (IsKeyDown(KEY_LEFT_CONTROL)) playerPosition.y -= step;
		verticalVelocity = 0.0f;
	}
	else
	{
		// gravity (Clamped)
		float dtScale = dt * 150.0f;
		verticalVelocity -= gravity * dtScale;
		if (verticalVelocity < -0.5f) verticalVelocity = -0.5f;

		// apply Y Movement
		playerPosition.y += verticalVelocity * dtScale;

		// ground Collision
		float playerHeight = 1.5f;
		float playerWidth = 0.3f;

		int footX = (int)floor(playerPosition.x);
		int footZ = (int)floor(playerPosition.z);
		int blockBelowY = (int)floor(playerPosition.y - playerHeight - 0.1f);

		if (verticalVelocity <= 0.0f)
		{
			// check multiple positions under the player (footprint)
			bool onGround = false;
			float lowestY = playerPosition.y - playerHeight;

			// check a 2x2 area under the player
			for (int dx = 0; dx <= 1; dx++) {
				for (int dz = 0; dz <= 1; dz++) {
					int checkX = (int)floor(playerPosition.x + (dx * 0.5f));
					int checkZ = (int)floor(playerPosition.z + (dz * 0.5f));
					int checkY = (int)floor(lowestY - 0.1f);

					if (world.GetBlock(checkX, checkY, checkZ) != 0) {
						float blockTop = (float)checkY + 1.0f;
						if (lowestY - 0.1f < blockTop) {
							playerPosition.y = blockTop + playerHeight;
							onGround = true;
						}
					}
				}
			}

			if (onGround) {
				verticalVelocity = 0.0f;
				if (IsKeyDown(KEY_SPACE)) verticalVelocity = jumpForce;
			}
		}

		// horizontal Collision
		Vector3 testPos = playerPosition;
		testPos.x += moveVec.x;

		int wallX = (int)floor(testPos.x + (moveVec.x > 0 ? playerWidth : -playerWidth));
		int kneeY = (int)floor(playerPosition.y - 1.0f);
		int headY = (int)floor(playerPosition.y - 0.2f);

		if (world.GetBlock(wallX, kneeY, footZ) == 0 &&
			world.GetBlock(wallX, headY, footZ) == 0) {
			playerPosition.x += moveVec.x;
		}

		testPos = playerPosition;
		testPos.z += moveVec.z;
		int wallZ = (int)floor(testPos.z + (moveVec.z > 0 ? playerWidth : -playerWidth));
		int currentFootX = (int)floor(playerPosition.x);

		if (world.GetBlock(currentFootX, kneeY, wallZ) == 0 &&
			world.GetBlock(currentFootX, headY, wallZ) == 0) {
			playerPosition.z += moveVec.z;
		}
	}

	// update Camera
	camera.position = playerPosition;
	Vector3 lookDir = {
		sinf(cameraAngleX) * cosf(cameraAngleY),
		sinf(cameraAngleY),
		cosf(cameraAngleX) * cosf(cameraAngleY)
	};
	camera.target.x = camera.position.x + lookDir.x;
	camera.target.y = camera.position.y + lookDir.y;
	camera.target.z = camera.position.z + lookDir.z;
}

void Game::UpdateRaycast()
{
	isBlockSelected = false;
	Ray ray = GetMouseRay(Vector2{ (float)GetScreenWidth() / 2, (float)GetScreenHeight() / 2 }, camera);
	float closestDist = 8.0f;

	int radius = 6;
	int camX = (int)floor(camera.position.x);
	int camY = (int)floor(camera.position.y);
	int camZ = (int)floor(camera.position.z);

	for (int x = camX - radius; x <= camX + radius; x++)
	{
		for (int y = camY - radius; y <= camY + radius; y++)
		{
			for (int z = camZ - radius; z <= camZ + radius; z++)
			{
				if (world.GetBlock(x, y, z, false) == 0) continue;

				Vector3 min = { (float)x, (float)y, (float)z };
				Vector3 max = { (float)x + 1.0f, (float)y + 1.0f, (float)z + 1.0f };
				BoundingBox box = { min, max };

				RayCollision collision = GetRayCollisionBox(ray, box);
				if (collision.hit && collision.distance < closestDist)
				{
					isBlockSelected = true;
					closestDist = collision.distance;
					selectedBlockPos = Vector3{ (float)x, (float)y, (float)z };
					selectedNormal = collision.normal;
				}
			}
		}
	}
}

void Game::EditMap()
{
	// select Hotbar Slots with 1-9
	if (IsKeyPressed(KEY_ONE))   inventory.selectedSlot = 0;
	if (IsKeyPressed(KEY_TWO))   inventory.selectedSlot = 1;
	if (IsKeyPressed(KEY_THREE)) inventory.selectedSlot = 2;
	if (IsKeyPressed(KEY_FOUR))  inventory.selectedSlot = 3;
	if (IsKeyPressed(KEY_FIVE))  inventory.selectedSlot = 4;
	if (IsKeyPressed(KEY_SIX))   inventory.selectedSlot = 5;
	if (IsKeyPressed(KEY_SEVEN)) inventory.selectedSlot = 6;
	if (IsKeyPressed(KEY_EIGHT)) inventory.selectedSlot = 7;
	if (IsKeyPressed(KEY_NINE))  inventory.selectedSlot = 8;

	// mouse Wheel to scroll hotbar
	float wheel = GetMouseWheelMove();
	if (wheel > 0) inventory.selectedSlot--;
	if (wheel < 0) inventory.selectedSlot++;

	// wrap around
	if (inventory.selectedSlot < 0) inventory.selectedSlot = 8;
	if (inventory.selectedSlot > 8) inventory.selectedSlot = 0;

	int currentID = GetHeldBlockID(); // Use helper

	if (isBlockSelected)
	{
		if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
		{
			world.SetBlock((int)selectedBlockPos.x, (int)selectedBlockPos.y, (int)selectedBlockPos.z, 0);
			UpdateRaycast();
		}

		if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
		{
			// Only place if we are holding a valid block
			if (currentID != 0)
			{
				int newX = (int)(selectedBlockPos.x + round(selectedNormal.x));
				int newY = (int)(selectedBlockPos.y + round(selectedNormal.y));
				int newZ = (int)(selectedBlockPos.z + round(selectedNormal.z));

				Vector3 blockCenter = { (float)newX + 0.5f, (float)newY + 0.5f, (float)newZ + 0.5f };
				if (Vector3Distance(camera.position, blockCenter) > 1.2f)
				{
					world.SetBlock(newX, newY, newZ, currentID);
					UpdateRaycast();
				}
			}
		}
	}
}

void Game::DrawHand(Color tint)
{
	bool isMoving = (IsKeyDown(KEY_W) || IsKeyDown(KEY_S) || IsKeyDown(KEY_A) || IsKeyDown(KEY_D));
	if (isMoving)
		handBobbing += GetFrameTime() * 10.0f;
	else
		handBobbing = Lerp(handBobbing, 0.0f, 0.1f);

	float bobOffset = sinf(handBobbing) * 0.1f;

	Vector3 handPos = camera.position;
	handPos.x += forward.x * 0.8f;
	handPos.z += forward.z * 0.8f;
	handPos.x -= right.x * 0.5f;
	handPos.z -= right.z * 0.5f;
	handPos.y -= 0.4f;
	handPos.y += bobOffset;

	int currentID = GetHeldBlockID();

	Texture2D handTexture;
	switch (currentID)
	{
	case 1: handTexture = texDirt; break;
	case 2: handTexture = texStone; break;
	case 3: handTexture = texWood; break;
	case 4: handTexture = texGrass; break;
	case 5: handTexture = texSand; break;
	case 6: handTexture = texBedrock; break;
	case 7: handTexture = texLeaves; break;
	case 9: handTexture = texSnow; break;
	case 10: handTexture = texCactus; break;
	default: handTexture = texDirt; break;
	}
	blockModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = handTexture;

	// TODO:  FIX FOR SPECIFICALLY THE GRASS
	// since the texture looks upside down, we just rotate the block 180 degrees (PI) around the Z-axis.
	// this flips it visually without messing with UV math.

	Vector3 scale = { 0.4f, 0.4f, 0.4f };
	Vector3 rotationAxis = { 0.0f, 0.0f, 1.0f }; // rotate around Z axis (Roll)
	float rotationAngle = 180.0f;                // flip 180 degrees

	DrawModelEx(blockModel, handPos, rotationAxis, rotationAngle, scale, tint);
}

void Game::DrawUI()
{
	int cx = GetScreenWidth() / 2;
	int cy = GetScreenHeight() / 2;
	DrawLine(cx - 10, cy, cx + 10, cy, BLACK);
	DrawLine(cx, cy - 10, cx, cy + 10, BLACK);
	DrawPixel(cx, cy, RED);

	DrawFPS(10, 10);


	// --- NOTIFICATION MESSAGE ---
	if (messageTimer > 0.0f)
	{
		// fade effect: Alpha is 1.0 normally, but fades out in the last second
		float alpha = 1.0f;
		if (messageTimer < 1.0f)
			alpha = messageTimer;

		// draw Text centered at the top
		const char* text = messageText;
		int textWidth = MeasureText(text, 30);
		int centerX = GetScreenWidth() / 2 - textWidth / 2;

		DrawText(text, centerX, 50, 30, Fade(GREEN, alpha));
	}

	// --- HOTBAR ---
	int blockSize = 50;
	int padding = 10;
	int numSlots = 9; // first 9 slots 

	// Calculate total width to center it
	int totalWidth = (blockSize * numSlots) + (padding * (numSlots - 1));
	int startX = (GetScreenWidth() - totalWidth) / 2;
	int startY = GetScreenHeight() - blockSize - 20;

	for (int i = 0; i < numSlots; i++)
	{
		int x = startX + (i * (blockSize + padding));

		// Draw Slot Background
		Color color = Fade(LIGHTGRAY, 0.5f);
		if (inventory.selectedSlot == i) color = YELLOW; // Highlight selected

		DrawRectangle(x - 2, startY - 2, blockSize + 4, blockSize + 4, BLACK);
		DrawRectangle(x, startY, blockSize, blockSize, color);

		int blockID = inventory.slots[i].blockID;

		// only draw texture if slot is not empty
		if (blockID != 0)
		{
			Texture2D previewTex;
			switch (blockID) {
			case 1: previewTex = texDirt; break;
			case 2: previewTex = texStone; break;
			case 3: previewTex = texWood; break;
			case 4: previewTex = texGrassSide; break;
			case 5: previewTex = texSand; break;
			case 6: previewTex = texBedrock; break;
			case 7: previewTex = texLeaves; break;
			case 9: previewTex = texSnowSide; break;
			case 10: previewTex = texCactus; break;
			default: previewTex = texDirt; break;
			}

			Rectangle sourceRec = { 0.0f, 0.0f, (float)previewTex.width, (float)previewTex.height };
			Rectangle destRec = { (float)x + 4, (float)startY + 4, (float)blockSize - 8, (float)blockSize - 8 };
			DrawTexturePro(previewTex, sourceRec, destRec, Vector2{ 0,0 }, 0.0f, WHITE);
		}

		// draw Slot Number (1-9)
		DrawText(TextFormat("%d", i + 1), x + 2, startY + 2, 10, BLACK);
	}
}

// --- UTILITIES (Textures) ---

Texture2D Game::GenStoneTexture(int size)
{
	Image img = GenImagePerlinNoise(size, size, 0, 0, 8.0f);
	ImageColorBrightness(&img, -40);
	ImageColorContrast(&img, -30);
	ImageColorTint(&img, Color{ 200, 200, 200, 255 });

	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);
	return tex;
}

Texture2D Game::GenWoodTexture(int size)
{
	Image img = GenImagePerlinNoise(size, size, 0, 0, 4.0f);
	ImageColorBrightness(&img, -10);
	ImageColorContrast(&img, -20);
	ImageColorTint(&img, Color{ 170, 135, 85, 255 });

	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);

	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenSandTexture(int size)
{
	Image img = GenImageColor(size, size, BLANK);
	Color* pixels = LoadImageColors(img);

	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
			for (int x = 0; x < size; x++)
			{
				Color sand = { 237, 229, 173, 255 };

				int nx = x / 2;
				int ny = y / 2;
				unsigned int seed = nx * 49632 ^ ny * 325176;
				seed = (seed << 13) ^ seed;

				float noise = 1.0f - ((seed * (seed * seed * 15731 + 789221) + 1376312589)
					& 0x7fffffff) / 1073741824.0f;

				noise = roundf(noise * 4.0f) / 4.0f;

				int variation = (int)(noise * 8.0f);
				sand.r = Clamp(sand.r + variation, 0, 255);
				sand.g = Clamp(sand.g + variation, 0, 255);
				sand.b = Clamp(sand.b + variation, 0, 255);

				if (GetRandomValue(0, 100) < 6)
				{
					sand.r -= 10;
					sand.g -= 10;
					sand.b -= 15;
				}

				// if we are at the edge of the block, darken the color
				if (x == 0 || x == size - 1 || y == 0 || y == size - 1)
				{
					float edgeNoise = 0.92f + GetRandomValue(0, 4) * 0.01f;
					sand.r *= edgeNoise;
					sand.g *= edgeNoise;
					sand.b *= edgeNoise;
				}


				pixels[y * size + x] = sand;
			}
	}

	Image newImg;
	newImg.data = pixels;
	newImg.width = size;
	newImg.height = size;
	newImg.mipmaps = 1;
	newImg.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

	Texture2D tex = LoadTextureFromImage(newImg);

	UnloadImageColors(pixels);

	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenLeavesTexture(int size)
{
	Image img = GenImagePerlinNoise(size, size, 0, 0, 5.0f);
	ImageColorBrightness(&img, -15);
	ImageColorContrast(&img, -10);
	ImageColorTint(&img, Color{ 70, 140, 60, 255 });

	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);

	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenBedrockTexture(int size)
{
	Image img = GenImageColor(size, size, BLANK);
	Color* pixels = LoadImageColors(img);

	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
		{
			// base dark stone
			Color rock = { 55, 55, 55, 255 };

			// large chaotic noise
			int nx = x / 3;
			int ny = y / 3;
			unsigned int seed = nx * 92837111 ^ ny * 689287499;
			seed = (seed << 13) ^ seed;

			float noise1 = 1.0f - ((seed * (seed * seed * 15731 + 789221)
				+ 1376312589) & 0x7fffffff) / 1073741824.0f;

			// smaller sharp noise
			unsigned int seed2 = x * 73428767 ^ y * 912931;
			seed2 = (seed2 << 13) ^ seed2;

			float noise2 = 1.0f - ((seed2 * (seed2 * seed2 * 12497 + 604727)
				+ 1345679039) & 0x7fffffff) / 1073741824.0f;

			// combine & quantize
			float combined = (noise1 * 0.7f + noise2 * 0.3f);
			combined = roundf(combined * 5.0f) / 5.0f;

			int variation = (int)(combined * 35.0f);

			rock.r = Clamp(rock.r + variation, 20, 110);
			rock.g = Clamp(rock.g + variation, 20, 110);
			rock.b = Clamp(rock.b + variation, 20, 110);

			// cracks (very dark pixels)
			if (noise2 < -0.65f)
			{
				rock = { 15, 15, 15, 255 };
			}

			pixels[y * size + x] = rock;
		}
	}

	Image newImg;
	newImg.data = pixels;
	newImg.width = size;
	newImg.height = size;
	newImg.mipmaps = 1;
	newImg.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

	Texture2D tex = LoadTextureFromImage(newImg);
	UnloadImageColors(pixels);

	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}


Texture2D Game::GenDirtTexture(int size)
{
	// MC Dirt is brownish-grey
	Image img = GenImagePerlinNoise(size, size, 50, 50, 4.0f); // Offset to diff from stone
	ImageColorBrightness(&img, -30);
	ImageColorContrast(&img, -10);
	ImageColorTint(&img, Color{ 150, 100, 70, 255 }); // Brown
	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);
	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenGrassTexture(int size)
{
	// MC Top Grass is bright green with noise
	Image img = GenImagePerlinNoise(size, size, 100, 100, 8.0f);
	ImageColorBrightness(&img, -10);
	ImageColorContrast(&img, -20);
	ImageColorTint(&img, Color{ 120, 200, 80, 255 }); // Vibrant Green
	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);
	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenGrassSideTexture(int size)
{
	// generate Base Dirt
	Image img = GenImagePerlinNoise(size, size, 50, 50, 4.0f);
	ImageColorBrightness(&img, -30);
	ImageColorContrast(&img, -10);
	ImageColorTint(&img, Color{ 150, 100, 70, 255 });

	// draw Green Top (The grassy part on the side)
	Color grassColor = { 120, 200, 80, 255 };

	// draw top 1/3rd as solid grass color
	ImageDrawRectangle(&img, 0, 0, size, size / 3, grassColor);

	// add a few random "hanging" pixels
	for (int i = 0; i < size; i++) {
		if (GetRandomValue(0, 1)) {
			ImageDrawPixel(&img, i, size / 3, grassColor);
			if (GetRandomValue(0, 1)) ImageDrawPixel(&img, i, (size / 3) + 1, grassColor);
		}
	}

	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);
	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenSnowTexture(int size)
{
	Image img = GenImageColor(size, size, BLANK);
	Color* pixels = LoadImageColors(img);

	for (int y = 0; y < size; y++)
	{
		for (int x = 0; x < size; x++)
		{
			// base snow color (NOT pure white)
			Color snow = { 240, 242, 245, 255 };

			// blocky noise (very subtle)
			int nx = x / 2;
			int ny = y / 2;
			unsigned int seed = nx * 73856093 ^ ny * 19349663;
			seed = (seed << 13) ^ seed;

			float noise = 1.0f - ((seed * (seed * seed * 15731 + 789221)
				+ 1376312589) & 0x7fffffff) / 1073741824.0f;

			noise = roundf(noise * 3.0f) / 3.0f;

			int variation = (int)(noise * 4.0f);
			snow.r = Clamp(snow.r + variation, 0, 255);
			snow.g = Clamp(snow.g + variation, 0, 255);
			snow.b = Clamp(snow.b + variation, 0, 255);


			// VERY soft edge (much weaker than sand)
			int edgeDist = MIN(
				MIN(x, size - 1 - x),
				MIN(y, size - 1 - y)
			);

			if (edgeDist == 0)
			{
				float edge = 0.97f;
				snow.r *= edge;
				snow.g *= edge;
				snow.b *= edge;
			}

			pixels[y * size + x] = snow;
		}
	}

	Image newImg;
	newImg.data = pixels;
	newImg.width = size;
	newImg.height = size;
	newImg.mipmaps = 1;
	newImg.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

	Texture2D tex = LoadTextureFromImage(newImg);

	UnloadImageColors(pixels);

	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenCactusTexture(int size)
{
	// cactus is green with vertical stripes
	Image img = GenImageColor(size, size, Color{ 60, 140, 60, 255 }); // Dark Green base

	// draw lighter vertical stripes
	for (int i = 2; i < size; i += 4) {
		ImageDrawRectangle(&img, i, 0, 1, size, Color{ 80, 180, 80, 255 });
	}

	// add random thorns (dots)
	for (int i = 0; i < 10; i++) {
		int rx = GetRandomValue(0, size - 1);
		int ry = GetRandomValue(0, size - 1);
		ImageDrawPixel(&img, rx, ry, BLACK);
	}

	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);
	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenSnowSideTexture(int size)
{
	// generate Base Dirt (same as dirt texture)
	Image img = GenImagePerlinNoise(size, size, 50, 50, 4.0f);
	ImageColorBrightness(&img, -30);
	ImageColorContrast(&img, -10);
	ImageColorTint(&img, Color{ 150, 100, 70, 255 });

	// draw Snow Top (White)
	Color snowColor = { 240, 242, 245, 255 };

	// draw top 1/3rd as solid snow
	ImageDrawRectangle(&img, 0, 0, size, size / 3, snowColor);

	// add "dripping" snow pixels
	for (int i = 0; i < size; i++) {
		if (GetRandomValue(0, 1)) {
			ImageDrawPixel(&img, i, size / 3, snowColor);
			if (GetRandomValue(0, 1)) ImageDrawPixel(&img, i, (size / 3) + 1, snowColor);
		}
	}

	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);
	SetTextureFilter(tex, TEXTURE_FILTER_POINT);
	return tex;
}

Texture2D Game::GenerateSkyTexture()
{
	const int width = 64;   // slightly wider = less repetition
	const int height = 512;

	Image img = GenImageColor(width, height, BLACK);
	Color* pixels = (Color*)MemAlloc(width * height * sizeof(Color));

	Color zenith = { 100, 160, 255, 255 };
	Color horizon = { 220, 240, 255, 255 };
	Color voidCol = { 20, 20, 45, 255 };

	for (int y = 0; y < height; y++)
	{
		float t = (float)y / (float)(height - 1);

		// non-linear curve (makes sky feel deeper)
		float curve = powf(t, 1.3f);

		Color rowColor;

		if (curve < 0.55f)
		{
			float localT = curve / 0.55f;
			rowColor.r = (unsigned char)Lerp(zenith.r, horizon.r, localT);
			rowColor.g = (unsigned char)Lerp(zenith.g, horizon.g, localT);
			rowColor.b = (unsigned char)Lerp(zenith.b, horizon.b, localT);
		}
		else
		{
			float localT = (curve - 0.55f) / 0.45f;
			rowColor.r = (unsigned char)Lerp(horizon.r, voidCol.r, localT);
			rowColor.g = (unsigned char)Lerp(horizon.g, voidCol.g, localT);
			rowColor.b = (unsigned char)Lerp(horizon.b, voidCol.b, localT);
		}

		// horizon glow band
		float glow = expf(-powf((t - 0.5f) * 6.0f, 2.0f));
		rowColor.r = Clamp(rowColor.r + glow * 12, 0, 255);
		rowColor.g = Clamp(rowColor.g + glow * 12, 0, 255);
		rowColor.b = Clamp(rowColor.b + glow * 6, 0, 255);

		for (int x = 0; x < width; x++)
		{
			// subtle horizontal variation (prevents flat look)
			float noise = ((x * 13 + y * 7) % 17) / 255.0f;

			Color finalColor = rowColor;
			finalColor.r = Clamp(finalColor.r + noise * 6, 0, 255);
			finalColor.g = Clamp(finalColor.g + noise * 6, 0, 255);
			finalColor.b = Clamp(finalColor.b + noise * 6, 0, 255);

			pixels[y * width + x] = finalColor;
		}
	}

	RL_FREE(img.data);
	img.data = pixels;

	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img);

	// bilinear is correct for skyboxes
	SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);

	return tex;
}

Texture2D Game::GenerateCloudTexture()
{
	const int size = 512;
	Image img = GenImageColor(size, size, BLANK);

	// direct pixel access
	Color* pixels = LoadImageColors(img);

	// clear to transparent
	for (int i = 0; i < size * size; i++) pixels[i] = BLANK;

	// draw 2000 random soft cloud blobs
	for (int i = 0; i < 2000; i++)
	{
		int x = GetRandomValue(0, size - 1);
		int y = GetRandomValue(0, size - 1);
		int radius = GetRandomValue(20, 60);
		int baseOpacity = GetRandomValue(10, 30); // base opacity for softness

		for (int py = -radius; py <= radius; py++)
		{
			for (int px = -radius; px <= radius; px++)
			{
				if (px * px + py * py > radius * radius) continue; // circle check

				// distance from center for soft edges
				float dist = sqrtf(float(px * px + py * py)) / radius;
				int blobAlpha = int((1.0f - dist) * baseOpacity);
				if (blobAlpha < 0) blobAlpha = 0;

				// calculate wrapped coordinates
				int finalX = (x + px + size) % size;
				int finalY = (y + py + size) % size;

				// add color with alpha blending
				Color* target = &pixels[finalY * size + finalX];

				int newAlpha = target->a + blobAlpha;
				if (newAlpha > 255) newAlpha = 255;

				// slight color variation for softness
				int shade = 220 + GetRandomValue(0, 35); // 220-255
				target->r = shade;
				target->g = shade;
				target->b = shade;
				target->a = (unsigned char)newAlpha;
			}
		}
	}

	// create texture
	UnloadImage(img);
	Image cloudImg = { pixels, size, size, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
	Texture2D tex = LoadTextureFromImage(cloudImg);
	UnloadImageColors(pixels);

	SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
	SetTextureWrap(tex, TEXTURE_WRAP_REPEAT); // repeat mode

	return tex;
}


Texture2D Game::GenerateHazeTexture()
{
	const int width = 32;
	const int height = 128;

	Image img = GenImageColor(width, height, BLANK);
	Color* pixels = LoadImageColors(img);

	for (int y = 0; y < height; y++)
	{
		float t = (float)y / (float)(height - 1);

		// stronger at horizon, fades upward
		float alpha = powf(t, 4.0f) * 120.0f;

		for (int x = 0; x < width; x++)
		{
			pixels[y * width + x] = {
				255, 255, 255,
				(unsigned char)alpha
			};
		}
	}

	Image finalImg = {
		pixels,
		width,
		height,
		1,
		PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
	};

	Texture2D tex = LoadTextureFromImage(finalImg);
	UnloadImageColors(pixels);

	SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
	SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);

	return tex;
}

Color Game::LerpColor(Color a, Color b, float t) {
	t = Clamp(t, 0.0f, 1.0f);
	return Color{
		(unsigned char)(a.r + (b.r - a.r) * t),
		(unsigned char)(a.g + (b.g - a.g) * t),
		(unsigned char)(a.b + (b.b - a.b) * t),
		(unsigned char)(a.a + (b.a - a.a) * t)
	};
}

bool Game::IsBlockActive(int x, int y, int z)
{
	// pass 'false' so we dont generate chunks just for culling checks
	if (world.GetBlock(x, y, z, false) == 0)
		return false;

	// check neighbors with 'false' too
	if (x > 0 && world.GetBlock(x - 1, y, z, false) == 0) return true;
	if (x < CHUNK_SIZE - 1 && world.GetBlock(x + 1, y, z, false) == 0) return true;

	if (y > 0 && world.GetBlock(x, y - 1, z, false) == 0) return true;
	if (y < CHUNK_SIZE - 1 && world.GetBlock(x, y + 1, z, false) == 0) return true;

	if (z > 0 && world.GetBlock(x, y, z - 1, false) == 0) return true;
	if (z < CHUNK_SIZE - 1 && world.GetBlock(x, y, z + 1, false) == 0) return true;

	if (x == 0 || x == CHUNK_SIZE - 1 ||
		y == 0 || y == CHUNK_SIZE - 1 ||
		z == 0 || z == CHUNK_SIZE - 1)
		return true;

	return false;
}

// DEBUGGING
void Game::DrawDebug()
{
	if (!showDebugUI)
		return;

	// Dimensions
	int width = 280;
	int height = 210;

	// calculate Top-Right Position
	int x = GetScreenWidth() - width - 10; // 10px padding from right edge
	int y = 10;                            // 10px padding from top

	// background: Dark Gray (0.9 opacity = almost solid)
	DrawRectangle(x, y, width, height, Fade(BLACK, 0.85f));
	DrawRectangleLines(x, y, width, height, WHITE);

	// styling for RayGui
	GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xFFFFFFFF);

	// --- DRAW SLIDERS ---
	// only adjust the X/Y of every element based on our new 'x' variable

	// Gravity
	GuiSlider(Rectangle{ (float)x + 100, (float)y + 30, 120, 20 },
		"Gravity", TextFormat("%2.3f", gravity), &gravity, 0.001f, 0.1f);

	// Jump Force
	GuiSlider(Rectangle{ (float)x + 100, (float)y + 60, 120, 20 },
		"Jump Power", TextFormat("%2.2f", jumpForce), &jumpForce, 0.1f, 1.0f);

	// Move Speed
	GuiSlider(Rectangle{ (float)x + 100, (float)y + 90, 120, 20 },
		"Walk Speed", TextFormat("%2.1f", moveSpeed), &moveSpeed, 1.0f, 20.0f);

	// Fly Speed Slider
	GuiSlider(Rectangle{ (float)x + 100, (float)y + 120, 120, 20 },
		"Fly Speed", TextFormat("%2.1f", flySpeed), &flySpeed, 5.0f, 50.0f);

	// Day Speed
	// Range: 0.0 (Time Stop) to 0.1 (Super Fast)
	GuiSlider(Rectangle{ (float)x + 100, (float)y + 150, 120, 20 },
		"Time Speed", TextFormat("%2.4f", daySpeed), &daySpeed, 0.0f, 0.1f);

	GuiToggleGroup(Rectangle{ (float)x + 100, (float)y + 180, 40, 20 }, "AUTO;DAY;NIGHT", &timeMode);

	DrawText("Time Mode", x + 20, y + 185, 10, WHITE);



	// Instructions
	DrawText("Press TAB to Close", x + 20, y + 220, 10, WHITE);
}

// --- SAVING & LOADING ---

void Game::SaveMap()
{
	// Infinite world saving is complex (requires serializing the std::map).
	// For now, we just show a message so the game doesnt crash.
	messageText = "SAVING DISABLED (INFINITE WORLD)";
	messageTimer = 2.0f;
}

void Game::LoadMap()
{
	messageText = "LOADING DISABLED (INFINITE WORLD)";
	messageTimer = 2.0f;
}