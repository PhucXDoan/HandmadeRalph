#pragma once

#include <stdint.h>
typedef uint8_t     byte;
typedef const char* strlit;
typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;
typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;
typedef uint8_t     bool8;
typedef uint16_t    bool16;
typedef uint32_t    bool32;
typedef uint64_t    bool64;
typedef float       f32;
typedef double      f64;

#define internal static
#define global   static
#define aliasing auto&
#define lambda   const auto

#define MACRO_CONCAT__(X, Y)                            X##Y
#define MACRO_CONCAT_(X, Y)                             MACRO_CONCAT__(X, Y)
#define MACRO_STRINGIFY__(X)                            #X
#define MACRO_STRINGIFY_(X)                             MACRO_STRINGIFY__(X)
#define MACRO_EXPAND_(X)                                X
#define MACRO_OVERLOADED_2_(_0, _1, MACRO, ...)         MACRO
#define MACRO_OVERLOADED_3_(_0, _1, _2, MACRO, ...)     MACRO
#define MACRO_OVERLOADED_4_(_0, _1, _2, _3, MACRO, ...) MACRO
#define MACRO_HEAD_ARGS_(X, ...)                        (X)
#define MACRO_TAIL_ARGS_(X, ...)                        (__VA_ARGS__)
#define MACRO_FOR_EACH_0_(MACRO,    ...)
#define MACRO_FOR_EACH_1_(MACRO, x     )                MACRO(x)
#define MACRO_FOR_EACH_2_(MACRO, x, ...)                MACRO(x) MACRO_FOR_EACH_1_(MACRO, __VA_ARGS__)
#define MACRO_FOR_EACH_3_(MACRO, x, ...)                MACRO(x) MACRO_EXPAND_(MACRO_FOR_EACH_2_(MACRO, __VA_ARGS__))
#define MACRO_FOR_EACH_4_(MACRO, x, ...)                MACRO(x) MACRO_EXPAND_(MACRO_FOR_EACH_3_(MACRO, __VA_ARGS__))
#define MACRO_FOR_EACH_(MACRO, ...)                     MACRO_EXPAND_(MACRO_OVERLOADED_4_(__VA_ARGS__, MACRO_FOR_EACH_4_, MACRO_FOR_EACH_3_, MACRO_FOR_EACH_2_, MACRO_FOR_EACH_1_, MACRO_FOR_EACH_0_)(MACRO, __VA_ARGS__))
#define MACRO_STRINGIFY_COMMA_(X)                       MACRO_STRINGIFY_(X),
#define MACRO_STRINGIFY_ARGS_(...)                      MACRO_FOR_EACH_(MACRO_STRINGIFY_COMMA_, __VA_ARGS__)

#define CAPACITY_OF_ARRAY_(XS)              (sizeof(XS) / sizeof((XS)[0]))
#define CAPACITY_OF_MEMBER_(TYPE, MEMBER)   (CAPACITY_OF_ARRAY_(((TYPE*) 0)->MEMBER))
#define ARRAY_CAPACITY(...)                 (MACRO_EXPAND_(MACRO_OVERLOADED_2_(__VA_ARGS__, CAPACITY_OF_MEMBER_, CAPACITY_OF_ARRAY_)(__VA_ARGS__)))

#define FOR_INTERVAL_(NAME, MINI, MAXI)     for (i32 NAME = (MINI); NAME < (MAXI); NAME += 1)
#define FOR_INDICIES_(NAME, MAXI)           FOR_INTERVAL_(NAME, 0, (MAXI))
#define FOR_REPEAT_(MAXI)                   FOR_INTERVAL_(MACRO_CONCAT_(FOR_REPEAT_, __LINE__), 0, (MAXI))
#define FOR_RANGE(...)                      MACRO_EXPAND_(MACRO_OVERLOADED_3_(__VA_ARGS__, FOR_INTERVAL_, FOR_INDICIES_, FOR_REPEAT_)(__VA_ARGS__))
#define FOR_INTERVAL_REV_(NAME, MINI, MAXI) for (i32 NAME = (MAXI) - 1, MACRO_CONCAT_(NAME, min); NAME >= MACRO_CONCAT_(NAME, min); NAME -= 1)
#define FOR_INDICIES_REV_(NAME, MAXI)       FOR_INTERVAL_REV_(NAME, 0, (MAXI))
#define FOR_RANGE_REV(...)                  MACRO_EXPAND_(MACRO_OVERLOADED_3_(__VA_ARGS__, FOR_INTERVAL_REV_, FOR_INDICIES_REV_)(__VA_ARGS__))

#define FOR_POINTER_(NAME, XS, COUNT)       for (i32 MACRO_CONCAT_(NAME, _index) = 0; MACRO_CONCAT_(NAME, _index) < (COUNT); MACRO_CONCAT_(NAME, _index) += 1) if (const auto NAME = &(XS)[MACRO_CONCAT_(NAME, _index)]; false); else
#define FOR_ARRAY_(NAME, XS)                FOR_POINTER_(NAME, (XS), ARRAY_CAPACITY(XS))
#define FOR_IT_(XS)                         FOR_POINTER_(it, (XS), ARRAY_CAPACITY(XS))
#define FOR_ELEMS(...)                      MACRO_EXPAND_(MACRO_OVERLOADED_3_(__VA_ARGS__, FOR_POINTER_, FOR_ARRAY_, FOR_IT_)(__VA_ARGS__))
#define FOR_POINTER_REV_(NAME, XS, COUNT)   for (i32 MACRO_CONCAT_(NAME, _index) = (COUNT) - 1; MACRO_CONCAT_(NAME, _index) >= 0; MACRO_CONCAT_(NAME, _index) -= 1) if (const auto NAME = &(XS)[MACRO_CONCAT_(NAME, _index)]; false); else
#define FOR_ARRAY_REV_(NAME, XS)            FOR_POINTER_REV_(NAME, (XS), ARRAY_CAPACITY(XS))
#define FOR_ELEMS_REV(...)                  MACRO_EXPAND_(MACRO_OVERLOADED_3_(__VA_ARGS__, FOR_POINTER_REV_, FOR_ARRAY_REV_)(__VA_ARGS__))

