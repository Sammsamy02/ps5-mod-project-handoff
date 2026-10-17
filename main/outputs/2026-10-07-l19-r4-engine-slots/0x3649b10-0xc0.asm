0x3649b10: 55                       push rbp
0x3649b11: 4889e5                   mov rbp, rsp
0x3649b14: 4157                     push r15
0x3649b16: 4156                     push r14
0x3649b18: 4154                     push r12
0x3649b1a: 53                       push rbx
0x3649b1b: 4883ec10                 sub rsp, 0x10
0x3649b1f: 488b1de283ae05           mov rbx, qword ptr [rip + 0x5ae83e2]
0x3649b26: 488b03                   mov rax, qword ptr [rbx]
0x3649b29: 488945d8                 mov qword ptr [rbp - 0x28], rax
0x3649b2d: 488b058c12f005           mov rax, qword ptr [rip + 0x5f0128c]
0x3649b34: 4885c0                   test rax, rax
0x3649b37: 757c                     jne 0x3649bb5
0x3649b39: 488d0560064dff           lea rax, [rip - 0xb2f9a0]
0x3649b40: 4c8d1539f88dff           lea r10, [rip - 0x7207c7]
0x3649b47: 4c8d5dd0                 lea r11, [rbp - 0x30]
0x3649b4b: 4c8d358e659bfc           lea r14, [rip - 0x3649a72]
0x3649b52: 4c8d3d97ffffff           lea r15, [rip - 0x69]
0x3649b59: 4c8d25d65bfe03           lea r12, [rip + 0x3fe5bd6] ; ['Game']
0x3649b60: 488d3d1b803704           lea rdi, [rip + 0x437801b] ; ['/Script/Engine']
0x3649b67: 488d35dc712404           lea rsi, [rip + 0x42471dc] ; ['SpectatorPawn']
0x3649b6e: 488d154b12f005           lea rdx, [rip + 0x5f0124b]
0x3649b75: 488d0d14179cfc           lea rcx, [rip - 0x363e8ec]
0x3649b7c: 41b850030000             mov r8d, 0x350
0x3649b82: 41b908000000             mov r9d, 8
0x3649b88: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x3649b8c: 488d055d29c5fc           lea rax, [rip - 0x33ad6a3]
0x3649b93: 50                       push rax
0x3649b94: 4152                     push r10
0x3649b96: 4153                     push r11
0x3649b98: 4156                     push r14
0x3649b9a: 4157                     push r15
0x3649b9c: 4154                     push r12
0x3649b9e: 6a00                     push 0
0x3649ba0: 6804000010               push 0x10000004
0x3649ba5: e8b63cccfc               call 0x30d860
0x3649baa: 4883c440                 add rsp, 0x40
0x3649bae: 488b050b12f005           mov rax, qword ptr [rip + 0x5f0120b]
0x3649bb5: 488b0b                   mov rcx, qword ptr [rbx]
0x3649bb8: 483b4dd8                 cmp rcx, qword ptr [rbp - 0x28]
0x3649bbc: 750d                     jne 0x3649bcb
0x3649bbe: 4883c410                 add rsp, 0x10
0x3649bc2: 5b                       pop rbx
0x3649bc3: 415c                     pop r12
0x3649bc5: 415e                     pop r14
0x3649bc7: 415f                     pop r15
0x3649bc9: 5d                       pop rbp
0x3649bca: c3                       ret 
0x3649bcb: e89082e203               call 0x7471e60
