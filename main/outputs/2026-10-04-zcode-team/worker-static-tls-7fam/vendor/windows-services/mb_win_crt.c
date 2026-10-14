#include "mb_win_crt.h"

_Static_assert(sizeof(&mb_win_memcpy) == sizeof(uintptr_t),
    "AMD64 function addresses must fit IAT slots");

void *MB_WIN_X64_ABI mb_win_memcpy(void *destination, const void *source,
    size_t count) {
    unsigned char *dst = destination;
    const unsigned char *src = source;
    for (size_t i = 0; i < count; ++i) dst[i] = src[i];
    return destination;
}

void *MB_WIN_X64_ABI mb_win_memmove(void *destination, const void *source,
    size_t count) {
    unsigned char *dst = destination;
    const unsigned char *src = source;
    if (dst == src) return destination;
    /* AMD64 has flat addresses. Integer comparison avoids C relational
     * comparison between pointers to unrelated objects. Both directions are
     * valid for disjoint ranges; higher destinations copy backwards. */
    if ((uintptr_t)dst > (uintptr_t)src) {
        while (count) { --count; dst[count] = src[count]; }
    } else {
        for (size_t i = 0; i < count; ++i) dst[i] = src[i];
    }
    return destination;
}

void *MB_WIN_X64_ABI mb_win_memset(void *destination, int value, size_t count) {
    unsigned char *dst = destination;
    for (size_t i = 0; i < count; ++i) dst[i] = (unsigned char)value;
    return destination;
}

int MB_WIN_X64_ABI mb_win_memcmp(const void *first, const void *second,
    size_t count) {
    const unsigned char *a = first, *b = second;
    for (size_t i = 0; i < count; ++i) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

size_t MB_WIN_X64_ABI mb_win_strlen(const char *string) {
    size_t count = 0;
    while (string[count]) ++count;
    return count;
}

int MB_WIN_X64_ABI mb_win_strcmp(const char *first, const char *second) {
    const unsigned char *a = (const unsigned char *)first;
    const unsigned char *b = (const unsigned char *)second;
    while (*a && *a == *b) { ++a; ++b; }
    return (int)*a - (int)*b;
}

void *MB_WIN_X64_ABI mb_win_memchr(const void *buffer, int value, size_t count) {
    const unsigned char *bytes = buffer;
    unsigned char wanted = (unsigned char)value;
    for (size_t i = 0; i < count; ++i) {
        if (bytes[i] == wanted) return (void *)(bytes + i);
    }
    return NULL;
}

char *MB_WIN_X64_ABI mb_win_strchr(const char *string, int value) {
    unsigned char wanted = (unsigned char)value;
    for (;;) {
        if ((unsigned char)*string == wanted) return (char *)string;
        if (*string == 0) return NULL;
        ++string;
    }
}

char *MB_WIN_X64_ABI mb_win_strrchr(const char *string, int value) {
    unsigned char wanted = (unsigned char)value;
    const char *last = NULL;
    for (;;) {
        if ((unsigned char)*string == wanted) last = string;
        if (*string == 0) return (char *)last;
        ++string;
    }
}

char *MB_WIN_X64_ABI mb_win_strstr(const char *string, const char *needle) {
    if (*needle == 0) return (char *)string;
    for (; *string; ++string) {
        const char *candidate = string, *match = needle;
        while (*match && *candidate == *match) { ++candidate; ++match; }
        if (*match == 0) return (char *)string;
    }
    return NULL;
}

size_t MB_WIN_X64_ABI mb_win_wcslen(const uint16_t *string) {
    size_t count = 0;
    while (string[count]) ++count;
    return count;
}

int MB_WIN_X64_ABI mb_win_wcscmp(const uint16_t *first, const uint16_t *second) {
    while (*first && *first == *second) { ++first; ++second; }
    return (int)*first - (int)*second;
}

int MB_WIN_X64_ABI mb_win_wcsncmp(const uint16_t *first, const uint16_t *second,
    size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (first[i] != second[i]) return (int)first[i] - (int)second[i];
        if (first[i] == 0) return 0;
    }
    return 0;
}

uint16_t *MB_WIN_X64_ABI mb_win_wcscpy(uint16_t *destination,
    const uint16_t *source) {
    uint16_t *out = destination;
    while ((*out++ = *source++) != 0) {}
    return destination;
}

static unsigned char ascii_lower(unsigned char byte) {
    return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte + ('a' - 'A')) : byte;
}

static int matches(mb_pe_string view, const char *expected, size_t size,
    int fold_case) {
    if (view.size != size) return 0;
    for (size_t i = 0; i < size; ++i) {
        unsigned char actual = (unsigned char)view.data[i];
        unsigned char wanted = (unsigned char)expected[i];
        if (fold_case) { actual = ascii_lower(actual); wanted = ascii_lower(wanted); }
        if (actual != wanted) return 0;
    }
    return 1;
}

#define LIBRARY_IS(text) matches(import->library, text, sizeof(text) - 1, 1)
#define BIND_NAME(text, function) \
    if (matches(import->name, text, sizeof(text) - 1, 0)) { \
        *value = (uint64_t)(uintptr_t)&function; return MB_PE_BIND_BOUND; \
    }

mb_pe_bind_result mb_win_crt_bind(void *context, const mb_pe_import *import,
    uint64_t *value) {
    (void)context;
    if (!import || !value || (!import->library.data && import->library.size) ||
        (!import->name.data && import->name.size)) return MB_PE_BIND_ERROR;
    if (import->by_ordinal) return MB_PE_BIND_UNRESOLVED;
    if (LIBRARY_IS("VCRUNTIME140.dll")) {
        BIND_NAME("memcpy", mb_win_memcpy)
        BIND_NAME("memmove", mb_win_memmove)
        BIND_NAME("memset", mb_win_memset)
        BIND_NAME("memcmp", mb_win_memcmp)
        BIND_NAME("memchr", mb_win_memchr)
        BIND_NAME("strchr", mb_win_strchr)
        BIND_NAME("strrchr", mb_win_strrchr)
        BIND_NAME("strstr", mb_win_strstr)
    } else if (LIBRARY_IS("api-ms-win-crt-string-l1-1-0.dll")) {
        BIND_NAME("strlen", mb_win_strlen)
        BIND_NAME("strcmp", mb_win_strcmp)
        BIND_NAME("wcslen", mb_win_wcslen)
        BIND_NAME("wcscmp", mb_win_wcscmp)
        BIND_NAME("wcsncmp", mb_win_wcsncmp)
        BIND_NAME("wcscpy", mb_win_wcscpy)
    }
    return MB_PE_BIND_UNRESOLVED;
}

#undef BIND_NAME
#undef LIBRARY_IS
