/*
 * rung3_chunk_addon.c
 *
 * Implementation of the rung-3 chunk/object add-on. See rung3_chunk_addon.h
 * for provenance and verdict mapping. C99, no OS calls, no allocation.
 *
 * SAFETY INVARIANT (inherited from worker-batch2-live-prep/src/live_gua_reads.c):
 * the only memory reads in this whole file happen through igu_read()/
 * igu_read_u64()/igu_read_u32() — i.e. through span->data, after the whole
 * window [addr, addr+len) was proven covered by ONE span with IGU_PROT_R.
 * The VMMAP walk never dereferences anything (byte-assembled loads from the
 * caller's sysctl buffer only). There is no other dereference anywhere.
 */

#include "rung3_chunk_addon.h"

#include <string.h>

/* Cited constants (provenance in rung3_chunk_addon.h / live_gua_reads.h). */
#define IGU_RF_ALL_FLAGS       0x0FFFFFFFu  /* UnrealFlags.hpp:95            */
#define IGU_NUM_ELEM_PER_CHUNK 65536u       /* UOA-HPP:88 (64*1024 elements) */
#define IGU_ITEM_STRIDE        0x18u        /* SET-5_06 FUObjectItem         */
#define IGU_UOBJECTBASE_SIZE   0x28u        /* SET-5_06 UObjectBase          */
#define IGU_FUOBJECTITEM_SIZE  0x18u
#define IGU_GUA_HEADER_SIZE    0x30u        /* GUA+0x00 .. GUA+0x2F          */

/* kinfo_vmentry walk constants (pinned SDK sys/user.h; v3 vmmap_walk.c). */
#define RC3_KVE_STRUCTSIZE 0u
#define RC3_KVE_START      8u
#define RC3_KVE_END        16u
#define RC3_KVE_PROT       56u
#define RC3_VM_HDR         136u  /* bytes before kve_path */
#define RC3_MAX_EXTENT     (1ull << 44)

/* ------------------------------------------------------------------ */
/* Byte-assembled loads (never struct casts: records are 4-aligned)    */
/* ------------------------------------------------------------------ */

static uint32_t ld32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t ld64(const uint8_t* p) {
    return (uint64_t)ld32(p) | ((uint64_t)ld32(p + 4) << 32);
}

/* ------------------------------------------------------------------ */
/* Minimal VMMAP walk                                                  */
/* ------------------------------------------------------------------ */

