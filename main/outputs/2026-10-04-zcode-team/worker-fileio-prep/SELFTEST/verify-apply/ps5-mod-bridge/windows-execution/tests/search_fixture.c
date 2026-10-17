/* Owned no-entry Windows DLL; every search goes through an actual import. */
typedef unsigned long long u64;
_Static_assert(sizeof(u64) == 8 && sizeof(int) == 4, "Windows size_t/int widths");
__declspec(dllimport) void *memchr(const void *, int, u64);
__declspec(dllimport) char *strchr(const char *, int);
__declspec(dllimport) char *strrchr(const char *, int);
__declspec(dllimport) char *strstr(const char *, const char *);

static const char repeated[] = "aaaaab";
/* Retains a genuine DIR64 relocation in the executed DLL. */
static const char *volatile source = repeated;

__declspec(dllexport) int SearchExercise(const char *text,
    const unsigned char *bounded_last_byte, void **found, u64 capacity)
{
    static const char high_needle[] = {(char)0x80, 'b', 0};
    static const char empty[] = "";
    static const char overlapping[] = "ababa";
    const char *relocated = source;
    if (!text || !bounded_last_byte || !found || capacity < 6) return 1;
    /* The host's eight bytes are a,80,b,a,ff,NUL,z,80. Returned addresses
     * must refer to those original bytes, including matches after NUL for memchr. */
    found[0] = memchr(text, 0x180, 8);
    found[1] = strchr(text, 'a');
    found[2] = strrchr(text, 'a');
    found[3] = strstr(text, high_needle);
    found[4] = strchr(text, 0x100);
    found[5] = strstr(text, empty);
    if (found[0] != text + 1 || found[1] != text || found[2] != text + 3 ||
        found[3] != text + 1 || found[4] != text + 5 || found[5] != text) return 2;
    if (memchr(text, -1, 8) != text + 4 || memchr(text, 0, 8) != text + 5 ||
        memchr(text, 'z', 8) != text + 6 || memchr(text, 'z', 6) != 0) return 3;
    if (memchr(text + 2, 'a', 6) != text + 3 || memchr(text, 'q', 8) != 0 || memchr(text, 'a', 0) != 0) return 4;
    if (strchr(text, 0x180) != text + 1 || strchr(text, -1) != text + 4 ||
        strchr(text, 0) != text + 5 || strchr(text, 'z') != 0) return 5;
    if (strrchr(text, 0x180) != text + 1 || strrchr(text, -1) != text + 4 ||
        strrchr(text, 0x100) != text + 5 || strrchr(text, 0) != text + 5 || strrchr(text, 'z') != 0) return 6;
    if (strchr(empty, 0) != empty || strrchr(empty, 0) != empty ||
        strchr(empty, 'a') != 0 || strrchr(empty, 'a') != 0) return 7;
    if (strstr(empty, empty) != empty || strstr(empty, "a") != 0 ||
        strstr(text, "az") != 0 || strstr(overlapping, "ababab") != 0) return 8;
    if (strstr(overlapping, "aba") != overlapping || strstr(overlapping, "bab") != overlapping + 1 ||
        strstr(relocated, "aaab") != relocated + 2) return 9;
    /* The following host page is inaccessible. Both a one-byte miss and a
     * zero-byte search at the one-past pointer must stay within their bounds. */
    if (memchr(bounded_last_byte, 0x180, 1) != bounded_last_byte ||
        memchr(bounded_last_byte, -128, 1) != bounded_last_byte ||
        memchr(bounded_last_byte, 0xff, 1) != 0 ||
        memchr(bounded_last_byte + 1, 0x80, 0) != 0) return 10;
    return 0;
}
