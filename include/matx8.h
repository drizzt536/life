#pragma once
#define MATX8_H

// defines Matx8 and the Matx8_* API

#include "windows.h" // integer types, Windows API

// NOTE: these trials were before the implementation of the arena. With
//       the arena, I only tested it for single multiplication hashing,
//       but TABLE_LEN = 1 << 9 is still the fastest by a decent margin.

// these were average of 5 I think. maybe 3 though.
// double multiplication speed results for 10^6 nrun trials:
	// 13:   122,945 collisions ( 12.29%) in 6.84 seconds
	// 12:   245,449 collisions ( 24.54%) in 4.24 seconds
	// 11:   486,902 collisions ( 48.69%) in 2.61 seconds
	// 10:   957,293 collisions ( 95.73%) in 2.10 seconds
	//  9: 1,874,358 collisions (187.44%) in 1.58 seconds
	//  8: 3,558,747 collisions (355.87%) in 1.60 seconds
	//  7: 6,482,330 collisions (648.23%) in 1.77 seconds
// single multiplication:
	// 10: 2.06 seconds
	//  9: 1.57 seconds   (4.428sec for 3e+6 trials)
	//  8: 1.57 seconds   (4.494sec for 3e+6 trials)
	//  7: 1.81 seconds
// 512 was the best table size in both cases.

#define FORCE_INLINE inline __attribute__((gnu_inline, always_inline))

typedef union {
	u64 matx;   // the whole matrix as a single integer
	u8 rows[8]; // each row as a separate integer
} Matx8;

// these are ordered from least to most work to compute (important).
// NOTE: these can't be an enum because they have to be integer constant
//       expressions at preproc time, which enum members are not.
#define TFM_IDENTITY	0
#define TFM_YFLIP		1
#define TFM_XFLIP		2
#define TFM_TRS			3
#define TFM_ANTI_TRS	4
#define TFM_ROT180		5
#define TFM_ROT90		6
#define TFM_ROT270		7

#define TFM_IDENTITY_STR	"identity"
#define TFM_YFLIP_STR		"y flip"
#define TFM_XFLIP_STR		"x flip"
#define TFM_TRS_STR			"transpose"
#define TFM_ANTI_TRS_STR	"anti-diagonal transpose"
#define TFM_ROT180_STR		"180 degree rotation"
#define TFM_ROT90_STR		 "90 degree ccw rotation"
#define TFM_ROT270_STR		 "90 degree cw rotation"

static const char *const tfm_strs[] = {
	[TFM_IDENTITY] = TFM_IDENTITY_STR,
	[TFM_YFLIP]    = TFM_YFLIP_STR,
	[TFM_XFLIP]    = TFM_XFLIP_STR,
	[TFM_TRS]      = TFM_TRS_STR,
	[TFM_ANTI_TRS] = TFM_ANTI_TRS_STR,
	[TFM_ROT180]   = TFM_ROT180_STR,
	[TFM_ROT90]    = TFM_ROT90_STR,
	[TFM_ROT270]   = TFM_ROT270_STR,
};

constexpr u8 rbit4_table[16] = {
	0,  8, 4, 12,
	2, 10, 6, 14,
	1,  9, 5, 13,
	3, 11, 7, 15,
};

static FORCE_INLINE u8 rbit8(u8 x) {
	return rbit4_table[x & 15] << 4 | rbit4_table[x >> 4];
}

constexpr u64 Matx8_xroll_masks[8] = {
	0b1111111111111111111111111111111111111111111111111111111111111111llu, // 0, unused
	0b0111111101111111011111110111111101111111011111110111111101111111llu, // 1
	0b0011111100111111001111110011111100111111001111110011111100111111llu, // 2
	0b0001111100011111000111110001111100011111000111110001111100011111llu, // 3
	0b0000111100001111000011110000111100001111000011110000111100001111llu, // 4
	0b0000011100000111000001110000011100000111000001110000011100000111llu, // 5
	0b0000001100000011000000110000001100000011000000110000001100000011llu, // 6
	0b0000000100000001000000010000000100000001000000010000000100000001llu, // 7
};

//////////////////////////////// methods ////////////////////////////////

static FORCE_INLINE u32 Matx8__hash_u(u64 state) {
	return state * 0xff51afd7ed558ccdllu >> 64 - TABLE_BITS;
}

static FORCE_INLINE u64 Matx8__xroll_u(const u64 state, u8 x) {
	// a positive roll rotates right (towards smaller bit indices).
	x &= 7;

	if (x == 0)
		return state;

	const u64 mask = Matx8_xroll_masks[x];

	return (state >> x) & mask | (state << (8 - x)) & ~mask;
}

