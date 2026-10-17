0x2fdaf50: 55                       push rbp
0x2fdaf51: 4889e5                   mov rbp, rsp
0x2fdaf54: 4157                     push r15
0x2fdaf56: 4156                     push r14
0x2fdaf58: 4155                     push r13
0x2fdaf5a: 4154                     push r12
0x2fdaf5c: 53                       push rbx
0x2fdaf5d: 4881ec98000000           sub rsp, 0x98
0x2fdaf64: 488b059d6f1506           mov rax, qword ptr [rip + 0x6156f9d]
0x2fdaf6b: 4989d7                   mov r15, rdx
0x2fdaf6e: 4889fb                   mov rbx, rdi
0x2fdaf71: 488b00                   mov rax, qword ptr [rax]
0x2fdaf74: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdaf78: e823f3b3ff               call 0x2b1a2a0
0x2fdaf7d: 4989c5                   mov r13, rax
0x2fdaf80: 4c8da3a8020000           lea r12, [rbx + 0x2a8]
0x2fdaf87: 4d39fc                   cmp r12, r15
0x2fdaf8a: 746d                     je 0x2fdaff9
0x2fdaf8c: 8b8bb4020000             mov ecx, dword ptr [rbx + 0x2b4]
0x2fdaf92: 4d637708                 movsxd r14, dword ptr [r15 + 8]
0x2fdaf96: 498b37                   mov rsi, qword ptr [r15]
0x2fdaf99: 488d83b4020000           lea rax, [rbx + 0x2b4]
0x2fdafa0: 89ca                     mov edx, ecx
0x2fdafa2: 4489b3b0020000           mov dword ptr [rbx + 0x2b0], r14d
0x2fdafa9: 4409f2                   or edx, r14d
0x2fdafac: 7445                     je 0x2fdaff3
0x2fdafae: 4889b548ffffff           mov qword ptr [rbp - 0xb8], rsi
0x2fdafb5: 4883ec08                 sub rsp, 8
0x2fdafb9: bf02000000               mov edi, 2
0x2fdafbe: be02000000               mov esi, 2
0x2fdafc3: 4489f2                   mov edx, r14d
0x2fdafc6: 4d89e0                   mov r8, r12
0x2fdafc9: 4589f1                   mov r9d, r14d
0x2fdafcc: 50                       push rax
0x2fdafcd: e8fec504fd               call 0x275d0
0x2fdafd2: 4883c410                 add rsp, 0x10
0x2fdafd6: 4585f6                   test r14d, r14d
0x2fdafd9: 741e                     je 0x2fdaff9
0x2fdafdb: 498b3c24                 mov rdi, qword ptr [r12]
0x2fdafdf: 488bb548ffffff           mov rsi, qword ptr [rbp - 0xb8]
0x2fdafe6: 4d01f6                   add r14, r14
0x2fdafe9: 4c89f2                   mov rdx, r14
0x2fdafec: e85f6e4904               call 0x7471e50
0x2fdaff1: eb06                     jmp 0x2fdaff9
0x2fdaff3: c70000000000             mov dword ptr [rax], 0
0x2fdaff9: 488dbd60ffffff           lea rdi, [rbp - 0xa0]
0x2fdb000: e8ebe17a00               call 0x37891f0
0x2fdb005: 4889df                   mov rdi, rbx
0x2fdb008: e8c33d0bff               call 0x208edd0
0x2fdb00d: 48898578ffffff           mov qword ptr [rbp - 0x88], rax
0x2fdb014: 804d9440                 or byte ptr [rbp - 0x6c], 0x40
0x2fdb018: 488dbd50ffffff           lea rdi, [rbp - 0xb0]
0x2fdb01f: 4889de                   mov rsi, rbx
0x2fdb022: 488b03                   mov rax, qword ptr [rbx]
0x2fdb025: ff90c0070000             call qword ptr [rax + 0x7c0]
0x2fdb02b: 4c8bb550ffffff           mov r14, qword ptr [rbp - 0xb0]
0x2fdb032: 4d85f6                   test r14, r14
0x2fdb035: 0f84c0000000             je 0x2fdb0fb
0x2fdb03b: 488b0526775406           mov rax, qword ptr [rip + 0x6547726]
0x2fdb042: 4d89ec                   mov r12, r13
0x2fdb045: 4885c0                   test rax, rax
0x2fdb048: 0f8588000000             jne 0x2fdb0d6
0x2fdb04e: 488d054bf1b3ff           lea rax, [rip - 0x4c0eb5]
0x2fdb055: 4c8d25845002fd           lea r12, [rip - 0x2fdaf7c]
0x2fdb05c: 4c8d157da57cfe           lea r10, [rip - 0x1835a83]
0x2fdb063: 4c8d9d58ffffff           lea r11, [rbp - 0xa8]
0x2fdb06a: 488d3d116b9e04           lea rdi, [rip + 0x49e6b11] ; ['/Script/Engine']
0x2fdb071: 488d35a8ca9604           lea rsi, [rip + 0x496caa8] ; ['GameSession']
0x2fdb078: 488d15e9765406           lea rdx, [rip + 0x65476e9]
0x2fdb07f: 488d0d0a0203fd           lea rcx, [rip - 0x2fcfdf6]
0x2fdb086: 41b8d0020000             mov r8d, 0x2d0
0x2fdb08c: 41b908000000             mov r9d, 8
0x2fdb092: 48898558ffffff           mov qword ptr [rbp - 0xa8], rax
0x2fdb099: 488d0550142cfd           lea rax, [rip - 0x2d3ebb0]
0x2fdb0a0: 50                       push rax
0x2fdb0a1: 4152                     push r10
0x2fdb0a3: 4153                     push r11
0x2fdb0a5: 4154                     push r12
0x2fdb0a7: 488d05d2510200           lea rax, [rip + 0x251d2]
0x2fdb0ae: 50                       push rax
0x2fdb0af: 4d89ec                   mov r12, r13
0x2fdb0b2: 488d057d466504           lea rax, [rip + 0x465467d] ; ['Game']
0x2fdb0b9: 50                       push rax
0x2fdb0ba: 6a00                     push 0
0x2fdb0bc: 6804000010               push 0x10000004
0x2fdb0c1: e89a2733fd               call 0x30d860
0x2fdb0c6: 4883c440                 add rsp, 0x40
