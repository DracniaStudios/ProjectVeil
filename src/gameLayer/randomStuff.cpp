#include "randomStuff.h"

float getRandomFloat(std::ranlux24_base& rng, float min, float max)
{
	std::uniform_real_distribution<> dis(min, max);
	return dis(rng);
}
int getRandomInt(std::ranlux24_base& rng, int min, int max)
{
	if (min > max)
	{
		std::uniform_real_distribution<> dis(max, min);
		return dis(rng);
	}
	else
	{
		std::uniform_real_distribution<> dis(min, max);
		return dis(rng);
	}

}
bool getRandomChance(std::ranlux24_base& rng, float chance)
{
	float dice = getRandomFloat(rng, 0.0f, 1.0f);
	return dice <= chance;
}

Rarity GetRandomRarity(Rarity minRarity, Rarity maxRarity)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());

	std::vector<double> weights = {
		40.0,  // Common
		25.0,  // Uncommon
		15.0,  // Rare
		10.0,  // Epic
		5.0,   // Legendary
		3.0,   // Mythic
		1.5,   // Masterwork
		0.5,   // Divine
		0.1    // Eternal
	};

	int minIndex = static_cast<int>(minRarity);
	int maxIndex = static_cast<int>(maxRarity);

	std::vector<double> limitedWeights(
		weights.begin() + minIndex + 1,
		weights.begin() + maxIndex + 1
	);

	std::discrete_distribution<> dist(
		limitedWeights.begin(),
		limitedWeights.end()
	);

	int index = dist(gen);
	return static_cast<Rarity>(index);

}
