#include "unified.h"
#include "platform.h"
#include "rng.cpp"

#define DEBUG_AUDIO 0

// @TODO@ Memory arena that return 0 if there isn't enough space left?
// @TODO@ Handle world chunk edges.

constexpr f32 PIXELS_PER_METER = 50.0f;
constexpr f32 METERS_PER_CHUNK = 16.0f;

struct Wall
{
	vf2 rel_pos;
	vf2 dims;
};

struct Chunk
{
	bool32 exists;
	vi2    coords;
	i32    wall_count;
	Wall   wall_buffer[128];
};

struct ChunkNode
{
	ChunkNode* next_node;
	Chunk      chunk;
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
	MemoryArena arena;

	union
	{
		struct
		{
			BMP hero_head  [4]; // @META@ hero_left_head.bmp , hero_right_head.bmp , hero_front_head.bmp , hero_back_head.bmp
			BMP hero_cape  [4]; // @META@ hero_left_cape.bmp , hero_right_cape.bmp , hero_front_cape.bmp , hero_back_cape.bmp
			BMP hero_torso [4]; // @META@ hero_left_torso.bmp, hero_right_torso.bmp, hero_front_torso.bmp, hero_back_torso.bmp
			BMP hero_shadow;    // @META@ hero_shadow.bmp
			BMP background;     // @META@ background.bmp
		}   bmp;
		BMP bmps[sizeof(bmp) / sizeof(BMP)];
	};
	#include "meta/bmp_file_paths.h"

	ChunkNode* available_chunk_node;
	ChunkNode  chunk_node_hash_table[64];

	Chunk*     hero_chunk;
	vf2        hero_rel_pos;
	vf2        hero_vel;
	Cardinal   hero_direction;

	f32        pet_hover_t;
	Chunk*     pet_chunk;
	vf2        pet_rel_pos;
	vf2        pet_vel;
	Cardinal   pet_direction;

	vi2        camera_coords;
	vf2        camera_rel_pos;
	vf2        camera_vel;

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
			aliasing top = bmp->rgba[(platform_framebuffer->dims.y - bottom_left.y - 1 - y) * bmp->dims.x + x - bottom_left.x];

			if (top.a == 255)
			{
				dst = pack_as_raw_argb(top);
			}
			else if (top.a)
			{
				dst = pack_as_raw_argb(rgba_from(lerp(vf3_from(unpack_raw_argb(dst)), vf3_from(top), top.a / 255.0f)));
			}
		}
	}
}

