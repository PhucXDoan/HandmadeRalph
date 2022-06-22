#include "unified.h"
#include "platform.h"

// @TODO@ Accurate kinematics.

constexpr f32 PIXELS_PER_METER = 50.0f;

struct Wall
{
	vf2 rel_position;
	vf2 dimensions;
};

struct Chunk
{
	vi2   coordinates;
	i32   wall_count;
	Wall  wall_buffer[128];
};

struct State
{
	bool32 is_initialized;
	Chunk  chunk;

	vf2    hero_position;
	vf2    hero_velocity;

	//f32    hertz;
	//vi2    offset;
	//f32    t;
};
static_assert(sizeof(State) < PLATFORM_MEMORY_SIZE / 4);

internal void set_pixel_rect(PlatformFramebuffer* platform_framebuffer, vi2 top_left, vi2 dimensions, u32 pixel)
{
	FOR_RANGE(y, max(top_left.y, 0), min(top_left.y + dimensions.y, platform_framebuffer->dimensions.y))
	{
		FOR_RANGE(x, max(top_left.x, 0), min(top_left.x + dimensions.x, platform_framebuffer->dimensions.x))
		{
			platform_framebuffer->pixels[y * platform_framebuffer->dimensions.x + x] = pixel;
		}
	}
}

internal void draw_rect(PlatformFramebuffer* platform_framebuffer, vf2 bottom_left, vf2 dimensions, vf3 rgb)
{
	set_pixel_rect(platform_framebuffer, vxx(bottom_left.x, platform_framebuffer->dimensions.y - bottom_left.y - dimensions.y), vxx(dimensions), vxx_argb(rgb));
}

PlatformUpdate_t(PlatformUpdate)
{
	State* state = reinterpret_cast<State*>(platform_memory);

	if (!state->is_initialized)
	{
		lambda add_wall =
			[&](Wall wall)
			{
				ASSERT(IN_RANGE(state->chunk.wall_count, 0, ARRAY_CAPACITY(state->chunk.wall_buffer)));
				state->chunk.wall_buffer[state->chunk.wall_count] = wall;
				state->chunk.wall_count += 1;
			};

		constexpr u8 WALLS[16][16] =
			{
				{ 0, 0, 0, 0, 0, 0, 0, 1,   0, 0, 0, 1, 0, 0, 0, 1},
				{ 0, 1, 1, 0, 0, 0, 0, 1,   0, 0, 0, 1, 0, 0, 0, 1},
				{ 0, 0, 1, 0, 0, 0, 0, 0,   0, 0, 0, 1, 1, 1, 0, 1},
				{ 0, 0, 0, 0, 1, 1, 0, 0,   1, 1, 0, 0, 1, 1, 0, 0},
				{ 0, 0, 0, 0, 0, 0, 0, 1,   0, 0, 0, 1, 0, 0, 0, 1},
				{ 0, 1, 1, 0, 0, 0, 0, 1,   0, 0, 0, 1, 0, 0, 0, 1},
				{ 0, 0, 1, 0, 1, 1, 0, 1,   1, 0, 0, 1, 1, 1, 0, 1},
				{ 0, 0, 0, 0, 1, 1, 0, 0,   0, 1, 0, 0, 1, 1, 0, 0},

				{ 0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 1, 0, 0, 0, 1},
				{ 0, 1, 1, 0, 0, 0, 0, 1,   0, 0, 0, 1, 0, 0, 0, 1},
				{ 0, 0, 1, 0, 1, 1, 0, 1,   1, 1, 0, 1, 1, 1, 0, 1},
				{ 0, 0, 0, 0, 1, 1, 0, 0,   1, 1, 0, 0, 1, 1, 0, 0},
				{ 0, 0, 0, 1, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0},
				{ 0, 0, 0, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0},
				{ 0, 0, 1, 0, 0, 0, 0, 0,   0, 0, 0, 0, 0, 0, 0, 0},
				{ 1, 1, 1, 0, 0, 0, 0, 1,   0, 0, 0, 1, 0, 0, 0, 1}
			};

		FOR_ELEMS(row, WALLS)
		{
			FOR_ELEMS(exists, *row)
			{
				if (*exists)
				{
					add_wall({ .rel_position = vxx(exists_index, static_cast<i32>(ARRAY_CAPACITY(WALLS) - 1 - row_index)), .dimensions = { 1.0f, 1.0f } });
				}
			}
		}

		state->hero_position = { 5.0f, 5.0f };

		state->is_initialized = true;
	}

	//
	// Update.
	//

	vf2 target_hero_velocity = { 0.0f, 0.0f };
	if (LTR_DOWN('a'))
	{
		target_hero_velocity.x -= 1.0f;
	}
	if (LTR_DOWN('d'))
	{
		target_hero_velocity.x += 1.0f;
	}
	if (LTR_DOWN('s'))
	{
		target_hero_velocity.y -= 1.0f;
	}
	if (LTR_DOWN('w'))
	{
		target_hero_velocity.y += 1.0f;
	}
	if (+target_hero_velocity)
	{
		target_hero_velocity = normalize(target_hero_velocity) * 1.78816f;

		if (BTN_DOWN(.shift))
		{
			target_hero_velocity *= 2.0f;
		}
	}

	state->hero_velocity  = dampen(state->hero_velocity, target_hero_velocity, 0.001f, platform_delta_time);
	state->hero_position += state->hero_velocity * platform_delta_time;

	//
	// Render.
	//

	memset(platform_framebuffer->pixels, 0, platform_framebuffer->dimensions.x * platform_framebuffer->dimensions.y * sizeof(u32));

	FOR_ELEMS(it, state->chunk.wall_buffer, state->chunk.wall_count)
	{
		draw_rect
		(
			platform_framebuffer,
			it->rel_position * PIXELS_PER_METER,
			it->dimensions * PIXELS_PER_METER,
			{ 0.25f, 0.275f, 0.3f }
		);
	}

	constexpr vf2 HERO_EFFECTIVE_DIMENSIONS = { 0.7f, 1.7399f };
	draw_rect
	(
		platform_framebuffer,
		state->hero_position * PIXELS_PER_METER,
		HERO_EFFECTIVE_DIMENSIONS * PIXELS_PER_METER,
		{ 0.85f, 0.87f, 0.3f }
	);

	#if 0
	DEBUG_printf("%d %d\n", PASS_V2(platform_input->mouse));

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

	state->t = fmodf(state->t, TAU);
	#endif
}

PlatformSound_t(PlatformSound)
{
	State* state = reinterpret_cast<State*>(platform_memory);

	#if 0
	FOR_ELEMS(sample, platform_sample_buffer, platform_sample_count)
	{
		*sample   = { static_cast<i16>(sinf(state->t) * 1250.0f), static_cast<i16>(sinf(state->t) * 1250.0f) };
		state->t += TAU * state->hertz / platform_samples_per_second;
	}
	#endif
}
