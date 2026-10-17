0x2fdc610: 55                       push rbp
0x2fdc611: 4889e5                   mov rbp, rsp
0x2fdc614: 4157                     push r15
0x2fdc616: 4156                     push r14
0x2fdc618: 4155                     push r13
0x2fdc61a: 4154                     push r12
0x2fdc61c: 53                       push rbx
0x2fdc61d: 4883ec58                 sub rsp, 0x58
0x2fdc621: 488b05e0581506           mov rax, qword ptr [rip + 0x61558e0]
0x2fdc628: 48897598                 mov qword ptr [rbp - 0x68], rsi
0x2fdc62c: 4189cd                   mov r13d, ecx
0x2fdc62f: 4189d4                   mov r12d, edx
0x2fdc632: 488b00                   mov rax, qword ptr [rax]
0x2fdc635: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdc639: e862dcb3ff               call 0x2b1a2a0
0x2fdc63e: 4889c7                   mov rdi, rax
0x2fdc641: e86abf7b00               call 0x37985b0
0x2fdc646: 85d2                     test edx, edx
0x2fdc648: 0f88ba010000             js 0x2fdc808
0x2fdc64e: 4189d7                   mov r15d, edx
0x2fdc651: 488945a0                 mov qword ptr [rbp - 0x60], rax
0x2fdc655: 395008                   cmp dword ptr [rax + 8], edx
0x2fdc658: 0f8eaa010000             jle 0x2fdc808
0x2fdc65e: 4180f501                 xor r13b, 1
0x2fdc662: 4489fb                   mov ebx, r15d
0x2fdc665: 410fb6cc                 movzx ecx, r12b
0x2fdc669: 4531f6                   xor r14d, r14d
0x2fdc66c: 410fb6c5                 movzx eax, r13b
0x2fdc670: 4c8b6da0                 mov r13, qword ptr [rbp - 0x60]
0x2fdc674: 48c1e303                 shl rbx, 3
0x2fdc678: 894dac                   mov dword ptr [rbp - 0x54], ecx
0x2fdc67b: 01c0                     add eax, eax
0x2fdc67d: 894594                   mov dword ptr [rbp - 0x6c], eax
0x2fdc680: eb3d                     jmp 0x2fdc6bf
0x2fdc682: 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x2fdc690: 498b0424                 mov rax, qword ptr [r12]
0x2fdc694: 488b7598                 mov rsi, qword ptr [rbp - 0x68]
0x2fdc698: 8b5594                   mov edx, dword ptr [rbp - 0x6c]
0x2fdc69b: 8b4dac                   mov ecx, dword ptr [rbp - 0x54]
0x2fdc69e: 4c89e7                   mov rdi, r12
0x2fdc6a1: ff90b80f0000             call qword ptr [rax + 0xfb8]
0x2fdc6a7: 4c8b6da0                 mov r13, qword ptr [rbp - 0x60]
0x2fdc6ab: 41ffc7                   inc r15d
0x2fdc6ae: 4883c308                 add rbx, 8
0x2fdc6b2: 4d89e6                   mov r14, r12
0x2fdc6b5: 45397d08                 cmp dword ptr [r13 + 8], r15d
0x2fdc6b9: 0f8e4c010000             jle 0x2fdc80b
0x2fdc6bf: 498b7d00                 mov rdi, qword ptr [r13]
0x2fdc6c3: 4801df                   add rdi, rbx
0x2fdc6c6: e835a64dfd               call 0x4b6d00
0x2fdc6cb: 4885c0                   test rax, rax
0x2fdc6ce: 0f842c010000             je 0x2fdc800
0x2fdc6d4: 4883b84803000000         cmp qword ptr [rax + 0x348], 0
0x2fdc6dc: 4989c4                   mov r12, rax
0x2fdc6df: 74af                     je 0x2fdc690
0x2fdc6e1: 4d89e5                   mov r13, r12
0x2fdc6e4: 4981c548030000           add r13, 0x348
0x2fdc6eb: e8a0572500               call 0x3231e90
0x2fdc6f0: 4c89ef                   mov rdi, r13
0x2fdc6f3: 4889c6                   mov rsi, rax
0x2fdc6f6: e8a5313ffd               call 0x3cf8a0
0x2fdc6fb: 84c0                     test al, al
0x2fdc6fd: 7491                     je 0x2fdc690
0x2fdc6ff: 49837d0000               cmp qword ptr [r13], 0
0x2fdc704: 748a                     je 0x2fdc690
0x2fdc706: 488b4598                 mov rax, qword ptr [rbp - 0x68]
0x2fdc70a: 48c745c000000000         mov qword ptr [rbp - 0x40], 0
0x2fdc712: 488b08                   mov rcx, qword ptr [rax]
0x2fdc715: 4c636808                 movsxd r13, dword ptr [rax + 8]
0x2fdc719: 44896dc8                 mov dword ptr [rbp - 0x38], r13d
0x2fdc71d: 4d85ed                   test r13, r13
0x2fdc720: 7449                     je 0x2fdc76b
0x2fdc722: 488d45cc                 lea rax, [rbp - 0x34]
0x2fdc726: bf02000000               mov edi, 2
0x2fdc72b: be02000000               mov esi, 2
0x2fdc730: 4489ea                   mov edx, r13d
0x2fdc733: 4c8d45c0                 lea r8, [rbp - 0x40]
0x2fdc737: 4589e9                   mov r9d, r13d
0x2fdc73a: 48894d88                 mov qword ptr [rbp - 0x78], rcx
0x2fdc73e: 31c9                     xor ecx, ecx
0x2fdc740: 48890424                 mov qword ptr [rsp], rax
0x2fdc744: e887ae04fd               call 0x275d0
0x2fdc749: 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x2fdc74d: 488b7588                 mov rsi, qword ptr [rbp - 0x78]
0x2fdc751: 4d01ed                   add r13, r13
0x2fdc754: 4c89ea                   mov rdx, r13
0x2fdc757: e8f4564904               call 0x7471e50
0x2fdc75c: 803d755b540600           cmp byte ptr [rip + 0x6545b75], 0
0x2fdc763: 4c8b6da0                 mov r13, qword ptr [rbp - 0x60]
0x2fdc767: 7416                     je 0x2fdc77f
0x2fdc769: eb67                     jmp 0x2fdc7d2
0x2fdc76b: c745cc00000000           mov dword ptr [rbp - 0x34], 0
0x2fdc772: 803d5f5b540600           cmp byte ptr [rip + 0x6545b5f], 0
0x2fdc779: 4c8b6da0                 mov r13, qword ptr [rbp - 0x60]
0x2fdc77d: 7553                     jne 0x2fdc7d2
0x2fdc77f: 488b7598                 mov rsi, qword ptr [rbp - 0x68]
0x2fdc783: 488d7db0                 lea rdi, [rbp - 0x50]
0x2fdc787: 488d1514139304           lea rdx, [rip + 0x4931314] ; ['?listen?']
