#undef UNICODE
#define UNICODE true
#pragma warning(push)
#pragma warning(disable : 5039)
#include <windows.h>
#pragma warning(pop)
#include <xinput.h>
#include "unified.h"

#define  XInputGetState_t(NAME) DWORD NAME(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef  XInputGetState_t(XInputGetState_t);
internal XInputGetState_t(stub_XInputGetState) { return ERROR_DEVICE_NOT_CONNECTED; }
global   XInputGetState_t* g_XInputGetState = stub_XInputGetState;

global constexpr vi2 BACKBUFFER_DIMENSIONS = { 1280, 720 };

global vi2        g_client_dimensions      = { 0, 0 };
global BITMAPINFO g_backbuffer_bitmap_info = {};
global byte*      g_backbuffer_bitmap_data = 0;

internal LRESULT window_procedure_callback(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
		case WM_ACTIVATEAPP:
		{
			DEBUG_printf("WM_ACTIVATEAPP\n");
			return 0;
		} break;

		case WM_CLOSE:
		{
			PostQuitMessage(0);
			return 0;
		} break;

		case WM_DESTROY:
		{
			DEBUG_printf("WM_DESTROY\n");
			return 0;
		} break;

		case WM_SIZE:
		{
			RECT client_rect;
			GetClientRect(window, &client_rect);

			g_client_dimensions = { client_rect.right - client_rect.left, client_rect.bottom - client_rect.top };

			// @NOTE@ Resizes framebuffer to fit the new client dimensions.
			#if 0
			g_backbuffer_bitmap_info.bmiHeader.biWidth  =  g_client_dimensions.x;
			g_backbuffer_bitmap_info.bmiHeader.biHeight = -g_client_dimensions.y;

			if (g_backbuffer_bitmap_data)
			{
				VirtualFree(g_backbuffer_bitmap_data, 0, MEM_RELEASE);
			}

			g_backbuffer_bitmap_data = reinterpret_cast<byte*>(VirtualAlloc(0, static_cast<size_t>(4) * g_backbuffer_bitmap_info.bmiHeader.biWidth * -g_backbuffer_bitmap_info.bmiHeader.biHeight, MEM_COMMIT, PAGE_READWRITE));
			#endif

			return 0;
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			BeginPaint(window, &paint);

			// @TODO@ Paint here...?

			EndPaint(window, &paint);
			return 0;
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			bool32 was_down = (lparam & (1 << 30)) != 0;
			bool32 is_down  = (lparam & (1 << 31)) == 0;

			switch (wparam)
			{
				case 'A':
				{
					if (is_down)
					{
						if (!was_down)
						{
							DEBUG_printf("a\n");
						}
					}
					else if (was_down)
					{
						DEBUG_printf("b\n");
					}
				} break;

				case VK_F4:
				{
					if (is_down && !was_down && (lparam & (1 << 29)))
					{
						PostQuitMessage(0);
					}
				} break;
			}

			return 0;
		} break;

		default:
		{
			return DefWindowProc(window, message, wparam, lparam);
		} break;
	}
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmd_show)
{
	g_backbuffer_bitmap_info.bmiHeader.biSize        = sizeof(g_backbuffer_bitmap_info.bmiHeader);
	g_backbuffer_bitmap_info.bmiHeader.biPlanes      = 1;
	g_backbuffer_bitmap_info.bmiHeader.biBitCount    = 32;
	g_backbuffer_bitmap_info.bmiHeader.biCompression = BI_RGB;

	g_backbuffer_bitmap_info.bmiHeader.biWidth  =  BACKBUFFER_DIMENSIONS.x;
	g_backbuffer_bitmap_info.bmiHeader.biHeight = -BACKBUFFER_DIMENSIONS.y;
	g_backbuffer_bitmap_data = reinterpret_cast<byte*>(VirtualAlloc(0, static_cast<size_t>(4) * g_backbuffer_bitmap_info.bmiHeader.biWidth * -g_backbuffer_bitmap_info.bmiHeader.biHeight, MEM_COMMIT, PAGE_READWRITE));

	constexpr wchar_t CLASS_NAME[] = L"HandemadeRalphWindowClass";

	WNDCLASSEXW window_class = {};
	window_class.cbSize        = sizeof(window_class);
	window_class.style         = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc   = window_procedure_callback;
	window_class.hInstance     = instance;
	window_class.hIcon         = 0; // @TODO@ Icon.
	window_class.lpszClassName = CLASS_NAME;
	window_class.hIconSm       = 0; // @TODO@ Icon.

	if (!RegisterClassExW(&window_class))
	{
		ASSERT(!"Failed to register class.");
		return -1;
	}

	HWND window = CreateWindowExW(0, CLASS_NAME, L"Handmade Ralph", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, instance, 0);
	if (!window)
	{
		ASSERT(!"Failed to create window.");
		return -1;
	}

	{
		HMODULE xinput_dll = LoadLibraryW(L"xinput1_4.dll");
		if (!xinput_dll)
		{
			DEBUG_printf("Could not load `xinput1_4.dll`.\n");
			xinput_dll = LoadLibraryW(L"xinput1_3.dll");
			if (!xinput_dll)
			{
				DEBUG_printf("Could not load `xinput1_3.dll`.\n");
			}
		}
		if (xinput_dll)
		{
			g_XInputGetState = reinterpret_cast<XInputGetState_t*>(GetProcAddress(xinput_dll, "XInputGetState"));
			if (!g_XInputGetState)
			{
				DEBUG_printf("Could not load `XInputGetState`.\n");
				g_XInputGetState = stub_XInputGetState;
			}
		}
	}

	while (true)
	{
		for (MSG message; PeekMessageW(&message, 0, 0, 0, PM_REMOVE);)
		{
			TranslateMessage(&message);
			DispatchMessage(&message);

			if (message.message == WM_QUIT)
			{
				DEBUG_printf("Exit code : %llu\n", message.wParam);
				return 0;
			}
		}

		DEBUG_persist offset_x = 0;
		DEBUG_persist offset_y = 0;

		FOR_RANGE(i, XUSER_MAX_COUNT)
		{
			XINPUT_STATE gamepad_state;
			if (g_XInputGetState(static_cast<DWORD>(i), &gamepad_state) == ERROR_SUCCESS)
			{
				bool8 dpad_up           = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP       ) != 0;
				bool8 dpad_down         = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN     ) != 0;
				bool8 dpad_left         = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT     ) != 0;
				bool8 dpad_right        = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT    ) != 0;
				bool8 misc_start        = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_START         ) != 0;
				bool8 misc_back         = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK          ) != 0;
				bool8 thumb_left        = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB    ) != 0;
				bool8 thumb_right       = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB   ) != 0;
				bool8 shoulder_left     = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ) != 0;
				bool8 shoulder_right    = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
				bool8 button_a          = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_A             ) != 0;
				bool8 button_b          = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_B             ) != 0;
				bool8 button_x          = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_X             ) != 0;
				bool8 button_y          = (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y             ) != 0;
				i32   trigger_left      = gamepad_state.Gamepad.bLeftTrigger;
				i32   trigger_right     = gamepad_state.Gamepad.bRightTrigger;
				vi2   thumb_stick_left  = { gamepad_state.Gamepad.sThumbLX, gamepad_state.Gamepad.sThumbLY };
				vi2   thumb_stick_right = { gamepad_state.Gamepad.sThumbRX, gamepad_state.Gamepad.sThumbRY };

				offset_x += thumb_stick_left.x >> 12;
				offset_y += thumb_stick_left.y >> 12;
			}
			else
			{
			}
		}

		FOR_RANGE(y, -g_backbuffer_bitmap_info.bmiHeader.biHeight)
		{
			FOR_RANGE(x, g_backbuffer_bitmap_info.bmiHeader.biWidth)
			{
				reinterpret_cast<u32*>(g_backbuffer_bitmap_data)[y * g_backbuffer_bitmap_info.bmiHeader.biWidth + x] =
					vxx_argb(vi3 { offset_x + x, offset_y + y, offset_x + offset_y + x + y });
			}
		}

		HDC device_context = GetDC(window);
		DEFER { ReleaseDC(window, device_context); };

		StretchDIBits
		(
			device_context,
			0,
			0,
			g_client_dimensions.x,
			g_client_dimensions.y,
			0,
			0,
			g_backbuffer_bitmap_info.bmiHeader.biWidth,
			-g_backbuffer_bitmap_info.bmiHeader.biHeight,
			g_backbuffer_bitmap_data,
			&g_backbuffer_bitmap_info,
			DIB_RGB_COLORS,
			SRCCOPY
		);
	}
}
