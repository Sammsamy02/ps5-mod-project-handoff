#include "pe_image.h"
#include <limits.h>
#include <string.h>

/* Byte readers avoid alignment, host endianness and struct packing assumptions. */
static uint16_t u16(const unsigned char *p) { return (uint16_t)(p[0] | (uint16_t)p[1] << 8); }
static uint32_t u32(const unsigned char *p) { return (uint32_t)u16(p) | (uint32_t)u16(p + 2) << 16; }
static uint64_t u64(const unsigned char *p) { return (uint64_t)u32(p) | (uint64_t)u32(p + 4) << 32; }
static void put64(unsigned char *p, uint64_t v) { unsigned i; for (i = 0; i < 8; ++i) p[i] = (unsigned char)(v >> (8 * i)); }
static int fits(uint64_t at, uint64_t n, uint64_t size) { return at <= size && n <= size - at; }
static int power2(uint32_t n) { return n && !(n & (n - 1)); }
static int intersects(uint64_t a, uint64_t n, uint64_t b, uint64_t m) {
    return n && m && (a <= b ? b - a < n : a - b < m);
}
static int valid_memory(const void *p, size_t n) { return p && n <= UINTPTR_MAX - (uintptr_t)p; }
static int aliases(const void *a, size_t n, const void *b, size_t m) {
    return a && b && intersects((uintptr_t)a, n, (uintptr_t)b, m);
}
static mb_pe_status fail(mb_pe_error *e, mb_pe_status code, uint32_t rva, uint32_t detail) {
    if (e) { e->code = code; e->rva = rva; e->detail = detail; }
    return code;
}
typedef struct pe {
    const unsigned char *file;
    size_t size;
    mb_pe_info info;
    uint32_t string_budget;
    mb_pe_error *error;
} pe;
#define TRY(x) do { mb_pe_status rc_ = (x); if (rc_ != MB_PE_OK) return rc_; } while (0)

