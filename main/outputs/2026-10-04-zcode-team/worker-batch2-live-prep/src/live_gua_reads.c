/*
 * live_gua_reads.c
 *
 * Implementation of the rung-3 live-read sequence. See live_gua_reads.h
 * for provenance and the verdict mapping. C99, no OS calls, no allocation.
 *
 * SAFETY INVARIANT: the only memory reads in this whole file happen inside
 * igu_read(), through span->data, after igu_find_covering() proved the
 * whole window [addr, addr+len) lies inside ONE span with IGU_PROT_R.
 * There is no other dereference of any target address anywhere.
 */

#include "live_gua_reads.h"

#include <string.h>

/* Cited constants (provenance in live_gua_reads.h). */
#define IGU_RF_ALL_FLAGS       0x0FFFFFFFu  /* UnrealFlags.hpp:95 */
#define IGU_NUM_ELEM_PER_CHUNK 65536u       /* UOA-HPP:88 (64*1024 elements) */
#define IGU_ITEM_STRIDE        0x18u        /* SET-5_06 FUObjectItem */
#define IGU_UOBJECTBASE_SIZE   0x28u        /* SET-5_06 UObjectBase */
#define IGU_FUOBJECTITEM_SIZE  0x18u        /* SET-5_06 FUObjectItem */
#define IGU_GUA_HEADER_SIZE    0x30u        /* GUA+0x00 .. GUA+0x2F */

/* ------------------------------------------------------------------ */
/* Span table                                                          */
/* ------------------------------------------------------------------ */

int igu_span_readable(const igu_span* s) {
    return (s != NULL) && ((s->prot & IGU_PROT_R) != 0u);
}

void igu_span_set_data_identity(igu_span* s) {
    if (s != NULL) {
        s->data = (const uint8_t*)(uintptr_t)s->start;
    }
}

uint32_t igu_posix_mode_to_kvme(uint32_t posix_mode) {
    /* POSIX: R=4 W=2 X=1  ->  KVME: R=1 W=2 X=4. */
    uint32_t out = 0;
    if (posix_mode & 4u) out |= IGU_PROT_R;
    if (posix_mode & 2u) out |= IGU_PROT_W;
    if (posix_mode & 1u) out |= IGU_PROT_X;
    return out;
}

int igu_ptr_canonical(uint64_t p) {
    /* 48-bit user half; kernel-half or non-canonical patterns refuse. */
    return (p >> 47) == 0u;
}

int igu_find_covering(const igu_span_table* t, uint64_t addr, uint64_t len,
                      uint32_t* prot_out) {
    size_t i;
    if (t == NULL || t->spans == NULL || len == 0u) {
        return IGU_E_ARG;
    }
    /* Overflow-safe end computation. */
    if (addr > UINT64_MAX - (uint64_t)len) {
        return IGU_E_NO_SPAN;
    }
    for (i = 0; i < t->count; ++i) {
        const igu_span* s = &t->spans[i];
        if (addr >= s->start && (addr + (uint64_t)len) <= s->end) {
            /* Full containment in ONE span: never straddle, never guess. */
            if (prot_out != NULL) {
                *prot_out = s->prot;
            }
            return (int)i;
        }
    }
    return IGU_E_NO_SPAN;
}

int igu_read(const igu_span_table* t, uint64_t addr, void* out, size_t len) {
    int idx;
    const igu_span* s;
    if (t == NULL || out == NULL || len == 0u) {
        return IGU_E_ARG;
    }
    idx = igu_find_covering(t, addr, (uint64_t)len, NULL);
    if (idx == IGU_E_NO_SPAN) {
        /* Classify WHY it is not covered: fully inside a span that lacks
         * READ, or crossing span boundaries, or truly unmapped. */
        size_t i;
        uint64_t win_end = addr + (uint64_t)len;
        for (i = 0; i < t->count; ++i) {
            const igu_span* c = &t->spans[i];
            if (addr >= c->start && addr < c->end) {
                /* Starts inside some span. */
                if (win_end <= c->end) {
                    /* Fully inside: coverage exists, readability decides. */
                    return (c->prot & IGU_PROT_R) ? IGU_E_ARG
                                                  : IGU_E_NOT_READABLE;
                }
                return IGU_E_STRADDLE; /* leaves the starting span */
            }
        }
        /* Also detect start-before/end-inside straddles vs. truly unmapped:
         * if any span contains the LAST byte, the window straddles. */
        for (i = 0; i < t->count; ++i) {
            const igu_span* c = &t->spans[i];
            if (win_end > c->start && win_end <= c->end) {
                return IGU_E_STRADDLE;
            }
        }
        return IGU_E_NO_SPAN;
    }
    if (idx < 0) {
        return idx;
    }
    s = &t->spans[idx];
    if (!igu_span_readable(s)) {
        return IGU_E_NOT_READABLE;
    }
    memcpy(out, s->data + (addr - s->start), len);
    return IGU_OK;
}

