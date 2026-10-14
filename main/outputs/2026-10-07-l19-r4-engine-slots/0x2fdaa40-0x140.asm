0x2fdaa40: 55                       push rbp
0x2fdaa41: 4889e5                   mov rbp, rsp
0x2fdaa44: 4157                     push r15
0x2fdaa46: 4156                     push r14
0x2fdaa48: 4154                     push r12
0x2fdaa4a: 53                       push rbx
0x2fdaa4b: 4883ec10                 sub rsp, 0x10
0x2fdaa4f: 488b1db2741506           mov rbx, qword ptr [rip + 0x61574b2]
0x2fdaa56: 488b03                   mov rax, qword ptr [rbx]
0x2fdaa59: 488945d8                 mov qword ptr [rbp - 0x28], rax
0x2fdaa5d: 488b05b47d5406           mov rax, qword ptr [rip + 0x6547db4]
0x2fdaa64: 4885c0                   test rax, rax
0x2fdaa67: 757c                     jne 0x2fdaae5
0x2fdaa69: 488d0530f7b3ff           lea rax, [rip - 0x4c08d0]
0x2fdaa70: 4c8d1569ab7cfe           lea r10, [rip - 0x1835497]
0x2fdaa77: 4c8d5dd0                 lea r11, [rbp - 0x30]
0x2fdaa7b: 4c8d355e5602fd           lea r14, [rip - 0x2fda9a2]
0x2fdaa82: 4c8d3da78c0200           lea r15, [rip + 0x28ca7]
0x2fdaa89: 4c8d25a64c6504           lea r12, [rip + 0x4654ca6] ; ['Game']
0x2fdaa90: 488d3deb709e04           lea rdi, [rip + 0x49e70eb] ; ['/Script/Engine']
0x2fdaa97: 488d3542786e04           lea rsi, [rip + 0x46e7842] ; ['GameStateBase']
0x2fdaa9e: 488d15737d5406           lea rdx, [rip + 0x6547d73]
0x2fdaaa5: 488d0da48b0200           lea rcx, [rip + 0x28ba4]
0x2fdaaac: 41b800030000             mov r8d, 0x300
0x2fdaab2: 41b908000000             mov r9d, 8
0x2fdaab8: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdaabc: 488d052d1a2cfd           lea rax, [rip - 0x2d3e5d3]
0x2fdaac3: 50                       push rax
0x2fdaac4: 4152                     push r10
0x2fdaac6: 4153                     push r11
0x2fdaac8: 4156                     push r14
0x2fdaaca: 4157                     push r15
0x2fdaacc: 4154                     push r12
0x2fdaace: 6a00                     push 0
0x2fdaad0: 6804000010               push 0x10000004
0x2fdaad5: e8862d33fd               call 0x30d860
0x2fdaada: 4883c440                 add rsp, 0x40
0x2fdaade: 488b05337d5406           mov rax, qword ptr [rip + 0x6547d33]
0x2fdaae5: 488b0b                   mov rcx, qword ptr [rbx]
0x2fdaae8: 483b4dd8                 cmp rcx, qword ptr [rbp - 0x28]
0x2fdaaec: 750d                     jne 0x2fdaafb
0x2fdaaee: 4883c410                 add rsp, 0x10
0x2fdaaf2: 5b                       pop rbx
0x2fdaaf3: 415c                     pop r12
0x2fdaaf5: 415e                     pop r14
0x2fdaaf7: 415f                     pop r15
0x2fdaaf9: 5d                       pop rbp
0x2fdaafa: c3                       ret 
0x2fdaafb: e860734904               call 0x7471e60
0x2fdab00: 0f0b                     ud2 
0x2fdab02: cc                       int3 
0x2fdab03: cc                       int3 
0x2fdab04: cc                       int3 
0x2fdab05: cc                       int3 
0x2fdab06: cc                       int3 
0x2fdab07: cc                       int3 
0x2fdab08: cc                       int3 
0x2fdab09: cc                       int3 
0x2fdab0a: cc                       int3 
0x2fdab0b: cc                       int3 
0x2fdab0c: cc                       int3 
0x2fdab0d: cc                       int3 
0x2fdab0e: cc                       int3 
0x2fdab0f: cc                       int3 
0x2fdab10: 55                       push rbp
0x2fdab11: 4889e5                   mov rbp, rsp
0x2fdab14: 808f2803000001           or byte ptr [rdi + 0x328], 1
0x2fdab1b: 5d                       pop rbp
0x2fdab1c: c3                       ret 
0x2fdab1d: cc                       int3 
0x2fdab1e: cc                       int3 
0x2fdab1f: cc                       int3 
0x2fdab20: 55                       push rbp
0x2fdab21: 4889e5                   mov rbp, rsp
0x2fdab24: 808f2803000002           or byte ptr [rdi + 0x328], 2
0x2fdab2b: 5d                       pop rbp
0x2fdab2c: c3                       ret 
0x2fdab2d: cc                       int3 
0x2fdab2e: cc                       int3 
0x2fdab2f: cc                       int3 
0x2fdab30: 55                       push rbp
0x2fdab31: 4889e5                   mov rbp, rsp
0x2fdab34: 808f2803000004           or byte ptr [rdi + 0x328], 4
0x2fdab3b: 5d                       pop rbp
0x2fdab3c: c3                       ret 
0x2fdab3d: cc                       int3 
0x2fdab3e: cc                       int3 
0x2fdab3f: cc                       int3 
0x2fdab40: 55                       push rbp
0x2fdab41: 4889e5                   mov rbp, rsp
0x2fdab44: 53                       push rbx
0x2fdab45: 50                       push rax
0x2fdab46: 488d0563f54a05           lea rax, [rip + 0x54af563]
0x2fdab4d: 4889fb                   mov rbx, rdi
0x2fdab50: 488907                   mov qword ptr [rdi], rax
0x2fdab53: 488bbf30030000           mov rdi, qword ptr [rdi + 0x330]
0x2fdab5a: 8bb338030000             mov esi, dword ptr [rbx + 0x338]
0x2fdab60: e85b6805fd               call 0x313c0
0x2fdab65: 488bbb30030000           mov rdi, qword ptr [rbx + 0x330]
0x2fdab6c: 4885ff                   test rdi, rdi
0x2fdab6f: 7405                     je 0x2fdab76
0x2fdab71: e81acf0dfd               call 0xb7a90
0x2fdab76: 488bbb18030000           mov rdi, qword ptr [rbx + 0x318]
0x2fdab7d: 4885ff                   test rdi, rdi
