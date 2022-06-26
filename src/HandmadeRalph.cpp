#include "unified.h"
#include "platform.h"
#include "rng.cpp"

#define DEBUG_AUDIO 0

// @TODO@ Memory arena that return 0 if there isn't enough space left?

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

struct BMP
{
	vi2   dims;
	RGBA* rgba;
};

enum struct Cardinal : u8
{
	left,
	right,
	down,
	up
};

struct State
{
	bool32      is_initialized;
	u32         seed;
	MemoryArena asset_arena;

	union
	{
		struct
		{
			struct
			{
				BMP head;
				BMP cape;
				BMP torso;
			} hero[4];
			BMP hero_shadow;
			BMP background;
		}   bmp;
		BMP bmps[sizeof(bmp) / sizeof(BMP)];
	};

	union
	{
		Chunk chunks[8][4];
		Chunk chunks_flat[sizeof(chunks) / sizeof(Chunk)];
	};

	Chunk*   hero_chunk;
	vf2      hero_rel_pos;
	vf2      hero_vel;
	Cardinal hero_direction;

	vi2    camera_coords;
	vf2    camera_rel_pos;
	vf2    camera_vel;

	#if DEBUG_AUDIO
	f32    hertz;
	f32    t;
	#endif
};
static_assert(sizeof(State) < PLATFORM_MEMORY_SIZE / 4);

template <typename TYPE>
internal TYPE eat(PlatformFileData* file_data)
{
	ASSERT(file_data->read_index + sizeof(TYPE) <= file_data->size);
	TYPE value;
	memcpy(&value, file_data->data + file_data->read_index, sizeof(TYPE));
	file_data->read_index += sizeof(TYPE);
	return value;
}

internal void draw_rect(PlatformFramebuffer* platform_framebuffer, vi2 bottom_left, vi2 dims, RGBA rgba)
{
	u32 pixel_value = pack_as_raw_argb(rgba);
	FOR_RANGE(y, max(platform_framebuffer->dims.y - bottom_left.y - dims.y, 0), min(platform_framebuffer->dims.y - bottom_left.y, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(bottom_left.x, 0), min(bottom_left.x + dims.x, platform_framebuffer->dims.x))
		{
			platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x] = pixel_value;
		}
	}
}

#if 0
// @TODO@ Subpixel.
internal void draw_bmp(PlatformFramebuffer* platform_framebuffer, BMP* bmp, vf2 bottom_left, vf2 dims)
{
	vi2 pixel_coords = vxx(bottom_left.x, platform_framebuffer->dims.y - bottom_left.y); // @TODO@ Round.
	vi2 pixel_dims   = vxx(dims); // @TODO@ Round.

	FOR_RANGE(y, max(pixel_coords.y - pixel_dims.y, 0), min(pixel_coords.y, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(pixel_coords.x, 0), min(pixel_coords.x + pixel_dims.x, platform_framebuffer->dims.x))
		{
			aliasing dst = platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x];
			vi2 sample_coords = vxx(vxx(x - pixel_coords.x, pixel_coords.y - 1 - y) / pixel_dims * bmp->dims); // @TODO@ Round.
			vf4 top           = vf4_from_rgba(bmp->rgba[sample_coords.y * bmp->dims.x + sample_coords.x]);

			if (top.w == 1.0f)
			{
				dst = pack_argb(top);
			}
			else
			{
				dst = pack_argb(lerp(vf3_from_argb(dst), top.xyz, top.w));
			}
		}
	}
}

internal void draw_bmp(PlatformFramebuffer* platform_framebuffer, BMP* bmp, vf2 bottom_left, f32 scalar, vf2 center)
{
	draw_bmp(platform_framebuffer, bmp, bottom_left - bmp->dims * scalar * center, bmp->dims * scalar);
}
#endif

