#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-template"


#include <stdint.h>
typedef uint8_t        byte;
typedef const char*    strlit;
typedef const wchar_t* wstrlit;
typedef int8_t         i8;
typedef int16_t        i16;
typedef int32_t        i32;
typedef int64_t        i64;
typedef uint8_t        u8;
typedef uint16_t       u16;
typedef uint32_t       u32;
typedef uint64_t       u64;
typedef uint8_t        bool8;
typedef uint16_t       bool16;
typedef uint32_t       bool32;
typedef uint64_t       bool64;
typedef float          f32;
typedef double         f64;

#define procedure static
#define global    static
#define aliasing  auto&
#define lambda    const auto

#define MACRO_CONCAT_(X, Y)                             X##Y
#define MACRO_CONCAT(X, Y)                              MACRO_CONCAT_(X, Y)
#define MACRO_STRINGIFY_(X)                             #X
#define MACRO_STRINGIFY(X)                              MACRO_STRINGIFY_(X)
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
#define MACRO_STRINGIFYCOMMA_(X)                        MACRO_STRINGIFY(X),
#define MACRO_STRINGIFYARGS_(...)                       MACRO_FOR_EACH_(MACRO_STRINGIFYCOMMA_, __VA_ARGS__)

#define ARRAY_CAPACITY(XS)                  (sizeof(XS) / sizeof((XS)[0]))

#define FOR_INTERVAL_(NAME, MINI, MAXI)     for (auto NAME = (MINI); NAME < (MAXI); NAME += 1)
#define FOR_INDICIES_(NAME, MAXI)           FOR_INTERVAL_(NAME, 0, (MAXI))
#define FOR_REPEAT_(MAXI)                   FOR_INTERVAL_(MACRO_CONCAT(FOR_REPEAT_, __LINE__), 0, (MAXI))
#define FOR_RANGE(...)                      MACRO_EXPAND_(MACRO_OVERLOADED_3_(__VA_ARGS__, FOR_INTERVAL_, FOR_INDICIES_, FOR_REPEAT_)(__VA_ARGS__))
#define FOR_INTERVAL_REV_(NAME, MINI, MAXI) for (i64 NAME = (MAXI) - 1; NAME >= (MINI); NAME -= 1)
#define FOR_INDICIES_REV_(NAME, MAXI)       FOR_INTERVAL_REV_(NAME, 0, (MAXI))
#define FOR_RANGE_REV(...)                  MACRO_EXPAND_(MACRO_OVERLOADED_3_(__VA_ARGS__, FOR_INTERVAL_REV_, FOR_INDICIES_REV_)(__VA_ARGS__))

#define FOR_POINTER_(NAME, XS, COUNT)\
for (i64 MACRO_CONCAT(NAME, _index) = 0; MACRO_CONCAT(NAME, _index) < static_cast<i64>(COUNT); MACRO_CONCAT(NAME, _index) += 1)\
	if (const auto NAME = &(XS)[MACRO_CONCAT(NAME, _index)]; false); else
#define FOR_ARRAY_(NAME, XS)                FOR_POINTER_(NAME, (XS), ARRAY_CAPACITY(XS))
#define FOR_IT_(XS)                         FOR_POINTER_(it, (XS), ARRAY_CAPACITY(XS))
#define FOR_ELEMS(...)                      MACRO_EXPAND_(MACRO_OVERLOADED_3_(__VA_ARGS__, FOR_POINTER_, FOR_ARRAY_, FOR_IT_)(__VA_ARGS__))

#define FOR_POINTER_REV_(NAME, XS, COUNT)   for (i64 MACRO_CONCAT(NAME, _index) = static_cast<i64>(COUNT) - 1; MACRO_CONCAT(NAME, _index) >= 0; MACRO_CONCAT(NAME, _index) -= 1) if (const auto NAME = &(XS)[MACRO_CONCAT(NAME, _index)]; false); else
#define FOR_ARRAY_REV_(NAME, XS)            FOR_POINTER_REV_(NAME, (XS), ARRAY_CAPACITY(XS))
#define FOR_ELEMS_REV(...)                  MACRO_EXPAND_(MACRO_OVERLOADED_3_(__VA_ARGS__, FOR_POINTER_REV_, FOR_ARRAY_REV_)(__VA_ARGS__))

#define FOR_NODES_(NAME, NODES)\
if (auto MACRO_CONCAT(NAME, __LINE__) = (NODES); false); else\
	for (i32 MACRO_CONCAT(NAME, _index) = 0; (void) MACRO_CONCAT(NAME, _index), MACRO_CONCAT(NAME, __LINE__); MACRO_CONCAT(NAME, _index) += 1, MACRO_CONCAT(NAME, __LINE__) = MACRO_CONCAT(NAME, __LINE__)->next)\
		if (const auto NAME = MACRO_CONCAT(NAME, __LINE__))
#define FOR_IT_NODES_(NODES)                FOR_NODES_(it, (NODES))
#define FOR_NODES(...)                      MACRO_EXPAND_(MACRO_OVERLOADED_2_(__VA_ARGS__, FOR_NODES_, FOR_IT_NODES_)(__VA_ARGS__))

