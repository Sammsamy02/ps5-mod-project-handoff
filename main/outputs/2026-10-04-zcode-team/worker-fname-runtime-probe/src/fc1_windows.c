/* fc1_windows.c — worker-fname-runtime-probe (team 2026-10-04).
 * PURE bounded x86-64 decoder for the FC1-B window contract. See header.
 * C89-safe, no allocations, no syscalls; every failure mode is explicit.
 */
#include "fc1_windows.h"

/* ---- tiny instruction model ---------------------------------------- */

typedef enum {
    FC1_I_OTHER = 0,     /* flow-continuing, no register write of interest */
    FC1_I_BRANCH,        /* call/jmp/jcc/ret (forbidden before terminator) */
    FC1_I_CALL_TERM,     /* E8 rel32 (legal only at the recorded call) */
    FC1_I_LEA,           /* 8d: writes REG from memory address */
    FC1_I_MOV_RM_R,      /* 88/89: writes RM operand (reg or memory) */
    FC1_I_MOV_R_RM,      /* 8a/8b + 0f b7 movzx: writes REG */
    FC1_I_MOV_R_IMM32    /* b8+r imm32: writes REG (no REX, no 66) */
} Fc1InsnKind;

typedef struct Fc1Insn {
    Fc1InsnKind kind;
    uint32_t    len;          /* total instruction length */
    uint32_t    reg;          /* destination register (0..15) when applicable */
    int         rm_is_reg;    /* MOV_RM_R: 1 when RM operand is a register */
    uint32_t    rm_reg;       /* MOV_RM_R: register number when rm_is_reg */
    int         mem_base;     /* -1 none; else base register 0..15 */
    int         mem_index;    /* -1 no index; else index register 0..15 */
    int         rip_rel;      /* 1 when operand is [rip+disp32] */
    int64_t     disp;         /* sign-extended displacement */
    uint64_t    next_addr;    /* address of next instruction */
    uint64_t    rel_target;   /* resolved branch/call target (CALL_TERM) */
} Fc1Insn;

/* Decode ModRM (+SIB, +disp). base_off = offset of ModRM within the insn,
   next_addr = address following the whole instruction. Returns consumed
   ModRM block length or 0 on malformed/truncated. */
static uint32_t fc1_modrm(const uint8_t *b, uint32_t avail, uint8_t rex,
                          uint64_t next_addr, Fc1Insn *in) {
    if (avail < 1) return 0;
    uint8_t modrm = b[0];
    uint8_t mod = (uint8_t)(modrm >> 6);
    uint8_t reg = (uint8_t)(((modrm >> 3) & 7) | (rex & 4 ? 8 : 0));
    uint8_t rm  = (uint8_t)((modrm & 7) | (rex & 1 ? 8 : 0));
    in->reg = reg;
    in->rm_is_reg = mod == 3;
    in->rm_reg = rm;
    in->mem_base = -1;
    in->mem_index = -1;
    in->rip_rel = 0;
    in->disp = 0;
    uint32_t off = 1;
    if (mod == 3) return off;
    if (rm == 4) {                       /* SIB byte follows */
        if (avail < off + 1) return 0;
        uint8_t sib = b[off++];
        uint8_t idx = (uint8_t)((sib >> 3) & 7);
        uint8_t base = (uint8_t)((sib & 7) | (rex & 1 ? 8 : 0));
        in->mem_index = (idx == 4 && !(rex & 2)) ? -1
                        : (int)(idx | (rex & 2 ? 8 : 0));
        if (base == 5 && mod == 0) {
            in->mem_base = -1;           /* disp32, no base */
        } else {
            in->mem_base = (int)base;
        }
    } else if (rm == 5 && mod == 0) {    /* RIP-relative */
        in->rip_rel = 1;
    } else {
        in->mem_base = (int)rm;
    }
    if (mod == 1) {
        if (avail < off + 1) return 0;
        in->disp = (int8_t)b[off];
        off += 1;
    } else if (mod == 2 || (rm == 4 && in->mem_base == -1 && mod == 0) ||
               (rm == 5 && mod == 0)) {
        if (avail < off + 4) return 0;
        in->disp = (int32_t)((uint32_t)b[off] |
                             ((uint32_t)b[off + 1] << 8) |
                             ((uint32_t)b[off + 2] << 16) |
                             ((uint32_t)b[off + 3] << 24));
        off += 4;
    }
    (void)next_addr;
    return off;
}

/* Decode one instruction from [b, b+avail) at runtime address addr.
   Returns 0 ok (fills *in), negative on truncation/malformed. Any opcode
   outside the allowed subset => FC1_E_UNSUPPORTED (fail closed). */
