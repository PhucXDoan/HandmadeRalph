#pragma once

struct PlatformFramebuffer
{
	vi2  dimensions;
	u32* pixels;
};

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

#define BUTTON_DOWN(BUTTON)     ((BUTTON) >> 7)
#define BUTTON_PRESSES(BUTTON)  ((((BUTTON) & 0b01111111) >> 1) + ( BUTTON_DOWN(BUTTON) & ((BUTTON) & 0b1)))
#define BUTTON_RELEASES(BUTTON) ((((BUTTON) & 0b01111111) >> 1) + (~BUTTON_DOWN(BUTTON) & ((BUTTON) & 0b1)))

#define BTN_DOWN(BTN)     BUTTON_DOWN(MACRO_CONCAT_(platform_input->button, BTN))
#define BTN_PRESSES(BTN)  BUTTON_DOWN(MACRO_CONCAT_(platform_input->button, BTN))
#define BTN_RELEASES(BTN) BUTTON_DOWN(MACRO_CONCAT_(platform_input->button, BTN))

global constexpr i32 PLATFORM_GAMEPAD_MAX = 4;

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
		} button;

		u8 buttons[sizeof(button) / sizeof(u8)];
	};

	struct
	{
		f32 trigger_left;
		f32 trigger_right;
		vf2 stick_left;
		vf2 stick_right;
	} gamepads[PLATFORM_GAMEPAD_MAX];
};

static_assert(sizeof(PlatformSample) == sizeof(u32));

#define PlatformUpdate_t(NAME) void NAME(PlatformFramebuffer* platform_framebuffer, PlatformInput* platform_input)
typedef PlatformUpdate_t(PlatformUpdate_t);

#define PlatformSound_t(NAME) PlatformSample NAME(i32 platform_samples_per_second)
typedef PlatformSound_t(PlatformSound_t);
