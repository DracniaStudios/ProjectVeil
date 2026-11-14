#include <UIEngine.h>
#include <Platform.h>
#include <Render/Render.h>

// Everything the UI needs from the renderer and the platform, kept apart from its rules in
// UIEngine.cpp, which calls no raylib function and so is tested without a window.
namespace UI {

	int RenderBackend::TextWidth(const std::string& text, int fontSize) const
	{
		return Render::TextWidth(text, fontSize);
	}

	void RenderBackend::Draw(const std::vector<DrawCommand>& commands)
	{
		for (const DrawCommand& command : commands)
		{
			switch (command.kind)
			{
			case DrawCommand::Kind::Rect:
				Render::Rect(command.rect, command.color);
				break;
			case DrawCommand::Kind::Outline:
				Render::RectOutline(command.rect, command.thickness, command.color);
				break;
			case DrawCommand::Kind::Text:
				Render::Text(command.text, command.position, command.fontSize, command.color);
				break;
			}
		}
	}

	Input ReadPlatformInput(bool mouseBlocked)
	{
		Input input;
		input.screenSize = Vector2{ static_cast<float>(Platform::ScreenWidth()), static_cast<float>(Platform::ScreenHeight()) };
		input.mouse = Platform::MousePosition();
		input.mouseDown = Platform::IsMouseButtonDown(Platform::MouseButton::Left);
		input.mouseReleased = Platform::IsMouseButtonReleased(Platform::MouseButton::Left);
		input.mouseBlocked = mouseBlocked;
		return input;
	}

	UIEngine& UIEngine::getInstance()
	{
		static RenderBackend backend;
		static UIEngine instance(backend); // Guaranteed to be destroyed and instantiated on first use
		return instance;
	}
}
