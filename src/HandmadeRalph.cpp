#include "unified.h"
#include "platform.h"
#include "rng.cpp"

#define DEBUG_AUDIO 0

// @TODO@ Memory arena that return 0 if there isn't enough space left?
// @TODO@ Handle world chunk edges.

constexpr f32 PIXELS_PER_METER = 50.0f;
constexpr f32 METERS_PER_CHUNK = 16.0f;
constexpr f32 GRAVITY          = -9.80665f;

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

constexpr vf2 CARDINAL_VF2[4] = { { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f, }, { 0.0f, 1.0f } };
constexpr vi2 CARDINAL_VI2[4] = { { -1   , 0    }, { 1   , 0    }, { 0   , -1   , }, { 0   , 1    } };
enum Cardinal : u8
{
	Cardinal_left,
	Cardinal_right,
	Cardinal_down,
	Cardinal_up
};

struct Rock
{
	i32    bmp_index;
	Chunk* chunk;
	vf3    rel_pos;
	vf3    vel;
	f32    existence_t;
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
			BMP hero_heads [4]; // @META@ hero_left_head.bmp , hero_right_head.bmp , hero_front_head.bmp , hero_back_head.bmp
			BMP hero_capes [4]; // @META@ hero_left_cape.bmp , hero_right_cape.bmp , hero_front_cape.bmp , hero_back_cape.bmp
			BMP hero_torsos[4]; // @META@ hero_left_torso.bmp, hero_right_torso.bmp, hero_front_torso.bmp, hero_back_torso.bmp
			BMP hero_shadow;    // @META@ hero_shadow.bmp
			BMP background;     // @META@ background.bmp
			BMP rocks      [4]; // @META@ rock00.bmp         , rock01.bmp          , rock02.bmp          , rock03.bmp
		}   bmp;
		BMP bmps[sizeof(bmp) / sizeof(BMP)];
	};
	#include "meta/bmp_file_paths.h"

	ChunkNode* available_chunk_node;
	ChunkNode  chunk_node_hash_table[64];

	Chunk*     hero_chunk;
	vf3        hero_rel_pos;
	vf3        hero_vel;
	Cardinal   hero_cardinal;

	f32        pet_hover_t;
	Chunk*     pet_chunk;
	vf3        pet_rel_pos;
	vf3        pet_vel;
	Cardinal   pet_cardinal;

	Rock       rock_buffer[16];
	i32        rock_count;
	f32        rock_throw_t;

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
	ASSERT(IN_RANGE(dims.x, 0, 1024));
	ASSERT(IN_RANGE(dims.y, 0, 1024));
	u32 pixel_value = pack_as_raw_argb(rgba);
	FOR_RANGE(y, max(platform_framebuffer->dims.y - bottom_left.y - dims.y, 0), min(platform_framebuffer->dims.y - bottom_left.y, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(bottom_left.x, 0), min(bottom_left.x + dims.x, platform_framebuffer->dims.x))
		{
			platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x] = pixel_value;
		}
	}
}

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
	ASSERT(IN_RANGE(radius, 0, 128));
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

internal CollisionResult collide_against_plane(vf2 displacement, vf2 plane_center, vf2 plane_normal)
{
	f32 inside_amount       = dot(plane_normal, plane_center);
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

internal CollisionResult collide_against_line(vf2 displacement, vf2 line_center, vf2 line_normal, f32 padding)
{
	vf2 plane_normal = line_normal * (dot(line_center, line_normal) > 0.0f ? -1.0f : 1.0f);
	return collide_against_plane(displacement, line_center + plane_normal * padding, plane_normal);
}

// @TODO@ Bugs out on `radius == 0.0f`
internal CollisionResult collide_against_circle(vf2 displacement, vf2 center, f32 radius)
{
	f32 distance_from_center    = norm(center);
	f32 amount_away_from_center = -dot(center, displacement);

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
							.normal           = -center / distance_from_center,
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
				.new_displacement = -center * (radius / distance_from_center - 1.0f),
				.normal           = -center / distance_from_center,
				.priority         = radius - distance_from_center
			};
	}
}

