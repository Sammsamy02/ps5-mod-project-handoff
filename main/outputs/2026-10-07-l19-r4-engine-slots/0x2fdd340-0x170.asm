0x2fdd340: 55                       push rbp
0x2fdd341: 4889e5                   mov rbp, rsp
0x2fdd344: 4157                     push r15
0x2fdd346: 4156                     push r14
0x2fdd348: 4155                     push r13
0x2fdd34a: 4154                     push r12
0x2fdd34c: 53                       push rbx
0x2fdd34d: 4883ec28                 sub rsp, 0x28
0x2fdd351: 4c8b25b04b1506           mov r12, qword ptr [rip + 0x6154bb0]
0x2fdd358: 4889fb                   mov rbx, rdi
0x2fdd35b: 498b0424                 mov rax, qword ptr [r12]
0x2fdd35f: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdd363: 488bbf00030000           mov rdi, qword ptr [rdi + 0x300]
0x2fdd36a: 4885ff                   test rdi, rdi
0x2fdd36d: 7409                     je 0x2fdd378
0x2fdd36f: 488b07                   mov rax, qword ptr [rdi]
0x2fdd372: ff90d8070000             call qword ptr [rax + 0x7d8]
0x2fdd378: 4889df                   mov rdi, rbx
0x2fdd37b: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdd37f: c5f81145c0               vmovups xmmword ptr [rbp - 0x40], xmm0
0x2fdd384: e817cfb3ff               call 0x2b1a2a0
0x2fdd389: 4889c7                   mov rdi, rax
0x2fdd38c: e80fb27b00               call 0x37985a0
0x2fdd391: 85d2                     test edx, edx
0x2fdd393: 0f88ef000000             js 0x2fdd488
0x2fdd399: 4989c6                   mov r14, rax
0x2fdd39c: 4189d7                   mov r15d, edx
0x2fdd39f: 31c9                     xor ecx, ecx
0x2fdd3a1: 395008                   cmp dword ptr [rax + 8], edx
0x2fdd3a4: 7e51                     jle 0x2fdd3f7
0x2fdd3a6: 4589fd                   mov r13d, r15d
0x2fdd3a9: 49c1e503                 shl r13, 3
0x2fdd3ad: eb1e                     jmp 0x2fdd3cd
0x2fdd3af: 90                       nop 
0x2fdd3b0: 8d4801                   lea ecx, [rax + 1]
0x2fdd3b3: 4898                     cdqe 
0x2fdd3b5: 41ffc7                   inc r15d
0x2fdd3b8: 4983c508                 add r13, 8
0x2fdd3bc: 894dc8                   mov dword ptr [rbp - 0x38], ecx
0x2fdd3bf: 488b55c0                 mov rdx, qword ptr [rbp - 0x40]
0x2fdd3c3: 4c8924c2                 mov qword ptr [rdx + rax*8], r12
0x2fdd3c7: 45397e08                 cmp dword ptr [r14 + 8], r15d
0x2fdd3cb: 7e2a                     jle 0x2fdd3f7
0x2fdd3cd: 498b3e                   mov rdi, qword ptr [r14]
0x2fdd3d0: 4c01ef                   add rdi, r13
0x2fdd3d3: e828994dfd               call 0x4b6d00
0x2fdd3d8: 4989c4                   mov r12, rax
0x2fdd3db: 8b45c8                   mov eax, dword ptr [rbp - 0x38]
0x2fdd3de: 3b45cc                   cmp eax, dword ptr [rbp - 0x34]
0x2fdd3e1: 75cd                     jne 0x2fdd3b0
0x2fdd3e3: bf08080000               mov edi, 0x808
0x2fdd3e8: 488d75c0                 lea rsi, [rbp - 0x40]
0x2fdd3ec: 488d55cc                 lea rdx, [rbp - 0x34]
0x2fdd3f0: e8abc404fd               call 0x298a0
0x2fdd3f5: ebb9                     jmp 0x2fdd3b0
0x2fdd3f7: 4c8b75c0                 mov r14, qword ptr [rbp - 0x40]
0x2fdd3fb: 85c9                     test ecx, ecx
0x2fdd3fd: 7475                     je 0x2fdd474
0x2fdd3ff: 4c63e9                   movsxd r13, ecx
0x2fdd402: 4531ff                   xor r15d, r15d
0x2fdd405: 49c1e503                 shl r13, 3
0x2fdd409: eb1e                     jmp 0x2fdd429
0x2fdd40b: 0f1f440000               nop dword ptr [rax + rax]
0x2fdd410: 488b03                   mov rax, qword ptr [rbx]
0x2fdd413: 4889df                   mov rdi, rbx
0x2fdd416: 488d75b8                 lea rsi, [rbp - 0x48]
0x2fdd41a: ff9048080000             call qword ptr [rax + 0x848]
0x2fdd420: 4983c708                 add r15, 8
0x2fdd424: 4d39fd                   cmp r13, r15
0x2fdd427: 7447                     je 0x2fdd470
0x2fdd429: 4f8b243e                 mov r12, qword ptr [r14 + r15]
0x2fdd42d: 4c8965b8                 mov qword ptr [rbp - 0x48], r12
0x2fdd431: 4983bc24b002000000       cmp qword ptr [r12 + 0x2b0], 0
0x2fdd43a: 74e4                     je 0x2fdd420
0x2fdd43c: 4d85e4                   test r12, r12
0x2fdd43f: 74cf                     je 0x2fdd410
0x2fdd441: e87aa14400               call 0x34275c0
0x2fdd446: 498b4c2410               mov rcx, qword ptr [r12 + 0x10]
0x2fdd44b: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x2fdd44f: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x2fdd452: 7fbc                     jg 0x2fdd410
0x2fdd454: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x2fdd458: 4883c030                 add rax, 0x30
0x2fdd45c: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x2fdd460: 75ae                     jne 0x2fdd410
0x2fdd462: 4c89e7                   mov rdi, r12
0x2fdd465: e8d6264600               call 0x343fb40
0x2fdd46a: 84c0                     test al, al
0x2fdd46c: 75a2                     jne 0x2fdd410
0x2fdd46e: ebb0                     jmp 0x2fdd420
0x2fdd470: 4c8b75c0                 mov r14, qword ptr [rbp - 0x40]
0x2fdd474: 4c8b258d4a1506           mov r12, qword ptr [rip + 0x6154a8d]
0x2fdd47b: 4d85f6                   test r14, r14
0x2fdd47e: 7408                     je 0x2fdd488
0x2fdd480: 4c89f7                   mov rdi, r14
0x2fdd483: e808a60dfd               call 0xb7a90
0x2fdd488: 498b0424                 mov rax, qword ptr [r12]
0x2fdd48c: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdd490: 750f                     jne 0x2fdd4a1
0x2fdd492: 4883c428                 add rsp, 0x28
0x2fdd496: 5b                       pop rbx
0x2fdd497: 415c                     pop r12
0x2fdd499: 415d                     pop r13
0x2fdd49b: 415e                     pop r14
0x2fdd49d: 415f                     pop r15
0x2fdd49f: 5d                       pop rbp
0x2fdd4a0: c3                       ret 
0x2fdd4a1: e8ba494904               call 0x7471e60
0x2fdd4a6: 0f0b                     ud2 
0x2fdd4a8: cc                       int3 
0x2fdd4a9: cc                       int3 
0x2fdd4aa: cc                       int3 
0x2fdd4ab: cc                       int3 
0x2fdd4ac: cc                       int3 
0x2fdd4ad: cc                       int3 
0x2fdd4ae: cc                       int3 
0x2fdd4af: cc                       int3 
