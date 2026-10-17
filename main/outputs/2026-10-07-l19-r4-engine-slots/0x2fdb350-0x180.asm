0x2fdb350: 55                       push rbp
0x2fdb351: 4889e5                   mov rbp, rsp
0x2fdb354: 4157                     push r15
0x2fdb356: 4156                     push r14
0x2fdb358: 4155                     push r13
0x2fdb35a: 4154                     push r12
0x2fdb35c: 53                       push rbx
0x2fdb35d: 4881ec88000000           sub rsp, 0x88
0x2fdb364: 488b059d6b1506           mov rax, qword ptr [rip + 0x6156b9d]
0x2fdb36b: 4889fb                   mov rbx, rdi
0x2fdb36e: 488b00                   mov rax, qword ptr [rax]
0x2fdb371: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdb375: e8e6e9b5ff               call 0x2b39d60
0x2fdb37a: 488dbd60ffffff           lea rdi, [rbp - 0xa0]
0x2fdb381: e86ade7a00               call 0x37891f0
0x2fdb386: 4889df                   mov rdi, rbx
0x2fdb389: e8423a0bff               call 0x208edd0
0x2fdb38e: 48898578ffffff           mov qword ptr [rbp - 0x88], rax
0x2fdb395: 804d9440                 or byte ptr [rbp - 0x6c], 0x40
0x2fdb399: 4c8b3578745406           mov r14, qword ptr [rip + 0x6547478]
0x2fdb3a0: 4c8bbbc0020000           mov r15, qword ptr [rbx + 0x2c0]
0x2fdb3a7: 4d85ff                   test r15, r15
0x2fdb3aa: 0f84c3000000             je 0x2fdb473
0x2fdb3b0: 4d85f6                   test r14, r14
0x2fdb3b3: 0f8587000000             jne 0x2fdb440
0x2fdb3b9: 488d05e0edb3ff           lea rax, [rip - 0x4c1220]
0x2fdb3c0: 4c8d35194d02fd           lea r14, [rip - 0x2fdb2e7]
0x2fdb3c7: 4c8d1512a27cfe           lea r10, [rip - 0x1835dee]
0x2fdb3ce: 4c8d9d58ffffff           lea r11, [rbp - 0xa8]
0x2fdb3d5: 4c8d2554830200           lea r12, [rip + 0x28354]
0x2fdb3dc: 4c8d2d53436504           lea r13, [rip + 0x4654353] ; ['Game']
0x2fdb3e3: 488d3d98679e04           lea rdi, [rip + 0x49e6798] ; ['/Script/Engine']
0x2fdb3ea: 488d35ef6e6e04           lea rsi, [rip + 0x46e6eef] ; ['GameStateBase']
0x2fdb3f1: 488d1520745406           lea rdx, [rip + 0x6547420]
0x2fdb3f8: 488d0d51820200           lea rcx, [rip + 0x28251]
0x2fdb3ff: 41b800030000             mov r8d, 0x300
0x2fdb405: 41b908000000             mov r9d, 8
0x2fdb40b: 48898558ffffff           mov qword ptr [rbp - 0xa8], rax
0x2fdb412: 488d05d7102cfd           lea rax, [rip - 0x2d3ef29]
0x2fdb419: 50                       push rax
0x2fdb41a: 4152                     push r10
0x2fdb41c: 4153                     push r11
0x2fdb41e: 4156                     push r14
0x2fdb420: 4154                     push r12
0x2fdb422: 4155                     push r13
0x2fdb424: 6a00                     push 0
0x2fdb426: 6804000010               push 0x10000004
0x2fdb42b: e8302433fd               call 0x30d860
0x2fdb430: 4883c440                 add rsp, 0x40
0x2fdb434: 4c8b35dd735406           mov r14, qword ptr [rip + 0x65473dd]
0x2fdb43b: 4d85f6                   test r14, r14
0x2fdb43e: 743c                     je 0x2fdb47c
0x2fdb440: 49634638                 movsxd rax, dword ptr [r14 + 0x38]
0x2fdb444: 413b4738                 cmp eax, dword ptr [r15 + 0x38]
0x2fdb448: 0f8fb0000000             jg 0x2fdb4fe
0x2fdb44e: 498b5730                 mov rdx, qword ptr [r15 + 0x30]
0x2fdb452: 498d4e30                 lea rcx, [r14 + 0x30]
0x2fdb456: 48390cc2                 cmp qword ptr [rdx + rax*8], rcx
0x2fdb45a: 0f859e000000             jne 0x2fdb4fe
0x2fdb460: 4883bbc002000000         cmp qword ptr [rbx + 0x2c0], 0
0x2fdb468: 0f85b0000000             jne 0x2fdb51e
0x2fdb46e: e98b000000               jmp 0x2fdb4fe
0x2fdb473: 4d85f6                   test r14, r14
0x2fdb476: 0f8582000000             jne 0x2fdb4fe
0x2fdb47c: 488d051dedb3ff           lea rax, [rip - 0x4c12e3]
0x2fdb483: 4c8d35564c02fd           lea r14, [rip - 0x2fdb3aa]
0x2fdb48a: 4c8d154fa17cfe           lea r10, [rip - 0x1835eb1]
0x2fdb491: 4c8d9d58ffffff           lea r11, [rbp - 0xa8]
0x2fdb498: 4c8d3d91820200           lea r15, [rip + 0x28291]
0x2fdb49f: 4c8d2590426504           lea r12, [rip + 0x4654290] ; ['Game']
0x2fdb4a6: 488d3dd5669e04           lea rdi, [rip + 0x49e66d5] ; ['/Script/Engine']
0x2fdb4ad: 488d352c6e6e04           lea rsi, [rip + 0x46e6e2c] ; ['GameStateBase']
0x2fdb4b4: 488d155d735406           lea rdx, [rip + 0x654735d]
0x2fdb4bb: 488d0d8e810200           lea rcx, [rip + 0x2818e]
0x2fdb4c2: 41b800030000             mov r8d, 0x300
0x2fdb4c8: 41b908000000             mov r9d, 8
