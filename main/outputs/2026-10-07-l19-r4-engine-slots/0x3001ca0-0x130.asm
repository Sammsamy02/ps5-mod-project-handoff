0x3001ca0: bba8020000               mov ebx, 0x2a8
0x3001ca5: 00747e48                 add byte ptr [rsi + rdi*2 + 0x48], dh
0x3001ca9: 833d7005520600           cmp dword ptr [rip + 0x6520570], 0
0x3001cb0: 7574                     jne 0x3001d26
0x3001cb2: 488d05e784b1ff           lea rax, [rip - 0x4e7b19]
0x3001cb9: 4c8d1520397afe           lea r10, [rip - 0x185c6e0]
0x3001cc0: 4c8d5dc8                 lea r11, [rbp - 0x38]
0x3001cc4: 488d1d15e4fffc           lea rbx, [rip - 0x3001beb]
0x3001ccb: 4c8d35ae8bfdff           lea r14, [rip - 0x27452]
0x3001cd2: 4c8d3d5dda6204           lea r15, [rip + 0x462da5d] ; ['Game']
0x3001cd9: 488d3da2fe9b04           lea rdi, [rip + 0x49bfea2] ; ['/Script/Engine']
0x3001ce0: 488d35338c8304           lea rsi, [rip + 0x4838c33] ; ['GameModeBase']
0x3001ce7: 488d1532055206           lea rdx, [rip + 0x6520532]
0x3001cee: 488d0dab8afdff           lea rcx, [rip - 0x27555]
0x3001cf5: 41b840030000             mov r8d, 0x340
0x3001cfb: 41b908000000             mov r9d, 8
0x3001d01: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x3001d05: 488d05e4a729fd           lea rax, [rip - 0x2d6581c]
0x3001d0c: 50                       push rax
0x3001d0d: 4152                     push r10
0x3001d0f: 4153                     push r11
0x3001d11: 53                       push rbx
0x3001d12: 4156                     push r14
0x3001d14: 4157                     push r15
0x3001d16: 6a00                     push 0
0x3001d18: 680c000010               push 0x1000000c
0x3001d1d: e83ebb30fd               call 0x30d860
0x3001d22: 4883c440                 add rsp, 0x40
0x3001d26: e88579fcff               call 0x2fc96b0
0x3001d2b: 488b05d6011306           mov rax, qword ptr [rip + 0x61301d6]
0x3001d32: 488b00                   mov rax, qword ptr [rax]
0x3001d35: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x3001d39: 750f                     jne 0x3001d4a
0x3001d3b: 4883c418                 add rsp, 0x18
0x3001d3f: 5b                       pop rbx
0x3001d40: 415c                     pop r12
0x3001d42: 415d                     pop r13
0x3001d44: 415e                     pop r14
0x3001d46: 415f                     pop r15
0x3001d48: 5d                       pop rbp
0x3001d49: c3                       ret 
0x3001d4a: e811014704               call 0x7471e60
0x3001d4f: 0f0b                     ud2 
0x3001d51: cc                       int3 
0x3001d52: cc                       int3 
0x3001d53: cc                       int3 
0x3001d54: cc                       int3 
0x3001d55: cc                       int3 
0x3001d56: cc                       int3 
0x3001d57: cc                       int3 
0x3001d58: cc                       int3 
0x3001d59: cc                       int3 
0x3001d5a: cc                       int3 
0x3001d5b: cc                       int3 
0x3001d5c: cc                       int3 
0x3001d5d: cc                       int3 
0x3001d5e: cc                       int3 
0x3001d5f: cc                       int3 
0x3001d60: 55                       push rbp
0x3001d61: 4889e5                   mov rbp, rsp
0x3001d64: 4157                     push r15
0x3001d66: 4156                     push r14
0x3001d68: 4155                     push r13
0x3001d6a: 4154                     push r12
0x3001d6c: 53                       push rbx
0x3001d6d: 4883ec18                 sub rsp, 0x18
0x3001d71: 488b0590011306           mov rax, qword ptr [rip + 0x6130190]
0x3001d78: 4889fb                   mov rbx, rdi
0x3001d7b: 488b00                   mov rax, qword ptr [rax]
0x3001d7e: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x3001d82: e81985b1ff               call 0x2b1a2a0
0x3001d87: 4889c7                   mov rdi, rax
0x3001d8a: e821687900               call 0x37985b0
0x3001d8f: 85d2                     test edx, edx
0x3001d91: 0f8887000000             js 0x3001e1e
0x3001d97: 4989c6                   mov r14, rax
0x3001d9a: 4189d7                   mov r15d, edx
0x3001d9d: 395008                   cmp dword ptr [rax + 8], edx
0x3001da0: 0f8e78000000             jle 0x3001e1e
0x3001da6: 4c8d25afd03906           lea r12, [rip + 0x639d0af]
0x3001dad: 4589fd                   mov r13d, r15d
0x3001db0: 48895dc0                 mov qword ptr [rbp - 0x40], rbx
0x3001db4: 49c1e503                 shl r13, 3
0x3001db8: eb23                     jmp 0x3001ddd
0x3001dba: 660f1f440000             nop word ptr [rax + rax]
0x3001dc0: 488b08                   mov rcx, qword ptr [rax]
0x3001dc3: 4889c7                   mov rdi, rax
0x3001dc6: 488d75c8                 lea rsi, [rbp - 0x38]
0x3001dca: ff91180f0000             call qword ptr [rcx + 0xf18]
