0x3003800: 41498b0424               mov rax, qword ptr [r12]
0x3003805: 483b45d8                 cmp rax, qword ptr [rbp - 0x28]
0x3003809: 750d                     jne 0x3003818
0x300380b: 4883c410                 add rsp, 0x10
0x300380f: 5b                       pop rbx
0x3003810: 415c                     pop r12
0x3003812: 415e                     pop r14
0x3003814: 415f                     pop r15
0x3003816: 5d                       pop rbp
0x3003817: c3                       ret 
0x3003818: e843e64604               call 0x7471e60
0x300381d: 0f0b                     ud2 
0x300381f: cc                       int3 
0x3003820: 55                       push rbp
0x3003821: 4889e5                   mov rbp, rsp
0x3003824: 4157                     push r15
0x3003826: 4156                     push r14
0x3003828: 4155                     push r13
0x300382a: 4154                     push r12
0x300382c: 53                       push rbx
0x300382d: 4883ec18                 sub rsp, 0x18
0x3003831: 4c8b35d0e61206           mov r14, qword ptr [rip + 0x612e6d0]
0x3003838: 498b06                   mov rax, qword ptr [r14]
0x300383b: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x300383f: 488b9fa8020000           mov rbx, qword ptr [rdi + 0x2a8]
0x3003846: 4885db                   test rbx, rbx
0x3003849: 0f84b0000000             je 0x30038ff
0x300384f: 488b05cae95106           mov rax, qword ptr [rip + 0x651e9ca]
0x3003856: 4885c0                   test rax, rax
0x3003859: 0f8589000000             jne 0x30038e8
0x300385f: 488d053a69b1ff           lea rax, [rip - 0x4e96c6]
0x3003866: 4c8d15838c29fd           lea r10, [rip - 0x2d6737d]
0x300386d: 4989ff                   mov r15, rdi
0x3003870: 4c8d1d691d7afe           lea r11, [rip - 0x185e297]
0x3003877: 4c8d65c8                 lea r12, [rbp - 0x38]
0x300387b: 4c8d2d5ec8fffc           lea r13, [rip - 0x30037a2]
0x3003882: 488d3591708304           lea rsi, [rip + 0x4837091] ; ['GameModeBase']
0x3003889: 488d1590e95106           lea rdx, [rip + 0x651e990]
0x3003890: 488d0d096ffdff           lea rcx, [rip - 0x290f7]
0x3003897: 41b840030000             mov r8d, 0x340
0x300389d: 41b908000000             mov r9d, 8
0x30038a3: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x30038a7: 488d05d4e29b04           lea rax, [rip + 0x49be2d4] ; ['/Script/Engine']
0x30038ae: 4889c7                   mov rdi, rax
0x30038b1: 4152                     push r10
0x30038b3: 4153                     push r11
0x30038b5: 4154                     push r12
0x30038b7: 4155                     push r13
0x30038b9: 488d05c06ffdff           lea rax, [rip - 0x29040]
0x30038c0: 50                       push rax
0x30038c1: 488d056ebe6204           lea rax, [rip + 0x462be6e] ; ['Game']
0x30038c8: 50                       push rax
0x30038c9: 6a00                     push 0
0x30038cb: 680c000010               push 0x1000000c
0x30038d0: e88b9f30fd               call 0x30d860
0x30038d5: 4c89ff                   mov rdi, r15
0x30038d8: 4883c440                 add rsp, 0x40
0x30038dc: 488b053de95106           mov rax, qword ptr [rip + 0x651e93d]
0x30038e3: 4885c0                   test rax, rax
0x30038e6: 7417                     je 0x30038ff
0x30038e8: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x30038ec: 3b4b38                   cmp ecx, dword ptr [rbx + 0x38]
0x30038ef: 7f0e                     jg 0x30038ff
0x30038f1: 488b5330                 mov rdx, qword ptr [rbx + 0x30]
0x30038f5: 4883c030                 add rax, 0x30
0x30038f9: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x30038fd: 741a                     je 0x3003919
0x30038ff: 31c0                     xor eax, eax
0x3003901: 498b0e                   mov rcx, qword ptr [r14]
0x3003904: 483b4dd0                 cmp rcx, qword ptr [rbp - 0x30]
0x3003908: 754f                     jne 0x3003959
0x300390a: 4883c418                 add rsp, 0x18
0x300390e: 5b                       pop rbx
0x300390f: 415c                     pop r12
0x3003911: 415d                     pop r13
0x3003913: 415e                     pop r14
0x3003915: 415f                     pop r15
0x3003917: 5d                       pop rbp
0x3003918: c3                       ret 
0x3003919: 488b97a8020000           mov rdx, qword ptr [rdi + 0x2a8]
0x3003920: 4885d2                   test rdx, rdx
0x3003923: 74da                     je 0x30038ff
0x3003925: 3b4a38                   cmp ecx, dword ptr [rdx + 0x38]
0x3003928: 7e04                     jle 0x300392e
0x300392a: 31db                     xor ebx, ebx
0x300392c: eb0e                     jmp 0x300393c
0x300392e: 488b7230                 mov rsi, qword ptr [rdx + 0x30]
0x3003932: 31db                     xor ebx, ebx
0x3003934: 483904ce                 cmp qword ptr [rsi + rcx*8], rax
0x3003938: 480f44da                 cmove rbx, rdx
0x300393c: 488b8310010000           mov rax, qword ptr [rbx + 0x110]
0x3003943: 4885c0                   test rax, rax
0x3003946: 75b9                     jne 0x3003901
0x3003948: 4889df                   mov rdi, rbx
0x300394b: e860fc31fd               call 0x3235b0
0x3003950: 488b8310010000           mov rax, qword ptr [rbx + 0x110]
0x3003957: eba8                     jmp 0x3003901
0x3003959: e802e54604               call 0x7471e60
0x300395e: 0f0b                     ud2 
0x3003960: 55                       push rbp
0x3003961: 4889e5                   mov rbp, rsp
0x3003964: 488b07                   mov rax, qword ptr [rdi]
0x3003967: ff9088070000             call qword ptr [rax + 0x788]
0x300396d: 5d                       pop rbp
0x300396e: c3                       ret 
0x300396f: cc                       int3 
0x3003970: 55                       push rbp
0x3003971: 4889e5                   mov rbp, rsp
0x3003974: 488b07                   mov rax, qword ptr [rdi]
0x3003977: ff9090070000             call qword ptr [rax + 0x790]
0x300397d: 5d                       pop rbp
0x300397e: c3                       ret 
0x300397f: cc                       int3 