static const unsigned char *raw(const pe *p, uint32_t rva, uint32_t n) {
    unsigned i;
    if (fits(rva, n, p->info.headers_size)) return p->file + rva;
    for (i = 0; i < p->info.section_count; ++i) {
        const mb_pe_section *s = &p->info.sections[i];
        if (rva >= s->rva && fits(rva - s->rva, n, s->raw_size))
            return p->file + s->raw_offset + (rva - s->rva);
    }
    return NULL;
}
static int mapped(const pe *p, uint32_t rva, uint32_t n) {
    unsigned i;
    if (fits(rva, n, p->info.headers_size)) return 1;
    for (i = 0; i < p->info.section_count; ++i) {
        const mb_pe_section *s = &p->info.sections[i];
        if (rva >= s->rva && fits(rva - s->rva, n, s->mapped_size)) return 1;
    }
    return 0;
}
static mb_pe_status string_at(pe *p, uint32_t rva, uint32_t bound, mb_pe_string *s) {
    const unsigned char *v = raw(p, rva, 1);
    uint32_t n = 0;
    if (!v) return fail(p->error, MB_PE_BAD_RVA, rva, 1);
    while (n <= MB_PE_MAX_STRING && n < bound) {
        const unsigned char *c;
        if (n > p->size - (size_t)(v - p->file) || n > UINT32_MAX - rva ||
            !(c = raw(p, rva + n, 1)) || c != v + n)
            return fail(p->error, MB_PE_BAD_RVA, rva, n);
        if (!p->string_budget--) return fail(p->error, MB_PE_LIMIT, rva, n);
        if (!*c) { s->data = (const char *)v; s->size = n; return MB_PE_OK; }
        ++n;
    }
    return fail(p->error, MB_PE_LIMIT, rva, n);
}
static mb_pe_status headers(pe *p) {
    const unsigned char *f = p->file, *o;
    uint32_t pos, count, section_at;
    uint16_t opt;
    unsigned i, j;
    mb_pe_info *v = &p->info;
    if (p->size < 64) return fail(p->error, MB_PE_TRUNCATED, 0, 64);
    if (u16(f) != 0x5a4d) return fail(p->error, MB_PE_BAD_FORMAT, 0, 0);
    pos = u32(f + 0x3c);
    if (!fits(pos, 24, p->size)) return fail(p->error, MB_PE_TRUNCATED, 0, pos);
    if (pos < 64 || u32(f + pos) != 0x4550) return fail(p->error, MB_PE_BAD_FORMAT, 0, pos);
    if (u16(f + pos + 4) != 0x8664) return fail(p->error, MB_PE_UNSUPPORTED, 0, u16(f + pos + 4));
    v->section_count = u16(f + pos + 6); opt = u16(f + pos + 20);
    v->characteristics = u16(f + pos + 22);
    if ((v->characteristics & 0x2002) != 0x2002) return fail(p->error, MB_PE_UNSUPPORTED, 0, v->characteristics);
    if (!v->section_count || v->section_count > MB_PE_MAX_SECTIONS) return fail(p->error, MB_PE_LIMIT, 0, v->section_count);
    if (!fits((uint64_t)pos + 24, opt, p->size)) return fail(p->error, MB_PE_TRUNCATED, 0, opt);
    if (opt < 112) return fail(p->error, MB_PE_BAD_FORMAT, 0, opt);
    o = f + pos + 24;
    if (u16(o) != 0x20b) return fail(p->error, MB_PE_UNSUPPORTED, 0, u16(o));
    count = u32(o + 108);
    if (count > MB_PE_MAX_DIRECTORIES) return fail(p->error, MB_PE_UNSUPPORTED, 0, count);
    if (count > (unsigned)(opt - 112) / 8) return fail(p->error, MB_PE_BAD_FORMAT, 0, count);
    v->preferred_base = u64(o + 24); v->entry_rva = u32(o + 16);
    v->section_alignment = u32(o + 32); v->file_alignment = u32(o + 36);
    v->image_size = u32(o + 56); v->headers_size = u32(o + 60); v->dll_characteristics = u16(o + 70);
    if (!power2(v->section_alignment) || !power2(v->file_alignment) || v->section_alignment < v->file_alignment ||
        (v->section_alignment < 4096 ? v->file_alignment != v->section_alignment :
          (v->file_alignment < 512 || v->file_alignment > 65536)))
        return fail(p->error, MB_PE_BAD_FORMAT, 0, v->section_alignment);
    if (!v->image_size || v->image_size > MB_PE_MAX_IMAGE_SIZE) return fail(p->error, MB_PE_LIMIT, 0, v->image_size);
    if (v->image_size % v->section_alignment || !v->headers_size || v->headers_size % v->file_alignment ||
        !fits(0, v->headers_size, v->image_size) || v->preferred_base > UINT64_MAX - v->image_size)
        return fail(p->error, MB_PE_BAD_FORMAT, 0, v->headers_size);
    if (!fits(0, v->headers_size, p->size)) return fail(p->error, MB_PE_TRUNCATED, 0, v->headers_size);
    if ((uint64_t)pos + 24 + opt > UINT32_MAX) return fail(p->error, MB_PE_BAD_FORMAT, 0, pos);
    section_at = pos + 24 + opt;
    if (!fits(section_at, (uint64_t)v->section_count * 40, v->headers_size))
        return fail(p->error, MB_PE_BAD_FORMAT, 0, section_at);
    for (i = 0; i < v->section_count; ++i) {
        const unsigned char *h = f + section_at + i * 40;
        mb_pe_section *s = &v->sections[i];
        s->virtual_size = u32(h + 8); s->rva = u32(h + 12); s->raw_size = u32(h + 16);
        s->raw_offset = u32(h + 20); s->characteristics = u32(h + 36);
        s->mapped_size = s->virtual_size > s->raw_size ? s->virtual_size : s->raw_size;
        if (s->rva % v->section_alignment || s->raw_size % v->file_alignment ||
            (s->raw_size && (s->raw_offset % v->file_alignment || s->raw_offset < v->headers_size)) ||
            (s->mapped_size && (s->rva < v->headers_size || !fits(s->rva, s->mapped_size, v->image_size))))
            return fail(p->error, MB_PE_BAD_FORMAT, s->rva, i);
        if (v->section_alignment < 4096 && s->raw_size && s->rva != s->raw_offset)
            return fail(p->error, MB_PE_UNSUPPORTED, s->rva, i);
        if (s->raw_size && !fits(s->raw_offset, s->raw_size, p->size)) return fail(p->error, MB_PE_TRUNCATED, s->rva, i);
        for (j = 0; j < i; ++j) {
            const mb_pe_section *t = &v->sections[j];
            if (intersects(s->rva, s->mapped_size, t->rva, t->mapped_size) ||
                intersects(s->raw_offset, s->raw_size, t->raw_offset, t->raw_size))
                return fail(p->error, MB_PE_OVERLAP, s->rva, i);
        }
    }
    if (v->entry_rva && !mapped(p, v->entry_rva, 1)) return fail(p->error, MB_PE_BAD_RVA, v->entry_rva, 0);
    for (i = 0; i < count; ++i) {
        mb_pe_directory *d = &v->directories[i]; d->rva = u32(o + 112 + i * 8); d->size = u32(o + 116 + i * 8);
        if (!d->rva && !d->size) continue;
        /* GlobalPtr alone has a zero Size in the PE specification. Security is a file offset. */
        if (!d->rva || (!d->size && i != 8)) return fail(p->error, MB_PE_BAD_FORMAT, d->rva, i);
        if (i == 4) {
            if (!fits(d->rva, d->size, p->size)) return fail(p->error, MB_PE_TRUNCATED, d->rva, i);
        } else if (!mapped(p, d->rva, d->size ? d->size : 8)) return fail(p->error, MB_PE_BAD_RVA, d->rva, i);
    }
    if (v->entry_rva) v->requirements |= MB_PE_REQUIRES_ENTRY;
    if (v->directories[1].rva) v->requirements |= MB_PE_REQUIRES_IMPORTS;
    if (v->directories[9].rva) v->requirements |= MB_PE_REQUIRES_TLS;
    if (v->directories[3].rva) v->requirements |= MB_PE_REQUIRES_EXCEPTION;
    if (v->directories[13].rva) v->requirements |= MB_PE_REQUIRES_DELAY_IMPORT;
    if (v->directories[10].rva) v->requirements |= MB_PE_REQUIRES_LOAD_CONFIG;
    if (v->directories[14].rva) v->requirements |= MB_PE_REQUIRES_CLR;
    return MB_PE_OK;
}
static uint64_t original64(const pe *p, uint32_t rva) {
    uint64_t v = 0; unsigned i;
    for (i = 0; i < 8; ++i) { const unsigned char *b = raw(p, rva + i, 1); if (b) v |= (uint64_t)*b << (8 * i); }
    return v;
}
/* During prepare, the caller's image first serves as a byte-granular write map.
 * This rejects duplicate/partial writes in linear time without allocation. */