internal void draw_circle(PlatformFramebuffer* platform_framebuffer, vi2 pos, i32 radius, RGBA rgba)
{
	i32 x0          = clamp(                               pos.x - radius, 0, platform_framebuffer->dims.x);
	i32 x1          = clamp(                               pos.x + radius, 0, platform_framebuffer->dims.x);
	i32 y0          = clamp(platform_framebuffer->dims.y - pos.y - radius, 0, platform_framebuffer->dims.y);
	i32 y1          = clamp(platform_framebuffer->dims.y - pos.y + radius, 0, platform_framebuffer->dims.y);
	u32 pixel_value = pack_as_raw_argb(rgba);

	FOR_RANGE(x, x0, x1)
	{
		FOR_RANGE(y, y0, y1)
		{
			if (square(x - pos.x) + square(y - platform_framebuffer->dims.y + pos.y) <= square(radius))
			{
				platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x] = pixel_value;
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

internal Chunk* get_chunk(State* state, vi2 coords)
{
	// @TODO@ Better hash function...
	i32 hash = mod(coords.x * 13 + coords.y * 7 + coords.x * coords.y * 17, ARRAY_CAPACITY(state->chunk_node_hash_table));

	ChunkNode* chunk_node = &state->chunk_node_hash_table[hash];

	while (true)
	{
		if (chunk_node->chunk.exists)
		{
			if (chunk_node->chunk.coords == coords)
			{
				break;
			}
			else
			{
				if (!chunk_node->next_node)
				{
					DEBUG_printf("Chunk (%d %d) allocated on arena!\n", PASS_V2(coords));
					chunk_node->next_node               = memory_arena_allocate_from_available(&state->available_chunk_node, &state->arena);
					chunk_node->next_node->chunk.exists = false;
				}

				chunk_node = chunk_node->next_node;
			}
		}
		else
		{
			DEBUG_printf("Chunk (%d %d) marked as existing!\n", PASS_V2(coords));
			*chunk_node =
				{
					.chunk =
						{
							.exists = true,
							.coords = coords
						}
				};
			break;
		}
	}

	return &chunk_node->chunk;
}

PlatformUpdate_t(PlatformUpdate)
{
	State* state = reinterpret_cast<State*>(platform_memory);

	if (!state->is_initialized)
	{
		state->is_initialized = true;

		state->arena =
			{
				.size = PLATFORM_MEMORY_SIZE - sizeof(State),
				.base = platform_memory      + sizeof(State)
			};

		FOR_ELEMS(bmp, state->bmps)
		{
			PlatformFileData file_data;
			if (!PlatformReadFileData(&file_data, state->BMP_FILE_PATHS[bmp_index]))
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
			bmp->rgba = memory_arena_allocate<RGBA>(&state->arena, static_cast<u64>(bmp->dims.x) * bmp->dims.y);

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

		vi2 coords = { 0, 0 };
		FOR_RANGE(8)
		{
			Chunk* chunk = get_chunk(state, coords);

			FOR_RANGE(16)
			{
				ASSERT(IN_RANGE(chunk->wall_count, 0, ARRAY_CAPACITY(chunk->wall_buffer)));
				chunk->wall_buffer[chunk->wall_count] =
					{
						.rel_pos = vxx(rng(&state->seed, 0, static_cast<i32>(METERS_PER_CHUNK)), rng(&state->seed, 0, static_cast<i32>(METERS_PER_CHUNK - 1))),
						.dims    = { 1.0f, 1.0f }
					};
				chunk->wall_count += 1;
			}

			if (rng(&state->seed) < 0.5f)
			{
				coords.x += 1;
			}
			else
			{
				coords.y += 1;
			}
		}

		state->hero_chunk   = get_chunk(state, { 0, 0 });
		state->hero_rel_pos = { 5.0f, 7.0f };

		state->pet_chunk   = get_chunk(state, { 0, 0 });
		state->pet_rel_pos = { 7.0f, 9.0f };
	}

	//
	// (Update) Hero.
	//

	constexpr f32 HERO_HITBOX_RADIUS = 0.3f;
	constexpr f32 PET_HITBOX_RADIUS  = 0.3f;
	{
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

		vf2 displacement = state->hero_vel * platform_delta_time;

		// @TODO@ Better performing collision.
		FOR_RANGE(8)
		{
			CollisionResult result = {};

			// @TODO@ This checks chunks in a 3x3 adjacenct fashion. Could be better.
			FOR_RANGE(i, 9)
			{
				Chunk* chunk = get_chunk(state, state->hero_chunk->coords + vi2 { i % 3 - 1, i / 3 - 1 });

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
								chunk->coords * METERS_PER_CHUNK + wall->rel_pos,
								wall->dims,
								HERO_HITBOX_RADIUS
							)
						);
				}
			}

			result =
				prioritize_collision_results
				(
					result,
					collide_against_circle
					(
						state->hero_chunk->coords * METERS_PER_CHUNK + state->hero_rel_pos,
						displacement,
						state->pet_chunk->coords * METERS_PER_CHUNK + state->pet_rel_pos,
						HERO_HITBOX_RADIUS + PET_HITBOX_RADIUS
					)
				);

			state->hero_rel_pos +=
				result.type == CollisionType::none
					? displacement
					: result.new_displacement;

			vi2 delta_coords = { 0, 0 };
			if      (state->hero_rel_pos.x <              0.0f) { delta_coords.x -= 1; }
			else if (state->hero_rel_pos.x >= METERS_PER_CHUNK) { delta_coords.x += 1; }
			if      (state->hero_rel_pos.y <              0.0f) { delta_coords.y -= 1; }
			else if (state->hero_rel_pos.y >= METERS_PER_CHUNK) { delta_coords.y += 1; }
			ASSERT(state->hero_chunk->exists);
			state->hero_chunk    = get_chunk(state, state->hero_chunk->coords + delta_coords);
			state->hero_rel_pos -= delta_coords * METERS_PER_CHUNK;

			if (result.type == CollisionType::none)
			{
				break;
			}
			else
			{
				state->hero_vel = dot(state->hero_vel - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
				displacement    = dot(displacement    - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
			}
		}
	}

	//
	// (Update) Pet.
	//

	state->pet_hover_t = mod(state->pet_hover_t + platform_delta_time / 4.0f, 1.0f);

	{
		{
			vf2 target_pet_vel   = { 0.0f, 0.0f };
			vf2 ray_to_hero      = (state->hero_chunk->coords - state->pet_chunk->coords) * METERS_PER_CHUNK + state->hero_rel_pos- state->pet_rel_pos;
			f32 distance_to_hero = norm(ray_to_hero);
			if (IN_RANGE(distance_to_hero, 1.0f, 7.0f))
			{
				ray_to_hero /= distance_to_hero;

				if (distance_to_hero > 3.0f)
				{
					target_pet_vel = ray_to_hero * 2.0f;
				}

				if      (ray_to_hero.x < -1.0f / SQRT2) { state->pet_direction = Cardinal::left;  }
				else if (ray_to_hero.x >  1.0f / SQRT2) { state->pet_direction = Cardinal::right; }
				else if (ray_to_hero.y < -1.0f / SQRT2) { state->pet_direction = Cardinal::down;  }
				else if (ray_to_hero.y >  1.0f / SQRT2) { state->pet_direction = Cardinal::up;    }
			}

			state->pet_vel = dampen(state->pet_vel, target_pet_vel, 0.1f, platform_delta_time);
		}

		vf2 displacement = state->pet_vel * platform_delta_time;

		// @TODO@ Better performing collision.
		FOR_RANGE(8)
		{
			CollisionResult result = {};

			// @TODO@ This checks chunks in a 3x3 adjacenct fashion. Could be better.
			FOR_RANGE(i, 9)
			{
				Chunk* chunk = get_chunk(state, state->pet_chunk->coords + vi2 { i % 3 - 1, i / 3 - 1 });

				FOR_ELEMS(wall, chunk->wall_buffer, chunk->wall_count)
				{
					result =
						prioritize_collision_results
						(
							result,
							collide_against_rounded_rectangle
							(
								state->pet_chunk->coords * METERS_PER_CHUNK + state->pet_rel_pos,
								displacement,
								chunk->coords * METERS_PER_CHUNK + wall->rel_pos,
								wall->dims,
								PET_HITBOX_RADIUS
							)
						);
				}
			}

			result =
				prioritize_collision_results
				(
					result,
					collide_against_circle
					(
						state->pet_chunk->coords * METERS_PER_CHUNK + state->pet_rel_pos,
						displacement,
						state->hero_chunk->coords * METERS_PER_CHUNK + state->hero_rel_pos,
						HERO_HITBOX_RADIUS + PET_HITBOX_RADIUS
					)
				);

			state->pet_rel_pos +=
				result.type == CollisionType::none
					? displacement
					: result.new_displacement;

			vi2 delta_coords = { 0, 0 };
			if      (state->pet_rel_pos.x <              0.0f) { delta_coords.x -= 1; }
			else if (state->pet_rel_pos.x >= METERS_PER_CHUNK) { delta_coords.x += 1; }
			if      (state->pet_rel_pos.y <              0.0f) { delta_coords.y -= 1; }
			else if (state->pet_rel_pos.y >= METERS_PER_CHUNK) { delta_coords.y += 1; }
			ASSERT(state->pet_chunk->exists);
			state->pet_chunk    = get_chunk(state, state->pet_chunk->coords + delta_coords);
			state->pet_rel_pos -= delta_coords * METERS_PER_CHUNK;

			if (result.type == CollisionType::none)
			{
				break;
			}
			else
			{
				state->pet_vel = dot(state->pet_vel - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
				displacement   = dot(displacement   - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
			}
		}
	}

	//
	// (Update) camera.
	//

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

	//DEBUG_printf("(%f %f)\n", PASS_V2(state->hero_rel_pos));

	//
	// Render.
	//

	memset(platform_framebuffer->pixels, 0, platform_framebuffer->dims.x * platform_framebuffer->dims.y * sizeof(u32));

	draw_bmp(platform_framebuffer, &state->bmp.background, { 0, 0 });

	FOR_ELEMS(chunk_node, state->chunk_node_hash_table) // @TODO@ This is going through the hash table to render. Bad!
	{
		if (chunk_node->chunk.exists)
		{
			FOR_ELEMS(wall, chunk_node->chunk.wall_buffer, chunk_node->chunk.wall_count)
			{
				draw_rect
				(
					platform_framebuffer,
					vxx(((chunk_node->chunk.coords - state->camera_coords) * METERS_PER_CHUNK + wall->rel_pos - state->camera_rel_pos) * PIXELS_PER_METER),
					vxx(wall->dims * PIXELS_PER_METER),
					rgba_from(0.15f, 0.225f, 0.2f)
				);
			}
		}
	}

	draw_circle
	(
		platform_framebuffer,
		vxx(((state->hero_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->hero_rel_pos - state->camera_rel_pos) * PIXELS_PER_METER),
		static_cast<i32>(HERO_HITBOX_RADIUS * PIXELS_PER_METER),
		rgba_from(0.9f, 0.5f, 0.1f)
	);

	{
		vi2 hero_screen_coords = vxx(((state->hero_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->hero_rel_pos - state->camera_rel_pos) * PIXELS_PER_METER - state->bmp.hero_torso[0].dims * vf2 { 0.5f, 0.225f });
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow, hero_screen_coords);
		draw_bmp(platform_framebuffer, &state->bmp.hero_torso[static_cast<i32>(state->hero_direction)], hero_screen_coords);
		draw_bmp(platform_framebuffer, &state->bmp.hero_cape [static_cast<i32>(state->hero_direction)], hero_screen_coords);
		draw_bmp(platform_framebuffer, &state->bmp.hero_head [static_cast<i32>(state->hero_direction)], hero_screen_coords);
	}

	draw_circle
	(
		platform_framebuffer,
		vxx(((state->pet_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->pet_rel_pos - state->camera_rel_pos) * PIXELS_PER_METER),
		static_cast<i32>(PET_HITBOX_RADIUS * PIXELS_PER_METER),
		rgba_from(0.9f, 0.5f, 0.8f)
	);

	{
		vi2 pet_screen_coords = vxx(((state->pet_chunk->coords - state->camera_coords) * METERS_PER_CHUNK + state->pet_rel_pos - state->camera_rel_pos) * PIXELS_PER_METER - state->bmp.hero_torso[0].dims * vf2 { 0.5f, 0.225f });
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow, pet_screen_coords);
		draw_bmp(platform_framebuffer, &state->bmp.hero_head[static_cast<i32>(state->pet_direction)], vxx(pet_screen_coords + vf2 { 0.0f, -0.4f * (1.0f - cosf(state->pet_hover_t * TAU)) / 2.0f } * PIXELS_PER_METER));
	}

	#if DEBUG_AUDIO
	state->hertz = 512.0f + platform_input->gamepads[0].stick_right.y * 100.0f;
	state->t     = mod(state->t, TAU);
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
