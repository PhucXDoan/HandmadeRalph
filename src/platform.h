#pragma once
#define BUTTON_DOWN(BUTTON)     ((BUTTON) >> 7)
#define BUTTON_PRESSES(BUTTON)  ((((BUTTON) & 0b01111111) >> 1) + ( BUTTON_DOWN(BUTTON) & ((BUTTON) & 0b1)))
#define BUTTON_RELEASES(BUTTON) ((((BUTTON) & 0b01111111) >> 1) + (~BUTTON_DOWN(BUTTON) & ((BUTTON) & 0b1)))
#define BTN_DOWN(BTN)           BUTTON_DOWN(MACRO_CONCAT(platform_input->button, BTN))
#define BTN_PRESSES(BTN)        BUTTON_PRESSES(MACRO_CONCAT(platform_input->button, BTN))
#define BTN_RELEASES(BTN)       BUTTON_RELEASES(MACRO_CONCAT(platform_input->button, BTN))

global constexpr i32 PLATFORM_GAMEPAD_MAX = 4;
global constexpr u64 PLATFORM_MEMORY_SIZE = GIBIBYTES_OF(1);

struct PlatformSample
{
	union
	{
		struct
		{
			i16 left;
			i16 right;
		};

		u32 sample;
	};
};
static_assert(sizeof(PlatformSample) == sizeof(u32));

struct PlatformInput
{
	union
	{
		struct
		{
			struct
			{
				u8 action_left;
				u8 action_right;
				u8 action_down;
				u8 action_up;
				u8 dpad_left;
				u8 dpad_right;
				u8 dpad_down;
				u8 dpad_up;
				u8 shoulder_left;
				u8 shoulder_right;
				u8 stick_left;
				u8 stick_right;
				u8 start;
				u8 back;
			} gamepads[PLATFORM_GAMEPAD_MAX];

			u8 letters['z' - 'a'];
			u8 numbers[10];
			u8 arrow_left;
			u8 arrow_right;
			u8 arrow_down;
			u8 arrow_up;
			u8 enter;
			u8 shift;
			u8 alt;
			u8 mouse_left;
			u8 mouse_right;
		} button;

		u8 buttons[sizeof(button) / sizeof(u8)];
	};

	struct
	{
		bool32 connected;
		f32    trigger_left;
		f32    trigger_right;
		vf2    stick_left;
		vf2    stick_right;
	} gamepads[PLATFORM_GAMEPAD_MAX];

	vi2 mouse;
	vi2 mouse_delta;
	i32 mouse_scroll;
};

struct PlatformFileData
{
	u64   size;
	byte* data;
};

// @TODO@ Less generic IO system.
// @TODO@ Macro that provides where the call was made?
#define PlatformReadFileData_t(NAME) bool32 NAME(PlatformFileData* platform_file_data, const wchar_t* platform_file_path)
typedef PlatformReadFileData_t(PlatformReadFileData_t);

#define PlatformFreeFileData_t(NAME) void NAME(PlatformFileData* platform_file_data)
typedef PlatformFreeFileData_t(PlatformFreeFileData_t);

#define PlatformWriteFile_t(NAME) bool32 NAME(const wchar_t* platform_file_path, byte* platform_write_data, u64 platform_write_size)
typedef PlatformWriteFile_t(PlatformWriteFile_t);

#define PlatformUpdate_t(NAME) void NAME(u32* platform_framebuffer, vi2 platform_framebuffer_dimensions, PlatformInput* platform_input, byte* platform_memory, f32 platform_delta_time, PlatformReadFileData_t PlatformReadFileData, PlatformFreeFileData_t PlatformFreeFileData, PlatformWriteFile_t PlatformWriteFile)
typedef PlatformUpdate_t(PlatformUpdate_t);

#define PlatformSound_t(NAME) void NAME(PlatformSample* platform_sample_buffer, i32 platform_sample_count, i32 platform_samples_per_second, byte* platform_memory)
typedef PlatformSound_t(PlatformSound_t);
