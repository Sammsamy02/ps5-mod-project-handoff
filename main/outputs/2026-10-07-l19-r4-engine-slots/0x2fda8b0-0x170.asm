0x2fda8b0: 8b1d52761506             mov ebx, dword ptr [rip + 0x6157652]
0x2fda8b6: 488b03                   mov rax, qword ptr [rbx]
0x2fda8b9: 488945d8                 mov qword ptr [rbp - 0x28], rax
0x2fda8bd: 488b055c795406           mov rax, qword ptr [rip + 0x654795c]
0x2fda8c4: 4885c0                   test rax, rax
0x2fda8c7: 757c                     jne 0x2fda945
0x2fda8c9: 488d05d0f8b3ff           lea rax, [rip - 0x4c0730]
0x2fda8d0: 4c8d1509ad7cfe           lea r10, [rip - 0x18352f7]
0x2fda8d7: 4c8d5dd0                 lea r11, [rbp - 0x30]
0x2fda8db: 4c8d35fe5702fd           lea r14, [rip - 0x2fda802]
0x2fda8e2: 4c8d3d97ffffff           lea r15, [rip - 0x69]
0x2fda8e9: 4c8d25464e6504           lea r12, [rip + 0x4654e46] ; ['Game']
0x2fda8f0: 488d3d8b729e04           lea rdi, [rip + 0x49e728b] ; ['/Script/Engine']
0x2fda8f7: 488d351c008604           lea rsi, [rip + 0x486001c] ; ['GameModeBase']
0x2fda8fe: 488d151b795406           lea rdx, [rip + 0x654791b]
0x2fda905: 488d0d94feffff           lea rcx, [rip - 0x16c]
0x2fda90c: 41b840030000             mov r8d, 0x340
0x2fda912: 41b908000000             mov r9d, 8
0x2fda918: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fda91c: 488d05cd1b2cfd           lea rax, [rip - 0x2d3e433]
0x2fda923: 50                       push rax
0x2fda924: 4152                     push r10
0x2fda926: 4153                     push r11
0x2fda928: 4156                     push r14
0x2fda92a: 4157                     push r15
0x2fda92c: 4154                     push r12
0x2fda92e: 6a00                     push 0
0x2fda930: 680c000010               push 0x1000000c
0x2fda935: e8262f33fd               call 0x30d860
0x2fda93a: 4883c440                 add rsp, 0x40
0x2fda93e: 488b05db785406           mov rax, qword ptr [rip + 0x65478db]
0x2fda945: 488b0b                   mov rcx, qword ptr [rbx]
0x2fda948: 483b4dd8                 cmp rcx, qword ptr [rbp - 0x28]
0x2fda94c: 750d                     jne 0x2fda95b
0x2fda94e: 4883c410                 add rsp, 0x10
0x2fda952: 5b                       pop rbx
0x2fda953: 415c                     pop r12
0x2fda955: 415e                     pop r14
0x2fda957: 415f                     pop r15
0x2fda959: 5d                       pop rbp
0x2fda95a: c3                       ret 
0x2fda95b: e800754904               call 0x7471e60
0x2fda960: 0f0b                     ud2 
0x2fda962: cc                       int3 
0x2fda963: cc                       int3 
0x2fda964: cc                       int3 
0x2fda965: cc                       int3 
0x2fda966: cc                       int3 
0x2fda967: cc                       int3 
0x2fda968: cc                       int3 
0x2fda969: cc                       int3 
0x2fda96a: cc                       int3 
0x2fda96b: cc                       int3 
0x2fda96c: cc                       int3 
0x2fda96d: cc                       int3 
0x2fda96e: cc                       int3 
0x2fda96f: cc                       int3 
0x2fda970: 55                       push rbp
0x2fda971: 4889e5                   mov rbp, rsp
0x2fda974: 4157                     push r15
0x2fda976: 4156                     push r14
0x2fda978: 4154                     push r12
0x2fda97a: 53                       push rbx
0x2fda97b: 4883ec10                 sub rsp, 0x10
0x2fda97f: 488b1d82751506           mov rbx, qword ptr [rip + 0x6157582]
0x2fda986: 488b03                   mov rax, qword ptr [rbx]
0x2fda989: 488945d8                 mov qword ptr [rbp - 0x28], rax
0x2fda98d: 488b05d47d5406           mov rax, qword ptr [rip + 0x6547dd4]
0x2fda994: 4885c0                   test rax, rax
0x2fda997: 757c                     jne 0x2fdaa15
0x2fda999: 488d0500f8b3ff           lea rax, [rip - 0x4c0800]
0x2fda9a0: 4c8d1539ac7cfe           lea r10, [rip - 0x18353c7]
0x2fda9a7: 4c8d5dd0                 lea r11, [rbp - 0x30]
0x2fda9ab: 4c8d352e5702fd           lea r14, [rip - 0x2fda8d2]
0x2fda9b2: 4c8d3dc7580200           lea r15, [rip + 0x258c7]
0x2fda9b9: 4c8d25764d6504           lea r12, [rip + 0x4654d76] ; ['Game']
0x2fda9c0: 488d3dbb719e04           lea rdi, [rip + 0x49e71bb] ; ['/Script/Engine']
0x2fda9c7: 488d3552d19604           lea rsi, [rip + 0x496d152] ; ['GameSession']
0x2fda9ce: 488d15937d5406           lea rdx, [rip + 0x6547d93]
0x2fda9d5: 488d0db40803fd           lea rcx, [rip - 0x2fcf74c]
0x2fda9dc: 41b8d0020000             mov r8d, 0x2d0
0x2fda9e2: 41b908000000             mov r9d, 8
0x2fda9e8: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fda9ec: 488d05fd1a2cfd           lea rax, [rip - 0x2d3e503]
0x2fda9f3: 50                       push rax
0x2fda9f4: 4152                     push r10
0x2fda9f6: 4153                     push r11
0x2fda9f8: 4156                     push r14
0x2fda9fa: 4157                     push r15
0x2fda9fc: 4154                     push r12
0x2fda9fe: 6a00                     push 0
0x2fdaa00: 6804000010               push 0x10000004
0x2fdaa05: e8562e33fd               call 0x30d860
0x2fdaa0a: 4883c440                 add rsp, 0x40
0x2fdaa0e: 488b05537d5406           mov rax, qword ptr [rip + 0x6547d53]
0x2fdaa15: 488b0b                   mov rcx, qword ptr [rbx]
0x2fdaa18: 483b4dd8                 cmp rcx, qword ptr [rbp - 0x28]
0x2fdaa1c: 750d                     jne 0x2fdaa2b