internal void draw_bmp(PlatformFramebuffer* platform_framebuffer, BMP* bmp, vi2 bottom_left)
{
	FOR_RANGE(y, max(platform_framebuffer->dims.y - bottom_left.y - bmp->dims.y, 0), min(platform_framebuffer->dims.y - bottom_left.y, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(bottom_left.x, 0), min(bottom_left.x + bmp->dims.x, platform_framebuffer->dims.x))
		{
			aliasing dst = platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x];
			aliasing top = bmp->rgba[(platform_framebuffer->dims.y - bottom_left.y - y) * bmp->dims.x + x - bottom_left.x];

			if (top.a == 255)
			{
				dst = pack_as_raw_argb(top);
			}
			else
			{
				dst = pack_as_raw_argb(rgba_from(lerp(vf3_from(unpack_raw_argb(dst)), vf3_from(top), top.a / 255.0f)));
			}
		}
	}
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

		state->asset_arena =
			{
				.size = PLATFORM_MEMORY_SIZE - sizeof(State),
				.base = platform_memory      + sizeof(State)
			};

		// @TODO@ Asset metaprogram.
		constexpr wstrlit bmp_file_paths[] =
			{
				DATA_DIR L"test_hero_left_head.bmp",
				DATA_DIR L"test_hero_left_cape.bmp",
				DATA_DIR L"test_hero_left_torso.bmp",
				DATA_DIR L"test_hero_right_head.bmp",
				DATA_DIR L"test_hero_right_cape.bmp",
				DATA_DIR L"test_hero_right_torso.bmp",
				DATA_DIR L"test_hero_front_head.bmp",
				DATA_DIR L"test_hero_front_cape.bmp",
				DATA_DIR L"test_hero_front_torso.bmp",
				DATA_DIR L"test_hero_back_head.bmp",
				DATA_DIR L"test_hero_back_cape.bmp",
				DATA_DIR L"test_hero_back_torso.bmp",
				DATA_DIR L"test_hero_shadow.bmp",
				DATA_DIR L"test_background.bmp"
			};

		FOR_ELEMS(bmp, state->bmps)
		{
			PlatformFileData file_data;
			if (!PlatformReadFileData(&file_data, bmp_file_paths[bmp_index]))
			{
				ASSERT(false);
				return PlatformUpdateExitCode::abort;
			}
			DEFER { PlatformFreeFileData(&file_data); };

			// @TODO@ Consider endianess.
			#pragma pack(push, 1)
			struct BitmapHeader
			{
				char name[2];
				u32  file_size;
				u16  reserved[2];
				u32  pixel_data_offset;
				u32  dib_header_size;
				vi2  dims;
				u16  color_planes;
				u16  bits_per_pixel;
				u32  compression_method;
				u32  pixel_data_size;
				vi2  pixels_per_meter;
				u32  color_count;
				u32  important_colors;
				u32  mask_r;
				u32  mask_g;
				u32  mask_b;
				u32  mask_a;
			};
			#pragma pack(pop)

			BitmapHeader header = eat<BitmapHeader>(&file_data);

			if
			(
				(header.name[0] != 'B' || header.name[1] != 'M')                   ||
				(header.file_size != file_data.size)                               ||
				(header.dib_header_size != 124)                                    || // @TODO@ For now only BITMAPV5HEADER.
				(header.dims.x <= 0 || header.dims.y <= 0)                         ||
				(header.color_planes != 1)                                         ||
				(header.bits_per_pixel != 32)                                      || // @TODO@ For now must be RGBA.
				(header.compression_method != 3)                                   || // @TODO@ Different compression methods and their meaning?
				(header.pixel_data_size != 4ULL * header.dims.x * header.dims.y)   ||
				(header.color_count != 0)                                          ||
				(header.important_colors != 0)                                     ||
				(~(header.mask_r | header.mask_g | header.mask_b | header.mask_a)) ||
				(  header.mask_r & header.mask_g & header.mask_b & header.mask_a )
			)
			{
				ASSERT(false);
				return PlatformUpdateExitCode::abort;
			}

			bmp->dims = header.dims;
			bmp->rgba = memory_arena_allocate<RGBA>(&state->asset_arena, static_cast<u64>(bmp->dims.x) * bmp->dims.y);

			// @TODO@ Microsoft specific intrinsic...
			u32 lz_r = __lzcnt(header.mask_r);
			u32 lz_g = __lzcnt(header.mask_g);
			u32 lz_b = __lzcnt(header.mask_b);
			u32 lz_a = __lzcnt(header.mask_a);
			FOR_RANGE(y, header.dims.y)
			{
				FOR_RANGE(x, header.dims.x)
				{
					aliasing bmp_pixel = reinterpret_cast<u32*>(file_data.data + header.pixel_data_offset)[y * header.dims.x + x];
					bmp->rgba[y * bmp->dims.x + x] =
						{
							((bmp_pixel & header.mask_r) << lz_r) >> 24,
							((bmp_pixel & header.mask_g) << lz_g) >> 24,
							((bmp_pixel & header.mask_b) << lz_b) >> 24,
							((bmp_pixel & header.mask_a) << lz_a) >> 24
						};
				}
			}
		}

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
		if      (target_hero_vel.x < 0.0f) { state->hero_direction = Cardinal::left;  }
		else if (target_hero_vel.x > 0.0f) { state->hero_direction = Cardinal::right; }
		else if (target_hero_vel.y < 0.0f) { state->hero_direction = Cardinal::down;  }
		else if (target_hero_vel.y > 0.0f) { state->hero_direction = Cardinal::up;    }

		target_hero_vel = normalize(target_hero_vel) * 1.78816f;

		if (BTN_DOWN(.shift))
		{
			target_hero_vel *= 2.0f;
		}
	}

	state->hero_vel = dampen(state->hero_vel, target_hero_vel, 0.001f, platform_delta_time);

	constexpr vf2 HERO_HITBOX_DIMS = { 0.7f, 0.4f };
	constexpr f32 WALL_PADDING     = 0.1f;

	{ // @TODO@ Better performing collision.
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
								state->hero_chunk->coords * METERS_PER_CHUNK + state->hero_rel_pos - vf2 { HERO_HITBOX_DIMS.x / 2.0f, 0.0f },
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

	draw_bmp(platform_framebuffer, &state->bmp.background, { 0, 0 });

	FOR_ELEMS(chunk, state->chunks_flat)
	{
		FOR_ELEMS(wall, chunk->wall_buffer, chunk->wall_count)
		{
			draw_rect
			(
				platform_framebuffer,
				vxx(((chunk->coords - state->camera_coords) * METERS_PER_CHUNK + wall->rel_pos - state->camera_rel_pos) * PIXELS_PER_METER),
				vxx(wall->dims * PIXELS_PER_METER),
				rgba_from(0.15f, 0.225f, 0.2f)
			);
		}
	}

	constexpr vf2 HERO_RENDER_DIMS = { 0.7f, 1.7399f };
	draw_rect
	(
		platform_framebuffer,
		vxx(((state->hero_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->hero_rel_pos - state->camera_rel_pos - vf2 { HERO_RENDER_DIMS.x / 2.0f, 0.0f }) * PIXELS_PER_METER),
		vxx(HERO_RENDER_DIMS * PIXELS_PER_METER),
		rgba_from(0.85f, 0.87f, 0.3f)
	);

	draw_rect
	(
		platform_framebuffer,
		vxx(((state->hero_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->hero_rel_pos - state->camera_rel_pos - vf2 { HERO_RENDER_DIMS.x / 2.0f, 0.0f }) * PIXELS_PER_METER),
		vxx(HERO_HITBOX_DIMS * PIXELS_PER_METER),
		rgba_from(0.9f, 0.5f, 0.1f)
	);

	vi2 hero_screen_coords = vxx(((state->hero_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->hero_rel_pos - state->camera_rel_pos) * PIXELS_PER_METER - state->bmp.hero[static_cast<i32>(state->hero_direction)].torso.dims * vf2 { 0.5f, 0.15f });
	draw_bmp(platform_framebuffer, &state->bmp.hero[static_cast<i32>(state->hero_direction)].torso, hero_screen_coords);
	draw_bmp(platform_framebuffer, &state->bmp.hero[static_cast<i32>(state->hero_direction)].cape , hero_screen_coords);
	draw_bmp(platform_framebuffer, &state->bmp.hero[static_cast<i32>(state->hero_direction)].head , hero_screen_coords);

	#if DEBUG_AUDIO
	state->hertz = 512.0f + platform_input->gamepads[0].stick_right.y * 100.0f;
	state->t     = fmodf(state->t, TAU);
	#endif

	return PlatformUpdateExitCode::normal;
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