#define FOR_STRING_(NAME, STR)              for (u64 MACRO_CONCAT(NAME, _index) = 0; MACRO_CONCAT(NAME, _index) < (STR).size; MACRO_CONCAT(NAME, _index) += 1) if (const auto NAME = &(STR).data[MACRO_CONCAT(NAME, _index)]; false); else
#define FOR_STR_(STR)                       FOR_STRING_(it, STR)
#define FOR_STR(...)                        MACRO_EXPAND_(MACRO_OVERLOADED_2_(__VA_ARGS__, FOR_STRING_, FOR_STR_)(__VA_ARGS__))
#define FOR_STRING_REV_(NAME, STR)          for (u64 MACRO_CONCAT(NAME, _index) = (STR).size; MACRO_CONCAT(NAME, _index)-- > 0;) if (const auto NAME = &(STR).data[MACRO_CONCAT(NAME, _index)]; false); else
#define FOR_STR_REV_(STR)                   FOR_STRING_REV_(it, STR)
#define FOR_STR_REV(...)                    MACRO_EXPAND_(MACRO_OVERLOADED_2_(__VA_ARGS__, FOR_STRING_REV_, FOR_STR_REV_)(__VA_ARGS__))

#define IMPLIES(P, Q)                       (!(P) || (Q))
#define IFF(P, Q)                           (!(P) == !(Q))
#define IN_RANGE(X, MINI, MAXI)             ((MINI) <= (X) && (X) < (MAXI))
#define SWAP(X, Y)                          do { auto MACRO_CONCAT(SWAP_, __LINE__) = *(X); *(X) = *(Y); *(Y) = MACRO_CONCAT(SWAP_, __LINE__); } while (false)
#define KIBIBYTES_OF(N)                     (1024ull *             (N))
#define MEBIBYTES_OF(N)                     (1024ull * KIBIBYTES_OF(N))
#define GIBIBYTES_OF(N)                     (1024ull * MEBIBYTES_OF(N))
#define TEBIBYTES_OF(N)                     (1024ull * GIBIBYTES_OF(N))

#if DEBUG
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wglobal-constructors"

	#include <stdio.h>
	#include <Windows.h>
	#include <signal.h>
	#undef interface
	#undef min
	#undef max

	#define DEBUG_persist static

	#define DEBUG_HALT()       __debugbreak()
	#define ASSERT(EXPRESSION) do { if (!(EXPRESSION)) { DEBUG_HALT(); abort(); } } while (false)

	#define DEBUG_printf(FSTR, ...)\
	do\
	{\
		char MACRO_CONCAT(DEBUG_PRINTF_, __LINE__)[4096];\
		sprintf_s(MACRO_CONCAT(DEBUG_PRINTF_, __LINE__), sizeof(MACRO_CONCAT(DEBUG_PRINTF_, __LINE__)), (FSTR), __VA_ARGS__);\
		OutputDebugStringA(MACRO_CONCAT(DEBUG_PRINTF_, __LINE__));\
	}\
	while (false)

	#define DEBUG_print_bits(N)\
	do\
	{\
		FOR_RANGE_REV(i, sizeof(N) * 8)\
		{\
			DEBUG_printf("%d", ((N) >> i) & 0b1);\
		}\
	}\
	while (false)

	#define DEBUG_once\
	for (DEBUG_persist bool32 MACRO_CONCAT(DEBUG_ONCE_, __LINE__) = true; MACRO_CONCAT(DEBUG_ONCE_, __LINE__); MACRO_CONCAT(DEBUG_ONCE_, __LINE__) = false)

	global const i64 DEBUG_PERFORMANCE_COUNTER_FREQUENCY =
		[](void)
		{
			LARGE_INTEGER n;
			QueryPerformanceFrequency(&n);
			return n.QuadPart;
		}();

	#define DEBUG_PROFILER_start(NAME)\
	LARGE_INTEGER MACRO_CONCAT(NAME, _LI_0_);\
	QueryPerformanceCounter(&MACRO_CONCAT(NAME, _LI_0_));\
	u64 MACRO_CONCAT(NAME, _CC_0_) = __rdtsc()

	#define DEBUG_PROFILER_end(NAME, SAMPLES)\
	u64 MACRO_CONCAT(NAME, _CC_1_) = __rdtsc();\
	LARGE_INTEGER MACRO_CONCAT(NAME, _LI_1_);\
	QueryPerformanceCounter(&MACRO_CONCAT(NAME, _LI_1_));\
	DEBUG_persist u64 MACRO_CONCAT(NAME, _LI_TOTAL_) = 0;\
	DEBUG_persist u64 MACRO_CONCAT(NAME, _CC_TOTAL_) = 0;\
	DEBUG_persist u64 MACRO_CONCAT(NAME, _COUNTER_)  = 0;\
	MACRO_CONCAT(NAME, _LI_TOTAL_)   += MACRO_CONCAT(NAME, _LI_1_).QuadPart - MACRO_CONCAT(NAME, _LI_0_).QuadPart;\
	MACRO_CONCAT(NAME, _CC_TOTAL_)   += MACRO_CONCAT(NAME, _CC_1_)          - MACRO_CONCAT(NAME, _CC_0_);\
	MACRO_CONCAT(NAME, _COUNTER_) += 1;\
	if (MACRO_CONCAT(NAME, _COUNTER_) >= (SAMPLES))\
	{\
		DEBUG_printf(MACRO_STRINGIFY(NAME) "\n\t:: %8.4f ms\n\t:: %8.4f mc\n", static_cast<f64>(MACRO_CONCAT(NAME, _LI_TOTAL_)) / MACRO_CONCAT(NAME, _COUNTER_) / DEBUG_PERFORMANCE_COUNTER_FREQUENCY * 1000.0, static_cast<f64>(MACRO_CONCAT(NAME, _CC_TOTAL_)) / MACRO_CONCAT(NAME, _COUNTER_) / 1000000.0);\
		MACRO_CONCAT(NAME, _LI_TOTAL_) = 0;\
		MACRO_CONCAT(NAME, _CC_TOTAL_) = 0;\
		MACRO_CONCAT(NAME, _COUNTER_)  = 0;\
	}

	#define DEBUG_STDOUT_HALT()\
	do\
	{\
		printf\
		(\
			"======================== DEBUG_STDOUT_HALT ========================\n"\
			"A stdout halt occured from file `" __FILE__ "` on line " MACRO_STRINGIFY(__LINE__) ".\n"\
			"===================================================================\n"\
		);\
		fgetc(stdin);\
	}\
	while (false)

	#define DEBUG_WIN32_ERR()\
	do\
	{\
		DWORD DEBUG_REPORT_WIN32_ERR = GetLastError();\
		if (DEBUG_REPORT_WIN32_ERR)\
		{\
			wchar_t* DEBUG_REPORT_WIN32_ERR_BUFFER;\
			ASSERT(FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, 0, DEBUG_REPORT_WIN32_ERR, 0, reinterpret_cast<wchar_t*>(&DEBUG_REPORT_WIN32_ERR_BUFFER), 0, 0));\
			DEBUG_printf(__FILE__ " :: Win32 Error Code `%lu`.\n\t%S", DEBUG_REPORT_WIN32_ERR, DEBUG_REPORT_WIN32_ERR_BUFFER);\
			LocalFree(DEBUG_REPORT_WIN32_ERR_BUFFER);\
		}\
	}\
	while (false)
	#pragma clang diagnostic pop
