#undef UNICODE
#define UNICODE true
#pragma warning(push)
#pragma warning(disable : 5039)
#include <windows.h>
#pragma warning(pop)
#include "unified.h"

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
			{
				RECT client_rect;
				GetClientRect(window, &client_rect);
				g_backbuffer_bitmap_info.bmiHeader.biWidth  = client_rect.right - client_rect.left;
				g_backbuffer_bitmap_info.bmiHeader.biHeight = client_rect.top   - client_rect.bottom; // @NOTE@ Top-down bitmap.
			}

			if (g_backbuffer_bitmap_data)
			{
				VirtualFree(g_backbuffer_bitmap_data, 0, MEM_RELEASE);
			}

			g_backbuffer_bitmap_data = reinterpret_cast<byte*>(VirtualAlloc(0, static_cast<size_t>(4) * g_backbuffer_bitmap_info.bmiHeader.biWidth * -g_backbuffer_bitmap_info.bmiHeader.biHeight, MEM_COMMIT, PAGE_READWRITE));

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

	constexpr wchar_t CLASS_NAME[] = L"HandemadeRalphWindowClass";

	WNDCLASSEXW window_class = {};
	window_class.cbSize        = sizeof(window_class);
	window_class.style         = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
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
		offset_x += 1;
		offset_y += 2;

		FOR_RANGE(y, -g_backbuffer_bitmap_info.bmiHeader.biHeight)
		{
			FOR_RANGE(x, g_backbuffer_bitmap_info.bmiHeader.biWidth)
			{
				reinterpret_cast<u32*>(g_backbuffer_bitmap_data)[y * g_backbuffer_bitmap_info.bmiHeader.biWidth + x] =
					static_cast<u32>(static_cast<u8>(offset_x                + x) << 16) |
					static_cast<u32>(static_cast<u8>(offset_y                + y) <<  8) |
					static_cast<u32>(static_cast<u8>(offset_x + offset_y + x + y) <<  0);
			}
		}

		HDC device_context = GetDC(window);
		DEFER { ReleaseDC(window, device_context); };

		RECT client_rect;
		GetClientRect(window, &client_rect);

		StretchDIBits
		(
			device_context,
			0,
			0,
			g_backbuffer_bitmap_info.bmiHeader.biWidth,
			-g_backbuffer_bitmap_info.bmiHeader.biHeight,
			0,
			0,
			client_rect.right  - client_rect.left,
			client_rect.bottom - client_rect.top,
			g_backbuffer_bitmap_data,
			&g_backbuffer_bitmap_info,
			DIB_RGB_COLORS,
			SRCCOPY
		);
	}
}
