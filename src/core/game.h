#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "../world/chunk_manager.h"
#include "../player/player.h"
#include "../graphics/renderer.h"

enum GameState {
	STATE_MENU,
	STATE_LOADING,
	STATE_PLAYING,
	STATE_PAUSE
};

class Game {
public:
	void Init();
	void Update();
	void Draw();
	void ShutDown();

private:
	ChunkManager world;
	Player player;
	Renderer renderer;

	// environment settings
	float timeOfDay;
	int timeMode;
	float daySpeed;
	float physicsTimer;

	// debug & ui state
	bool showDebugUI;
	float messageTimer;
	const char* messageText;

	// game state
	GameState currentState;

	// loading State
	int loadingProgress;    // 0 to 100
	bool isNewGame;         // are we generating or loading?

	// Auto-Save
	float autoSaveTimer;

	// internal Helpers
	void UpdateMenu();
	void DrawMenu();

	void UpdateLoading(); // function that loads chunks 1-by-1
	void DrawLoading();

	// UI Buffers
	char worldNameBuffer[64]; // text box input
	char seedBuffer[64];      // seed input

	// helper to get list of files
	std::vector<std::string> saveFiles;
	int selectedSaveIndex;

	void SaveMap(const char* filename);
	bool LoadMap(const char* filename);
	std::string currentSaveName; // To remember what file we are playing on

	// state trackers for text boxes
	bool editWorldNameMode;
	bool editSeedMode;
};

#endif