#pragma once
#ifndef RANDOM_STUFF_H
#define RANDOM_STUFF_H

#include <random>
#include <raylib.h>

enum Rarity
{
	COMMON, // #FDFEFE 40%
	UNCOMMON, // 27AE60 25%
	RARE, // 2471A3 15%
	EPIC, // 7D3C98 10%
	LEGENDARY, // F1C40F 5%
	MYTHIC, // D35400 3%
	MASTERWORK, // 7B241C 1.5%
	DIVINE, // B3CA1F 0.05% 
	ETERNAL // DC2367 0.01%
};

// Returns a float
float getRandomFloat(std::ranlux24_base& rng, float min = -1, float max = 1);

// Returns an int
int getRandomInt(std::ranlux24_base& rng, int min = -10, int max = 10);

// Gets a Chance
bool getRandomChance(std::ranlux24_base& rng, float chance);

Rarity GetRandomRarity(Rarity minRarity = COMMON, Rarity maxRarity = ETERNAL);
#endif
