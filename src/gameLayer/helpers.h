#pragma once 
#ifndef HELPERS_H
#define HELPERS_H

#include <raylib.h>
#include <iostream>

// Length in milliseconds
inline bool WaitTimer(float& timer, bool& trait, int length = 1)
{
	if (GetTime() > timer + length)
	{
		std::cout << "Timer: " << timer << "\n";
		std::cout << "Length: " << length << "\n";
		timer += length;
		trait = true;
		return true;
	}
	return false;
}

#endif