static int fc1_decode_one(const uint8_t *b, uint32_t avail, uint64_t addr,
                          Fc1Insn *in) {
    in->kind = FC1_I_OTHER;
    in->len = 0;
    in->reg = 0;
    in->rm_is_reg = 0;
    in->rm_reg = 0;
    in->mem_base = -1;
    in->mem_index = -1;
    in->rip_rel = 0;
    in->disp = 0;
    in->next_addr = addr;
    in->rel_target = 0;

    uint32_t i = 0;
    uint8_t rex = 0;
    int opsz16 = 0;
    for (;;) {                            /* legacy prefixes (bounded) */
        if (i >= avail) return FC1_E_UNSUPPORTED;
        uint8_t p = b[i];
        if (p == 0x66) { opsz16 = 1; ++i; continue; }
        if (p == 0x67 || p == 0xf2 || p == 0xf3 || p == 0x2e || p == 0x3e ||
            p == 0x26 || p == 0x36 || p == 0x64 || p == 0x65) { ++i; continue; }
        if (p >= 0x40 && p <= 0x4f) { rex = p; ++i; }
        break;
    }
    if (i >= avail) return FC1_E_UNSUPPORTED;
    uint8_t op = b[i++];
    uint8_t rex_w = (uint8_t)(rex & 8);

    if (op == 0x0f) {                     /* two-byte opcodes */
        if (i >= avail) return FC1_E_UNSUPPORTED;
        uint8_t op2 = b[i++];
        if (op2 >= 0x80 && op2 <= 0x8f) { /* JCC rel32 */
            if (opsz16 || avail - i < 4) return FC1_E_UNSUPPORTED;
            int32_t rel = (int32_t)((uint32_t)b[i] |
                                    ((uint32_t)b[i + 1] << 8) |
                                    ((uint32_t)b[i + 2] << 16) |
                                    ((uint32_t)b[i + 3] << 24));
            in->kind = FC1_I_BRANCH;
            in->rel_target = addr + i + 4 + (int64_t)rel;
            in->len = i + 4;
            return 0;
        }
        if (op2 == 0xb7) {                /* MOVZX r32, rm16 */
            if (opsz16) return FC1_E_UNSUPPORTED;
            uint32_t n = fc1_modrm(b + i, avail - i, rex, 0, in);
            if (n == 0) return FC1_E_UNSUPPORTED;
            in->kind = FC1_I_MOV_R_RM;
            in->len = i + n;
            return 0;
        }
        if (op2 == 0x1f) {                /* multi-byte NOP */
            uint32_t n = fc1_modrm(b + i, avail - i, rex, 0, in);
            if (n == 0) return FC1_E_UNSUPPORTED;
            in->len = i + n;
            return 0;
        }
        return FC1_E_UNSUPPORTED;
    }

    switch (op) {
    case 0x88: case 0x89: {               /* MOV rm, r */
        if (rex_w && op == 0x88) return FC1_E_UNSUPPORTED;
        uint32_t n = fc1_modrm(b + i, avail - i, rex, 0, in);
        if (n == 0) return FC1_E_UNSUPPORTED;
        in->kind = FC1_I_MOV_RM_R;        /* writes RM operand */
        in->len = i + n;
        return 0;
    }
    case 0x8a: case 0x8b: {               /* MOV r, rm */
        if (rex_w && op == 0x8a) return FC1_E_UNSUPPORTED;
        uint32_t n = fc1_modrm(b + i, avail - i, rex, 0, in);
        if (n == 0) return FC1_E_UNSUPPORTED;
        in->kind = FC1_I_MOV_R_RM;
        in->len = i + n;
        return 0;
    }
    case 0x8d: {                          /* LEA r, m */
        if (opsz16) return FC1_E_UNSUPPORTED;
        uint32_t n = fc1_modrm(b + i, avail - i, rex, 0, in);
        if (n == 0) return FC1_E_UNSUPPORTED;
        in->kind = FC1_I_LEA;
        in->len = i + n;
        in->next_addr = addr + in->len;
        if (in->rip_rel)
            in->rel_target = in->next_addr + (uint64_t)in->disp;
        return 0;
    }
    case 0xb8: case 0xb9: case 0xba: case 0xbb:
    case 0xbc: case 0xbd: case 0xbe: case 0xbf: { /* MOV r32, imm32 */
        if (rex_w || opsz16 || (rex & 7)) return FC1_E_UNSUPPORTED;
        if (avail - i < 4) return FC1_E_UNSUPPORTED;
        in->kind = FC1_I_MOV_R_IMM32;
        in->reg = (uint32_t)((op - 0xb8) | (rex & 1 ? 8 : 0));
        in->disp = (int32_t)((uint32_t)b[i] | ((uint32_t)b[i + 1] << 8) |
                             ((uint32_t)b[i + 2] << 16) |
                             ((uint32_t)b[i + 3] << 24));
        in->len = i + 4;
        return 0;
    }
    case 0x90:                            /* NOP */
        in->len = i;
        return 0;
    case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75:
    case 0x76: case 0x77: case 0x78: case 0x79: case 0x7a: case 0x7b:
    case 0x7c: case 0x7d: case 0x7e: case 0x7f: { /* JCC rel8 */
        if (i >= avail) return FC1_E_UNSUPPORTED;
        in->kind = FC1_I_BRANCH;
        in->rel_target = addr + i + 1 + (int64_t)(int8_t)b[i];
        in->len = i + 1;
        return 0;
    }
    case 0xeb: {                          /* JMP rel8 */
        if (i >= avail) return FC1_E_UNSUPPORTED;
        in->kind = FC1_I_BRANCH;
        in->rel_target = addr + i + 1 + (int64_t)(int8_t)b[i];
        in->len = i + 1;
        return 0;
    }
    case 0xe9: {                          /* JMP rel32 */
        if (avail - i < 4) return FC1_E_UNSUPPORTED;
        int32_t rel = (int32_t)((uint32_t)b[i] | ((uint32_t)b[i + 1] << 8) |
                                ((uint32_t)b[i + 2] << 16) |
                                ((uint32_t)b[i + 3] << 24));
        in->kind = FC1_I_BRANCH;
        in->rel_target = addr + i + 4 + (int64_t)rel;
        in->len = i + 4;
        return 0;
    }
    case 0xe8: {                          /* CALL rel32 — the terminator */
        if (avail - i < 4) return FC1_E_UNSUPPORTED;
        int32_t rel = (int32_t)((uint32_t)b[i] | ((uint32_t)b[i + 1] << 8) |
                                ((uint32_t)b[i + 2] << 16) |
                                ((uint32_t)b[i + 3] << 24));
        in->kind = FC1_I_CALL_TERM;
        in->rel_target = addr + i + 4 + (int64_t)rel;
        in->len = i + 4;
        return 0;
    }
    case 0xc3:                            /* RET */
        in->kind = FC1_I_BRANCH;
        in->len = i;
        return 0;
    default:
        return FC1_E_UNSUPPORTED;
    }
}

