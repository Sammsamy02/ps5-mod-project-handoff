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