static FORCE_INLINE u64 Matx8__yroll_u(const u64 state, u8 y) {
	// a positive roll rotates down (towards smaller bit indices).
	return ROR(state, (y & 7) << 3);
}

static Matx8 Matx8__xflip(Matx8 this) {
	// I could also do a transpose, y flip, transpose, but I think that
	// since each line operates on a separate byte, the CPU will likely
	// execute like 2-4 of the lines in parallel.

	this.rows[0] = rbit8(this.rows[0]);
	this.rows[1] = rbit8(this.rows[1]);
	this.rows[2] = rbit8(this.rows[2]);
	this.rows[3] = rbit8(this.rows[3]);
	this.rows[4] = rbit8(this.rows[4]);
	this.rows[5] = rbit8(this.rows[5]);
	this.rows[6] = rbit8(this.rows[6]);
	this.rows[7] = rbit8(this.rows[7]);

	return this;
}

static FORCE_INLINE u64 Matx8__yflip_u(u64 state) {
	return __builtin_bswap64(state);
}

static FORCE_INLINE u64 Matx8__mdtrs_u(u64 state) {
	// main diagonal transpose
	u64 tmp;

	tmp = (state ^ (state >> 7)) & 0x00AA00AA00AA00AAllu;
	state ^= tmp ^ (tmp << 7);

	tmp = (state ^ (state >> 14)) & 0x0000CCCC0000CCCCllu;
	state ^= tmp ^ (tmp << 14);

	tmp = (state ^ (state >> 28)) & 0x00000000F0F0F0F0llu;
	state ^= tmp ^ (tmp << 28);

	return state;
}

static FORCE_INLINE u64 Matx8__adtrs_u(u64 state) {
	// anti-diagonal transpose
	u64 tmp;

	tmp = state ^ (state << 36);
	state ^= (tmp ^ (state >> 36)) & 0xF0F0F0F00F0F0F0Fllu;

	tmp = (state ^ (state << 18)) & 0xCCCC0000CCCC0000llu;
	state ^= tmp ^ (tmp >> 18);

	tmp = (state ^ (state << 9)) & 0xAA00AA00AA00AA00llu;
	state ^= tmp ^ (tmp >> 9);

	return state;
}

static FORCE_INLINE u64 Matx8__rot90_u(u64 state) {
	return Matx8__yflip_u(Matx8__mdtrs_u(state));
}

static FORCE_INLINE u64 Matx8__rot180_u(u64 state) {
	// unfortunately, the matrix xflip has to be defined before the integer one
	// so this has to convert to a matrix and use that one.
	return Matx8__xflip((Matx8) {
		.matx = Matx8__yflip_u(state)
	}).matx;
}

static FORCE_INLINE u64 Matx8__rot270_u(u64 state) {
	return Matx8__yflip_u(Matx8__adtrs_u(state));
}

static u64 Matx8__tfm_u(u64 state, const u8 tfm) {
	_Static_assert(sizeof(tfm_strs) / sizeof(*tfm_strs) == 8,
		"transform count has changed. this code assumes it is 8.");

	switch (tfm) {
	case TFM_IDENTITY: return state;
	case TFM_YFLIP:    return Matx8__yflip_u(state);
	case TFM_XFLIP:    return Matx8__xflip((Matx8) {.matx = state}).matx;
	case TFM_ANTI_TRS: return Matx8__adtrs_u(state);
	case TFM_TRS:      return Matx8__mdtrs_u(state);
	case TFM_ROT180:   return Matx8__rot180_u(state);
	case TFM_ROT90:    return Matx8__rot90_u(state);
	case TFM_ROT270:   return Matx8__rot270_u(state);
	default:
		__builtin_unreachable();
	}
}

// derived functions

static FORCE_INLINE u32 Matx8__hash(const Matx8 this) {
	return Matx8__hash_u(this.matx);
}

static FORCE_INLINE Matx8 Matx8__xroll(const Matx8 this, u8 x) {
	return (Matx8) {.matx = Matx8__xroll_u(this.matx, x)};
}

static FORCE_INLINE Matx8 Matx8__yroll(const Matx8 this, u8 y) {
	return (Matx8) {.matx = Matx8__yroll_u(this.matx, y)};
}

static FORCE_INLINE u64 Matx8__xflip_u(u64 state) {
	return Matx8__xflip((Matx8) {.matx = state}).matx;
}

static FORCE_INLINE Matx8 Matx8__yflip(Matx8 state) {
	return (Matx8) {.matx = Matx8__yflip_u(state.matx)};
}