#else
	#define ASSERT(EXPRESSION)
	#define DEBUG_HALT()
	#define DEBUG_printf(FSTR, ...)
	#define DEBUG_once                       if (true); else
	#define DEBUG_PROFILER_create_group(...)
	#define DEBUG_PROFILER_flush_group(...)
	#define DEBUG_PROFILER_start(...)
	#define DEBUG_PROFILER_end(...)
	#define DEBUG_STDOUT_HALT()
#endif

#define flag_struct(NAME, TYPE)\
enum struct NAME : TYPE;\
procedure constexpr TYPE operator+  (const NAME& a               ) { return     static_cast<TYPE>(a);            }\
procedure constexpr NAME operator&  (const NAME& a, const NAME& b) { return     static_cast<NAME>( (+a) & (+b)); }\
procedure constexpr NAME operator|  (const NAME& a, const NAME& b) { return     static_cast<NAME>( (+a) | (+b)); }\
procedure constexpr NAME operator^  (const NAME& a, const NAME& b) { return     static_cast<NAME>( (+a) ^ (+b)); }\
procedure constexpr NAME operator<< (const NAME& a, const i32&  n) { return     static_cast<NAME>( (+a) << n  ); }\
procedure constexpr NAME operator>> (const NAME& a, const i32&  n) { return     static_cast<NAME>( (+a) >> n  ); }\
procedure constexpr NAME operator~  (const NAME& a               ) { return     static_cast<NAME>( ~+a        ); }\
procedure constexpr NAME operator&= (      NAME& a, const NAME& b) { return a = static_cast<NAME>( (+a) & (+b)); }\
procedure constexpr NAME operator|= (      NAME& a, const NAME& b) { return a = static_cast<NAME>( (+a) | (+b)); }\
procedure constexpr NAME operator^= (      NAME& a, const NAME& b) { return a = static_cast<NAME>( (+a) ^ (+b)); }\
procedure constexpr NAME operator<<=(      NAME& a, const i32&  n) { return a = static_cast<NAME>( (+a) << n  ); }\
procedure constexpr NAME operator>>=(      NAME& a, const i32&  n) { return a = static_cast<NAME>( (+a) >> n  ); }\
enum struct NAME : TYPE

#include <utility>
#define DEFER auto MACRO_CONCAT(DEFER_, __LINE__) = DEFER_EMPTY_ {} + [&]()

template <typename F>
struct DEFER_
{
	F f;
	DEFER_(F f_) : f(f_) {}
	~DEFER_() { f(); }
};

struct DEFER_EMPTY_ {};

template <typename F>
procedure DEFER_<F> operator+(DEFER_EMPTY_, F&& f)
{
	return DEFER_<F>(std::forward<F>(f));
}

//
// Memory.
//

#define deferred_arena_reset(ARENA) u64 MACRO_CONCAT(ARENA_CHECKPOINT_, __LINE__) = (ARENA)->used; DEFER { (ARENA)->used = MACRO_CONCAT(ARENA_CHECKPOINT_, __LINE__); }

struct MemoryArena
{
	u64   used;
	u64   size;
	byte* data;
};

template <typename TYPE>
procedure TYPE* allocate(MemoryArena* arena, const u64& count = 1)
{
	if (arena && arena->data && sizeof(TYPE) * count <= arena->size - arena->used)
	{
		arena->used += sizeof(TYPE) * count;
		return reinterpret_cast<TYPE*>(arena->data + arena->size - arena->used);
	}
	else
	{
		return {};
	}
}

//
// Math.
//

#include <algorithm>
using std::min;
using std::max;
using std::clamp;

#define PASS_V2(V) (V).x, (V).y
#define PASS_V3(V) (V).x, (V).y, (V).z
#define PASS_V4(V) (V).x, (V).y, (V).z, (V).w

global constexpr f32 TAU   = 6.28318530717f;
global constexpr f32 SQRT2 = 1.41421356237f;

struct vf2
{
	union
	{
		struct { f32 x; f32 y; };
		f32 components[2];
	};
};

struct vf3
{
	union
	{
		struct { f32 x; f32 y; f32 z; };
		vf2 xy;
		f32 components[3];
	};
};

struct vf4
{
	union
	{
		struct { f32 x; f32 y; f32 z; f32 w; };
		vf3 xyz;
		vf2 xy;
		f32 components[4];
	};
};