int igu_read_u64(const igu_span_table* t, uint64_t addr, uint64_t* out) {
    uint8_t b[8];
    int rc = igu_read(t, addr, b, 8u);
    if (rc != IGU_OK) return rc;
    *out = (uint64_t)b[0] | ((uint64_t)b[1] << 8) | ((uint64_t)b[2] << 16) |
           ((uint64_t)b[3] << 24) | ((uint64_t)b[4] << 32) |
           ((uint64_t)b[5] << 40) | ((uint64_t)b[6] << 48) |
           ((uint64_t)b[7] << 56);
    return IGU_OK;
}

int igu_read_u32(const igu_span_table* t, uint64_t addr, uint32_t* out) {
    uint8_t b[4];
    int rc = igu_read(t, addr, b, 4u);
    if (rc != IGU_OK) return rc;
    *out = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
           ((uint32_t)b[3] << 24);
    return IGU_OK;
}

int igu_read_i32(const igu_span_table* t, uint64_t addr, int32_t* out) {
    uint32_t u = 0;
    int rc = igu_read_u32(t, addr, &u);
    if (rc != IGU_OK) return rc;
    *out = (int32_t)u;
    return IGU_OK;
}

/* ------------------------------------------------------------------ */
/* Rung-3 sequence                                                     */
/* ------------------------------------------------------------------ */

static void rec_reset(igu_rung3_record* rec, uint64_t gua_addr) {
    /* 0 == IGU_CSK_NOT_RUN for check fields; step statuses are set
     * explicitly on every path below. */
    memset(rec, 0, sizeof(*rec));
    rec->gua_addr = gua_addr;
}