#define FOR_NODES_(NAME, NODES)             if (auto NAME = (NODES); false); else for (i32 MACRO_CONCAT_(NAME, _index) = 0; NAME; MACRO_CONCAT_(NAME, _index) += 1, NAME = NAME->next_node)
#define FOR_IT_NODES_(NODES)                FOR_NODES_(it, (NODES))
#define FOR_NODES(...)                      MACRO_EXPAND_(MACRO_OVERLOADED_2_(__VA_ARGS__, FOR_NODES_, FOR_IT_NODES_)(__VA_ARGS__))

#define IMPLIES(P, Q)                       (!(P) || (Q))
#define IFF(P, Q)                           (!(P) == !(Q))
#define IN_RANGE(X, MINI, MAXI)             ((MINI) <= (X) && (X) < (MAXI))
#define SWAP(X, Y)                          do { auto MACRO_CONCAT_(SWAP_, __LINE__) = *(X); *(X) = *(Y); *(Y) = MACRO_CONCAT_(SWAP_, __LINE__); } while (false)
#define KIBIBYTES_OF(N)                     (1024ULL *             (N))
#define MEBIBYTES_OF(N)                     (1024ULL * KIBIBYTES_OF(N))
#define GIBIBYTES_OF(N)                     (1024ULL * MEBIBYTES_OF(N))
#define TEBIBYTES_OF(N)                     (1024ULL * GIBIBYTES_OF(N))

#if DEBUG
	#include <assert.h>
	#include <stdio.h>
	#pragma warning(push)
	#pragma warning(disable : 5039)
	#include <windows.h>
	#pragma warning(pop)
	#undef interface
	#undef min
	#undef max

	#define DEBUG_persist static

	#define ASSERT(EXPRESSION) do { if (!(EXPRESSION)) { *((i32*)(0)) = 0; } } while (false)

	#define DEBUG_printf(FSTR, ...)\
	do\
	{\
		char MACRO_CONCAT_(DEBUG_PRINTF_, __LINE__)[1024];\
		sprintf_s(MACRO_CONCAT_(DEBUG_PRINTF_, __LINE__), sizeof(MACRO_CONCAT_(DEBUG_PRINTF_, __LINE__)), (FSTR), __VA_ARGS__);\
		OutputDebugStringA(MACRO_CONCAT_(DEBUG_PRINTF_, __LINE__));\
	}\
	while (false)

	#define DEBUG_once\
	for (DEBUG_persist bool32 MACRO_CONCAT_(DEBUG_ONCE_, __LINE__) = true; MACRO_CONCAT_(DEBUG_ONCE_, __LINE__); MACRO_CONCAT_(DEBUG_ONCE_, __LINE__) = false)

	global const i64 DEBUG_PERFORMANCE_COUNTER_FREQUENCY =
		[](void)
		{
			LARGE_INTEGER n;
			QueryPerformanceFrequency(&n);
			return n.QuadPart;
		}();

	#define DEBUG_PROFILER_start(NAME)\
	LARGE_INTEGER MACRO_CONCAT_(NAME, _LI_0_);\
	QueryPerformanceCounter(&MACRO_CONCAT_(NAME, _LI_0_));\
	u64 MACRO_CONCAT_(NAME, _CC_0_) = __rdtsc()

	#define DEBUG_PROFILER_end(NAME, SAMPLES)\
	u64 MACRO_CONCAT_(NAME, _CC_1_) = __rdtsc();\
	LARGE_INTEGER MACRO_CONCAT_(NAME, _LI_1_);\
	QueryPerformanceCounter(&MACRO_CONCAT_(NAME, _LI_1_));\
	DEBUG_persist u64 MACRO_CONCAT_(NAME, _LI_TOTAL_) = 0;\
	DEBUG_persist u64 MACRO_CONCAT_(NAME, _CC_TOTAL_) = 0;\
	DEBUG_persist u64 MACRO_CONCAT_(NAME, _COUNTER_)  = 0;\
	MACRO_CONCAT_(NAME, _LI_TOTAL_)   += MACRO_CONCAT_(NAME, _LI_1_).QuadPart - MACRO_CONCAT_(NAME, _LI_0_).QuadPart;\
	MACRO_CONCAT_(NAME, _CC_TOTAL_)   += MACRO_CONCAT_(NAME, _CC_1_)          - MACRO_CONCAT_(NAME, _CC_0_);\
	MACRO_CONCAT_(NAME, _COUNTER_) += 1;\
	if (MACRO_CONCAT_(NAME, _COUNTER_) >= (SAMPLES))\
	{\
		DEBUG_printf(MACRO_STRINGIFY_(NAME) "\n\t:: %8.4f ms\n\t:: %8.4f mc\n", static_cast<f64>(MACRO_CONCAT_(NAME, _LI_TOTAL_)) / MACRO_CONCAT_(NAME, _COUNTER_) / DEBUG_PERFORMANCE_COUNTER_FREQUENCY * 1000.0, static_cast<f64>(MACRO_CONCAT_(NAME, _CC_TOTAL_)) / MACRO_CONCAT_(NAME, _COUNTER_) / 1000000.0);\
		MACRO_CONCAT_(NAME, _LI_TOTAL_) = 0;\
		MACRO_CONCAT_(NAME, _CC_TOTAL_) = 0;\
		MACRO_CONCAT_(NAME, _COUNTER_)  = 0;\
	}
