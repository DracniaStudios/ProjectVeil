/**
 * Standalone tests for the InputSystem's action table.
 *
 * Gameplay, the Overlays and the World Editor read the keyboard and gamepad
 * through actions, never through keys, so the table is the whole of what a key
 * means. These cases cover what the table holds: every action has a binding,
 * no two gameplay actions answer the same key or button, and a lookup outside
 * the table finds nothing instead of reading past it.
 *
 * InputSystem.cpp reads raylib's input state, so raylib is linked, but no
 * window is opened: with none, raylib reports every key and button up. That is
 * the one input state this suite can drive, so it tests the table and the
 * lookups rather than live key presses.
 *
 * Build and run: tests/run_tests.sh
 */

#include <InputSystem.h>

#include <cstdio>
#include <map>
#include <string>

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

static InputSystem::InputSystem& Input()
{
	return InputSystem::InputSystem::getInstance();
}

// The actions read while gameplay or an Overlay has input. The editor's two
// are read only while the World Editor flies its camera, so they may share a
// key with these.
static bool IsGameplayAction(int action)
{
	return action != InputSystem::ACTION_EDITOR_UP && action != InputSystem::ACTION_EDITOR_DOWN;
}

static void TestEveryActionHasADefaultBinding()
{
	std::printf("every action starts with a name, a key and a button\n");

	Check(Input().inputActions.size() == InputSystem::MAX_ACTION, "the table holds one slot per action");

	for (int action = InputSystem::NO_ACTION + 1; action < InputSystem::MAX_ACTION; ++action)
	{
		const InputSystem::InputAction* input = Input().GetAction(action);
		const std::string label = "action " + std::to_string(action);

		Check(input != nullptr, label + " can be looked up");
		if (input == nullptr) { continue; }

		Check(input->id == action, label + " is stored under its own id");
		Check(!input->name.empty(), label + " has a name for the controls menu");
		Check(input->key != KEY_NULL, label + " (" + input->name + ") has a key");
		Check(input->button >= 0, label + " (" + input->name + ") has a gamepad button");
		Check(input->isEnabled, label + " (" + input->name + ") starts enabled");
	}
}

static void TestNoTwoGameplayActionsShareAKey()
{
	std::printf("no two gameplay actions answer the same key\n");

	std::map<int, std::string> owners;
	for (int action = InputSystem::NO_ACTION + 1; action < InputSystem::MAX_ACTION; ++action)
	{
		if (!IsGameplayAction(action)) { continue; }
		const InputSystem::InputAction* input = Input().GetAction(action);

		const auto [existing, inserted] = owners.emplace(input->key, input->name);
		Check(inserted, input->name + " has a key of its own (shared with " + existing->second + ")");
	}
}

static void TestNoTwoGameplayActionsShareAButton()
{
	std::printf("no two gameplay actions answer the same gamepad button\n");

	std::map<int, std::string> owners;
	for (int action = InputSystem::NO_ACTION + 1; action < InputSystem::MAX_ACTION; ++action)
	{
		if (!IsGameplayAction(action)) { continue; }
		const InputSystem::InputAction* input = Input().GetAction(action);

		const auto [existing, inserted] = owners.emplace(input->button, input->name);
		Check(inserted, input->name + " has a button of its own (shared with " + existing->second + ")");
	}
}

static void TestTheEditorCameraLeavesControlFree()
{
	std::printf("the editor's camera keys leave Ctrl to the editor's hotkeys\n");

	// Every Ctrl+ editor hotkey reads Left Control directly. Bound to an editor
	// action as well, holding Ctrl for any hotkey flew the camera too.
	Check(Input().GetAction(InputSystem::ACTION_EDITOR_UP)->key != KEY_LEFT_CONTROL, "Editor Up is not Left Control");
	Check(Input().GetAction(InputSystem::ACTION_EDITOR_DOWN)->key != KEY_LEFT_CONTROL, "Editor Down is not Left Control");
}

static void TestTheInventoryActionIsTab()
{
	std::printf("the Inventory action, which also backs out of Overlays, is on Tab\n");

	const InputSystem::InputAction* inventory = Input().GetAction(InputSystem::ACTION_UI_INVENTORY);
	Check(inventory->key == KEY_TAB, "Inventory is bound to Tab");
	Check(inventory->name == "Inventory", "and is named for the controls menu");
}

static void TestLookupsOutsideTheTableFindNothing()
{
	std::printf("an id outside the table finds no action\n");

	Check(Input().GetAction(InputSystem::NO_ACTION) == nullptr, "NO_ACTION is not an action");
	Check(Input().GetAction(InputSystem::MAX_ACTION) == nullptr, "MAX_ACTION is past the end");
	Check(Input().GetAction(-1) == nullptr, "a negative id finds nothing");
	Check(Input().GetAction(10000) == nullptr, "a large id finds nothing");
}

static void TestUnknownActionsAreNeverDown()
{
	std::printf("an unknown action reads as up, pressed by nobody and never released\n");

	for (int action : { static_cast<int>(InputSystem::NO_ACTION), static_cast<int>(InputSystem::MAX_ACTION), -7 })
	{
		const std::string label = "action " + std::to_string(action);
		Check(!Input().IsActionDown(action), label + " is not down");
		Check(!Input().IsActionPressed(action), label + " is not pressed");
		Check(!Input().IsActionReleased(action), label + " is not released");
	}
}