/* Does this instruction write the given 64-bit-capable register? Only
   register destinations count (memory stores preserve the register). */
static int fc1_writes_reg(const Fc1Insn *in, uint32_t r) {
    switch (in->kind) {
    case FC1_I_LEA:
    case FC1_I_MOV_R_RM:
    case FC1_I_MOV_R_IMM32:
        return in->reg == r;
    case FC1_I_MOV_RM_R:
        return in->rm_is_reg && in->rm_reg == r;
    default:
        return 0;
    }
}

/* ---- public API ----------------------------------------------------- */

int igfc1_check_call_bytes(const uint8_t *b, uint64_t call_rt,
                           uint64_t expect_target_rt,
                           uint64_t *rel32_target_out) {
    uint64_t tgt = 0;
    if (b[0] == 0xe8) {
        int32_t rel = (int32_t)((uint32_t)b[1] | ((uint32_t)b[2] << 8) |
                                ((uint32_t)b[3] << 16) |
                                ((uint32_t)b[4] << 24));
        tgt = call_rt + 5 + (uint64_t)(int64_t)rel;
    }
    if (rel32_target_out) *rel32_target_out = tgt;
    return (b[0] == 0xe8 && tgt == expect_target_rt) ? 0 : -1;
}

int igfc1_decode_window(const uint8_t *buf, uint32_t buflen,
                        uint64_t window_rt,
                        uint64_t expect_call_rt,
                        uint64_t expect_target_rt,
                        uint64_t expect_anchor_rt,
                        Fc1WindowOut *out) {
    out->status = FC1_E_UNSUPPORTED;
    out->decoded_len = 0;
    out->lea_rsi_target = 0;
    out->rdi_writes = 0;
    out->rdx_writes = 0;
    out->rdi_disp = 0;
    out->call_target_rt = 0;
    out->call_byte = buf[0];

    uint32_t call_off = (uint32_t)(expect_call_rt - window_rt);
    if (call_off < 5 || call_off > FC1_WINDOW_MAX ||
        buflen != call_off + 5) {
        out->status = FC1_E_BUFLEN;
        return out->status;
    }
    out->call_byte = buf[call_off];
    if (out->call_byte != 0xe8) {         /* T1: byte[site] must be E8 */
        out->status = FC1_E_NO_CALL_TERM;
        return out->status;
    }

    uint32_t off = 0;
    int rdi_bad = 0, rdx_bad = 0;
    while (off <= call_off) {
        if (off == call_off) {            /* terminator position */
            uint64_t tgt = 0;
            if (igfc1_check_call_bytes(buf + off, window_rt + off,
                                       expect_target_rt, &tgt) != 0) {
                out->call_target_rt = tgt;
                out->status = FC1_E_CALL_TARGET;
                out->decoded_len = off + 5;
                return out->status;
            }
            out->call_target_rt = tgt;
            out->decoded_len = off + 5;
            if (rdi_bad || out->rdi_writes != 1) {
                out->status = FC1_E_RDI_SHAPE;
                return out->status;
            }
            if (rdx_bad || out->rdx_writes != 1) {
                out->status = FC1_E_RDX_SHAPE;
                return out->status;
            }
            out->status = FC1_WOK;
            return out->status;
        }
        Fc1Insn in;
        int rc = fc1_decode_one(buf + off, call_off - off, window_rt + off,
                                &in);
        if (rc != 0) {                    /* includes truncation */
            out->status = rc == FC1_E_UNSUPPORTED ? FC1_E_UNSUPPORTED
                                                  : FC1_E_NO_CALL_TERM;
            out->decoded_len = off;
            return out->status;
        }
        if (in.kind == FC1_I_BRANCH || in.kind == FC1_I_CALL_TERM) {
            out->status = FC1_E_BRANCH;   /* branch before the terminator */
            out->decoded_len = off;
            return out->status;
        }
        if (off == 0) {                   /* rule 1: first insn LEA RSI */
            /* exact documented encoding: 48 8D 35 <disp32> */
            if (buf[0] != 0x48 || buf[1] != 0x8d || buf[2] != 0x35 ||
                in.kind != FC1_I_LEA || !in.rip_rel ||
                in.reg != FC1_REG_RSI) {
                out->status = FC1_E_FIRST_NOT_LEA;
                out->decoded_len = in.len;
                return out->status;
            }
            out->lea_rsi_target = in.rel_target;
            if (in.rel_target != expect_anchor_rt) {
                out->status = FC1_E_ANCHOR_TARGET;
                out->decoded_len = in.len;
                return out->status;
            }
        } else if (fc1_writes_reg(&in, FC1_REG_RSI)) {
            out->status = FC1_E_RSI_REWRITTEN;  /* rule 4 */
            out->decoded_len = off;
            return out->status;
        }
        if (fc1_writes_reg(&in, FC1_REG_RDI)) {
            ++out->rdi_writes;
            /* rule 2: LEA RDI,[RBP|RSP + disp], no index */
            int ok = in.kind == FC1_I_LEA &&
                     (in.mem_base == FC1_REG_RBP ||
                      in.mem_base == FC1_REG_RSP) &&
                     in.mem_index == -1 && !in.rip_rel &&
                     (in.rm_is_reg == 0);
            if (ok) out->rdi_disp = in.disp;
            else rdi_bad = 1;
        }
        if (fc1_writes_reg(&in, FC1_REG_RDX)) {
            ++out->rdx_writes;
            /* rule 3: exactly BA 01 00 00 00 */
            if (!(in.kind == FC1_I_MOV_R_IMM32 && in.reg == FC1_REG_RDX &&
                  in.disp == 1))
                rdx_bad = 1;
        }
        off += in.len;
    }
    /* Stepped past the call without landing on it. */
    out->status = FC1_E_NO_CALL_TERM;
    out->decoded_len = off;
    return out->status;
}

const char *igfc1_status_string(int status) {
    switch (status) {
    case FC1_WOK:             return "ok";
    case FC1_E_BUFLEN:        return "buflen_mismatch";
    case FC1_E_FIRST_NOT_LEA: return "first_insn_not_lea_rsi";
    case FC1_E_ANCHOR_TARGET: return "lea_rsi_target_not_anchor_string";
    case FC1_E_RDI_SHAPE:     return "rdi_store_missing_or_malformed";
    case FC1_E_RDX_SHAPE:     return "rdx_not_mov_edx_1";
    case FC1_E_RSI_REWRITTEN: return "rsi_rewritten_in_window";
    case FC1_E_BRANCH:        return "branch_in_window";
    case FC1_E_NO_CALL_TERM:  return "no_call_terminator_at_recorded_site";
    case FC1_E_CALL_TARGET:   return "call_target_not_candidate";
    case FC1_E_UNSUPPORTED:   return "unsupported_encoding_fail_closed";
    default:                  return "unknown";
    }
}
