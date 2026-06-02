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

public:
	bool isLoop = true;
	void Update();
	void Draw();
	void Emit(int count);
private:
	ParticleSystem();
	~ParticleSystem();
	std::vector<Particle> particles;
};