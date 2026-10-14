0x2fdfc70: 55                       push rbp
0x2fdfc71: 4889e5                   mov rbp, rsp
0x2fdfc74: 4157                     push r15
0x2fdfc76: 4156                     push r14
0x2fdfc78: 4155                     push r13
0x2fdfc7a: 4154                     push r12
0x2fdfc7c: 53                       push rbx
0x2fdfc7d: 4881ec88000000           sub rsp, 0x88
0x2fdfc84: 4c8b2d7d221506           mov r13, qword ptr [rip + 0x615227d]
0x2fdfc8b: 4989fe                   mov r14, rdi
0x2fdfc8e: 488dbd60ffffff           lea rdi, [rbp - 0xa0]
0x2fdfc95: 4889d3                   mov rbx, rdx
0x2fdfc98: 4989f7                   mov r15, rsi
0x2fdfc9b: 498b4500                 mov rax, qword ptr [r13]
0x2fdfc9f: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdfca3: e848957a00               call 0x37891f0
0x2fdfca8: 4c89f7                   mov rdi, r14
0x2fdfcab: e820f10aff               call 0x208edd0
0x2fdfcb0: 48898578ffffff           mov qword ptr [rbp - 0x88], rax
0x2fdfcb7: 804d9440                 or byte ptr [rbp - 0x6c], 0x40
0x2fdfcbb: 488b3526245406           mov rsi, qword ptr [rip + 0x6542426]
0x2fdfcc2: 4c89f7                   mov rdi, r14
0x2fdfcc5: e8769a47fd               call 0x459740
0x2fdfcca: 4889c7                   mov rdi, rax
0x2fdfccd: 4989c4                   mov r12, rax
0x2fdfcd0: e82ba533fd               call 0x31a200
0x2fdfcd5: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdfcdc: 7531                     jne 0x2fdfd0f
0x2fdfcde: 48c78558ffffff00000000   mov qword ptr [rbp - 0xa8], 0
0x2fdfce9: 4c89bd50ffffff           mov qword ptr [rbp - 0xb0], r15
0x2fdfcf0: 488d9550ffffff           lea rdx, [rbp - 0xb0]
0x2fdfcf7: 4c89f7                   mov rdi, r14
0x2fdfcfa: 4c89e6                   mov rsi, r12
0x2fdfcfd: 498b06                   mov rax, qword ptr [r14]
0x2fdfd00: ff9068020000             call qword ptr [rax + 0x268]
0x2fdfd06: 4c8bbd58ffffff           mov r15, qword ptr [rbp - 0xa8]
0x2fdfd0d: eb12                     jmp 0x2fdfd21
0x2fdfd0f: 498b06                   mov rax, qword ptr [r14]
0x2fdfd12: 4c89f7                   mov rdi, r14
0x2fdfd15: 4c89fe                   mov rsi, r15
0x2fdfd18: ff90a8070000             call qword ptr [rax + 0x7a8]
0x2fdfd1e: 4989c7                   mov r15, rax
0x2fdfd21: 4c89f7                   mov rdi, r14
0x2fdfd24: e877a5b3ff               call 0x2b1a2a0
0x2fdfd29: 488d8d60ffffff           lea rcx, [rbp - 0xa0]
0x2fdfd30: 4889c7                   mov rdi, rax
0x2fdfd33: 4c89fe                   mov rsi, r15
0x2fdfd36: 4889da                   mov rdx, rbx
0x2fdfd39: e8e2ed1900               call 0x317eb20
0x2fdfd3e: 48837da000               cmp qword ptr [rbp - 0x60], 0
0x2fdfd43: 741b                     je 0x2fdfd60
0x2fdfd45: 488b4dc8                 mov rcx, qword ptr [rbp - 0x38]
0x2fdfd49: 488d7db0                 lea rdi, [rbp - 0x50]
0x2fdfd4d: 4889c3                   mov rbx, rax
0x2fdfd50: 4885c9                   test rcx, rcx
0x2fdfd53: 480f45f9                 cmovne rdi, rcx
0x2fdfd57: 488b0f                   mov rcx, qword ptr [rdi]
0x2fdfd5a: ff5110                   call qword ptr [rcx + 0x10]
0x2fdfd5d: 4889d8                   mov rax, rbx
0x2fdfd60: 498b4d00                 mov rcx, qword ptr [r13]
0x2fdfd64: 483b4dd0                 cmp rcx, qword ptr [rbp - 0x30]
0x2fdfd68: 7512                     jne 0x2fdfd7c
0x2fdfd6a: 4881c488000000           add rsp, 0x88
0x2fdfd71: 5b                       pop rbx
0x2fdfd72: 415c                     pop r12
0x2fdfd74: 415d                     pop r13
0x2fdfd76: 415e                     pop r14
0x2fdfd78: 415f                     pop r15
0x2fdfd7a: 5d                       pop rbp
0x2fdfd7b: c3                       ret 
0x2fdfd7c: e8df204904               call 0x7471e60
0x2fdfd81: 0f0b                     ud2 
0x2fdfd83: cc                       int3 
0x2fdfd84: cc                       int3 
0x2fdfd85: cc                       int3 
0x2fdfd86: cc                       int3 
0x2fdfd87: cc                       int3 
0x2fdfd88: cc                       int3 
0x2fdfd89: cc                       int3 
0x2fdfd8a: cc                       int3 
0x2fdfd8b: cc                       int3 
0x2fdfd8c: cc                       int3 
0x2fdfd8d: cc                       int3 
0x2fdfd8e: cc                       int3 
0x2fdfd8f: cc                       int3 
