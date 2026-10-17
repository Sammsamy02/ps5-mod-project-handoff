0x2fdd110: 55                       push rbp
0x2fdd111: 4889e5                   mov rbp, rsp
0x2fdd114: 4157                     push r15
0x2fdd116: 4156                     push r14
0x2fdd118: 4155                     push r13
0x2fdd11a: 4154                     push r12
0x2fdd11c: 53                       push rbx
0x2fdd11d: 4883ec48                 sub rsp, 0x48
0x2fdd121: 4c8b2de04d1506           mov r13, qword ptr [rip + 0x6154de0]
0x2fdd128: 4989f7                   mov r15, rsi
0x2fdd12b: 4889fb                   mov rbx, rdi
0x2fdd12e: 498b4500                 mov rax, qword ptr [r13]
0x2fdd132: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdd136: 4c8b36                   mov r14, qword ptr [rsi]
0x2fdd139: 4d85f6                   test r14, r14
0x2fdd13c: 7424                     je 0x2fdd162
0x2fdd13e: e87da44400               call 0x34275c0
0x2fdd143: 498b4e10                 mov rcx, qword ptr [r14 + 0x10]
0x2fdd147: 48635038                 movsxd rdx, dword ptr [rax + 0x38]
0x2fdd14b: 3b5138                   cmp edx, dword ptr [rcx + 0x38]
0x2fdd14e: 7f12                     jg 0x2fdd162
0x2fdd150: 488b4930                 mov rcx, qword ptr [rcx + 0x30]
0x2fdd154: 4883c030                 add rax, 0x30
0x2fdd158: 483904d1                 cmp qword ptr [rcx + rdx*8], rax
0x2fdd15c: 0f8499000000             je 0x2fdd1fb
0x2fdd162: 4531f6                   xor r14d, r14d
0x2fdd165: 498b37                   mov rsi, qword ptr [r15]
0x2fdd168: 488b03                   mov rax, qword ptr [rbx]
0x2fdd16b: 4889df                   mov rdi, rbx
0x2fdd16e: ff9040090000             call qword ptr [rax + 0x940]
0x2fdd174: 498b37                   mov rsi, qword ptr [r15]
0x2fdd177: 488b03                   mov rax, qword ptr [rbx]
0x2fdd17a: 4889df                   mov rdi, rbx
0x2fdd17d: ff9010090000             call qword ptr [rax + 0x910]
0x2fdd183: 4d85f6                   test r14, r14
0x2fdd186: 745a                     je 0x2fdd1e2
0x2fdd188: 488b35794f5406           mov rsi, qword ptr [rip + 0x6544f79]
0x2fdd18f: 4889df                   mov rdi, rbx
0x2fdd192: e8a9c547fd               call 0x459740
0x2fdd197: 4889c7                   mov rdi, rax
0x2fdd19a: 4989c7                   mov r15, rax
0x2fdd19d: e85ed033fd               call 0x31a200
0x2fdd1a2: f680d400000080           test byte ptr [rax + 0xd4], 0x80
0x2fdd1a9: 7528                     jne 0x2fdd1d3
0x2fdd1ab: 4c8975b8                 mov qword ptr [rbp - 0x48], r14
0x2fdd1af: 488d55b8                 lea rdx, [rbp - 0x48]
0x2fdd1b3: 4889df                   mov rdi, rbx
0x2fdd1b6: 4c89fe                   mov rsi, r15
0x2fdd1b9: 488b03                   mov rax, qword ptr [rbx]
0x2fdd1bc: ff9068020000             call qword ptr [rax + 0x268]
0x2fdd1c2: 498b4500                 mov rax, qword ptr [r13]
0x2fdd1c6: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdd1ca: 7420                     je 0x2fdd1ec
0x2fdd1cc: e88f4c4904               call 0x7471e60
0x2fdd1d1: 0f0b                     ud2 
0x2fdd1d3: 488b03                   mov rax, qword ptr [rbx]
0x2fdd1d6: 4889df                   mov rdi, rbx
0x2fdd1d9: 4c89f6                   mov rsi, r14
0x2fdd1dc: ff9098070000             call qword ptr [rax + 0x798]
0x2fdd1e2: 498b4500                 mov rax, qword ptr [r13]
0x2fdd1e6: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdd1ea: 75e0                     jne 0x2fdd1cc
0x2fdd1ec: 4883c448                 add rsp, 0x48
0x2fdd1f0: 5b                       pop rbx
0x2fdd1f1: 415c                     pop r12
0x2fdd1f3: 415d                     pop r13
0x2fdd1f5: 415e                     pop r14
0x2fdd1f7: 415f                     pop r15
0x2fdd1f9: 5d                       pop rbp
0x2fdd1fa: c3                       ret 
0x2fdd1fb: 4983be4803000000         cmp qword ptr [r14 + 0x348], 0
0x2fdd203: 0f845cffffff             je 0x2fdd165
0x2fdd209: 488b03                   mov rax, qword ptr [rbx]
0x2fdd20c: 488d7db8                 lea rdi, [rbp - 0x48]
0x2fdd210: 4889de                   mov rsi, rbx
0x2fdd213: 4c89f2                   mov rdx, r14
0x2fdd216: ff9040080000             call qword ptr [rax + 0x840]
0x2fdd21c: 4c8b65b8                 mov r12, qword ptr [rbp - 0x48]
0x2fdd220: 4d85e4                   test r12, r12
0x2fdd223: 742a                     je 0x2fdd24f
0x2fdd225: e896a34400               call 0x34275c0
0x2fdd22a: 4885c0                   test rax, rax
0x2fdd22d: 7420                     je 0x2fdd24f
0x2fdd22f: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdd233: 413b4c2438               cmp ecx, dword ptr [r12 + 0x38]
0x2fdd238: 7f15                     jg 0x2fdd24f
0x2fdd23a: 498b542430               mov rdx, qword ptr [r12 + 0x30]
0x2fdd23f: 4883c030                 add rax, 0x30
0x2fdd243: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdd247: 7506                     jne 0x2fdd24f
0x2fdd249: 488b45b8                 mov rax, qword ptr [rbp - 0x48]
0x2fdd24d: eb02                     jmp 0x2fdd251
0x2fdd24f: 31c0                     xor eax, eax
0x2fdd251: 41f6863803000002         test byte ptr [r14 + 0x338], 2
0x2fdd259: 41bc02000000             mov r12d, 2
0x2fdd25f: 48894590                 mov qword ptr [rbp - 0x70], rax
0x2fdd263: 7412                     je 0x2fdd277
0x2fdd265: 498b06                   mov rax, qword ptr [r14]
0x2fdd268: 4c89f7                   mov rdi, r14
0x2fdd26b: ff90e8070000             call qword ptr [rax + 0x7e8]
0x2fdd271: 0fb6c0                   movzx eax, al
0x2fdd274: 4129c4                   sub r12d, eax
0x2fdd277: 498b06                   mov rax, qword ptr [r14]
0x2fdd27a: 488d7db8                 lea rdi, [rbp - 0x48]
0x2fdd27e: 4c89f6                   mov rsi, r14
0x2fdd281: ff90600f0000             call qword ptr [rax + 0xf60]
0x2fdd287: 498b06                   mov rax, qword ptr [r14]
0x2fdd28a: 488d7da0                 lea rdi, [rbp - 0x60]
