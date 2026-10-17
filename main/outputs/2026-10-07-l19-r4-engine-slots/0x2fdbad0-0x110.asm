0x2fdbad0: 55                       push rbp
0x2fdbad1: 4889e5                   mov rbp, rsp
0x2fdbad4: 4157                     push r15
0x2fdbad6: 4156                     push r14
0x2fdbad8: 4155                     push r13
0x2fdbada: 4154                     push r12
0x2fdbadc: 53                       push rbx
0x2fdbadd: 4883ec28                 sub rsp, 0x28
0x2fdbae1: 488b0520641506           mov rax, qword ptr [rip + 0x6156420]
0x2fdbae8: 4989fe                   mov r14, rdi
0x2fdbaeb: 488b00                   mov rax, qword ptr [rax]
0x2fdbaee: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdbaf2: e8a9e7b3ff               call 0x2b1a2a0
0x2fdbaf7: 4889c7                   mov rdi, rax
0x2fdbafa: e8b1ca7b00               call 0x37985b0
0x2fdbaff: 4531ff                   xor r15d, r15d
0x2fdbb02: 85d2                     test edx, edx
0x2fdbb04: 0f88ab000000             js 0x2fdbbb5
0x2fdbb0a: 4189d4                   mov r12d, edx
0x2fdbb0d: 395008                   cmp dword ptr [rax + 8], edx
0x2fdbb10: 0f8e9f000000             jle 0x2fdbbb5
0x2fdbb16: 4489e3                   mov ebx, r12d
0x2fdbb19: 4531ff                   xor r15d, r15d
0x2fdbb1c: 488945b0                 mov qword ptr [rbp - 0x50], rax
0x2fdbb20: 48c1e303                 shl rbx, 3
0x2fdbb24: eb2a                     jmp 0x2fdbb50
0x2fdbb26: 498b06                   mov rax, qword ptr [r14]
0x2fdbb29: 4c89f7                   mov rdi, r14
0x2fdbb2c: 4c89ee                   mov rsi, r13
0x2fdbb2f: ff9090070000             call qword ptr [rax + 0x790]
0x2fdbb35: 448b7dbc                 mov r15d, dword ptr [rbp - 0x44]
0x2fdbb39: 0fb6c0                   movzx eax, al
0x2fdbb3c: 4101c7                   add r15d, eax
0x2fdbb3f: 488b45b0                 mov rax, qword ptr [rbp - 0x50]
0x2fdbb43: 41ffc4                   inc r12d
0x2fdbb46: 4883c308                 add rbx, 8
0x2fdbb4a: 44396008                 cmp dword ptr [rax + 8], r12d
0x2fdbb4e: 7e65                     jle 0x2fdbbb5
0x2fdbb50: 488b38                   mov rdi, qword ptr [rax]
0x2fdbb53: 4801df                   add rdi, rbx
0x2fdbb56: e8a5b14dfd               call 0x4b6d00
0x2fdbb5b: 4885c0                   test rax, rax
0x2fdbb5e: 74df                     je 0x2fdbb3f
0x2fdbb60: 4883b8b002000000         cmp qword ptr [rax + 0x2b0], 0
0x2fdbb68: 4989c5                   mov r13, rax
0x2fdbb6b: 74d2                     je 0x2fdbb3f
0x2fdbb6d: 488b352c665406           mov rsi, qword ptr [rip + 0x654662c]
0x2fdbb74: 4c89f7                   mov rdi, r14
0x2fdbb77: 44897dbc                 mov dword ptr [rbp - 0x44], r15d
0x2fdbb7b: e8c0db47fd               call 0x459740
0x2fdbb80: 4889c7                   mov rdi, rax
0x2fdbb83: 4989c7                   mov r15, rax
0x2fdbb86: e875e633fd               call 0x31a200
0x2fdbb8b: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdbb92: 7592                     jne 0x2fdbb26
0x2fdbb94: c645c800                 mov byte ptr [rbp - 0x38], 0
0x2fdbb98: 4c896dc0                 mov qword ptr [rbp - 0x40], r13
0x2fdbb9c: 4c89f7                   mov rdi, r14
0x2fdbb9f: 4c89fe                   mov rsi, r15
0x2fdbba2: 488d55c0                 lea rdx, [rbp - 0x40]
0x2fdbba6: 498b06                   mov rax, qword ptr [r14]
0x2fdbba9: ff9068020000             call qword ptr [rax + 0x268]
0x2fdbbaf: 0fb645c8                 movzx eax, byte ptr [rbp - 0x38]
0x2fdbbb3: eb80                     jmp 0x2fdbb35
0x2fdbbb5: 488b054c631506           mov rax, qword ptr [rip + 0x615634c]
0x2fdbbbc: 488b00                   mov rax, qword ptr [rax]
0x2fdbbbf: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdbbc3: 7512                     jne 0x2fdbbd7
0x2fdbbc5: 4489f8                   mov eax, r15d
0x2fdbbc8: 4883c428                 add rsp, 0x28
0x2fdbbcc: 5b                       pop rbx
0x2fdbbcd: 415c                     pop r12
0x2fdbbcf: 415d                     pop r13
0x2fdbbd1: 415e                     pop r14
0x2fdbbd3: 415f                     pop r15
0x2fdbbd5: 5d                       pop rbp
0x2fdbbd6: c3                       ret 
0x2fdbbd7: e884624904               call 0x7471e60
0x2fdbbdc: 0f0b                     ud2 
0x2fdbbde: cc                       int3 
0x2fdbbdf: cc                       int3 
