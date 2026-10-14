/* Owned no-entry Windows DLL. UTF-16 is tested as unsigned 16-bit code units,
 * including surrogate pairs and values which are negative as signed shorts. */
typedef unsigned short u16;
typedef unsigned long long u64;
_Static_assert(sizeof(u16) == 2 && sizeof(u64) == 8, "Windows UTF-16/size_t widths");
__declspec(dllimport) u64 wcslen(const u16 *);
__declspec(dllimport) int wcscmp(const u16 *, const u16 *);
__declspec(dllimport) int wcsncmp(const u16 *, const u16 *, u64);
__declspec(dllimport) u16 *wcscpy(u16 *, const u16 *);

static const u16 sample[] = {0x0041, 0xd83d, 0xde80, 0x8000, 0xffff, 0};
/* Retains a genuine DIR64 relocation in the executed DLL. */
static const u16 *volatile source = sample;

__declspec(dllexport) int WideExercise(u16 *out, u64 capacity, u64 *length,
                                      const u16 *bounded_last_unit)
{
    static const u16 empty[] = {0};
    static const u16 low_bytes_zero[] = {0x0100, 0x0200, 0};
    static const u16 high[] = {0x8000, 0};
    static const u16 low[] = {0x7fff, 0};
    static const u16 maximum[] = {0xffff, 0};
    static const u16 minimum[] = {1, 0};
    static const u16 prefix[] = {0x0041, 0xd83d, 0};
    u16 cleared[2] = {0xaaaa, 0xbbbb};
    const u16 *text = source;
    if (!out || capacity < 8 || !length || !bounded_last_unit) return 1;
    if (wcslen(text) != 5 || wcslen(empty) != 0 || wcslen(low_bytes_zero) != 2) return 2;
    if (wcscmp(text, sample) != 0 || wcscmp(high, low) <= 0 || wcscmp(low, high) >= 0) return 3;
    if (wcscmp(maximum, minimum) <= 0 || wcscmp(prefix, text) >= 0 || wcscmp(text, prefix) <= 0) return 4;
    if (wcsncmp(high, low, 0) != 0 || wcsncmp(text, prefix, 2) != 0 || wcsncmp(text, prefix, 3) <= 0) return 5;
    if (wcsncmp(high, low, 1) <= 0 || wcsncmp(low, high, 1) >= 0 || wcsncmp(empty, empty, 8) != 0) return 6;
    /* The next host memory page is inaccessible: count one must not overread,
     * and count zero must not dereference its one-past-last-unit pointers. */
    if (wcsncmp(bounded_last_unit, maximum, 1) != 0 ||
        wcsncmp(bounded_last_unit + 1, bounded_last_unit + 1, 0) != 0) return 7;
    for (u64 i = 0; i < 8; ++i) out[i] = 0x5a5a;
    if (wcscpy(out, text) != out) return 8;
    for (u64 i = 0; i < 6; ++i) if (out[i] != sample[i]) return 9;
    if (out[6] != 0x5a5a || out[7] != 0x5a5a) return 10;
    if (wcscpy(cleared, empty) != cleared || cleared[0] != 0 || cleared[1] != 0xbbbb) return 11;
    *length = wcslen(out);
    return *length == 5 ? 0 : 12;
}
