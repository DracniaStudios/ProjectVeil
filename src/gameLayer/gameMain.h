#ifndef GAME_MAIN_H
#define GAME_MAIN_H

#include <raylib.h>
#include <iostream>
#include <fstream>
#include <SceneManager.h>
#include <AssetManager.h>
#include <AudioManager.h>
#include <LightingSystem.h>
#include <Settings.h>
#include <InputSystem.h>

bool init_game();
bool update_game();
void close_game();
void request_exit(); // Ends the main loop after the current frame

#endif