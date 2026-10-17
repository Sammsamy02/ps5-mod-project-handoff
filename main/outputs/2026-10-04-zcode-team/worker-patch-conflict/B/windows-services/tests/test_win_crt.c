#define _DEFAULT_SOURCE
#include "../mb_win_crt.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static unsigned failures;
#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #c); ++failures; \
} } while (0)

typedef void *(MB_WIN_X64_ABI *copy_fn)(void *, const void *, size_t);
typedef void *(MB_WIN_X64_ABI *set_fn)(void *, int, size_t);
typedef int (MB_WIN_X64_ABI *compare_fn)(const void *, const void *, size_t);
typedef size_t (MB_WIN_X64_ABI *length_fn)(const char *);
typedef int (MB_WIN_X64_ABI *string_compare_fn)(const char *, const char *);
typedef size_t (MB_WIN_X64_ABI *wide_length_fn)(const uint16_t *);
typedef int (MB_WIN_X64_ABI *wide_compare_fn)(const uint16_t *, const uint16_t *);
typedef int (MB_WIN_X64_ABI *wide_ncompare_fn)(const uint16_t *, const uint16_t *, size_t);
typedef uint16_t *(MB_WIN_X64_ABI *wide_copy_fn)(uint16_t *, const uint16_t *);
typedef void *(MB_WIN_X64_ABI *memory_search_fn)(const void *, int, size_t);
typedef char *(MB_WIN_X64_ABI *byte_search_fn)(const char *, int);
typedef char *(MB_WIN_X64_ABI *substring_fn)(const char *, const char *);
extern int mb_test_nonvolatile(uint64_t, uint64_t, uint64_t, uint64_t);

static mb_pe_import named(const char *library, const char *name) {
    mb_pe_import i = {0};
    i.library.data = library; i.library.size = strlen(library);
    i.name.data = name; i.name.size = strlen(name);
    return i;
}
static uint64_t bind(const char *library, const char *name) {
    mb_pe_import i = named(library, name);
    uint64_t value = 0;
    CHECK(mb_win_crt_bind(NULL, &i, &value) == MB_PE_BIND_BOUND);
    CHECK(value != 0);
    return value;
}

/* Catches wrong argument registers, missing copy, changed return pointer,
 * overrun and signed-byte handling. Calls use binder-returned addresses from
 * a separately compiled TU so the optimizer cannot replace or inline them. */