#else
	#define ASSERT(EXPRESSION)
	#define DEBUG_printf(FSTR, ...)
	#define DEBUG_once                       if (true); else
	#define DEBUG_PROFILER_create_group(...)
	#define DEBUG_PROFILER_flush_group(...)
	#define DEBUG_PROFILER_start(...)
	#define DEBUG_PROFILER_end(...)
	#define DEBUG_STDOUT_HALT()
#endif


#define enum_loose(NAME, TYPE)\
enum struct NAME : TYPE;\
internal constexpr TYPE operator+  (const NAME& a               ) { return static_cast<TYPE>(a); }\
internal constexpr TYPE operator<  (const NAME& a, const NAME& b) { return +a <  +b; }\
internal constexpr TYPE operator<= (const NAME& a, const NAME& b) { return +a <= +b; }\
internal constexpr TYPE operator>  (const NAME& a, const NAME& b) { return +a >  +b; }\
internal constexpr TYPE operator>= (const NAME& a, const NAME& b) { return +a >= +b; }\
enum struct NAME : TYPE

#define flag_struct(NAME, TYPE)\
enum struct NAME : TYPE;\
internal constexpr TYPE operator+  (const NAME& a               ) { return     static_cast<TYPE>(a);            }\
internal constexpr NAME operator&  (const NAME& a, const NAME& b) { return     static_cast<NAME>( (+a) & (+b)); }\
internal constexpr NAME operator|  (const NAME& a, const NAME& b) { return     static_cast<NAME>( (+a) | (+b)); }\
internal constexpr NAME operator^  (const NAME& a, const NAME& b) { return     static_cast<NAME>( (+a) ^ (+b)); }\
internal constexpr NAME operator<< (const NAME& a, const i32&  n) { return     static_cast<NAME>( (+a) << n  ); }\
internal constexpr NAME operator>> (const NAME& a, const i32&  n) { return     static_cast<NAME>( (+a) >> n  ); }\
internal constexpr NAME operator~  (const NAME& a               ) { return     static_cast<NAME>( ~+a        ); }\
internal constexpr NAME operator&= (      NAME& a, const NAME& b) { return a = static_cast<NAME>( (+a) & (+b)); }\
internal constexpr NAME operator|= (      NAME& a, const NAME& b) { return a = static_cast<NAME>( (+a) | (+b)); }\
internal constexpr NAME operator^= (      NAME& a, const NAME& b) { return a = static_cast<NAME>( (+a) ^ (+b)); }\
internal constexpr NAME operator<<=(      NAME& a, const i32&  n) { return a = static_cast<NAME>( (+a) << n  ); }\
internal constexpr NAME operator>>=(      NAME& a, const i32&  n) { return a = static_cast<NAME>( (+a) >> n  ); }\
enum struct NAME : TYPE

#define enum_start_region(NAME) MACRO_CONCAT_(NAME, _START), MACRO_CONCAT_(NAME, _START_) = MACRO_CONCAT_(NAME, _START) - 1,
#define enum_end_region(NAME)   MACRO_CONCAT_(NAME, _END), MACRO_CONCAT_(NAME, _COUNT) = MACRO_CONCAT_(NAME, _END) - MACRO_CONCAT_(NAME, _START), MACRO_CONCAT_(NAME, _END_) = MACRO_CONCAT_(NAME, _END) - 1,

#include <utility>
#define DEFER auto MACRO_CONCAT_(DEFER_, __LINE__) = DEFER_EMPTY_ {} + [&]()

template <typename F>
struct DEFER_
{
	F f;
	DEFER_(F f) : f(f) {}
	~DEFER_() { f(); }
	DEFER_ operator=(DEFER_&) = delete;
};

struct DEFER_EMPTY_ {};

template <typename F>
internal DEFER_<F> operator+(DEFER_EMPTY_, F&& f)
{
	return DEFER_<F>(std::forward<F>(f));
}

//
// Misc.
//

template <typename TYPE>
internal TYPE* pop_node(TYPE** node)
{
	TYPE* head = *node;
	*node = (*node)->next_node;
	return head;
}

template <typename TYPE>
internal void push_single_node(TYPE* head, TYPE** node)
{
	head->next_node = *node;
	*node           = head;
}

template <typename TYPE>
internal void push_entire_node(TYPE* head, TYPE** node)
{
	if (head)
	{
		TYPE* last = head;
		while (last->next_node)
		{
			last = last->next_node;
		}
		last->next_node = *node;
		*node           = head;
	}
}

//
// Memory.
//

#define memory_arena_checkpoint(ARENA)\
u64 MACRO_CONCAT_(MEMORY_ARENA_CHECKPOINT_, __LINE__) = (ARENA)->used;\
DEFER { (ARENA)->used = MACRO_CONCAT_(MEMORY_ARENA_CHECKPOINT_, __LINE__); }

struct MemoryArena
{
	u64   used;
	u64   size;
	byte* base;
};

template <typename TYPE>
internal TYPE* memory_arena_allocate(MemoryArena* arena, const u64& count = 1)
{
	ASSERT(arena->used + sizeof(TYPE) * count <= arena->size);
	byte* allocation = arena->base + arena->used;
	arena->used += sizeof(TYPE) * count;
	return reinterpret_cast<TYPE*>(allocation);
}

