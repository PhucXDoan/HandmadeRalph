#undef UNICODE
#define UNICODE true
#pragma warning(push)
#pragma warning(disable : 5039 4820)
#include <windows.h>
#include <xinput.h>
#include <dsound.h>
#pragma warning(pop)
#include "unified.h"
#include "platform.h"

#include "HandmadeRalph.cpp"

#define PROCESS_PLATFORM_BUTTON(BUTTON, IS_DOWN) MACRO_CONCAT_(g_platform_input.button, BUTTON) = static_cast<u8>(((MACRO_CONCAT_(g_platform_input.button, BUTTON) + ((MACRO_CONCAT_(g_platform_input.button, BUTTON) >> 7) != (IS_DOWN))) & 0b01111111) | ((IS_DOWN) << 7))

#define  XInputGetState_t(NAME) DWORD NAME(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef  XInputGetState_t(XInputGetState_t);
internal XInputGetState_t(stub_XInputGetState) { return ERROR_DEVICE_NOT_CONNECTED; }
global   XInputGetState_t* g_XInputGetState = stub_XInputGetState;

#define  DirectSoundCreate_t(NAME) HRESULT WINAPI NAME(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef  DirectSoundCreate_t(DirectSoundCreate_t);

global vi2           g_client_dimensions             = { 0, 0 };
global PlatformInput g_platform_input                = {};
global i64           g_performance_counter_frequency =
	[](void)
	{
		LARGE_INTEGER n;
		QueryPerformanceFrequency(&n);
		return n.QuadPart;
	}();

internal i64 query_performance_counter(void)
{
	LARGE_INTEGER n;
	QueryPerformanceCounter(&n);
	return n.QuadPart;
}

internal f64 calc_performance_counter_delta_time(i64 start, i64 end)
{
	return static_cast<f64>(end - start) / g_performance_counter_frequency;
}

