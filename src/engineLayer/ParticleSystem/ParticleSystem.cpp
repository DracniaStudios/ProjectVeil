#include "ParticleSystem.h"
#include <Platform.h>
#include <Render/Render.h>

#include <algorithm>
#include <random>

namespace ParticleSystem {

	namespace {
		// Replaces raylib's GetRandomValue; inclusive on both ends, either argument order.
		int RandomInt(int a, int b) {
			static std::mt19937 engine{ std::random_device{}() };
			std::uniform_int_distribution<int> distribution(std::min(a, b), std::max(a, b));
			return distribution(engine);
		}
	}

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
			}
			else {
				++i;
			}
		}
	}
	void ParticleSystem::Draw() {
		for (const Particle& p : particles) {
			Render::Circle({ p.x, p.y }, p.size, p.color);
		}
	}

	//Generate and emit 'count' number of particles
	void ParticleSystem::Emit(int count) {
		for (int i = 0; i < count; i++) {
			Particle p;
			p.x = RandomInt(0, Platform::ScreenWidth());
			p.y = RandomInt(0, Platform::ScreenHeight());
			p.speedX = (float)RandomInt(-10, 10) / 10.0f;
			p.speedY = (float)RandomInt(-10, 10) / 10.0f;
			p.size = (float)RandomInt(1, 5);
			p.life = (float)RandomInt(30, 100);
			p.color = Color{
				(unsigned char)RandomInt(0, 255),
				(unsigned char)RandomInt(0, 255),
				(unsigned char)RandomInt(0, 255),
				255
			};
			particles.push_back(p);
		}
	}
}