procedure constexpr bool32 operator+ (const vf2& v              ) { return v.x != 0.0f || v.y != 0.0f;                               }
procedure constexpr bool32 operator+ (const vf3& v              ) { return v.x != 0.0f || v.y != 0.0f || v.z != 0.0f;                }
procedure constexpr bool32 operator+ (const vf4& v              ) { return v.x != 0.0f || v.y != 0.0f || v.z != 0.0f || v.w != 0.0f; }
procedure constexpr vf2    operator- (const vf2& v              ) { return { -v.x, -v.y             }; }
procedure constexpr vf3    operator- (const vf3& v              ) { return { -v.x, -v.y, -v.z       }; }
procedure constexpr vf4    operator- (const vf4& v              ) { return { -v.x, -v.y, -v.z, -v.w }; }
procedure constexpr vf2    operator+ (const vf2& u, const vf2& v) { return { u.x + v.x, u.y + v.y                       }; }
procedure constexpr vf3    operator+ (const vf3& u, const vf3& v) { return { u.x + v.x, u.y + v.y, u.z + v.z            }; }
procedure constexpr vf4    operator+ (const vf4& u, const vf4& v) { return { u.x + v.x, u.y + v.y, u.z + v.z, u.w + v.w }; }
procedure constexpr vf2    operator- (const vf2& u, const vf2& v) { return { u.x - v.x, u.y - v.y                       }; }
procedure constexpr vf3    operator- (const vf3& u, const vf3& v) { return { u.x - v.x, u.y - v.y, u.z - v.z            }; }
procedure constexpr vf4    operator- (const vf4& u, const vf4& v) { return { u.x - v.x, u.y - v.y, u.z - v.z, u.w - v.w }; }
procedure constexpr vf2    operator/ (const vf2& v, const f32& k) { return { v.x / k, v.y / k                   }; }
procedure constexpr vf3    operator/ (const vf3& v, const f32& k) { return { v.x / k, v.y / k, v.z / k          }; }
procedure constexpr vf4    operator/ (const vf4& v, const f32& k) { return { v.x / k, v.y / k, v.z / k, v.w / k }; }
procedure constexpr vf2    operator* (const vf2& u, const vf2& v) { return { u.x * v.x, u.y * v.y                       }; }
procedure constexpr vf3    operator* (const vf3& u, const vf3& v) { return { u.x * v.x, u.y * v.y, u.z * v.z            }; }
procedure constexpr vf4    operator* (const vf4& u, const vf4& v) { return { u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w }; }
procedure constexpr vf2    operator* (const vf2& v, const f32& k) { return { v.x * k, v.y * k                   }; }
procedure constexpr vf3    operator* (const vf3& v, const f32& k) { return { v.x * k, v.y * k, v.z * k          }; }
procedure constexpr vf4    operator* (const vf4& v, const f32& k) { return { v.x * k, v.y * k, v.z * k, v.w * k }; }
procedure constexpr vf2    operator* (const f32& k, const vf2& v) { return v * k; }
procedure constexpr vf3    operator* (const f32& k, const vf3& v) { return v * k; }
procedure constexpr vf4    operator* (const f32& k, const vf4& v) { return v * k; }
procedure constexpr vf2&   operator+=(      vf2& u, const vf2& v) { return u = u + v; }
procedure constexpr vf3&   operator+=(      vf3& u, const vf3& v) { return u = u + v; }
procedure constexpr vf4&   operator+=(      vf4& u, const vf4& v) { return u = u + v; }
procedure constexpr vf2&   operator-=(      vf2& u, const vf2& v) { return u = u - v; }
procedure constexpr vf3&   operator-=(      vf3& u, const vf3& v) { return u = u - v; }
procedure constexpr vf4&   operator-=(      vf4& u, const vf4& v) { return u = u - v; }
procedure constexpr vf2&   operator*=(      vf2& u, const vf2& v) { return u = u * v; }
procedure constexpr vf3&   operator*=(      vf3& u, const vf3& v) { return u = u * v; }
procedure constexpr vf4&   operator*=(      vf4& u, const vf4& v) { return u = u * v; }
procedure constexpr vf2&   operator*=(      vf2& v, const f32& k) { return v = v * k; }
procedure constexpr vf3&   operator*=(      vf3& v, const f32& k) { return v = v * k; }
procedure constexpr vf4&   operator*=(      vf4& v, const f32& k) { return v = v * k; }
procedure constexpr vf2&   operator/=(      vf2& v, const f32& k) { return v = v / k; }
procedure constexpr vf3&   operator/=(      vf3& v, const f32& k) { return v = v / k; }
procedure constexpr vf4&   operator/=(      vf4& v, const f32& k) { return v = v / k; }

struct vi2
{
	union
	{
		struct { i32 x; i32 y; };
		i32 components[2];
	};
};

struct vi3
{
	union
	{
		struct { i32 x; i32 y; i32 z; };
		vi2 xy;
		i32 components[3];
	};
};

struct vi4
{
	union
	{
		struct { i32 x; i32 y; i32 z; i32 w; };
		vi3 xyz;
		vi2 xy;
		i32 components[4];
	};
};

