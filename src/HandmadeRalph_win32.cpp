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

#define PROCESS_PLATFORM_BUTTON(BUTTON, IS_DOWN) MACRO_CONCAT(g_platform_input.button, BUTTON) = static_cast<u8>(((MACRO_CONCAT(g_platform_input.button, BUTTON) + ((MACRO_CONCAT(g_platform_input.button, BUTTON) >> 7) != (IS_DOWN))) & 0b01111111) | ((IS_DOWN) << 7))

#define  XInputGetState_t(NAME) DWORD NAME(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef  XInputGetState_t(XInputGetState_t);
internal XInputGetState_t(stub_XInputGetState) { return ERROR_DEVICE_NOT_CONNECTED; }
global   XInputGetState_t* g_XInputGetState = stub_XInputGetState;

#define  DirectSoundCreate_t(NAME) HRESULT WINAPI NAME(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
typedef  DirectSoundCreate_t(DirectSoundCreate_t);

global vi2           g_client_dimensions             = { 0, 0 };
global PlatformInput g_platform_input                = {};
global i32           g_unfreed_file_data_counter     = 0;
global i64           g_performance_counter_frequency =

	[](void)
	{
		LARGE_INTEGER n;
		QueryPerformanceFrequency(&n);
		return n.QuadPart;
	}();

#if DEBUG
#define HANDMADE_RALPH_DLL_FILE_PATH      EXE_DIR L"HandmadeRalph.dll"
#define HANDMADE_RALPH_DLL_FILE_PATH_TEMP HANDMADE_RALPH_DLL_FILE_PATH ".temp"

struct HandmadeRalphDLL
{
	HMODULE           handle;
	FILETIME          write_time;
	PlatformUpdate_t* PlatformUpdate;
	PlatformSound_t*  PlatformSound;
};

internal HandmadeRalphDLL DEBUG_load_handmade_ralph_dll(void)
{
	if (!CopyFileW(HANDMADE_RALPH_DLL_FILE_PATH, HANDMADE_RALPH_DLL_FILE_PATH_TEMP, false))
	{
		ASSERT(false);
		return {};
	}

	WIN32_FILE_ATTRIBUTE_DATA dll_attribute_data;
	if (!GetFileAttributesExW(HANDMADE_RALPH_DLL_FILE_PATH, GetFileExInfoStandard, &dll_attribute_data))
	{
		ASSERT(false);
		return {};
	}

	HandmadeRalphDLL dll =
		{
			.handle         = LoadLibraryW(HANDMADE_RALPH_DLL_FILE_PATH_TEMP),
			.write_time     = dll_attribute_data.ftLastWriteTime,
			.PlatformUpdate = reinterpret_cast<PlatformUpdate_t*>(GetProcAddress(dll.handle, "PlatformUpdate")),
			.PlatformSound  = reinterpret_cast<PlatformSound_t *>(GetProcAddress(dll.handle, "PlatformSound"))
		};


	ASSERT(dll.handle);
	ASSERT(dll.PlatformUpdate);
	ASSERT(dll.PlatformSound);

	DEBUG_printf(__FUNCTION__ " :: Hotloaded.\n");

	return dll;
}
#endif

