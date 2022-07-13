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

struct Circle
{
	f32 radius;
};

struct RoundedRect
{
	vf2 dims;
	f32 radius;
};

#include "META/variant/Shape.h" // @META@ Circle, RoundedRect

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
	vf2         rel_pos;
	RoundedRect collider;
	i32         bmp_index;
};

struct Hero
{
	Chunk*   chunk;
	vf3      rel_pos;
	vf3      vel;
	Circle   collider;
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
	Circle   collider;
	Cardinal cardinal;
	i32      hp;
	f32      hover_t;
};

struct Monstar
{
	Chunk*      chunk;
	vf3         rel_pos;
	vf3         vel;
	Circle      collider;
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
	Circle collider;
	i32    bmp_index;
	f32    existence_t;
};

struct PressurePlate
{
	Chunk* chunk;
	vf2    rel_pos;
	vf3    vel;
	Circle collider;
	bool32 down;
};

#include "META/variant/Entity.h" // @META@ Tree, Hero, Pet, Monstar, Rock, PressurePlate

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

template <typename TYPE>
procedure TYPE eat(PlatformFileData* file_data)
{
	ASSERT(file_data->read_index + sizeof(TYPE) <= file_data->size);
	TYPE value;
	memcpy(&value, file_data->data + file_data->read_index, sizeof(TYPE));
	file_data->read_index += sizeof(TYPE);
	return value;
}

procedure void draw_rect(PlatformFramebuffer* platform_framebuffer, vi2 bottom_left, vi2 dims, u32 rgba)
{
	ASSERT(IN_RANGE(dims.x, 0, 1024));
	ASSERT(IN_RANGE(dims.y, 0, 1024));
	FOR_RANGE(y, max(platform_framebuffer->dims.y - bottom_left.y - dims.y, 0), min(platform_framebuffer->dims.y - bottom_left.y, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(bottom_left.x, 0), min(bottom_left.x + dims.x, platform_framebuffer->dims.x))
		{
			platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x] = rgba;
		}
	}
}