template <typename TYPE>
internal TYPE* memory_arena_allocate_zero(MemoryArena* arena, const u64& count = 1)
{
	ASSERT(arena->used + sizeof(TYPE) * count <= arena->size);
	byte* allocation = arena->base + arena->used;
	memset(allocation, static_cast<unsigned char>(0), sizeof(TYPE) * count);
	arena->used += sizeof(TYPE) * count;
	return reinterpret_cast<TYPE*>(allocation);
}

internal MemoryArena memory_arena_reserve(MemoryArena* arena, const u64& size)
{
	ASSERT(arena->used + size <= arena->size);
	MemoryArena reservation;
	reservation.size = size;
	reservation.base = arena->base + arena->used;
	reservation.used = 0;
	arena->used += size;
	return reservation;
}

template <typename TYPE>
internal TYPE* memory_arena_allocate_from_available(TYPE** available, MemoryArena* arena)
{
	if (*available)
	{
		TYPE* allocation = *available;
		*available = (*available)->next_node;
		return allocation;
	}
	else
	{
		return memory_arena_allocate<TYPE>(arena);
	}
}

//
// Math.
//

#define PASS_V2(V) (V).x, (V).y
#define PASS_V3(V) (V).x, (V).y, (V).z
#define PASS_V4(V) (V).x, (V).y, (V).z, (V).w

#include <algorithm>
using std::clamp;
using std::min;
using std::max;

global constexpr f32 TAU   = 6.28318530717f;
global constexpr f32 SQRT2 = 1.41421356237f;

struct vf2
{
	union
	{
		struct { f32 x; f32 y; };
		f32 coordinates[2];
	};
};

struct vf3
{
	union
	{
		struct { f32 x; f32 y; f32 z; };
		vf2 xy;
		f32 coordinates[3];
	};
};

struct vf4
{
	union
	{
		struct { f32 x; f32 y; f32 z; f32 w; };
		vf3 xyz;
		vf2 xy;
		f32 coordinates[4];
	};
};

internal constexpr bool32 operator+ (const vf2& v              ) { return v.x || v.y;               }
internal constexpr bool32 operator+ (const vf3& v              ) { return v.x || v.y || v.z;        }
internal constexpr bool32 operator+ (const vf4& v              ) { return v.x || v.y || v.z || v.w; }
internal constexpr vf2    operator- (const vf2& v              ) { return { -v.x, -v.y             }; }
internal constexpr vf3    operator- (const vf3& v              ) { return { -v.x, -v.y, -v.z       }; }
internal constexpr vf4    operator- (const vf4& v              ) { return { -v.x, -v.y, -v.z, -v.w }; }
internal constexpr bool32 operator==(const vf2& u, const vf2& v) { return u.x == v.x && u.y == v.y;                             }
internal constexpr bool32 operator==(const vf3& u, const vf3& v) { return u.x == v.x && u.y == v.y && u.z == v.z;               }
internal constexpr bool32 operator==(const vf4& u, const vf4& v) { return u.x == v.x && u.y == v.y && u.z == v.z && u.w == v.w; }
internal constexpr bool32 operator!=(const vf2& u, const vf2& v) { return !(u == v); }
internal constexpr bool32 operator!=(const vf3& u, const vf3& v) { return !(u == v); }
internal constexpr bool32 operator!=(const vf4& u, const vf4& v) { return !(u == v); }
internal constexpr vf2    operator+ (const vf2& u, const vf2& v) { return { u.x + v.x, u.y + v.y                       }; }
internal constexpr vf3    operator+ (const vf3& u, const vf3& v) { return { u.x + v.x, u.y + v.y, u.z + v.z            }; }
internal constexpr vf4    operator+ (const vf4& u, const vf4& v) { return { u.x + v.x, u.y + v.y, u.z + v.z, u.w + v.w }; }
internal constexpr vf2    operator- (const vf2& u, const vf2& v) { return { u.x - v.x, u.y - v.y                       }; }
internal constexpr vf3    operator- (const vf3& u, const vf3& v) { return { u.x - v.x, u.y - v.y, u.z - v.z            }; }
internal constexpr vf4    operator- (const vf4& u, const vf4& v) { return { u.x - v.x, u.y - v.y, u.z - v.z, u.w - v.w }; }
internal constexpr vf2    operator/ (const vf2& v, const f32& k) { return { v.x / k, v.y / k                   }; }
internal constexpr vf3    operator/ (const vf3& v, const f32& k) { return { v.x / k, v.y / k, v.z / k          }; }
internal constexpr vf4    operator/ (const vf4& v, const f32& k) { return { v.x / k, v.y / k, v.z / k, v.w / k }; }
internal constexpr vf2    operator* (const vf2& u, const vf2& v) { return { u.x * v.x, u.y * v.y                       }; }
internal constexpr vf3    operator* (const vf3& u, const vf3& v) { return { u.x * v.x, u.y * v.y, u.z * v.z            }; }
internal constexpr vf4    operator* (const vf4& u, const vf4& v) { return { u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w }; }
internal constexpr vf2    operator* (const vf2& v, const f32& k) { return { v.x * k, v.y * k                   }; }
internal constexpr vf3    operator* (const vf3& v, const f32& k) { return { v.x * k, v.y * k, v.z * k          }; }
internal constexpr vf4    operator* (const vf4& v, const f32& k) { return { v.x * k, v.y * k, v.z * k, v.w * k }; }
internal constexpr vf2    operator* (const f32& k, const vf2& v) { return v * k; }
internal constexpr vf3    operator* (const f32& k, const vf3& v) { return v * k; }
internal constexpr vf4    operator* (const f32& k, const vf4& v) { return v * k; }
internal constexpr vf2&   operator+=(      vf2& u, const vf2& v) { return u = u + v; }
internal constexpr vf3&   operator+=(      vf3& u, const vf3& v) { return u = u + v; }
internal constexpr vf4&   operator+=(      vf4& u, const vf4& v) { return u = u + v; }
internal constexpr vf2&   operator-=(      vf2& u, const vf2& v) { return u = u - v; }
internal constexpr vf3&   operator-=(      vf3& u, const vf3& v) { return u = u - v; }
internal constexpr vf4&   operator-=(      vf4& u, const vf4& v) { return u = u - v; }
internal constexpr vf2&   operator*=(      vf2& u, const vf2& v) { return u = u * v; }
internal constexpr vf3&   operator*=(      vf3& u, const vf3& v) { return u = u * v; }
internal constexpr vf4&   operator*=(      vf4& u, const vf4& v) { return u = u * v; }
internal constexpr vf2&   operator*=(      vf2& v, const f32& k) { return v = v * k; }
internal constexpr vf3&   operator*=(      vf3& v, const f32& k) { return v = v * k; }
internal constexpr vf4&   operator*=(      vf4& v, const f32& k) { return v = v * k; }
internal constexpr vf2&   operator/=(      vf2& v, const f32& k) { return v = v / k; }
internal constexpr vf3&   operator/=(      vf3& v, const f32& k) { return v = v / k; }
internal constexpr vf4&   operator/=(      vf4& v, const f32& k) { return v = v / k; }