int igu_probe_rung3(const igu_span_table* t, uint64_t gua_addr,
                    uint32_t scan_max, igu_rung3_record* rec) {
    uint8_t hdr[IGU_GUA_HEADER_SIZE];
    int rc;

    if (t == NULL || rec == NULL) {
        return IGU_E_ARG;
    }
    rec_reset(rec, gua_addr);
    if (scan_max == 0u || scan_max > IGU_SCAN_MAX_ITEMS) {
        scan_max = IGU_SCAN_MAX_ITEMS;
    }

    /* ---- header (GUA+0x00..0x2F): 96-byte window, layout A ---- */
    rc = igu_read(t, gua_addr, hdr, sizeof(hdr));
    if (rc != IGU_OK) {
        rec->st_hdr = rc;
        rec->rc = rc;
        return rc;
    }
    rec->st_hdr = IGU_OK;
    rec->first_gc     = (uint32_t)hdr[0x00] | ((uint32_t)hdr[0x01] << 8) |
                        ((uint32_t)hdr[0x02] << 16) | ((uint32_t)hdr[0x03] << 24);
    rec->last_non_gc  = (uint32_t)hdr[0x04] | ((uint32_t)hdr[0x05] << 8) |
                        ((uint32_t)hdr[0x06] << 16) | ((uint32_t)hdr[0x07] << 24);
    rec->max_not_gc   = (uint32_t)hdr[0x08] | ((uint32_t)hdr[0x09] << 8) |
                        ((uint32_t)hdr[0x0A] << 16) | ((uint32_t)hdr[0x0B] << 24);
    rec->open_dfgc    = (uint32_t)hdr[0x0C] | ((uint32_t)hdr[0x0D] << 8) |
                        ((uint32_t)hdr[0x0E] << 16) | ((uint32_t)hdr[0x0F] << 24);
    rec->max_elements = (uint32_t)hdr[0x20] | ((uint32_t)hdr[0x21] << 8) |
                        ((uint32_t)hdr[0x22] << 16) | ((uint32_t)hdr[0x23] << 24);
    rec->num_elements = (uint32_t)hdr[0x24] | ((uint32_t)hdr[0x25] << 8) |
                        ((uint32_t)hdr[0x26] << 16) | ((uint32_t)hdr[0x27] << 24);
    rec->max_chunks   = (uint32_t)hdr[0x28] | ((uint32_t)hdr[0x29] << 8) |
                        ((uint32_t)hdr[0x2A] << 16) | ((uint32_t)hdr[0x2B] << 24);
    rec->num_chunks   = (uint32_t)hdr[0x2C] | ((uint32_t)hdr[0x2D] << 8) |
                        ((uint32_t)hdr[0x2E] << 16) | ((uint32_t)hdr[0x2F] << 24);

    /* Non-gating info checks (evidence, never gate): */
    rec->info_max_eq_chunks =
        (rec->max_elements == rec->max_chunks * IGU_NUM_ELEM_PER_CHUNK)
            ? IGU_CSK_PASS : IGU_CSK_CONTRADICTED;
    rec->info_num_le_max =
        (rec->num_elements <= rec->max_elements) ? IGU_CSK_PASS
                                                 : IGU_CSK_CONTRADICTED;
    rec->info_nchunks_ok =
        (rec->num_chunks >= 1u && rec->num_chunks <= rec->max_chunks)
            ? IGU_CSK_PASS : IGU_CSK_CONTRADICTED;

    /* ---- a) Objects** = u64 @ GUA+0x10 ---- */
    rc = igu_read_u64(t, gua_addr + 0x10, &rec->objects_ptr);
    rec->st_a = rc;
    if (rc != IGU_OK) { rec->rc = rc; return rc; }
    if (rec->objects_ptr == 0u) {
        rec->st_a = IGU_R_NULL;
        rec->rc = IGU_R_NULL;
        return rec->rc;
    }
    if (!igu_ptr_canonical(rec->objects_ptr)) {
        rec->st_a = IGU_R_NONCANONICAL;
        rec->rc = IGU_R_NONCANONICAL;
        return rec->rc;
    }

    /* ---- b) chunk[i] pointer from Objects** ----
     * Scan from chunk 0 (the chunk holding items 0..65535); only
     * num_chunks entries can hold live items. */
    for (rec->chunk_index = 0; rec->chunk_index < rec->num_chunks;
         ++rec->chunk_index) {
        rc = igu_read_u64(t, rec->objects_ptr + (uint64_t)rec->chunk_index * 8u,
                          &rec->chunk_ptr);
        if (rc != IGU_OK) { rec->st_b = rc; rec->rc = rc; return rc; }
        if (rec->chunk_ptr == 0u) { rec->st_b = IGU_R_SKIPPED; continue; }
        if (!igu_ptr_canonical(rec->chunk_ptr)) {
            rec->st_b = IGU_R_NONCANONICAL;
            rec->rc = IGU_R_NONCANONICAL;
            return rec->rc;
        }
        rec->st_b = IGU_OK;
        break;
    }
    if (rec->st_b != IGU_OK) {
        rec->rc = (rec->st_b == IGU_R_SKIPPED) ? IGU_R_NULL : rec->st_b;
        return rec->rc;
    }

    /* ---- c) first non-null FUObjectItem (0x18) in that chunk ---- */
    {
        uint32_t slot;
        uint8_t item[IGU_FUOBJECTITEM_SIZE];
        uint64_t item_addr;
        rec->st_c = IGU_R_NULL; /* exhausted-scan default */
        for (slot = 0; slot < scan_max; ++slot) {
            item_addr = rec->chunk_ptr + (uint64_t)slot * IGU_ITEM_STRIDE;
            rc = igu_read(t, item_addr, item, sizeof(item));
            if (rc != IGU_OK) { rec->st_c = rc; rec->rc = rc; return rc; }
            rec->item_object =
                (uint64_t)item[0x00] | ((uint64_t)item[0x01] << 8) |
                ((uint64_t)item[0x02] << 16) | ((uint64_t)item[0x03] << 24) |
                ((uint64_t)item[0x04] << 32) | ((uint64_t)item[0x05] << 40) |
                ((uint64_t)item[0x06] << 48) | ((uint64_t)item[0x07] << 56);
            rec->item_flags =
                (uint32_t)item[0x08] | ((uint32_t)item[0x09] << 8) |
                ((uint32_t)item[0x0A] << 16) | ((uint32_t)item[0x0B] << 24);
            rec->item_cluster =
                (uint32_t)item[0x0C] | ((uint32_t)item[0x0D] << 8) |
                ((uint32_t)item[0x0E] << 16) | ((uint32_t)item[0x0F] << 24);
            rec->item_serial =
                (uint32_t)item[0x10] | ((uint32_t)item[0x11] << 8) |
                ((uint32_t)item[0x12] << 16) | ((uint32_t)item[0x13] << 24);
            rec->items_scanned = slot + 1u;
            if (rec->item_object != 0u) {
                if (!igu_ptr_canonical(rec->item_object)) {
                    rec->st_c = IGU_R_NONCANONICAL;
                    rec->rc = IGU_R_NONCANONICAL;
                    return rec->rc;
                }
                rec->st_c = IGU_OK;
                break;
            }
            /* null item: legal GC/freed slot; keep raw fields of the last
             * one only for evidence; keep scanning. */
        }
        if (rec->st_c != IGU_OK) {
            rec->rc = rec->st_c; /* IGU_R_NULL: no candidates in scan */
            return rec->rc;
        }
    }

    /* ---- d) UObjectBase (0x28) at item_object ---- */
    {
        uint8_t obj[IGU_UOBJECTBASE_SIZE];
        rc = igu_read(t, rec->item_object, obj, sizeof(obj));
        if (rc != IGU_OK) { rec->st_d = rc; rec->rc = rc; return rc; }
        rec->st_d = IGU_OK;
        rec->vptr =
            (uint64_t)obj[0x00] | ((uint64_t)obj[0x01] << 8) |
            ((uint64_t)obj[0x02] << 16) | ((uint64_t)obj[0x03] << 24) |
            ((uint64_t)obj[0x04] << 32) | ((uint64_t)obj[0x05] << 40) |
            ((uint64_t)obj[0x06] << 48) | ((uint64_t)obj[0x07] << 56);
        rec->obj_flags =
            (uint32_t)obj[0x08] | ((uint32_t)obj[0x09] << 8) |
            ((uint32_t)obj[0x0A] << 16) | ((uint32_t)obj[0x0B] << 24);
        rec->internal_index =
            (int32_t)((uint32_t)obj[0x0C] | ((uint32_t)obj[0x0D] << 8) |
                      ((uint32_t)obj[0x0E] << 16) | ((uint32_t)obj[0x0F] << 24));
        rec->class_ptr =
            (uint64_t)obj[0x10] | ((uint64_t)obj[0x11] << 8) |
            ((uint64_t)obj[0x12] << 16) | ((uint64_t)obj[0x13] << 24) |
            ((uint64_t)obj[0x14] << 32) | ((uint64_t)obj[0x15] << 40) |
            ((uint64_t)obj[0x16] << 48) | ((uint64_t)obj[0x17] << 56);
        memcpy(rec->name_raw, &obj[0x18], 8u); /* RAW; never interpreted */
        rec->outer_ptr =
            (uint64_t)obj[0x20] | ((uint64_t)obj[0x21] << 8) |
            ((uint64_t)obj[0x22] << 16) | ((uint64_t)obj[0x23] << 24) |
            ((uint64_t)obj[0x24] << 32) | ((uint64_t)obj[0x25] << 40) |
            ((uint64_t)obj[0x26] << 48) | ((uint64_t)obj[0x27] << 56);
    }

    /* ---- e) checks ---- */
    /* vtable: non-null, canonical, 8-byte window readable. */
    if (rec->vptr == 0u || !igu_ptr_canonical(rec->vptr)) {
        rec->chk_vptr = IGU_CSK_CONTRADICTED;
    } else {
        uint32_t prot = 0;
        rc = igu_find_covering(t, rec->vptr, 8u, &prot);
        if (rc == IGU_E_NO_SPAN)          rec->chk_vptr = IGU_CSK_UNAVAILABLE;
        else if (rc < 0)                  rec->chk_vptr = IGU_CSK_UNAVAILABLE;
        else if (!(prot & IGU_PROT_R))    rec->chk_vptr = IGU_CSK_UNAVAILABLE;
        else                              rec->chk_vptr = IGU_CSK_PASS;
    }

    /* flags: RF_AllFlags mask. */
    rec->chk_flags = ((rec->obj_flags & ~IGU_RF_ALL_FLAGS) == 0u)
                         ? IGU_CSK_PASS : IGU_CSK_CONTRADICTED;

    /* index bounds. */
    if (rec->internal_index >= 0 &&
        (uint32_t)rec->internal_index < rec->num_elements) {
        rec->chk_index_bounds = IGU_CSK_PASS;
    } else {
        rec->chk_index_bounds = IGU_CSK_CONTRADICTED;
    }

    /* round-trip: item[internal_index].Object == item_object, via the
     * cited chunk math (batch2 item_addr parity). */
    if (rec->chk_index_bounds == IGU_CSK_PASS) {
        const uint32_t idx = (uint32_t)rec->internal_index;
        const uint32_t ch = idx / IGU_NUM_ELEM_PER_CHUNK;
        const uint32_t sl = idx % IGU_NUM_ELEM_PER_CHUNK;
        uint64_t ch_ptr = 0;
        uint64_t obj_at_idx = 0;
        if (ch >= rec->num_chunks) {
            rec->chk_roundtrip = IGU_CSK_UNAVAILABLE; /* chunk not allocated */
        } else {
            rc = igu_read_u64(t, rec->objects_ptr + (uint64_t)ch * 8u, &ch_ptr);
            if (rc != IGU_OK) {
                rec->chk_roundtrip = IGU_CSK_UNAVAILABLE;
            } else if (ch_ptr == 0u) {
                rec->chk_roundtrip = IGU_CSK_UNAVAILABLE;
            } else {
                rc = igu_read_u64(t, ch_ptr + (uint64_t)sl * IGU_ITEM_STRIDE,
                                  &obj_at_idx);
                if (rc != IGU_OK) {
                    rec->chk_roundtrip = IGU_CSK_UNAVAILABLE;
                } else {
                    rec->chk_roundtrip =
                        (obj_at_idx == rec->item_object) ? IGU_CSK_PASS
                                                         : IGU_CSK_CONTRADICTED;
                }
            }
        }
    } else {
        rec->chk_roundtrip = IGU_CSK_NOT_RUN;
    }

    /* class: non-null, canonical, readable (batch2 OBJ_R_CLASSPTR_BAD
     * family; coverage failure is UNAVAILABLE here, divergence from
     * batch2's Refute-mapping documented in the header + REPORT). */
    if (rec->class_ptr == 0u || !igu_ptr_canonical(rec->class_ptr)) {
        rec->chk_class = IGU_CSK_CONTRADICTED;
    } else {
        uint32_t prot = 0;
        rc = igu_find_covering(t, rec->class_ptr, 8u, &prot);
        if (rc < 0)                        rec->chk_class = IGU_CSK_UNAVAILABLE;
        else if (!(prot & IGU_PROT_R))     rec->chk_class = IGU_CSK_UNAVAILABLE;
        else                               rec->chk_class = IGU_CSK_PASS;
    }

    /* outer: null-or-readable. */
    if (rec->outer_ptr == 0u) {
        rec->chk_outer = IGU_CSK_PASS;
    } else if (!igu_ptr_canonical(rec->outer_ptr)) {
        rec->chk_outer = IGU_CSK_CONTRADICTED;
    } else {
        uint32_t prot = 0;
        rc = igu_find_covering(t, rec->outer_ptr, 8u, &prot);
        if (rc < 0)                        rec->chk_outer = IGU_CSK_UNAVAILABLE;
        else if (!(prot & IGU_PROT_R))     rec->chk_outer = IGU_CSK_UNAVAILABLE;
        else                               rec->chk_outer = IGU_CSK_PASS;
    }

    /* Overall rc: first CONTRADICTED check wins (refute-class), else
     * first UNAVAILABLE (inconclusive-class), else OK. */
    if (rec->chk_vptr == IGU_CSK_CONTRADICTED ||
        rec->chk_flags == IGU_CSK_CONTRADICTED ||
        rec->chk_index_bounds == IGU_CSK_CONTRADICTED ||
        rec->chk_roundtrip == IGU_CSK_CONTRADICTED ||
        rec->chk_class == IGU_CSK_CONTRADICTED ||
        rec->chk_outer == IGU_CSK_CONTRADICTED) {
        rec->rc = IGU_R_CHECK_FAIL;
    } else if (rec->chk_vptr == IGU_CSK_UNAVAILABLE ||
               rec->chk_roundtrip == IGU_CSK_UNAVAILABLE ||
               rec->chk_class == IGU_CSK_UNAVAILABLE ||
               rec->chk_outer == IGU_CSK_UNAVAILABLE) {
        rec->rc = IGU_E_NO_SPAN; /* coverage-class; see rec fields for which */
    } else {
        rec->rc = IGU_OK;
    }
    return rec->rc;
}