PlatformReadFileData_t(PlatformReadFileData)
{
	HANDLE handle = CreateFileW(platform_file_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (handle == INVALID_HANDLE_VALUE)
	{
		DEBUG_printf(__FUNCTION__ " :: Couldn't open file `%S` for reading.\n", platform_file_path);
		return false;
	}
	DEFER { CloseHandle(handle); };

	LARGE_INTEGER file_size;
	if (!GetFileSizeEx(handle, &file_size))
	{
		DEBUG_printf(__FUNCTION__ " :: Couldn't get size of `%S`.\n", platform_file_path);
		return false;
	}

	platform_file_data->size = static_cast<u64>(file_size.QuadPart),
	platform_file_data->data = reinterpret_cast<byte*>(VirtualAlloc(0, platform_file_data->size, MEM_COMMIT, PAGE_READWRITE));

	// @TODO@ Larger file sizes.
	if (platform_file_data->size > 0xFFFFFFFF)
	{
		DEBUG_printf(__FUNCTION__ " :: File `%S` is too big (`%zu` bytes); must be less than 2^32 bytes (4.29GB).\n", platform_file_path, platform_file_data->size);
		return false;
	}

	if (!platform_file_data->data)
	{
		DEBUG_printf(__FUNCTION__ " :: Couldn't allocate `%zu` bytes for `%S`.\n", platform_file_data->size, platform_file_path);
		return false;
	}

	DWORD read_size;
	if (!ReadFile(handle, platform_file_data->data, static_cast<u32>(platform_file_data->size), &read_size, 0))
	{
		DEBUG_printf(__FUNCTION__ " :: Couldn't read file `%S`.\n", platform_file_path);
		VirtualFree(platform_file_data->data, 0, MEM_RELEASE);
		return false;
	}

	if (read_size != platform_file_data->size)
	{
		DEBUG_printf(__FUNCTION__ " :: Incomplete read of `%ld` out of `%zu` bytes of `%S`.\n", read_size, platform_file_data->size, platform_file_path);
		VirtualFree(platform_file_data->data, 0, MEM_RELEASE);
		return false;
	}

	g_unfreed_file_data_counter += 1;
	return true;
}

PlatformFreeFileData_t(PlatformFreeFileData)
{
	ASSERT(platform_file_data->data);
	VirtualFree(platform_file_data->data, 0, MEM_RELEASE);
	g_unfreed_file_data_counter -= 1;
}

PlatformWriteFile_t(PlatformWriteFile)
{
	HANDLE handle = CreateFileW(platform_file_path, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (handle == INVALID_HANDLE_VALUE)
	{
		DEBUG_printf(__FUNCTION__ " :: Couldn't open file `%S` for writing.\n", platform_file_path);
		return false;
	}
	DEFER { CloseHandle(handle); };

	// @TODO@ Larger file sizes.
	if (platform_write_size > 0xFFFFFFFF)
	{
		DEBUG_printf(__FUNCTION__ " :: `%zu` bytes is too big to write out to `%S`; must be less than 2^32 bytes (4.29GB).\n", platform_write_size, platform_file_path);
		return false;
	}

	DWORD write_size;
	if (!WriteFile(handle, platform_write_data, static_cast<u32>(platform_write_size), &write_size, 0))
	{
		DEBUG_printf(__FUNCTION__ " :: Couldn't write into `%S`.\n", platform_file_path);
		return false;
	}

	if (write_size != platform_write_size)
	{
		DEBUG_printf(__FUNCTION__ " :: Incomplete write of `%ld` out of `%zu` bytes to `%S`.\n", write_size, platform_write_size, platform_file_path);
		return false;
	}

	return true;
}

internal i64 query_performance_counter(void)
{
	LARGE_INTEGER n;
	QueryPerformanceCounter(&n);
	return n.QuadPart;
}

internal f32 calc_performance_counter_delta_time(i64 start, i64 end)
{
	return static_cast<f32>(end - start) / g_performance_counter_frequency;
}

internal void poll_gamepads(void)
{
	FOR_RANGE(i, min(PLATFORM_GAMEPAD_MAX, XUSER_MAX_COUNT))
	{
		XINPUT_STATE gamepad_state;
		if (g_XInputGetState(static_cast<DWORD>(i), &gamepad_state) == ERROR_SUCCESS)
		{
			g_platform_input.gamepads[i].connected = true;

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

			// @TODO@ Better deadzoning.
			constexpr f32 DEADZONE_MIN = 0.075f;
			constexpr f32 DEADZONE_MAX = 0.8f;
			g_platform_input.gamepads[i].stick_left.x  = sign(gamepad_state.Gamepad.sThumbLX) * clamp((fabsf(gamepad_state.Gamepad.sThumbLX / 32768.0f) - DEADZONE_MIN) / (DEADZONE_MAX - DEADZONE_MIN), 0.0f, 1.0f);
			g_platform_input.gamepads[i].stick_left.y  = sign(gamepad_state.Gamepad.sThumbLY) * clamp((fabsf(gamepad_state.Gamepad.sThumbLY / 32768.0f) - DEADZONE_MIN) / (DEADZONE_MAX - DEADZONE_MIN), 0.0f, 1.0f);
			g_platform_input.gamepads[i].stick_right.x = sign(gamepad_state.Gamepad.sThumbRX) * clamp((fabsf(gamepad_state.Gamepad.sThumbRX / 32768.0f) - DEADZONE_MIN) / (DEADZONE_MAX - DEADZONE_MIN), 0.0f, 1.0f);
			g_platform_input.gamepads[i].stick_right.y = sign(gamepad_state.Gamepad.sThumbRY) * clamp((fabsf(gamepad_state.Gamepad.sThumbRY / 32768.0f) - DEADZONE_MIN) / (DEADZONE_MAX - DEADZONE_MIN), 0.0f, 1.0f);
		}
		else
		{
			g_platform_input.gamepads[i].connected = false;
		}
	}
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
						case VK_MENU   : { PROCESS_PLATFORM_BUTTON(.alt,         is_down); } break;
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
			DEBUG_printf("XInput :: Could not load `xinput1_4.dll`.\n");
			xinput_dll = LoadLibraryW(L"xinput1_3.dll");
			if (!xinput_dll)
			{
				DEBUG_printf("XInput :: Could not load `xinput1_3.dll`.\n");
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

	constexpr i32 SAMPLES_PER_SECOND   = 48000;
	constexpr i32 SOUNDBUFFER_CAPACITY = 1 * SAMPLES_PER_SECOND;
	constexpr i32 SOUNDBUFFER_SIZE     = SOUNDBUFFER_CAPACITY * sizeof(PlatformSample);

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
				.dwBufferBytes = SOUNDBUFFER_SIZE,
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
		if (directsound_buffer->Lock(0, SOUNDBUFFER_SIZE, reinterpret_cast<void**>(&region_0), &region_size_0, reinterpret_cast<void**>(&region_1), &region_size_1, 0) == DS_OK)
		{
			memset(region_0, 0, region_size_0);
			memset(region_1, 0, region_size_1);
			directsound_buffer->Unlock(region_0, region_size_0, region_1, region_size_1);
		}

		directsound_buffer->Play(0, 0, DSBPLAY_LOOPING);
	}

	//
	// Miscellaneous initializations.
	//

	constexpr f32        SECONDS_PER_UPDATE              = 1.0f / 30.0f;
	constexpr i32        SAMPLES_OF_LATENCY              = SAMPLES_PER_SECOND / 30;
	constexpr i32        MAXIMUM_SAMPLES_PER_UPDATE      = static_cast<i32>(SAMPLES_PER_SECOND * SECONDS_PER_UPDATE + 1);
	constexpr i32        PLATFORM_SAMPLE_BUFFER_CAPACITY = 4 * MAXIMUM_SAMPLES_PER_UPDATE;
	constexpr BITMAPINFO BACKBUFFER_BITMAP_INFO          =
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

	bool32          is_sleep_granular      = timeBeginPeriod(1) == TIMERR_NOERROR;
	byte*           backbuffer_bitmap_data = reinterpret_cast<byte*>(VirtualAlloc(0, static_cast<size_t>(4) * BACKBUFFER_BITMAP_INFO.bmiHeader.biWidth * -BACKBUFFER_BITMAP_INFO.bmiHeader.biHeight, MEM_COMMIT, PAGE_READWRITE));
	PlatformSample* platform_sample_buffer = reinterpret_cast<PlatformSample*>(VirtualAlloc(0, PLATFORM_SAMPLE_BUFFER_CAPACITY * sizeof(PlatformSample), MEM_COMMIT, PAGE_READWRITE));
	byte*           platform_memory        =
		#if DEBUG
		reinterpret_cast<byte*>(VirtualAlloc(reinterpret_cast<void*>(TEBIBYTES_OF(8)), PLATFORM_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		#else
		reinterpret_cast<byte*>(VirtualAlloc(0, PLATFORM_MEMORY_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		#endif

	ASSERT(backbuffer_bitmap_data);
	ASSERT(platform_memory);

	HandmadeRalphDLL handmade_ralph_dll = DEBUG_load_handmade_ralph_dll();

	bool32 is_computed_sample_on_time = false;
	i32    last_computed_sample_index = 0;
	i64    performance_counter_start;
	while (true)
	{
		performance_counter_start = query_performance_counter();

		//
		// Input.
		//

		for (MSG message; PeekMessageW(&message, 0, 0, 0, PM_REMOVE);)
		{
			TranslateMessage(&message);
			DispatchMessage(&message);

			if (message.message == WM_QUIT)
			{
				if (message.wParam)
				{
					DEBUG_printf(__FILE__ " :: Windows exit code `%llu`.\n", message.wParam);
				}
				goto BREAK;
			}
		}

		poll_gamepads();

		PlatformInput* platform_input = &g_platform_input;

		#if DEBUG
		//
		// (Debug) Pause.
		//

		{
			DEBUG_persist bool32 is_paused = false;
			if (BUTTON_DOWN(g_platform_input.button.alt) && BUTTON_PRESSES(g_platform_input.button.numbers[0]))
			{
				is_paused = !is_paused;
			}
			if (is_paused)
			{
				goto PAUSE_END;
			}
		}

		//
		// (Debug) Hotloading.
		//

		{
			WIN32_FILE_ATTRIBUTE_DATA dll_attribute_data;
			if
			(
				GetFileAttributesExW(HANDMADE_RALPH_DLL_FILE_PATH, GetFileExInfoStandard, &dll_attribute_data)
				&& !(dll_attribute_data.ftLastWriteTime.dwHighDateTime == handmade_ralph_dll.write_time.dwHighDateTime && dll_attribute_data.ftLastWriteTime.dwLowDateTime == handmade_ralph_dll.write_time.dwLowDateTime)
				&& GetFileAttributesW(L"" LOCK_FILE_PATH) == INVALID_FILE_ATTRIBUTES
			)
			{
				FreeLibrary(handmade_ralph_dll.handle);
				handmade_ralph_dll = DEBUG_load_handmade_ralph_dll();
			}
		}

		//
		// (Debug) Playback.
		//

		{
			enum struct PlaybackState : u8
			{
				null,
				recording,
				replaying
			};

			DEBUG_persist PlaybackState playback_state        = PlaybackState::null;
			DEBUG_persist HANDLE        playback_file         = INVALID_HANDLE_VALUE;
			DEBUG_persist HANDLE        playback_file_mapping = INVALID_HANDLE_VALUE;
			DEBUG_persist byte*         playback_data         = 0;
			DEBUG_persist u64           playback_size         = 0;
			DEBUG_persist i32           playback_input_index  = 0;

			constexpr const wchar_t* PLAYBACK_FILE_PATH = EXE_DIR L"HandmadeRalph.playback";
			lambda stop_playback =
				[&]()
				{
					playback_state = PlaybackState::null;
					if (playback_file != INVALID_HANDLE_VALUE)
					{
						CloseHandle(playback_file);
						playback_file = INVALID_HANDLE_VALUE;
					}
					if (playback_file_mapping != INVALID_HANDLE_VALUE)
					{
						CloseHandle(playback_file_mapping);
						playback_file_mapping = INVALID_HANDLE_VALUE;
					}
					if (playback_data)
					{
						UnmapViewOfFile(playback_data);
						playback_data = 0;
					}
				};

			if (BUTTON_DOWN(g_platform_input.button.alt) && BUTTON_PRESSES(g_platform_input.button.numbers[1]))
			{
				switch (playback_state)
				{
					case PlaybackState::null:
					{
						playback_state = PlaybackState::recording;

						playback_file = CreateFileW(PLAYBACK_FILE_PATH, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
						if (playback_file == INVALID_HANDLE_VALUE)
						{
							DEBUG_printf(__FILE__ " :: Failed to create `%S` for playback.\n", PLAYBACK_FILE_PATH);
							stop_playback();
							break;
						}

						DWORD resulting_write_size;
						if (!WriteFile(playback_file, platform_memory, PLATFORM_MEMORY_SIZE, &resulting_write_size, 0) || resulting_write_size != PLATFORM_MEMORY_SIZE)
						{
							DEBUG_printf(__FILE__ " :: Recorded `%lu` out of `%zu` bytes of memory to `%S`; aborting playback.\n", resulting_write_size, PLATFORM_MEMORY_SIZE, PLAYBACK_FILE_PATH);
							stop_playback();
							break;
						}

						DEBUG_printf(__FILE__ " :: Recording `%S`.\n", PLAYBACK_FILE_PATH);
					} break;

					case PlaybackState::recording:
					{
						playback_state = PlaybackState::replaying;

						playback_file_mapping = CreateFileMappingW(playback_file, 0, PAGE_READWRITE, 0, 0, 0);
						if (playback_file_mapping == INVALID_HANDLE_VALUE)
						{
							DEBUG_printf(__FILE__ " :: Failed to file map `%S`; aborting playback.\n", PLAYBACK_FILE_PATH);
							stop_playback();
							break;
						}

						playback_data = reinterpret_cast<byte*>(MapViewOfFile(playback_file_mapping, FILE_MAP_READ, 0, 0, 0));
						if (!playback_data)
						{
							DEBUG_printf(__FILE__ " :: Failed to get a map view of `%S`; aborting playback.\n", PLAYBACK_FILE_PATH);
							stop_playback();
							break;
						}

						LARGE_INTEGER playback_file_size;
						if (GetFileSizeEx(playback_file, &playback_file_size))
						{
							playback_size = static_cast<u64>(playback_file_size.QuadPart);
						}
						else
						{
							DEBUG_printf(__FILE__ " :: Failed to get size of `%S`; aborting playback.\n", PLAYBACK_FILE_PATH);
							stop_playback();
							break;
						}

						ASSERT(playback_size > PLATFORM_MEMORY_SIZE);
						ASSERT((playback_size - PLATFORM_MEMORY_SIZE) % sizeof(PlatformInput) == 0);

						playback_input_index = 0;

						DEBUG_printf(__FILE__ " :: Replaying `%S`.\n", PLAYBACK_FILE_PATH);
					} break;

					case PlaybackState::replaying:
					{
						stop_playback();
						DEBUG_printf(__FILE__ " :: Stopped `%S`.\n", PLAYBACK_FILE_PATH);
					} break;
				}
			}

			if (playback_state == PlaybackState::recording)
			{
				DWORD resulting_write_size;
				if (!WriteFile(playback_file, &g_platform_input, sizeof(PlatformInput), &resulting_write_size, 0) || resulting_write_size != sizeof(PlatformInput))
				{
					DEBUG_printf(__FILE__ " :: Recorded `%lu` out of `%zu` bytes of input to `%S`; aborting playback.\n", resulting_write_size, sizeof(PlatformInput), PLAYBACK_FILE_PATH);
					stop_playback();
				}
			}
			else if (playback_state == PlaybackState::replaying)
			{
				if (playback_input_index == 0)
				{
					memcpy(platform_memory, playback_data, PLATFORM_MEMORY_SIZE);
				}

				platform_input        = &reinterpret_cast<PlatformInput*>(playback_data + PLATFORM_MEMORY_SIZE)[playback_input_index];
				playback_input_index += 1;
				playback_input_index %= (playback_size - PLATFORM_MEMORY_SIZE) / sizeof(PlatformInput);
			}
		}
		#endif

		//
		// Update.
		//

		handmade_ralph_dll.PlatformUpdate
		(
			reinterpret_cast<u32*>(backbuffer_bitmap_data),
			{ BACKBUFFER_BITMAP_INFO.bmiHeader.biWidth, -BACKBUFFER_BITMAP_INFO.bmiHeader.biHeight },
			platform_input,
			platform_memory,
			SECONDS_PER_UPDATE,
			PlatformReadFileData,
			PlatformFreeFileData,
			PlatformWriteFile
		);

		//
		// Sound.
		//

		{
			DWORD player_byte_offset;
			DWORD writer_byte_offset;
			if (directsound_buffer->GetCurrentPosition(&player_byte_offset, &writer_byte_offset) == DS_OK)
			{
				ASSERT(player_byte_offset % sizeof(PlatformSample) == 0);
				ASSERT(writer_byte_offset % sizeof(PlatformSample) == 0);

				if (!is_computed_sample_on_time)
				{
					is_computed_sample_on_time = true;
					last_computed_sample_index = static_cast<i32>(writer_byte_offset / sizeof(PlatformSample));
				}

				i32 player_sample_index            = static_cast<i32>(player_byte_offset / sizeof(PlatformSample));
				i32 abs_last_computed_sample_index = last_computed_sample_index;
				if (abs_last_computed_sample_index < player_sample_index)
				{
					abs_last_computed_sample_index += SOUNDBUFFER_CAPACITY;
				}

				if (abs_last_computed_sample_index >= static_cast<i32>(writer_byte_offset / sizeof(PlatformSample)) + SOUNDBUFFER_CAPACITY * (writer_byte_offset < player_byte_offset))
				{
					i32 abs_sample_index_after_next_update = static_cast<i32>(ceilf((SECONDS_PER_UPDATE - calc_performance_counter_delta_time(performance_counter_start, query_performance_counter())) * SAMPLES_PER_SECOND));
					i32 new_computed_sample_count          = player_sample_index + abs_sample_index_after_next_update - abs_last_computed_sample_index + (1 + (MAXIMUM_SAMPLES_PER_UPDATE + SAMPLES_OF_LATENCY - abs_sample_index_after_next_update) / MAXIMUM_SAMPLES_PER_UPDATE) * MAXIMUM_SAMPLES_PER_UPDATE;
					if (new_computed_sample_count > 0)
					{
						if (new_computed_sample_count > PLATFORM_SAMPLE_BUFFER_CAPACITY)
						{
							DEBUG_printf(__FILE__ " :: Too many samples are needed to be computed (`%d`); maximum is `%d`.\n", new_computed_sample_count, PLATFORM_SAMPLE_BUFFER_CAPACITY);
							is_computed_sample_on_time = false;
						}
						else
						{
							byte* region_0;
							DWORD region_size_0;
							byte* region_1;
							DWORD region_size_1;
							if (directsound_buffer->Lock(last_computed_sample_index * sizeof(PlatformSample), new_computed_sample_count * sizeof(PlatformSample), reinterpret_cast<void**>(&region_0), &region_size_0, reinterpret_cast<void**>(&region_1), &region_size_1, 0) == DS_OK)
							{
								handmade_ralph_dll.PlatformSound(platform_sample_buffer, new_computed_sample_count, SAMPLES_PER_SECOND, platform_memory);

								PlatformSample* curr_sample = platform_sample_buffer;

								ASSERT(region_size_0 % sizeof(PlatformSample) == 0);
								FOR_ELEMS(sample, reinterpret_cast<u32*>(region_0), static_cast<i32>(region_size_0 / sizeof(PlatformSample)))
								{
									*sample      = curr_sample->sample;
									curr_sample += 1;
								}

								ASSERT(region_size_1 % sizeof(PlatformSample) == 0);
								FOR_ELEMS(sample, reinterpret_cast<u32*>(region_1), static_cast<i32>(region_size_1 / sizeof(PlatformSample)))
								{
									*sample      = curr_sample->sample;
									curr_sample += 1;
								}

								last_computed_sample_index = (last_computed_sample_index + new_computed_sample_count) % SOUNDBUFFER_CAPACITY;
								directsound_buffer->Unlock(region_0, region_size_0, region_1, region_size_1);
							}
							else
							{
								DEBUG_printf("DirectSound :: Failed to lock soundbuffer.\n");
								is_computed_sample_on_time = false;
							}
						}
					}
				}
				else
				{
					DEBUG_printf(__FILE__ " :: Cursor played over uncomputed audio in the soundbuffer.\n");
					is_computed_sample_on_time = false;
				}
			}
			else
			{
				DEBUG_printf("DirectSound :: Failed to get play/write position in soundbuffer.\n");
				is_computed_sample_on_time = false;
			}
		}

		//
		// Flip and wait.
		//

		#if DEBUG
		PAUSE_END:;
		#endif

		FOR_ELEMS(g_platform_input.buttons)
		{
			*it &= 0b10000000;
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
			// @TODO@ Remove spinlock.
			// @TODO@ Gamepad polls are done after a sleep...
			f32 computation_time = calc_performance_counter_delta_time(performance_counter_start, query_performance_counter());
			if (computation_time < SECONDS_PER_UPDATE)
			{
				if (is_sleep_granular)
				{
					while (SECONDS_PER_UPDATE - computation_time > 0.04f)
					{
						Sleep(1);
						poll_gamepads();

						computation_time = calc_performance_counter_delta_time(performance_counter_start, query_performance_counter());
						if (computation_time >= SECONDS_PER_UPDATE)
						{
							DEBUG_printf(__FILE__ " :: Missed frame from sleeping.\n");
						}
					}
				}

				while (computation_time < SECONDS_PER_UPDATE)
				{
					computation_time = calc_performance_counter_delta_time(performance_counter_start, query_performance_counter());
				}
			}
			else
			{
				DEBUG_printf(__FILE__ " :: Missed frame from computing.\n");
			}
		}

		#if 0
		{ // @NOTE@ For getting all Windows error. Some errors are the result of successful operations and therefore false positives.
			DWORD last_error = GetLastError();
			if (last_error)
			{
				wchar_t* buffer;
				ASSERT(FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, last_error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, 0));
				DEBUG_printf(__FILE__ " :: Latest Windows error (Code `%lu`).\n\t%S", last_error, buffer);
				LocalFree(buffer);
			}
		}
		#endif
	}
	BREAK:;

	if (g_unfreed_file_data_counter)
	{
		DEBUG_printf(__FILE__ " :: `%d` unfreed file data.\n", g_unfreed_file_data_counter);
		return 1;
	}

	return 0;
}