procedure constexpr bool32 operator+ (const vi2& v              ) { return v.x || v.y;               }
procedure constexpr bool32 operator+ (const vi3& v              ) { return v.x || v.y || v.z;        }
procedure constexpr bool32 operator+ (const vi4& v              ) { return v.x || v.y || v.z || v.w; }
procedure constexpr vi2    operator- (const vi2& v              ) { return { -v.x, -v.y             }; }
procedure constexpr vi3    operator- (const vi3& v              ) { return { -v.x, -v.y, -v.z       }; }
procedure constexpr vi4    operator- (const vi4& v              ) { return { -v.x, -v.y, -v.z, -v.w }; }
procedure constexpr bool32 operator==(const vi2& u, const vi2& v) { return u.x == v.x && u.y == v.y;                             }
procedure constexpr bool32 operator==(const vi3& u, const vi3& v) { return u.x == v.x && u.y == v.y && u.z == v.z;               }
procedure constexpr bool32 operator==(const vi4& u, const vi4& v) { return u.x == v.x && u.y == v.y && u.z == v.z && u.w == v.w; }
procedure constexpr bool32 operator!=(const vi2& u, const vi2& v) { return !(u == v); }
procedure constexpr bool32 operator!=(const vi3& u, const vi3& v) { return !(u == v); }
procedure constexpr bool32 operator!=(const vi4& u, const vi4& v) { return !(u == v); }
procedure constexpr vi2    operator+ (const vi2& u, const vi2& v) { return { u.x + v.x, u.y + v.y                       }; }
procedure constexpr vi3    operator+ (const vi3& u, const vi3& v) { return { u.x + v.x, u.y + v.y, u.z + v.z            }; }
procedure constexpr vi4    operator+ (const vi4& u, const vi4& v) { return { u.x + v.x, u.y + v.y, u.z + v.z, u.w + v.w }; }
procedure constexpr vi2    operator- (const vi2& u, const vi2& v) { return { u.x - v.x, u.y - v.y                       }; }
procedure constexpr vi3    operator- (const vi3& u, const vi3& v) { return { u.x - v.x, u.y - v.y, u.z - v.z            }; }
procedure constexpr vi4    operator- (const vi4& u, const vi4& v) { return { u.x - v.x, u.y - v.y, u.z - v.z, u.w - v.w }; }
procedure constexpr vi2    operator/ (const vi2& v, const i32& k) { return { v.x / k, v.y / k                   }; }
procedure constexpr vi3    operator/ (const vi3& v, const i32& k) { return { v.x / k, v.y / k, v.z / k          }; }
procedure constexpr vi4    operator/ (const vi4& v, const i32& k) { return { v.x / k, v.y / k, v.z / k, v.w / k }; }
procedure constexpr vi2    operator* (const vi2& u, const vi2& v) { return { u.x * v.x, u.y * v.y                       }; }
procedure constexpr vi3    operator* (const vi3& u, const vi3& v) { return { u.x * v.x, u.y * v.y, u.z * v.z            }; }
procedure constexpr vi4    operator* (const vi4& u, const vi4& v) { return { u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w }; }
procedure constexpr vi2    operator* (const vi2& v, const i32& k) { return { v.x * k, v.y * k                   }; }
procedure constexpr vi3    operator* (const vi3& v, const i32& k) { return { v.x * k, v.y * k, v.z * k          }; }
procedure constexpr vi4    operator* (const vi4& v, const i32& k) { return { v.x * k, v.y * k, v.z * k, v.w * k }; }
procedure constexpr vi2    operator* (const i32& k, const vi2& v) { return v * k; }
procedure constexpr vi3    operator* (const i32& k, const vi3& v) { return v * k; }
procedure constexpr vi4    operator* (const i32& k, const vi4& v) { return v * k; }
procedure constexpr vi2&   operator+=(      vi2& u, const vi2& v) { return u = u + v; }
procedure constexpr vi3&   operator+=(      vi3& u, const vi3& v) { return u = u + v; }
procedure constexpr vi4&   operator+=(      vi4& u, const vi4& v) { return u = u + v; }
procedure constexpr vi2&   operator-=(      vi2& u, const vi2& v) { return u = u - v; }
procedure constexpr vi3&   operator-=(      vi3& u, const vi3& v) { return u = u - v; }
procedure constexpr vi4&   operator-=(      vi4& u, const vi4& v) { return u = u - v; }
procedure constexpr vi2&   operator*=(      vi2& u, const vi2& v) { return u = u * v; }
procedure constexpr vi3&   operator*=(      vi3& u, const vi3& v) { return u = u * v; }
procedure constexpr vi4&   operator*=(      vi4& u, const vi4& v) { return u = u * v; }
procedure constexpr vi2&   operator*=(      vi2& v, const i32& k) { return v = v * k; }
procedure constexpr vi3&   operator*=(      vi3& v, const i32& k) { return v = v * k; }
procedure constexpr vi4&   operator*=(      vi4& v, const i32& k) { return v = v * k; }
procedure constexpr vi2&   operator/=(      vi2& v, const i32& k) { return v = v / k; }
procedure constexpr vi3&   operator/=(      vi3& v, const i32& k) { return v = v / k; }
procedure constexpr vi4&   operator/=(      vi4& v, const i32& k) { return v = v / k; }

