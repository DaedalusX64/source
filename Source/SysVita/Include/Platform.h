#pragma once

#ifndef SYSVITA_INCLUDE_PLATFORM_H_
#define SYSVITA_INCLUDE_PLATFORM_H_

//
//	Make sure this platform is defined correctly
//
#ifndef DAEDALUS_VITA
#define DAEDALUS_VITA
#endif

#define DAEDALUS_COMPRESSED_ROM_SUPPORT

#define DAEDALUS_ENABLE_DYNAREC
#define DAEDALUS_ENABLE_OS_HOOKS

#define DAEDALUS_ENDIAN_MODE DAEDALUS_ENDIAN_LITTLE

#define DAEDALUS_EXPECT_LIKELY(c) __builtin_expect((c),1)
#define DAEDALUS_EXPECT_UNLIKELY(c) __builtin_expect((c),0)

#define DAEDALUS_ATTRIBUTE_NOINLINE __attribute__((noinline))

#define DAEDALUS_HALT			__asm__ __volatile__ ( "bkpt" )

//#define DAEDALUS_DYNAREC_HALT	SW(PspReg_R0, PspReg_R0, 0)

#define MAKE_UNCACHED_PTR(x)	(x)

#define DAEDALUS_ATTRIBUTE_PURE   __attribute__((pure))
#define DAEDALUS_ATTRIBUTE_CONST   __attribute__((const))

#define __has_feature(x) 0

#endif // SYSVITA_INCLUDE_PLATFORM_H_
