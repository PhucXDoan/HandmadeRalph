#include "unified.h"
#include "platform.h"
#include "rng.cpp"

#define DEBUG_AUDIO 0

// @TODO@ Handle world chunk edges.

constexpr f32 PIXELS_PER_METER = 50.0f;
constexpr f32 METERS_PER_CHUNK = 16.0f;
constexpr f32 GRAVITY          = -9.80665f;

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

enum struct CollisionResultType : u8
{
	none,
	collided,
	embedded
};

struct CollisionResult
{
	CollisionResultType type;
	vf3                 displacement_delta;
	vf3                 normal;
	f32                 priority;
};

procedure bool32 prioritize_collision(CollisionResult* a, const CollisionResult& b)
{
	if (a->priority < b.priority)
	{
		*a = b;
		return true;
	}
	else
	{
		return false;
	}
}

procedure CollisionResult collide_side(vf3 displacement, vf3 center, vf3 normal)
{
	constexpr f32 EFFECTIVE_EMBEDDED_EPSILON     = 0.00001f;
	constexpr f32 EFFECTIVE_INTERSECTION_EPSILON = 0.00001f;

	f32 insideness = dot(center, normal);
	if (insideness > EFFECTIVE_EMBEDDED_EPSILON)
	{
		return
			{
				.type               = CollisionResultType::embedded,
				.displacement_delta = normal * insideness - displacement,
				.normal             = normal,
				.priority           = norm_sq(displacement) + insideness
			};
	}

	f32 parallelness = dot(displacement, normal);
	if (parallelness < -EFFECTIVE_INTERSECTION_EPSILON)
	{
		f32 scalar = insideness / parallelness;
		if (scalar < 1.0f)
		{
			return
				{
					.type               = CollisionResultType::collided,
					.displacement_delta = displacement * (scalar - 1.0f ),
					.normal             = normal,
					.priority           = norm_sq(displacement) * (1.0f - scalar)
				};
		}
	}

	return {};
}

procedure CollisionResult collide_plane(vf3 displacement, vf3 center, vf3 normal, f32 thickness)
{
	vf3 plane_normal = normal * (dot(center, normal) > 0.0f ? -1.0f : 1.0f);
	return collide_side(displacement, center + plane_normal * thickness / 2.0f, plane_normal);
}

procedure CollisionResult collide_rect(vf3 displacement, vf3 center, vf3 dims)
{
	CollisionResult left_right = collide_plane(displacement, center, { 1.0f, 0.0f, 0.0f }, dims.x);
	if
	(
		left_right.type != CollisionResultType::none &&
		(
			displacement.y + left_right.displacement_delta.y <= center.y - dims.y / 2.0f ||
			displacement.y + left_right.displacement_delta.y >= center.y + dims.y / 2.0f ||
			displacement.z + left_right.displacement_delta.z <= center.z - dims.z / 2.0f ||
			displacement.z + left_right.displacement_delta.z >= center.z + dims.z / 2.0f
		)
	)
	{
		left_right = {};
	}

	CollisionResult bottom_top = collide_plane(displacement, center, { 0.0f, 1.0f, 0.0f }, dims.y);
	if
	(
		bottom_top.type != CollisionResultType::none &&
		(
			displacement.x + bottom_top.displacement_delta.x <= center.x - dims.x / 2.0f ||
			displacement.x + bottom_top.displacement_delta.x >= center.x + dims.x / 2.0f ||
			displacement.z + bottom_top.displacement_delta.z <= center.z - dims.z / 2.0f ||
			displacement.z + bottom_top.displacement_delta.z >= center.z + dims.z / 2.0f
		)
	)
	{
		bottom_top = {};
	}

	CollisionResult back_front = collide_plane(displacement, center, { 0.0f, 0.0f, 1.0f }, dims.z);
	if
	(
		back_front.type != CollisionResultType::none &&
		(
			displacement.x + back_front.displacement_delta.x <= center.x - dims.x / 2.0f ||
			displacement.x + back_front.displacement_delta.x >= center.x + dims.x / 2.0f ||
			displacement.y + back_front.displacement_delta.y <= center.y - dims.y / 2.0f ||
			displacement.y + back_front.displacement_delta.y >= center.y + dims.y / 2.0f
		)
	)
	{
		back_front = {};
	}

	return
		left_right.type == CollisionResultType::embedded || bottom_top.type == CollisionResultType::embedded || back_front.type == CollisionResultType::embedded
			?
				left_right.priority < bottom_top.priority && left_right.priority < back_front.priority ? left_right :
				bottom_top.priority < left_right.priority && bottom_top.priority < back_front.priority ? bottom_top : back_front
			:
				left_right.priority > bottom_top.priority && left_right.priority > back_front.priority ? left_right :
				bottom_top.priority > left_right.priority && bottom_top.priority > back_front.priority ? bottom_top : back_front;
}