uint32_t rc3_vmmap_walk_min(const uint8_t* buf, uint64_t len,
                            uint32_t kve_sizeof,
                            igu_span* out, uint32_t cap,
                            const uint64_t* targets, uint32_t ntargets,
                            rc3_vmstats* st) {
    uint32_t kept = 0;
    uint32_t use_nt;
    uint32_t full_mask;
    uint64_t off = 0;

    if (st == NULL) {
        return 0;
    }
    memset(st, 0, sizeof(*st));
    st->walk_mode = 1;
    if (buf == NULL) {
        return 0;
    }
    if (kve_sizeof < RC3_VM_HDR) {
        kve_sizeof = RC3_VM_HDR;
    }

    use_nt = ntargets;
    if (use_nt > RC3_MAX_TARGETS) {
        use_nt = RC3_MAX_TARGETS; /* clamped, recorded in targets_total */
    }
    st->targets_total = use_nt;
    /* ntargets==0 disables the early stop (full_mask stays 0). */
    full_mask = (use_nt >= 32u) ? 0xFFFFFFFFu : ((1u << use_nt) - 1u);

    while (off + RC3_VM_HDR <= len) {
        uint32_t ksz = ld32(buf + off);
        uint64_t start, end;
        uint32_t prot;
        int sane;

        if (off + (uint64_t)ksz > len) {
            /* Header present but the claimed record runs past the buffer:
             * partial tail, distinguished from an insane structsize. */
            st->stop_reason = RC3_WALK_STOP_PARTIAL;
            break;
        }
        if (ksz < RC3_VM_HDR || ksz > kve_sizeof || (ksz & 3u) != 0u) {
            st->stop_reason = RC3_WALK_STOP_INSANE; /* stop, never guess */
            break;
        }
        if (ksz == kve_sizeof) ++st->ksz_full; else ++st->ksz_var;
        if (ksz == 144u) ++st->ksz_144;   /* span-arbitration prediction P1 */

        start = ld64(buf + off + RC3_KVE_START);
        end   = ld64(buf + off + RC3_KVE_END);
        prot  = ld32(buf + off + RC3_KVE_PROT);
        ++st->total;

        /* Per-record sanity (v3 parity): real mappings have start<=end,
         * sane extent, SMALL protection word (KVME bits, <=0xff). */
        sane = start <= end && (end - start) <= RC3_MAX_EXTENT &&
               prot <= 0xffu;
        if (!sane) {
            ++st->insane;
        } else {
            ++st->retained;
            if (((start | end) & 0x3fffull) == 0) ++st->aligned16k;
            if (kept < cap) {
                igu_span* s = &out[kept++];
                s->start = start;
                s->end   = end;
                s->prot  = prot;
                /* Live discipline: identity data pointer. A host test MUST
                 * re-point s->data into fixture bytes before probing
                 * (igu_read would otherwise dereference a raw vaddr). */
                s->data  = (const uint8_t*)(uintptr_t)start;
                /* Early-stop coverage: target t is covered when this span
                 * with READ prot contains [t, t+8). */
                {
                    uint32_t k;
                    for (k = 0; k < use_nt; ++k) {
                        const uint64_t t = targets[k];
                        if (((st->targets_mask >> k) & 1u) == 0u &&
                            t >= start && (t + 8u) <= end &&
                            (prot & IGU_PROT_R) != 0u) {
                            st->targets_mask |= (1u << k);
                        }
                    }
                }
            } else {
                ++st->truncated;
            }
            if (full_mask != 0u && st->targets_mask == full_mask &&
                st->truncated == 0u) {
                /* Minimal by contract: every named target is covered by a
                 * retained readable span; do not drain the rest of the
                 * buffer. stop_off points past the last consumed record. */
                off += ksz;
                st->stop_reason = RC3_WALK_STOP_TARGETS;
                st->stop_off = off;
                {
                    uint32_t k, c = 0;
                    for (k = 0; k < use_nt; ++k)
                        if ((st->targets_mask >> k) & 1u) ++c;
                    st->targets_covered = c;
                }
                st->trustworthy = rc3_vmmap_trustworthy(st);
                return kept;
            }
        }
        off += ksz;
    }

    st->stop_off = off;
    {
        uint32_t k, c = 0;
        for (k = 0; k < use_nt; ++k)
            if ((st->targets_mask >> k) & 1u) ++c;
        st->targets_covered = c;
    }
    st->trustworthy = rc3_vmmap_trustworthy(st);
    return kept;
}

