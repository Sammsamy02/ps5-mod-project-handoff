/*
 * classifier_v1_regression — host-side regression test for the v1 probe's
 * result classifier (KG directive, 2026-10-13: "regression-test successful
 * high-bit values alongside genuine errors").
 *
 * Mirrors classify_msr_result() from KernelReadProbe-v1.c EXACTLY (byte-equal
 * logic; keep the two in sync if either changes). Runs on the Mac only — it
 * tests classification semantics, not the console path.
 *
 * Cases: successful high-bit MSR values (canonical kernel LSTAR, the v0 bug
 * class), genuine CF errors, errno-in-RAX errors, zero success, the 0xFFF /
 * 0x1000 boundary, a real EFER-class value, and CF-wins-over-high-bit.
 */

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int classify_msr_result(uint64_t rc, int cf_error, uint64_t* value)
{
    if(cf_error) /* carry-flag error: errno = rc, raw value logged by caller */
        return 1;
    if(rc != 0 && rc < UINT64_C(0x1000)) /* errno-in-RAX convention or CHECK-style */
        return 1;
    *value = rc;
    return 0;
}

static int fails;

static void expect(const char* name, int got, int want, uint64_t val,
                   uint64_t want_val)
{
    const int ok = (got == want) && (want == 0 ? val == want_val : 1);
    printf("%-38s %s (got=%d want=%d val=0x%016" PRIx64 ")\n",
           name, ok ? "PASS" : "FAIL", got, want, val);
    if(!ok)
        fails++;
}

#define T_VALUE(name, rc, cf, wantval) do { \
    uint64_t v = 0xDEADBEEFDEADBEEFULL; \
    expect(name, classify_msr_result((rc), (cf), &v), 0, v, (wantval)); \
} while(0)

#define T_ERROR(name, rc, cf) do { \
    uint64_t v = 0xDEADBEEFDEADBEEFULL; \
    expect(name, classify_msr_result((rc), (cf), &v), 1, v, 0); \
} while(0)

int main(void)
{
    /* v0 bug class: successful high-bit values MUST classify as values */
    T_VALUE("high-bit canonical LSTAR", UINT64_C(0xffffffff82a5b000), 0,
            UINT64_C(0xffffffff82a5b000));
    T_VALUE("high-bit canonical LSTAR #2", UINT64_C(0xffffffff82467000), 0,
            UINT64_C(0xffffffff82467000));
    T_VALUE("top-bit-set generic", UINT64_C(0x8000000000000000), 0,
            UINT64_C(0x8000000000000000));
    T_VALUE("TSC-class mid value", UINT64_C(0x000006a16ddc88af), 0,
            UINT64_C(0x000006a16ddc88af));
    T_VALUE("EFER-class 0x11d01", UINT64_C(0x0000000000011d01), 0,
            UINT64_C(0x0000000000011d01));

    /* genuine errors MUST classify as errors under BOTH conventions */
    T_ERROR("CF error, errno 14 (EFAULT)", 14, 1);
    T_ERROR("CF error, errno 1", 1, 1);
    T_ERROR("errno-in-RAX, errno 14, CF clear", 14, 0);
    T_ERROR("errno-in-RAX, errno 1, CF clear", 1, 0);
    T_ERROR("errno-class max-1 (0xFFF)", UINT64_C(0xfff), 0);
    /* CF wins over a high-bit rc: flag is authoritative, never the value */
    T_ERROR("CF error with high-bit rc", UINT64_C(0xffffffff82a5b000), 1);

    /* zero is a SUCCESS with value 0 (valid zero MSR result) */
    T_VALUE("zero success", 0, 0, 0);
    /* boundary: 0x1000 is the first value-class number */
    T_VALUE("boundary 0x1000 is a value", UINT64_C(0x1000), 0,
            UINT64_C(0x1000));

    printf("summary cases=14 failures=%d\n", fails);
    printf("overall=%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