static void copy_bytes(void) {
    const unsigned char src[] = {0, 0xff, 0x80, 7};
    unsigned char dst[] = {9, 9, 9, 9, 9, 9};
    const unsigned char want[] = {9, 0, 0xff, 0x80, 7, 9};
    uint64_t address = bind("VcRuNtImE140.DlL", "memcpy");
    if (!address) return;
    copy_fn f = (copy_fn)(uintptr_t)address;
    CHECK(f(dst + 1, src, sizeof(src)) == dst + 1);
    CHECK(memcmp(dst, want, sizeof(dst)) == 0);
    CHECK(f(dst + 1, src, 0) == dst + 1);
    CHECK(memcmp(dst, want, sizeof(dst)) == 0);
}
/* Catches forward-only and backward-only implementations of overlap. */
static void move_overlap(void) {
    char up[] = "01234567", down[] = "01234567", other[] = "abcdefgh";
    uint64_t address = bind("VCRUNTIME140.dll", "memmove");
    if (!address) return;
    copy_fn f = (copy_fn)(uintptr_t)address;
    CHECK(f(up + 2, up, 6) == up + 2);
    CHECK(memcmp(up, "01012345", 9) == 0);
    CHECK(f(down, down + 2, 6) == down);
    CHECK(memcmp(down, "23456767", 9) == 0);
    CHECK(f(down, down, sizeof(down)) == down);
    CHECK(memcmp(down, "23456767", 9) == 0);
    CHECK(f(down, up, 0) == down);
    CHECK(memcmp(down, "23456767", 9) == 0);
    CHECK(f(other, down, sizeof(down)) == other);
    CHECK(memcmp(other, "23456767", 9) == 0);
}
/* Catches missing count and failure to truncate int to unsigned char. */
static void fill_bytes(void) {
    unsigned char dst[] = {1, 2, 3, 4, 5};
    const unsigned char first[] = {1, 0xa5, 0xa5, 0xa5, 5};
    const unsigned char second[] = {1, 0xff, 0xff, 0xff, 5};
    uint64_t address = bind("VCRUNTIME140.dll", "memset");
    if (!address) return;
    set_fn f = (set_fn)(uintptr_t)address;
    CHECK(f(dst + 1, 0x1a5, 3) == dst + 1);
    CHECK(memcmp(dst, first, sizeof(dst)) == 0);
    CHECK(f(dst + 1, -1, 3) == dst + 1);
    CHECK(memcmp(dst, second, sizeof(dst)) == 0);
    CHECK(f(dst, 0, 0) == dst);
    CHECK(memcmp(dst, second, sizeof(dst)) == 0);
}
/* Catches signed char comparison, ignoring count or stopping at NUL. */
static void compare_bytes(void) {
    const unsigned char a[] = {0, 0xff, 1}, b[] = {0, 0x7f, 2};
    uint64_t address = bind("VCRUNTIME140.dll", "memcmp");
    if (!address) return;
    compare_fn f = (compare_fn)(uintptr_t)address;
    CHECK(f(a, b, 0) == 0);
    CHECK(f(a, b, 1) == 0);
    CHECK(f(a, b, 3) > 0);
    CHECK(f(b, a, 3) < 0);
    CHECK(f(a, a, sizeof(a)) == 0);
}
/* Catches counting characters instead of bytes and including the terminator. */
static void string_length(void) {
    const char bytes[] = {'x', (char)0xc3, (char)0xa9, '\0', 'z'};
    uint64_t address = bind("API-MS-WIN-CRT-STRING-L1-1-0.DLL", "strlen");
    if (!address) return;
    length_fn f = (length_fn)(uintptr_t)address;
    CHECK(f("") == 0);
    CHECK(f(bytes) == 3);
    CHECK(f(bytes + 1) == 2);
}
/* Catches signed bytes, prefix handling and comparing past NUL. */
static void compare_strings(void) {
    const char a[] = {(char)0xff, '\0', 'a'}, b[] = {(char)0x7f, '\0'};
    uint64_t address = bind("api-ms-win-crt-string-l1-1-0.dll", "strcmp");
    if (!address) return;
    string_compare_fn f = (string_compare_fn)(uintptr_t)address;
    CHECK(f(a, b) > 0);
    CHECK(f(b, a) < 0);
    CHECK(f("ab", "abc") < 0);
    CHECK(f("abc", "ab") > 0);
    CHECK(f("a\0x", "a\0y") == 0);
    CHECK(f("", "") == 0);
    CHECK(f("A", "a") < 0);
}
/* Catches ignoring count, stopping at embedded NUL, returning the last match,
 * or failing to convert the int argument to an unsigned byte. */
static void search_memory(void) {
    const unsigned char bytes[] = {0x11, 0xff, 0, 0x7f, 0xff, 0x22};
    uint64_t address = bind("VcRuNtImE140.DlL", "memchr");
    if (!address) return;
    memory_search_fn f = (memory_search_fn)(uintptr_t)address;
    CHECK(f(bytes, 0x11, 0) == NULL);
    CHECK(f(bytes, 0x11, sizeof(bytes)) == bytes);
    CHECK(f(bytes, -1, sizeof(bytes)) == bytes + 1);
    CHECK(f(bytes, 0x1ff, sizeof(bytes)) == bytes + 1);
    CHECK(f(bytes, 0x100, sizeof(bytes)) == bytes + 2);
    CHECK(f(bytes, 0x22, 5) == NULL);
    CHECK(f(bytes, 0x22, sizeof(bytes)) == bytes + 5);
    CHECK(f(bytes + 2, 0xff, 4) == bytes + 4);
    CHECK(f(bytes, 0x33, sizeof(bytes)) == NULL);
}
/* Catches non-first match, skipping the terminator, crossing the terminator or
 * sign-sensitive matching of a high byte. Results are original-array pointers. */