internal CollisionResult collide_against_rounded_rectangle(vf2 displacement, vf2 rect_bottom_left, vf2 rect_dims, f32 padding)
{
	CollisionResult left_right = collide_against_line(displacement, rect_bottom_left + vf2 { rect_dims.x / 2.0f, 0.0f }, { 1.0f, 0.0f }, rect_dims.x / 2.0f + padding);
	if (left_right.type != CollisionType::none && !IN_RANGE(left_right.new_displacement.y - rect_bottom_left.y, 0.0f, rect_dims.y))
	{
		left_right.type = CollisionType::none;
	}
	CollisionResult bottom_top = collide_against_line(displacement, rect_bottom_left + vf2 { 0.0f, rect_dims.y / 2.0f }, { 0.0f, 1.0f }, rect_dims.y / 2.0f + padding);
	if (bottom_top.type != CollisionType::none && !IN_RANGE(bottom_top.new_displacement.x - rect_bottom_left.x, 0.0f, rect_dims.x))
	{
		bottom_top.type = CollisionType::none;
	}

	return
		prioritize_collision_results
		(
			prioritize_collision_results(bottom_top, left_right),
			prioritize_collision_results
			(
				prioritize_collision_results(collide_against_circle(displacement, rect_bottom_left                            , padding), collide_against_circle(displacement, rect_bottom_left + vf2 { rect_dims.x, 0.0f }, padding)),
				prioritize_collision_results(collide_against_circle(displacement, rect_bottom_left + vf2 { 0.0f, rect_dims.y }, padding), collide_against_circle(displacement, rect_bottom_left +       rect_dims          , padding))
			)
		);
}

// @TODO@ Add the other shapes.
enum struct CollisionShapeType : u8
{
	null,
	point,
	circle,
	rounded_rectangle
};

struct CollisionShape
{
	CollisionShapeType type;
	union
	{
		struct
		{
			f32 radius;
		} circle;

		struct
		{
			vf2 dims;
			f32 padding;
		} rounded_rectangle;
	};
};

