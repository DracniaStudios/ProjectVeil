#include "Presentation.h"

namespace Sim {
	namespace {
		Presentation*& Installed()
		{
			static Presentation* installed = nullptr;
			return installed;
		}
	}

	Presentation& CurrentPresentation()
	{
		static Presentation nothing;
		Presentation* installed = Installed();
		return installed ? *installed : nothing;
	}

	void InstallPresentation(Presentation* presentation)
	{
		Installed() = presentation;
	}
}
