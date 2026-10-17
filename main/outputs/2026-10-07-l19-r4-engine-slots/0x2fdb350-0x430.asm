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
0x2fdb4ce: 48898558ffffff           mov qword ptr [rbp - 0xa8], rax
0x2fdb4d5: 488d0514102cfd           lea rax, [rip - 0x2d3efec]
0x2fdb4dc: 50                       push rax
0x2fdb4dd: 4152                     push r10
0x2fdb4df: 4153                     push r11
0x2fdb4e1: 4156                     push r14
0x2fdb4e3: 4157                     push r15
0x2fdb4e5: 4154                     push r12
0x2fdb4e7: 6a00                     push 0
0x2fdb4e9: 6804000010               push 0x10000004
0x2fdb4ee: e86d2333fd               call 0x30d860
0x2fdb4f3: 4883c440                 add rsp, 0x40
0x2fdb4f7: 4c8b351a735406           mov r14, qword ptr [rip + 0x654731a]
0x2fdb4fe: 4d85f6                   test r14, r14
0x2fdb501: 7414                     je 0x2fdb517
0x2fdb503: 488d0552393c06           lea rax, [rip + 0x63c3952]
0x2fdb50a: 803800                   cmp byte ptr [rax], 0
0x2fdb50d: 7408                     je 0x2fdb517
0x2fdb50f: 4c89f7                   mov rdi, r14
0x2fdb512: e8a9cd38fd               call 0x3682c0
0x2fdb517: 4c89b3c0020000           mov qword ptr [rbx + 0x2c0], r14
0x2fdb51e: 4889df                   mov rdi, rbx
0x2fdb521: e87aedb3ff               call 0x2b1a2a0
0x2fdb526: 4c8bbbc0020000           mov r15, qword ptr [rbx + 0x2c0]
0x2fdb52d: 4989c6                   mov r14, rax
0x2fdb530: 4d85ff                   test r15, r15
0x2fdb533: 0f84b2000000             je 0x2fdb5eb
0x2fdb539: 488b05d8725406           mov rax, qword ptr [rip + 0x65472d8]
0x2fdb540: 4885c0                   test rax, rax
0x2fdb543: 0f8586000000             jne 0x2fdb5cf
0x2fdb549: 488d0550ecb3ff           lea rax, [rip - 0x4c13b0]
0x2fdb550: 4c8d1589a07cfe           lea r10, [rip - 0x1835f77]
0x2fdb557: 4c8d9d58ffffff           lea r11, [rbp - 0xa8]
0x2fdb55e: 4c8d257b4b02fd           lea r12, [rip - 0x2fdb485]
0x2fdb565: 4c8d2dc4810200           lea r13, [rip + 0x281c4]
0x2fdb56c: 488d3d0f669e04           lea rdi, [rip + 0x49e660f] ; ['/Script/Engine']
0x2fdb573: 488d35666d6e04           lea rsi, [rip + 0x46e6d66] ; ['GameStateBase']
0x2fdb57a: 488d1597725406           lea rdx, [rip + 0x6547297]
0x2fdb581: 488d0dc8800200           lea rcx, [rip + 0x280c8]
0x2fdb588: 41b800030000             mov r8d, 0x300
0x2fdb58e: 41b908000000             mov r9d, 8
0x2fdb594: 48898558ffffff           mov qword ptr [rbp - 0xa8], rax
0x2fdb59b: 488d054e0f2cfd           lea rax, [rip - 0x2d3f0b2]
0x2fdb5a2: 50                       push rax
0x2fdb5a3: 4152                     push r10
0x2fdb5a5: 4153                     push r11
0x2fdb5a7: 4154                     push r12
0x2fdb5a9: 4155                     push r13
0x2fdb5ab: 488d0584416504           lea rax, [rip + 0x4654184] ; ['Game']
0x2fdb5b2: 50                       push rax
0x2fdb5b3: 6a00                     push 0
0x2fdb5b5: 6804000010               push 0x10000004
0x2fdb5ba: e8a12233fd               call 0x30d860
0x2fdb5bf: 4883c440                 add rsp, 0x40
0x2fdb5c3: 488b054e725406           mov rax, qword ptr [rip + 0x654724e]
0x2fdb5ca: 4885c0                   test rax, rax
0x2fdb5cd: 741c                     je 0x2fdb5eb
0x2fdb5cf: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdb5d3: 413b4f38                 cmp ecx, dword ptr [r15 + 0x38]
0x2fdb5d7: 7f12                     jg 0x2fdb5eb
0x2fdb5d9: 498b5730                 mov rdx, qword ptr [r15 + 0x30]
0x2fdb5dd: 4883c030                 add rax, 0x30
0x2fdb5e1: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdb5e5: 0f8406010000             je 0x2fdb6f1
0x2fdb5eb: 31f6                     xor esi, esi
0x2fdb5ed: 4c8d8560ffffff           lea r8, [rbp - 0xa0]
0x2fdb5f4: 4c89f7                   mov rdi, r14
0x2fdb5f7: 31d2                     xor edx, edx
0x2fdb5f9: 31c9                     xor ecx, ecx
0x2fdb5fb: e880401a00               call 0x317f680
0x2fdb600: 4c8d2d55383c06           lea r13, [rip + 0x63c3855]
0x2fdb607: 4989c7                   mov r15, rax
0x2fdb60a: 4885c0                   test rax, rax
0x2fdb60d: 740f                     je 0x2fdb61e
0x2fdb60f: 41807d0000               cmp byte ptr [r13], 0
0x2fdb614: 7408                     je 0x2fdb61e
0x2fdb616: 4c89ff                   mov rdi, r15
0x2fdb619: e8a2cc38fd               call 0x3682c0
0x2fdb61e: 4c89f7                   mov rdi, r14
0x2fdb621: 4c89fe                   mov rsi, r15
0x2fdb624: 4c89bb08030000           mov qword ptr [rbx + 0x308], r15
0x2fdb62b: e830667c00               call 0x37a1c60
0x2fdb630: 4c8bbb08030000           mov r15, qword ptr [rbx + 0x308]
0x2fdb637: 4d85ff                   test r15, r15
0x2fdb63a: 741b                     je 0x2fdb657
0x2fdb63c: 4885db                   test rbx, rbx
0x2fdb63f: 740f                     je 0x2fdb650
0x2fdb641: 41807d0000               cmp byte ptr [r13], 0
0x2fdb646: 7408                     je 0x2fdb650
0x2fdb648: 4889df                   mov rdi, rbx
0x2fdb64b: e870cc38fd               call 0x3682c0
0x2fdb650: 49899fb0020000           mov qword ptr [r15 + 0x2b0], rbx
0x2fdb657: 4c89f7                   mov rdi, r14
0x2fdb65a: ba01000000               mov edx, 1
0x2fdb65f: 31f6                     xor esi, esi
0x2fdb661: 4531ff                   xor r15d, r15d
0x2fdb664: e8e71c7b00               call 0x378d350
0x2fdb669: 4c8ba050030000           mov r12, qword ptr [rax + 0x350]
0x2fdb670: 4d85e4                   test r12, r12
0x2fdb673: 742a                     je 0x2fdb69f
0x2fdb675: 4989c7                   mov r15, rax
0x2fdb678: e89381a9ff               call 0x2a73810
0x2fdb67d: 4885c0                   test rax, rax
0x2fdb680: 741a                     je 0x2fdb69c
0x2fdb682: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdb686: 413b4c2438               cmp ecx, dword ptr [r12 + 0x38]
0x2fdb68b: 7f0f                     jg 0x2fdb69c
0x2fdb68d: 498b542430               mov rdx, qword ptr [r12 + 0x30]
0x2fdb692: 4883c030                 add rax, 0x30
0x2fdb696: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdb69a: 7461                     je 0x2fdb6fd
0x2fdb69c: 4531ff                   xor r15d, r15d
0x2fdb69f: 4d897e58                 mov qword ptr [r14 + 0x58], r15
0x2fdb6a3: 4889df                   mov rdi, rbx
0x2fdb6a6: 488b03                   mov rax, qword ptr [rbx]
0x2fdb6a9: ff90b8070000             call qword ptr [rax + 0x7b8]
0x2fdb6af: 48837da000               cmp qword ptr [rbp - 0x60], 0
0x2fdb6b4: 7415                     je 0x2fdb6cb
0x2fdb6b6: 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x2fdb6ba: 488d7db0                 lea rdi, [rbp - 0x50]
0x2fdb6be: 4885c0                   test rax, rax
0x2fdb6c1: 480f45f8                 cmovne rdi, rax
0x2fdb6c5: 488b07                   mov rax, qword ptr [rdi]
0x2fdb6c8: ff5010                   call qword ptr [rax + 0x10]
0x2fdb6cb: 488b0536681506           mov rax, qword ptr [rip + 0x6156836]
0x2fdb6d2: 488b00                   mov rax, qword ptr [rax]
0x2fdb6d5: 483b45d0                 cmp rax, qword ptr [rbp - 0x30]
0x2fdb6d9: 0f8590000000             jne 0x2fdb76f
0x2fdb6df: 4881c488000000           add rsp, 0x88
0x2fdb6e6: 5b                       pop rbx
0x2fdb6e7: 415c                     pop r12
0x2fdb6e9: 415d                     pop r13
0x2fdb6eb: 415e                     pop r14
0x2fdb6ed: 415f                     pop r15
0x2fdb6ef: 5d                       pop rbp
0x2fdb6f0: c3                       ret 
0x2fdb6f1: 488bb3c0020000           mov rsi, qword ptr [rbx + 0x2c0]
0x2fdb6f8: e9f0feffff               jmp 0x2fdb5ed
0x2fdb6fd: 4d8ba750030000           mov r12, qword ptr [r15 + 0x350]
0x2fdb704: 4d85e4                   test r12, r12
0x2fdb707: 7493                     je 0x2fdb69c
0x2fdb709: e80281a9ff               call 0x2a73810
0x2fdb70e: 4885c0                   test rax, rax
0x2fdb711: 741a                     je 0x2fdb72d
0x2fdb713: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdb717: 413b4c2438               cmp ecx, dword ptr [r12 + 0x38]
0x2fdb71c: 7f0f                     jg 0x2fdb72d
0x2fdb71e: 498b542430               mov rdx, qword ptr [r12 + 0x30]
0x2fdb723: 4883c030                 add rax, 0x30
0x2fdb727: 483904ca                 cmp qword ptr [rdx + rcx*8], rax
0x2fdb72b: 7439                     je 0x2fdb766
0x2fdb72d: 31f6                     xor esi, esi
0x2fdb72f: 4c8d8560ffffff           lea r8, [rbp - 0xa0]
0x2fdb736: 4c89f7                   mov rdi, r14
0x2fdb739: 31d2                     xor edx, edx
0x2fdb73b: 31c9                     xor ecx, ecx
0x2fdb73d: e83e3f1a00               call 0x317f680
0x2fdb742: 4989c7                   mov r15, rax
0x2fdb745: 4885c0                   test rax, rax
0x2fdb748: 0f8451ffffff             je 0x2fdb69f
0x2fdb74e: 41807d0000               cmp byte ptr [r13], 0
0x2fdb753: 0f8446ffffff             je 0x2fdb69f
0x2fdb759: 4c89ff                   mov rdi, r15
0x2fdb75c: e85fcb38fd               call 0x3682c0
0x2fdb761: e939ffffff               jmp 0x2fdb69f
0x2fdb766: 498bb750030000           mov rsi, qword ptr [r15 + 0x350]
0x2fdb76d: ebc0                     jmp 0x2fdb72f
0x2fdb76f: e8ec664904               call 0x7471e60
0x2fdb774: 0f0b                     ud2 
0x2fdb776: cc                       int3 
0x2fdb777: cc                       int3 
0x2fdb778: cc                       int3 
0x2fdb779: cc                       int3 
0x2fdb77a: cc                       int3 
0x2fdb77b: cc                       int3 
0x2fdb77c: cc                       int3 
0x2fdb77d: cc                       int3 
0x2fdb77e: cc                       int3 
0x2fdb77f: cc                       int3 
