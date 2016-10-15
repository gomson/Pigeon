#define WIN32_LEAN_AND_MEAN
// TODO: #include <minwindef.h>?
#include <Windows.h>
// TODO: Switch to WRL ComPtr
#include <atlbase.h> //CComPtr

#include "shared.hpp"
#include "notification.hpp"
#include "audio.hpp"
#include "video.hpp"

// TODO: Move to resources?
static const c16* GUIDSTR_PIGEON = L"{C1FA11EF-FC16-46DF-A268-104F59E94672}";

int CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, i32 nCmdShow)
{
	b32 success;
	u32 uResult;
	i32 iResult;
	HRESULT hr;
	MSG msg = {};


	// TODO: Use a different animation timing method. SetTimer is not precise enough (rounds to multiples of 15.6ms)
	// TODO: Pigeon image on startup
	// TODO: Pigeon sounds
	// TODO: SetProcessDPIAware?
	// TODO: Log failures
	// Using gotos is a pretty bad idea. It skips initialization of local variables and they'll be filled with garbage.

	// TODO: Hotkey to restart
	// TODO: Sound doesn't play on most devices when cycling audio devices
	// TODO: Look for a way to start faster at login (using Startup folder seems to take quite a few seconds)
	// TODO: Integrate volume ducking?
	// https://msdn.microsoft.com/en-us/library/windows/desktop/dd940522(v=vs.85).aspx
	// TODO: Use RawInput to get hardware key so it's not Logitech/Corsair profile dependent?
	// TODO: Auto-detect headset being turned on/off
	// TODO: Test with mutliple users. Might need use Local\ namespace for the event

	// NOTE: Handles are closed when process terminates.
	// Events are destroyed when the last handle is destroyed.
	HANDLE singleInstanceEvent = CreateEventW(nullptr, false, false, GUIDSTR_PIGEON);
	if (!singleInstanceEvent) return false;

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		success = SetEvent(singleInstanceEvent);
		if (!success) return false;

		uResult = WaitForSingleObject(singleInstanceEvent, 1000);
		if (uResult == WAIT_TIMEOUT) return false;
		if (uResult == WAIT_FAILED ) return false;
	}

	hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) return false;


	// Notification
	NotificationWindow notification = {};
	{
		// NOTE: QPC and QPF are documented as not being able to fail on XP+
		LARGE_INTEGER tickFrequency = {};
		QueryPerformanceFrequency(&tickFrequency);

		f64 tickFrequencyF64 = (f64) tickFrequency.QuadPart;

		notification.windowMinWidth    = 200; //TODO: Implement
		notification.windowMaxWidth    = 600; //TODO: Implement
		notification.windowSize        = {200, 60};
		notification.windowPosition    = { 50, 60};
		notification.backgroundColor   = RGBA(16, 16, 16, 242);
		notification.textColorNormal   = RGB(255, 255, 255);
		notification.textColorError    = RGB(255, 0, 0);
		notification.textColorWarning  = RGB(255, 255, 0);
		notification.textPadding       = 20;
		notification.animShowTicks     = 0.1 * tickFrequencyF64;
		notification.animIdleTicks     = 2.0 * tickFrequencyF64;
		notification.animHideTicks     = 1.0 * tickFrequencyF64;
		notification.animUpdateMS      = 1000 / 30;
		notification.timerID           = 1;
		notification.tickFrequency     = tickFrequencyF64;

		// TODO: Queue. Ensure notification is ready
		//if (notification.animUpdateMS < USER_TIMER_MINIMUM)
		//	Notify(&notification, L"Animation update time is less than allowed minimum.", Error::Warning);
	}


	// Window
	{
		WNDCLASSW windowClass = {};
		windowClass.style         = 0; // TODO: CS_DROPSHADOW?
		windowClass.lpfnWndProc   = NotificationWndProc;
		windowClass.cbClsExtra    = 0;
		windowClass.cbWndExtra    = sizeof(&notification);
		windowClass.hInstance     = hInstance;
		windowClass.hIcon         = nullptr;
		windowClass.hCursor       = nullptr;
		windowClass.hbrBackground = nullptr;
		windowClass.lpszMenuName  = nullptr;
		windowClass.lpszClassName = L"Pigeon Notification Class";

		ATOM classAtom = RegisterClassW(&windowClass);
		if (classAtom == INVALID_ATOM) goto Cleanup;

		notification.hwnd = CreateWindowExW(
			WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
			windowClass.lpszClassName,
			L"Pigeon Notification Window",
			WS_POPUP,
			notification.windowPosition.x,
			notification.windowPosition.y,
			notification.windowSize.cx,
			notification.windowSize.cy,
			nullptr,
			nullptr,
			hInstance,
			&notification
		);
		if (notification.hwnd == INVALID_HANDLE_VALUE) goto Cleanup;
	}


	// Hotkeys
	const int cycleAudioDeviceHotkeyID = 0;
	success = RegisterHotKey(nullptr, cycleAudioDeviceHotkeyID, MOD_WIN | MOD_NOREPEAT, VK_F5);
	if (!success) goto Cleanup;

	const int openPlaybackDevicesHotkeyID = 1;
	success = RegisterHotKey(nullptr, openPlaybackDevicesHotkeyID, MOD_CONTROL | MOD_WIN | MOD_NOREPEAT, VK_F5);
	if (!success) goto Cleanup;

	const int cycleRefreshRateHotkeyID = 2;
	success = RegisterHotKey(nullptr, cycleRefreshRateHotkeyID, MOD_WIN | MOD_NOREPEAT, VK_F6);
	if (!success) goto Cleanup;

	const int openDisplayAdapterSettingsHotkeyID = 3;
	success = RegisterHotKey(nullptr, openDisplayAdapterSettingsHotkeyID, MOD_CONTROL | MOD_WIN | MOD_NOREPEAT, VK_F6);
	if (!success) goto Cleanup;

	// TODO: BELOW_NORMAL_PRIORITY_CLASS?
	success = SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
	if (!success) goto Cleanup;


	//DEBUG
	Notify(&notification, L"Started!");


	// Message pump
	bool quit = false;
	while (!quit)
	{
		//TODO: I'm unsure if this releases on ALL possible messages
		uResult = MsgWaitForMultipleObjects(1, &singleInstanceEvent, false, INFINITE, QS_ALLINPUT | QS_ALLPOSTMESSAGE);
		if (uResult == WAIT_OBJECT_0) PostQuitMessage(0);
		if (uResult == WAIT_FAILED  ) PostQuitMessage(1);

		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);

			switch (msg.message)
			{
				case WM_HOTKEY:
				{
					switch (msg.wParam)
					{
						case cycleAudioDeviceHotkeyID:
							CycleDefaultAudioDevice(&notification);
							break;

						case openPlaybackDevicesHotkeyID:
							OpenAudioPlaybackDevicesWindow();
							break;

						case cycleRefreshRateHotkeyID:
							CycleRefreshRate();
							break;

						case openDisplayAdapterSettingsHotkeyID:
							OpenDisplayAdapterSettingsWindow();
							break;
					}
					break;
				}

				case WM_QUIT:
					quit = true;
					break;

				#define UxdDisplayChangeMessage 0xC22D

				// Expected messages
				case WM_TIMER:
				case UxdDisplayChangeMessage:
					break;

				// TODO: Disable key messages?
				//       Somehow we're getting key messages, but only sometimes
				case WM_KEYDOWN: //256
				case WM_KEYUP: //257
					//Fall through to default

				default:
					c16 buffer[256] = {};
					swprintf(buffer, L"Unexpected message: %d\n", msg.message);
					Notify(&notification, buffer, Error::Warning);
			}
		}
	}

Cleanup:
	CoUninitialize();

	// TODO: These may be unnecessary, but I don't know what guarantees Windows
	// makes about when SetEvent will cause waiting threads to release. If the
	// release happens immediately, the hotkeys need to be unregistered first.
	UnregisterHotKey(nullptr, cycleAudioDeviceHotkeyID);
	UnregisterHotKey(nullptr, openPlaybackDevicesHotkeyID);
	UnregisterHotKey(nullptr, cycleRefreshRateHotkeyID);
	UnregisterHotKey(nullptr, openDisplayAdapterSettingsHotkeyID);
	SetEvent(singleInstanceEvent);

	// Leak all the things!
	// (Windows destroys everything automatically)

	// TODO: This is wrong
	return LOWORD(msg.wParam);
}