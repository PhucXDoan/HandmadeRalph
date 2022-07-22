#include "unified.h"
#include "platform.h"
#include "rng.cpp"

constexpr i32 CHUNK_DIM = 16;

struct BMP
{
	vi2  dims;
	u32* rgba;
};

enum Cardinal : u8 // @META@ vf2 vf; vi2 vi;
{
	Cardinal_left,  // @META@ { -1.0f,  0.0f }, { -1,  0 }
	Cardinal_right, // @META@ {  1.0f,  0.0f }, {  1,  0 }
	Cardinal_down,  // @META@ {  0.0f, -1.0f }, {  0, -1 }
	Cardinal_up,    // @META@ {  0.0f,  1.0f }, {  0,  1 }
};
#include "META/enum/Cardinal.h"

struct Hero
{
	vi2      coords;
	vf3      rel_pos;
	Cardinal cardinal;
	i32      hp;
};

struct Tree
{
	vi2 coords;
	i32 bmp_index;
};

struct Pet
{
	vi2      coords;
	vf3      rel_pos;
	Cardinal cardinal;
	f32      hover_t;
	f32      move_t;
};

enum struct MonstarFlag : u8
{
	fast       = 1 << 0,
	flying     = 1 << 1,
	strong     = 1 << 2,
	attractive = 1 << 3,
};
#include "META/flag/MonstarFlag.h"

struct Monstar
{
	vi2         coords;
	vf3         rel_pos;
	Cardinal    cardinal;
	f32         hover_t;
	f32         move_t;
	i32         hp;
	f32         existence_t;
	MonstarFlag flag;
};

struct PressurePlate
{
	vi2    coords;
	bool32 pressed;
};

#include "META/kind/Entity.h" // @META@ Hero, Tree, Pet, Monstar, PressurePlate

struct Chunk
{
	bool32    exists;
	vi2       coords;
	Tree      tree_buffer[32];
	i32       tree_count;
	struct
	{
		EntityRef      entity;
		PressurePlate* pressure_plate;
	} tiles[CHUNK_DIM][CHUNK_DIM];
};

struct State
{
	bool32      inited;
	u32         seed;
	MemoryArena arena;

	union
	{
		struct
		{
			BMP hero_heads [ARRAY_CAPACITY(META_Cardinal)]; // @META@ hero_left_head.bmp , hero_right_head.bmp , hero_front_head.bmp , hero_back_head.bmp
			BMP hero_capes [ARRAY_CAPACITY(META_Cardinal)]; // @META@ hero_left_cape.bmp , hero_right_cape.bmp , hero_front_cape.bmp , hero_back_cape.bmp
			BMP hero_torsos[ARRAY_CAPACITY(META_Cardinal)]; // @META@ hero_left_torso.bmp, hero_right_torso.bmp, hero_front_torso.bmp, hero_back_torso.bmp
			BMP hero_shadow;                                // @META@ hero_shadow.bmp
			BMP background;                                 // @META@ background.bmp
			BMP rocks      [4];                             // @META@ rock00.bmp         , rock01.bmp          , rock02.bmp          , rock03.bmp
			BMP trees      [3];                             // @META@ tree00.bmp         , tree01.bmp          , tree02.bmp
			BMP pressure_plate;                             // @META@ grass00.bmp
		}   bmp;
		BMP bmps[sizeof(bmp) / sizeof(BMP)];
	};
	#include "META/asset/bmp_file_paths.h"

	Chunk         chunk_hashtable[256];
	Hero          hero;
	Pet           pet;
	Monstar       monstar;
	PressurePlate pressure_plate;
	vi2           camera_coords;
};

procedure void draw_rect(PlatformFramebuffer* platform_framebuffer, vi2 center, vi2 dims, u32 rgba)
{
	ASSERT(IN_RANGE(dims.x, 0, 2048));
	ASSERT(IN_RANGE(dims.y, 0, 2048));
	FOR_RANGE(y, max(platform_framebuffer->dims.y - center.y - dims.y / 2, 0), min(platform_framebuffer->dims.y - center.y + dims.y / 2, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(center.x - dims.x / 2, 0), min(center.x + dims.x / 2, platform_framebuffer->dims.x))
		{
			platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x] = rgba;
		}
	}
}