procedure constexpr vf2    operator+ (const vf2& u, const vi2& v) { return { u.x + static_cast<f32>(v.x), u.y + static_cast<f32>(v.y)                                                           }; }
procedure constexpr vf3    operator+ (const vf3& u, const vi3& v) { return { u.x + static_cast<f32>(v.x), u.y + static_cast<f32>(v.y), u.z + static_cast<f32>(v.z)                              }; }
procedure constexpr vf4    operator+ (const vf4& u, const vi4& v) { return { u.x + static_cast<f32>(v.x), u.y + static_cast<f32>(v.y), u.z + static_cast<f32>(v.z), u.w + static_cast<f32>(v.w) }; }
procedure constexpr vf2    operator+ (const vi2& u, const vf2& v) { return v + u; }
procedure constexpr vf3    operator+ (const vi3& u, const vf3& v) { return v + u; }
procedure constexpr vf4    operator+ (const vi4& u, const vf4& v) { return v + u; }
procedure constexpr vf2    operator- (const vf2& u, const vi2& v) { return { u.x - static_cast<f32>(v.x), u.y - static_cast<f32>(v.y)                                                           }; }
procedure constexpr vf3    operator- (const vf3& u, const vi3& v) { return { u.x - static_cast<f32>(v.x), u.y - static_cast<f32>(v.y), u.z - static_cast<f32>(v.z)                              }; }
procedure constexpr vf4    operator- (const vf4& u, const vi4& v) { return { u.x - static_cast<f32>(v.x), u.y - static_cast<f32>(v.y), u.z - static_cast<f32>(v.z), u.w - static_cast<f32>(v.w) }; }
procedure constexpr vf2    operator- (const vi2& u, const vf2& v) { return { static_cast<f32>(u.x) - v.x, static_cast<f32>(u.y) - v.y                                                           }; }
procedure constexpr vf3    operator- (const vi3& u, const vf3& v) { return { static_cast<f32>(u.x) - v.x, static_cast<f32>(u.y) - v.y, static_cast<f32>(u.z) - v.z                              }; }
procedure constexpr vf4    operator- (const vi4& u, const vf4& v) { return { static_cast<f32>(u.x) - v.x, static_cast<f32>(u.y) - v.y, static_cast<f32>(u.z) - v.z, static_cast<f32>(u.w) - v.w }; }
procedure constexpr vf2    operator/ (const vf2& v, const i32& k) { return { v.x / k, v.y / k                   }; }
procedure constexpr vf3    operator/ (const vf3& v, const i32& k) { return { v.x / k, v.y / k, v.z / k          }; }
procedure constexpr vf4    operator/ (const vf4& v, const i32& k) { return { v.x / k, v.y / k, v.z / k, v.w / k }; }
procedure constexpr vf2    operator/ (const vi2& v, const f32& k) { return { v.x / k, v.y / k                   }; }
procedure constexpr vf3    operator/ (const vi3& v, const f32& k) { return { v.x / k, v.y / k, v.z / k          }; }
procedure constexpr vf4    operator/ (const vi4& v, const f32& k) { return { v.x / k, v.y / k, v.z / k, v.w / k }; }
procedure constexpr vf2    operator/ (const vf2& u, const vi2& v) { return { u.x / v.x, u.y / v.y                       }; }
procedure constexpr vf3    operator/ (const vf3& u, const vi3& v) { return { u.x / v.x, u.y / v.y, u.z / v.z            }; }
procedure constexpr vf4    operator/ (const vf4& u, const vi4& v) { return { u.x / v.x, u.y / v.y, u.z / v.z, u.w / v.w }; }
procedure constexpr vf2    operator/ (const vi2& u, const vf2& v) { return { u.x / v.x, u.y / v.y                       }; }
procedure constexpr vf3    operator/ (const vi3& u, const vf3& v) { return { u.x / v.x, u.y / v.y, u.z / v.z            }; }
procedure constexpr vf4    operator/ (const vi4& u, const vf4& v) { return { u.x / v.x, u.y / v.y, u.z / v.z, u.w / v.w }; }
procedure constexpr vf2    operator* (const vf2& v, const i32& k) { return { v.x * k, v.y * k                   }; }
procedure constexpr vf3    operator* (const vf3& v, const i32& k) { return { v.x * k, v.y * k, v.z * k          }; }
procedure constexpr vf4    operator* (const vf4& v, const i32& k) { return { v.x * k, v.y * k, v.z * k, v.w * k }; }
procedure constexpr vf2    operator* (const vi2& v, const f32& k) { return { v.x * k, v.y * k                   }; }
procedure constexpr vf3    operator* (const vi3& v, const f32& k) { return { v.x * k, v.y * k, v.z * k          }; }
procedure constexpr vf4    operator* (const vi4& v, const f32& k) { return { v.x * k, v.y * k, v.z * k, v.w * k }; }
procedure constexpr vf2    operator* (const vf2& u, const vi2& v) { return { u.x * v.x, u.y * v.y                       }; }
procedure constexpr vf3    operator* (const vf3& u, const vi3& v) { return { u.x * v.x, u.y * v.y, u.z * v.z            }; }
procedure constexpr vf4    operator* (const vf4& u, const vi4& v) { return { u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w }; }
procedure constexpr vf2    operator* (const vi2& u, const vf2& v) { return { u.x * v.x, u.y * v.y                       }; }
procedure constexpr vf3    operator* (const vi3& u, const vf3& v) { return { u.x * v.x, u.y * v.y, u.z * v.z            }; }
procedure constexpr vf4    operator* (const vi4& u, const vf4& v) { return { u.x * v.x, u.y * v.y, u.z * v.z, u.w * v.w }; }
procedure constexpr vf2    operator* (const f32& k, const vi2& v) { return v * k; }
procedure constexpr vf3    operator* (const f32& k, const vi3& v) { return v * k; }
procedure constexpr vf4    operator* (const f32& k, const vi4& v) { return v * k; }
procedure constexpr vf2&   operator+=(      vf2& u, const vi2& v) { return u = u + v; }
procedure constexpr vf3&   operator+=(      vf3& u, const vi3& v) { return u = u + v; }
procedure constexpr vf4&   operator+=(      vf4& u, const vi4& v) { return u = u + v; }
procedure constexpr vf2&   operator-=(      vf2& u, const vi2& v) { return u = u - v; }
procedure constexpr vf3&   operator-=(      vf3& u, const vi3& v) { return u = u - v; }
procedure constexpr vf4&   operator-=(      vf4& u, const vi4& v) { return u = u - v; }
procedure constexpr vf2&   operator*=(      vf2& u, const vi2& v) { return u = u * v; }
procedure constexpr vf3&   operator*=(      vf3& u, const vi3& v) { return u = u * v; }
procedure constexpr vf4&   operator*=(      vf4& u, const vi4& v) { return u = u * v; }
procedure constexpr vf2&   operator*=(      vf2& v, const i32& k) { return v = v * k; }
procedure constexpr vf3&   operator*=(      vf3& v, const i32& k) { return v = v * k; }
procedure constexpr vf4&   operator*=(      vf4& v, const i32& k) { return v = v * k; }
procedure constexpr vf2&   operator/=(      vf2& v, const i32& k) { return v = v / k; }
procedure constexpr vf3&   operator/=(      vf3& v, const i32& k) { return v = v / k; }
procedure constexpr vf4&   operator/=(      vf4& v, const i32& k) { return v = v / k; }

