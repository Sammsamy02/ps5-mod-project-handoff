0x2fddb00: 55                       push rbp
0x2fddb01: 4889e5                   mov rbp, rsp
0x2fddb04: 4157                     push r15
0x2fddb06: 4156                     push r14
0x2fddb08: 4155                     push r13
0x2fddb0a: 4154                     push r12
0x2fddb0c: 53                       push rbx
0x2fddb0d: 4881ece8000000           sub rsp, 0xe8
0x2fddb14: 488b05ed431506           mov rax, qword ptr [rip + 0x61543ed]
0x2fddb1b: 89b5fcfeffff             mov dword ptr [rbp - 0x104], esi
0x2fddb21: 4989fc                   mov r12, rdi
0x2fddb24: 488dbd60ffffff           lea rdi, [rbp - 0xa0]
0x2fddb2b: 4d89c5                   mov r13, r8
0x2fddb2e: 4989ce                   mov r14, rcx
0x2fddb31: 4889d3                   mov rbx, rdx
0x2fddb34: 488b00                   mov rax, qword ptr [rax]
0x2fddb37: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fddb3b: e8b0b67a00               call 0x37891f0
0x2fddb40: 4c89e7                   mov rdi, r12
0x2fddb43: e888120bff               call 0x208edd0
0x2fddb48: 48898578ffffff           mov qword ptr [rbp - 0x88], rax
0x2fddb4f: 804d9440                 or byte ptr [rbp - 0x6c], 0x40
0x2fddb53: 804d9204                 or byte ptr [rbp - 0x6e], 4
0x2fddb57: 4c89e7                   mov rdi, r12
0x2fddb5a: e841c7b3ff               call 0x2b1a2a0
0x2fddb5f: 4d8b7d00                 mov r15, qword ptr [r13]
0x2fddb63: 4989c4                   mov r12, rax
0x2fddb66: 4d85ff                   test r15, r15
0x2fddb69: 7426                     je 0x2fddb91
0x2fddb6b: e8509a4400               call 0x34275c0
0x2fddb70: 4885c0                   test rax, rax
0x2fddb73: 741c                     je 0x2fddb91
0x2fddb75: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fddb79: 413b4f38                 cmp ecx, dword ptr [r15 + 0x38]
0x2fddb7d: 7f12                     jg 0x2fddb91
0x2fddb7f: 498b5730                 mov rdx, qword ptr [r15 + 0x30]
0x2fddb83: 4883c030                 add rax, 0x30
0x2fddb87: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fddb8b: 0f84cb000000             je 0x2fddc5c
0x2fddb91: 31f6                     xor esi, esi
0x2fddb93: 4c8d8560ffffff           lea r8, [rbp - 0xa0]
0x2fddb9a: 4c89e7                   mov rdi, r12
0x2fddb9d: 4889da                   mov rdx, rbx
0x2fddba0: 4c89f1                   mov rcx, r14
0x2fddba3: e8d81a1a00               call 0x317f680
0x2fddba8: 4989c4                   mov r12, rax
0x2fddbab: 4885c0                   test rax, rax
0x2fddbae: 746b                     je 0x2fddc1b
0x2fddbb0: 83bdfcfeffff01           cmp dword ptr [rbp - 0x104], 1
0x2fddbb7: 7509                     jne 0x2fddbc2
0x2fddbb9: 41c68424c406000001       mov byte ptr [r12 + 0x6c4], 1
0x2fddbc2: 4c8dbd00ffffff           lea r15, [rbp - 0x100]
0x2fddbc9: 4c89f6                   mov rsi, r14
0x2fddbcc: 4c89ff                   mov rdi, r15
0x2fddbcf: e88c4e17fd               call 0x152a60
0x2fddbd4: c5f81003                 vmovups xmm0, xmmword ptr [rbx]
0x2fddbd8: c4e27d195310             vbroadcastsd ymm2, qword ptr [rbx + 0x10]
0x2fddbde: c5f057c9                 vxorps xmm1, xmm1, xmm1
0x2fddbe2: 4c89e7                   mov rdi, r12
0x2fddbe5: 4c89fe                   mov rsi, r15
0x2fddbe8: 41b801000000             mov r8d, 1
0x2fddbee: 31d2                     xor edx, edx
0x2fddbf0: 31c9                     xor ecx, ecx
0x2fddbf2: c4e37d0cc230             vblendps ymm0, ymm0, ymm2, 0x30
0x2fddbf8: c4e37d0cc1c0             vblendps ymm0, ymm0, ymm1, 0xc0
0x2fddbfe: c5fc100d5a57a704         vmovups ymm1, ymmword ptr [rip + 0x4a7575a]
0x2fddc06: c5fc118520ffffff         vmovups ymmword ptr [rbp - 0xe0], ymm0
0x2fddc0e: c5fc118d40ffffff         vmovups ymmword ptr [rbp - 0xc0], ymm1
0x2fddc16: e8f5a2b4ff               call 0x2b27f10
0x2fddc1b: 48837da000               cmp qword ptr [rbp - 0x60], 0
0x2fddc20: 7415                     je 0x2fddc37
0x2fddc22: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x2fddc26: 488d7db0                 lea rdi, [rbp - 0x50]
0x2fddc2a: 4885c0                   test rax, rax
0x2fddc2d: 480f45f8                 cmovne rdi, rax
0x2fddc31: 488b07                   mov rax, qword ptr [rdi]
0x2fddc34: ff5010                   call qword ptr [rax + 0x10]
0x2fddc37: 488b05ca421506           mov rax, qword ptr [rip + 0x61542ca]
0x2fddc3e: 488b00                   mov rax, qword ptr [rax]
0x2fddc41: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fddc45: 751e                     jne 0x2fddc65
0x2fddc47: 4c89e0                   mov rax, r12
0x2fddc4a: 4881c4e8000000           add rsp, 0xe8
0x2fddc51: 5b                       pop rbx
0x2fddc52: 415c                     pop r12
0x2fddc54: 415d                     pop r13
0x2fddc56: 415e                     pop r14
0x2fddc58: 415f                     pop r15
0x2fddc5a: 5d                       pop rbp
0x2fddc5b: c3                       ret 
0x2fddc5c: 498b7500                 mov rsi, qword ptr [r13]
0x2fddc60: e92effffff               jmp 0x2fddb93
0x2fddc65: e8f6414904               call 0x7471e60
0x2fddc6a: 0f0b                     ud2 
0x2fddc6c: cc                       int3 
0x2fddc6d: cc                       int3 
0x2fddc6e: cc                       int3 
0x2fddc6f: cc                       int3 
0x2fddc70: 55                       push rbp
0x2fddc71: 4889e5                   mov rbp, rsp
0x2fddc74: 53                       push rbx
0x2fddc75: 50                       push rax
0x2fddc76: 4889fb                   mov rbx, rdi
0x2fddc79: 4885ff                   test rdi, rdi
0x2fddc7c: 7410                     je 0x2fddc8e