internal LRESULT window_procedure_callback(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
		case WM_CLOSE:
		{
			PostQuitMessage(0);
			return 0;
		} break;

		case WM_SIZE:
		{
			RECT client_rect;
			GetClientRect(window, &client_rect);
			g_client_dimensions = { client_rect.right - client_rect.left, client_rect.bottom - client_rect.top };
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
			bool8 was_down = (lparam & (1 << 30)) != 0;
			bool8 is_down  = (lparam & (1 << 31)) == 0;
			if (!(was_down && is_down))
			{
				if (IN_RANGE(wparam, 'A', 'Z' + 1))
				{
					PROCESS_PLATFORM_BUTTON(.letters[wparam - 'A'], is_down);
				}
				else if (IN_RANGE(wparam, '0', '9' + 1))
				{
					PROCESS_PLATFORM_BUTTON(.numbers[wparam - '0'], is_down);
				}
				else
				{
					switch (wparam)
					{
						case VK_LEFT   : { PROCESS_PLATFORM_BUTTON(.arrow_left,  is_down); } break;
						case VK_RIGHT  : { PROCESS_PLATFORM_BUTTON(.arrow_right, is_down); } break;
						case VK_DOWN   : { PROCESS_PLATFORM_BUTTON(.arrow_down,  is_down); } break;
						case VK_UP     : { PROCESS_PLATFORM_BUTTON(.arrow_up,    is_down); } break;
						case VK_RETURN : { PROCESS_PLATFORM_BUTTON(.enter,       is_down); } break;
						case VK_SHIFT  : { PROCESS_PLATFORM_BUTTON(.shift,       is_down); } break;

						case VK_F4:
						{
							if (lparam & (1 << 29))
							{
								PostQuitMessage(0);
							}
						} break;
					}
				}
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
	//
	// Initialize window.
	//

	constexpr wchar_t CLASS_NAME[] = L"HandemadeRalphWindowClass";

	WNDCLASSEXW window_class =
		{
			.cbSize        = sizeof(window_class),
			.style         = CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc   = window_procedure_callback,
			.hInstance     = instance,
			.lpszClassName = CLASS_NAME
		};

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

	//
	// Initialize XInput.
	//

	{
		HMODULE xinput_dll = LoadLibraryW(L"xinput1_4.dll");
		if (!xinput_dll)
		{
			DEBUG_printf("XInput:: Could not load `xinput1_4.dll`.\n");
			xinput_dll = LoadLibraryW(L"xinput1_3.dll");
			if (!xinput_dll)
			{
				DEBUG_printf("XInput:: Could not load `xinput1_3.dll`.\n");
			}
		}
		if (xinput_dll)
		{
			g_XInputGetState = reinterpret_cast<XInputGetState_t*>(GetProcAddress(xinput_dll, "XInputGetState"));
			ASSERT(g_XInputGetState);
		}
	}

	//
	// Initialize DirectSound.
	//

	constexpr i32 SAMPLES_PER_SECOND = 48000;
	constexpr i32 SAMPLE_SIZE        = 2 * sizeof(i16);
	constexpr u32 SOUND_BUFFER_SIZE  = 1 * SAMPLES_PER_SECOND * SAMPLE_SIZE;

	IDirectSoundBuffer* directsound_buffer;
	{
		HMODULE directsound_dll = LoadLibraryW(L"dsound.dll");
		if (!directsound_dll)
		{
			DEBUG_printf("DirectSound :: Could not load `dsound.dll`.\n");
			return -1;
		}

		DirectSoundCreate_t* l_DirectSoundCreate = reinterpret_cast<DirectSoundCreate_t*>(GetProcAddress(directsound_dll, "DirectSoundCreate"));
		ASSERT(l_DirectSoundCreate);

		IDirectSound* directsound;
		if (l_DirectSoundCreate(0, &directsound, 0) != DS_OK)
		{
			DEBUG_printf("DirectSound :: Could not create object.\n");
			return -1;
		}

		if (directsound->SetCooperativeLevel(window, DSSCL_PRIORITY) != DS_OK)
		{
			DEBUG_printf("DirectSound :: Could not set a cooperative level.\n");
			return -1;
		}

		DSBUFFERDESC primary_buffer_description =
			{
				.dwSize  = sizeof(primary_buffer_description),
				.dwFlags = DSBCAPS_PRIMARYBUFFER
			};
		IDirectSoundBuffer* primary_buffer;
		if (directsound->CreateSoundBuffer(&primary_buffer_description, &primary_buffer, 0) != DS_OK)
		{
			DEBUG_printf("DirectSound :: Could not create primary buffer.\n");
			return -1;
		}

		WAVEFORMATEX primary_buffer_format = {};
		primary_buffer_format.wFormatTag      = WAVE_FORMAT_PCM;
		primary_buffer_format.nChannels       = 2;
		primary_buffer_format.nSamplesPerSec  = SAMPLES_PER_SECOND;
		primary_buffer_format.wBitsPerSample  = 16;
		primary_buffer_format.nBlockAlign     = primary_buffer_format.nChannels * primary_buffer_format.wBitsPerSample / 8U;
		primary_buffer_format.nAvgBytesPerSec = primary_buffer_format.nSamplesPerSec * primary_buffer_format.nBlockAlign;
		if (primary_buffer->SetFormat(&primary_buffer_format) != DS_OK)
		{
			DEBUG_printf("DirectSound :: Could not set format of primary buffer.\n");
			return -1;
		}

		DSBUFFERDESC secondary_buffer_description =
			{
				.dwSize        = sizeof(secondary_buffer_description),
				.dwBufferBytes = SOUND_BUFFER_SIZE,
				.lpwfxFormat   = &primary_buffer_format
			};
		if (directsound->CreateSoundBuffer(&secondary_buffer_description, &directsound_buffer, 0) != DS_OK)
		{
			DEBUG_printf("DirectSound :: Could not create secondary buffer.\n");
			return -1;
		}

		byte* region_0;
		DWORD region_size_0;
		byte* region_1;
		DWORD region_size_1;
		if (directsound_buffer->Lock(0, SOUND_BUFFER_SIZE, reinterpret_cast<void**>(&region_0), &region_size_0, reinterpret_cast<void**>(&region_1), &region_size_1, 0) == DS_OK)
		{
			memset(region_0, 0, region_size_0);
			memset(region_1, 0, region_size_1);
			directsound_buffer->Unlock(region_0, region_size_0, region_1, region_size_1);
		}

		directsound_buffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	//
	// Loop.
	//

	constexpr BITMAPINFO BACKBUFFER_BITMAP_INFO =
		{
			.bmiHeader =
				{
					.biSize        = sizeof(BACKBUFFER_BITMAP_INFO.bmiHeader),
					.biWidth       =  1080,
					.biHeight      = -720,
					.biPlanes      = 1,
					.biBitCount    = 32,
					.biCompression = BI_RGB
				}
		};

	byte* backbuffer_bitmap_data = reinterpret_cast<byte*>(VirtualAlloc(0, static_cast<size_t>(4) * BACKBUFFER_BITMAP_INFO.bmiHeader.biWidth * -BACKBUFFER_BITMAP_INFO.bmiHeader.biHeight, MEM_COMMIT, PAGE_READWRITE));

	u32 curr_sample_index = 0;

	f64 frame_time    = 0.0;
	i64 counter_start = query_performance_counter();
	while (true)
	{
		i64 counter_end = query_performance_counter();
		f64 delta_time  = calc_performance_counter_delta_time(counter_start, counter_end);
		counter_start = counter_end;

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

		FOR_RANGE(i, XUSER_MAX_COUNT)
		{
			XINPUT_STATE gamepad_state;
			if (g_XInputGetState(static_cast<DWORD>(i), &gamepad_state) == ERROR_SUCCESS)
			{
				PROCESS_PLATFORM_BUTTON(.gamepads[i].action_left,    (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_X             ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].action_right,   (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_B             ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].action_down,    (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_A             ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].action_up,      (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y             ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].dpad_up,        (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP       ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].dpad_down,      (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN     ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].dpad_left,      (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT     ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].dpad_right,     (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT    ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].shoulder_left,  (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].shoulder_right, (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].stick_left,     (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB    ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].stick_right,    (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB   ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].start,          (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_START         ) != 0);
				PROCESS_PLATFORM_BUTTON(.gamepads[i].back,           (gamepad_state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK          ) != 0);

				g_platform_input.gamepads[i].trigger_left  = gamepad_state.Gamepad.bLeftTrigger  / 255.0f;
				g_platform_input.gamepads[i].trigger_right = gamepad_state.Gamepad.bRightTrigger / 255.0f;
				g_platform_input.gamepads[i].stick_left    = vi2 { gamepad_state.Gamepad.sThumbLX, gamepad_state.Gamepad.sThumbLY } / 32768.0f;
				g_platform_input.gamepads[i].stick_right   = vi2 { gamepad_state.Gamepad.sThumbRX, gamepad_state.Gamepad.sThumbRY } / 32768.0f;
			}
		}

		PlatformFramebuffer platform_framebuffer =
			{
				.dimensions = { BACKBUFFER_BITMAP_INFO.bmiHeader.biWidth, -BACKBUFFER_BITMAP_INFO.bmiHeader.biHeight },
				.pixels     = reinterpret_cast<u32*>(backbuffer_bitmap_data)
			};

		frame_time += delta_time;

		constexpr f64 SECONDS_PER_UPDATE = 1.0 / 60.0;
		if (frame_time >= SECONDS_PER_UPDATE)
		{
			frame_time -= SECONDS_PER_UPDATE;

			PlatformUpdate(&platform_framebuffer, &g_platform_input);

			FOR_ELEMS(g_platform_input.buttons)
			{
				*it &= 0b10000000;
			}
		}

		{
			HDC device_context = GetDC(window);
			StretchDIBits
			(
				device_context,
				0,
				0,
				g_client_dimensions.x,
				g_client_dimensions.y,
				0,
				0,
				BACKBUFFER_BITMAP_INFO.bmiHeader.biWidth,
				-BACKBUFFER_BITMAP_INFO.bmiHeader.biHeight,
				backbuffer_bitmap_data,
				&BACKBUFFER_BITMAP_INFO,
				DIB_RGB_COLORS,
				SRCCOPY
			);
			ReleaseDC(window, device_context);
		}

		{
			DWORD play_offset;
			DWORD write_offset;
			if (directsound_buffer->GetCurrentPosition(&play_offset, &write_offset) == DS_OK)
			{
				constexpr DWORD SOUND_LATENCY = SAMPLES_PER_SECOND / 15 * SAMPLE_SIZE; // @TODO@ Better latency.

				DWORD target_offset = (play_offset + SOUND_LATENCY) % SOUND_BUFFER_SIZE;
				DWORD sample_offset = curr_sample_index * SAMPLE_SIZE % SOUND_BUFFER_SIZE;
				DWORD write_size    = target_offset - sample_offset;
				if (target_offset < sample_offset)
				{
					write_size = SOUND_BUFFER_SIZE + write_size;
				}

				byte* region_0;
				DWORD region_size_0;
				byte* region_1;
				DWORD region_size_1;
				if (directsound_buffer->Lock(sample_offset, write_size, reinterpret_cast<void**>(&region_0), &region_size_0, reinterpret_cast<void**>(&region_1), &region_size_1, 0) == DS_OK)
				{
					ASSERT(region_size_0 % SAMPLE_SIZE == 0);
					FOR_ELEMS(sample, reinterpret_cast<u32*>(region_0), static_cast<i32>(region_size_0 / SAMPLE_SIZE))
					{
						*sample = PlatformSound(SAMPLES_PER_SECOND).sample;
					}

					ASSERT(region_size_1 % SAMPLE_SIZE == 0);
					FOR_ELEMS(sample, reinterpret_cast<u32*>(region_1), static_cast<i32>(region_size_1 / SAMPLE_SIZE))
					{
						*sample = PlatformSound(SAMPLES_PER_SECOND).sample;
					}

					curr_sample_index += static_cast<i32>(region_size_0 / SAMPLE_SIZE) + static_cast<i32>(region_size_1 / SAMPLE_SIZE);
					directsound_buffer->Unlock(region_0, region_size_0, region_1, region_size_1);
				}
			}
		}
	}
}