procedure void draw_rect(PlatformFramebuffer* platform_framebuffer, vi2 center, vi2 dims, vf4 rgba)
{
	ASSERT(IN_RANGE(dims.x, 0, 2048));
	ASSERT(IN_RANGE(dims.y, 0, 2048));
	FOR_RANGE(y, max(platform_framebuffer->dims.y - center.y - dims.y / 2, 0), min(platform_framebuffer->dims.y - center.y + dims.y / 2, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(center.x - dims.x / 2, 0), min(center.x + dims.x / 2, platform_framebuffer->dims.x))
		{
			aliasing dst = platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x];
			dst =
				(static_cast<u32>(static_cast<u8>(lerp(static_cast<f32>((dst >> 16) & 255), rgba.x * 255.0f, rgba.w))) << 16) |
				(static_cast<u32>(static_cast<u8>(lerp(static_cast<f32>((dst >>  8) & 255), rgba.y * 255.0f, rgba.w))) <<  8) |
				(static_cast<u32>(static_cast<u8>(lerp(static_cast<f32>((dst >>  0) & 255), rgba.z * 255.0f, rgba.w))) <<  0);
		}
	}
}

procedure void draw_bmp(PlatformFramebuffer* platform_framebuffer, BMP* bmp, vi2 center, f32 alpha = 1.0f)
{
	FOR_RANGE(y, max(platform_framebuffer->dims.y - center.y - bmp->dims.y / 2, 0), min(platform_framebuffer->dims.y - center.y + bmp->dims.y / 2, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(center.x - bmp->dims.x / 2, 0), min(center.x + bmp->dims.x / 2, platform_framebuffer->dims.x))
		{
			aliasing dst = platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x];
			aliasing top = bmp->rgba[(platform_framebuffer->dims.y - center.y + bmp->dims.y / 2 - y) * bmp->dims.x + x - center.x - bmp->dims.x / 2];

			if ((top >> 24) == 255 && alpha == 1.0f)
			{
				dst = top;
			}
			else if ((top >> 24) && alpha > 0.0f)
			{
				dst =
					(static_cast<u32>(static_cast<u8>(static_cast<f32>((dst >> 16) & 255) * (1.0f - ((top >> 24) & 255) / 255.0f * alpha) + static_cast<f32>((top >> 16) & 255) * ((top >> 24) & 255) / 255.0f * alpha)) << 16) |
					(static_cast<u32>(static_cast<u8>(static_cast<f32>((dst >>  8) & 255) * (1.0f - ((top >> 24) & 255) / 255.0f * alpha) + static_cast<f32>((top >>  8) & 255) * ((top >> 24) & 255) / 255.0f * alpha)) <<  8) |
					(static_cast<u32>(static_cast<u8>(static_cast<f32>((dst >>  0) & 255) * (1.0f - ((top >> 24) & 255) / 255.0f * alpha) + static_cast<f32>((top >>  0) & 255) * ((top >> 24) & 255) / 255.0f * alpha)) <<  0);
			}
		}
	}
}

procedure void draw_circle(PlatformFramebuffer* platform_framebuffer, vi2 pos, i32 radius, u32 rgba)
{
	ASSERT(IN_RANGE(radius, 0, 128));
	i32 x0          = clamp(                               pos.x - radius, 0, platform_framebuffer->dims.x);
	i32 x1          = clamp(                               pos.x + radius, 0, platform_framebuffer->dims.x);
	i32 y0          = clamp(platform_framebuffer->dims.y - pos.y - radius, 0, platform_framebuffer->dims.y);
	i32 y1          = clamp(platform_framebuffer->dims.y - pos.y + radius, 0, platform_framebuffer->dims.y);

	FOR_RANGE(x, x0, x1)
	{
		FOR_RANGE(y, y0, y1)
		{
			if (square(x - pos.x) + square(y - platform_framebuffer->dims.y + pos.y) <= square(radius))
			{
				platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x] = rgba;
			}
		}
	}
}

procedure u32 rgba_from(f32 r, f32 g, f32 b)
{
	return ((static_cast<u32>(r * 255.0f) << 16)) | ((static_cast<u32>(g * 255.0f) <<  8)) | ((static_cast<u32>(b * 255.0f) <<  0));
}