static FORCE_INLINE Matx8 Matx8__mdtrs(Matx8 this) {
	return (Matx8) {.matx = Matx8__mdtrs_u(this.matx)};
}

static FORCE_INLINE Matx8 Matx8__adtrs(Matx8 this) {
	return (Matx8) {.matx = Matx8__adtrs_u(this.matx)};
}

static FORCE_INLINE Matx8 Matx8__rot90(Matx8 this) {
	return (Matx8) {.matx = Matx8__rot90_u(this.matx)};
}

static FORCE_INLINE Matx8 Matx8__rot180(Matx8 this) {
	return (Matx8) {.matx = Matx8__rot180_u(this.matx)};
}

static FORCE_INLINE Matx8 Matx8__rot270(Matx8 this) {
	return (Matx8) {.matx = Matx8__rot270_u(this.matx)};
}

static FORCE_INLINE Matx8 Matx8__tfm(Matx8 this, const u8 tfm) {
	return (Matx8) {.matx = Matx8__tfm_u(this.matx, tfm)};
}

/////////////////////////// public API methods ///////////////////////////

// technically all the functions are public, but these are the intended API.

#define Matx8_hash(s)     _Generic(s, Matx8: Matx8__hash  , u64: Matx8__hash_u)(s)
#define Matx8_xroll(s, x) _Generic(s, Matx8: Matx8__xroll , u64: Matx8__xroll_u)(s, x)
#define Matx8_yroll(s, y) _Generic(s, Matx8: Matx8__yroll , u64: Matx8__yroll_u)(s, y)
#define Matx8_xflip(s)    _Generic(s, Matx8: Matx8__xflip , u64: Matx8__xflip_u)(s)
#define Matx8_yflip(s)    _Generic(s, Matx8: Matx8__yflip , u64: Matx8__yflip_u)(s)
#define Matx8_mdtrs(s)    _Generic(s, Matx8: Matx8__mdtrs , u64: Matx8__mdtrs_u)(s)
#define Matx8_adtrs(s)    _Generic(s, Matx8: Matx8__adtrs , u64: Matx8__adtrs_u)(s)
#define Matx8_rot90(s)    _Generic(s, Matx8: Matx8__rot90 , u64: Matx8__rot90_u)(s)
#define Matx8_rot180(s)   _Generic(s, Matx8: Matx8__rot180, u64: Matx8__rot180_u)(s)
#define Matx8_rot270(s)   _Generic(s, Matx8: Matx8__rot270, u64: Matx8__rot270_u)(s)
#define Matx8_tfm(s, t)   _Generic(s, Matx8: Matx8__tfm, u64: Matx8__tfm_u)(s, t)

#if RAND_BUF_LEN < 1
	#error "RAND_BUF_LEN must be at least 1"
#elif RAND_BUF_LEN > 256
	#error "RAND_BUF_LEN must be 256 or less"
#elif RAND_BUF_LEN == 1
static FORCE_INLINE Matx8 Matx8_random(void) {
	// RDRAND is only faster than RtlGenRandom in the unbuffered case
	// 1.53% faster than unbuffered RtlGenRandom
	u64 value;
	until (__builtin_ia32_rdrand64_step(&value));
	return (Matx8) {.matx = value};
}
#else
// plain RDRAND is always slower for these cases.
// those cap at around 2.6% speedup regardless of buffer size
// it also isn't an issue with the CPU running out of entropy
// and returning 0, the instructions are just really slow.

// these statistics may or may not be accurate now.
// speedup for each buffer size using RtlGenRandom:
	//   1: 0% (base case, no buffering)
	//   2: 1.74%
	//   4: 3.3%
	//   8: 3.27%
	//  16: 3.43%
	//  32: 5.63%
	//  64: 5.7%
	// 128: 6.21%
	// 256: 5.28%
	// 512 (u16 idx): 6.19%
	// 512 (u32 idx): 6.23%

static Matx8 Matx8_random(void) {
	_Static_assert((RAND_BUF_LEN & (RAND_BUF_LEN - 1)) == 0, "RAND_BUF_LEN must be power of two");

	static u64 buffer[RAND_BUF_LEN] __attribute__((aligned(64)));
	static u8 idx = 0; // initialize the first time it is called.

#if RAND_BUF_LEN < 256
	if (idx >= RAND_BUF_LEN)
		__builtin_unreachable();
#endif

	unlikelyp_if (idx == 0, 1.0 - 1.0 / RAND_BUF_LEN) {
		RtlGenRandom(buffer, sizeof buffer);
		idx = RAND_BUF_LEN & UINT8_MAX; // NOTE: still fine for 256
	}

	return (Matx8) {
		.matx = buffer[--idx]
	};
}
#endif
