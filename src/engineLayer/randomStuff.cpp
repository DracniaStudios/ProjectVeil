#include "randomStuff.h"

float getRandomFloat(std::ranlux24_base& rng, float min, float max)
{
	std::uniform_real_distribution<> dis(min, max);
	return dis(rng);
}

int getRandomInt(std::ranlux24_base& rng, int min, int max)
{
	// uniform_real_distribution is half-open on [min, max), and truncating the
	// result to int discards the fraction — so max was only ever reachable in
	// the same rare case any other value near it would be, which is to say
	// effectively never. Callers picking from an inclusive range (e.g. 1-3)
	// silently lost their top value. std::uniform_int_distribution is
	// inclusive on both ends and matches the "int" the function returns.
	if (min > max) { std::swap(min, max); }
	std::uniform_int_distribution<int> dis(min, max);
	return dis(rng);
}

bool getRandomChance(std::ranlux24_base& rng, float chance)
{
	float dice = getRandomFloat(rng, 0.0f, 1.0f);
	return dice <= chance;
}

Vector3 getRandomVector3(std::ranlux24_base& rng, Vector3 min, Vector3 max) {

	Vector3 newVector{
		getRandomFloat(rng, min.x, max.x),
		getRandomFloat(rng, min.y, max.y),
		getRandomFloat(rng, min.z, max.z),
	};
	return newVector;
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
		weights.begin() + minIndex,
		weights.begin() + maxIndex + 1
	);

	std::discrete_distribution<> dist(
		limitedWeights.begin(),
		limitedWeights.end()
	);

	int index = dist(gen);
	return static_cast<Rarity>(index + minIndex);

}

Color getRandomColor(std::ranlux24_base& rng, int alpha)
{
	return Color{
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		static_cast<unsigned char>(getRandomInt(rng, 0, 255)),
		static_cast<unsigned char>(alpha)
	};
}
