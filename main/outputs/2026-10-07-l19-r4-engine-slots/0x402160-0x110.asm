0x402160: 55                       push rbp
0x402161: 4889e5                   mov rbp, rsp
0x402164: 4157                     push r15
0x402166: 4156                     push r14
0x402168: 53                       push rbx
0x402169: 4883ec18                 sub rsp, 0x18
0x40216d: 4c8b3594fdd208           mov r14, qword ptr [rip + 0x8d2fd94]
0x402174: 4889fb                   mov rbx, rdi
0x402177: 498b06                   mov rax, qword ptr [r14]
0x40217a: 488945e0                 mov qword ptr [rbp - 0x20], rax
0x40217e: 488b3e                   mov rdi, qword ptr [rsi]
0x402181: 48897dd8                 mov qword ptr [rbp - 0x28], rdi
0x402185: 40f6c701                 test dil, 1
0x402189: 741d                     je 0x4021a8
0x40218b: 4883e7fe                 and rdi, 0xfffffffffffffffe
0x40218f: 7417                     je 0x4021a8
0x402191: 488d05c4ccf908           lea rax, [rip + 0x8f9ccc4]
0x402198: 803801                   cmp byte ptr [rax], 1
0x40219b: 750b                     jne 0x4021a8
0x40219d: 4989d7                   mov r15, rdx
0x4021a0: e81b61f6ff               call 0x3682c0
0x4021a5: 4c89fa                   mov rdx, r15
0x4021a8: 488d75d8                 lea rsi, [rbp - 0x28]
0x4021ac: 4889df                   mov rdi, rbx
0x4021af: e8ac6b0100               call 0x418d60
0x4021b4: 488d05cdfaae07           lea rax, [rip + 0x7aefacd]
0x4021bb: 488903                   mov qword ptr [rbx], rax
0x4021be: 498b06                   mov rax, qword ptr [r14]
0x4021c1: 483b45e0                 cmp rax, qword ptr [rbp - 0x20]
0x4021c5: 750b                     jne 0x4021d2
0x4021c7: 4883c418                 add rsp, 0x18
0x4021cb: 5b                       pop rbx
0x4021cc: 415e                     pop r14
0x4021ce: 415f                     pop r15
0x4021d0: 5d                       pop rbp
0x4021d1: c3                       ret 
0x4021d2: e889fc0607               call 0x7471e60
0x4021d7: 0f0b                     ud2 
0x4021d9: cc                       int3 
0x4021da: cc                       int3 
0x4021db: cc                       int3 
0x4021dc: cc                       int3 
0x4021dd: cc                       int3 
0x4021de: cc                       int3 
0x4021df: cc                       int3 
0x4021e0: 55                       push rbp
0x4021e1: 4889e5                   mov rbp, rsp
0x4021e4: 4157                     push r15
0x4021e6: 4156                     push r14
0x4021e8: 53                       push rbx
0x4021e9: 4883ec18                 sub rsp, 0x18
0x4021ed: 4c8b3d14fdd208           mov r15, qword ptr [rip + 0x8d2fd14]
0x4021f4: 4889fb                   mov rbx, rdi
0x4021f7: 498b07                   mov rax, qword ptr [r15]
0x4021fa: 488945e0                 mov qword ptr [rbp - 0x20], rax
0x4021fe: 48837f7800               cmp qword ptr [rdi + 0x78], 0
0x402203: 7434                     je 0x402239
0x402205: 4c8d7378                 lea r14, [rbx + 0x78]
0x402209: e872f2fbff               call 0x3c1480
0x40220e: 4c89f7                   mov rdi, r14
0x402211: 4889c6                   mov rsi, rax
0x402214: e887d6fcff               call 0x3cf8a0
0x402219: 84c0                     test al, al
0x40221b: 741c                     je 0x402239
0x40221d: 498b3e                   mov rdi, qword ptr [r14]
0x402220: 4885ff                   test rdi, rdi
0x402223: 7414                     je 0x402239
0x402225: 4881c7d0020000           add rdi, 0x2d0
0x40222c: 488d75d0                 lea rsi, [rbp - 0x30]
0x402230: 48895dd0                 mov qword ptr [rbp - 0x30], rbx
0x402234: e8c7b50200               call 0x42d800
0x402239: 48837b7000               cmp qword ptr [rbx + 0x70], 0
0x40223e: 7434                     je 0x402274
0x402240: 4c8d7370                 lea r14, [rbx + 0x70]
0x402244: e837f2fbff               call 0x3c1480
0x402249: 4c89f7                   mov rdi, r14
0x40224c: 4889c6                   mov rsi, rax
0x40224f: e84cd6fcff               call 0x3cf8a0
0x402254: 84c0                     test al, al
0x402256: 741c                     je 0x402274
0x402258: 498b3e                   mov rdi, qword ptr [r14]
0x40225b: 4885ff                   test rdi, rdi
0x40225e: 7414                     je 0x402274
0x402260: 4881c7d0020000           add rdi, 0x2d0
0x402267: 488d75d8                 lea rsi, [rbp - 0x28]
0x40226b: 48895dd8                 mov qword ptr [rbp - 0x28], rbx
