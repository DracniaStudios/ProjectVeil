#include <UIEngine.h>

// Everything the UI needs from raylib, kept apart from its rules in UIEngine.cpp,
// which calls no raylib function and so is tested without a window.
namespace UI {

	int RaylibBackend::TextWidth(const std::string& text, int fontSize) const
	{
		return MeasureText(text.c_str(), fontSize);
	}

	void RaylibBackend::Draw(const std::vector<DrawCommand>& commands)
	{
		for (const DrawCommand& command : commands)
		{
			switch (command.kind)
			{
			case DrawCommand::Kind::Rect:
				DrawRectangleRec(command.rect, command.color);
				break;
			case DrawCommand::Kind::Outline:
				DrawRectangleLinesEx(command.rect, command.thickness, command.color);
				break;
			case DrawCommand::Kind::Text:
				DrawText(command.text.c_str(), static_cast<int>(command.position.x), static_cast<int>(command.position.y),
					command.fontSize, command.color);
				break;
			}
		}
	}

	Input ReadRaylibInput(bool mouseBlocked)
	{
		Input input;
		input.screenSize = Vector2{ static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()) };
		input.mouse = GetMousePosition();
		input.mouseDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
		input.mouseReleased = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
		input.mouseBlocked = mouseBlocked;
		return input;
	}

	UIEngine& UIEngine::getInstance()
	{
		static RaylibBackend backend;
		static UIEngine instance(backend); // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}
}
