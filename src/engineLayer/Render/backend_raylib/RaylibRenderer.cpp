// Render on raylib (B-07). Lifetime only; nothing draws through it yet.
#include <Render/Render.h>

namespace {
	bool ready = false;
}

namespace Render {

bool Init()
{
	ready = true;
	return true;
}

void Shutdown() { ready = false; }

bool IsReady() { return ready; }

}
