/* fc1_windows.h — worker-fname-runtime-probe (team 2026-10-04).
 *
 * PURE bounded x86-64 window decoder for FC1-B (host-testable: no syscalls,
 * no imports, no foreign dereferences). Implements exactly the FC1-CHECKS.md
 * section 3 / FC-1-SPEC-UPDATE.md section 3 T3 contract:
 *
 *   1. first instruction: 48 8D 35 <disp32> (lea rsi,[rip+disp32]) whose
 *      target == the site's anchor-string runtime address;
 *   2. exactly one write to RDI inside the window, of the form
 *      lea rdi,[rbp|rsp + disp] with NO index (generic mask accepts
 *      48 8D 7D d8 / 48 8D 7C 24 d8 / disp32 forms);
 *   3. exactly one write to RDX, of the form BA 01 00 00 00 (mov edx,1);
 *   4. no call/jmp/jcc/ret and no write to RSI between window start and
 *      the call (branch-free path, argument evidence preserved);
 *   5. terminator E8 <rel32> at the recorded call address with
 *      call + 5 + (int32)rel32 == the candidate runtime address.
 *
 * FAIL-CLOSED: any instruction outside the documented allowed subset is
 * status FC1_E_UNSUPPORTED (never guessed through). Mirrors FC1-C rule:
 * the runtime probe must fail closed if any recorded site disagrees.
 */
#ifndef IGFC1_WINDOWS_H
#define IGFC1_WINDOWS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FC1_WINDOW_MAX   64   /* branch-free bytes ending at the call */
#define FC1_NANCHORS      5
#define FC1_NSITES        9

/* GPR numbering used by the decoder (x86-64). */
#define FC1_REG_RAX 0
#define FC1_REG_RDX 2
#define FC1_REG_RSP 4
#define FC1_REG_RBP 5
#define FC1_REG_RSI 6
#define FC1_REG_RDI 7

/* Decode status. 0 == pass; negative == failed (reason recorded). */
enum {
    FC1_WOK             = 0,
    FC1_E_BUFLEN        = -1,  /* buflen != (call-window)+5 or cap exceeded */
    FC1_E_FIRST_NOT_LEA = -2,  /* insn0 not 48 8d 35 disp32 */
    FC1_E_ANCHOR_TARGET = -3,  /* lea rsi target != anchor string rt */
    FC1_E_RDI_SHAPE     = -4,  /* RDI written 0 or >1 times, or wrong form */
    FC1_E_RDX_SHAPE     = -5,  /* RDX written 0 or >1 times, or not mov edx,1 */
    FC1_E_RSI_REWRITTEN = -6,  /* RSI register rewritten inside the window */
    FC1_E_BRANCH        = -7,  /* call/jmp/jcc/ret before the terminator */
    FC1_E_NO_CALL_TERM  = -8,  /* walk did not land exactly on the recorded E8 */
    FC1_E_CALL_TARGET   = -9,  /* rel32 target != expected candidate runtime */
    FC1_E_UNSUPPORTED   = -10  /* encoding outside the allowed subset */
};

/* One decoded window (all facts recorded even on failure, best effort). */
typedef struct Fc1WindowOut {
    int      status;          /* FC1_* enum */
    uint32_t decoded_len;     /* bytes consumed from window start to call+5 */
    uint64_t lea_rsi_target;  /* 0 if first insn did not decode as LEA RSI */
    uint32_t rdi_writes;      /* count of RDI-register writes seen */
    uint32_t rdx_writes;      /* count of RDX-register writes seen */
    int64_t  rdi_disp;        /* disp of the qualifying LEA RDI store */
    uint64_t call_target_rt;  /* resolved E8 rel32 target (0 if none) */
    uint8_t  call_byte;       /* byte at the recorded call site */
} Fc1WindowOut;

/* Decode [buf, buf+buflen): the recorded window bytes starting at window_rt
   and ending at expect_call_rt+5 inclusive. buflen must equal
   (expect_call_rt - window_rt) + 5 and be <= FC1_WINDOW_MAX + 5.
   expect_target_rt = candidate runtime (0x63d590) for rule 5.
   Always writes *out. Returns out->status. */
int igfc1_decode_window(const uint8_t *buf, uint32_t buflen,
                        uint64_t window_rt,
                        uint64_t expect_call_rt,
                        uint64_t expect_target_rt,
                        uint64_t expect_anchor_rt,
                        Fc1WindowOut *out);

/* Standalone T1/T2 check (usable even when the window decode failed):
   b[0]==0xE8 and call_rt + 5 + (int32)rel32 == expect_target_rt.
   Returns 0 pass, -1 fail. rel32_target_out (nullable) receives the
   resolved absolute target either way. */
int igfc1_check_call_bytes(const uint8_t *b, uint64_t call_rt,
                           uint64_t expect_target_rt,
                           uint64_t *rel32_target_out);

const char *igfc1_status_string(int status);

#ifdef __cplusplus
}
#endif

#endif /* IGFC1_WINDOWS_H */