struct vi2
{
	union
	{
		struct { i32 x; i32 y; };
		i32 coordinates[2];
	};
};

struct vi3
{
	union
	{
		struct { i32 x; i32 y; i32 z; };
		vi2 xy;
		i32 coordinates[3];
	};
};

struct vi4
{
	union
	{
		struct { i32 x; i32 y; i32 z; i32 w; };
		vi3 xyz;
		vi2 xy;
		i32 coordinates[4];
	};
};

internal constexpr bool32 operator+ (const vi2& v              ) { return v.x || v.y;               }
internal constexpr bool32 operator+ (const vi3& v              ) { return v.x || v.y || v.z;        }
internal constexpr bool32 operator+ (const vi4& v              ) { return v.x || v.y || v.z || v.w; }
internal constexpr vi2    operator- (const vi2& v              ) { return { -v.x, -v.y             }; }
internal constexpr vi3    operator- (const vi3& v              ) { return { -v.x, -v.y, -v.z       }; }
internal constexpr vi4    operator- (const vi4& v              ) { return { -v.x, -v.y, -v.z, -v.w }; }
internal constexpr bool32 operator==(const vi2& u, const vi2& v) { return u.x == v.x && u.y == v.y;                             }
internal constexpr bool32 operator==(const vi3& u, const vi3& v) { return u.x == v.x && u.y == v.y && u.z == v.z;               }
internal constexpr bool32 operator==(const vi4& u, const vi4& v) { return u.x == v.x && u.y == v.y && u.z == v.z && u.w == v.w; }
internal constexpr bool32 operator!=(const vi2& u, const vi2& v) { return !(u == v); }
internal constexpr bool32 operator!=(const vi3& u, const vi3& v) { return !(u == v); }
internal constexpr bool32 operator!=(const vi4& u, const vi4& v) { return !(u == v); }
internal constexpr vi2    operator+ (const vi2& u, const vi2& v) { return { u.x + v.x, u.y + v.y                       }; }
internal constexpr vi3    operator+ (const vi3& u, const vi3& v) { return { u.x + v.x, u.y + v.y, u.z + v.z            }; }
internal constexpr vi4    operator+ (const vi4& u, const vi4& v) { return { u.x + v.x, u.y + v.y, u.z + v.z, u.w + v.w }; }
internal constexpr vi2    operator- (const vi2& u, const vi2& v) { return { u.x - v.x, u.y - v.y                       }; }
internal constexpr vi3    operator- (const vi3& u, const vi3& v) { return { u.x - v.x, u.y - v.y, u.z - v.z            }; }
internal constexpr vi4    operator- (const vi4& u, const vi4& v) { return { u.x - v.x, u.y - v.y, u.z - v.z, u.w - v.w }; }
internal constexpr vi2    operator/ (const vi2& v, const i32& k) { return { v.x / k, v.y / k                   }; }
internal constexpr vi3    operator/ (const vi3& v, const i32& k) { return { v.x / k, v.y / k, v.z / k          }; }
internal constexpr vi4    operator/ (const vi4& v, const i32& k) { return { v.x / k, v.y / k, v.z / k, v.w / k }; }
internal constexpr vi2    operator* (const vi2& u, const vi2& v) { return { u.x * v.x, u.y * v.y                       }; }
internal constexpr vi3    operator* (const vi3& u, const vi3& v) { return { u.x * v.x, u.y * v.y, u.z * v.z            }; }
internal constexpr vi4    operator* (const vi4& u, const vi4& v) { return { u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w }; }
internal constexpr vi2    operator* (const vi2& v, const i32& k) { return { v.x * k, v.y * k                   }; }
internal constexpr vi3    operator* (const vi3& v, const i32& k) { return { v.x * k, v.y * k, v.z * k          }; }
internal constexpr vi4    operator* (const vi4& v, const i32& k) { return { v.x * k, v.y * k, v.z * k, v.w * k }; }
internal constexpr vi2    operator* (const i32& k, const vi2& v) { return v * k; }
internal constexpr vi3    operator* (const i32& k, const vi3& v) { return v * k; }
internal constexpr vi4    operator* (const i32& k, const vi4& v) { return v * k; }
internal constexpr vi2&   operator+=(      vi2& u, const vi2& v) { return u = u + v; }
internal constexpr vi3&   operator+=(      vi3& u, const vi3& v) { return u = u + v; }
internal constexpr vi4&   operator+=(      vi4& u, const vi4& v) { return u = u + v; }
internal constexpr vi2&   operator-=(      vi2& u, const vi2& v) { return u = u - v; }
internal constexpr vi3&   operator-=(      vi3& u, const vi3& v) { return u = u - v; }
internal constexpr vi4&   operator-=(      vi4& u, const vi4& v) { return u = u - v; }
internal constexpr vi2&   operator*=(      vi2& u, const vi2& v) { return u = u * v; }
internal constexpr vi3&   operator*=(      vi3& u, const vi3& v) { return u = u * v; }
internal constexpr vi4&   operator*=(      vi4& u, const vi4& v) { return u = u * v; }
internal constexpr vi2&   operator*=(      vi2& v, const i32& k) { return v = v * k; }
internal constexpr vi3&   operator*=(      vi3& v, const i32& k) { return v = v * k; }
internal constexpr vi4&   operator*=(      vi4& v, const i32& k) { return v = v * k; }
internal constexpr vi2&   operator/=(      vi2& v, const i32& k) { return v = v / k; }
internal constexpr vi3&   operator/=(      vi3& v, const i32& k) { return v = v / k; }
internal constexpr vi4&   operator/=(      vi4& v, const i32& k) { return v = v / k; }

