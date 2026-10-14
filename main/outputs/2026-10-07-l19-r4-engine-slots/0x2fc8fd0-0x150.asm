0x2fc8fd0: 55                       push rbp
0x2fc8fd1: 4889e5                   mov rbp, rsp
0x2fc8fd4: e8c7180100               call 0x2fda8a0
0x2fc8fd9: 5d                       pop rbp
0x2fc8fda: c3                       ret 
0x2fc8fdb: cc                       int3 
0x2fc8fdc: cc                       int3 
0x2fc8fdd: cc                       int3 
0x2fc8fde: cc                       int3 
0x2fc8fdf: cc                       int3 
0x2fc8fe0: 55                       push rbp
0x2fc8fe1: 4889e5                   mov rbp, rsp
0x2fc8fe4: 4157                     push r15
0x2fc8fe6: 4156                     push r14
0x2fc8fe8: 4155                     push r13
0x2fc8fea: 4154                     push r12
0x2fc8fec: 53                       push rbx
0x2fc8fed: 50                       push rax
0x2fc8fee: 4c8d3d675e3d06           lea r15, [rip + 0x63d5e67]
0x2fc8ff5: 4889f3                   mov rbx, rsi
0x2fc8ff8: 4989fe                   mov r14, rdi
0x2fc8ffb: 4885f6                   test rsi, rsi
0x2fc8ffe: 740e                     je 0x2fc900e
0x2fc9000: 41803f00                 cmp byte ptr [r15], 0
0x2fc9004: 7408                     je 0x2fc900e
0x2fc9006: 4889df                   mov rdi, rbx
0x2fc9009: e8b2f239fd               call 0x3682c0
0x2fc900e: 4d636e58                 movsxd r13, dword ptr [r14 + 0x58]
0x2fc9012: 4d85ed                   test r13, r13
0x2fc9015: 746c                     je 0x2fc9083
0x2fc9017: 4d8b6650                 mov r12, qword ptr [r14 + 0x50]
0x2fc901b: 4c8965d0                 mov qword ptr [rbp - 0x30], r12
0x2fc901f: 4885db                   test rbx, rbx
0x2fc9022: 742e                     je 0x2fc9052
0x2fc9024: 410fb607                 movzx eax, byte ptr [r15]
0x2fc9028: 49c1e503                 shl r13, 3
0x2fc902c: 0f1f4000                 nop dword ptr [rax]
0x2fc9030: a801                     test al, 1
0x2fc9032: 740c                     je 0x2fc9040
0x2fc9034: 4889df                   mov rdi, rbx
0x2fc9037: e884f239fd               call 0x3682c0
0x2fc903c: 410fb607                 movzx eax, byte ptr [r15]
0x2fc9040: 49391c24                 cmp qword ptr [r12], rbx
0x2fc9044: 742d                     je 0x2fc9073
0x2fc9046: 4983c408                 add r12, 8
0x2fc904a: 4983c5f8                 add r13, -8
0x2fc904e: 75e0                     jne 0x2fc9030
0x2fc9050: eb31                     jmp 0x2fc9083
0x2fc9052: 49c1e503                 shl r13, 3
0x2fc9056: 662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x2fc9060: 49833c2400               cmp qword ptr [r12], 0
0x2fc9065: 740c                     je 0x2fc9073
0x2fc9067: 4983c408                 add r12, 8
0x2fc906b: 4983c5f8                 add r13, -8
0x2fc906f: 75ef                     jne 0x2fc9060
0x2fc9071: eb10                     jmp 0x2fc9083
0x2fc9073: 4c2b65d0                 sub r12, qword ptr [rbp - 0x30]
0x2fc9077: 49f7d4                   not r12
0x2fc907a: 49c1ec03                 shr r12, 3
0x2fc907e: 4585e4                   test r12d, r12d
0x2fc9081: 7540                     jne 0x2fc90c3
0x2fc9083: 418b4658                 mov eax, dword ptr [r14 + 0x58]
0x2fc9087: 413b465c                 cmp eax, dword ptr [r14 + 0x5c]
0x2fc908b: 7512                     jne 0x2fc909f
0x2fc908d: 498d7650                 lea rsi, [r14 + 0x50]
0x2fc9091: 498d565c                 lea rdx, [r14 + 0x5c]
0x2fc9095: bf08080000               mov edi, 0x808
0x2fc909a: e8010806fd               call 0x298a0
0x2fc909f: 8d4801                   lea ecx, [rax + 1]
0x2fc90a2: 4898                     cdqe 
0x2fc90a4: 41894e58                 mov dword ptr [r14 + 0x58], ecx
0x2fc90a8: 498b4e50                 mov rcx, qword ptr [r14 + 0x50]
0x2fc90ac: 48891cc1                 mov qword ptr [rcx + rax*8], rbx
0x2fc90b0: 4885db                   test rbx, rbx
0x2fc90b3: 740e                     je 0x2fc90c3
0x2fc90b5: 41803f00                 cmp byte ptr [r15], 0
0x2fc90b9: 7408                     je 0x2fc90c3
0x2fc90bb: 4889df                   mov rdi, rbx
0x2fc90be: e8fdf139fd               call 0x3682c0
0x2fc90c3: 4883c408                 add rsp, 8
0x2fc90c7: 5b                       pop rbx
0x2fc90c8: 415c                     pop r12
0x2fc90ca: 415d                     pop r13
0x2fc90cc: 415e                     pop r14
0x2fc90ce: 415f                     pop r15
0x2fc90d0: 5d                       pop rbp
0x2fc90d1: c3                       ret 
0x2fc90d2: cc                       int3 
0x2fc90d3: cc                       int3 
0x2fc90d4: cc                       int3 
0x2fc90d5: cc                       int3 
0x2fc90d6: cc                       int3 
0x2fc90d7: cc                       int3 
0x2fc90d8: cc                       int3 
0x2fc90d9: cc                       int3 
0x2fc90da: cc                       int3 
0x2fc90db: cc                       int3 
0x2fc90dc: cc                       int3 
0x2fc90dd: cc                       int3 
0x2fc90de: cc                       int3 
0x2fc90df: cc                       int3 
0x2fc90e0: 55                       push rbp
0x2fc90e1: 4889e5                   mov rbp, rsp
0x2fc90e4: 4157                     push r15
0x2fc90e6: 4156                     push r14
0x2fc90e8: 4155                     push r13
0x2fc90ea: 4154                     push r12
0x2fc90ec: 53                       push rbx
0x2fc90ed: 50                       push rax
0x2fc90ee: 4c8d25675d3d06           lea r12, [rip + 0x63d5d67]
0x2fc90f5: 4989f6                   mov r14, rsi
0x2fc90f8: 4889fb                   mov rbx, rdi
0x2fc90fb: 4885f6                   test rsi, rsi
0x2fc90fe: 740f                     je 0x2fc910f
0x2fc9100: 41803c2400               cmp byte ptr [r12], 0
0x2fc9105: 7408                     je 0x2fc910f
0x2fc9107: 4c89f7                   mov rdi, r14
0x2fc910a: e8b1f139fd               call 0x3682c0
0x2fc910f: 4c636b58                 movsxd r13, dword ptr [rbx + 0x58]
0x2fc9113: 4d85ed                   test r13, r13
0x2fc9116: 0f84cf000000             je 0x2fc91eb
0x2fc911c: 4c8b7b50                 mov r15, qword ptr [rbx + 0x50]