procedure CollisionResult collide_rect_against_side(vf3 displacement, vf3 dims, vf3 plane_center, vf3 normal)
{
	return
		collide_side
		(
			displacement,
			plane_center + (fabsf(dims.x * normal.x) + fabsf(dims.y * normal.y) + fabsf(dims.z * normal.z)) / 2.0f * normal,
			normal
		);
}

enum struct MonstarFlag : u8
{
	fast       = 1 << 0,
	flying     = 1 << 1,
	strong     = 1 << 2,
	attractive = 1 << 3,
};
#include "META/flag/MonstarFlag.h"

struct Chunk;

struct Tree
{
	vf2 rel_pos;
	vf3 hitbox;
	vf3 hitbox_offset;
	i32 bmp_index;
};

struct Hero
{
	Chunk*   chunk;
	vf3      rel_pos;
	vf3      vel;
	f32      level_z;
	vf3      hitbox;
	vf3      hitbox_offset;
	Cardinal cardinal;
	i32      hp;
	f32      rock_throw_t;
	f32      hit_t;
};

struct Pet
{
	Chunk*   chunk;
	vf3      rel_pos;
	vf3      vel;
	vf3      hitbox;
	Cardinal cardinal;
	i32      hp;
	f32      hover_t;
};

struct Monstar
{
	Chunk*      chunk;
	vf3         rel_pos;
	vf3         vel;
	vf3         hitbox;
	vf3         hitbox_offset;
	Cardinal    cardinal;
	i32         hp;
	f32         hit_t;
	f32         existence_t;
	MonstarFlag flag;
};

struct Rock
{
	Chunk* chunk;
	vf3    rel_pos;
	vf3    vel;
	vf3    hitbox;
	vf3    hitbox_offset;
	i32    bmp_index;
	f32    existence_t;
};

struct PressurePlate
{
	Chunk* chunk;
	vf2    rel_pos;
	vf3    vel;
	vf3    hitbox;
	bool32 down;
};

struct Stair
{
	Chunk* chunk;
	vf2    rel_pos;
	vf2    dims;
	f32    rail_width;
	f32    height;
};

#include "META/kind/Entity.h" // @META@ Tree, Hero, Pet, Monstar, Rock, PressurePlate, Stair

struct Chunk
{
	bool32 exists;
	vi2    coords;
	i32    tree_count;
	Tree   tree_buffer[128];
};

struct ChunkNode
{
	ChunkNode* next;
	Chunk      chunk;
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
			BMP hero_heads [ARRAY_CAPACITY(META_Cardinal)]; // @META@ hero_left_head.bmp , hero_right_head.bmp , hero_front_head.bmp , hero_back_head.bmp
			BMP hero_capes [ARRAY_CAPACITY(META_Cardinal)]; // @META@ hero_left_cape.bmp , hero_right_cape.bmp , hero_front_cape.bmp , hero_back_cape.bmp
			BMP hero_torsos[ARRAY_CAPACITY(META_Cardinal)]; // @META@ hero_left_torso.bmp, hero_right_torso.bmp, hero_front_torso.bmp, hero_back_torso.bmp
			BMP hero_shadow;                                // @META@ hero_shadow.bmp
			BMP background;                                 // @META@ background.bmp
			BMP rocks      [4];                             // @META@ rock00.bmp         , rock01.bmp          , rock02.bmp          , rock03.bmp
			BMP trees      [3];                             // @META@ tree00.bmp         , tree01.bmp          , tree02.bmp
			BMP pressure_plate;                             // @META@ grass01.bmp
		}   bmp;
		BMP bmps[sizeof(bmp) / sizeof(BMP)];
	};
	#include "META/asset/bmp_file_paths.h"

	ChunkNode*          available_chunk_node;
	ChunkNode           chunk_node_hash_table[64];

	Hero          hero;
	Pet           pet;
	Monstar       monstar;
	Rock          rock_buffer[16];
	i32           rock_count;
	PressurePlate pressure_plate;

	vi2           camera_coords;
	vf2           camera_rel_pos;
	vf2           camera_vel;

	#if DEBUG_AUDIO
	f32    hertz;
	f32    t;
	#endif
};
static_assert(sizeof(State) < PLATFORM_MEMORY_SIZE / 4);

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

procedure Chunk* get_chunk(State* state, vi2 coords)
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
				if (!chunk_node->next)
				{
					if (state->available_chunk_node)
					{
						chunk_node->next            = state->available_chunk_node;
						state->available_chunk_node = state->available_chunk_node->next;
					}
					else
					{
						chunk_node->next = allocate<ChunkNode>(&state->arena);
					}
					chunk_node->next->chunk.exists = false;
				}

				chunk_node = chunk_node->next;
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

