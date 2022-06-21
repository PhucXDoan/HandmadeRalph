#include "unified.h"
#include "platform.h"

struct State
{
	bool32 initialized;
	f32    hertz;
	vi2    offset;
	f32    t;
};
static_assert(sizeof(State) < PLATFORM_MEMORY_SIZE / 4);

PlatformUpdate_t(PlatformUpdate)
{
	State* state = reinterpret_cast<State*>(platform_memory);

	if (!state->initialized)
	{
		state->initialized = true;
	}

	state->offset.x += static_cast<i32>(200.0f * platform_delta_time);
	state->offset   += vxx(200.0f * platform_input->gamepads[0].stick_left * platform_delta_time);

	if (BTN_DOWN(.gamepads[0].action_left))
	{
		state->offset.x -= 1;
	}
	if (BTN_DOWN(.gamepads[0].action_right))
	{
		state->offset.x += 1;
	}
	if (BTN_DOWN(.gamepads[0].action_down))
	{
		state->offset.y += 1;
	}
	if (BTN_DOWN(.gamepads[0].action_up))
	{
		state->offset.y -= 1;
	}
	state->hertz = 512.0f + platform_input->gamepads[0].stick_right.y * 100.0f;

	FOR_RANGE(y, platform_framebuffer_dimensions.y)
	{
		FOR_RANGE(x, platform_framebuffer_dimensions.x)
		{
			platform_framebuffer[y * platform_framebuffer_dimensions.x + x] =
				vxx_argb(vi3 { state->offset.x + x, state->offset.y + y, state->offset.x + x + state->offset.y + y });
		}
	}

	//DEBUG_printf("down %d | presses %d | releases %d\n", BTN_DOWN(.letters[0]), BTN_PRESSES(.letters[0]), BTN_RELEASES(.letters[0]));

	state->t = fmodf(state->t, TAU);
}

PlatformSound_t(PlatformSound)
{
	State* state = reinterpret_cast<State*>(platform_memory);

	FOR_ELEMS(sample, platform_sample_buffer, platform_sample_count)
	{
		*sample   = { static_cast<i16>(sinf(state->t) * 250.0f), static_cast<i16>(sinf(state->t) * 250.0f) };
		state->t += TAU * state->hertz / platform_samples_per_second;
	}
}
