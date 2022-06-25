#include "unified.h"
#include "platform.h"
#include "rng.cpp"

#define DEBUG_AUDIO 0

constexpr f32 PIXELS_PER_METER = 50.0f;
constexpr f32 METERS_PER_CHUNK = 16.0f;

struct Wall
{
	vf2 rel_pos;
	vf2 dims;
};

struct Chunk
{
	vi2   coords;
	i32   wall_count;
	Wall  wall_buffer[128];
};

struct State
{
	bool32 is_initialized;
	u32    seed;

	union
	{
		Chunk chunks[8][4];
		Chunk chunks_flat[sizeof(chunks) / sizeof(Chunk)];
	};

	Chunk* hero_chunk;
	vf2    hero_rel_pos;
	vf2    hero_vel;

	vi2    camera_coords;
	vf2    camera_rel_pos;
	vf2    camera_vel;

	#if DEBUG_AUDIO
	f32    hertz;
	f32    t;
	#endif
};
static_assert(sizeof(State) < PLATFORM_MEMORY_SIZE / 4);

internal void set_pixel_rect(PlatformFramebuffer* platform_framebuffer, vi2 top_left, vi2 dims, u32 pixel)
{
	FOR_RANGE(y, max(top_left.y, 0), min(top_left.y + dims.y, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(top_left.x, 0), min(top_left.x + dims.x, platform_framebuffer->dims.x))
		{
			platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x] = pixel;
		}
	}
}

internal void draw_rect(PlatformFramebuffer* platform_framebuffer, vf2 bottom_left, vf2 dims, vf3 rgb)
{
	set_pixel_rect(platform_framebuffer, vxx(bottom_left.x, platform_framebuffer->dims.y - bottom_left.y - dims.y), vxx(dims), vxx_argb(rgb));
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

internal CollisionResult collide_against_plane(vf2 pos, vf2 displacement, vf2 plane_center, vf2 plane_normal)
{
	f32 inside_amount       = dot(plane_normal, plane_center - pos);
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

internal CollisionResult collide_against_line(vf2 pos, vf2 displacement, vf2 line_center, vf2 line_normal, f32 padding)
{
	vf2 plane_normal = line_normal * (dot(pos - line_center, line_normal) < 0.0f ? -1.0f : 1.0f);
	return collide_against_plane(pos, displacement, line_center + plane_normal * padding, plane_normal);
}

// @TODO@ Bugs out on `radius == 0.0f`
internal CollisionResult collide_against_circle(vf2 pos, vf2 displacement, vf2 center, f32 radius)
{
	vf2 rel_pos                 = pos - center;
	f32 distance_from_center    = norm(rel_pos);
	f32 amount_away_from_center = dot(rel_pos, displacement);

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
							.normal           = rel_pos / distance_from_center,
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
				.new_displacement = rel_pos * (radius / distance_from_center - 1.0f),
				.normal           = rel_pos / distance_from_center,
				.priority         = radius - distance_from_center
			};
	}
}

internal CollisionResult collide_against_rounded_rectangle(vf2 pos, vf2 displacement, vf2 rect_bottom_left, vf2 rect_dims, f32 padding)
{
	CollisionResult left_right = collide_against_line(pos, displacement, rect_bottom_left + vf2 { rect_dims.x / 2.0f, 0.0f }, { 1.0f, 0.0f }, rect_dims.x / 2.0f + padding);
	if (left_right.type != CollisionType::none && !IN_RANGE(pos.y + left_right.new_displacement.y - rect_bottom_left.y, 0.0f, rect_dims.y))
	{
		left_right.type = CollisionType::none;
	}
	CollisionResult bottom_top = collide_against_line(pos, displacement, rect_bottom_left + vf2 { 0.0f, rect_dims.y / 2.0f }, { 0.0f, 1.0f }, rect_dims.y / 2.0f + padding);
	if (bottom_top.type != CollisionType::none && !IN_RANGE(pos.x + bottom_top.new_displacement.x - rect_bottom_left.x, 0.0f, rect_dims.x))
	{
		bottom_top.type = CollisionType::none;
	}

	return
		prioritize_collision_results
		(
			prioritize_collision_results(bottom_top, left_right),
			prioritize_collision_results
			(
				prioritize_collision_results(collide_against_circle(pos, displacement, rect_bottom_left                            , padding), collide_against_circle(pos, displacement, rect_bottom_left + vf2 { rect_dims.x, 0.0f }, padding)),
				prioritize_collision_results(collide_against_circle(pos, displacement, rect_bottom_left + vf2 { 0.0f, rect_dims.y }, padding), collide_against_circle(pos, displacement, rect_bottom_left +       rect_dims          , padding))
			)
		);
}

