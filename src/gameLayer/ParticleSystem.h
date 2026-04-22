#pragma once

#include <raylib.h>
#include <vector>

typedef struct Particle {
	float x, y;
	float speedX, speedY;
	float size;
	float life;
	Color color;
} Particle;

class ParticleSystem {
private:
	std::vector<Particle> particles;

public:
	bool isLoop = true;
	ParticleSystem();
	~ParticleSystem();
	void Update();
	void Draw();
	void Emit(int count);
};