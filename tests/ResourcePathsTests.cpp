/**
 * ResourcePaths: where the game finds resources/ and saves/ at run time
 * (src/engineLayer/ResourcePaths.h).
 *
 * The answer is resolved once per process, so each case is its own run:
 *
 *   resource_paths_tests --default    no override: beside the executable
 *   resource_paths_tests --override   VEIL_RESOURCES_DIR names the folder
 *
 * Links with ResourcePaths.cpp alone: no raylib, no engine.
 */

#include <ResourcePaths.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

static int g_failures = 0;
static int g_checks = 0;

static void Check(bool condition, const std::string& what)
{
	++g_checks;
	if (!condition)
	{
		++g_failures;
		std::printf("  FAIL  %s\n", what.c_str());
	}
}

// An empty value counts as not set
static void SetEnvironment(const char* name, const char* value)
{
#if defined(_WIN32)
	_putenv_s(name, value);
#else
	setenv(name, value, 1);
#endif
}

static void TestBesideTheExecutable(const char* argv0)
{
	std::printf("with no override, resources/ and saves/ sit beside the executable\n");

	// Found before the working directory moves, since argv[0] may be relative to it
	std::error_code error;
	const fs::path here = fs::weakly_canonical(fs::absolute(argv0, error), error).parent_path();
	Check(!error && fs::is_directory(here), "this test can find its own folder");

	// Launched from somewhere else entirely: the answer must not follow the working directory
	fs::current_path(fs::temp_directory_path(), error);
	SetEnvironment("VEIL_RESOURCES_DIR", "");

	const fs::path& resources = ResourcePaths::Resources();
	const fs::path& saves = ResourcePaths::Saves();

	Check(resources.filename() == "resources", "Resources() is a folder named resources");
	Check(saves.filename() == "saves", "Saves() is a folder named saves");
	Check(fs::equivalent(resources.parent_path(), here, error), "resources/ is in the executable's folder");
	Check(fs::equivalent(saves.parent_path(), here, error), "saves/ is in the executable's folder");
	Check(resources.is_absolute() && saves.is_absolute(), "both are absolute, whatever the working directory");
}

static void TestTheOverrideNamesTheFolder()
{
	std::printf("VEIL_RESOURCES_DIR names the resources folder, with saves/ beside it\n");

	const fs::path wanted = (fs::temp_directory_path() / "veil_resource_paths_tests" / "resources").lexically_normal();

	// A trailing separator names the same folder
	SetEnvironment("VEIL_RESOURCES_DIR", (wanted.string() + "/").c_str());

	Check(ResourcePaths::Resources().lexically_normal() == wanted, "Resources() is the folder named, without its trailing separator");
	Check(ResourcePaths::Saves().lexically_normal() == (wanted.parent_path() / "saves").lexically_normal(), "Saves() is beside it");

	// Resolved once: a later change to the variable does not move the game's files
	SetEnvironment("VEIL_RESOURCES_DIR", "somewhere_else");
	Check(ResourcePaths::Resources().lexically_normal() == wanted, "the answer is kept for the life of the process");
}

int main(int argc, char** argv)
{
	const bool useOverride = argc > 1 && std::strcmp(argv[1], "--override") == 0;
	const bool useDefault = argc > 1 && std::strcmp(argv[1], "--default") == 0;
	if (!useOverride && !useDefault)
	{
		std::printf("usage: %s --default | --override\n", argv[0]);
		return 2;
	}

	std::printf("ResourcePaths tests (%s)\n\n", argv[1]);

	if (useDefault) { TestBesideTheExecutable(argv[0]); }
	else { TestTheOverrideNamesTheFolder(); }

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