internal constexpr vf2    operator+ (const vf2& u, const vi2& v) { return { u.x + static_cast<f32>(v.x), u.y + static_cast<f32>(v.y)                                                           }; }
internal constexpr vf3    operator+ (const vf3& u, const vi3& v) { return { u.x + static_cast<f32>(v.x), u.y + static_cast<f32>(v.y), u.z + static_cast<f32>(v.z)                              }; }
internal constexpr vf4    operator+ (const vf4& u, const vi4& v) { return { u.x + static_cast<f32>(v.x), u.y + static_cast<f32>(v.y), u.z + static_cast<f32>(v.z), u.w + static_cast<f32>(v.w) }; }
internal constexpr vf2    operator+ (const vi2& u, const vf2& v) { return v + u; }
internal constexpr vf3    operator+ (const vi3& u, const vf3& v) { return v + u; }
internal constexpr vf4    operator+ (const vi4& u, const vf4& v) { return v + u; }
internal constexpr vf2    operator- (const vf2& u, const vi2& v) { return { u.x - static_cast<f32>(v.x), u.y - static_cast<f32>(v.y)                                                           }; }
internal constexpr vf3    operator- (const vf3& u, const vi3& v) { return { u.x - static_cast<f32>(v.x), u.y - static_cast<f32>(v.y), u.z - static_cast<f32>(v.z)                              }; }
internal constexpr vf4    operator- (const vf4& u, const vi4& v) { return { u.x - static_cast<f32>(v.x), u.y - static_cast<f32>(v.y), u.z - static_cast<f32>(v.z), u.w - static_cast<f32>(v.w) }; }
internal constexpr vf2    operator- (const vi2& u, const vf2& v) { return { static_cast<f32>(u.x) - v.x, static_cast<f32>(u.y) - v.y                                                           }; }
internal constexpr vf3    operator- (const vi3& u, const vf3& v) { return { static_cast<f32>(u.x) - v.x, static_cast<f32>(u.y) - v.y, static_cast<f32>(u.z) - v.z                              }; }
internal constexpr vf4    operator- (const vi4& u, const vf4& v) { return { static_cast<f32>(u.x) - v.x, static_cast<f32>(u.y) - v.y, static_cast<f32>(u.z) - v.z, static_cast<f32>(u.w) - v.w }; }
internal constexpr vf2    operator/ (const vf2& v, const i32& k) { return { v.x / k, v.y / k                   }; }
internal constexpr vf3    operator/ (const vf3& v, const i32& k) { return { v.x / k, v.y / k, v.z / k          }; }
internal constexpr vf4    operator/ (const vf4& v, const i32& k) { return { v.x / k, v.y / k, v.z / k, v.w / k }; }
internal constexpr vf2    operator/ (const vi2& v, const f32& k) { return { v.x / k, v.y / k                   }; }
internal constexpr vf3    operator/ (const vi3& v, const f32& k) { return { v.x / k, v.y / k, v.z / k          }; }
internal constexpr vf4    operator/ (const vi4& v, const f32& k) { return { v.x / k, v.y / k, v.z / k, v.w / k }; }
internal constexpr vf2    operator* (const vf2& u, const vi2& v) { return { u.x * v.x, u.y * v.y                       }; }
internal constexpr vf3    operator* (const vf3& u, const vi3& v) { return { u.x * v.x, u.y * v.y, u.z * v.z            }; }
internal constexpr vf4    operator* (const vf4& u, const vi4& v) { return { u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w }; }
internal constexpr vf2    operator* (const vi2& u, const vf2& v) { return { u.x * v.x, u.y * v.y                       }; }
internal constexpr vf3    operator* (const vi3& u, const vf3& v) { return { u.x * v.x, u.y * v.y, u.z * v.z            }; }
internal constexpr vf4    operator* (const vi4& u, const vf4& v) { return { u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w }; }
internal constexpr vf2    operator* (const vf2& v, const i32& k) { return { v.x * k, v.y * k                   }; }
internal constexpr vf3    operator* (const vf3& v, const i32& k) { return { v.x * k, v.y * k, v.z * k          }; }
internal constexpr vf4    operator* (const vf4& v, const i32& k) { return { v.x * k, v.y * k, v.z * k, v.w * k }; }
internal constexpr vf2    operator* (const vi2& v, const f32& k) { return { v.x * k, v.y * k                   }; }
internal constexpr vf3    operator* (const vi3& v, const f32& k) { return { v.x * k, v.y * k, v.z * k          }; }
internal constexpr vf4    operator* (const vi4& v, const f32& k) { return { v.x * k, v.y * k, v.z * k, v.w * k }; }
internal constexpr vf2    operator* (const f32& k, const vi2& v) { return v * k; }
internal constexpr vf3    operator* (const f32& k, const vi3& v) { return v * k; }
internal constexpr vf4    operator* (const f32& k, const vi4& v) { return v * k; }
internal constexpr vf2&   operator+=(      vf2& u, const vi2& v) { return u = u + v; }
internal constexpr vf3&   operator+=(      vf3& u, const vi3& v) { return u = u + v; }
internal constexpr vf4&   operator+=(      vf4& u, const vi4& v) { return u = u + v; }
internal constexpr vf2&   operator-=(      vf2& u, const vi2& v) { return u = u - v; }
internal constexpr vf3&   operator-=(      vf3& u, const vi3& v) { return u = u - v; }
internal constexpr vf4&   operator-=(      vf4& u, const vi4& v) { return u = u - v; }
internal constexpr vf2&   operator*=(      vf2& u, const vi2& v) { return u = u * v; }
internal constexpr vf3&   operator*=(      vf3& u, const vi3& v) { return u = u * v; }
internal constexpr vf4&   operator*=(      vf4& u, const vi4& v) { return u = u * v; }
internal constexpr vf2&   operator*=(      vf2& v, const i32& k) { return v = v * k; }
internal constexpr vf3&   operator*=(      vf3& v, const i32& k) { return v = v * k; }
internal constexpr vf4&   operator*=(      vf4& v, const i32& k) { return v = v * k; }
internal constexpr vf2&   operator/=(      vf2& v, const i32& k) { return v = v / k; }
internal constexpr vf3&   operator/=(      vf3& v, const i32& k) { return v = v / k; }
internal constexpr vf4&   operator/=(      vf4& v, const i32& k) { return v = v / k; }

