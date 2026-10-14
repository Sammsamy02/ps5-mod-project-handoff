0x2fdc840: 55                       push rbp
0x2fdc841: 4889e5                   mov rbp, rsp
0x2fdc844: 4157                     push r15
0x2fdc846: 4156                     push r14
0x2fdc848: 53                       push rbx
0x2fdc849: 4883ec28                 sub rsp, 0x28
0x2fdc84d: 4c8b3db4561506           mov r15, qword ptr [rip + 0x61556b4]
0x2fdc854: 4989f6                   mov r14, rsi
0x2fdc857: 498b07                   mov rax, qword ptr [r15]
0x2fdc85a: 488945e0                 mov qword ptr [rbp - 0x20], rax
0x2fdc85e: e83ddab3ff               call 0x2b1a2a0
0x2fdc863: 488d35e8645804           lea rsi, [rip + 0x45864e8] ; ['%']
0x2fdc86a: 4c89f7                   mov rdi, r14
0x2fdc86d: ba01000000               mov edx, 1
0x2fdc872: b901000000               mov ecx, 1
0x2fdc877: 41b9ffffffff             mov r9d, 0xffffffff
0x2fdc87d: 4531c0                   xor r8d, r8d
0x2fdc880: 31db                     xor ebx, ebx
0x2fdc882: e8e98508fd               call 0x64e70
0x2fdc887: 83f8ff                   cmp eax, -1
0x2fdc88a: 0f854b010000             jne 0x2fdc9db
0x2fdc890: 488d354f4b9e04           lea rsi, [rip + 0x49e4b4f] ; [':']
0x2fdc897: 4c89f7                   mov rdi, r14
0x2fdc89a: ba01000000               mov edx, 1
0x2fdc89f: b901000000               mov ecx, 1
0x2fdc8a4: 41b9ffffffff             mov r9d, 0xffffffff
0x2fdc8aa: 4531c0                   xor r8d, r8d
0x2fdc8ad: 31db                     xor ebx, ebx
0x2fdc8af: e8bc8508fd               call 0x64e70
0x2fdc8b4: 83f8ff                   cmp eax, -1
0x2fdc8b7: 0f851e010000             jne 0x2fdc9db
0x2fdc8bd: 488d3526268704           lea rsi, [rip + 0x4872626] ; ['\\']
0x2fdc8c4: 4c89f7                   mov rdi, r14
0x2fdc8c7: ba01000000               mov edx, 1
0x2fdc8cc: b901000000               mov ecx, 1
0x2fdc8d1: 41b9ffffffff             mov r9d, 0xffffffff
0x2fdc8d7: 4531c0                   xor r8d, r8d
0x2fdc8da: 31db                     xor ebx, ebx
0x2fdc8dc: e88f8508fd               call 0x64e70
0x2fdc8e1: 83f8ff                   cmp eax, -1
0x2fdc8e4: 0f85f1000000             jne 0x2fdc9db
0x2fdc8ea: 488d355d5f5804           lea rsi, [rip + 0x4585f5d] ; ['?']
0x2fdc8f1: 4c89f7                   mov rdi, r14
0x2fdc8f4: ba01000000               mov edx, 1
0x2fdc8f9: b901000000               mov ecx, 1
0x2fdc8fe: 41b9ffffffff             mov r9d, 0xffffffff
0x2fdc904: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdc908: 4531c0                   xor r8d, r8d
0x2fdc90b: c5f81145c0               vmovups xmmword ptr [rbp - 0x40], xmm0
0x2fdc910: e85b8508fd               call 0x64e70
0x2fdc915: 83f8ff                   cmp eax, -1
0x2fdc918: 740d                     je 0x2fdc927
0x2fdc91a: 418b4e08                 mov ecx, dword ptr [r14 + 8]
0x2fdc91e: 85c9                     test ecx, ecx
0x2fdc920: 7413                     je 0x2fdc935
0x2fdc922: 498b36                   mov rsi, qword ptr [r14]
0x2fdc925: eb15                     jmp 0x2fdc93c
0x2fdc927: 488d7dc0                 lea rdi, [rbp - 0x40]
0x2fdc92b: 4c89f6                   mov rsi, r14
0x2fdc92e: e8ed90a4fd               call 0xa25a20
0x2fdc933: eb3f                     jmp 0x2fdc974
0x2fdc935: 488d35642c6504           lea rsi, [rip + 0x4652c64]
0x2fdc93c: 31d2                     xor edx, edx
0x2fdc93e: 83e901                   sub ecx, 1
0x2fdc941: 488d7dd0                 lea rdi, [rbp - 0x30]
0x2fdc945: 0f42ca                   cmovb ecx, edx
0x2fdc948: 39c8                     cmp eax, ecx
0x2fdc94a: 0f4cc8                   cmovl ecx, eax
0x2fdc94d: 89c8                     mov eax, ecx
0x2fdc94f: c1f81f                   sar eax, 0x1f
0x2fdc952: c4e278f2d1               andn edx, eax, ecx
0x2fdc957: e8b47a08fd               call 0x64410
0x2fdc95c: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdc960: 4885ff                   test rdi, rdi
0x2fdc963: 7405                     je 0x2fdc96a
0x2fdc965: e826b10dfd               call 0xb7a90
0x2fdc96a: c5f81045d0               vmovups xmm0, xmmword ptr [rbp - 0x30]
0x2fdc96f: c5f81145c0               vmovups xmmword ptr [rbp - 0x40], xmm0
0x2fdc974: 488d7dd0                 lea rdi, [rbp - 0x30]
0x2fdc978: e8d3ff11fd               call 0xfc950
0x2fdc97d: 488d35ae036d04           lea rsi, [rip + 0x46d03ae] ; ['/']
0x2fdc984: 488d7dc0                 lea rdi, [rbp - 0x40]
0x2fdc988: ba01000000               mov edx, 1
0x2fdc98d: b901000000               mov ecx, 1
0x2fdc992: e8a9a308fd               call 0x66d40
0x2fdc997: 84c0                     test al, al
0x2fdc999: 7421                     je 0x2fdc9bc
0x2fdc99b: 8b75c8                   mov esi, dword ptr [rbp - 0x38]
0x2fdc99e: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdc9a2: 31db                     xor ebx, ebx
0x2fdc9a4: 488d4dd0                 lea rcx, [rbp - 0x30]
0x2fdc9a8: ba01000000               mov edx, 1
0x2fdc9ad: 83ee01                   sub esi, 1
0x2fdc9b0: 0f42f3                   cmovb esi, ebx
0x2fdc9b3: e808382efd               call 0x2c01c0
0x2fdc9b8: 84c0                     test al, al
0x2fdc9ba: 7402                     je 0x2fdc9be
0x2fdc9bc: b301                     mov bl, 1
