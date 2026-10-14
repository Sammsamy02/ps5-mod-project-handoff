0x384ba60: 488bbd78ffffff           mov rdi, qword ptr [rbp - 0x88]
0x384ba67: 4885ff                   test rdi, rdi
0x384ba6a: 7405                     je 0x384ba71
0x384ba6c: e81fc086fc               call 0xb7a90
0x384ba71: 488bbd60ffffff           mov rdi, qword ptr [rbp - 0xa0]
0x384ba78: 4885ff                   test rdi, rdi
0x384ba7b: 7405                     je 0x384ba82
0x384ba7d: e80ec086fc               call 0xb7a90
0x384ba82: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384ba89: 4885ff                   test rdi, rdi
0x384ba8c: 0f8404040000             je 0x384be96
0x384ba92: e8f9bf86fc               call 0xb7a90
0x384ba97: e9fa030000               jmp 0x384be96
0x384ba9c: 4c89e7                   mov rdi, r12
0x384ba9f: e86ce8edff               call 0x372a310
0x384baa4: 84c0                     test al, al
0x384baa6: 0f8435020000             je 0x384bce1
0x384baac: 4c8db578fdffff           lea r14, [rbp - 0x288]
0x384bab3: 4c89e6                   mov rsi, r12
0x384bab6: 4c89f7                   mov rdi, r14
0x384bab9: e8a26b78ff               call 0x2fd2660
0x384babe: 498b4500                 mov rax, qword ptr [r13]
0x384bac2: 488bb568fdffff           mov rsi, qword ptr [rbp - 0x298]
0x384bac9: 4c89ef                   mov rdi, r13
0x384bacc: 4c89f2                   mov rdx, r14
0x384bacf: 4d89f8                   mov r8, r15
0x384bad2: 31c9                     xor ecx, ecx
0x384bad4: ff90f8040000             call qword ptr [rax + 0x4f8]
0x384bada: 3401                     xor al, 1
0x384badc: 4c89f7                   mov rdi, r14
0x384badf: 0fb6d8                   movzx ebx, al
0x384bae2: e829edf3ff               call 0x378a810
0x384bae7: e9aa030000               jmp 0x384be96
0x384baec: c7855cffffff00000000     mov dword ptr [rbp - 0xa4], 0
0x384baf6: be01000000               mov esi, 1
0x384bafb: 31ff                     xor edi, edi
0x384bafd: e85e10b2fc               call 0x36cb60
0x384bb02: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384bb09: 8bb558ffffff             mov esi, dword ptr [rbp - 0xa8]
0x384bb0f: e87cf77bfc               call 0xb290
0x384bb14: 488bbd50ffffff           mov rdi, qword ptr [rbp - 0xb0]
0x384bb1b: 4885ff                   test rdi, rdi
0x384bb1e: 7405                     je 0x384bb25
0x384bb20: e86bbf86fc               call 0xb7a90
0x384bb25: e876aeb7fc               call 0x3c69a0
0x384bb2a: 488d3553d51404           lea rsi, [rip + 0x414d553] ; ["NewObject with empty name can't be used to create default subobjects (inside of UObject derived class constructor) as it produces inconsistent object "]
0x384bb31: 4889c7                   mov rdi, rax
0x384bb34: 4889c3                   mov rbx, rax
0x384bb37: e8447ac4fc               call 0x493580
0x384bb3c: e8df36b1ff               call 0x335f220
0x384bb41: 4c8db550ffffff           lea r14, [rbp - 0xb0]
0x384bb48: 4889c6                   mov rsi, rax
0x384bb4b: 4c89f7                   mov rdi, r14
0x384bb4e: e8dd79c4fc               call 0x493530
0x384bb53: 4c89f7                   mov rdi, r14
0x384bb56: 48899d58ffffff           mov qword ptr [rbp - 0xa8], rbx
0x384bb5d: e83e4fc4fc               call 0x490aa0
0x384bb62: 48837d9000               cmp qword ptr [rbp - 0x70], 0
0x384bb67: 4889c3                   mov rbx, rax
0x384bb6a: 7415                     je 0x384bb81
0x384bb6c: 488b45b8                 mov rax, qword ptr [rbp - 0x48]
0x384bb70: 488d7da0                 lea rdi, [rbp - 0x60]
0x384bb74: 4885c0                   test rax, rax
0x384bb77: 480f45f8                 cmovne rdi, rax
0x384bb7b: 488b07                   mov rax, qword ptr [rdi]
0x384bb7e: ff5010                   call qword ptr [rax + 0x10]
0x384bb81: 4885db                   test rbx, rbx
0x384bb84: 7414                     je 0x384bb9a
0x384bb86: 488d05cf32b505           lea rax, [rip + 0x5b532cf]
0x384bb8d: 803800                   cmp byte ptr [rax], 0
0x384bb90: 7408                     je 0x384bb9a
0x384bb92: 4889df                   mov rdi, rbx
0x384bb95: e826c7b1fc               call 0x3682c0
0x384bb9a: 498db5c0000000           lea rsi, [r13 + 0xc0]
0x384bba1: 4889df                   mov rdi, rbx
0x384bba4: 49899d90010000           mov qword ptr [r13 + 0x190], rbx
0x384bbab: e8c038b1ff               call 0x335f470
