0x2fda7b0: 8b1d52771506             mov ebx, dword ptr [rip + 0x6157752]
0x2fda7b6: 488b03                   mov rax, qword ptr [rbx]
0x2fda7b9: 488945d8                 mov qword ptr [rbp - 0x28], rax
0x2fda7bd: 488b3d5c7a5406           mov rdi, qword ptr [rip + 0x6547a5c]
0x2fda7c4: 4885ff                   test rdi, rdi
0x2fda7c7: 757c                     jne 0x2fda845
0x2fda7c9: 488d05d0f9b3ff           lea rax, [rip - 0x4c0630]
0x2fda7d0: 488d3dab739e04           lea rdi, [rip + 0x49e73ab] ; ['/Script/Engine']
0x2fda7d7: 4c8d1502ae7cfe           lea r10, [rip - 0x18351fe]
0x2fda7de: 4c8d5dd0                 lea r11, [rbp - 0x30]
0x2fda7e2: 4c8d35f75802fd           lea r14, [rip - 0x2fda709]
0x2fda7e9: 4c8d3d90000000           lea r15, [rip + 0x90]
0x2fda7f0: 4c8d253f4f6504           lea r12, [rip + 0x4654f3f] ; ['Game']
0x2fda7f7: 488d351c018604           lea rsi, [rip + 0x486011c] ; ['GameModeBase']
0x2fda7fe: 488d151b7a5406           lea rdx, [rip + 0x6547a1b]
0x2fda805: 488d0d94ffffff           lea rcx, [rip - 0x6c]
0x2fda80c: 41b840030000             mov r8d, 0x340
0x2fda812: 41b908000000             mov r9d, 8
0x2fda818: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fda81c: 488d05cd1c2cfd           lea rax, [rip - 0x2d3e333]
0x2fda823: 50                       push rax
0x2fda824: 4152                     push r10
0x2fda826: 4153                     push r11
0x2fda828: 4156                     push r14
0x2fda82a: 4157                     push r15
0x2fda82c: 4154                     push r12
0x2fda82e: 6a00                     push 0
0x2fda830: 680c000010               push 0x1000000c
0x2fda835: e8263033fd               call 0x30d860
0x2fda83a: 4883c440                 add rsp, 0x40
0x2fda83e: 488b3ddb795406           mov rdi, qword ptr [rip + 0x65479db]
0x2fda845: 488d3514ef4a05           lea rsi, [rip + 0x54aef14]
0x2fda84c: ba18000000               mov edx, 0x18
0x2fda851: e8daf733fd               call 0x31a030
0x2fda856: 488b03                   mov rax, qword ptr [rbx]
0x2fda859: 483b45d8                 cmp rax, qword ptr [rbp - 0x28]
0x2fda85d: 750d                     jne 0x2fda86c
0x2fda85f: 4883c410                 add rsp, 0x10
0x2fda863: 5b                       pop rbx
0x2fda864: 415c                     pop r12
0x2fda866: 415e                     pop r14
0x2fda868: 415f                     pop r15
0x2fda86a: 5d                       pop rbp
0x2fda86b: c3                       ret 
0x2fda86c: e8ef754904               call 0x7471e60
0x2fda871: 0f0b                     ud2 
0x2fda873: cc                       int3 
0x2fda874: cc                       int3 
0x2fda875: cc                       int3 
0x2fda876: cc                       int3 
0x2fda877: cc                       int3 
0x2fda878: cc                       int3 
0x2fda879: cc                       int3 
0x2fda87a: cc                       int3 
0x2fda87b: cc                       int3 
0x2fda87c: cc                       int3 
0x2fda87d: cc                       int3 
0x2fda87e: cc                       int3 
0x2fda87f: cc                       int3 
0x2fda880: 55                       push rbp
0x2fda881: 4889e5                   mov rbp, rsp
0x2fda884: 4889fe                   mov rsi, rdi
0x2fda887: 488b3f                   mov rdi, qword ptr [rdi]
0x2fda88a: e891030000               call 0x2fdac20
0x2fda88f: 5d                       pop rbp
0x2fda890: c3                       ret 
0x2fda891: cc                       int3 
0x2fda892: cc                       int3 
0x2fda893: cc                       int3 
0x2fda894: cc                       int3 
0x2fda895: cc                       int3 
0x2fda896: cc                       int3 
0x2fda897: cc                       int3 
0x2fda898: cc                       int3 
0x2fda899: cc                       int3 
0x2fda89a: cc                       int3 
0x2fda89b: cc                       int3 
0x2fda89c: cc                       int3 
0x2fda89d: cc                       int3 
0x2fda89e: cc                       int3 
0x2fda89f: cc                       int3 
0x2fda8a0: 55                       push rbp
0x2fda8a1: 4889e5                   mov rbp, rsp
0x2fda8a4: 4157                     push r15
0x2fda8a6: 4156                     push r14
0x2fda8a8: 4154                     push r12
0x2fda8aa: 53                       push rbx
0x2fda8ab: 4883ec10                 sub rsp, 0x10
0x2fda8af: 488b1d52761506           mov rbx, qword ptr [rip + 0x6157652]
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