static mb_pe_status mark_write(pe *p, unsigned char *writes, uint32_t rva) {
    unsigned i;
    for (i = 0; i < 8; ++i)
        if (writes[rva + i]) return fail(p->error, MB_PE_OVERLAP, rva, 8);
    memset(writes + rva, 1, 8);
    return MB_PE_OK;
}
static mb_pe_status relocations(pe *p, uint64_t base, unsigned char *image, int check_base, uint32_t *total, unsigned char *writes) {
    mb_pe_directory d = p->info.directories[5]; uint32_t used = 0, n = 0;
    uint64_t delta = base >= p->info.preferred_base ? base - p->info.preferred_base : p->info.preferred_base - base;
    if (check_base && delta && (!d.rva || (p->info.characteristics & 1)))
        return fail(p->error, MB_PE_RELOCATIONS_REQUIRED, d.rva, 0);
    while (used < d.size) {
        const unsigned char *h; uint32_t bytes, page, i;
        if (d.size - used < 8 || !(h = raw(p, d.rva + used, 8))) return fail(p->error, MB_PE_BAD_FORMAT, d.rva + used, 0);
        page = u32(h); bytes = u32(h + 4);
        if (bytes < 8 || bytes % 4 || bytes > d.size - used || page % 4096 || !raw(p, d.rva + used, bytes))
            return fail(p->error, MB_PE_BAD_FORMAT, d.rva + used, bytes);
        for (i = 8; i < bytes; i += 2) {
            uint16_t item = u16(h + i), type = item >> 12; uint32_t rva; uint64_t v;
            if (!type) continue;
            if (type != 10) return fail(p->error, MB_PE_UNSUPPORTED, d.rva + used + i, type);
            if (++n > MB_PE_MAX_ITEMS) return fail(p->error, MB_PE_LIMIT, d.rva + used + i, n);
            if (page > UINT32_MAX - (item & 4095)) return fail(p->error, MB_PE_BAD_RVA, page, item);
            rva = page + (item & 4095);
            if (!mapped(p, rva, 8)) return fail(p->error, MB_PE_BAD_RVA, rva, 8);
            v = original64(p, rva);
            if (check_base) {
                if (base >= p->info.preferred_base) {
                    if (v > UINT64_MAX - delta) return fail(p->error, MB_PE_RELOCATION_OVERFLOW, rva, 0);
                    v += delta;
                } else {
                    if (v < delta) return fail(p->error, MB_PE_RELOCATION_OVERFLOW, rva, 0);
                    v -= delta;
                }
            }
            if (writes) TRY(mark_write(p, writes, rva));
            if (image) put64(image + rva, v);
        }
        used += bytes;
    }
    *total = n;
    return MB_PE_OK;
}
static mb_pe_status imports(pe *p, unsigned char *image, mb_pe_binder binder, void *ctx, mb_pe_report *report, unsigned char *writes, mb_pe_import_visitor visit) {
    mb_pe_directory d = p->info.directories[1]; uint32_t offset = 0, descriptors = 0, count = 0;
    if (!d.rva) return MB_PE_OK;
    while (fits(offset, 20, d.size)) {
        const unsigned char *h = raw(p, d.rva + offset, 20);
        uint32_t lookup, iat, index;
        mb_pe_import imp;
        if (!h) return fail(p->error, MB_PE_BAD_RVA, d.rva + offset, 20);
        if (!u32(h) && !u32(h + 4) && !u32(h + 8) && !u32(h + 12) && !u32(h + 16)) {
            p->info.import_count = count; return MB_PE_OK;
        }
        if (++descriptors > 4096) return fail(p->error, MB_PE_LIMIT, d.rva + offset, descriptors);
        memset(&imp, 0, sizeof(imp));
        lookup = u32(h); iat = u32(h + 16);
        if (!iat || !u32(h + 12)) return fail(p->error, MB_PE_BAD_FORMAT, d.rva + offset, 0);
        if (!lookup) {
            if (u32(h + 4)) return fail(p->error, MB_PE_UNSUPPORTED, d.rva + offset, 0);
            lookup = iat;
        }
        TRY(string_at(p, u32(h + 12), MB_PE_MAX_STRING + 1, &imp.library));
        if (!imp.library.size) return fail(p->error, MB_PE_BAD_FORMAT, u32(h + 12), 0);
        for (index = 0; index <= MB_PE_MAX_ITEMS; ++index) {
            const unsigned char *t; uint64_t value, symbol;
            uint64_t step = (uint64_t)index * 8;
            if (step > UINT32_MAX - lookup || step > UINT32_MAX - iat ||
                !(t = raw(p, lookup + (uint32_t)step, 8)) || !mapped(p, iat + (uint32_t)step, 8))
                return fail(p->error, MB_PE_BAD_RVA, lookup, index);
            symbol = u64(t);
            if (!symbol) {
                if (original64(p, iat + (uint32_t)step))
                    return fail(p->error, MB_PE_BAD_FORMAT, iat + (uint32_t)step, 0);
                if (writes) TRY(mark_write(p, writes, iat + (uint32_t)step));
                break;
            }
            if (++count > MB_PE_MAX_ITEMS) return fail(p->error, MB_PE_LIMIT, lookup, count);
            imp.iat_rva = iat + (uint32_t)step; imp.name = (mb_pe_string){0}; imp.hint = 0; imp.ordinal = 0;
            imp.by_ordinal = (uint8_t)(symbol >> 63);
            if (imp.by_ordinal) {
                if (symbol & UINT64_C(0x7fffffffffff0000)) return fail(p->error, MB_PE_BAD_FORMAT, lookup + (uint32_t)step, 0);
                imp.ordinal = (uint16_t)symbol;
            } else {
                const unsigned char *hint;
                if (symbol > UINT32_MAX - 2 || !(hint = raw(p, (uint32_t)symbol, 2)))
                    return fail(p->error, MB_PE_BAD_RVA, (uint32_t)symbol, 2);
                imp.hint = u16(hint);
                TRY(string_at(p, (uint32_t)symbol + 2, MB_PE_MAX_STRING + 1, &imp.name));
                if (!imp.name.size) return fail(p->error, MB_PE_BAD_FORMAT, (uint32_t)symbol + 2, 0);
            }
            if (writes) TRY(mark_write(p, writes, imp.iat_rva));
            if (visit && visit(ctx, &imp)) {
                if (p->error) p->error->import = imp;
                return fail(p->error, MB_PE_VISITOR_ERROR, imp.iat_rva, count - 1);
            }
            if (image) {
                mb_pe_bind_result result = MB_PE_BIND_UNRESOLVED;
                value = 0;
                if (binder) result = binder(ctx, &imp, &value);
                if (result == MB_PE_BIND_BOUND) { put64(image + imp.iat_rva, value); ++report->imports_bound; }
                else if (result == MB_PE_BIND_UNRESOLVED) {
                    ++report->imports_unresolved;
                    /* Preserve the original slot, including the OFT==0 case. */
                    put64(image + imp.iat_rva, original64(p, imp.iat_rva));
                    if (p->error && p->error->code == MB_PE_OK) {
                        p->error->code = MB_PE_UNRESOLVED_IMPORTS; p->error->rva = imp.iat_rva; p->error->import = imp;
                    }
                } else {
                    if (p->error) p->error->import = imp;
                    return fail(p->error, MB_PE_BINDER_ERROR, imp.iat_rva, (uint32_t)result);
                }
            }
        }
        offset += 20;
    }
    return fail(p->error, MB_PE_BAD_FORMAT, d.rva + offset, 20);
}
static mb_pe_status export_value(pe *p, uint32_t ordinal, uint32_t rva, mb_pe_string name,
                                  mb_pe_export_visitor visit, void *ctx) {
    mb_pe_directory d = p->info.directories[0]; mb_pe_export record;
    memset(&record, 0, sizeof(record)); record.ordinal = ordinal; record.rva = rva; record.name = name;
    if (!rva) return MB_PE_OK; /* EAT holes are not callable or named exports. */
    if (rva >= d.rva && (uint64_t)rva < (uint64_t)d.rva + d.size) {
        TRY(string_at(p, rva, d.size - (rva - d.rva), &record.forwarder));
        if (!record.forwarder.size) return fail(p->error, MB_PE_BAD_FORMAT, rva, 0);
    } else if (!mapped(p, rva, 1)) return fail(p->error, MB_PE_BAD_RVA, rva, 1);
    if (visit && visit(ctx, &record)) return fail(p->error, MB_PE_VISITOR_ERROR, rva, ordinal);
    return MB_PE_OK;
}
static mb_pe_status exports(pe *p, mb_pe_export_visitor visit, void *ctx) {
    mb_pe_directory d = p->info.directories[0];
    const unsigned char *h, *functions, *names, *ordinals;
    uint32_t base, count, named, i;
    unsigned char seen[MB_PE_MAX_ITEMS / 8] = {0};
    if (!d.rva) return MB_PE_OK;
    if (d.size < 40 || !(h = raw(p, d.rva, 40))) return fail(p->error, MB_PE_BAD_FORMAT, d.rva, 40);
    base = u32(h + 16); count = u32(h + 20); named = u32(h + 24);
    if (count > MB_PE_MAX_ITEMS || named > MB_PE_MAX_ITEMS || (count && base > UINT32_MAX - (count - 1)))
        return fail(p->error, MB_PE_LIMIT, d.rva, count);
    functions = count ? raw(p, u32(h + 28), count * 4) : NULL;
    names = named ? raw(p, u32(h + 32), named * 4) : NULL;
    ordinals = named ? raw(p, u32(h + 36), named * 2) : NULL;
    if ((count && !functions) || (named && (!names || !ordinals))) return fail(p->error, MB_PE_BAD_RVA, d.rva, 0);
    p->info.export_count = count; p->info.export_name_count = named;
    for (i = 0; i < count; ++i) TRY(export_value(p, base + i, u32(functions + i * 4), (mb_pe_string){0}, NULL, NULL));
    for (i = 0; i < named; ++i) {
        uint16_t index = u16(ordinals + i * 2); mb_pe_string name;
        if (index >= count) return fail(p->error, MB_PE_BAD_FORMAT, u32(h + 36) + i * 2, index);
        TRY(string_at(p, u32(names + i * 4), MB_PE_MAX_STRING + 1, &name));
        if (!name.size) return fail(p->error, MB_PE_BAD_FORMAT, u32(names + i * 4), 0);
        seen[index / 8] |= (unsigned char)(1u << (index % 8));
        TRY(export_value(p, base + index, u32(functions + index * 4), name, visit, ctx));
    }
    for (i = 0; i < count; ++i)
        if (!(seen[i / 8] & (1u << (i % 8))))
            TRY(export_value(p, base + i, u32(functions + i * 4), (mb_pe_string){0}, visit, ctx));
    return MB_PE_OK;
}
static mb_pe_status parse(pe *p, const void *file, size_t size, mb_pe_error *error) {
    memset(p, 0, sizeof(*p)); p->file = file; p->size = size; p->error = error; p->string_budget = 64u * 1024u * 1024u;
    if (size > MB_PE_MAX_IMAGE_SIZE) return fail(error, MB_PE_LIMIT, 0, 0);
    TRY(headers(p));
    TRY(relocations(p, p->info.preferred_base, NULL, 0, &p->info.relocation_count, NULL));
    TRY(imports(p, NULL, NULL, NULL, NULL, NULL, NULL));
    TRY(exports(p, NULL, NULL));
    return MB_PE_OK;
}
static mb_pe_status arguments(const void *file, size_t n, void *image, size_t capacity,
                             mb_pe_info *info, mb_pe_report *report, mb_pe_error *error) {
    const void *ptr[5] = {file, image, info, report, error};
    size_t len[5] = {n, capacity, info ? sizeof(*info) : 0, report ? sizeof(*report) : 0, error ? sizeof(*error) : 0};
    unsigned i, j;
    for (i = 0; i < 5; ++i) if (len[i] && !valid_memory(ptr[i], len[i])) return MB_PE_INVALID_ARGUMENT;
    for (i = 0; i < 5; ++i) for (j = 0; j < i; ++j)
        if (aliases(ptr[i], len[i], ptr[j], len[j])) return MB_PE_ALIAS;
    if (error) memset(error, 0, sizeof(*error));
    if (info) memset(info, 0, sizeof(*info));
    if (report) memset(report, 0, sizeof(*report));
    if (!file || !n) return fail(error, MB_PE_INVALID_ARGUMENT, 0, 0);
    return MB_PE_OK;
}
mb_pe_status mb_pe_inspect(const void *file, size_t size, mb_pe_info *info, mb_pe_error *error) {
    pe p; mb_pe_status status;
    TRY(arguments(file, size, NULL, 0, info, NULL, error));
    if (!info) return fail(error, MB_PE_INVALID_ARGUMENT, 0, 0);
    status = parse(&p, file, size, error);
    if (status == MB_PE_OK) *info = p.info;
    return status;
}
mb_pe_status mb_pe_prepare(const void *file, size_t size, uint64_t base, void *image, size_t capacity,
                          mb_pe_binder binder, void *ctx, mb_pe_info *info, mb_pe_report *report, mb_pe_error *error) {
    pe p; unsigned i; uint32_t total = 0; mb_pe_status status;
    mb_pe_report result = {0};
    TRY(arguments(file, size, image, capacity, info, report, error));
    if (!image || !info || !report) return fail(error, MB_PE_INVALID_ARGUMENT, 0, 0);
    TRY(parse(&p, file, size, error));
    if (capacity < p.info.image_size) return fail(error, MB_PE_CAPACITY, 0, p.info.image_size);
    if (base > UINT64_MAX - p.info.image_size) return fail(error, MB_PE_RELOCATION_OVERFLOW, 0, 0);
    TRY(relocations(&p, base, NULL, 1, &total, NULL));
    memset(image, 0, p.info.image_size);
    status = relocations(&p, base, NULL, 1, &total, image);
    if (status == MB_PE_OK) {
        p.string_budget = 64u * 1024u * 1024u;
        status = imports(&p, NULL, NULL, NULL, NULL, image, NULL);
    }
    memset(image, 0, p.info.image_size);
    if (status != MB_PE_OK) return status;
    memcpy(image, file, p.info.headers_size);
    for (i = 0; i < p.info.section_count; ++i) {
        const mb_pe_section *s = &p.info.sections[i];
        if (s->raw_size) memcpy((unsigned char *)image + s->rva, (const unsigned char *)file + s->raw_offset, s->raw_size);
    }
    status = relocations(&p, base, image, 1, &total, NULL);
    result.logical_base = base; result.relocations_applied = base != p.info.preferred_base ? total : 0;
    if (status == MB_PE_OK) { p.string_budget = 64u * 1024u * 1024u; status = imports(&p, image, binder, ctx, &result, NULL, NULL); }
    if (status != MB_PE_OK) { memset(image, 0, p.info.image_size); return status; }
    result.image_prepared = 1; *info = p.info; *report = result;
    return result.imports_unresolved ? MB_PE_UNRESOLVED_IMPORTS : MB_PE_OK;
}
mb_pe_status mb_pe_visit_exports(const void *file, size_t size, mb_pe_export_visitor visitor, void *ctx, mb_pe_error *error) {
    pe p;
    TRY(arguments(file, size, NULL, 0, NULL, NULL, error));
    if (!visitor) return fail(error, MB_PE_INVALID_ARGUMENT, 0, 0);
    TRY(parse(&p, file, size, error));
    p.string_budget = 64u * 1024u * 1024u;
    return exports(&p, visitor, ctx);
}
mb_pe_status mb_pe_visit_imports(const void *file, size_t size, mb_pe_import_visitor visitor, void *ctx, mb_pe_error *error) {
    pe p;
    TRY(arguments(file, size, NULL, 0, NULL, NULL, error));
    if (!visitor) return fail(error, MB_PE_INVALID_ARGUMENT, 0, 0);
    TRY(parse(&p, file, size, error));
    p.string_budget = 64u * 1024u * 1024u;
    return imports(&p, NULL, NULL, ctx, NULL, NULL, visitor);
}
const char *mb_pe_status_name(mb_pe_status status) {
    static const char *const names[] = {"ok", "unresolved_imports", "invalid_argument", "alias", "truncated", "bad_format",
        "unsupported", "limit", "bad_rva", "overlap", "capacity", "relocations_required", "relocation_overflow", "binder_error", "visitor_error"};
    return (unsigned)status < sizeof(names) / sizeof(names[0]) ? names[status] : "unknown";
}
