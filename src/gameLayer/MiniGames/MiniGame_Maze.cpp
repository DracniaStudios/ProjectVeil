#include "MiniGame_Maze.h"

namespace {

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

	CellMap renderMap1(Rectangle screen){
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
			cellSize.x = screen.x + (cell->pos.x * map.size);
			cellSize.y = screen.y + (cell->pos.y * map.size);
			cellSize.width = screen.x / map.size;
			cellSize.height = screen.y / map.size;

			DrawRectangleRec(cellSize, cell->color);
		}
		return map;
	}

	// The speed Player::update2D moved the cursor at: the player's base speed of 1,
	// doubled while sprinting, scaled to 60 px/s
	constexpr float kCursorSpeed = 1.0f;
}

Maze::Maze(const MiniGameSetup& setup)
{
	name = "Maze";

	data.screen = getScreenScale(setup.screenSize, Rectangle{ 0.25f, 0.3f, 0.3f, 0.5f });

	cursor.teleport(Vector2(setup.screenSize.x * 0.1f, setup.screenSize.y * 0.1f));
	cursor.bounds = Rectangle{ 0, 0, setup.screenSize.x, setup.screenSize.y };
}

void Maze::DrawGame() const
{
	DrawRectangleRec(data.screen, DARKBLUE);
	renderMap1(data.screen);

}

MiniGameOutcome Maze::Update(const MiniGameInput& input, float deltaTime)
{
	// The only game that never took the cursor over, so it used to ride on
	// Player::update2D's WASD movement. Forward is +y, down the screen, as it was there.
	Vector2 moveDirection = Vector2Zero();
	moveDirection.x = input.held.left ? -1.0f : input.held.right ? 1.0f : 0.0f;
	moveDirection.y = input.held.forward ? 1.0f : input.held.backward ? -1.0f : 0.0f;

	const float speed = input.held.sprint ? kCursorSpeed * 2 : kCursorSpeed;
	cursor.translation += Vector3(moveDirection.x, moveDirection.y, 0) * speed * 60.0f * deltaTime;

	// Nothing to win yet: its score never reaches its goal
	return data.score >= data.scoreGoal ? MiniGameOutcome::Won : MiniGameOutcome::Running;
}
