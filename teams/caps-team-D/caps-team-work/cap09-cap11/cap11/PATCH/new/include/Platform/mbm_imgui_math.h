// mbm_imgui_math.h -- Dear ImGui math hooks (IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS)
// built ONLY on C functions the runtime PRX already imports (cos, acos, fmod,
// pow, strtod; R11 import list), so linking Dear ImGui into the runtime adds no
// new float-math import (cosf, sinf, acosf, atan2f, powf, logf, sqrtf, atof
// would otherwise be new libc imports needing supplier-contract admission).
// Included by mbm_imconfig.h; identical in host and mods.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace mbm_imgui_math
{
    // Newton-Raphson square root (no libcall: the compiler would turn
    // pow(x, 0.5) back into a sqrt/sqrtf import). Bit-hack seed, 6 steps:
    // full double precision for every positive finite x.
    inline double sqrt_d(double x)
    {
        if (!(x > 0.0)) return 0.0;
        if (x > 1.7e308) return x;
        uint64_t bits;
        memcpy(&bits, &x, sizeof bits);
        bits = (bits >> 1) + 0x1FF8000000000000ull; // halve the exponent
        double r;
        memcpy(&r, &bits, sizeof r);
        for (int i = 0; i < 6; ++i) r = 0.5 * (r + x / r);
        return r;
    }
    inline float sqrt_f(float x) { return static_cast<float>(sqrt_d(static_cast<double>(x))); }
    inline float ceil_f(float x)
    {
        if (!(x == x)) return x;                          // NaN
        if (x >= 8388608.0f || x <= -8388608.0f) return x; // already integral (|x| >= 2^23)
        const int i = static_cast<int>(x);
        return static_cast<float>(i + (x > static_cast<float>(i) ? 1 : 0));
    }
    inline float atan2_f(float y, float x)
    {
        const double r = sqrt_d(static_cast<double>(x) * x + static_cast<double>(y) * y);
        if (r == 0.0) return 0.0f;
        double c = static_cast<double>(x) / r;
        c = c > 1.0 ? 1.0 : (c < -1.0 ? -1.0 : c);
        const double a = acos(c);
        return static_cast<float>(y < 0.0f ? -a : a);
    }
    inline double log_d(double x)
    {
        if (!(x > 0.0)) return x == 0.0 ? -1.0e308 : 0.0;
        uint64_t bits;
        memcpy(&bits, &x, sizeof bits);
        int e = static_cast<int>((bits >> 52) & 0x7FF);
        if (e == 0) // subnormal: scale up
        {
            x *= 4503599627370496.0; // 2^52
            memcpy(&bits, &x, sizeof bits);
            e = static_cast<int>((bits >> 52) & 0x7FF) - 52;
        }
        e -= 1023;
        bits = (bits & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull; // mantissa in [1,2)
        double m;
        memcpy(&m, &bits, sizeof m);
        if (m > 1.4142135623730951) { m *= 0.5; ++e; }   // m in [0.707, 1.414]
        const double t = (m - 1.0) / (m + 1.0), t2 = t * t;
        double sum = 0.0, term = t;
        for (int k = 1; k < 40; k += 2) { sum += term / k; term *= t2; }
        return 2.0 * sum + e * 0.69314718055994530942;
    }
} // namespace mbm_imgui_math

#define ImFabs(X) __builtin_fabsf(X)
#define ImSqrt(X) ::mbm_imgui_math::sqrt_f(X)
#define ImFmod(X, Y) static_cast<float>(fmod(static_cast<double>(X), static_cast<double>(Y)))
#define ImCos(X) static_cast<float>(cos(static_cast<double>(X)))
#define ImSin(X) static_cast<float>(cos(static_cast<double>(X) - 1.57079632679489661923))
#define ImAcos(X) static_cast<float>(acos(static_cast<double>(X)))
#define ImAtan2(Y, X) ::mbm_imgui_math::atan2_f((Y), (X))
#define ImAtof(STR) strtod((STR), nullptr)
#define ImCeil(X) ::mbm_imgui_math::ceil_f(X)
static inline float ImPow(float x, float y) { return static_cast<float>(pow(static_cast<double>(x), static_cast<double>(y))); }
static inline double ImPow(double x, double y) { return pow(x, y); }
static inline float ImLog(float x) { return static_cast<float>(::mbm_imgui_math::log_d(static_cast<double>(x))); }
static inline double ImLog(double x) { return ::mbm_imgui_math::log_d(x); }
static inline int ImAbs(int x) { return x < 0 ? -x : x; }
static inline float ImAbs(float x) { return __builtin_fabsf(x); }
static inline double ImAbs(double x) { return __builtin_fabs(x); }
static inline float ImSign(float x) { return (x < 0.0f) ? -1.0f : (x > 0.0f) ? 1.0f : 0.0f; }
static inline double ImSign(double x) { return (x < 0.0) ? -1.0 : (x > 0.0) ? 1.0 : 0.0; }
static inline float ImRsqrt(float x) { return 1.0f / ::mbm_imgui_math::sqrt_f(x); }
static inline double ImRsqrt(double x) { return 1.0 / ::mbm_imgui_math::sqrt_d(x); }
