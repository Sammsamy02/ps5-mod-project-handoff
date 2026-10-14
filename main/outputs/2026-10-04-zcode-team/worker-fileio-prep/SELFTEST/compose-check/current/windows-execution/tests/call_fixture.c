/* Owned Windows DLL fixture: no CRT startup, TLS, exceptions or threads.
 * Calls real imported services; this code is used only for developer tests. */
typedef unsigned long long u64;
/* Compiler's link marker for floating-point use; no CRT initialization here. */
int _fltused = 0;
__declspec(dllimport) void *memcpy(void *, const void *, u64);
__declspec(dllimport) void *memmove(void *, const void *, u64);
__declspec(dllimport) void *memset(void *, int, u64);
__declspec(dllimport) int memcmp(const void *, const void *, u64);
__declspec(dllimport) u64 strlen(const char *);
__declspec(dllimport) int strcmp(const char *, const char *);

/* Forces a real DIR64 relocation which the execution path must apply. */
static const char *volatile prefix = "DLL->native:";

__declspec(dllexport) int Exercise(char *out, u64 capacity, const char *input, u64 n)
{
    char scratch[80];
    const unsigned char high = 0x80, low = 0x7f;
    const char *p = prefix;
    u64 length;
    if (!out || !input || n > 60 || capacity < 80) return 1;
    if (memset(scratch, 0xa5, sizeof scratch) != scratch) return 2;
    if (memcpy(scratch, input, n + 1) != scratch) return 3;
    if (memmove(scratch + 1, scratch, n + 1) != scratch + 1) return 4;
    if (memmove(scratch, scratch + 1, n + 1) != scratch) return 5;
    if (strlen(scratch) != n || strcmp(scratch, input) != 0) return 6;
    if (memcmp(scratch, input, n + 1) != 0 || memcmp(&high, &low, 1) <= 0) return 7;
    length = strlen(p);
    memcpy(out, p, length);
    memcpy(out + length, scratch, n + 1);
    return 0;
}

/* A signature with both register classes and arguments on the Windows stack. */
__declspec(dllexport) double Mixed(u64 a, double b, u64 c, double d, u64 e, double f)
{
    return (double)a + b * 2.0 + (double)c * 3.0 + d * 4.0 + (double)e * 5.0 + f * 6.0;
}