procedure void draw_bmp(PlatformFramebuffer* platform_framebuffer, BMP* bmp, vi2 bottom_left, f32 alpha = 1.0f)
{
	FOR_RANGE(y, max(platform_framebuffer->dims.y - bottom_left.y - bmp->dims.y, 0), min(platform_framebuffer->dims.y - bottom_left.y, platform_framebuffer->dims.y))
	{
		FOR_RANGE(x, max(bottom_left.x, 0), min(bottom_left.x + bmp->dims.x, platform_framebuffer->dims.x))
		{
			aliasing dst = platform_framebuffer->pixels[y * platform_framebuffer->dims.x + x];
			aliasing top = bmp->rgba[(platform_framebuffer->dims.y - bottom_left.y - 1 - y) * bmp->dims.x + x - bottom_left.x];

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

procedure const CollisionResult& prioritize_collision_results(const CollisionResult& a, const CollisionResult& b)
{
	return b.type == CollisionType::none || (a.type != CollisionType::none && a.priority > b.priority)
		? a
		: b;
}

procedure bool32 prioritize_collision_results(CollisionResult* a, const CollisionResult& b)
{
	if (b.type == CollisionType::none || (a->type != CollisionType::none && a->priority > b.priority))
	{
		return false;
	}
	else
	{
		*a = b;
		return true;
	}
}

procedure CollisionResult collide_against_plane(vf2 displacement, vf2 plane_center, vf2 plane_normal)
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

procedure CollisionResult collide_against_line(vf2 displacement, vf2 line_center, vf2 line_normal, f32 padding)
{
	vf2 plane_normal = line_normal * (dot(line_center, line_normal) > 0.0f ? -1.0f : 1.0f);
	return collide_against_plane(displacement, line_center + plane_normal * padding, plane_normal);
}

// @TODO@ Bugs out on `radius == 0.0f`
// @TODO@ Should detect collision still when the center is on the origin.
procedure CollisionResult collide_against_circle(vf2 displacement, vf2 center, f32 radius)
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

	if (distance_from_center > radius || (radius - distance_from_center < COLLISION_EPSILON && amount_away_from_center >= 0.0f))
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

procedure CollisionResult collide_against_rounded_rect(vf2 displacement, vf2 rect_bottom_left, vf2 rect_dims, f32 radius)
{
	CollisionResult left_right = collide_against_line(displacement, rect_bottom_left + vf2 { rect_dims.x / 2.0f, 0.0f }, { 1.0f, 0.0f }, rect_dims.x / 2.0f + radius);
	if (left_right.type != CollisionType::none && !IN_RANGE(left_right.new_displacement.y - rect_bottom_left.y, 0.0f, rect_dims.y))
	{
		left_right.type = CollisionType::none;
	}
	CollisionResult bottom_top = collide_against_line(displacement, rect_bottom_left + vf2 { 0.0f, rect_dims.y / 2.0f }, { 0.0f, 1.0f }, rect_dims.y / 2.0f + radius);
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
				prioritize_collision_results(collide_against_circle(displacement, rect_bottom_left                            , radius), collide_against_circle(displacement, rect_bottom_left + vf2 { rect_dims.x, 0.0f }, radius)),
				prioritize_collision_results(collide_against_circle(displacement, rect_bottom_left + vf2 { 0.0f, rect_dims.y }, radius), collide_against_circle(displacement, rect_bottom_left +       rect_dims          , radius))
			)
		);
}

procedure CollisionResult collide(vf2 displacement, ShapeRef a, vf2 pos, ShapeRef b)
{
	if (a.ref_type <= b.ref_type)
	{
		switch (a.ref_type)
		{
			case ShapeType::Circle: switch (b.ref_type)
			{
				case ShapeType::Circle: return
					collide_against_circle(displacement, pos, a.Circle_->radius + b.Circle_->radius);

				case ShapeType::RoundedRect: return
					collide_against_rounded_rect(displacement, pos, b.RoundedRect_->dims, b.RoundedRect_->radius + a.Circle_->radius);

				case ShapeType::null : ASSERT(false); return {};
			}

			case ShapeType::RoundedRect: switch (b.ref_type)
			{
				case ShapeType::RoundedRect: return
					collide_against_rounded_rect(displacement, pos, a.RoundedRect_->dims + b.RoundedRect_->dims, b.RoundedRect_->radius + b.RoundedRect_->radius);

				case ShapeType::Circle :
				case ShapeType::null   : ASSERT(false); return {};
			}

			case ShapeType::null : ASSERT(false); return {};
		}
	}
	else
	{
		CollisionResult result = collide(-displacement, b, -pos, a);
		return
			{
				.type             =  result.type,
				.new_displacement = -result.new_displacement,
				.normal           = -result.normal,
				.priority         =  result.priority
			};
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
	Chunk**  chunk    = 0;
	vf3*     rel_pos  = 0;
	vf3*     vel      = 0;
	ShapeRef collider = {};

	switch (entity.ref_type)
	{
		#define GRAB(TYPE)\
		case EntityType::TYPE:\
		{\
			chunk    = &entity.TYPE##_->chunk;\
			rel_pos  = &entity.TYPE##_->rel_pos;\
			vel      = &entity.TYPE##_->vel;\
			collider = ref(&entity.TYPE##_->collider);\
		} break\

		GRAB(Hero);
		GRAB(Pet);
		GRAB(Monstar);
		GRAB(Rock);
		#undef GRAB

		case EntityType::null          :
		case EntityType::Tree          :
		case EntityType::PressurePlate : return;
	}

	vel->z += GRAVITY * delta_time;

	vf3 displacement = *vel * delta_time;

	// @TODO@ Better performing collision.
	FOR_RANGE(8)
	{
		CollisionResult collision        = {};
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
					prioritize_collision_results
					(
						&collision,
						collide
						(
							displacement.xy,
							collider,
							(tree_chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + tree->rel_pos - rel_pos->xy,
							ref(&tree->collider)
						)
					)
				)
				{
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
					prioritize_collision_results
					(
						&collision,
						collide
						(
							displacement.xy,
							collider,
							(rock->chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + rock->rel_pos.xy - rel_pos->xy,
							ref(&rock->collider)
						)
					)
				)
				{
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
				prioritize_collision_results
				(
					&collision,
					collide
					(
						displacement.xy,
						collider,
						(state->hero.chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + state->hero.rel_pos.xy - rel_pos->xy,
						ref(&state->hero.collider)
					)
				)
			)
			{
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
				prioritize_collision_results
				(
					&collision,
					collide
					(
						displacement.xy,
						collider,
						(state->pet.chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + state->pet.rel_pos.xy - rel_pos->xy,
						ref(&state->pet.collider)
					)
				)
			)
			{
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
			&& prioritize_collision_results
				(
					&collision,
					collide
					(
						displacement.xy,
						collider,
						(state->monstar.chunk->coords - (*chunk)->coords) * METERS_PER_CHUNK + state->monstar.rel_pos.xy - rel_pos->xy,
						ref(&state->monstar.collider)
					)
				)
		)
		{
			collision_entity = ref(&state->monstar);
		}

		//
		// Processing.
		//

		state->pressure_plate.down =
			collide
			(
				collision.type == CollisionType::none
					? displacement.xy
					: collision.new_displacement,
				ref(&state->pressure_plate.collider),
				(state->hero.chunk->coords - state->pressure_plate.chunk->coords) * METERS_PER_CHUNK + state->hero.rel_pos.xy - state->pressure_plate.rel_pos,
				ref(&state->hero.collider)
			).type != CollisionType::none;

		if
		(
			state->monstar.hp
			&& state->hero.hit_t == 0.0f
			&& ((entity.ref_type == EntityType::Hero && collision_entity.ref_type == EntityType::Monstar) || (entity.ref_type == EntityType::Monstar && collision_entity.ref_type == EntityType::Hero))
		)
		{
			state->hero.hit_t = 1.0f;
			state->hero.hp    = max(state->hero.hp - 1, 0);

			vf2 direction_to_monstar = (state->monstar.chunk->coords - state->hero.chunk->coords) * METERS_PER_CHUNK + state->monstar.rel_pos.xy - state->hero.rel_pos.xy;
			f32 distance_to_monstar  = norm(direction_to_monstar);
			if (distance_to_monstar > 0.001f)
			{
				direction_to_monstar /= distance_to_monstar;

				if (+(state->monstar.flag & MonstarFlag::strong))
				{
					state->hero.vel.xy    -= direction_to_monstar * 8.0f;
					state->hero.vel.z     += 6.0f;
					state->monstar.vel.xy += direction_to_monstar * 10.0f;
					state->monstar.vel.z  += 10.0f;
				}
				else
				{
					state->hero.vel.xy    -= direction_to_monstar * 4.0f;
					state->hero.vel.z     += 3.0f;
					state->monstar.vel.xy += direction_to_monstar * 8.0f;
					state->monstar.vel.z  += 8.0f;
				}
			}
		}

		{
			Monstar* monstar;
			Rock*    rock;
			if
			(
				state->monstar.hp && state->monstar.hit_t == 0.0f
				&& ((deref(&monstar, entity) && deref(&rock, collision_entity)) || (deref(&monstar, collision_entity) && deref(&rock, entity)))
				&& dot(monstar->vel, rock->vel) < -2.0f
			)
			{
				state->monstar.hit_t = 1.0f;
				state->monstar.hp    = max(state->monstar.hp - 1, 0);
			}
		}

		rel_pos->xy +=
			collision.type == CollisionType::none
				? displacement.xy
				: collision.new_displacement;

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
			rel_pos->z = 0.0f;
			if (vel->z < 0.0f)
			{
				vel->z = vel->z * -0.35f;
			}

			if (entity.ref_type == EntityType::Rock)
			{
				vel->xy = dampen(vel->xy, { 0.0f, 0.0f }, 0.005f, delta_time);
			}
		}

		if (collision.type == CollisionType::none)
		{
			break;
		}
		else
		{
			if (entity.ref_type == EntityType::Rock)
			{
				vel->xy         = 0.15f * (dot(vel->xy         - collision.new_displacement, rotate90(collision.normal)) * rotate90(collision.normal) * 2.0f + (collision.new_displacement - vel->xy        ));
				displacement.xy = 0.15f * (dot(displacement.xy - collision.new_displacement, rotate90(collision.normal)) * rotate90(collision.normal) * 2.0f + (collision.new_displacement - displacement.xy));
			}
			else
			{
				vel->xy         = dot(vel->xy         - collision.new_displacement, rotate90(collision.normal)) * rotate90(collision.normal) + max(dot(vel->xy         - collision.new_displacement, collision.normal), 0.0f) * collision.normal;
				displacement.xy = dot(displacement.xy - collision.new_displacement, rotate90(collision.normal)) * rotate90(collision.normal) + max(dot(displacement.xy - collision.new_displacement, collision.normal), 0.0f) * collision.normal;
			}

			displacement.z  = 0.0f;
		}
	}
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

			BitmapHeader header = eat<BitmapHeader>(&file_data);

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
						.rel_pos  = vxx(rng(&state->seed, 0, static_cast<i32>(METERS_PER_CHUNK)), rng(&state->seed, 0, static_cast<i32>(METERS_PER_CHUNK - 1))),
						.collider =
							{
								.dims   = { 1.0f, 1.0f },
								.radius = 0.0f // @TODO@ Rect shape type.
							},
						.bmp_index = rng(&state->seed, 0, ARRAY_CAPACITY(state->bmp.trees))
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

		state->hero =
			{
				.chunk    = get_chunk(state, { 0, 0 }),
				.rel_pos  = { 5.0f, 7.0f, 0.0f },
				.collider = { .radius = 0.3f },
				.hp       = 4
			};


		state->pet =
			{
				.chunk    = get_chunk(state, { 0, 0 }),
				.rel_pos  = { 7.0f, 9.0f, 0.0f },
				.collider = { .radius = 0.3f },
				.hp = 3
			};


		state->pressure_plate =
			{
				.chunk    = get_chunk(state, { 0, 0 }),
				.rel_pos  = { 5.0f, 5.0f },
				.collider = { .radius = 1.0f }
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

		process_move(ref(&state->hero), state, platform_delta_time);
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

		state->pet.vel.xy = dampen(state->pet.vel.xy, target_pet_vel, 0.1f, platform_delta_time);
		state->pet.vel.z  = dampen(state->pet.vel.z, (2.0f + sinf(state->pet.hover_t * TAU) * 0.25f - state->pet.rel_pos.z) * 8.0f, 0.01f, platform_delta_time);

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
			else
			{
				state->monstar.vel.z = dampen(state->monstar.vel.z, (0.5f - state->monstar.rel_pos.z) * 8.0f, 0.01f, platform_delta_time);
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

		process_move(ref(&state->monstar), state, platform_delta_time);
	}

	if (state->pressure_plate.down && state->monstar.existence_t == 0.0f)
	{
		state->monstar =
			{
				.chunk       = get_chunk(state, { 0, 0 }),
				.rel_pos     = { 9.0f, 4.0f, 0.0f },
				.collider    = { .radius = 0.3f },
				.hp          = 2,
				.existence_t = 1.0f
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
		state->hero.rock_throw_t -= platform_delta_time / 1.0f;

		if (BTN_PRESSES(.space) && state->hero.rock_throw_t <= 0.0f)
		{
			state->hero.rock_throw_t = 1.0f;

			// @TODO@ Nice way to find out if a rock is on top of an entity.
			bool32 colliding = false;
			FOR_ELEMS(rock, state->rock_buffer, state->rock_count)
			{
				if (collide({ 0.0f, 0.0f }, ref(&rock->collider), (rock->chunk->coords - state->hero.chunk->coords) * METERS_PER_CHUNK + rock->rel_pos.xy - state->hero.rel_pos.xy, ref(&rock->collider)).type != CollisionType::none)
				{
					colliding = true;
					break;
				}
			}
			if (!colliding)
			{
				ASSERT(IN_RANGE(static_cast<u32>(state->rock_count), 0, ARRAY_CAPACITY(state->rock_buffer)));
				state->rock_buffer[state->rock_count] =
					{
						.chunk       = state->hero.chunk,
						.rel_pos     = state->hero.rel_pos + vf3 { 0.0f, 0.0f, 1.0f },
						.vel         = vxn(state->hero.vel.xy + META_Cardinal[state->hero.cardinal].vf * 2.0f, 1.0f),
						.collider    = { .radius = 0.6f },
						.bmp_index   = rng(&state->seed, 0, ARRAY_CAPACITY(state->bmp.rocks)),
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

	draw_bmp(platform_framebuffer, &state->bmp.background, { 0, 0 });

	lambda screen_coords_of =
		[&](vi2 chunk_coords, vf3 rel_pos)
		{
			return vxx(((chunk_coords - state->camera_coords) * METERS_PER_CHUNK + rel_pos.xy - state->camera_rel_pos + vf2 { 0.0f, 1.0f } * rel_pos.z) * PIXELS_PER_METER);
		};

	lambda draw_thing =
		[&](vi2 chunk_coords, vf3 rel_pos, vf2 center, BMP* bmp)
		{
			f32 alpha = 1.0f;
			if (bmp == &state->bmp.hero_shadow)
			{
				alpha     = square(clamp(1.0f - rel_pos.z / 12.0f, 0.0f, 1.0f));
				rel_pos.z = 0.0f;
			}
			else if (bmp == &state->bmp.pressure_plate)
			{
				alpha = state->pressure_plate.down ? 1.0f : 0.5f;
			}

			draw_bmp(platform_framebuffer, bmp, screen_coords_of(chunk_coords, rel_pos) - vxx(bmp->dims * center), alpha);
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
					screen_coords_of(chunk_coords, rel_pos + vf3 { (static_cast<f32>(i) - hp / 2.0f + 0.5f) * 0.5f, 0.0f, 0.0f }) - vx2(HP_DIM) / 2,
					vx2(HP_DIM),
					rgba_from(0.9f, 0.1f, 0.1f)
				);
			}
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
				draw_rect
				(
					platform_framebuffer,
					screen_coords_of(chunk_node->chunk.coords, vxn(tree->rel_pos, 0.0f)),
					vxx(tree->collider.dims * PIXELS_PER_METER),
					rgba_from(0.25f, 0.825f, 0.4f)
				);
				draw_thing(chunk_node->chunk.coords, vxn(tree->rel_pos + tree->collider.dims / 2.0f, 0.0f), { 0.5f, 0.3f }, &state->bmp.trees[tree->bmp_index]);
			}
		}
	}

	//
	// Render pressure plate.
	//

	{
		draw_circle
		(
			platform_framebuffer,
			screen_coords_of(state->pressure_plate.chunk->coords, vxn(state->pressure_plate.rel_pos, 0.0f)),
			static_cast<i32>(state->pressure_plate.collider.radius * PIXELS_PER_METER),
			rgba_from(0.5f, 0.5f, 0.5f)
		);

		draw_thing(state->pressure_plate.chunk->coords, vxn(state->pressure_plate.rel_pos, 0.0f), { 0.5f, 0.5f }, &state->bmp.pressure_plate);
	}

	//
	// Render hero.
	//

	{
		draw_circle
		(
			platform_framebuffer,
			screen_coords_of(state->hero.chunk->coords, vxn(state->hero.rel_pos.xy, 0.0f)),
			static_cast<i32>(state->hero.collider.radius * PIXELS_PER_METER),
			rgba_from(0.9f, 0.5f, 0.1f)
		);

		draw_thing(state->hero.chunk->coords, state->hero.rel_pos, { 0.48f, 0.225f }, &state->bmp.hero_shadow                      );
		draw_thing(state->hero.chunk->coords, state->hero.rel_pos, { 0.48f, 0.225f }, &state->bmp.hero_torsos[state->hero.cardinal]);
		draw_thing(state->hero.chunk->coords, state->hero.rel_pos, { 0.48f, 0.225f }, &state->bmp.hero_capes [state->hero.cardinal]);
		draw_thing(state->hero.chunk->coords, state->hero.rel_pos, { 0.48f, 0.225f }, &state->bmp.hero_heads [state->hero.cardinal]);
		draw_hp   (state->hero.chunk->coords, state->hero.rel_pos, state->hero.hp);
	}

	//
	// Render pet.
	//

	{
		draw_circle
		(
			platform_framebuffer,
			screen_coords_of(state->pet.chunk->coords, vxn(state->pet.rel_pos.xy, 0.0f)),
			static_cast<i32>(state->pet.collider.radius * PIXELS_PER_METER),
			rgba_from(0.9f, 0.5f, 0.8f)
		);

		draw_thing(state->pet.chunk->coords, state->pet.rel_pos, { 0.48f, 0.225f }, &state->bmp.hero_shadow                    );
		draw_thing(state->pet.chunk->coords, state->pet.rel_pos, { 0.48f, 0.550f }, &state->bmp.hero_heads[state->pet.cardinal]);
		draw_hp   (state->pet.chunk->coords, state->pet.rel_pos, state->pet.hp);
	}

	//
	// Render monstar.
	//

	if (state->monstar.existence_t > 0.0f)
	{
		draw_circle
		(
			platform_framebuffer,
			screen_coords_of(state->monstar.chunk->coords, vxn(state->monstar.rel_pos.xy, 0.0f)),
			static_cast<i32>(state->pet.collider.radius * PIXELS_PER_METER),
			rgba_from(0.5f, 0.1f, 0.1f)
		);

		draw_thing(state->monstar.chunk->coords, state->monstar.rel_pos, { 0.48f, 0.225f }, &state->bmp.hero_shadow                        );
		draw_thing(state->monstar.chunk->coords, state->monstar.rel_pos, { 0.48f, 0.550f }, &state->bmp.hero_heads[state->monstar.cardinal]);
		draw_hp   (state->monstar.chunk->coords, state->monstar.rel_pos, state->monstar.hp);
	}

	//
	// Render rocks.
	//

	FOR_ELEMS(rock, state->rock_buffer, state->rock_count)
	{
		ASSERT(IN_RANGE(static_cast<u32>(rock->bmp_index), 0, ARRAY_CAPACITY(state->bmp.rocks)));
		draw_circle
		(
			platform_framebuffer,
			screen_coords_of(rock->chunk->coords, vxn(rock->rel_pos.xy, 0.0f)),
			static_cast<i32>(rock->collider.radius * PIXELS_PER_METER),
			rgba_from(0.2f, 0.4f, 0.3f)
		);
		draw_bmp
		(
			platform_framebuffer,
			&state->bmp.hero_shadow,
			screen_coords_of(rock->chunk->coords, vxn(rock->rel_pos.xy, 0.0f)) - vxx(state->bmp.hero_shadow.dims * vf2 { 0.5f, 0.225f })
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
