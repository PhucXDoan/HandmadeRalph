#undef UNICODE
#define UNICODE true
#pragma warning(push)
#pragma warning(disable : 5039)
#include <windows.h>
#pragma warning(pop)
#include "unified.h"

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
			DEBUG_printf("WM_CLOSE\n");
			return 0;
		} break;

		case WM_DESTROY:
		{
			DEBUG_printf("WM_DESTROY\n");
			return 0;
		} break;

		case WM_SIZE:
		{
			DEBUG_printf("WM_SIZE\n");
			return 0;
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			BeginPaint(window, &paint);

			PatBlt(paint.hdc, paint.rcPaint.left, paint.rcPaint.top, paint.rcPaint.right - paint.rcPaint.left, paint.rcPaint.bottom - paint.rcPaint.top, BLACKNESS);

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
		}
	}

	return 0;
}
