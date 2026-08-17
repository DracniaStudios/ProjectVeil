#include "MiniGame_Maze.h"

#include <SceneManager.h>

namespace MazeSpace {
	
	Rectangle GetScreen() { return { getScreenScale(Rectangle{0.25f, 0.3, 0.3f, 0.5f}) }; }
	
	struct Cell {
		Vector2 pos = Vector2Zero();
		bool isActive = false;
		Color color = SKYBLUE;
	};

	struct CellMap {
		std::vector<Cell> map;
		std::vector<uint8_t> cellsActive;
		uint64_t size = 64;

		CellMap(int mapSize) {
			size = mapSize;
			map.resize(size * size);

			for (int x = 0; x < mapSize; x++) {
				for (int y = 0; y < mapSize; y++) {
					Cell cell;
					cell.pos = Vector2(x, y);
					cell.isActive = false;
					map.push_back(cell);
				}
			}
		}
	};

	CellMap renderMap1(){
		CellMap map = CellMap(8);
		
		for (size_t i = 0; i < map.map.size(); i++) {
			auto cell = &map.map[i];
			if (cell->isActive) {
				cell->color = SKYBLUE;
			}
			else {
				cell->color = DARKBLUE;
			}

			Rectangle cellSize;
			cellSize.x = GetScreen().x + (cell->pos.x * map.size);
			cellSize.y = GetScreen().y + (cell->pos.y * map.size);
			cellSize.width = GetScreen().x / map.size;
			cellSize.height = GetScreen().y / map.size;

			DrawRectangleRec(cellSize, cell->color);
		}
		return map;
	}

	Vector2 playerLocation = Vector2{ 0, 0 };

}

MiniGame* MiniGame_Maze(Player* player)
{
	auto* game = new MiniGame();
	game->name = "Maze";
	game->update = &Maze::update;
	game->draw = &Maze::render;
	game->data = new MiniGameData;

	player->rigidBody2D.teleport(Vector2(GetScreenWidth() * 0.1f, GetScreenHeight() * 0.1f));

	return game;
}

void Maze::render(MiniGameData* data, Player* player)
{
	using namespace MazeSpace;
	DrawRectangleRec(GetScreen(), DARKBLUE);
	auto map = renderMap1();

}

void Maze::update(MiniGameData* data, Player* player, float delta)
{
	auto inputSystem = &InputSystem::getInstance();
	using namespace MazeSpace;

	auto playerRect = Rectangle(player->rigidBody2D.translation.x, player->rigidBody2D.translation.y, player->rigidBody2D.scale.x, player->rigidBody2D.scale.y);

	if (!CheckCollisionRecs(playerRect, GetScreen())) {

	}
	CompleteMiniGame(data, player, BUFF_SEARCH);
}
