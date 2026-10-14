0x2fdca00: 55                       push rbp
0x2fdca01: 4889e5                   mov rbp, rsp
0x2fdca04: 4157                     push r15
0x2fdca06: 4156                     push r14
0x2fdca08: 4155                     push r13
0x2fdca0a: 4154                     push r12
0x2fdca0c: 53                       push rbx
0x2fdca0d: 4881ec98000000           sub rsp, 0x98
0x2fdca14: 488b05ed541506           mov rax, qword ptr [rip + 0x61554ed]
0x2fdca1b: 4189d7                   mov r15d, edx
0x2fdca1e: 4989f4                   mov r12, rsi
0x2fdca21: 4989fe                   mov r14, rdi
0x2fdca24: 488b00                   mov rax, qword ptr [rax]
0x2fdca27: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdca2b: 488b07                   mov rax, qword ptr [rdi]
0x2fdca2e: ff9058080000             call qword ptr [rax + 0x858]
0x2fdca34: 4c89f7                   mov rdi, r14
0x2fdca37: e864d8b3ff               call 0x2b1a2a0
0x2fdca3c: 488d0dd56e5706           lea rcx, [rip + 0x6576ed5]
0x2fdca43: 4889c3                   mov rbx, rax
0x2fdca46: 4889de                   mov rsi, rbx
0x2fdca49: 488b39                   mov rdi, qword ptr [rcx]
0x2fdca4c: e81f858600               call 0x3844f70
0x2fdca51: 41f6862803000001         test byte ptr [r14 + 0x328], 1
0x2fdca59: 4989c5                   mov r13, rax
0x2fdca5c: 7522                     jne 0x2fdca80
0x2fdca5e: c78554ffffff00000000     mov dword ptr [rbp - 0xac], 0
0x2fdca68: 4981c5c0000000           add r13, 0xc0
0x2fdca6f: 41837c240800             cmp dword ptr [r12 + 8], 0
0x2fdca75: 7539                     jne 0x2fdcab0
0x2fdca77: 488d15222b6504           lea rdx, [rip + 0x4652b22]
0x2fdca7e: eb34                     jmp 0x2fdcab4
0x2fdca80: 4c89f7                   mov rdi, r14
0x2fdca83: e818d8b3ff               call 0x2b1a2a0
0x2fdca88: c5fb1005d8b4a604         vmovsd xmm0, qword ptr [rip + 0x4a6b4d8]
0x2fdca90: c5f92e8098070000         vucomisd xmm0, qword ptr [rax + 0x798]
0x2fdca98: 0f97c0                   seta al
0x2fdca9b: 898554ffffff             mov dword ptr [rbp - 0xac], eax
0x2fdcaa1: 4981c5c0000000           add r13, 0xc0
0x2fdcaa8: 41837c240800             cmp dword ptr [r12 + 8], 0
0x2fdcaae: 74c7                     je 0x2fdca77
0x2fdcab0: 498b1424                 mov rdx, qword ptr [r12]
0x2fdcab4: 4489f8                   mov eax, r15d
0x2fdcab7: 4489bd50ffffff           mov dword ptr [rbp - 0xb0], r15d
0x2fdcabe: 4c8dbd68ffffff           lea r15, [rbp - 0x98]
0x2fdcac5: 4c89ee                   mov rsi, r13
0x2fdcac8: 4989dc                   mov r12, rbx
0x2fdcacb: 3401                     xor al, 1
0x2fdcacd: 4c89ff                   mov rdi, r15
0x2fdcad0: 0fb6c8                   movzx ecx, al
0x2fdcad3: 01c9                     add ecx, ecx
0x2fdcad5: e8d6b67400               call 0x37281b0
0x2fdcada: 488d357d908d04           lea rsi, [rip + 0x48d907d] ; ['SeamlessTravel']
0x2fdcae1: 4c89ff                   mov rdi, r15
0x2fdcae4: e847d97400               call 0x372a430
0x2fdcae9: 41b701                   mov r15b, 1
0x2fdcaec: 84c0                     test al, al
0x2fdcaee: 7523                     jne 0x2fdcb13
0x2fdcaf0: 488d353dde8504           lea rsi, [rip + 0x485de3d] ; ['NoSeamlessTravel']
0x2fdcaf7: 488dbd68ffffff           lea rdi, [rbp - 0x98]
0x2fdcafe: e82dd97400               call 0x372a430
0x2fdcb03: 0fb68d54ffffff           movzx ecx, byte ptr [rbp - 0xac]
0x2fdcb0a: 4531ff                   xor r15d, r15d
0x2fdcb0d: 84c0                     test al, al
0x2fdcb0f: 440f44f9                 cmove r15d, ecx
0x2fdcb13: 4180bc248a01000003       cmp byte ptr [r12 + 0x18a], 3
0x2fdcb1c: 4c89a548ffffff           mov qword ptr [rbp - 0xb8], r12
0x2fdcb23: 752f                     jne 0x2fdcb54
0x2fdcb25: 4584ff                   test r15b, r15b
0x2fdcb28: 742a                     je 0x2fdcb54
0x2fdcb2a: e8f17f19fd               call 0x174b20
0x2fdcb2f: 488d35844c6904           lea rsi, [rip + 0x4694c84] ; ['MultiprocessOSS']
0x2fdcb36: 4889c7                   mov rdi, rax
0x2fdcb39: e802dc1ffd               call 0x1da740
0x2fdcb3e: 84c0                     test al, al
0x2fdcb40: 7512                     jne 0x2fdcb54
0x2fdcb42: 410fb6c7                 movzx eax, r15b
0x2fdcb46: 4531ff                   xor r15d, r15d
0x2fdcb49: 803d7057540600           cmp byte ptr [rip + 0x6545770], 0
0x2fdcb50: 440f45f8                 cmovne r15d, eax
0x2fdcb54: 4c8dad58ffffff           lea r13, [rbp - 0xa8]
0x2fdcb5b: 488db568ffffff           lea rsi, [rbp - 0x98]
0x2fdcb62: 31d2                     xor edx, edx
0x2fdcb64: 4c89ef                   mov rdi, r13
0x2fdcb67: e874d37400               call 0x3729ee0
0x2fdcb6c: 450fb6e7                 movzx r12d, r15b
0x2fdcb70: 440fb6bd50ffffff         movzx r15d, byte ptr [rbp - 0xb0]
0x2fdcb78: 498b06                   mov rax, qword ptr [r14]
0x2fdcb7b: 4c89f7                   mov rdi, r14