procedure constexpr vf2    vxx(const vi2& v                                          ) { return { static_cast<f32>(v.x), static_cast<f32>(v.y)                                               }; }
procedure constexpr vi2    vxx(const vf2& v                                          ) { return { static_cast<i32>(v.x), static_cast<i32>(v.y)                                               }; }
procedure constexpr vf3    vxx(const vi3& v                                          ) { return { static_cast<f32>(v.x), static_cast<f32>(v.y), static_cast<f32>(v.z)                        }; }
procedure constexpr vi3    vxx(const vf3& v                                          ) { return { static_cast<i32>(v.x), static_cast<i32>(v.y), static_cast<i32>(v.z)                        }; }
procedure constexpr vf4    vxx(const vi4& v                                          ) { return { static_cast<f32>(v.x), static_cast<f32>(v.y), static_cast<f32>(v.z), static_cast<f32>(v.w) }; }
procedure constexpr vi4    vxx(const vf4& v                                          ) { return { static_cast<i32>(v.x), static_cast<i32>(v.y), static_cast<i32>(v.z), static_cast<i32>(v.w) }; }
procedure constexpr vf2    vxx(const i32& x, const i32& y                            ) { return { static_cast<f32>(  x), static_cast<f32>(  y)                                               }; }
procedure constexpr vi2    vxx(const f32& x, const f32& y                            ) { return { static_cast<i32>(  x), static_cast<i32>(  y)                                               }; }
procedure constexpr vf3    vxx(const i32& x, const i32& y, const i32& z              ) { return { static_cast<f32>(  x), static_cast<f32>(  y), static_cast<f32>(  z)                        }; }
procedure constexpr vi3    vxx(const f32& x, const f32& y, const f32& z              ) { return { static_cast<i32>(  x), static_cast<i32>(  y), static_cast<i32>(  z)                        }; }
procedure constexpr vf4    vxx(const i32& x, const i32& y, const i32& z, const i32& w) { return { static_cast<f32>(  x), static_cast<f32>(  y), static_cast<f32>(  z), static_cast<f32>(  w) }; }
procedure constexpr vi4    vxx(const f32& x, const f32& y, const f32& z, const f32& w) { return { static_cast<i32>(  x), static_cast<i32>(  y), static_cast<i32>(  z), static_cast<i32>(  w) }; }

procedure constexpr vf3    vxn(const vf2& v,               const f32& z              ) { return { v.x, v.y,   z      }; }
procedure constexpr vi3    vxn(const vi2& v,               const i32& z              ) { return { v.x, v.y,   z      }; }
procedure constexpr vf4    vxn(const vf2& v,               const f32& z, const f32& w) { return { v.x, v.y,   z,   w }; }
procedure constexpr vi4    vxn(const vi2& v,               const i32& z, const i32& w) { return { v.x, v.y,   z,   w }; }
procedure constexpr vf4    vxn(const vf3& v,                             const f32& w) { return { v.x, v.y, v.z,   w }; }
procedure constexpr vi4    vxn(const vi3& v,                             const i32& w) { return { v.x, v.y, v.z,   w }; }

procedure constexpr vf2    vx2(const f32& n) { return { n, n       }; }
procedure constexpr vi2    vx2(const i32& n) { return { n, n       }; }
procedure constexpr vf3    vx3(const f32& n) { return { n, n, n    }; }
procedure constexpr vi3    vx3(const i32& n) { return { n, n, n    }; }
procedure constexpr vf4    vx4(const f32& n) { return { n, n, n, n }; }
procedure constexpr vi4    vx4(const i32& n) { return { n, n, n, n }; }

template <typename TYPE>
procedure constexpr TYPE sign(const TYPE& x)
{
	return (static_cast<TYPE>(0) < x) - (x < static_cast<TYPE>(0));
}

template <typename TYPE>
procedure constexpr TYPE square(const TYPE& x)
{
	return x * x;
}

procedure constexpr f32 lerp(const f32& a, const f32& b, const f32& t) { return a * (1.0f - t) + b * t; }
procedure constexpr vf2 lerp(const vf2& a, const vf2& b, const f32& t) { return a * (1.0f - t) + b * t; }
procedure constexpr vf3 lerp(const vf3& a, const vf3& b, const f32& t) { return a * (1.0f - t) + b * t; }
procedure constexpr vf4 lerp(const vf4& a, const vf4& b, const f32& t) { return a * (1.0f - t) + b * t; }

procedure constexpr vf2 conjugate(const vf2& v) { return {  v.x, -v.y }; }
procedure constexpr vi2 conjugate(const vi2& v) { return {  v.x, -v.y }; }
procedure constexpr vf2 rotate90 (const vf2& v) { return { -v.y,  v.x }; }