int igu_rung3_class(const igu_rung3_record* rec) {
    if (rec == NULL) {
        return IGU_E_ARG;
    }
    if (rec->st_hdr != IGU_OK || rec->st_a != IGU_OK ||
        rec->st_b != IGU_OK || rec->st_c != IGU_OK || rec->st_d != IGU_OK) {
        /* Step failures: semantic ones refute, coverage ones are
         * inconclusive. */
        const int st = rec->st_hdr != IGU_OK ? rec->st_hdr :
                       rec->st_a   != IGU_OK ? rec->st_a   :
                       rec->st_b   != IGU_OK ? rec->st_b   :
                       rec->st_c   != IGU_OK ? rec->st_c   : rec->st_d;
        if (st == IGU_R_NONCANONICAL) {
            return 2; /* non-canonical forbids the bytes (batch2 parity) */
        }
        return 1; /* coverage failure or exhausted scan: unavailable */
    }
    if (rec->chk_vptr == IGU_CSK_CONTRADICTED ||
        rec->chk_flags == IGU_CSK_CONTRADICTED ||
        rec->chk_index_bounds == IGU_CSK_CONTRADICTED ||
        rec->chk_roundtrip == IGU_CSK_CONTRADICTED ||
        rec->chk_class == IGU_CSK_CONTRADICTED ||
        rec->chk_outer == IGU_CSK_CONTRADICTED ||
        rec->info_max_eq_chunks == IGU_CSK_CONTRADICTED ||
        rec->info_num_le_max == IGU_CSK_CONTRADICTED ||
        rec->info_nchunks_ok == IGU_CSK_CONTRADICTED) {
        /* Info checks are non-gating evidence, so a CONTRADICTED info check
         * alone with a valid object chain is still only recorded, not a
         * refutation of the chain; handled below. */
        const int gating = (rec->chk_vptr == IGU_CSK_CONTRADICTED ||
                            rec->chk_flags == IGU_CSK_CONTRADICTED ||
                            rec->chk_index_bounds == IGU_CSK_CONTRADICTED ||
                            rec->chk_roundtrip == IGU_CSK_CONTRADICTED ||
                            rec->chk_class == IGU_CSK_CONTRADICTED ||
                            rec->chk_outer == IGU_CSK_CONTRADICTED);
        if (gating) {
            return 2;
        }
    }
    if (rec->chk_vptr == IGU_CSK_UNAVAILABLE ||
        rec->chk_roundtrip == IGU_CSK_UNAVAILABLE ||
        rec->chk_class == IGU_CSK_UNAVAILABLE ||
        rec->chk_outer == IGU_CSK_UNAVAILABLE) {
        return 1;
    }
    return (rec->rc == IGU_OK) ? 0 : 1;
}
