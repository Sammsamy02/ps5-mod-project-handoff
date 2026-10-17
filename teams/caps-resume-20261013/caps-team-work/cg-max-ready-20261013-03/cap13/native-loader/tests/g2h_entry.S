// Mac x86_64 test-only Win64 entry shims for the real shared g2h thunks.
.text
.globl _guest_bridge5
_guest_bridge5:
    leaq _target5(%rip), %r11
    jmp _cap13_g2h_thunk4s1
.globl _guest_bridge6
_guest_bridge6:
    leaq _target6(%rip), %r11
    jmp _cap13_g2h_thunk4s2
.globl _guest_bridge7
_guest_bridge7:
    leaq _target7(%rip), %r11
    jmp _cap13_g2h_thunk4s3
.globl _guest_bridge8
_guest_bridge8:
    leaq _target8(%rip), %r11
    jmp _cap13_g2h_thunk4s4
