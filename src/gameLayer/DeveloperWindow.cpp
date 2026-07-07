#include "DeveloperWindow.h"

#include <SceneManager.h>
#include <Player.h>

void DeveloperWindow::update(Player* player)
{
	isActive =
		isCameraActive ||
		isInspectorActive ||
		isMiniGameActive ||
		isGameDataActive||
		isAssetActive
		;

	if (IsKeyPressed(KEY_F1)) { isGameDataActive = !isGameDataActive; }
	if (IsKeyPressed(KEY_F2)) { isPlayerActive = !isPlayerActive; }
	if (IsKeyPressed(KEY_F3)) { isCameraActive = !isCameraActive; }
	if (IsKeyPressed(KEY_F4)) { isInspectorActive = !isInspectorActive; inspectObject = nullptr; }
	if (IsKeyPressed(KEY_F5)) { isMiniGameActive = !isMiniGameActive; }
	if (IsKeyPressed(KEY_F6)) { isEntityActive = !isEntityActive; }
	if (IsKeyPressed(KEY_F7)) { isAssetActive = !isAssetActive; }

	/// Update Game Data Windows
	if (isPlayerActive) ShowPlayerData(player);
	if (isCameraActive) ShowCameraData(player);
	if (isInspectorActive) ShowObjectInspector();
	if (isEntityActive) ShowEntityInspector();
	if (isMiniGameActive) ShowMiniGameData(player);
	if (isGameDataActive) ShowGameData(player);
	if (isAssetActive) ShowAssetData();
}