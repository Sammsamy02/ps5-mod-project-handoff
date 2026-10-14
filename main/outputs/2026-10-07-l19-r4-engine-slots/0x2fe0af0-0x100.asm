0x2fe0af0: 55                       push rbp
0x2fe0af1: 4889e5                   mov rbp, rsp
0x2fe0af4: 4157                     push r15
0x2fe0af6: 4156                     push r14
0x2fe0af8: 4155                     push r13
0x2fe0afa: 4154                     push r12
0x2fe0afc: 53                       push rbx
0x2fe0afd: 4883ec38                 sub rsp, 0x38
0x2fe0b01: 4c8b3d00141506           mov r15, qword ptr [rip + 0x6151400]
0x2fe0b08: 498b07                   mov rax, qword ptr [r15]
0x2fe0b0b: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fe0b0f: 4885f6                   test rsi, rsi
0x2fe0b12: 0f84b8000000             je 0x2fe0bd0
0x2fe0b18: 837a0802                 cmp dword ptr [rdx + 8], 2
0x2fe0b1c: 0f8cae000000             jl 0x2fe0bd0
0x2fe0b22: 4989fc                   mov r12, rdi
0x2fe0b25: 488bbeb0020000           mov rdi, qword ptr [rsi + 0x2b0]
0x2fe0b2c: 4989f6                   mov r14, rsi
0x2fe0b2f: 4889d6                   mov rsi, rdx
0x2fe0b32: 4189cd                   mov r13d, ecx
0x2fe0b35: 4889d3                   mov rbx, rdx
0x2fe0b38: 488b07                   mov rax, qword ptr [rdi]
0x2fe0b3b: ff9098070000             call qword ptr [rax + 0x798]
0x2fe0b41: 4c8d45b8                 lea r8, [rbp - 0x48]
0x2fe0b45: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fe0b49: c5f81145b8               vmovups xmmword ptr [rbp - 0x48], xmm0
0x2fe0b4e: 4c8975b0                 mov qword ptr [rbp - 0x50], r14
0x2fe0b52: 4939d8                   cmp r8, rbx
0x2fe0b55: 7444                     je 0x2fe0b9b
0x2fe0b57: 4889d8                   mov rax, rbx
0x2fe0b5a: 488b1b                   mov rbx, qword ptr [rbx]
0x2fe0b5d: 4c637008                 movsxd r14, dword ptr [rax + 8]
0x2fe0b61: 448975c0                 mov dword ptr [rbp - 0x40], r14d
0x2fe0b65: 4d85f6                   test r14, r14
0x2fe0b68: 7431                     je 0x2fe0b9b
0x2fe0b6a: 488d45c4                 lea rax, [rbp - 0x3c]
0x2fe0b6e: bf02000000               mov edi, 2
0x2fe0b73: be02000000               mov esi, 2
0x2fe0b78: 4489f2                   mov edx, r14d
0x2fe0b7b: 4589f1                   mov r9d, r14d
0x2fe0b7e: 31c9                     xor ecx, ecx
0x2fe0b80: 48890424                 mov qword ptr [rsp], rax
0x2fe0b84: e8476a04fd               call 0x275d0
0x2fe0b89: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x2fe0b8d: 4d01f6                   add r14, r14
0x2fe0b90: 4889de                   mov rsi, rbx
0x2fe0b93: 4c89f2                   mov rdx, r14
0x2fe0b96: e8b5124904               call 0x7471e50
0x2fe0b9b: 488b35ae155406           mov rsi, qword ptr [rip + 0x65415ae]
0x2fe0ba2: 4c89e7                   mov rdi, r12
0x2fe0ba5: 44886dc8                 mov byte ptr [rbp - 0x38], r13b
0x2fe0ba9: e8928b47fd               call 0x459740
0x2fe0bae: 498b0c24                 mov rcx, qword ptr [r12]
0x2fe0bb2: 488d55b0                 lea rdx, [rbp - 0x50]
0x2fe0bb6: 4c89e7                   mov rdi, r12
0x2fe0bb9: 4889c6                   mov rsi, rax
0x2fe0bbc: ff9168020000             call qword ptr [rcx + 0x268]
0x2fe0bc2: 488b7db8                 mov rdi, qword ptr [rbp - 0x48]
0x2fe0bc6: 4885ff                   test rdi, rdi
0x2fe0bc9: 7405                     je 0x2fe0bd0
0x2fe0bcb: e8c06e0dfd               call 0xb7a90
0x2fe0bd0: 498b07                   mov rax, qword ptr [r15]
0x2fe0bd3: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fe0bd7: 750f                     jne 0x2fe0be8
0x2fe0bd9: 4883c438                 add rsp, 0x38
0x2fe0bdd: 5b                       pop rbx
0x2fe0bde: 415c                     pop r12
0x2fe0be0: 415d                     pop r13
0x2fe0be2: 415e                     pop r14
0x2fe0be4: 415f                     pop r15
0x2fe0be6: 5d                       pop rbp
0x2fe0be7: c3                       ret 
0x2fe0be8: e873124904               call 0x7471e60
0x2fe0bed: 0f0b                     ud2 
0x2fe0bef: cc                       int3 
