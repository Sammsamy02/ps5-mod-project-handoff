0x2fdb640: 0f41807d000074           cmovno eax, dword ptr [rax + 0x7400007d]
0x2fdb647: 084889                   or byte ptr [rax - 0x77], cl
0x2fdb64a: dfe8                     fucompi st(0)
0x2fdb64c: 70cc                     jo 0x2fdb61a
0x2fdb64e: 38fd                     cmp ch, bh
0x2fdb650: 49899fb0020000           mov qword ptr [r15 + 0x2b0], rbx
0x2fdb657: 4c89f7                   mov rdi, r14
0x2fdb65a: ba01000000               mov edx, 1
0x2fdb65f: 31f6                     xor esi, esi
0x2fdb661: 4531ff                   xor r15d, r15d
0x2fdb664: e8e71c7b00               call 0x378d350
0x2fdb669: 4c8ba050030000           mov r12, qword ptr [rax + 0x350]
0x2fdb670: 4d85e4                   test r12, r12
0x2fdb673: 742a                     je 0x2fdb69f
0x2fdb675: 4989c7                   mov r15, rax
0x2fdb678: e89381a9ff               call 0x2a73810
0x2fdb67d: 4885c0                   test rax, rax
0x2fdb680: 741a                     je 0x2fdb69c
0x2fdb682: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdb686: 413b4c2438               cmp ecx, dword ptr [r12 + 0x38]
0x2fdb68b: 7f0f                     jg 0x2fdb69c
0x2fdb68d: 498b542430               mov rdx, qword ptr [r12 + 0x30]
0x2fdb692: 4883c030                 add rax, 0x30
0x2fdb696: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdb69a: 7461                     je 0x2fdb6fd
0x2fdb69c: 4531ff                   xor r15d, r15d
0x2fdb69f: 4d897e58                 mov qword ptr [r14 + 0x58], r15
0x2fdb6a3: 4889df                   mov rdi, rbx
0x2fdb6a6: 488b03                   mov rax, qword ptr [rbx]
0x2fdb6a9: ff90b8070000             call qword ptr [rax + 0x7b8]
0x2fdb6af: 48837da000               cmp qword ptr [rbp - 0x60], 0
0x2fdb6b4: 7415                     je 0x2fdb6cb