internal constexpr vf2    vxx(const vi2& v                            ) { return { static_cast<f32>(v.x), static_cast<f32>(v.y)                                               }; }
internal constexpr vi2    vxx(const vf2& v                            ) { return { static_cast<i32>(v.x), static_cast<i32>(v.y)                                               }; }
internal constexpr vf3    vxx(const vi3& v                            ) { return { static_cast<f32>(v.x), static_cast<f32>(v.y), static_cast<f32>(v.z)                        }; }
internal constexpr vi3    vxx(const vf3& v                            ) { return { static_cast<i32>(v.x), static_cast<i32>(v.y), static_cast<i32>(v.z)                        }; }
internal constexpr vf4    vxx(const vi4& v                            ) { return { static_cast<f32>(v.x), static_cast<f32>(v.y), static_cast<f32>(v.z), static_cast<f32>(v.w) }; }
internal constexpr vi4    vxx(const vf4& v                            ) { return { static_cast<i32>(v.x), static_cast<i32>(v.y), static_cast<i32>(v.z), static_cast<i32>(v.w) }; }
internal constexpr vf3    vxx(const vf2& v, const f32& a              ) { return { v.x, v.y,   a    }; }
internal constexpr vf4    vxx(const vf2& v, const f32& a, const f32& b) { return { v.x, v.y,   a, b }; }
internal constexpr vf4    vxx(const vf3& v, const f32& a              ) { return { v.x, v.y, v.z, a }; }

internal constexpr vf2    vx2(const f32& a) { return { a, a       }; }
internal constexpr vi2    vx2(const i32& a) { return { a, a       }; }
internal constexpr vf3    vx3(const f32& a) { return { a, a, a    }; }
internal constexpr vi3    vx3(const i32& a) { return { a, a, a    }; }
internal constexpr vf4    vx4(const f32& a) { return { a, a, a, a }; }
internal constexpr vi4    vx4(const i32& a) { return { a, a, a, a }; }

#define VXX_COLOR_F3_(C0, C1, C2)     (static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C0) * 255.0f) << 16) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C1) * 255.0f) <<  8) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C2) * 255.0f) << 0))
#define VXX_COLOR_F4_(C0, C1, C2, C3) (static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C0) * 255.0f) << 24) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C1) * 255.0f) << 16) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C2) * 255.0f) << 8) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C3) * 255.0f) << 0))
#define VXX_COLOR_I3_(C0, C1, C2)     (static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C0)) << 16) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C1)) <<  8) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C2)) << 0))
#define VXX_COLOR_I4_(C0, C1, C2, C3) (static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C0)) << 24) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C1)) << 16) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C2)) << 8) | static_cast<u32>(static_cast<u8>(MACRO_CONCAT_(v., C3)) << 0))
internal constexpr u32 vxx_argb(const vf3& v) { return VXX_COLOR_F3_(   x, y, z); };
internal constexpr u32 vxx_argb(const vf4& v) { return VXX_COLOR_F4_(w, x, y, z); };
internal constexpr u32 vxx_argb(const vi3& v) { return VXX_COLOR_I3_(   x, y, z); };
internal constexpr u32 vxx_argb(const vi4& v) { return VXX_COLOR_I4_(w, x, y, z); };
internal constexpr u32 vxx_rgba(const vf3& v) { return VXX_COLOR_F3_(x, y, z   ); };
internal constexpr u32 vxx_rgba(const vf4& v) { return VXX_COLOR_F4_(x, y, z, w); };
internal constexpr u32 vxx_rgba(const vi3& v) { return VXX_COLOR_I3_(x, y, z   ); };
internal constexpr u32 vxx_rgba(const vi4& v) { return VXX_COLOR_I4_(x, y, z, w); };
#undef VXX_COLOR_F3_
#undef VXX_COLOR_F4_
#undef VXX_COLOR_I3_
#undef VXX_COLOR_I4_

