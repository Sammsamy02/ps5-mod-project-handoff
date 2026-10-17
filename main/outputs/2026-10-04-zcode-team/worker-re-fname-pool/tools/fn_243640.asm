0x643640  (file 0x243640)  55                       push rbp
0x643641  (file 0x243641)  4889e5                   mov rbp, rsp
0x643644  (file 0x243644)  4157                     push r15
0x643646  (file 0x243646)  4156                     push r14
0x643648  (file 0x243648)  4155                     push r13
0x64364a  (file 0x24364a)  4154                     push r12
0x64364c  (file 0x24364c)  53                       push rbx
0x64364d  (file 0x24364d)  50                       push rax
0x64364e  (file 0x24364e)  803d4b68120900           cmp byte ptr [rip + 0x912684b], 0  ; rip->0x9769ea0 (file 0x9369ea0)
0x643655  (file 0x243655)  448b37                   mov r14d, dword ptr [rdi]
0x643658  (file 0x243658)  4889f3                   mov rbx, rsi
0x64365b  (file 0x24365b)  48897dd0                 mov qword ptr [rbp - 0x30], rdi
0x64365f  (file 0x24365f)  7513                     jne 0x643674  ; -> 0x643674 (file 0x243674)
0x643661  (file 0x243661)  488d3d58681209           lea rdi, [rip + 0x9126858]  ; rip->0x9769ec0 (file 0x9369ec0)
0x643668  (file 0x243668)  e863afffff               call 0x63e5d0  ; -> 0x63e5d0 (file 0x23e5d0)
0x64366d  (file 0x24366d)  c6052c68120901           mov byte ptr [rip + 0x912682c], 1  ; rip->0x9769ea0 (file 0x9369ea0)
0x643674  (file 0x243674)  4489f0                   mov eax, r14d
0x643677  (file 0x243677)  488d0d42681209           lea rcx, [rip + 0x9126842]  ; rip->0x9769ec0 (file 0x9369ec0)
0x64367e  (file 0x24367e)  c1e810                   shr eax, 0x10
0x643681  (file 0x243681)  488b44c110               mov rax, qword ptr [rcx + rax*8 + 0x10]
0x643686  (file 0x243686)  410fb7ce                 movzx ecx, r14w
0x64368a  (file 0x24368a)  440fb73c48               movzx r15d, word ptr [rax + rcx*2]
0x64368f  (file 0x24368f)  4c8d2448                 lea r12, [rax + rcx*2]
0x643693  (file 0x243693)  488b4308                 mov rax, qword ptr [rbx + 8]
0x643697  (file 0x243697)  41c1ef06                 shr r15d, 6
0x64369b  (file 0x24369b)  4a8d0c78                 lea rcx, [rax + r15*2]
0x64369f  (file 0x24369f)  483b4b10                 cmp rcx, qword ptr [rbx + 0x10]
0x6436a3  (file 0x2436a3)  720f                     jb 0x6436b4  ; -> 0x6436b4 (file 0x2436b4)
0x6436a5  (file 0x2436a5)  4889df                   mov rdi, rbx
0x6436a8  (file 0x2436a8)  4c89fe                   mov rsi, r15
0x6436ab  (file 0x2436ab)  e85050fcff               call 0x608700  ; -> 0x608700 (file 0x208700)
0x6436b0  (file 0x2436b0)  488b4308                 mov rax, qword ptr [rbx + 8]
0x6436b4  (file 0x2436b4)  4c8b2b                   mov r13, qword ptr [rbx]
0x6436b7  (file 0x2436b7)  4a8d0c78                 lea rcx, [rax + r15*2]
0x6436bb  (file 0x2436bb)  48894b08                 mov qword ptr [rbx + 8], rcx
0x6436bf  (file 0x2436bf)  410fb71424               movzx edx, word ptr [r12]
0x6436c4  (file 0x2436c4)  4c29e8                   sub rax, r13
0x6436c7  (file 0x2436c7)  48d1e8                   shr rax, 1
0x6436ca  (file 0x2436ca)  4c63f0                   movsxd r14, eax
0x6436cd  (file 0x2436cd)  4f8d7c7500               lea r15, [r13 + r14*2]
0x6436d2  (file 0x2436d2)  f6c201                   test dl, 1
0x6436d5  (file 0x2436d5)  754f                     jne 0x643726  ; -> 0x643726 (file 0x243726)
0x6436d7  (file 0x2436d7)  c1ea06                   shr edx, 6
0x6436da  (file 0x2436da)  498d742402               lea rsi, [r12 + 2]
0x6436df  (file 0x2436df)  4c89ff                   mov rdi, r15
0x6436e2  (file 0x2436e2)  e869272307               call 0x7875e50  ; -> 0x7875e50 (file 0x7475e50)
0x6436e7  (file 0x2436e7)  410fb70c24               movzx ecx, word ptr [r12]
0x6436ec  (file 0x2436ec)  4883f940                 cmp rcx, 0x40
0x6436f0  (file 0x2436f0)  7249                     jb 0x64373b  ; -> 0x64373b (file 0x24373b)
0x6436f2  (file 0x2436f2)  89c8                     mov eax, ecx
0x6436f4  (file 0x2436f4)  c1e806                   shr eax, 6
0x6436f7  (file 0x2436f7)  6681f90002               cmp cx, 0x200
0x6436fc  (file 0x2436fc)  0f8398000000             jae 0x64379a  ; -> 0x64379a (file 0x24379a)
0x643702  (file 0x243702)  4889c7                   mov rdi, rax
0x643705  (file 0x243705)  4889f8                   mov rax, rdi
0x643708  (file 0x243708)  0f1f840000000000         nop dword ptr [rax + rax]
0x643710  (file 0x243710)  410fbe4c3fff             movsx ecx, byte ptr [r15 + rdi - 1]
0x643716  (file 0x243716)  48ffc8                   dec rax
0x643719  (file 0x243719)  6641894c7ffe             mov word ptr [r15 + rdi*2 - 2], cx
0x64371f  (file 0x24371f)  4889c7                   mov rdi, rax
0x643722  (file 0x243722)  75ec                     jne 0x643710  ; -> 0x643710 (file 0x243710)
0x643724  (file 0x243724)  eb15                     jmp 0x64373b  ; -> 0x64373b (file 0x24373b)
0x643726  (file 0x243726)  c1ea05                   shr edx, 5
0x643729  (file 0x243729)  4983c402                 add r12, 2
0x64372d  (file 0x24372d)  4c89ff                   mov rdi, r15
0x643730  (file 0x243730)  83e2fe                   and edx, 0xfffffffe
0x643733  (file 0x243733)  4c89e6                   mov rsi, r12
0x643736  (file 0x243736)  e815272307               call 0x7875e50  ; -> 0x7875e50 (file 0x7475e50)
0x64373b  (file 0x24373b)  488b45d0                 mov rax, qword ptr [rbp - 0x30]
0x64373f  (file 0x24373f)  448b7004                 mov r14d, dword ptr [rax + 4]
0x643743  (file 0x243743)  4585f6                   test r14d, r14d
0x643746  (file 0x243746)  7443                     je 0x64378b  ; -> 0x64378b (file 0x24378b)
0x643748  (file 0x243748)  488b4308                 mov rax, qword ptr [rbx + 8]
0x64374c  (file 0x24374c)  488d4802                 lea rcx, [rax + 2]
0x643750  (file 0x243750)  483b4b10                 cmp rcx, qword ptr [rbx + 0x10]
0x643754  (file 0x243754)  7211                     jb 0x643767  ; -> 0x643767 (file 0x243767)
0x643756  (file 0x243756)  be01000000               mov esi, 1
0x64375b  (file 0x24375b)  4889df                   mov rdi, rbx
0x64375e  (file 0x24375e)  e89d4ffcff               call 0x608700  ; -> 0x608700 (file 0x208700)
0x643763  (file 0x243763)  488b4308                 mov rax, qword ptr [rbx + 8]
0x643767  (file 0x243767)  41ffce                   dec r14d
0x64376a  (file 0x24376a)  488d4802                 lea rcx, [rax + 2]
0x64376e  (file 0x24376e)  488d35c1066e07           lea rsi, [rip + 0x76e06c1]  ; rip->0x7d23e36 (file 0x7923e36)
0x643775  (file 0x243775)  4889df                   mov rdi, rbx
0x643778  (file 0x243778)  4489f2                   mov edx, r14d
0x64377b  (file 0x24377b)  48894b08                 mov qword ptr [rbx + 8], rcx
0x64377f  (file 0x24377f)  66c7005f00               mov word ptr [rax], 0x5f
0x643784  (file 0x243784)  31c0                     xor eax, eax
0x643786  (file 0x243786)  e85555fcff               call 0x608ce0  ; -> 0x608ce0 (file 0x208ce0)
0x64378b  (file 0x24378b)  4883c408                 add rsp, 8
0x64378f  (file 0x24378f)  5b                       pop rbx
0x643790  (file 0x243790)  415c                     pop r12
0x643792  (file 0x243792)  415d                     pop r13
0x643794  (file 0x243794)  415e                     pop r14
0x643796  (file 0x243796)  415f                     pop r15
0x643798  (file 0x243798)  5d                       pop rbp
0x643799  (file 0x243799)  c3                       ret 
0x64379a  (file 0x24379a)  498d1406                 lea rdx, [r14 + rax]
0x64379e  (file 0x24379e)  8d3c00                   lea edi, [rax + rax]
0x6437a1  (file 0x2437a1)  498d7455fe               lea rsi, [r13 + rdx*2 - 2]
0x6437a6  (file 0x2437a6)  4989f0                   mov r8, rsi
0x6437a9  (file 0x2437a9)  4929f8                   sub r8, rdi
0x6437ac  (file 0x2437ac)  4983c002                 add r8, 2
0x6437b0  (file 0x2437b0)  4939f0                   cmp r8, rsi
0x6437b3  (file 0x2437b3)  0f8749ffffff             ja 0x643702  ; -> 0x643702 (file 0x243702)
0x6437b9  (file 0x2437b9)  4a8d7438ff               lea rsi, [rax + r15 - 1]
0x6437be  (file 0x2437be)  4939f7                   cmp r15, rsi
0x6437c1  (file 0x2437c1)  0f873bffffff             ja 0x643702  ; -> 0x643702 (file 0x243702)
0x6437c7  (file 0x2437c7)  498d3407                 lea rsi, [r15 + rax]
0x6437cb  (file 0x2437cb)  4939f7                   cmp r15, rsi
0x6437ce  (file 0x2437ce)  730e                     jae 0x6437de  ; -> 0x6437de (file 0x2437de)
0x6437d0  (file 0x2437d0)  498d745500               lea rsi, [r13 + rdx*2]
0x6437d5  (file 0x2437d5)  4939f7                   cmp r15, rsi
0x6437d8  (file 0x2437d8)  0f8224ffffff             jb 0x643702  ; -> 0x643702 (file 0x243702)
0x6437de  (file 0x2437de)  4d01f6                   add r14, r14
0x6437e1  (file 0x2437e1)  6681f90010               cmp cx, 0x1000
0x6437e6  (file 0x2437e6)  7304                     jae 0x6437ec  ; -> 0x6437ec (file 0x2437ec)
0x6437e8  (file 0x2437e8)  31f6                     xor esi, esi
0x6437ea  (file 0x2437ea)  eb7f                     jmp 0x64386b  ; -> 0x64386b (file 0x24386b)
0x6437ec  (file 0x2437ec)  4189c9                   mov r9d, ecx
0x6437ef  (file 0x2437ef)  4e8d0430                 lea r8, [rax + r14]
0x6437f3  (file 0x2437f3)  4c01f7                   add rdi, r14
0x6437f6  (file 0x2437f6)  89c6                     mov esi, eax
0x6437f8  (file 0x2437f8)  4531d2                   xor r10d, r10d
0x6437fb  (file 0x2437fb)  41c1e90c                 shr r9d, 0xc
0x6437ff  (file 0x2437ff)  4f8d4405f0               lea r8, [r13 + r8 - 0x10]
0x643804  (file 0x243804)  498d7c3de0               lea rdi, [r13 + rdi - 0x20]
0x643809  (file 0x243809)  83e6c0                   and esi, 0xffffffc0
0x64380c  (file 0x24380c)  41c1e106                 shl r9d, 6
0x643810  (file 0x243810)  49f7d9                   neg r9
0x643813  (file 0x243813)  666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x643820  (file 0x243820)  c4827d200410             vpmovsxbw ymm0, xmmword ptr [r8 + r10]
0x643826  (file 0x243826)  c4827d204c10f0           vpmovsxbw ymm1, xmmword ptr [r8 + r10 - 0x10]
0x64382d  (file 0x24382d)  c4827d205410e0           vpmovsxbw ymm2, xmmword ptr [r8 + r10 - 0x20]
0x643834  (file 0x243834)  c4827d205c10d0           vpmovsxbw ymm3, xmmword ptr [r8 + r10 - 0x30]
0x64383b  (file 0x24383b)  c4a17e7f0457             vmovdqu ymmword ptr [rdi + r10*2], ymm0
0x643841  (file 0x243841)  c4a17e7f4c57e0           vmovdqu ymmword ptr [rdi + r10*2 - 0x20], ymm1
0x643848  (file 0x243848)  c4a17e7f5457c0           vmovdqu ymmword ptr [rdi + r10*2 - 0x40], ymm2
0x64384f  (file 0x24384f)  c4a17e7f5c57a0           vmovdqu ymmword ptr [rdi + r10*2 - 0x60], ymm3
0x643856  (file 0x243856)  4983c2c0                 add r10, -0x40
0x64385a  (file 0x24385a)  4d39d1                   cmp r9, r10
0x64385d  (file 0x24385d)  75c1                     jne 0x643820  ; -> 0x643820 (file 0x243820)
0x64385f  (file 0x24385f)  39c6                     cmp esi, eax
0x643861  (file 0x243861)  0f84d4feffff             je 0x64373b  ; -> 0x64373b (file 0x24373b)
0x643867  (file 0x243867)  a838                     test al, 0x38
0x643869  (file 0x243869)  7457                     je 0x6438c2  ; -> 0x6438c2 (file 0x2438c2)
0x64386b  (file 0x24386b)  c1e909                   shr ecx, 9
0x64386e  (file 0x24386e)  4801d2                   add rdx, rdx
0x643871  (file 0x243871)  4901c6                   add r14, rax
0x643874  (file 0x243874)  4189c0                   mov r8d, eax
0x643877  (file 0x243877)  89c7                     mov edi, eax
0x643879  (file 0x243879)  48f7de                   neg rsi
0x64387c  (file 0x24387c)  c1e103                   shl ecx, 3
0x64387f  (file 0x24387f)  4a8d542af0               lea rdx, [rdx + r13 - 0x10]
0x643884  (file 0x243884)  4f8d4c35f8               lea r9, [r13 + r14 - 8]
0x643889  (file 0x243889)  4183e0f8                 and r8d, 0xfffffff8
0x64388d  (file 0x24388d)  83e707                   and edi, 7
0x643890  (file 0x243890)  48f7d9                   neg rcx
0x643893  (file 0x243893)  666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x6438a0  (file 0x2438a0)  c4c279200431             vpmovsxbw xmm0, qword ptr [r9 + rsi]
0x6438a6  (file 0x2438a6)  c5fa7f0472               vmovdqu xmmword ptr [rdx + rsi*2], xmm0
0x6438ab  (file 0x2438ab)  4883c6f8                 add rsi, -8
0x6438af  (file 0x2438af)  4839f1                   cmp rcx, rsi
0x6438b2  (file 0x2438b2)  75ec                     jne 0x6438a0  ; -> 0x6438a0 (file 0x2438a0)
0x6438b4  (file 0x2438b4)  4139c0                   cmp r8d, eax
0x6438b7  (file 0x2438b7)  0f8548feffff             jne 0x643705  ; -> 0x643705 (file 0x243705)
0x6438bd  (file 0x2438bd)  e979feffff               jmp 0x64373b  ; -> 0x64373b (file 0x24373b)
0x6438c2  (file 0x2438c2)  83e03f                   and eax, 0x3f
0x6438c5  (file 0x2438c5)  e938feffff               jmp 0x643702  ; -> 0x643702 (file 0x243702)
0x6438ca  (file 0x2438ca)  cc                       int3 
0x6438cb  (file 0x2438cb)  cc                       int3 
0x6438cc  (file 0x2438cc)  cc                       int3 
0x6438cd  (file 0x2438cd)  cc                       int3 
0x6438ce  (file 0x2438ce)  cc                       int3 
0x6438cf  (file 0x2438cf)  cc                       int3 
