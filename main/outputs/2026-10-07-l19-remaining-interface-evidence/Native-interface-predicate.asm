0x324550 f0x328550 4885f6                         test rsi, rsi
0x324553 f0x328553 7409                           je 0x32455e
0x324555 f0x328555 f686d500000040                 test byte ptr [rsi + 0xd5], 0x40
0x32455c f0x32855c 7503                           jne 0x324561
0x32455e f0x32855e 31c0                           xor eax, eax
0x324560 f0x328560 c3                             ret 
0x324561 f0x328561 55                             push rbp
0x324562 f0x328562 4889e5                         mov rbp, rsp
0x324565 f0x328565 4156                           push r14
0x324567 f0x328567 53                             push rbx
0x324568 f0x328568 4889f3                         mov rbx, rsi
0x32456b f0x32856b 4989fe                         mov r14, rdi
0x32456e f0x32856e e8adb40700                     call 0x39fa20
0x324573 f0x328573 4839d8                         cmp rax, rbx
0x324576 f0x328576 0f94c0                         sete al
0x324579 f0x328579 4d85f6                         test r14, r14
0x32457c f0x32857c 400f94c6                       sete sil
0x324580 f0x328580 4008c6                         or sil, al
0x324583 f0x328583 7407                           je 0x32458c
0x324585 f0x328585 31c0                           xor eax, eax
0x324587 f0x328587 5b                             pop rbx
0x324588 f0x328588 415e                           pop r14
0x32458a f0x32858a 5d                             pop rbp
0x32458b f0x32858b c3                             ret 
0x32458c f0x32858c 4c89f1                         mov rcx, r14
0x32458f f0x32858f 4889da                         mov rdx, rbx
0x324592 f0x328592 488d7330                       lea rsi, [rbx + 0x30]
0x324596 f0x328596 eb13                           jmp 0x3245ab
0x324598 f0x328598 0f1f840000000000               nop dword ptr [rax + rax]
0x3245a0 f0x3285a0 488b4940                       mov rcx, qword ptr [rcx + 0x40]
0x3245a4 f0x3285a4 31c0                           xor eax, eax
0x3245a6 f0x3285a6 4885c9                         test rcx, rcx
0x3245a9 f0x3285a9 74dc                           je 0x324587
0x3245ab f0x3285ab 486381e0010000                 movsxd rax, dword ptr [rcx + 0x1e0]
0x3245b2 f0x3285b2 4885c0                         test rax, rax
0x3245b5 f0x3285b5 7ee9                           jle 0x3245a0
0x3245b7 f0x3285b7 488bb9d8010000                 mov rdi, qword ptr [rcx + 0x1d8]
0x3245be f0x3285be 48c1e004                       shl rax, 4
0x3245c2 f0x3285c2 4531c0                         xor r8d, r8d
0x3245c5 f0x3285c5 eb12                           jmp 0x3245d9
0x3245c7 f0x3285c7 660f1f840000000000             nop word ptr [rax + rax]
0x3245d0 f0x3285d0 4983c010                       add r8, 0x10
0x3245d4 f0x3285d4 4c39c0                         cmp rax, r8
0x3245d7 f0x3285d7 74c7                           je 0x3245a0
0x3245d9 f0x3285d9 4e8b0c07                       mov r9, qword ptr [rdi + r8]
0x3245dd f0x3285dd 4d85c9                         test r9, r9
0x3245e0 f0x3285e0 74ee                           je 0x3245d0
0x3245e2 f0x3285e2 4c635238                       movsxd r10, dword ptr [rdx + 0x38]
0x3245e6 f0x3285e6 453b5138                       cmp r10d, dword ptr [r9 + 0x38]
0x3245ea f0x3285ea 7fe4                           jg 0x3245d0
0x3245ec f0x3285ec 4d8b4930                       mov r9, qword ptr [r9 + 0x30]
0x3245f0 f0x3285f0 4b3934d1                       cmp qword ptr [r9 + r10*8], rsi
0x3245f4 f0x3285f4 75da                           jne 0x3245d0
0x3245f6 f0x3285f6 b001                           mov al, 1
0x3245f8 f0x3285f8 5b                             pop rbx
0x3245f9 f0x3285f9 415e                           pop r14
0x3245fb f0x3285fb 5d                             pop rbp
0x3245fc f0x3285fc c3                             ret 
0x3245fd f0x3285fd cc                             int3 
0x3245fe f0x3285fe cc                             int3 
0x3245ff f0x3285ff cc                             int3 
