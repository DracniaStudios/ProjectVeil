#include "ResourcePaths.h"

#include <cstdint>
#include <cstdlib>
#include <string>
#include <system_error>

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <windows.h>
#elif defined(__APPLE__)
	#include <mach-o/dyld.h>
#endif

namespace ResourcePaths {

	namespace fs = std::filesystem;

	namespace {

		// The folder holding the running executable, however the process was started
		// (a shortcut, another working directory, a debugger)
		fs::path ExecutableFolder()
		{
			std::error_code error;

#if defined(_WIN32)
			std::wstring buffer(MAX_PATH, L'\0');
			for (;;)
			{
				const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
				if (size == 0) { break; }

				// A path that fills the buffer was cut short: try again with room
				if (size < buffer.size())
				{
					buffer.resize(size);
					return fs::path(buffer).parent_path();
				}
				buffer.resize(buffer.size() * 2);
			}
#elif defined(__APPLE__)
			std::uint32_t size = 0;
			_NSGetExecutablePath(nullptr, &size);
			std::string buffer(size, '\0');
			if (_NSGetExecutablePath(buffer.data(), &size) == 0)
			{
				const fs::path resolved = fs::canonical(fs::path(buffer.c_str()), error);
				if (!error) { return resolved.parent_path(); }
			}
#else
			const fs::path executable = fs::read_symlink("/proc/self/exe", error);
			if (!error) { return executable.parent_path(); }
#endif
			// The platform would not say: the working directory is the best guess left
			return fs::current_path(error);
		}

		fs::path ResolveResources()
		{
			const char* value = std::getenv("VEIL_RESOURCES_DIR");
			if (value != nullptr && *value != '\0')
			{
				fs::path folder(value);

				// "dir/resources/" and "dir/resources" are the same folder
				if (!folder.has_filename()) { folder = folder.parent_path(); }
				return folder;
			}

			return ExecutableFolder() / "resources";
		}
	}

	const fs::path& Resources()
	{
		static const fs::path resources = ResolveResources();
		return resources;
	}

	const fs::path& Saves()
	{
		static const fs::path saves = Resources().parent_path() / "saves";
		return saves;
	}
}