static void search_first_byte(void) {
    const char text[] = {'a', 'b', 'a', (char)0xff, '\0', 'z'};
    uint64_t address = bind("VCRUNTIME140.dll", "strchr");
    if (!address) return;
    byte_search_fn f = (byte_search_fn)(uintptr_t)address;
    CHECK(f(text, 'a') == text);
    CHECK(f(text + 1, 'a') == text + 2);
    CHECK(f(text, 'b') == text + 1);
    CHECK(f(text, -1) == text + 3);
    CHECK(f(text, 0x1ff) == text + 3);
    CHECK(f(text, 0x100) == text + 4);
    CHECK(f(text, 'z') == NULL);
    CHECK(f(text, 'A') == NULL);
    CHECK(f(text + 4, 0) == text + 4);
    CHECK(f(text + 4, 'a') == NULL);
}
/* Catches a first-match implementation and incorrect empty-string/NUL rules. */
static void search_last_byte(void) {
    const char text[] = {'a', 'b', 'a', (char)0xff, '\0', 'z'};
    uint64_t address = bind("VCRUNTIME140.dll", "strrchr");
    if (!address) return;
    byte_search_fn f = (byte_search_fn)(uintptr_t)address;
    CHECK(f(text, 'a') == text + 2);
    CHECK(f(text, 'b') == text + 1);
    CHECK(f(text, -1) == text + 3);
    CHECK(f(text, 0x1ff) == text + 3);
    CHECK(f(text, 0x100) == text + 4);
    CHECK(f(text, 'z') == NULL);
    CHECK(f(text + 4, 0) == text + 4);
    CHECK(f(text + 4, 'a') == NULL);
}
/* Catches skipped overlapping candidates, last-match selection, reading past
 * either NUL, and wrong empty-needle results. */
