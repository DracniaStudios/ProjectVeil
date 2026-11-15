#pragma once
#include <raylib.h>

// Client-only debug drawing; not part of the simulation.
void DrawOrientedBoxWires(Vector3 center, Vector3 size, Quaternion rotation, Color color);