procedure void process_move(EntityRef entity, State* state, f32 delta_time)
{
	Chunk**  chunk         = 0;
	vf3*     rel_pos       = 0;
	vf3*     vel           = 0;
	vf3      hitbox        = { 0.0f, 0.0f, 0.0f };
	vf3      hitbox_offset = { 0.0f, 0.0f, 0.0f };

	switch (entity.ref_type)
	{
		case EntityType::Hero:
		{
			chunk         = &entity.Hero_->chunk;
			rel_pos       = &entity.Hero_->rel_pos;
			vel           = &entity.Hero_->vel;
			hitbox        = entity.Hero_->hitbox;
			hitbox_offset = entity.Hero_->hitbox_offset;

			state->pressure_plate.down = false;
		} break;

		case EntityType::Pet:
		{
			chunk   = &entity.Pet_->chunk;
			rel_pos = &entity.Pet_->rel_pos;
			vel     = &entity.Pet_->vel;
			hitbox  = entity.Pet_->hitbox;
		} break;

		case EntityType::Monstar:
		{
			chunk         = &entity.Monstar_->chunk;
			rel_pos       = &entity.Monstar_->rel_pos;
			vel           = &entity.Monstar_->vel;
			hitbox        = entity.Monstar_->hitbox;
			hitbox_offset = entity.Monstar_->hitbox_offset;
		} break;

		case EntityType::Rock:
		{
			chunk         = &entity.Rock_->chunk;
			rel_pos       = &entity.Rock_->rel_pos;
			vel           = &entity.Rock_->vel;
			hitbox        = entity.Rock_->hitbox;
			hitbox_offset = entity.Rock_->hitbox_offset;
		} break;

		case EntityType::null          :
		case EntityType::Tree          :
		case EntityType::Stair         :
		case EntityType::PressurePlate : return;
	}

	f32 remaining_delta_time = delta_time;

	// @TODO@ Better performing collision.
	FOR_RANGE(4)
	{
		if (remaining_delta_time == 0.0f)
		{
			return;
		}

		enum struct CollisionObj
		{
			null,
			entity,
			ground
		};
		CollisionResult collision_result = {};
		CollisionObj    collision_obj    = {};
		EntityRef       collision_entity = {};

		//
		// Collide against trees.
		//

		// @TODO@ This checks chunks in a 3x3 adjacenct fashion. Could be better.
		FOR_RANGE(i, 9)
		{
			Chunk* tree_chunk = get_chunk(state, (*chunk)->coords + vi2 { i % 3 - 1, i / 3 - 1 });

			FOR_ELEMS(tree, tree_chunk->tree_buffer, tree_chunk->tree_count)
			{
				if
				(
					prioritize_collision
					(
						&collision_result,
						collide_rect
						(
							*vel * remaining_delta_time,
							vxn((tree_chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK, 0.0f) + (vxn(tree->rel_pos, 0.0f) - *rel_pos) + (tree->hitbox_offset - hitbox_offset),
							tree->hitbox + hitbox
						)
					)
				)
				{
					collision_obj    = CollisionObj::entity;
					collision_entity = ref(tree);
				}
			}
		}

		//
		// Collide against rocks.
		//

		if (entity.ref_type != EntityType::Hero)
		{
			FOR_ELEMS(rock, state->rock_buffer, state->rock_count)
			{
				Rock* entity_rock;
				if
				(
					IMPLIES(deref(&entity_rock, entity), entity_rock != rock) &&
					prioritize_collision
					(
						&collision_result,
						collide_rect
						(
							*vel * remaining_delta_time,
							vxn((rock->chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK, 0.0f) + (rock->rel_pos - *rel_pos) + (rock->hitbox_offset - hitbox_offset),
							rock->hitbox + hitbox
						)
					)
				)
				{
					collision_obj    = CollisionObj::entity;
					collision_entity = ref(rock);
				}
			}
		}

		//
		// Collide against hero.
		//

		if (entity.ref_type != EntityType::Hero && entity.ref_type != EntityType::Rock)
		{
			if
			(
				prioritize_collision
				(
					&collision_result,
					collide_rect
					(
						*vel * remaining_delta_time,
						vxn((state->hero.chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK, 0.0f) + (state->hero.rel_pos - *rel_pos) + (state->hero.hitbox_offset - hitbox_offset),
						state->hero.hitbox + hitbox
					)
				)
			)
			{
				collision_obj    = CollisionObj::entity;
				collision_entity = ref(&state->hero);
			}
		}

		//
		// Collide against pet.
		//

		if (entity.ref_type != EntityType::Pet)
		{
			if
			(
				prioritize_collision
				(
					&collision_result,
					collide_rect
					(
						*vel * remaining_delta_time,
						vxn((state->pet.chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK, 0.0f) + (state->pet.rel_pos - *rel_pos) + (-hitbox_offset),
						state->pet.hitbox + hitbox
					)
				)
			)
			{
				collision_obj    = CollisionObj::entity;
				collision_entity = ref(&state->pet);
			}
		}

		//
		// Collide against monstar.
		//

		if
		(
			state->monstar.existence_t > 0.0f
			&& entity.ref_type != EntityType::Monstar
			&& prioritize_collision
				(
					&collision_result,
					collide_rect
					(
						*vel * remaining_delta_time,
						vxn((state->monstar.chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK, 0.0f) + (state->monstar.rel_pos - *rel_pos) + (state->monstar.hitbox_offset - hitbox_offset),
						state->monstar.hitbox + hitbox
					)
				)
		)
		{
			collision_obj    = CollisionObj::entity;
			collision_entity = ref(&state->monstar);
		}

		//
		// Collide against ground.
		//

		if
		(
			prioritize_collision
			(
				&collision_result,
				collide_rect_against_side
				(
					*vel * remaining_delta_time,
					hitbox,
					vf3 { 0.0f, 0.0f, -rel_pos->z - hitbox_offset.z },
					normalize(vf3 { 0.0f, 0.0f, 1.0f })
				)
			)
		)
		{
			collision_obj    = CollisionObj::ground;
			collision_entity = {};
		}

		//
		// Processing.
		//

		if (collision_result.type == CollisionResultType::embedded)
		{
			DEBUG_printf("EMBEDDED\n");
			*rel_pos += *vel * remaining_delta_time + collision_result.displacement_delta;
		}
		else
		{
			f32 furthest_delta_time =
				collision_result.type == CollisionResultType::collided
					? remaining_delta_time - norm(collision_result.displacement_delta) / norm(*vel)
					: remaining_delta_time;

			remaining_delta_time -= furthest_delta_time;

			{
				Hero* hero;
				if (deref(&hero, entity))
				{
					state->pressure_plate.down =
						state->pressure_plate.down ||
						collide_rect
						(
							hero->vel * remaining_delta_time,
							vxn((hero->chunk->coords - state->pressure_plate.chunk->coords) * METERS_PER_CHUNK, 0.0f) + (hero->rel_pos - vxn(state->pressure_plate.rel_pos, 0.0f)) + (hero->hitbox_offset),
							hero->hitbox + state->pressure_plate.hitbox
						).type != CollisionResultType::none;
				}
			}
			{
				Monstar* monstar;
				Rock*    rock;
				if
				(
					((deref(&monstar, entity) && deref(&rock, collision_entity)) || (deref(&monstar, collision_entity) && deref(&rock, entity)))
					&& monstar->hp && monstar->hit_t == 0.0f
					&& dot(monstar->vel, rock->vel) < -2.0f
				)
				{
					monstar->hit_t = 1.0f;
					monstar->hp    = max(monstar->hp - 1, 0);
				}
			}

			*rel_pos += *vel * furthest_delta_time;

			if (fabsf(dot(*vel, collision_result.normal)) <= 1.0f)
			{
				*vel -= 1.0f * dot(*vel, collision_result.normal) * collision_result.normal;
				if (entity.ref_type == EntityType::Rock && collision_obj == CollisionObj::ground)
				{
					*vel = dampen(*vel, { 0.0f, 0.0f, 0.0f }, 0.01f, remaining_delta_time);
				}
			}
			else
			{
				*vel -= 1.25f * dot(*vel, collision_result.normal) * collision_result.normal;
			}

			{
				Monstar* monstar;
				Hero*    hero;
				if
				(
					((deref(&monstar, entity) && deref(&hero, collision_entity)) || (deref(&monstar, collision_entity) && deref(&hero, entity)))
					&& monstar->hp
					&& hero->hit_t == 0.0f
				)
				{
					hero->hit_t = 1.0f;
					hero->hp    = max(hero->hp - 1, 0);

					vf2 direction_to_monstar = (monstar->chunk->coords - hero->chunk->coords) * METERS_PER_CHUNK + monstar->rel_pos.xy - hero->rel_pos.xy;
					f32 distance_to_monstar  = norm(direction_to_monstar);
					if (distance_to_monstar > 0.001f)
					{
						direction_to_monstar /= distance_to_monstar;

						if (+(monstar->flag & MonstarFlag::strong))
						{
							hero->vel.xy    -= direction_to_monstar * 8.0f;
							hero->vel.z     += 6.0f;
							monstar->vel.xy += direction_to_monstar * 10.0f;
							monstar->vel.z  += 10.0f;
						}
						else
						{
							hero->vel.xy    -= direction_to_monstar * 4.0f;
							hero->vel.z     += 3.0f;
							monstar->vel.xy += direction_to_monstar * 8.0f;
							monstar->vel.z  += 8.0f;
						}
					}
				}
			}
		}

		{
			vi2 delta_coords = { 0, 0 };
			if      (rel_pos->x <              0.0f) { delta_coords.x -= 1; }
			else if (rel_pos->x >= METERS_PER_CHUNK) { delta_coords.x += 1; }
			if      (rel_pos->y <              0.0f) { delta_coords.y -= 1; }
			else if (rel_pos->y >= METERS_PER_CHUNK) { delta_coords.y += 1; }
			ASSERT((*chunk)->exists);
			*chunk       = get_chunk(state, (*chunk)->coords + delta_coords);
			rel_pos->xy -= delta_coords * METERS_PER_CHUNK;
		}
	}

	DEBUG_printf("ITERATION MAXED\n");
}

procedure u32 rgba_from(vf3 rgb)
{
	return
		((static_cast<u32>(rgb.x * 255.0f) << 16)) |
		((static_cast<u32>(rgb.y * 255.0f) <<  8)) |
		((static_cast<u32>(rgb.z * 255.0f) <<  0));
}

procedure u32 rgba_from(f32 r, f32 g, f32 b)
{
	return rgba_from({ r, g, b, });
}

extern PlatformUpdate_t(PlatformUpdate)
{
	State* state = reinterpret_cast<State*>(platform_memory);

	if (!state->is_initialized)
	{
		state->is_initialized = true;

		state->arena =
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

		vi2 coords = { 0, 0 };
		FOR_RANGE(8)
		{
			Chunk* chunk = get_chunk(state, coords);

			FOR_RANGE(16)
			{
				ASSERT(IN_RANGE(static_cast<u32>(chunk->tree_count), 0, ARRAY_CAPACITY(chunk->tree_buffer)));
				chunk->tree_buffer[chunk->tree_count] =
					{
						.rel_pos       = vxx(rng(&state->seed, 0, static_cast<i32>(METERS_PER_CHUNK)), rng(&state->seed, 0, static_cast<i32>(METERS_PER_CHUNK - 1))),
						.hitbox        = { 1.0f, 1.0f, 3.0f  },
						.hitbox_offset = { 0.0f, 0.0f, 1.5f },
						.bmp_index     = rng(&state->seed, 0, ARRAY_CAPACITY(state->bmp.trees))
					};
				chunk->tree_count += 1;
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

		state->camera_rel_pos = { 8.0f, 8.0f };

		state->hero =
			{
				.chunk         = get_chunk(state, { 0, 0 }),
				.rel_pos       = { 5.0f, 7.0f, 4.0f },
				.hitbox        = { 1.0f, 1.0f, 5.0f },
				.hitbox_offset = { 0.0f, 0.0f, 2.5f },
				.hp            = 4
			};

		state->pet =
			{
				.chunk         = get_chunk(state, { 0, 0 }),
				.rel_pos       = { 7.0f, 9.0f, 0.0f },
				.hitbox        = { 1.0f, 1.0f, 1.0f },
				.hp            = 3
			};

		state->pressure_plate =
			{
				.chunk    = get_chunk(state, { 0, 0 }),
				.rel_pos  = { 5.0f, 5.0f },
				.hitbox   = { 2.25f, 1.5f, 0.1f },
			};
	}

	//
	// Update hero.
	//

	{
		vf2 target_hero_vel = vxx(WASD_DOWN());
		if (+target_hero_vel)
		{
			if      (target_hero_vel.x < 0.0f) { state->hero.cardinal = Cardinal_left;  }
			else if (target_hero_vel.x > 0.0f) { state->hero.cardinal = Cardinal_right; }
			else if (target_hero_vel.y < 0.0f) { state->hero.cardinal = Cardinal_down;  }
			else if (target_hero_vel.y > 0.0f) { state->hero.cardinal = Cardinal_up;    }

			target_hero_vel = normalize(target_hero_vel) * 1.78816f;

			if (BTN_DOWN(.shift))
			{
				target_hero_vel *= 2.0f;
			}
		}

		state->hero.vel.xy = dampen(state->hero.vel.xy, target_hero_vel, 0.001f, platform_delta_time);

		if (BTN_PRESSES(.space))
		{
			state->hero.vel.z += 4.0f;
		}

		state->hero.vel.z += GRAVITY * platform_delta_time;

		process_move(ref(&state->hero), state, platform_delta_time);

		#if 0
		DEBUG_printf("%f %f %f\n", PASS_V3(state->hero.vel));
		#elif 0
		DEBUG_printf
		(
			"%f\n",
			static_cast<f64>(norm(state->hero.vel))
		);
		#endif
	}

	//
	// Update pet.
	//

	state->pet.hover_t = mod(state->pet.hover_t + platform_delta_time / 4.0f, 1.0f);

	{
		vf2 target_pet_vel   = { 0.0f, 0.0f };
		vf2 ray_to_hero      = (state->hero.chunk->coords - state->pet.chunk->coords) * METERS_PER_CHUNK + state->hero.rel_pos.xy - state->pet.rel_pos.xy;
		f32 distance_to_hero = norm(ray_to_hero);
		if (IN_RANGE(distance_to_hero, 1.0f, 7.0f))
		{
			ray_to_hero /= distance_to_hero;

			if (distance_to_hero > 3.0f)
			{
				target_pet_vel = ray_to_hero * 2.0f;
			}

			if      (ray_to_hero.x < -1.0f / SQRT2) { state->pet.cardinal = Cardinal_left;  }
			else if (ray_to_hero.x >  1.0f / SQRT2) { state->pet.cardinal = Cardinal_right; }
			else if (ray_to_hero.y < -1.0f / SQRT2) { state->pet.cardinal = Cardinal_down;  }
			else if (ray_to_hero.y >  1.0f / SQRT2) { state->pet.cardinal = Cardinal_up;    }
		}

		state->pet.vel.xy  = dampen(state->pet.vel.xy, target_pet_vel, 0.1f, platform_delta_time);
		state->pet.vel.z  += GRAVITY * platform_delta_time;

		process_move(ref(&state->pet), state, platform_delta_time);
	}

	//
	// Update monstar.
	//

	if (state->monstar.existence_t > 0.0f)
	{
		if (state->monstar.hp)
		{
			vf2 target_monstar_vel = { 0.0f, 0.0f };
			vf2 ray_to_hero        = (state->hero.chunk->coords - state->monstar.chunk->coords) * METERS_PER_CHUNK + state->hero.rel_pos.xy - state->monstar.rel_pos.xy;
			f32 distance_to_hero   = norm(ray_to_hero);
			if (IN_RANGE(distance_to_hero, 0.1f, 10.0f))
			{
				target_monstar_vel = ray_to_hero / distance_to_hero * 2.0f;

				if (+(state->monstar.flag & MonstarFlag::fast))
				{
					target_monstar_vel *= 2.0f;
				}
			}

			if (+(state->monstar.flag & MonstarFlag::attractive))
			{
				target_monstar_vel *= 6.0f;
				state->monstar.vel.xy = dampen(state->monstar.vel.xy, target_monstar_vel, 0.8f, platform_delta_time);
			}
			else
			{
				state->monstar.vel.xy = dampen(state->monstar.vel.xy, target_monstar_vel, 0.4f, platform_delta_time);
			}

			if (+(state->monstar.flag & MonstarFlag::flying))
			{
				state->monstar.vel.z = dampen(state->monstar.vel.z, (2.0f - state->monstar.rel_pos.z) * 8.0f, 0.01f, platform_delta_time);
			}
		}
		else
		{
			state->monstar.vel.xy = dampen(state->monstar.vel.xy, { 0.0f, 0.0f }, 0.2f, platform_delta_time);
			state->monstar.vel.z  = dampen(state->monstar.vel.z, (1.0f - state->monstar.rel_pos.z) * 4.0f, 0.01f, platform_delta_time);
		}

		if (state->monstar.hp)
		{
			f32 monstar_speed = norm(state->monstar.vel.xy);
			if (monstar_speed > 0.01f)
			{
				vf2 direction = state->monstar.vel.xy / monstar_speed;
				if      (direction.x < -1.0f / SQRT2) { state->monstar.cardinal = Cardinal_left;  }
				else if (direction.x >  1.0f / SQRT2) { state->monstar.cardinal = Cardinal_right; }
				else if (direction.y < -1.0f / SQRT2) { state->monstar.cardinal = Cardinal_down;  }
				else if (direction.y >  1.0f / SQRT2) { state->monstar.cardinal = Cardinal_up;    }
			}
		}
		else
		{
			state->monstar.existence_t = max(state->monstar.existence_t - platform_delta_time / 1.0f, 0.0f);
		}

		state->monstar.vel.z += GRAVITY * platform_delta_time;

		process_move(ref(&state->monstar), state, platform_delta_time);
	}

	if (state->pressure_plate.down && state->monstar.existence_t == 0.0f)
	{
		state->monstar =
			{
				.chunk         = get_chunk(state, { 0, 0 }),
				.rel_pos       = { 9.0f, 4.0f, 1.0f },
				.hitbox        = { 1.0f, 1.0f, 1.0f },
				.hitbox_offset = { 0.0f, 0.0f, 0.5f },
				.hp            = 2,
				.existence_t   = 1.0f
			};

		FOR_ELEMS(META_MonstarFlag)
		{
			if (rng(&state->seed) < 0.5f)
			{
				state->monstar.flag |= it->flag;
			}
		}
	}

	//
	// Update rocks.
	//

	{
		state->hero.rock_throw_t = max(state->hero.rock_throw_t - platform_delta_time / 1.0f, 0.0f);


		if (BTN_PRESSES(.arrow_right) && state->hero.rock_throw_t <= 0.0f)
		{
			state->hero.rock_throw_t = 1.0f;

			if (IN_RANGE(static_cast<u32>(state->rock_count), 0, ARRAY_CAPACITY(state->rock_buffer)))
			{
				state->rock_buffer[state->rock_count] =
					{
						.chunk         = state->hero.chunk,
						.rel_pos       = state->hero.rel_pos + vf3 { 0.0f, 0.0f, 1.0f },
						.vel           = vxn(state->hero.vel.xy + META_Cardinal[state->hero.cardinal].vf * 2.0f, 1.0f),
						.hitbox        = { 1.0f, 1.0f, 1.0f },
						.hitbox_offset = { 0.0f, 0.0f, 0.5f },
						.bmp_index     = rng(&state->seed, 0, ARRAY_CAPACITY(state->bmp.rocks)),
						.existence_t   = 1.0f
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
			rock->existence_t     = clamp(rock->existence_t - platform_delta_time / DURATION, 0.0f, 1.0f);
			rock->vel.z          += GRAVITY * platform_delta_time;
			process_move(ref(rock), state, (old_existence_t - rock->existence_t) * DURATION);
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

	//
	// Render.
	//

	memset(platform_framebuffer->pixels, 0, static_cast<u64>(platform_framebuffer->dims.x * platform_framebuffer->dims.y) * sizeof(u32));

	draw_bmp(platform_framebuffer, &state->bmp.background, platform_framebuffer->dims / 2);

	lambda screen_coords_of =
		[&](vi2 chunk_coords, vf3 rel_pos)
		{
			return vxx(((chunk_coords - state->camera_coords) * METERS_PER_CHUNK + rel_pos.xy - state->camera_rel_pos + vf2 { 0.0f, 0.5f } * rel_pos.z) * PIXELS_PER_METER) + platform_framebuffer->dims / 2;
		};

	lambda draw_hp =
		[&](vi2 chunk_coords, vf3 rel_pos, i32 hp)
		{
			rel_pos = vxn(rel_pos.xy, 0.0f);
			FOR_RANGE(i, hp)
			{
				constexpr i32 HP_DIM = 10;
				draw_rect
				(
					platform_framebuffer,
					screen_coords_of(chunk_coords, rel_pos) + vxx(vf2 { -HP_DIM * 2.0f * (static_cast<f32>(i) - static_cast<f32>(hp) / 2.0f + 0.5f), -10.0f }),
					vx2(HP_DIM),
					rgba_from(0.9f, 0.1f, 0.1f)
				);
			}
		};

	lambda draw_hitbox =
		[&](vi2 chunk_coords, vf3 rel_pos, vf3 hitbox, vf3 hitbox_offset, vf3 color)
		{
			draw_rect(platform_framebuffer, screen_coords_of(chunk_coords,     rel_pos    + hitbox_offset + vf3 { 0.0f, 0.0f, -hitbox.z / 2.0f }), vxx(hitbox.xy * PIXELS_PER_METER), vxn(color / 2.0f, 0.75f));
			draw_rect(platform_framebuffer, screen_coords_of(chunk_coords, vxn(rel_pos.xy + hitbox_offset.xy, 0.0f)                             ), vxx(hitbox.xy * PIXELS_PER_METER), vxn(color / 4.0f, 0.75f));
			draw_rect(platform_framebuffer, screen_coords_of(chunk_coords,     rel_pos    + hitbox_offset + vf3 { 0.0f, 0.0f,  hitbox.z / 2.0f }), vxx(hitbox.xy * PIXELS_PER_METER), vxn(color       , 0.75f));
		};

	//
	// Render trees.
	//

	FOR_ELEMS(chunk_node, state->chunk_node_hash_table) // @TODO@ This is going through the hash table to render. Bad!
	{
		if (chunk_node->chunk.exists)
		{
			FOR_ELEMS(tree, chunk_node->chunk.tree_buffer, chunk_node->chunk.tree_count)
			{
				draw_hitbox(chunk_node->chunk.coords, vxn(tree->rel_pos, 0.0f), tree->hitbox, tree->hitbox_offset, { 0.25f, 0.75f, 0.1f });
				draw_bmp(platform_framebuffer, &state->bmp.trees[tree->bmp_index], screen_coords_of(chunk_node->chunk.coords, vxn(tree->rel_pos, 0.0f)) - vxx(state->bmp.trees[tree->bmp_index].dims * vf2 { 0.0f, -0.175f }));
			}
		}
	}

	//
	// Render pressure plate.
	//

	{
		draw_hitbox(state->pressure_plate.chunk->coords, vxn(state->pressure_plate.rel_pos, 0.0f), state->pressure_plate.hitbox, { 0.0f, 0.0f, 0.0f }, { 0.5f, 0.5f, 0.5f });
		draw_bmp(platform_framebuffer, &state->bmp.pressure_plate, screen_coords_of(state->pressure_plate.chunk->coords, vxn(state->pressure_plate.rel_pos, 0.0f)), state->pressure_plate.down ? 1.0f : 0.5f);
	}

	//
	// Render hero.
	//

	{
		draw_hitbox(state->hero.chunk->coords, state->hero.rel_pos, state->hero.hitbox, state->hero.hitbox_offset, { 1.0f, 0.6f, 0.2f });
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow                      , screen_coords_of(state->hero.chunk->coords, vxn(state->hero.rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow                      .dims * vf2 { 0.0f, -0.3f }));
		draw_bmp(platform_framebuffer, &state->bmp.hero_torsos[state->hero.cardinal], screen_coords_of(state->hero.chunk->coords,     state->hero.rel_pos          ) - vxx(state->bmp.hero_torsos[state->hero.cardinal].dims * vf2 { 0.0f, -0.3f }));
		draw_bmp(platform_framebuffer, &state->bmp.hero_capes [state->hero.cardinal], screen_coords_of(state->hero.chunk->coords,     state->hero.rel_pos          ) - vxx(state->bmp.hero_capes [state->hero.cardinal].dims * vf2 { 0.0f, -0.3f }));
		draw_bmp(platform_framebuffer, &state->bmp.hero_heads [state->hero.cardinal], screen_coords_of(state->hero.chunk->coords,     state->hero.rel_pos          ) - vxx(state->bmp.hero_heads [state->hero.cardinal].dims * vf2 { 0.0f, -0.3f }));
		draw_hp(state->hero.chunk->coords, state->hero.rel_pos, state->hero.hp);
	}

	//
	// Render pet.
	//

	{
		draw_hitbox(state->pet.chunk->coords, state->pet.rel_pos, state->pet.hitbox, { 0.0f, 0.0f, 0.0f }, { 0.25f, 0.25f, 1.0f });
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow                     , screen_coords_of(state->pet.chunk->coords, vxn(state->pet.rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow                     .dims * vf2 { 0.0f, -0.3f }));
		draw_bmp(platform_framebuffer, &state->bmp.hero_heads [state->pet.cardinal], screen_coords_of(state->pet.chunk->coords,     state->pet.rel_pos          ) - vxx(state->bmp.hero_heads [state->pet.cardinal].dims * vf2 { 0.0f, -0.3f + sinf(state->pet.hover_t * TAU) * 0.1f }));
		draw_hp(state->pet.chunk->coords, state->pet.rel_pos, state->pet.hp);
	}

	//
	// Render monstar.
	//

	if (state->monstar.existence_t > 0.0f)
	{
		draw_hitbox(state->monstar.chunk->coords, state->monstar.rel_pos, state->monstar.hitbox, state->monstar.hitbox_offset, { 1.0f, 0.25f, 0.4f });
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow                         , screen_coords_of(state->monstar.chunk->coords, vxn(state->monstar.rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow                         .dims * vf2 { 0.0f, -0.3f }));
		draw_bmp(platform_framebuffer, &state->bmp.hero_torsos[state->monstar.cardinal], screen_coords_of(state->monstar.chunk->coords,     state->monstar.rel_pos          ) - vxx(state->bmp.hero_torsos[state->monstar.cardinal].dims * vf2 { 0.0f, -0.3f }));
		draw_hp(state->monstar.chunk->coords, state->monstar.rel_pos, state->monstar.hp);
	}

	//
	// Render rocks.
	//

	constexpr f32 CENTER_Y[sizeof(state->bmp.rocks)] = { -0.1f, -0.1f, -0.1f, 0.2f };
	FOR_ELEMS(rock, state->rock_buffer, state->rock_count)
	{
		draw_hitbox(rock->chunk->coords, rock->rel_pos, rock->hitbox, rock->hitbox_offset, { 0.25f, 0.25f, 0.2f });
		draw_bmp(platform_framebuffer, &state->bmp.hero_shadow           , screen_coords_of(rock->chunk->coords, vxn(rock->rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow           .dims * vf2 { 0.0f,                     -0.3f }));
		draw_bmp(platform_framebuffer, &state->bmp.rocks[rock->bmp_index], screen_coords_of(rock->chunk->coords,     rock->rel_pos          ) - vxx(state->bmp.rocks[rock->bmp_index].dims * vf2 { 0.0f, CENTER_Y[rock->bmp_index] }));
	}

	#if DEBUG_AUDIO
	state->hertz = 512.0f + platform_input->gamepads[0].stick_right.y * 100.0f;
	state->t     = mod(state->t, TAU);
	#endif

	state->hero.hit_t    = max(state->hero.hit_t    - platform_delta_time / 0.2f, 0.0f);
	state->monstar.hit_t = max(state->monstar.hit_t - platform_delta_time / 0.2f, 0.0f);

	return PlatformUpdateExitCode::normal;
}

extern PlatformSound_t(PlatformSound)
{
	#if DEBUG_AUDIO
	State* state = reinterpret_cast<State*>(platform_memory);

	FOR_ELEMS(sample, platform_sample_buffer, platform_sample_count)
	{
		*sample   = { static_cast<i16>(sinf(state->t) * 1250.0f), static_cast<i16>(sinf(state->t) * 1250.0f) };
		state->t += TAU * state->hertz / static_cast<f32>(platform_samples_per_second);
	}
	#endif
}