int rc3_vmmap_trustworthy(const rc3_vmstats* st) {
    /* v3 gate formula, cited (worker-ingame-probe-v3/src/vmmap_walk.c). */
    if (st == NULL) return 0;
    return (st->retained >= 8u && st->insane * 4u <= st->retained) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* Per-chunk deep read                                                 */
/* ------------------------------------------------------------------ */

static void chunk_view_reset(rc3_chunk_view* cv) {
    memset(cv, 0, sizeof(*cv));
    cv->st_ptr = RC3_ST_NOT_RUN;
    cv->st_item = RC3_ST_NOT_RUN;
    cv->chk_needed = IGU_CSK_NOT_RUN;
}

/* Scan one chunk's items from slot 0 for the first non-null item. */
static void chunk_scan_items(const igu_span_table* t,
                             const rc3_chunk_view* cv_in,
                             uint32_t scan_max,
                             rc3_chunk_view* cv) {
    uint32_t slot;
    *cv = *cv_in;
    cv->st_item = IGU_R_NULL; /* exhausted-scan default */
    cv->items_scanned = 0;
    for (slot = 0; slot < scan_max; ++slot) {
        uint8_t item[IGU_FUOBJECTITEM_SIZE];
        const uint64_t item_addr = cv->ptr + (uint64_t)slot * IGU_ITEM_STRIDE;
        int rc = igu_read(t, item_addr, item, sizeof(item));
        if (rc != IGU_OK) {
            cv->st_item = rc; /* coverage-class evidence, exact code kept */
            cv->items_scanned = slot;
            return;
        }
        cv->item_object =
            (uint64_t)item[0x00] | ((uint64_t)item[0x01] << 8) |
            ((uint64_t)item[0x02] << 16) | ((uint64_t)item[0x03] << 24) |
            ((uint64_t)item[0x04] << 32) | ((uint64_t)item[0x05] << 40) |
            ((uint64_t)item[0x06] << 48) | ((uint64_t)item[0x07] << 56);
        cv->item_flags =
            (uint32_t)item[0x08] | ((uint32_t)item[0x09] << 8) |
            ((uint32_t)item[0x0A] << 16) | ((uint32_t)item[0x0B] << 24);
        cv->item_cluster =
            (uint32_t)item[0x0C] | ((uint32_t)item[0x0D] << 8) |
            ((uint32_t)item[0x0E] << 16) | ((uint32_t)item[0x0F] << 24);
        cv->item_serial =
            (uint32_t)item[0x10] | ((uint32_t)item[0x11] << 8) |
            ((uint32_t)item[0x12] << 16) | ((uint32_t)item[0x13] << 24);
        cv->items_scanned = slot + 1u;
        if (cv->item_object != 0u) {
            if (!igu_ptr_canonical(cv->item_object)) {
                cv->st_item = IGU_R_NONCANONICAL; /* refute-class */
                cv->item_nonnull = 1; /* non-null but forbidden */
                return;
            }
            cv->st_item = IGU_OK;
            cv->item_nonnull = 1;
            return;
        }
        /* Null item: legal GC/freed slot; raw fields above are from the
         * LAST examined slot only; keep scanning. */
    }
    cv->items_scanned = scan_max;
}

int rc3_probe_chunks(const igu_span_table* t, const rc3_vmstats* vm,
                     uint64_t gua_addr, uint32_t scan_max,
                     rc3_record* rec) {
    uint8_t hdr[IGU_GUA_HEADER_SIZE];
    int rc;
    int refute = 0, coverfail = 0;
    int i;

    if (t == NULL || rec == NULL) {
        return IGU_E_ARG;
    }
    memset(rec, 0, sizeof(*rec));
    rec->obj_source_chunk = -1;
    rec->map_trusted = 0;
    chunk_view_reset(&rec->chunk[0]);
    chunk_view_reset(&rec->chunk[1]);
    if (vm != NULL) {
        rec->vm = *vm;
        rec->map_trusted = rc3_vmmap_trustworthy(vm);
    }
    if (!rec->map_trusted) {
        /* Needs-map: refuse EVERY read (the honest failure mode after a
         * corrupt/incomplete map — never guess through it). */
        rec->st_hdr = RC3_ST_NO_MAP;
        rec->rc = RC3_ST_NO_MAP;
        return rec->rc;
    }
    if (scan_max == 0u || scan_max > IGU_SCAN_MAX_ITEMS) {
        scan_max = IGU_SCAN_MAX_ITEMS;
    }

    /* ---- header (GUA+0x00..0x2F): one 48-byte window, layout A ---- */
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

    /* Non-gating info checks (evidence, never gate — core discipline). */
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

    /* ---- b+c) chunk[0] and chunk[1], each recorded individually ---- */
    for (i = 0; i < 2; ++i) {
        rc3_chunk_view* cv = &rec->chunk[i];
        const uint32_t needed =
            (i < (int)((rec->num_elements + IGU_NUM_ELEM_PER_CHUNK - 1u) /
                       IGU_NUM_ELEM_PER_CHUNK))
                ? 1u : 0u;
        if ((uint32_t)i >= rec->num_chunks) {
            /* Not commissioned under this header: neutral, never degrades. */
            cv->st_ptr = RC3_ST_NOT_RUN;
            cv->st_item = RC3_ST_NOT_RUN;
            continue;
        }
        rc = igu_read_u64(t, rec->objects_ptr + (uint64_t)i * 8u, &cv->ptr);
        cv->st_ptr = rc;
        if (rc != IGU_OK) {
            coverfail = 1;
            cv->chk_needed = IGU_CSK_UNAVAILABLE;
            continue; /* partial evidence preserved; other chunk proceeds */
        }
        if (cv->ptr == 0u) {
            cv->st_ptr = IGU_R_SKIPPED; /* not-yet-allocated chunk */
            cv->chk_needed = needed ? IGU_CSK_CONTRADICTED /* worker-5
                                          GOA_R_CHUNK_NULL_WHILE_NEEDED */
                                    : IGU_CSK_PASS;
            if (needed) refute = 1;
            continue;
        }
        if (!igu_ptr_canonical(cv->ptr)) {
            cv->st_ptr = IGU_R_NONCANONICAL;
            cv->chk_needed = IGU_CSK_NOT_RUN;
            refute = 1;
            continue;
        }
        cv->chk_needed = IGU_CSK_PASS;
        chunk_scan_items(t, cv, scan_max, cv);
        if (cv->st_item == IGU_R_NONCANONICAL) {
            refute = 1;
        } else if (cv->st_item != IGU_OK && cv->st_item != IGU_R_NULL) {
            coverfail = 1; /* coverage failure inside the item window */
        }
    }

    /* ---- d) UObjectBase behind the first non-null item (chunk 0 first) */
    {
        int src = -1;
        uint64_t obj = 0;
        if (rec->chunk[0].item_nonnull && rec->chunk[0].st_item == IGU_OK) {
            src = 0; obj = rec->chunk[0].item_object;
        } else if (rec->chunk[1].item_nonnull &&
                   rec->chunk[1].st_item == IGU_OK) {
            src = 1; obj = rec->chunk[1].item_object;
        }
        if (src < 0) {
            /* No candidate anywhere in the scanned windows. An exhausted
             * scan is evidence-unavailable (core IGU_R_NULL discipline). */
            int saw_coverfail = 0;
            for (i = 0; i < 2; ++i) {
                if (rec->chunk[i].st_item != IGU_OK &&
                    rec->chunk[i].st_item != IGU_R_NULL &&
                    rec->chunk[i].st_item != RC3_ST_NOT_RUN &&
                    rec->chunk[i].st_item != IGU_R_SKIPPED &&
                    rec->chunk[i].st_item != IGU_R_NONCANONICAL) {
                    rec->st_obj = rec->chunk[i].st_item;
                    saw_coverfail = 1;
                    break;
                }
            }
            if (!saw_coverfail) rec->st_obj = IGU_R_NULL;
            rec->rc = rec->st_obj;
            if (rec->st_obj == IGU_R_NONCANONICAL) refute = 1;
            else coverfail = 1;
            goto classify;
        }
        {
            uint8_t objb[IGU_UOBJECTBASE_SIZE];
            rec->obj_source_chunk = src;
            rec->obj_addr = obj;
            rc = igu_read(t, obj, objb, sizeof(objb));
            if (rc != IGU_OK) {
                rec->st_obj = rc;
                coverfail = 1;
                goto classify;
            }
            rec->st_obj = IGU_OK;
            rec->vptr =
                (uint64_t)objb[0x00] | ((uint64_t)objb[0x01] << 8) |
                ((uint64_t)objb[0x02] << 16) | ((uint64_t)objb[0x03] << 24) |
                ((uint64_t)objb[0x04] << 32) | ((uint64_t)objb[0x05] << 40) |
                ((uint64_t)objb[0x06] << 48) | ((uint64_t)objb[0x07] << 56);
            rec->obj_flags =
                (uint32_t)objb[0x08] | ((uint32_t)objb[0x09] << 8) |
                ((uint32_t)objb[0x0A] << 16) | ((uint32_t)objb[0x0B] << 24);
            rec->internal_index =
                (int32_t)((uint32_t)objb[0x0C] | ((uint32_t)objb[0x0D] << 8) |
                          ((uint32_t)objb[0x0E] << 16) |
                          ((uint32_t)objb[0x0F] << 24));
            rec->class_ptr =
                (uint64_t)objb[0x10] | ((uint64_t)objb[0x11] << 8) |
                ((uint64_t)objb[0x12] << 16) | ((uint64_t)objb[0x13] << 24) |
                ((uint64_t)objb[0x14] << 32) | ((uint64_t)objb[0x15] << 40) |
                ((uint64_t)objb[0x16] << 48) | ((uint64_t)objb[0x17] << 56);
            memcpy(rec->name_raw, &objb[0x18], 8u); /* RAW; never judged */
            rec->outer_ptr =
                (uint64_t)objb[0x20] | ((uint64_t)objb[0x21] << 8) |
                ((uint64_t)objb[0x22] << 16) | ((uint64_t)objb[0x23] << 24) |
                ((uint64_t)objb[0x24] << 32) | ((uint64_t)objb[0x25] << 40) |
                ((uint64_t)objb[0x26] << 48) | ((uint64_t)objb[0x27] << 56);
        }
    }

    /* ---- e) checks (batch2 check family, core semantics) ---- */
    /* vtable: non-null, canonical, 8-byte window readable. Coverage
     * failures are UNAVAILABLE (Inconclusive), never Refuted — including
     * the image-prot-4 X-only case documented in live_gua_reads.h. */
    if (rec->vptr == 0u || !igu_ptr_canonical(rec->vptr)) {
        rec->chk_vptr = IGU_CSK_CONTRADICTED;
        refute = 1;
    } else {
        uint32_t prot = 0;
        rc = igu_find_covering(t, rec->vptr, 8u, &prot);
        if (rc < 0 || !(prot & IGU_PROT_R)) {
            rec->chk_vptr = IGU_CSK_UNAVAILABLE;
            coverfail = 1;
        } else {
            rec->chk_vptr = IGU_CSK_PASS;
        }
    }

    /* flags: RF_AllFlags mask. */
    rec->chk_flags = ((rec->obj_flags & ~IGU_RF_ALL_FLAGS) == 0u)
                         ? IGU_CSK_PASS : IGU_CSK_CONTRADICTED;
    if (rec->chk_flags == IGU_CSK_CONTRADICTED) refute = 1;

    /* index bounds. */
    if (rec->internal_index >= 0 &&
        (uint32_t)rec->internal_index < rec->num_elements) {
        rec->chk_index_bounds = IGU_CSK_PASS;
    } else {
        rec->chk_index_bounds = IGU_CSK_CONTRADICTED;
        refute = 1;
    }

    /* round-trip: item[internal_index].Object == obj_addr via the cited
     * chunk math (65536 elements/chunk, stride 0x18). */
    if (rec->chk_index_bounds == IGU_CSK_PASS) {
        const uint32_t idx = (uint32_t)rec->internal_index;
        const uint32_t ch = idx / IGU_NUM_ELEM_PER_CHUNK;
        const uint32_t sl = idx % IGU_NUM_ELEM_PER_CHUNK;
        uint64_t ch_ptr = 0, obj_at_idx = 0;
        if (ch >= rec->num_chunks) {
            rec->chk_roundtrip = IGU_CSK_UNAVAILABLE;
            coverfail = 1;
        } else {
            rc = igu_read_u64(t, rec->objects_ptr + (uint64_t)ch * 8u,
                              &ch_ptr);
            if (rc == IGU_OK && ch_ptr != 0u) {
                rc = igu_read_u64(t, ch_ptr + (uint64_t)sl * IGU_ITEM_STRIDE,
                                  &obj_at_idx);
            }
            if (rc != IGU_OK) {
                rec->chk_roundtrip = IGU_CSK_UNAVAILABLE;
                coverfail = 1;
            } else if (obj_at_idx == rec->obj_addr) {
                rec->chk_roundtrip = IGU_CSK_PASS;
            } else {
                rec->chk_roundtrip = IGU_CSK_CONTRADICTED;
                refute = 1;
            }
        }
    } else {
        rec->chk_roundtrip = IGU_CSK_NOT_RUN;
    }

    /* class: non-null, canonical, readable. */
    if (rec->class_ptr == 0u || !igu_ptr_canonical(rec->class_ptr)) {
        rec->chk_class = IGU_CSK_CONTRADICTED;
        refute = 1;
    } else {
        uint32_t prot = 0;
        rc = igu_find_covering(t, rec->class_ptr, 8u, &prot);
        if (rc < 0 || !(prot & IGU_PROT_R)) {
            rec->chk_class = IGU_CSK_UNAVAILABLE;
            coverfail = 1;
        } else {
            rec->chk_class = IGU_CSK_PASS;
        }
    }

    /* outer: null-or-readable. */
    if (rec->outer_ptr == 0u) {
        rec->chk_outer = IGU_CSK_PASS;
    } else if (!igu_ptr_canonical(rec->outer_ptr)) {
        rec->chk_outer = IGU_CSK_CONTRADICTED;
        refute = 1;
    } else {
        uint32_t prot = 0;
        rc = igu_find_covering(t, rec->outer_ptr, 8u, &prot);
        if (rc < 0 || !(prot & IGU_PROT_R)) {
            rec->chk_outer = IGU_CSK_UNAVAILABLE;
            coverfail = 1;
        } else {
            rec->chk_outer = IGU_CSK_PASS;
        }
    }

classify:
    /* Overall rc: refute > coverage-class > null-exhaustion > OK. Info
     * checks stay non-gating (recorded evidence only). */
    if (refute) {
        rec->rc = IGU_R_CHECK_FAIL;
    } else if (coverfail) {
        /* Keep the FIRST coverage code for the record if one was stored in
         * a step status; otherwise the generic coverage class. */
        if (rec->rc == IGU_OK) {
            int c = IGU_E_NO_SPAN;
            if (rec->st_hdr != IGU_OK && rec->st_hdr != RC3_ST_NO_MAP)
                c = rec->st_hdr;
            else if (rec->st_a != IGU_OK && rec->st_a != IGU_R_NULL &&
                     rec->st_a != IGU_R_NONCANONICAL)
                c = rec->st_a;
            else {
                for (i = 0; i < 2; ++i) {
                    const int sp = rec->chunk[i].st_ptr;
                    const int si = rec->chunk[i].st_item;
                    if (sp != IGU_OK && sp != IGU_R_SKIPPED &&
                        sp != IGU_R_NONCANONICAL && sp != RC3_ST_NOT_RUN) {
                        c = sp; break;
                    }
                    if (si != IGU_OK && si != IGU_R_NULL &&
                        si != IGU_R_NONCANONICAL && si != RC3_ST_NOT_RUN) {
                        c = si; break;
                    }
                }
                if (c == IGU_E_NO_SPAN && rec->st_obj != IGU_OK &&
                    rec->st_obj != IGU_R_NULL &&
                    rec->st_obj != IGU_R_NONCANONICAL) {
                    c = rec->st_obj;
                }
            }
            rec->rc = c;
        }
    } else if (rec->st_obj == IGU_R_NULL) {
        rec->rc = IGU_R_NULL;
    } else {
        rec->rc = IGU_OK;
    }
    return rec->rc;
}

int rc3_class(const rc3_record* rec) {
    int i;
    if (rec == NULL) return IGU_E_ARG;
    if (!rec->map_trusted || rec->st_hdr == RC3_ST_NO_MAP) return 1;

    /* Refute-class: non-canonical pointers or contradicted checks. */
    if (rec->st_a == IGU_R_NONCANONICAL) return 2;
    for (i = 0; i < 2; ++i) {
        if (rec->chunk[i].st_ptr == IGU_R_NONCANONICAL ||
            rec->chunk[i].st_item == IGU_R_NONCANONICAL ||
            rec->chunk[i].chk_needed == IGU_CSK_CONTRADICTED) return 2;
    }
    if (rec->chk_vptr == IGU_CSK_CONTRADICTED ||
        rec->chk_flags == IGU_CSK_CONTRADICTED ||
        rec->chk_index_bounds == IGU_CSK_CONTRADICTED ||
        rec->chk_roundtrip == IGU_CSK_CONTRADICTED ||
        rec->chk_class == IGU_CSK_CONTRADICTED ||
        rec->chk_outer == IGU_CSK_CONTRADICTED) return 2;

    /* Inconclusive-class: any coverage failure or exhausted scan. */
    if (rec->st_hdr != IGU_OK || rec->st_a != IGU_OK) return 1;
    for (i = 0; i < 2; ++i) {
        const int sp = rec->chunk[i].st_ptr;
        const int si = rec->chunk[i].st_item;
        const int cn = rec->chunk[i].chk_needed;
        if (sp == RC3_ST_NOT_RUN) {
            /* chunk i not commissioned under this header: neutral. */
        } else if (sp != IGU_OK && sp != IGU_R_SKIPPED) {
            return 1; /* coverage failure on a commissioned chunk ptr */
        }
        if (si == RC3_ST_NOT_RUN) {
            /* neutral */
        } else if (si != IGU_OK && si != IGU_R_NULL) {
            return 1; /* item-window coverage failure */
        }
        if (cn == IGU_CSK_UNAVAILABLE) return 1;
    }
    if (rec->st_obj != IGU_OK) return 1; /* incl. IGU_R_NULL exhaustion */
    if (rec->chk_vptr == IGU_CSK_UNAVAILABLE ||
        rec->chk_roundtrip == IGU_CSK_UNAVAILABLE ||
        rec->chk_class == IGU_CSK_UNAVAILABLE ||
        rec->chk_outer == IGU_CSK_UNAVAILABLE) return 1;
    if (rec->rc == IGU_OK) return 0;
    return 1;
}