static void TestNothingIsDownWithoutInput()
{
	std::printf("with no keyboard or gamepad input, no action is down\n");

	bool anyDown = false;
	for (int action = InputSystem::NO_ACTION + 1; action < InputSystem::MAX_ACTION; ++action)
	{
		if (Input().IsActionDown(action) || Input().IsActionPressed(action) || Input().IsActionReleased(action))
		{
			anyDown = true;
		}
	}
	Check(!anyDown, "every action reads as up");
}

static void TestCreateActionNeverOverwrites()
{
	std::printf("creating an action that already exists changes nothing\n");

	const InputSystem::InputAction before = *Input().GetAction(InputSystem::ACTION_MOVE_JUMP);
	Input().CreateAction(InputSystem::ACTION_MOVE_JUMP, "Not Jump", KEY_Z, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
	const InputSystem::InputAction* after = Input().GetAction(InputSystem::ACTION_MOVE_JUMP);

	Check(after->name == before.name, "the name is kept");
	Check(after->key == before.key, "the key is kept");
	Check(after->button == before.button, "the button is kept");

	// Out of range is refused without touching the table
	Input().CreateAction(InputSystem::MAX_ACTION, "Past The End", KEY_Z, 0);
	Check(Input().inputActions.size() == InputSystem::MAX_ACTION, "an out-of-range action does not grow the table");
}

static void TestSetActionRebinds()
{
	std::printf("SetAction rebinds an action's key and button, and nothing else\n");

	InputSystem::InputAction* sprint = Input().GetAction(InputSystem::ACTION_MOVE_SPRINT);
	const InputSystem::InputAction original = *sprint;

	Input().SetAction(InputSystem::ACTION_MOVE_SPRINT, KEY_RIGHT_SHIFT, -1);
	Check(sprint->key == KEY_RIGHT_SHIFT, "the key is the new one");
	Check(sprint->button == -1, "the button is cleared");
	Check(sprint->name == original.name && sprint->id == original.id, "the name and id are untouched");
	Check(!Input().IsActionDown(InputSystem::ACTION_MOVE_SPRINT), "an action with no button still reads safely");

	Input().SetAction(InputSystem::ACTION_MOVE_SPRINT, original.key, original.button);
	Check(sprint->key == original.key && sprint->button == original.button, "rebinding back restores the default");
}

static void TestSetActionOnAnUnknownActionChangesNothing()
{
	std::printf("rebinding an unknown action changes no binding\n");

	std::map<int, int> keys;
	for (int action = InputSystem::NO_ACTION + 1; action < InputSystem::MAX_ACTION; ++action)
	{
		keys[action] = Input().GetAction(action)->key;
	}

	Input().SetAction(InputSystem::NO_ACTION, KEY_Z, 0);
	Input().SetAction(InputSystem::MAX_ACTION, KEY_Z, 0);

	bool unchanged = true;
	for (const auto& [action, key] : keys)
	{
		if (Input().GetAction(action)->key != key) { unchanged = false; }
	}
	Check(unchanged, "every action keeps its key");
}

static void TestADisabledActionKeepsItsBinding()
{
	std::printf("disabling an action keeps its binding for when it is enabled again\n");

	InputSystem::InputAction* interact = Input().GetAction(InputSystem::ACTION_MOVE_INTERACT);
	const int key = interact->key;

	interact->isEnabled = false;
	Check(!Input().IsActionDown(InputSystem::ACTION_MOVE_INTERACT), "a disabled action is not down");
	Check(!Input().IsActionPressed(InputSystem::ACTION_MOVE_INTERACT), "nor pressed");

	interact->isEnabled = true;
	Check(interact->key == key, "and keeps its key while it is off");
}

static void TestUpdateWithoutInputHoldsNoKeys()
{
	std::printf("a frame with no key presses records none\n");

	Input().Update();
	Check(Input().GetPressedKeys().empty(), "no key was pressed this frame");
	Check(Input().GetHeldKeys().empty(), "no key is held");
	Check(Input().GetLastPressedKey() == KEY_NULL, "the last pressed key is KEY_NULL");

	Input().Update();
	Check(Input().GetPressedKeys().empty() && Input().GetHeldKeys().empty(), "and the next frame is the same");
}

int main()
{
	std::printf("InputSystem tests\n\n");

	TestEveryActionHasADefaultBinding();
	TestNoTwoGameplayActionsShareAKey();
	TestNoTwoGameplayActionsShareAButton();
	TestTheEditorCameraLeavesControlFree();
	TestTheInventoryActionIsTab();
	TestLookupsOutsideTheTableFindNothing();
	TestUnknownActionsAreNeverDown();
	TestNothingIsDownWithoutInput();
	TestCreateActionNeverOverwrites();
	TestSetActionRebinds();
	TestSetActionOnAnUnknownActionChangesNothing();
	TestADisabledActionKeepsItsBinding();
	TestUpdateWithoutInputHoldsNoKeys();

	std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