static void search_substring(void) {
    const char text[] = "abababa", overlap[] = "aaaab", empty[] = "";
    const char high[] = {(char)0xff, 'x', (char)0xff, 'y', 0};
    const char high_needle[] = {(char)0xff, 'y', 0};
    const char after_nul[] = {'a', 0, 'b', 0};
    uint64_t address = bind("VCRUNTIME140.dll", "strstr");
    if (!address) return;
    substring_fn f = (substring_fn)(uintptr_t)address;
    CHECK(f(text, "aba") == text);
    CHECK(f(text, "baba") == text + 1);
    CHECK(f(overlap, "aaab") == overlap + 1);
    CHECK(f(text, text + 2) == text);
    CHECK(f(text, "") == text);
    CHECK(f(empty, "") == empty);
    CHECK(f(empty, "a") == NULL);
    CHECK(f(text, "abababab") == NULL);
    CHECK(f(text, "AB") == NULL);
    CHECK(f(high, high_needle) == high + 2);
    CHECK(f(after_nul, "ab") == NULL);
    CHECK(f(text, "aba\0z") == text);
}
/* Guard pages detect any read past a bounded region or string terminator. */
static void search_boundaries(void) {
    uint64_t memory_address = bind("VCRUNTIME140.dll", "memchr");
    uint64_t first_address = bind("VCRUNTIME140.dll", "strchr");
    uint64_t last_address = bind("VCRUNTIME140.dll", "strrchr");
    uint64_t substring_address = bind("VCRUNTIME140.dll", "strstr");
    if (!memory_address || !first_address || !last_address || !substring_address) return;
    long page_size = sysconf(_SC_PAGESIZE);
    CHECK(page_size > 0);
    if (page_size <= 0) return;
    size_t page = (size_t)page_size;
    char *memory = mmap(NULL, page * 4, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(memory != MAP_FAILED);
    if (memory == MAP_FAILED) return;
    int protected_first = mprotect(memory + page, page, PROT_NONE);
    int protected_second = mprotect(memory + page * 3, page, PROT_NONE);
    CHECK(protected_first == 0 && protected_second == 0);
    if (protected_first == 0 && protected_second == 0) {
        char *text = memory + page - 4, *needle = memory + page * 3 - 3;
        memory_search_fn search = (memory_search_fn)(uintptr_t)memory_address;
        byte_search_fn first = (byte_search_fn)(uintptr_t)first_address;
        byte_search_fn last = (byte_search_fn)(uintptr_t)last_address;
        substring_fn substring = (substring_fn)(uintptr_t)substring_address;
        text[0] = 'a'; text[1] = 'b'; text[2] = 'a'; text[3] = 0;
        needle[0] = 'b'; needle[1] = 'a'; needle[2] = 0;
        CHECK(first(text, 0) == text + 3);
        CHECK(last(text, 0) == text + 3);
        CHECK(first(text, 'z') == NULL);
        CHECK(last(text, 'z') == NULL);
        CHECK(substring(text, needle) == text + 1);
        CHECK(substring(text + 2, needle) == NULL);
        CHECK(substring(text + 3, needle + 2) == text + 3);
        needle[0] = 'a'; needle[1] = 'a';
        CHECK(substring(text + 2, needle) == NULL);
        text[3] = (char)0xff;
        CHECK(search(text, -1, 4) == text + 3);
        CHECK(search(text, 'z', 4) == NULL);
        CHECK(search(text + 4, 0, 0) == NULL);
    }
    CHECK(munmap(memory, page * 4) == 0);
}
/* Catches 32-bit host wchar_t, byte counting and surrogate-pair decoding. */
static void wide_length(void) {
    const uint16_t units[] = {0x0041, 0x0100, 0xd83d, 0xde80, 0xffff, 0, 0x7777};
    const uint16_t isolated[] = {0xd800, 0};
    uint64_t address = bind("API-MS-WIN-CRT-STRING-L1-1-0.DLL", "wcslen");
    if (!address) return;
    wide_length_fn f = (wide_length_fn)(uintptr_t)address;
    CHECK(f(units) == 5);
    CHECK(f(units + 2) == 3);
    CHECK(f(units + 5) == 0);
    CHECK(f(isolated) == 1);
}
/* Catches signed-16-bit or bytewise ordering, prefix errors and NUL overread. */
static void compare_wide(void) {
    const uint16_t high[] = {0xffff, 0}, low[] = {0x7fff, 0};
    const uint16_t u100[] = {0x0100, 0}, uff[] = {0x00ff, 0};
    const uint16_t short_string[] = {0x0041, 0}, long_string[] = {0x0041, 0x0042, 0};
    const uint16_t a[] = {0x0041, 0, 0x1111}, b[] = {0x0041, 0, 0x2222};
    uint64_t address = bind("api-ms-win-crt-string-l1-1-0.dll", "wcscmp");
    if (!address) return;
    wide_compare_fn f = (wide_compare_fn)(uintptr_t)address;
    CHECK(f(high, low) > 0);
    CHECK(f(low, high) < 0);
    CHECK(f(u100, uff) > 0);
    CHECK(f(short_string, long_string) < 0);
    CHECK(f(long_string, short_string) > 0);
    CHECK(f(a, b) == 0);
    CHECK(f(a + 1, b + 1) == 0);
}
/* Catches ignoring the bound, truncating count to 32 bits or comparing after
 * either terminator. A large count still stops at an early differing unit. */
static void compare_wide_counted(void) {
    const uint16_t a[] = {0x0041, 0xffff, 0}, b[] = {0x0041, 0x7fff, 0};
    const uint16_t short_string[] = {0x0041, 0};
    const uint16_t after_a[] = {0x0041, 0, 0x1111}, after_b[] = {0x0041, 0, 0x2222};
    uint64_t address = bind("api-ms-win-crt-string-l1-1-0.dll", "wcsncmp");
    if (!address) return;
    wide_ncompare_fn f = (wide_ncompare_fn)(uintptr_t)address;
    CHECK(f(a, b, 0) == 0);
    CHECK(f(a, b, 1) == 0);
    CHECK(f(a, b, 2) > 0);
    CHECK(f(b, a, 2) < 0);
    CHECK(f(a, b, (size_t)UINT64_C(0x100000000)) > 0);
    CHECK(f(after_a, after_b, 3) == 0);
    CHECK(f(short_string, a, 2) < 0);
    CHECK(f(a, short_string, 2) > 0);
}
/* Catches omission of the terminator, wrong return address and out-of-bounds
 * stores. Expected code units are literal, independent of host wide routines. */
static void copy_wide(void) {
    const uint16_t src[] = {0x0041, 0x0100, 0xd83d, 0xde80, 0xffff, 0, 0x7777};
    uint16_t dst[] = {0xbeef, 1, 2, 3, 4, 5, 6, 0xbeef};
    const uint16_t want[] = {0xbeef, 0x0041, 0x0100, 0xd83d, 0xde80, 0xffff, 0, 0xbeef};
    uint64_t address = bind("api-ms-win-crt-string-l1-1-0.dll", "wcscpy");
    if (!address) return;
    wide_copy_fn f = (wide_copy_fn)(uintptr_t)address;
    CHECK(f(dst + 1, src) == dst + 1);
    CHECK(memcmp(dst, want, sizeof(dst)) == 0);
    CHECK(f(dst + 1, src + 5) == dst + 1);
    CHECK(dst[0] == 0xbeef && dst[1] == 0 && dst[2] == 0x0100);
}
/* Guard pages catch reads beyond a terminator/bound, writes beyond the copied
 * terminator and any dereference for wcsncmp count zero. Host test memory only. */
static void wide_boundaries(void) {
    uint64_t length_address = bind("api-ms-win-crt-string-l1-1-0.dll", "wcslen");
    uint64_t compare_address = bind("api-ms-win-crt-string-l1-1-0.dll", "wcscmp");
    uint64_t counted_address = bind("api-ms-win-crt-string-l1-1-0.dll", "wcsncmp");
    uint64_t copy_address = bind("api-ms-win-crt-string-l1-1-0.dll", "wcscpy");
    if (!length_address || !compare_address || !counted_address || !copy_address) return;
    long page_size = sysconf(_SC_PAGESIZE);
    CHECK(page_size > 0);
    if (page_size <= 0) return;
    size_t page = (size_t)page_size;
    unsigned char *memory = mmap(NULL, page * 4, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(memory != MAP_FAILED);
    if (memory == MAP_FAILED) return;
    int protected_first = mprotect(memory + page, page, PROT_NONE);
    int protected_second = mprotect(memory + page * 3, page, PROT_NONE);
    CHECK(protected_first == 0 && protected_second == 0);
    if (protected_first == 0 && protected_second == 0) {
        uint16_t *a = (uint16_t *)(memory + page) - 2;
        uint16_t *b = (uint16_t *)(memory + page * 3) - 2;
        a[0] = 0xffff; a[1] = 0; b[0] = 0xffff; b[1] = 0;
        wide_length_fn length = (wide_length_fn)(uintptr_t)length_address;
        wide_compare_fn compare = (wide_compare_fn)(uintptr_t)compare_address;
        wide_ncompare_fn counted = (wide_ncompare_fn)(uintptr_t)counted_address;
        wide_copy_fn copy = (wide_copy_fn)(uintptr_t)copy_address;
        CHECK(length(a) == 1);
        CHECK(compare(a, b) == 0);
        CHECK(counted(a, b, SIZE_MAX) == 0);
        CHECK(counted(a + 2, b + 2, 0) == 0);
        b[0] = 0x1111; b[1] = 0x2222;
        CHECK(copy(b, a) == b);
        CHECK(b[0] == 0xffff && b[1] == 0);
        a[1] = 0x1234; b[1] = 0x1234;
        CHECK(counted(a, b, 2) == 0);
    }
    CHECK(munmap(memory, page * 4) == 0);
}
/* Catches overly permissive module/symbol matching and accidental fallback. */
static void exact_imports(void) {
    const char *pairs[][2] = {
        {"KERNEL32.dll", "memcpy"}, {"MSVCP140.dll", "memcpy"},
        {"ucrtbase.dll", "strlen"}, {"VCRUNTIME140_1.dll", "memcpy"},
        {"VCRUNTIME140.dll", "Memcpy"}, {"VCRUNTIME140.dll", "memcpy_s"},
        {"VCRUNTIME140.dll", "strlen"},
        {"api-ms-win-crt-string-l1-1-0.dll", "memcpy"},
        {"api-ms-win-crt-string-l1-1-0.dll", "STRLEN"},
        {"path/VCRUNTIME140.dll", "memcpy"}, {"VCRUNTIME140.dllX", "memcpy"},
        {"VCRUNTIME140.dll", "wcslen"}, {"ucrtbase.dll", "wcslen"},
        {"api-ms-win-crt-string-l1-1-0.dll", "WCSLEN"},
        {"api-ms-win-crt-string-l1-1-0.dll", "wcscpy_s"},
        {"api-ms-win-crt-string-l1-1-1.dll", "wcsncmp"},
        {"api-ms-win-crt-string-l1-1-0.dll", "strchr"},
        {"ucrtbase.dll", "strstr"}, {"VCRUNTIME140_1.dll", "memchr"},
        {"VCRUNTIME140.dll", "Strrchr"}, {"VCRUNTIME140.dll", "strstr_s"}
    };
    for (size_t k = 0; k < sizeof(pairs) / sizeof(pairs[0]); ++k) {
        mb_pe_import i = named(pairs[k][0], pairs[k][1]);
        uint64_t value = UINT64_C(0x12345678abcdef00);
        CHECK(mb_win_crt_bind(NULL, &i, &value) == MB_PE_BIND_UNRESOLVED);
        CHECK(value == UINT64_C(0x12345678abcdef00));
    }
    mb_pe_import i = named("VCRUNTIME140.dll", "memcpy");
    uint64_t value = 7;
    i.by_ordinal = 1; i.ordinal = 1;
    CHECK(mb_win_crt_bind(NULL, &i, &value) == MB_PE_BIND_UNRESOLVED);
    CHECK(value == 7);
    i.by_ordinal = 0;
    /* Exact sized slices with garbage immediately after either name. */
    i.library.data = "VCRUNTIME140.dll-no-terminator";
    i.name.data = "memcpy-no-terminator";
    CHECK(mb_win_crt_bind(NULL, &i, &value) == MB_PE_BIND_BOUND);
    CHECK(value != 7);
    value = 7;
    ++i.name.size;
    CHECK(mb_win_crt_bind(NULL, &i, &value) == MB_PE_BIND_UNRESOLVED);
    CHECK(value == 7);
}
/* Catches crashes or output corruption for malformed callback records. */
static void invalid_imports(void) {
    uint64_t value = 7;
    mb_pe_import i = named("VCRUNTIME140.dll", "memcpy");
    CHECK(mb_win_crt_bind(NULL, NULL, &value) == MB_PE_BIND_ERROR);
    CHECK(mb_win_crt_bind(NULL, &i, NULL) == MB_PE_BIND_ERROR);
    i.library.data = NULL;
    CHECK(mb_win_crt_bind(NULL, &i, &value) == MB_PE_BIND_ERROR);
    CHECK(value == 7);
    i = named("VCRUNTIME140.dll", "memcpy"); i.name.data = NULL;
    CHECK(mb_win_crt_bind(NULL, &i, &value) == MB_PE_BIND_ERROR);
    CHECK(value == 7);
    i.name.size = 0;
    CHECK(mb_win_crt_bind(NULL, &i, &value) == MB_PE_BIND_UNRESOLVED);
    CHECK(value == 7);
}
/* Catches corruption of Microsoft's extra callee-saved registers, including
 * RDI/RSI and all 128 bits of XMM6--XMM15. Larger ranges take vectorized paths. */
static void nonvolatile_registers(void) {
    const char src[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ+-";
    char dst[80] = {0};
    const char *names[] = {"memcpy", "memmove", "memset", "memcmp", "strlen", "strcmp"};
    for (size_t i = 0; i < 6; ++i) {
        const char *library = i < 4 ? "VCRUNTIME140.dll" : "api-ms-win-crt-string-l1-1-0.dll";
        uint64_t address = bind(library, names[i]);
        if (!address) continue;
        uint64_t a = (uint64_t)(uintptr_t)dst;
        uint64_t b = (uint64_t)(uintptr_t)src;
        uint64_t count = sizeof(src);
        if (i == 1) { a = (uint64_t)(uintptr_t)(dst + 1); b = (uint64_t)(uintptr_t)dst; }
        if (i == 2) b = 0x1a5;
        if (i == 4 || i == 5) a = (uint64_t)(uintptr_t)src;
        CHECK(mb_test_nonvolatile(address, a, b, count) == 0);
        if (i == 0) CHECK(memcmp(dst, src, sizeof(src)) == 0);
        if (i == 1) CHECK(memcmp(dst + 1, src, sizeof(src)) == 0);
        if (i == 2) {
            for (size_t j = 0; j < sizeof(src); ++j) CHECK((unsigned char)dst[j] == 0xa5);
            CHECK(dst[sizeof(src)] == '\0');
        }
    }
    const uint16_t wide_src[] = {0x0041, 0xd83d, 0xde80, 0xffff, 0};
    uint16_t wide_dst[6] = {0};
    const char *wide_names[] = {"wcslen", "wcscmp", "wcsncmp", "wcscpy"};
    for (size_t i = 0; i < 4; ++i) {
        uint64_t address = bind("api-ms-win-crt-string-l1-1-0.dll", wide_names[i]);
        if (!address) continue;
        uint64_t a = (uint64_t)(uintptr_t)(i == 3 ? wide_dst : wide_src);
        CHECK(mb_test_nonvolatile(address, a, (uint64_t)(uintptr_t)wide_src, 5) == 0);
        if (i == 3) CHECK(memcmp(wide_dst, wide_src, sizeof(wide_src)) == 0);
    }
    const char *search_names[] = {"memchr", "strchr", "strrchr", "strstr"};
    for (size_t i = 0; i < 4; ++i) {
        uint64_t address = bind("VCRUNTIME140.dll", search_names[i]);
        if (!address) continue;
        uint64_t value = i == 3 ? (uint64_t)(uintptr_t)"XYZ" : (uint64_t)'Z';
        CHECK(mb_test_nonvolatile(address, (uint64_t)(uintptr_t)src, value, sizeof(src)) == 0);
    }
}
int main(void) {
    copy_bytes(); move_overlap(); fill_bytes(); compare_bytes();
    string_length(); compare_strings(); exact_imports(); invalid_imports(); nonvolatile_registers();
    wide_length(); compare_wide(); compare_wide_counted(); copy_wide(); wide_boundaries();
    search_memory(); search_first_byte(); search_last_byte(); search_substring(); search_boundaries();
    if (failures) { fprintf(stderr, "%u checks failed\n", failures); return 1; }
    puts("19 groups passed: fourteen Microsoft x64 ABI calls, exact binding, invalid records, nonvolatile registers, guarded boundaries");
    return 0;
}