internal CollisionResult collide_shapes(vf2 displacement, CollisionShape a, vf2 pos, CollisionShape b)
{
	if (a.type <= b.type)
	{
		switch (a.type)
		{
			case CollisionShapeType::point: switch (b.type)
			{
				case CollisionShapeType::point: return
					{};

				case CollisionShapeType::circle: return
					collide_against_circle(displacement, pos, b.circle.radius);

				case CollisionShapeType::rounded_rectangle: return
					collide_against_rounded_rectangle(displacement, pos, b.rounded_rectangle.dims, b.rounded_rectangle.padding);

				default: ASSERT(false);
					return {};
			}

			case CollisionShapeType::circle: switch (b.type)
			{
				case CollisionShapeType::circle: return
					collide_against_circle(displacement, pos, a.circle.radius + b.circle.radius);

				case CollisionShapeType::rounded_rectangle: return
					collide_against_rounded_rectangle(displacement, pos, b.rounded_rectangle.dims, b.rounded_rectangle.padding + a.circle.radius);

				default: ASSERT(false);
					return {};
			}

			case CollisionShapeType::rounded_rectangle: switch (b.type)
			{
				case CollisionShapeType::rounded_rectangle: return
					collide_against_rounded_rectangle(displacement, pos, a.rounded_rectangle.dims + b.rounded_rectangle.dims, b.rounded_rectangle.padding + b.rounded_rectangle.padding);

				default: ASSERT(false);
					return {};
			}

			default: ASSERT(false);
				return {};
		}
	}
	else
	{
		CollisionResult result = collide_shapes(-displacement, b, -pos, a);
		return
			{
				.type             =  result.type,
				.new_displacement = -result.new_displacement,
				.normal           = -result.normal,
				.priority         =  result.priority
			};
	}
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
					chunk_node->next_node               = memory_arena_allocate_from_available(&state->available_chunk_node, &state->arena);
					chunk_node->next_node->chunk.exists = false;
				}

				chunk_node = chunk_node->next_node;
			}
		}
		else
		{
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

constexpr CollisionShape HERO_COLLISION_SHAPE =
	{
		.type   = CollisionShapeType::circle,
		.circle = { .radius = 0.3f }
	};

constexpr CollisionShape PET_COLLISION_SHAPE =
	{
		.type   = CollisionShapeType::circle,
		.circle = { .radius = 0.3f }
	};

constexpr CollisionShape ROCK_COLLISION_SHAPE =
	{
		.type   = CollisionShapeType::circle,
		.circle = { .radius = 0.6f }
	};

enum struct MoveTag : u8
{
	null,
	hero,
	pet,
	rock
};

internal void process_move(Chunk** chunk, vf3* rel_pos, vf3* vel, CollisionShape shape, MoveTag tag, void* semantic, State* state, f32 delta_time)
{
	vel->z += GRAVITY * delta_time;

	vf3 displacement = *vel * delta_time;

	// @TODO@ Better performing collision.
	FOR_RANGE(8)
	{
		CollisionResult result = {};

		// @TODO@ This checks chunks in a 3x3 adjacenct fashion. Could be better.
		FOR_RANGE(i, 9)
		{
			Chunk* wall_chunk = get_chunk(state, (*chunk)->coords + vi2 { i % 3 - 1, i / 3 - 1 });

			FOR_ELEMS(wall, wall_chunk->wall_buffer, wall_chunk->wall_count)
			{
				result =
					prioritize_collision_results
					(
						result,
						collide_shapes
						(
							displacement.xy,
							shape,
							(wall_chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + wall->rel_pos - rel_pos->xy,
							{
								.type              = CollisionShapeType::rounded_rectangle,
								.rounded_rectangle =
									{
										.dims    = wall->dims,
										.padding = 0.0f // @TODO@ Rectangle shape type.
									}
							}
						)
					);
			}
		}

		if (tag != MoveTag::hero)
		{
			FOR_ELEMS(rock, state->rock_buffer, state->rock_count)
			{
				if (IMPLIES(tag == MoveTag::rock, semantic != rock))
				{
					result =
						prioritize_collision_results
						(
							result,
							collide_shapes
							(
								displacement.xy,
								shape,
								(rock->chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + rock->rel_pos.xy - rel_pos->xy,
								ROCK_COLLISION_SHAPE
							)
						);
				}
			}
		}

		if (tag != MoveTag::hero && tag != MoveTag::rock)
		{
			result =
				prioritize_collision_results
				(
					result,
					collide_shapes
					(
						displacement.xy,
						shape,
						(state->hero_chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + state->hero_rel_pos.xy - rel_pos->xy,
						HERO_COLLISION_SHAPE
					)
				);
		}

		if (tag != MoveTag::pet)
		{
			result =
				prioritize_collision_results
				(
					result,
					collide_shapes
					(
						displacement.xy,
						shape,
						(state->pet_chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + state->pet_rel_pos.xy - rel_pos->xy,
						PET_COLLISION_SHAPE
					)
				);
		}

		rel_pos->xy +=
			result.type == CollisionType::none
				? displacement.xy
				: result.new_displacement;

		vi2 delta_coords = { 0, 0 };
		if      (rel_pos->x <              0.0f) { delta_coords.x -= 1; }
		else if (rel_pos->x >= METERS_PER_CHUNK) { delta_coords.x += 1; }
		if      (rel_pos->y <              0.0f) { delta_coords.y -= 1; }
		else if (rel_pos->y >= METERS_PER_CHUNK) { delta_coords.y += 1; }
		ASSERT((*chunk)->exists);
		*chunk       = get_chunk(state, (*chunk)->coords + delta_coords);
		rel_pos->xy -= delta_coords * METERS_PER_CHUNK;

		rel_pos->z += displacement.z;
		if (rel_pos->z <= 0.0f)
		{
			rel_pos->z  = 0.0f;
			vel->z     *= -0.35f;

			if (tag == MoveTag::rock)
			{
				vel->xy = dampen(vel->xy, { 0.0f, 0.0f }, 0.005f, delta_time);
			}
		}

		if (result.type == CollisionType::none)
		{
			break;
		}
		else
		{
			if (tag == MoveTag::rock)
			{
				vel->xy         = 0.15f * (dot(vel->xy         - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal) * 2.0f + (result.new_displacement - vel->xy   ));
				displacement.xy = 0.15f * (dot(displacement.xy - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal) * 2.0f + (result.new_displacement - displacement.xy));
			}
			else
			{
				vel->xy         = dot(vel->xy         - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
				displacement.xy = dot(displacement.xy - result.new_displacement, rotate90(result.normal)) * rotate90(result.normal);
			}

			displacement.z  = 0.0f;
		}
	}
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
			PlatformFileData file_data = PlatformReadFileData(state->BMP_FILE_PATHS[bmp_index]);
			if (!file_data.data)
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
	// Update Hero.
	//

	{
		vf2 target_hero_vel = vxx(WASD_DOWN());
		if (+target_hero_vel)
		{
			if      (target_hero_vel.x < 0.0f) { state->hero_cardinal = Cardinal_left;  }
			else if (target_hero_vel.x > 0.0f) { state->hero_cardinal = Cardinal_right; }
			else if (target_hero_vel.y < 0.0f) { state->hero_cardinal = Cardinal_down;  }
			else if (target_hero_vel.y > 0.0f) { state->hero_cardinal = Cardinal_up;    }

			target_hero_vel = normalize(target_hero_vel) * 1.78816f;

			if (BTN_DOWN(.shift))
			{
				target_hero_vel *= 2.0f;
			}
		}

		state->hero_vel.xy = dampen(state->hero_vel.xy, target_hero_vel, 0.001f, platform_delta_time);

		process_move(&state->hero_chunk, &state->hero_rel_pos, &state->hero_vel, HERO_COLLISION_SHAPE, MoveTag::hero, 0, state, platform_delta_time);
	}

	//
	// Update Pet.
	//

	state->pet_hover_t = mod(state->pet_hover_t + platform_delta_time / 4.0f, 1.0f);

	{
		vf2 target_pet_vel   = { 0.0f, 0.0f };
		vf2 ray_to_hero      = (state->hero_chunk->coords - state->pet_chunk->coords) * METERS_PER_CHUNK + state->hero_rel_pos.xy - state->pet_rel_pos.xy;
		f32 distance_to_hero = norm(ray_to_hero);
		if (IN_RANGE(distance_to_hero, 1.0f, 7.0f))
		{
			ray_to_hero /= distance_to_hero;

			if (distance_to_hero > 3.0f)
			{
				target_pet_vel = ray_to_hero * 2.0f;
			}

			if      (ray_to_hero.x < -1.0f / SQRT2) { state->pet_cardinal = Cardinal_left;  }
			else if (ray_to_hero.x >  1.0f / SQRT2) { state->pet_cardinal = Cardinal_right; }
			else if (ray_to_hero.y < -1.0f / SQRT2) { state->pet_cardinal = Cardinal_down;  }
			else if (ray_to_hero.y >  1.0f / SQRT2) { state->pet_cardinal = Cardinal_up;    }
		}

		state->pet_vel.xy = dampen(state->pet_vel.xy, target_pet_vel, 0.1f, platform_delta_time);

		process_move(&state->pet_chunk, &state->pet_rel_pos, &state->pet_vel, PET_COLLISION_SHAPE, MoveTag::pet, 0, state, platform_delta_time);
	}

	//
	// Update rocks.
	//

	{
		state->rock_throw_t -= platform_delta_time / 1.0f;

		if (BTN_PRESSES(.space) && state->rock_throw_t <= 0.0f)
		{
			state->rock_throw_t = 1.0f;

			// @TODO@ Nice way to find out if a rock is on top of an entity.
			bool32 colliding = false;
			FOR_ELEMS(rock, state->rock_buffer, state->rock_count)
			{
				if (collide_shapes({ 0.0f, 0.0f }, ROCK_COLLISION_SHAPE, (rock->chunk->coords - state->hero_chunk->coords) * METERS_PER_CHUNK + rock->rel_pos.xy - state->hero_rel_pos.xy, ROCK_COLLISION_SHAPE).type != CollisionType::none)
				{
					colliding = true;
					break;
				}
			}
			if (!colliding)
			{
				ASSERT(IN_RANGE(state->rock_count, 0, ARRAY_CAPACITY(state->rock_buffer)));
				state->rock_buffer[state->rock_count] =
					{
						.bmp_index   = rng(&state->seed, 0, ARRAY_CAPACITY(state->bmp.rocks)),
						.chunk       = state->hero_chunk,
						.rel_pos     = state->hero_rel_pos + vf3 { 0.0f, 0.0f, 1.0f },
						.vel         = vxx(state->hero_vel.xy + CARDINAL_VF2[state->hero_cardinal] * 2.0f, 1.0f),
						.existence_t = 1.0f
					};
				state->rock_count += 1;
			}
		}
	}

	FOR_ELEMS(rock, state->rock_buffer, state->rock_count)
	{
		if (rock->existence_t > 0.0f)
		{
			constexpr f32 DURATION = 4.0f;
			f32 old_existence_t = rock->existence_t;
			rock->existence_t = clamp(rock->existence_t - platform_delta_time / DURATION, 0.0f, 1.0f);
			process_move(&rock->chunk, &rock->rel_pos, &rock->vel, ROCK_COLLISION_SHAPE, MoveTag::rock, rock, state, (old_existence_t - rock->existence_t) * DURATION);
		}
	}

	for (i32 i = 0; i < state->rock_count;)
	{
		if (state->rock_buffer[i].existence_t <= 0.0f)
		{
			state->rock_count     -= 1;
			state->rock_buffer[i]  = state->rock_buffer[state->rock_count];
		}
		else
		{
			i += 1;
		}
	}

	//
	// Update camera.
	//

	{
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
	}

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

	lambda screen_coords_of =
		[&](vi2 chunk_coords, vf3 rel_pos)
		{
			return vxx(((chunk_coords - state->camera_coords) * METERS_PER_CHUNK + rel_pos.xy - state->camera_rel_pos + vf2 { 0.0f, 1.0f } * rel_pos.z) * PIXELS_PER_METER);
		};

	draw_circle
	(
		platform_framebuffer,
		screen_coords_of(state->hero_chunk->coords, state->hero_rel_pos),
		static_cast<i32>(HERO_COLLISION_SHAPE.circle.radius * PIXELS_PER_METER),
		rgba_from(0.9f, 0.5f, 0.1f)
	);

	{
		vi2 hero_screen_coords = screen_coords_of(state->hero_chunk->coords, state->hero_rel_pos) - vxx(state->bmp.hero_torsos[0].dims * vf2 { 0.5f, 0.225f });
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow, hero_screen_coords);
		draw_bmp(platform_framebuffer, &state->bmp.hero_torsos[state->hero_cardinal], hero_screen_coords);
		draw_bmp(platform_framebuffer, &state->bmp.hero_capes [state->hero_cardinal], hero_screen_coords);
		draw_bmp(platform_framebuffer, &state->bmp.hero_heads [state->hero_cardinal], hero_screen_coords);
	}

	draw_circle
	(
		platform_framebuffer,
		screen_coords_of(state->pet_chunk->coords, state->pet_rel_pos),
		static_cast<i32>(PET_COLLISION_SHAPE.circle.radius * PIXELS_PER_METER),
		rgba_from(0.9f, 0.5f, 0.8f)
	);

	{
		vi2 pet_screen_coords = screen_coords_of(state->pet_chunk->coords, state->pet_rel_pos) - vxx(state->bmp.hero_torsos[0].dims * vf2 { 0.5f, 0.225f });
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow, pet_screen_coords);
		draw_bmp(platform_framebuffer, &state->bmp.hero_heads[state->pet_cardinal], vxx(pet_screen_coords + vf2 { 0.0f, -0.4f * (1.0f - cosf(state->pet_hover_t * TAU)) / 2.0f } * PIXELS_PER_METER));
	}

	FOR_ELEMS(rock, state->rock_buffer, state->rock_count)
	{
		ASSERT(IN_RANGE(rock->bmp_index, 0, ARRAY_CAPACITY(state->bmp.rocks)));
		draw_circle
		(
			platform_framebuffer,
			screen_coords_of(rock->chunk->coords, vxx(rock->rel_pos.xy, 0.0f)),
			static_cast<i32>(ROCK_COLLISION_SHAPE.circle.radius * PIXELS_PER_METER),
			rgba_from(0.2f, 0.4f, 0.3f)
		);
		draw_bmp
		(
			platform_framebuffer,
			&state->bmp.hero_shadow,
			screen_coords_of(rock->chunk->coords, vxx(rock->rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow.dims * vf2 { 0.5f, 0.225f })
		);
		draw_bmp
		(
			platform_framebuffer,
			&state->bmp.rocks[rock->bmp_index],
			screen_coords_of(rock->chunk->coords, rock->rel_pos) - vxx(state->bmp.rocks[rock->bmp_index].dims * vf2 { 0.5f, 0.5f })
		);
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