template <typename TYPE>
i32 sign(const TYPE& x)
{
	return (static_cast<TYPE>(0) < x) - (x < static_cast<TYPE>(0));
}

internal constexpr f32 square(const f32& x) { return x * x;     }
internal constexpr f32 cube  (const f32& x) { return x * x * x; }

internal constexpr vf4 lerp(const vf4& a, const vf4& b, const f32& t) { return a * (1.0f - t) + b * t; }
internal constexpr vf3 lerp(const vf3& a, const vf3& b, const f32& t) { return a * (1.0f - t) + b * t; }
internal constexpr vf2 lerp(const vf2& a, const vf2& b, const f32& t) { return a * (1.0f - t) + b * t; }
internal constexpr f32 lerp(const f32& a, const f32& b, const f32& t) { return a * (1.0f - t) + b * t; }

internal constexpr vf2 conjugate(const vf2& v) { return {  v.x, -v.y }; }
internal constexpr vi2 conjugate(const vi2& v) { return {  v.x, -v.y }; }
internal constexpr vf2 rotate90 (const vf2& v) { return { -v.y,  v.x }; }

internal vf2 polar(const f32& angle) { return { cosf(angle), sinf(angle) }; }

internal vf2 rotate(const vf2& v, const f32& angle)
{
	vf2 p = polar(angle);
	return { v.x * p.x - v.y * p.y, v.x * p.y + v.y * p.x };
}

internal constexpr f32 dampen(const f32& a, const f32& b, const f32& k, const f32& dt) { return lerp(a, b, 1.0f - expf(-k * dt)); }
internal constexpr vf2 dampen(const vf2& a, const vf2& b, const f32& k, const f32& dt) { return lerp(a, b, 1.0f - expf(-k * dt)); }
internal constexpr vf3 dampen(const vf3& a, const vf3& b, const f32& k, const f32& dt) { return lerp(a, b, 1.0f - expf(-k * dt)); }
internal constexpr vf4 dampen(const vf4& a, const vf4& b, const f32& k, const f32& dt) { return lerp(a, b, 1.0f - expf(-k * dt)); }

internal constexpr f32 dot(const vf2& u, const vf2& v) { return u.x * v.x + u.y * v.y;                         }
internal constexpr f32 dot(const vf3& u, const vf3& v) { return u.x * v.x + u.y * v.y + u.z * v.z;             }
internal constexpr f32 dot(const vf4& u, const vf4& v) { return u.x * v.x + u.y * v.y + u.z * v.z + u.w * v.w; }

internal constexpr vf3 cross(const vf3& u, const vf3& v) { return { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x }; }

internal constexpr f32 norm_sq(const vf2& v) { return v.x * v.x + v.y * v.y;                         }
internal constexpr f32 norm_sq(const vf3& v) { return v.x * v.x + v.y * v.y + v.z * v.z;             }
internal constexpr f32 norm_sq(const vf4& v) { return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w; }

internal f32 norm(const vf2& v) { return sqrtf(norm_sq(v)); }
internal f32 norm(const vf3& v) { return sqrtf(norm_sq(v)); }
internal f32 norm(const vf4& v) { return sqrtf(norm_sq(v)); }

internal vf2 normalize(const vf2& v) { return v / norm(v); }
internal vf3 normalize(const vf3& v) { return v / norm(v); }
internal vf4 normalize(const vf4& v) { return v / norm(v); }

internal constexpr i32 mod(const i32& x, const i32& m) { return (x % m + m) % m; }
internal           f32 mod(const f32& x, const f32& m) { f32 y = fmodf(x, m); return y < 0.0f ? y + m : y; }

internal f32 atan2(const vf2& v) { return atan2f(v.y, v.x); }

#include <xmmintrin.h>

global constexpr __m128 m_0   = {     0.0f,     0.0f,     0.0f,     0.0f };
global constexpr __m128 m_1   = {     1.0f,     1.0f,     1.0f,     1.0f };
global constexpr __m128 m_2   = {     2.0f,     2.0f,     2.0f,     2.0f };
global constexpr __m128 m_3   = {     3.0f,     3.0f,     3.0f,     3.0f };
global constexpr __m128 m_4   = {     4.0f,     4.0f,     4.0f,     4.0f };
global constexpr __m128 m_8   = {     8.0f,     8.0f,     8.0f,     8.0f };
global constexpr __m128 m_255 = {   255.0f,   255.0f,   255.0f,   255.0f };
global constexpr __m128 m_inf = { INFINITY, INFINITY, INFINITY, INFINITY };

internal __m128 square(const __m128& x                                  ) { return _mm_mul_ps(x, x); }
internal __m128 cube  (const __m128& x                                  ) { return _mm_mul_ps(_mm_mul_ps(x, x), x); }
internal __m128 lerp  (const __m128& a, const __m128& b, const __m128& t) { return _mm_add_ps(_mm_mul_ps(a, _mm_sub_ps(m_1, t)), _mm_mul_ps(b, t)); }
internal __m128 clamp (const __m128& x, const __m128& a, const __m128& b) { return _mm_min_ps(_mm_max_ps(x, a), b); }