procedure vf2 polar(const f32& angle) { return { cosf(angle), sinf(angle) }; }

procedure vf2 rotate(const vf2& v, const f32& angle)
{
	vf2 p = polar(angle);
	return { v.x * p.x - v.y * p.y, v.x * p.y + v.y * p.x };
}

procedure f32 dampen(const f32& a, const f32& b, const f32& k, const f32& dt) { return b + (a - b) * powf(k, dt); }
procedure vf2 dampen(const vf2& a, const vf2& b, const f32& k, const f32& dt) { return b + (a - b) * powf(k, dt); }
procedure vf3 dampen(const vf3& a, const vf3& b, const f32& k, const f32& dt) { return b + (a - b) * powf(k, dt); }
procedure vf4 dampen(const vf4& a, const vf4& b, const f32& k, const f32& dt) { return b + (a - b) * powf(k, dt); }

procedure constexpr f32 dot(const vf2& u, const vf2& v) { return u.x * v.x + u.y * v.y;                         }
procedure constexpr f32 dot(const vf3& u, const vf3& v) { return u.x * v.x + u.y * v.y + u.z * v.z;             }
procedure constexpr f32 dot(const vf4& u, const vf4& v) { return u.x * v.x + u.y * v.y + u.z * v.z + u.w * v.w; }

procedure constexpr vf3 cross(const vf3& u, const vf3& v) { return { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x }; }

procedure constexpr f32 norm_sq(const vf2& v) { return v.x * v.x + v.y * v.y;                         }
procedure constexpr f32 norm_sq(const vf3& v) { return v.x * v.x + v.y * v.y + v.z * v.z;             }
procedure constexpr f32 norm_sq(const vf4& v) { return v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w; }

procedure f32 norm(const vf2& v) { return sqrtf(norm_sq(v)); }
procedure f32 norm(const vf3& v) { return sqrtf(norm_sq(v)); }
procedure f32 norm(const vf4& v) { return sqrtf(norm_sq(v)); }

procedure vf2 normalize(const vf2& v) { return v / norm(v); }
procedure vf3 normalize(const vf3& v) { return v / norm(v); }
procedure vf4 normalize(const vf4& v) { return v / norm(v); }

procedure constexpr i32 mod(const i32& x, const i32& m) { return (x % m + m) % m; }
procedure           f32 mod(const f32& x, const f32& m) { f32 y = fmodf(x, m); return y < 0.0f ? y + m : y; }

procedure f32 atan2(const vf2& v) { return atan2f(v.y, v.x); }

//
// Strings.
//

procedure constexpr bool32 is_alpha     (const char& c) { return IN_RANGE(c, 'a', 'z' + 1) || IN_RANGE(c, 'A', 'Z' + 1); }
procedure constexpr bool32 is_digit     (const char& c) { return IN_RANGE(c, '0', '9' + 1); }
procedure constexpr bool32 is_whitespace(const char& c)
{
	switch (c)
	{
		case ' ' :
		case '\t':
		case '\r':
		case '\n': return true;
		default  : return false;
	}
}

procedure constexpr char   uppercase(const char& c) { return IN_RANGE(c, 'a', 'z' + 1) ? c - 'a' + 'A' : c; }
procedure constexpr char   lowercase(const char& c) { return IN_RANGE(c, 'A', 'Z' + 1) ? c - 'A' + 'a' : c; }

#define String(STRLIT) (String { .size = sizeof(STRLIT) - 1, .data = (STRLIT) })
#define PASS_STR(STR)  (STR).size, (STR).data
#define PASS_ISTR(STR) static_cast<i32>((STR).size), (STR).data

struct String
{
	u64         size;
	const char* data;
};

procedure constexpr bool32 operator+ (const String& str                 ) { return str.size && str.data; }
procedure constexpr bool32 operator==(const String& a  , const String& b) { return a.size == b.size && memcmp(a.data, b.data, static_cast<size_t>(a.size)) == 0; }
procedure constexpr bool32 operator!=(const String& a  , const String& b) { return !(a == b); }

procedure constexpr String ltrunc(const String& str, const u64& maximum_length)
{
	return { .size = min(str.size, maximum_length), .data = str.data };
}

procedure constexpr String rtrunc(const String& str, const u64& maximum_length)
{
	return { .size = min(str.size, maximum_length), .data = str.data + str.size - min(str.size, maximum_length) };
}

procedure constexpr String ltrim(const String& str, const u64& offset)
{
	return { str.size - min(offset, str.size), str.data + min(offset, str.size) };
}

procedure constexpr String rtrim(const String& str, const u64& offset)
{
	return { str.size - min(offset, str.size), str.data };
}

procedure constexpr String trim(const String& str, const u64& loffset, const u64& roffset)
{
	return ltrim(rtrim(str, loffset), roffset);
}

procedure constexpr bool32 starts_with(const String& str, const String& prefix)
{
	return ltrunc(str, prefix.size) == prefix;
}

procedure constexpr String ltrim_whitespace(const String& str)
{
	FOR_STR(str)
	{
		if (!is_whitespace(*it))
		{
			return { str.size - it_index, str.data + it_index };
		}
	}
	return { 0, str.data + str.size };
}

procedure constexpr String rtrim_whitespace(const String& str)
{
	FOR_STR_REV(str)
	{
		if (!is_whitespace(*it))
		{
			return { it_index + 1, str.data };
		}
	}
	return { 0, str.data };
}

procedure constexpr String trim_whitespace(const String& str)
{
	return ltrim_whitespace(rtrim_whitespace(str));
}

#pragma clang diagnostic pop