PlatformUpdate_t(PlatformUpdate)
{
	State* state = reinterpret_cast<State*>(platform_memory);

	if (!state->is_initialized)
	{
		state->is_initialized = true;

		FOR_RANGE(chunk_y, ARRAY_CAPACITY(state->chunks))
		{
			FOR_RANGE(chunk_x, ARRAY_CAPACITY(state->chunks[0]))
			{
				aliasing chunk = state->chunks[chunk_y][chunk_x];

				chunk.coords = { chunk_x, chunk_y };

				FOR_RANGE(wall_y, 16)
				{
					FOR_RANGE(wall_x, 16)
					{
						if (rng(&state->seed) < 0.15f)
						{
							ASSERT(IN_RANGE(chunk.wall_count, 0, ARRAY_CAPACITY(chunk.wall_buffer)));
							chunk.wall_buffer[chunk.wall_count] =
								{
									.rel_pos = vxx(wall_x, wall_y),
									.dims    = { 1.0f, 1.0f }
								};
							chunk.wall_count += 1;
						}
					}
				}
			}
		}


		state->hero_chunk   = &state->chunks[0][0];
		state->hero_rel_pos = { 5.0f, 7.0f };
	}

	//
	// Update.
	//

	vf2 target_hero_vel = vxx(WASD_DOWN());
	if (+target_hero_vel)
	{
		target_hero_vel = normalize(target_hero_vel) * 1.78816f;

		if (BTN_DOWN(.shift))
		{
			target_hero_vel *= 2.0f;
		}
	}

	state->hero_vel = dampen(state->hero_vel, target_hero_vel, 0.001f, platform_delta_time);

	constexpr vf2 HERO_HITBOX_DIMS = { 0.7f, 0.4f };
	constexpr f32 WALL_PADDING     = 0.1f;
	{
		vf2 displacement = state->hero_vel * platform_delta_time;

		FOR_RANGE(8)
		{
			CollisionResult result = {};

			FOR_ELEMS(chunk, state->chunks_flat)
			{
				FOR_ELEMS(wall, chunk->wall_buffer, chunk->wall_count)
				{
					result =
						prioritize_collision_results
						(
							result,
							collide_against_rounded_rectangle
							(
								state->hero_chunk->coords * METERS_PER_CHUNK + state->hero_rel_pos,
								displacement,
								chunk->coords * METERS_PER_CHUNK + wall->rel_pos - HERO_HITBOX_DIMS,
								wall->dims + HERO_HITBOX_DIMS,
								WALL_PADDING
							)
						);
				}
			}

			lambda displace_hero =
				[&](vf2 delta_pos)
				{
					state->hero_rel_pos += delta_pos;

					vi2 delta_coords = { 0, 0 };
					if      (state->hero_rel_pos.x <              0.0f) { delta_coords.x -= 1; }
					else if (state->hero_rel_pos.x >= METERS_PER_CHUNK) { delta_coords.x += 1; }
					if      (state->hero_rel_pos.y <              0.0f) { delta_coords.y -= 1; }
					else if (state->hero_rel_pos.y >= METERS_PER_CHUNK) { delta_coords.y += 1; }
					ASSERT(IN_RANGE(state->hero_chunk->coords.y + delta_coords.y, 0, ARRAY_CAPACITY(state->chunks   )));
					ASSERT(IN_RANGE(state->hero_chunk->coords.x + delta_coords.x, 0, ARRAY_CAPACITY(state->chunks[0])));
					state->hero_chunk    = &state->chunks[state->hero_chunk->coords.y + delta_coords.y][state->hero_chunk->coords.x + delta_coords.x];
					state->hero_rel_pos -= delta_coords * METERS_PER_CHUNK;
				};

			if (result.type == CollisionType::none)
			{
				displace_hero(displacement);
				break;
			}
			else
			{
				displace_hero(result.new_displacement);
				state->hero_vel = dot(state->hero_vel - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
				displacement    = dot(displacement    - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
			}
		}
	}

	vf2 target_camera_vel = vxx(HJKL_DOWN());
	if (+target_camera_vel)
	{
		target_camera_vel = normalize(target_camera_vel) * 8.0f;
	}

	state->camera_vel      = dampen(state->camera_vel, target_camera_vel, 0.001f, platform_delta_time);
	state->camera_rel_pos += state->camera_vel * platform_delta_time;
	if      (state->camera_rel_pos.x <              0.0f) { state->camera_coords.x -= 1; state->camera_rel_pos.x += METERS_PER_CHUNK; }
	else if (state->camera_rel_pos.x >= METERS_PER_CHUNK) { state->camera_coords.x += 1; state->camera_rel_pos.x -= METERS_PER_CHUNK; }
	if      (state->camera_rel_pos.y <              0.0f) { state->camera_coords.y -= 1; state->camera_rel_pos.y += METERS_PER_CHUNK; }
	else if (state->camera_rel_pos.y >= METERS_PER_CHUNK) { state->camera_coords.y += 1; state->camera_rel_pos.y -= METERS_PER_CHUNK; }

	//DEBUG_printf("(%i %i) (%f %f) <%i %i> <%f %f>\n", PASS_V2(state->hero_chunk->coords), PASS_V2(state->hero_rel_pos), PASS_V2(state->camera_coords), PASS_V2(state->camera_rel_pos));

	//
	// Render.
	//

	memset(platform_framebuffer->pixels, 0, platform_framebuffer->dims.x * platform_framebuffer->dims.y * sizeof(u32));

	FOR_ELEMS(chunk, state->chunks_flat)
	{
		FOR_ELEMS(wall, chunk->wall_buffer, chunk->wall_count)
		{
			draw_rect
			(
				platform_framebuffer,
				((chunk->coords - state->camera_coords) * METERS_PER_CHUNK + wall->rel_pos - state->camera_rel_pos) * PIXELS_PER_METER,
				wall->dims * PIXELS_PER_METER,
				{ 0.25f, 0.275f, 0.3f }
			);
		}
	}

	constexpr vf2 HERO_RENDER_DIMS = { 0.7f, 1.7399f };
	draw_rect
	(
		platform_framebuffer,
		((state->hero_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->hero_rel_pos - state->camera_rel_pos) * PIXELS_PER_METER,
		HERO_RENDER_DIMS * PIXELS_PER_METER,
		{ 0.85f, 0.87f, 0.3f }
	);

	draw_rect
	(
		platform_framebuffer,
		((state->hero_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->hero_rel_pos - state->camera_rel_pos) * PIXELS_PER_METER,
		HERO_HITBOX_DIMS * PIXELS_PER_METER,
		{ 0.9f, 0.5f, 0.1f }
	);

	#if DEBUG_AUDIO
	//DEBUG_printf("%d %d\n", PASS_V2(platform_input->mouse));
	state->hertz = 512.0f + platform_input->gamepads[0].stick_right.y * 100.0f;
	state->t     = fmodf(state->t, TAU);
	#endif
}

PlatformSound_t(PlatformSound)
{
	#if DEBUG_AUDIO
	State* state = reinterpret_cast<State*>(platform_memory);

	FOR_ELEMS(sample, platform_sample_buffer, platform_sample_count)
	{
		*sample   = { static_cast<i16>(sinf(state->t) * 1250.0f), static_cast<i16>(sinf(state->t) * 1250.0f) };
		state->t += TAU * state->hertz / platform_samples_per_second;
	}
	#endif
}