procedure u32 rgba_from(vf3 rgb)
{
	return rgba_from(rgb.x, rgb.y, rgb.z);
}

procedure Chunk* get_chunk(State* state, vi2 coords)
{
	vi2 chunk_coords =
		{
			CHUNK_DIM * (coords.x / CHUNK_DIM + (coords.x < 0 && coords.x % CHUNK_DIM ? -1 : 0)),
			CHUNK_DIM * (coords.y / CHUNK_DIM + (coords.y < 0 && coords.y % CHUNK_DIM ? -1 : 0))
		};

	i32 hash = mod(chunk_coords.x * 17 + chunk_coords.y * 13 - 19, ARRAY_CAPACITY(state->chunk_hashtable));

	FOR_RANGE(ARRAY_CAPACITY(state->chunk_hashtable))
	{
		if (!state->chunk_hashtable[hash].exists)
		{
			state->chunk_hashtable[hash].exists = true;
			state->chunk_hashtable[hash].coords = chunk_coords;
		}
		if (state->chunk_hashtable[hash].coords == chunk_coords)
		{
			return &state->chunk_hashtable[hash];
		}

		hash = mod(hash + 1, ARRAY_CAPACITY(state->chunk_hashtable));
	}

	return 0;
}

PlatformUpdate_t(PlatformUpdate)
{
	State* state = reinterpret_cast<State*>(platform_memory);

	if (!state->inited)
	{
		state->inited = true;
		state->arena  =
			{
				.size = PLATFORM_MEMORY_SIZE - sizeof(State),
				.data = platform_memory      + sizeof(State)
			};

		FOR_ELEMS(bmp, state->bmps)
		{
			PlatformFileData file_data = PlatformReadFileData(State::META_bmp_file_paths[bmp_index]);
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

			BitmapHeader header;
			memcpy(&header, file_data.data, sizeof(BitmapHeader));

			if
			(
				(header.name[0] != 'B' || header.name[1] != 'M')                                ||
				(header.file_size != file_data.size)                                            ||
				(header.dib_header_size != 124)                                                 || // @TODO@ For now only BITMAPV5HEADER.
				(header.dims.x <= 0 || header.dims.y <= 0)                                      ||
				(header.color_planes != 1)                                                      ||
				(header.bits_per_pixel != 32)                                                   || // @TODO@ For now must be RGBA.
				(header.compression_method != 3)                                                || // @TODO@ Different compression methods and their meaning?
				(header.pixel_data_size != 4 * static_cast<u32>(header.dims.x * header.dims.y)) ||
				(header.color_count != 0)                                                       ||
				(header.important_colors != 0)                                                  ||
				(~(header.mask_r | header.mask_g | header.mask_b | header.mask_a))              ||
				(  header.mask_r & header.mask_g & header.mask_b & header.mask_a )
			)
			{
				ASSERT(false);
				return PlatformUpdateExitCode::abort;
			}

			bmp->dims = header.dims;
			bmp->rgba = allocate<u32>(&state->arena, static_cast<u64>(bmp->dims.x * bmp->dims.y));

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
						((((bmp_pixel & header.mask_a) << lz_a) >> 24) << 24) |
						((((bmp_pixel & header.mask_r) << lz_r) >> 24) << 16) |
						((((bmp_pixel & header.mask_g) << lz_g) >> 24) <<  8) |
						((((bmp_pixel & header.mask_b) << lz_b) >> 24) <<  0);
				}
			}
		}

		{
			state->hero.coords = { 0, 0 };
			state->hero.hp     = 4;
			Chunk* chunk = get_chunk(state, state->hero.coords);
			ASSERT(chunk->tiles[state->hero.coords.y][state->hero.coords.x].entity.ref_type == EntityType::null);
			chunk->tiles[state->hero.coords.y][state->hero.coords.x].entity = ref(&state->hero);
		}

		{
			state->pet.coords = { 3, 3 };
			Chunk* chunk = get_chunk(state, state->pet.coords);
			ASSERT(chunk->tiles[state->pet.coords.y][state->pet.coords.x].entity.ref_type == EntityType::null);
			chunk->tiles[state->pet.coords.y][state->pet.coords.x].entity = ref(&state->pet);
		}

		{
			state->pressure_plate.coords = { 1, 3 };
			Chunk* chunk = get_chunk(state, state->pressure_plate.coords);
			ASSERT(!chunk->tiles[state->pressure_plate.coords.y][state->pressure_plate.coords.x].pressure_plate);
			chunk->tiles[state->pressure_plate.coords.y][state->pressure_plate.coords.x].pressure_plate = &state->pressure_plate;
		}

		FOR_RANGE(chunk_iy, -4, 4)
		{
			FOR_RANGE(chunk_ix, -4, 4)
			{
				Chunk* chunk = get_chunk(state, { chunk_ix * CHUNK_DIM, chunk_iy * CHUNK_DIM });
				chunk->tree_count = rng(&state->seed, static_cast<i32>(ARRAY_CAPACITY(chunk->tree_buffer)) / 2, static_cast<i32>(ARRAY_CAPACITY(chunk->tree_buffer)));
				FOR_ELEMS(it, chunk->tree_buffer, chunk->tree_count)
				{
					do
					{
						it->coords = chunk->coords + vi2 { rng(&state->seed, 0, CHUNK_DIM), rng(&state->seed, 0, CHUNK_DIM) };
					}
					while (chunk->tiles[it->coords.y - chunk->coords.y][it->coords.x - chunk->coords.x].entity.ref_type != EntityType::null);
					chunk->tiles[it->coords.y - chunk->coords.y][it->coords.x - chunk->coords.x].entity = ref(it);

					it->bmp_index = rng(&state->seed, 0, ARRAY_CAPACITY(state->bmp.trees));
				}
			}
		}
	}

	//
	// Update.
	//

	lambda move =
		[&](EntityRef entity, Cardinal movement)
		{
			vi2* coords;
			vf3* rel_pos;
			switch (entity.ref_type)
			{
				case EntityType::Hero:
				{
					coords  = &entity.Hero_->coords;
					rel_pos = &entity.Hero_->rel_pos;
				} break;

				case EntityType::Pet:
				{
					coords  = &entity.Pet_->coords;
					rel_pos = &entity.Pet_->rel_pos;
				} break;

				case EntityType::Monstar:
				{
					coords  = &entity.Monstar_->coords;
					rel_pos = &entity.Monstar_->rel_pos;
				} break;

				case EntityType::null:
				case EntityType::Tree:
				case EntityType::PressurePlate:
				{
					ASSERT(false);
					return;
				} break;
			}

			vi2 delta_coords = META_Cardinal[movement].vi;

			Chunk* old_chunk = get_chunk(state, *coords               );
			Chunk* new_chunk = get_chunk(state, *coords + delta_coords);

			ASSERT(IN_RANGE(coords->x + - old_chunk->coords.x, 0, CHUNK_DIM));
			ASSERT(IN_RANGE(coords->y + - old_chunk->coords.y, 0, CHUNK_DIM));
			aliasing old_tile  = old_chunk->tiles[coords->y - old_chunk->coords.y][coords->x - old_chunk->coords.x];

			ASSERT(IN_RANGE(coords->x + delta_coords.x - new_chunk->coords.x, 0, CHUNK_DIM));
			ASSERT(IN_RANGE(coords->y + delta_coords.y - new_chunk->coords.y, 0, CHUNK_DIM));
			aliasing new_tile  = new_chunk->tiles[coords->y + delta_coords.y - new_chunk->coords.y][coords->x + delta_coords.x - new_chunk->coords.x];

			ASSERT(old_tile.entity.ref_type != EntityType::null);
			if (new_tile.entity.ref_type == EntityType::null)
			{
				if (old_tile.pressure_plate)
				{
					ASSERT(old_tile.pressure_plate->pressed);
					old_tile.pressure_plate->pressed = false;
				}
				if (new_tile.pressure_plate)
				{
					ASSERT(!new_tile.pressure_plate->pressed);
					new_tile.pressure_plate->pressed = true;
				}

				*coords += delta_coords;
				old_tile.entity = {};
				new_tile.entity = entity;
			}
			else
			{
				{
					Hero*    hero;
					Monstar* monstar;
					if (deref(&hero, entity) && deref(&monstar, new_tile.entity))
					{
						hero->rel_pos.xy += delta_coords / 2.0f;
						monstar->hp       = max(monstar->hp - 1, 0);
					}
				}
				{
					Monstar* monstar;
					Hero*    hero;
					if (deref(&monstar, entity) && deref(&hero, new_tile.entity))
					{
						monstar->rel_pos.xy += delta_coords / 2.0f;

						if (+(monstar->flag & MonstarFlag::strong))
						{
							hero->hp = max(hero->hp - 2, 0);
						}
						else
						{
							hero->hp = max(hero->hp - 1, 0);
						}
					}
				}
			}
		};

	//
	// Update hero.
	//

	{
		vi2 delta_coords = WASD_PRESSES();
		if (+delta_coords)
		{
			if      (delta_coords.x < 0) { state->hero.cardinal = Cardinal_left;  }
			else if (delta_coords.x > 0) { state->hero.cardinal = Cardinal_right; }
			else if (delta_coords.y < 0) { state->hero.cardinal = Cardinal_down;  }
			else if (delta_coords.y > 0) { state->hero.cardinal = Cardinal_up;    }
			move(ref(&state->hero), state->hero.cardinal);
		}
	}

	//
	// Update pet.
	//

	{
		state->hero.rel_pos.xy = dampen(state->hero.rel_pos.xy, { 0.0f, 0.0f }, 0.01f, platform_delta_time);

		state->pet.hover_t += platform_delta_time / 2.0f;
		if (state->pet.hover_t >= 1.0f)
		{
			state->pet.hover_t -= 1.0f;
		}
		state->pet.rel_pos.z = 1.0f + sinf(state->pet.hover_t * TAU) * 0.25f;

		vi2 delta_coords = state->hero.coords - state->pet.coords;
		if (abs(delta_coords.x) >= abs(delta_coords.y))
		{
			if      (delta_coords.x < 0) { state->pet.cardinal = Cardinal_left;  }
			else if (delta_coords.x > 0) { state->pet.cardinal = Cardinal_right; }
		}
		else if (delta_coords.y < 0) { state->pet.cardinal = Cardinal_down;  }
		else if (delta_coords.y > 0) { state->pet.cardinal = Cardinal_up;    }

		state->pet.move_t += platform_delta_time / 0.5f;
		if (state->pet.move_t >= 1.0f)
		{
			if (max(abs(delta_coords.x), abs(delta_coords.y)) <= 2)
			{
				state->pet.move_t = 1.0f;
			}
			else
			{
				delta_coords = { sign(delta_coords.x), sign(delta_coords.y) };
				state->pet.move_t -= 1.0f;
				move(ref(&state->pet), state->pet.cardinal);
			}
		}
	}

	//
	// Update monstar.
	//

	if (state->pressure_plate.pressed && state->monstar.existence_t == 0.0f)
	{
		state->monstar             = {};
		state->monstar.coords      = { 5, 5 };
		state->monstar.hp          = 3;
		state->monstar.existence_t = 1.0f;

		FOR_ELEMS(META_MonstarFlag)
		{
			if (rng(&state->seed) < 0.5f)
			{
				state->monstar.flag |= it->flag;
			}
		}

		Chunk* chunk = get_chunk(state, state->monstar.coords);
		ASSERT(chunk->tiles[state->monstar.coords.y][state->monstar.coords.x].entity.ref_type == EntityType::null);
		chunk->tiles[state->monstar.coords.y][state->monstar.coords.x].entity = ref(&state->monstar);
	}

	if (state->monstar.existence_t != 0.0f)
	{
		if (state->monstar.hp)
		{
			state->monstar.rel_pos.xy = dampen(state->monstar.rel_pos.xy, { 0.0f, 0.0f }, 0.01f, platform_delta_time);

			if (+(state->monstar.flag & MonstarFlag::flying))
			{
				state->monstar.hover_t += platform_delta_time / 3.0f;
				if (state->monstar.hover_t >= 1.0f)
				{
					state->monstar.hover_t -= 1.0f;
				}
				state->monstar.rel_pos.z = 2.0f + sinf(state->monstar.hover_t * TAU) * 0.5f;
			}

			vi2 delta_coords = state->hero.coords - state->monstar.coords;
			if (abs(delta_coords.x) >= abs(delta_coords.y))
			{
				if      (delta_coords.x < 0) { state->monstar.cardinal = Cardinal_left;  }
				else if (delta_coords.x > 0) { state->monstar.cardinal = Cardinal_right; }
			}
			else if (delta_coords.y < 0) { state->monstar.cardinal = Cardinal_down;  }
			else if (delta_coords.y > 0) { state->monstar.cardinal = Cardinal_up;    }

			if (+(state->monstar.flag & MonstarFlag::fast))
			{
				state->monstar.move_t += platform_delta_time / 0.25f;
			}
			else
			{
				state->monstar.move_t += platform_delta_time / 0.75f;
			}
			if (state->monstar.move_t >= 1.0f)
			{
				delta_coords = { sign(delta_coords.x), sign(delta_coords.y) };
				state->monstar.move_t -= 1.0f;
				move(ref(&state->monstar), state->monstar.cardinal);
			}
		}
		else
		{
			state->monstar.existence_t = max(state->monstar.existence_t - platform_delta_time / 1.0f, 0.0f);
			state->monstar.rel_pos.z  = dampen(state->monstar.rel_pos.z, 0.0f, 0.1f, platform_delta_time);

			if (state->monstar.existence_t == 0.0f)
			{
				Chunk*   chunk = get_chunk(state, state->monstar.coords);
				aliasing tile  = chunk->tiles[state->monstar.coords.y - chunk->coords.y][state->monstar.coords.x - chunk->coords.x];
				tile.entity = {};
				if (tile.pressure_plate)
				{
					ASSERT(tile.pressure_plate->pressed);
					tile.pressure_plate->pressed = false;
				}
			}
		}
	}

	//
	// Update camera.
	//

	state->camera_coords += HJKL_PRESSES();

	//
	// Render.
	//

	constexpr f32 PIXELS_PER_METER = 64.0f;
	constexpr f32 PIXELS_PER_Z     = 32.0f;

	lambda screen_coords_of =
		[&](vi2 coords, vf3 rel_pos)
		{
			return vxx((coords - state->camera_coords + rel_pos.xy) * PIXELS_PER_METER + platform_framebuffer->dims / 2.0f + vf2 { 0.0f, rel_pos.z * PIXELS_PER_Z });
		};

	lambda draw_hp =
		[&](vi2 coords, i32 hp)
		{
			FOR_RANGE(i, hp)
			{
				constexpr i32 HP_DIM = 10;
				draw_rect
				(
					platform_framebuffer,
					screen_coords_of(coords, { 0.0f, 0.0f, 0.0f }) + vxx(vf2 { -HP_DIM * 2.0f * (static_cast<f32>(i) - static_cast<f32>(hp) / 2.0f + 0.5f), -25.0f }),
					vx2(HP_DIM),
					rgba_from(0.9f, 0.1f, 0.1f)
				);
			}
		};

	memset(platform_framebuffer->pixels, 32, static_cast<u64>(platform_framebuffer->dims.x * platform_framebuffer->dims.y) * sizeof(u32));

	//
	// Render trees.
	//

	// @TODO@ Render what's visible.
	FOR_RANGE(chunk_iy, -4, 4)
	{
		FOR_RANGE(chunk_ix, -4, 4)
		{
			Chunk* chunk = get_chunk(state, { chunk_ix * CHUNK_DIM, chunk_iy * CHUNK_DIM });
			FOR_ELEMS(it, chunk->tree_buffer, chunk->tree_count)
			{
				draw_rect(platform_framebuffer, screen_coords_of(it->coords, { 0.0f, 0.0f, 0.0f }), vxx(vf2 { 1.0f, 1.0f } * PIXELS_PER_METER), rgba_from(0.1f, 0.3f, 0.1f));
				draw_bmp(platform_framebuffer, &state->bmp.trees[it->bmp_index], screen_coords_of(it->coords, { 0.0f, 0.0f, 0.0f }) - vxx(state->bmp.trees[it->bmp_index].dims * vf2 { 0.0f, -0.175f }));
			}
		}
	}

	//
	// Render pressure plate.
	//

	draw_rect(platform_framebuffer, screen_coords_of(state->pressure_plate.coords, { 0.0f, 0.0f, 0.0f }), vxx(vf2 { 1.0f, 1.0f } * PIXELS_PER_METER), rgba_from(0.25f, 0.25f, 0.25f));
	draw_bmp(platform_framebuffer, &state->bmp.pressure_plate, screen_coords_of(state->pressure_plate.coords, { 0.0f, 0.0f, 0.0f }), state->pressure_plate.pressed ? 1.0f : 0.5f);

	//
	// Render hero.
	//

	draw_rect(platform_framebuffer, screen_coords_of(state->hero.coords, { 0.0f, 0.0f, 0.0f }), vxx(vf2 { 1.0f, 1.0f } * PIXELS_PER_METER), rgba_from(0.1f, 0.2f, 0.3f));
	draw_bmp(platform_framebuffer, &state->bmp.hero_shadow                      , screen_coords_of(state->hero.coords, vxn(state->hero.rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow                      .dims * vf2 { 0.0f, -0.3f }));
	draw_bmp(platform_framebuffer, &state->bmp.hero_torsos[state->hero.cardinal], screen_coords_of(state->hero.coords,     state->hero.rel_pos          ) - vxx(state->bmp.hero_torsos[state->hero.cardinal].dims * vf2 { 0.0f, -0.3f }));
	draw_bmp(platform_framebuffer, &state->bmp.hero_capes [state->hero.cardinal], screen_coords_of(state->hero.coords,     state->hero.rel_pos          ) - vxx(state->bmp.hero_capes [state->hero.cardinal].dims * vf2 { 0.0f, -0.3f }));
	draw_bmp(platform_framebuffer, &state->bmp.hero_heads [state->hero.cardinal], screen_coords_of(state->hero.coords,     state->hero.rel_pos          ) - vxx(state->bmp.hero_heads [state->hero.cardinal].dims * vf2 { 0.0f, -0.3f }));
	draw_hp(state->hero.coords, state->hero.hp);

	//
	// Render pet.
	//

	draw_rect(platform_framebuffer, screen_coords_of(state->pet.coords, { 0.0f, 0.0f, 0.0f }), vxx(vf2 { 1.0f, 1.0f } * PIXELS_PER_METER), rgba_from(0.1f, 0.3f, 0.3f));
	draw_bmp(platform_framebuffer, &state->bmp.hero_shadow                     , screen_coords_of(state->pet.coords, vxn(state->pet.rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow                     .dims * vf2 { 0.0f, -0.300f }));
	draw_bmp(platform_framebuffer, &state->bmp.hero_heads [state->pet.cardinal], screen_coords_of(state->pet.coords,     state->pet.rel_pos          ) - vxx(state->bmp.hero_heads [state->pet.cardinal].dims * vf2 { 0.0f,  0.025f}));

	//
	// Render monstar.
	//

	if (state->monstar.existence_t != 0.0f)
	{
		draw_rect(platform_framebuffer, screen_coords_of(state->monstar.coords, { 0.0f, 0.0f, 0.0f }), vxx(vf2 { 1.0f, 1.0f } * PIXELS_PER_METER), rgba_from(0.3f, 0.1f, 0.1f));
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow                         , screen_coords_of(state->monstar.coords, vxn(state->monstar.rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow                          .dims * vf2 { 0.0f, -0.3f }));
		draw_bmp(platform_framebuffer, &state->bmp.hero_torsos[state->monstar.cardinal], screen_coords_of(state->monstar.coords,     state->monstar.rel_pos          ) - vxx(state->bmp.hero_torsos [state->monstar.cardinal].dims * vf2 { 0.0f, -0.3f }));
		if (+(state->monstar.flag & MonstarFlag::attractive))
		{
			draw_bmp(platform_framebuffer, &state->bmp.hero_capes[state->monstar.cardinal], screen_coords_of(state->monstar.coords, state->monstar.rel_pos) - vxx(state->bmp.hero_capes [state->monstar.cardinal].dims * vf2 { 0.0f, -0.3f }));
		}
		draw_hp(state->monstar.coords, state->monstar.hp);
	}

	return PlatformUpdateExitCode::normal;
}

PlatformSound_t(PlatformSound)
{
}
