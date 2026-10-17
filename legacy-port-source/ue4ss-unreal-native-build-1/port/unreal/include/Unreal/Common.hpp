#pragma once


#if defined(_MSC_VER)
#define FORCENOINLINE __declspec(noinline)
#define FORCEINLINE __forceinline
#else
#define FORCENOINLINE __attribute__((noinline))
#define FORCEINLINE inline __attribute__((always_inline))
#endif
#define RESTRICT __restrict

#ifndef RC_UNREAL_EXPORTS
#ifndef RC_UNREAL_BUILD_STATIC
#ifndef RC_UE_API
#define RC_UE_API __declspec(dllimport)
#endif
#else
#ifndef RC_UE_API
#define RC_UE_API
#endif
#endif
#else
#ifndef RC_UE_API
#define RC_UE_API __declspec(dllexport)
#endif
#endif


