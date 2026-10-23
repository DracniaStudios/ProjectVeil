/**
 * Tests for the Lighting rig's data: its lights, its save data and the
 * player's quality settings.
 *
 * A world owns its lighting. What a world saves is what it was authored with,
 * a world that saves no lights still gets lit, and the player's Shadows and
 * Luminosity change only what is drawn. SaveSystemTests covers the lighting
 * travelling with a World save and the player's settings never reaching it;
 * these cases cover the rest of the rules: a light round-trips every field,
 * a malformed or oversized rig is repaired on load, and the quality caps are
 * the sizes the Shadows menu promises.
 *
 * None of this needs the shader: Init() is never called, so nothing here
 * touches the GPU. LightingSystem.cpp still includes the engine, so this links
 * against the game's own objects like the other engine-linked suites. See
 * tests/run_tests.sh.
 */

#include <raylib.h>

#include <LightingSystem.h>
#include <Scene.h>
#include <SceneManager.h>

#include <cmath>
#include <cstdio>
#include <string>

using Lighting::Light;
using Lighting::LightingSystem;

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

static bool SameColor(Color a, Color b)
{
	return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bool SameVector(Vector3 a, Vector3 b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

// The singleton outlives each case, so every case starts from the same rig
static LightingSystem& FreshLighting()
{
	LightingSystem& lighting = LightingSystem::getInstance();
	lighting.ClearLights();
	lighting.SetFlashlightEnabled(false);
	lighting.shadowsEnabled = true;
	lighting.exposure = 1.0f;
	lighting.SetShadowResolution(2048);
	lighting.SetPlayerQuality(4, 1.0f);
	return lighting;
}

static Light MakeSpot()
{
	Light spot = {};
	spot.name = "Corridor Spot";
	spot.type = Lighting::LIGHT_SPOT;
	spot.isEnabled = false;
	spot.castsShadow = true;
	spot.position = Vector3{ 1.5f, 3.25f, -7.0f };
	spot.direction = Vector3{ 0.0f, -0.5f, 1.0f };
	spot.color = Color{ 12, 34, 56, 78 };
	spot.intensity = 2.75f;
	spot.range = 13.5f;
	spot.innerConeDegrees = 11.0f;
	spot.outerConeDegrees = 29.0f;
	return spot;
}

// ─── A light ───────────────────────────────────────────────────────────────

static void TestALightRoundTripsEveryField()
{
	std::printf("a light saved and loaded keeps every field\n");
	const Light saved = MakeSpot();

	Light loaded = {};
	Check(loaded.loadFromJson(saved.formatToJson()), "the saved light loads");

	Check(loaded.name == saved.name, "name");
	Check(loaded.type == saved.type, "type");
	Check(loaded.isEnabled == saved.isEnabled, "enabled");
	Check(loaded.castsShadow == saved.castsShadow, "casts shadow");
	Check(SameVector(loaded.position, saved.position), "position");
	Check(SameVector(loaded.direction, saved.direction), "direction");
	Check(SameColor(loaded.color, saved.color), "colour, alpha included");
	Check(loaded.intensity == saved.intensity, "intensity");
	Check(loaded.range == saved.range, "range");
	Check(loaded.innerConeDegrees == saved.innerConeDegrees, "inner cone");
	Check(loaded.outerConeDegrees == saved.outerConeDegrees, "outer cone");
}

static void TestAnUnknownLightTypeLoadsAsAPoint()
{
	std::printf("a light type this build does not know loads as a point light\n");

	for (int type : { -1, static_cast<int>(Lighting::LIGHT_TYPE_COUNT), 99 })
	{
		Json data = MakeSpot().formatToJson();
		data["Type"] = type;

		Light loaded = {};
		loaded.loadFromJson(data);
		Check(loaded.type == Lighting::LIGHT_POINT, "type " + std::to_string(type) + " becomes a point light");
	}
}

static void TestALightMissingFieldsKeepsItsDefaults()
{
	std::printf("a light saved with only some fields keeps the defaults for the rest\n");
	const Light defaults = {};

	Light loaded = {};
	Check(loaded.loadFromJson(Json{ { "Name", "Bare" }, { "Position", { 1, 2 } }, { "Color", { 1, 2, 3 } } }),
		"a sparse light still loads");
	Check(loaded.name == "Bare", "the field it has is read");
	Check(SameVector(loaded.position, defaults.position), "a position without three values is ignored");
	Check(SameColor(loaded.color, defaults.color), "a colour without four values is ignored");
	Check(loaded.range == defaults.range && loaded.intensity == defaults.intensity, "absent numbers keep their defaults");
	Check(loaded.isEnabled, "and a light is on unless saved off");
}

static void TestALightThatIsNotAnObjectIsRefused()
{
	std::printf("a light that is not a JSON object is refused\n");

	Light loaded = MakeSpot();
	Check(!loaded.loadFromJson(Json::array({ 1, 2, 3 })), "an array is refused");
	Check(!loaded.loadFromJson(Json("Moonlight")), "a string is refused");
	Check(loaded.name == "Corridor Spot", "and the light is left as it was");
}

static void TestTypeNames()
{
	std::printf("each light type has a name for the inspector\n");

	Check(std::string(Lighting::lightTypeToString(Lighting::LIGHT_DIRECTIONAL)) == "Directional", "Directional");
	Check(std::string(Lighting::lightTypeToString(Lighting::LIGHT_POINT)) == "Point", "Point");
	Check(std::string(Lighting::lightTypeToString(Lighting::LIGHT_SPOT)) == "Spot", "Spot");
	Check(std::string(Lighting::lightTypeToString(42)) == "Unknown", "anything else is Unknown");
}

// ─── The rig ───────────────────────────────────────────────────────────────

static void TestTheDefaultRigHasAShadowCastingMoon()
{
	std::printf("the default rig is a shadow-casting moon and a lamp over the spawn\n");
	LightingSystem& lighting = FreshLighting();
	lighting.AddLight(MakeSpot());

	lighting.CreateDefaultRig();
	const auto& lights = lighting.GetLights();

	Check(lights.size() == 2, "it has two lights, replacing what was there");
	if (lights.size() == 2)
	{
		Check(lights[0].type == Lighting::LIGHT_DIRECTIONAL && lights[0].castsShadow, "the first is directional and casts the shadow");
		Check(lights[1].type == Lighting::LIGHT_POINT && !lights[1].castsShadow, "the second is a point light that does not");
		Check(lights[0].isEnabled && lights[1].isEnabled, "both are on");
	}
}

static void TestAddingAndRemovingLights()
{
	std::printf("lights are added at the end and removed by index\n");
	LightingSystem& lighting = FreshLighting();

	Light a = {};
	a.name = "A";
	Light b = {};
	b.name = "B";
	Light c = {};
	c.name = "C";

	Light* added = lighting.AddLight(a);
	Check(added != nullptr && added->name == "A", "AddLight hands back the stored light");
	lighting.AddLight(b);
	lighting.AddLight(c);

	lighting.RemoveLight(1);
	const auto& lights = lighting.GetLights();
	Check(lights.size() == 2 && lights[0].name == "A" && lights[1].name == "C", "removing the middle light keeps the order of the rest");

	lighting.RemoveLight(-1);
	lighting.RemoveLight(2);
	lighting.RemoveLight(100);
	Check(lighting.GetLights().size() == 2, "removing an index that is not there does nothing");

	lighting.ClearLights();
	Check(lighting.GetLights().empty(), "ClearLights empties the rig");
	Check(lighting.GetShadowLightIndex() == -1, "and leaves nothing owning the shadow map");
}

// ─── The rig's save data ───────────────────────────────────────────────────

static void TestTheRigRoundTrips()
{
	std::printf("the rig and its atmosphere round-trip through save data\n");
	LightingSystem& lighting = FreshLighting();
	lighting.AddLight(MakeSpot());
	Light lamp = {};
	lamp.name = "Lamp";
	lighting.AddLight(lamp);
	lighting.ambientColor = Color{ 1, 2, 3, 4 };
	lighting.ambientStrength = 0.33f;
	lighting.fogColor = Color{ 5, 6, 7, 8 };
	lighting.fogDensity = 0.07f;
	lighting.exposure = 1.6f;
	lighting.specularStrength = 0.4f;
	lighting.shininess = 24.0f;
	lighting.shadowsEnabled = false;
	lighting.shadowBias = 0.003f;
	lighting.shadowDistance = 45.0f;
	lighting.shadowSoftness = 1.5f;
	lighting.SetShadowResolution(1024);

	const Json saved = lighting.formatToJson();
	FreshLighting();
	Check(lighting.loadFromJson(saved), "the saved rig loads");

	const auto& lights = lighting.GetLights();
	Check(lights.size() == 2 && lights[0].name == "Corridor Spot" && lights[1].name == "Lamp", "both lights, in order");
	Check(SameColor(lighting.ambientColor, Color{ 1, 2, 3, 4 }) && lighting.ambientStrength == 0.33f, "ambient");
	Check(SameColor(lighting.fogColor, Color{ 5, 6, 7, 8 }) && lighting.fogDensity == 0.07f, "fog");
	Check(lighting.exposure == 1.6f && lighting.specularStrength == 0.4f && lighting.shininess == 24.0f, "exposure and specular");
	Check(!lighting.shadowsEnabled && lighting.shadowBias == 0.003f && lighting.shadowDistance == 45.0f
		&& lighting.shadowSoftness == 1.5f, "shadow tuning");
	Check(lighting.GetShadowResolution() == 1024, "shadow resolution");
}

static void TestAWorldWithMoreThanTheLimitKeepsTheFirstLights()
{
	std::printf("a world with more lights than the shader takes keeps the first LIGHT_LIMIT\n");
	LightingSystem& lighting = FreshLighting();

	Json data = Json::object();
	Json lights = Json::array();
	for (int i = 0; i < Lighting::LIGHT_LIMIT + 3; ++i)
	{
		Light light = {};
		light.name = "Light " + std::to_string(i);
		lights.push_back(light.formatToJson());
	}
	data["Lights"] = lights;

	Check(lighting.loadFromJson(data), "the oversized rig loads");
	Check(static_cast<int>(lighting.GetLights().size()) == Lighting::LIGHT_LIMIT, "with exactly LIGHT_LIMIT lights");
	Check(lighting.GetLights().back().name == "Light " + std::to_string(Lighting::LIGHT_LIMIT - 1), "the extras at the end are the ones dropped");
}

static void TestAWorldWithNoLightsIsStillLit()
{
	std::printf("a world that saves no usable lights gets the default rig\n");
	LightingSystem& lighting = FreshLighting();

	Check(lighting.loadFromJson(Json{ { "Lights", Json::array() } }), "an empty light list loads");
	Check(lighting.GetLights().size() == 2, "and the default rig is put in");

	FreshLighting();
	Check(lighting.loadFromJson(Json{ { "Lights", { "not a light", 4, nullptr } } }), "a list of things that are not lights loads");
	Check(lighting.GetLights().size() == 2, "and also gets the default rig");

	FreshLighting();
	Check(!lighting.loadFromJson(Json::array()), "rig data that is not an object is refused");
}

static void TestTheFlashlightIsNeverSaved()
{
	std::printf("the player's flashlight is never written into a world\n");
	LightingSystem& lighting = FreshLighting();
	lighting.CreateDefaultRig();
	lighting.SetFlashlightEnabled(true);
	lighting.GetFlashlight().name = "Flashlight";

	const Json saved = lighting.formatToJson();
	Check(saved["Lights"].size() == 2, "only the world's two lights are saved");
	for (const Json& light : saved["Lights"])
	{
		Check(light.value("Name", "") != "Flashlight", light.value("Name", "?") + " is not the flashlight");
	}

	lighting.ToggleFlashlight();
	Check(!lighting.IsFlashlightEnabled(), "toggling turns it off");
	lighting.ToggleFlashlight();
	Check(lighting.IsFlashlightEnabled(), "and on again");
}

// ─── The player's quality ──────────────────────────────────────────────────

static void TestTheShadowResolutionIsClamped()
{
	std::printf("a world's shadow map size is kept between 256 and 8192\n");
	LightingSystem& lighting = FreshLighting();

	lighting.SetShadowResolution(16);
	Check(lighting.GetShadowResolution() == 256, "16 is raised to 256");
	lighting.SetShadowResolution(100000);
	Check(lighting.GetShadowResolution() == 8192, "100000 is lowered to 8192");
	lighting.SetShadowResolution(3000);
	Check(lighting.GetShadowResolution() == 3000, "3000 is kept as it is");

	FreshLighting();
	lighting.loadFromJson(Json{ { "ShadowResolution", 1 } });
	Check(lighting.GetShadowResolution() == 256, "a saved size of 1 loads as 256");
}

static void TestEachShadowsSettingCapsTheMap()
{
	std::printf("each Shadows setting caps the drawn map at its own size\n");
	LightingSystem& lighting = FreshLighting();
	lighting.SetShadowResolution(8192);

	const int caps[5] = { 0, 512, 1024, 2048, 4096 };
	const char* names[5] = { "Off", "Low", "Medium", "High", "Ultra" };
	for (int quality = 0; quality < 5; ++quality)
	{
		lighting.SetPlayerQuality(quality, 1.0f);
		Check(lighting.DrawnShadowMapSize() == caps[quality],
			std::string(names[quality]) + " draws an 8192 world's map at " + std::to_string(caps[quality]));
		Check(lighting.ShadowsDrawn() == (quality > 0), std::string(names[quality]) + (quality > 0 ? " draws shadows" : " draws none"));
	}

	lighting.SetPlayerQuality(-3, 1.0f);
	Check(!lighting.ShadowsDrawn(), "a setting below Off is Off");
	lighting.SetPlayerQuality(12, 1.0f);
	Check(lighting.DrawnShadowMapSize() == 4096, "a setting above Ultra is Ultra");
}

static void TestQualityNeverRaisesAWorldsMap()
{
	std::printf("the player's quality never draws a map larger than the world's own\n");
	LightingSystem& lighting = FreshLighting();

	lighting.SetShadowResolution(1024);
	lighting.SetPlayerQuality(4, 1.0f);
	Check(lighting.DrawnShadowMapSize() == 1024, "Ultra draws a 1024 world at 1024");

	lighting.shadowsEnabled = false;
	Check(!lighting.ShadowsDrawn() && lighting.DrawnShadowMapSize() == 0, "a world with shadows off draws none at Ultra");
}

static void TestLuminosityScalesTheDrawnExposure()
{
	std::printf("Luminosity scales the exposure drawn, never below zero\n");
	LightingSystem& lighting = FreshLighting();
	lighting.exposure = 2.0f;

	lighting.SetPlayerQuality(4, 0.5f);
	Check(lighting.DrawnExposure() == 1.0f, "50% of a 2.0 exposure draws 1.0");
	Check(lighting.GetPlayerExposure() == 0.5f, "the player's own value is kept");

	lighting.SetPlayerQuality(4, -1.0f);
	Check(lighting.GetPlayerExposure() == 0.0f && lighting.DrawnExposure() == 0.0f, "a negative Luminosity is read as 0");
	Check(lighting.exposure == 2.0f, "and the world's exposure is never changed");
}

int main()
{
	// Nothing here touches the GPU, but the engine objects this links expect
	// the window the game always has.
	SetTraceLogLevel(LOG_ERROR);
	InitWindow(64, 64, "veil-lighting-tests");

	std::printf("Lighting tests\n\n");

	TestALightRoundTripsEveryField();
	TestAnUnknownLightTypeLoadsAsAPoint();
	TestALightMissingFieldsKeepsItsDefaults();
	TestALightThatIsNotAnObjectIsRefused();
	TestTypeNames();

	TestTheDefaultRigHasAShadowCastingMoon();
	TestAddingAndRemovingLights();

	TestTheRigRoundTrips();
	TestAWorldWithMoreThanTheLimitKeepsTheFirstLights();
	TestAWorldWithNoLightsIsStillLit();
	TestTheFlashlightIsNeverSaved();

	TestTheShadowResolutionIsClamped();
	TestEachShadowsSettingCapsTheMap();
	TestQualityNeverRaisesAWorldsMap();
	TestLuminosityScalesTheDrawnExposure();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);

	CloseWindow();
	return g_failures == 0 ? 0 : 1;
}
