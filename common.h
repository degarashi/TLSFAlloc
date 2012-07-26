#pragma once

// #define MSVC
#define USEASM_BITSEARCH
#define TLSF_MEMFILL
	
#include <stdint.h>
#include <stddef.h>
#include <memory.h>
#include <algorithm>
#include <exception>
#include <assert.h>
#include <iostream>
#include <new>
#include "type.h"

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

#ifdef DEBUG
	#define L_ASSERT(e, msg) { assert((e) || !(msg)); }
	#define LA_OUTRANGE(e, msg) { assert((e) || !(msg)); }
#else
	#define L_ASSERT(...) {}
	#define LA_OUTRANGE(...) {}
#endif

// ビット操作関数群
namespace Bit {
	inline u32 LowClear(u32 x) {
		x = x | (x >> 1);
		x = x | (x >> 2);
		x = x | (x >> 4);
		x = x | (x >> 8);
		x = x | (x >>16);
		return x & ~(x>>1);
	}
	// ビットの位置を計算	
	#ifdef USEASM_BITSEARCH
		inline u32 MSB_N(u32 x) {
			#ifdef MSVC
				__asm or [x], 0x01
				__asm bsr eax, dword ptr[x]
			#else
				__asm__("or %0, 0x01\n\t"
						"bsr eax, %0"
						:
						:"q"(x)
				);
			#endif
		}
		inline u32 LSB_N(u32 x) {
			#ifdef MSVC
				__asm or [x], 0x80000000
				__asm bsf eax, dword ptr[x]
			#else
				__asm__("or %0, 0x80000000\n\t"
						"bsf eax, %0"
					   :
					   :"q"(x));
			#endif
		}
	#else
		const char SB_TABLE[] = "\x00\x00\x01\x0e\x1c\x02\x16\x0f\x1d\x1a\x03\x05\x0b\x17"
							"\x07\x10\x1e\x0d\x1b\x15\x19\x04\x0a\x06\x0c\x14\x18\x09"
							"\x13\x08\x12\x11";
		inline u32 MSB_N(u32 x) {
			return SB_TABLE[0x0aec7cd2U * LowClear(x) >> 27];
		}
		inline u32 LSB_N(u32 x) {
			return SB_TABLE[0x0aec7cd2U * (x & -x) >> 27];
		}
	#endif
}
