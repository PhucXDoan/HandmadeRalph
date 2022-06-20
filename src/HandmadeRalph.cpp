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
		state->hertz       = 512.0f;
	}

	state->offset.x += static_cast<i32>(200.0f * platform_delta_time);

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
	state->hertz += BTN_DOWN(.arrow_up);

	FOR_RANGE(y, platform_framebuffer->dimensions.y)
	{
		FOR_RANGE(x, platform_framebuffer->dimensions.x)
		{
			platform_framebuffer->pixels[y * platform_framebuffer->dimensions.x + x] =
				vxx_argb(vi3 { state->offset.x + x, state->offset.y + y, state->offset.x + x + state->offset.y + y });
		}
	}

	DEBUG_printf("down %d | presses %d | releases %d\n", BTN_DOWN(.letters[0]), BTN_PRESSES(.letters[0]), BTN_RELEASES(.letters[0]));
}

PlatformSound_t(PlatformSound)
{
	//State* state = reinterpret_cast<State*>(platform_memory);
	//state->t += TAU * state->hertz / platform_samples_per_second;
	//return { static_cast<i16>(sinf(state->t) * 500.0f), static_cast<i16>(sinf(state->t) * 500.0f) };
	return {};
}
