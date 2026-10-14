/* SPDX-License-Identifier: GPL-3.0-or-later
 * Derived from ps5-payload-dev/sdk crt/_start.S (GPL-3.0-or-later).
 * Modified: no CRT startup or saved global stack; callable own-process entry.
 */
.intel_syntax noprefix
.text
.global _start
.type _start, @function
_start:
    push rbp
    mov rbp, rsp
    and rsp, -16
    call probe_entry
    mov rsp, rbp
    pop rbp
    ret
.size _start, . - _start
.section .note.GNU-stack,"",@progbits
