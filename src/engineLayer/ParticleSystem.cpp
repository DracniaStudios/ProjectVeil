#include "ParticleSystem.h"

ParticleSystem::ParticleSystem() {
	particles.reserve(1000); // Reserve space for 1000 particles
}
ParticleSystem::~ParticleSystem() {
	particles.clear();
}
void ParticleSystem::Update() {
	for (size_t i = 0; i < particles.size(); ) {
		Particle& p = particles[i];
		p.x += p.speedX;
		p.y += p.speedY;
		p.life -= 1.0f;
		if (p.life <= 0) {
			particles.erase(particles.begin() + i);
			if (isLoop) {
				Emit(1); // Emit a new particle to replace the dead one
			}
		} else {
			++i;
		}
	}
}
void ParticleSystem::Draw() {
	for (const Particle& p : particles) {
		DrawCircleV({p.x, p.y}, p.size, p.color);
	}
}

//Generate and emit 'count' number of particles
void ParticleSystem::Emit(int count) {
	for (int i = 0; i < count; i++) {
		Particle p;
		p.x = GetRandomValue(0, GetScreenWidth());
		p.y = GetRandomValue(0, GetScreenHeight());
		p.speedX = (float)GetRandomValue(-10, 10) / 10.0f;
		p.speedY = (float)GetRandomValue(-10, 10) / 10.0f;
		p.size = (float)GetRandomValue(1, 5);
		p.life = (float)GetRandomValue(30, 100);
		p.color = Color{ 
			(unsigned char)GetRandomValue(0, 255), 
			(unsigned char)GetRandomValue(0, 255), 
			(unsigned char)GetRandomValue(0, 255), 
			255 
		};
		particles.push_back(p);
	}
}