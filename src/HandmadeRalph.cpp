#include "unified.h"
#include "platform.h"

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

global constexpr f32 COLLISION_EPSILON = 0.000001f;

enum struct CollisionType : u8
{
	none,
	collided,
	embedded
};

struct CollisionResult
{
	CollisionType type;
	vf2           new_displacement;
	vf2           normal;
	f32           priority;
};

internal const CollisionResult& prioritize_collision_results(const CollisionResult& a, const CollisionResult& b)
{
	return b.type == CollisionType::none || a.type != CollisionType::none && a.priority > b.priority
		? a
		: b;
}

internal CollisionResult collide_against_plane(vf2 position, vf2 displacement, vf2 plane_center, vf2 plane_normal)
{
	f32 inside_amount       = dot(plane_normal, plane_center - position);
	f32 intersection_scalar = inside_amount / dot(plane_normal, displacement);
	if (inside_amount < 0.0f && IN_RANGE(intersection_scalar, 0.0f, 1.0f))
	{
		return
			{
				.type             = CollisionType::collided,
				.new_displacement = displacement * intersection_scalar,
				.normal           = plane_normal,
				.priority         = -intersection_scalar
			};
	}
	else if (inside_amount < 0.0f || (inside_amount <= COLLISION_EPSILON && dot(displacement, plane_normal) >= -COLLISION_EPSILON))
	{
		return {};
	}
	else
	{
		return
			{
				.type             = CollisionType::embedded,
				.new_displacement = plane_normal * inside_amount,
				.normal           = plane_normal,
				.priority         = inside_amount
			};
	}
}

internal CollisionResult collide_against_line(vf2 position, vf2 displacement, vf2 line_center, vf2 line_normal, f32 padding)
{
	vf2 plane_normal = line_normal * (dot(position - line_center, line_normal) < 0.0f ? -1.0f : 1.0f);
	return collide_against_plane(position, displacement, line_center + plane_normal * padding, plane_normal);
}

internal CollisionResult collide_against_circle(vf2 position, vf2 displacement, vf2 center, f32 radius)
{
	vf2 rel_position            = position - center;
	f32 distance_from_center    = norm(rel_position);
	f32 amount_away_from_center = dot(rel_position, displacement);

	if (distance_from_center < COLLISION_EPSILON)
	{
		return {};
	}

	if (distance_from_center > radius)
	{
		f32 norm_sq_displacement = norm_sq(displacement);
		if (norm_sq_displacement >= COLLISION_EPSILON)
		{
			f32 discriminant = square(amount_away_from_center) - norm_sq_displacement * (square(distance_from_center) - square(radius));
			if (discriminant >= COLLISION_EPSILON)
			{
				f32 intersection_scalar = -(amount_away_from_center + sqrtf(discriminant)) / norm_sq_displacement;
				if (IN_RANGE(intersection_scalar, 0.0f, 1.0f))
				{
					return
						{
							.type             = CollisionType::collided,
							.new_displacement = displacement * intersection_scalar,
							.normal           = rel_position / distance_from_center,
							.priority         = -intersection_scalar
						};
				}
			}
		}
	}

	if (distance_from_center > radius || radius - distance_from_center < COLLISION_EPSILON && amount_away_from_center >= 0.0f)
	{
		return {};
	}
	else
	{
		return
			{
				.type             = CollisionType::embedded,
				.new_displacement = rel_position * (radius / distance_from_center - 1.0f),
				.normal           = rel_position / distance_from_center,
				.priority         = radius - distance_from_center
			};
	}
}

internal CollisionResult collide_against_rounded_rectangle(vf2 position, vf2 displacement, vf2 rect_bottom_left, vf2 rect_dimensions, f32 padding)
{
	CollisionResult left_right = collide_against_line(position, displacement, rect_bottom_left + vf2 { rect_dimensions.x / 2.0f, 0.0f }, { 1.0f, 0.0f }, rect_dimensions.x / 2.0f + padding);
	if (left_right.type != CollisionType::none && !IN_RANGE(position.y + left_right.new_displacement.y - rect_bottom_left.y, 0.0f, rect_dimensions.y))
	{
		left_right.type = CollisionType::none;
	}
	CollisionResult bottom_top = collide_against_line(position, displacement, rect_bottom_left + vf2 { 0.0f, rect_dimensions.y / 2.0f }, { 0.0f, 1.0f }, rect_dimensions.y / 2.0f + padding);
	if (bottom_top.type != CollisionType::none && !IN_RANGE(position.x + bottom_top.new_displacement.x - rect_bottom_left.x, 0.0f, rect_dimensions.x))
	{
		bottom_top.type = CollisionType::none;
	}

	return
		prioritize_collision_results
		(
			prioritize_collision_results(bottom_top, left_right),
			prioritize_collision_results
			(
				prioritize_collision_results(collide_against_circle(position, displacement, rect_bottom_left                                  , padding), collide_against_circle(position, displacement, rect_bottom_left + vf2 { rect_dimensions.x, 0.0f }, padding)),
				prioritize_collision_results(collide_against_circle(position, displacement, rect_bottom_left + vf2 { 0.0f, rect_dimensions.y }, padding), collide_against_circle(position, displacement, rect_bottom_left +       rect_dimensions          , padding))
			)
		);
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

		state->hero_position = { 5.0f, 7.0f };

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

	state->hero_velocity = dampen(state->hero_velocity, target_hero_velocity, 0.001f, platform_delta_time);

	constexpr f32 HERO_COLLISION_PADDING = 0.15f;
	{
		vf2 displacement = state->hero_velocity * platform_delta_time;

		if (norm(state->hero_position - vf2 { 6.150000f, 10.832338f }) < 0.00001f)
		{
			DEBUG_printf("PAUSE\n");
		}

		FOR_RANGE(8)
		{
			CollisionResult result = {};

			FOR_ELEMS(it, state->chunk.wall_buffer, state->chunk.wall_count)
			{
				result = prioritize_collision_results(result, collide_against_rounded_rectangle(state->hero_position, displacement, it->rel_position, it->dimensions, HERO_COLLISION_PADDING));
			}

			if (result.type == CollisionType::none)
			{
				state->hero_position += displacement;
				break;
			}
			else
			{
				state->hero_position += result.new_displacement;
				state->hero_velocity  = dot(state->hero_velocity - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
				displacement          = dot(displacement         - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
			}
		}
	}
	//DEBUG_printf("%f %f\n", PASS_V2(state->hero_position));

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

	constexpr vf2 HERO_RENDER_DIMENSIONS = { 0.7f, 1.7399f };
	draw_rect
	(
		platform_framebuffer,
		state->hero_position * PIXELS_PER_METER,
		HERO_RENDER_DIMENSIONS * PIXELS_PER_METER,
		{ 0.85f, 0.87f, 0.3f }
	);

	constexpr vf2 HERO_HITBOX_DIMENSIONS = { 0.7f, 0.4f };
	draw_rect
	(
		platform_framebuffer,
		state->hero_position * PIXELS_PER_METER,
		HERO_HITBOX_DIMENSIONS * PIXELS_PER_METER,
		{ 0.9f, 0.5f, 0.1f }
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
	#if 0
	State* state = reinterpret_cast<State*>(platform_memory);

	FOR_ELEMS(sample, platform_sample_buffer, platform_sample_count)
	{
		*sample   = { static_cast<i16>(sinf(state->t) * 1250.0f), static_cast<i16>(sinf(state->t) * 1250.0f) };
		state->t += TAU * state->hertz / platform_samples_per_second;
	}
	#endif
}
