#include <iostream>
#include "../../Public/MainGame/BaseGameClass/Game.h"
#include "../../Public/InputDevice/InputDevice.h"


InputDevice::InputDevice(Game* inGame) : game(inGame)
{
	RAWINPUTDEVICE Rid[2];

	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x02;
	Rid[0].dwFlags = 0;   // adds HID mouse and also ignores legacy mouse messages
	Rid[0].hwndTarget = game->GetDisplay()->GetHwnd();

	Rid[1].usUsagePage = 0x01;
	Rid[1].usUsage = 0x06;
	Rid[1].dwFlags = 0;   // adds HID keyboard and also ignores legacy keyboard messages
	Rid[1].hwndTarget = game->GetDisplay()->GetHwnd();

	if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
	{
		auto errorCode = GetLastError();
		std::cout << "ERROR: " << errorCode << std::endl;
	}
}

InputDevice::~InputDevice() = default;

void InputDevice::OnKeyDown(KeyboardInputEventArgs args)
{
	bool Break = args.Flags & 0x01;

	auto key = static_cast<Keys>(args.VKey);

	if (args.MakeCode == 42) key = Keys::LeftShift;
	if (args.MakeCode == 54) key = Keys::RightShift;

	if(Break) {
		RemovePressedKey(key);
	} else {
		AddPressedKey(key);
	}
}

void InputDevice::OnMouseMove(RawMouseEventArgs args)
{
	if(args.ButtonFlags & static_cast<int>(MouseButtonFlags::LeftButtonDown))
		AddPressedKey(Keys::LeftButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::LeftButtonUp))
		RemovePressedKey(Keys::LeftButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::RightButtonDown))
		AddPressedKey(Keys::RightButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::RightButtonUp))
		RemovePressedKey(Keys::RightButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::MiddleButtonDown))
		AddPressedKey(Keys::MiddleButton);
	if (args.ButtonFlags & static_cast<int>(MouseButtonFlags::MiddleButtonUp))
		RemovePressedKey(Keys::MiddleButton);

	POINT p;
	GetCursorPos(&p);
	ScreenToClient(game->GetDisplay()->GetHwnd(), &p);

	// Several Raw Input events can arrive per frame: accumulate instead of keeping only the last one.
	const glm::vec2 eventOffset(static_cast<float>(args.X), static_cast<float>(args.Y));
	const int eventWheel = (args.ButtonFlags & static_cast<int>(MouseButtonFlags::MouseWheel)) ? args.WheelDelta : 0;
	MousePosition = glm::vec2(p.x, p.y);
	MouseOffset += eventOffset;
	MouseWheelDelta += eventWheel;

	const MouseMoveEventArgs moveArgs = {MousePosition, eventOffset, eventWheel};

	MouseMove.Broadcast(moveArgs);
}

void InputDevice::AddPressedKey(Keys key)
{
	keys.insert(key);
}

void InputDevice::RemovePressedKey(Keys key)
{
	keys.erase(key);
}

bool InputDevice::IsKeyDown(Keys key) const
{
	return keys.count(key) != 0;
}

void InputDevice::ClearPressedKeys()
{
	keys.clear();
}

void InputDevice::EndFrame()
{
	MouseOffset = glm::vec2(0.0f, 0.0f);
	MouseWheelDelta = 0;
}
