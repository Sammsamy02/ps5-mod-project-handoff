0x890fb0  (f 0x490fb0)  55                     push rbp
0x890fb1  (f 0x490fb1)  4889e5                 mov rbp, rsp
0x890fb4  (f 0x490fb4)  4157                   push r15
0x890fb6  (f 0x490fb6)  4156                   push r14
0x890fb8  (f 0x490fb8)  4155                   push r13
0x890fba  (f 0x490fba)  4154                   push r12
0x890fbc  (f 0x490fbc)  53                     push rbx
0x890fbd  (f 0x490fbd)  4881ecb8020000         sub rsp, 0x2b8
0x890fc4  (f 0x490fc4)  488b053d0fca08         mov rax, qword ptr [rip + 0x8ca0f3d]  ; rip->0x9531f08 (file 0x9131f08)
0x890fcb  (f 0x490fcb)  44898d38fdffff         mov dword ptr [rbp - 0x2c8], r9d
0x890fd2  (f 0x490fd2)  4989fc                 mov r12, rdi
0x890fd5  (f 0x490fd5)  89cf                   mov edi, ecx
0x890fd7  (f 0x490fd7)  4589c6                 mov r14d, r8d
0x890fda  (f 0x490fda)  4189cf                 mov r15d, ecx
0x890fdd  (f 0x490fdd)  4189cd                 mov r13d, ecx
0x890fe0  (f 0x490fe0)  81e710000004           and edi, 0x4000010
0x890fe6  (f 0x490fe6)  488b00                 mov rax, qword ptr [rax]
0x890fe9  (f 0x490fe9)  488945d0               mov qword ptr [rbp - 0x30], rax
0x890fed  (f 0x490fed)  488995a8fdffff         mov qword ptr [rbp - 0x258], rdx
0x890ff4  (f 0x490ff4)  89bd6cfdffff           mov dword ptr [rbp - 0x294], edi
0x890ffa  (f 0x490ffa)  83ff10                 cmp edi, 0x10
0x890ffd  (f 0x490ffd)  752e                   jne 0x89102d
0x890fff  (f 0x490fff)  488d3d097aec08         lea rdi, [rip + 0x8ec7a09]  ; rip->0x9758a0f (file 0x9358a0f)
0x891006  (f 0x491006)  4889f3                 mov rbx, rsi
0x891009  (f 0x491009)  e8d2c7d1ff             call 0x5ad7e0
0x89100e  (f 0x49100e)  4c89e7                 mov rdi, r12
0x891011  (f 0x491011)  e87a16e9ff             call 0x722690
0x891016  (f 0x491016)  4589fd                 mov r13d, r15d
0x891019  (f 0x491019)  4889de                 mov rsi, rbx
0x89101c  (f 0x49101c)  4889c2                 mov rdx, rax
0x89101f  (f 0x49101f)  488985a8fdffff         mov qword ptr [rbp - 0x258], rax
0x891026  (f 0x491026)  4181e5ffcffffb         and r13d, 0xfbffcfff
0x89102d  (f 0x49102d)  4489bd34fdffff         mov dword ptr [rbp - 0x2cc], r15d
0x891034  (f 0x491034)  4c8b7d18               mov r15, qword ptr [rbp + 0x18]
0x891038  (f 0x491038)  4c89a520fdffff         mov qword ptr [rbp - 0x2e0], r12
0x89103f  (f 0x49103f)  4889b560fdffff         mov qword ptr [rbp - 0x2a0], rsi
0x891046  (f 0x491046)  4885d2                 test rdx, rdx
0x891049  (f 0x491049)  0f84c5000000           je 0x891114
0x89104f  (f 0x49104f)  b901000000             mov ecx, 1
0x891054  (f 0x491054)  31ff                   xor edi, edi
0x891056  (f 0x491056)  4531c0                 xor r8d, r8d
0x891059  (f 0x491059)  4531c9                 xor r9d, r9d
0x89105c  (f 0x49105c)  e81fc70100             call 0x8ad780
0x891061  (f 0x491061)  4885c0                 test rax, rax
0x891064  (f 0x491064)  0f84c0000000           je 0x89112a
0x89106a  (f 0x49106a)  4889c3                 mov rbx, rax
0x89106d  (f 0x49106d)  4489ad40fdffff         mov dword ptr [rbp - 0x2c0], r13d
0x891074  (f 0x491074)  4d85e4                 test r12, r12
0x891077  (f 0x491077)  0f84cb030000           je 0x891448
0x89107d  (f 0x49107d)  488b4310               mov rax, qword ptr [rbx + 0x10]
0x891081  (f 0x491081)  49634c2438             movsxd rcx, dword ptr [r12 + 0x38]
0x891086  (f 0x491086)  3b4838                 cmp ecx, dword ptr [rax + 0x38]
0x891089  (f 0x491089)  0f8fb9030000           jg 0x891448
0x89108f  (f 0x49108f)  488b4030               mov rax, qword ptr [rax + 0x30]
0x891093  (f 0x491093)  498d542430             lea rdx, [r12 + 0x30]
0x891098  (f 0x491098)  483914c8               cmp qword ptr [rax + rcx*8], rdx
0x89109c  (f 0x49109c)  0f85a6030000           jne 0x891448
0x8910a2  (f 0x4910a2)  418b442458             mov eax, dword ptr [r12 + 0x58]
0x8910a7  (f 0x4910a7)  4889df                 mov rdi, rbx
0x8910aa  (f 0x4910aa)  898528fdffff           mov dword ptr [rbp - 0x2d8], eax
0x8910b0  (f 0x4910b0)  488b03                 mov rax, qword ptr [rbx]
0x8910b3  (f 0x4910b3)  ff90a8010000           call qword ptr [rax + 0x1a8]
0x8910b9  (f 0x4910b9)  4989c5                 mov r13, rax
0x8910bc  (f 0x4910bc)  66666664488b042500000000 mov rax, qword ptr fs:[0]
0x8910c8  (f 0x4910c8)  4889df                 mov rdi, rbx
0x8910cb  (f 0x4910cb)  4c89a840f9ffff         mov qword ptr [rax - 0x6c0], r13
0x8910d2  (f 0x4910d2)  e8293c0200             call 0x8b4d00
0x8910d7  (f 0x4910d7)  4889df                 mov rdi, rbx
0x8910da  (f 0x4910da)  48898548fdffff         mov qword ptr [rbp - 0x2b8], rax
0x8910e1  (f 0x4910e1)  e89a3c0200             call 0x8b4d80
0x8910e6  (f 0x4910e6)  898558fdffff           mov dword ptr [rbp - 0x2a8], eax
0x8910ec  (f 0x4910ec)  8b430c                 mov eax, dword ptr [rbx + 0xc]
0x8910ef  (f 0x4910ef)  488d151a90f208         lea rdx, [rip + 0x8f2901a]  ; rip->0x97ba110 (file 0x93ba110)
0x8910f6  (f 0x4910f6)  394224                 cmp dword ptr [rdx + 0x24], eax
0x8910f9  (f 0x4910f9)  7e7b                   jle 0x891176
0x8910fb  (f 0x4910fb)  488b5210               mov rdx, qword ptr [rdx + 0x10]
0x8910ff  (f 0x4910ff)  89c1                   mov ecx, eax
0x891101  (f 0x491101)  0fb7c0                 movzx eax, ax
0x891104  (f 0x491104)  488d0440               lea rax, [rax + rax*2]
0x891108  (f 0x491108)  c1e910                 shr ecx, 0x10
0x89110b  (f 0x49110b)  c1e003                 shl eax, 3
0x89110e  (f 0x49110e)  480304ca               add rax, qword ptr [rdx + rcx*8]
0x891112  (f 0x491112)  eb64                   jmp 0x891178
0x891114  (f 0x491114)  4889f7                 mov rdi, rsi
0x891117  (f 0x491117)  4c89e6                 mov rsi, r12
0x89111a  (f 0x49111a)  31d2                   xor edx, edx
0x89111c  (f 0x49111c)  31c9                   xor ecx, ecx
0x89111e  (f 0x49111e)  e88da9ffff             call 0x88bab0
0x891123  (f 0x491123)  488985a8fdffff         mov qword ptr [rbp - 0x258], rax
0x89112a  (f 0x49112a)  418b4c2458             mov ecx, dword ptr [r12 + 0x58]
0x89112f  (f 0x49112f)  418b44245c             mov eax, dword ptr [r12 + 0x5c]
0x891134  (f 0x491134)  4489b530fdffff         mov dword ptr [rbp - 0x2d0], r14d
0x89113b  (f 0x49113b)  898d28fdffff           mov dword ptr [rbp - 0x2d8], ecx
0x891141  (f 0x491141)  4863f9                 movsxd rdi, ecx
0x891144  (f 0x491144)  6683f805               cmp ax, 5
0x891148  (f 0x491148)  b904000000             mov ecx, 4
0x89114d  (f 0x49114d)  0f4dc8                 cmovge ecx, eax
0x891150  (f 0x491150)  0fb7f1                 movzx esi, cx
0x891153  (f 0x491153)  488d0dc6bae408         lea rcx, [rip + 0x8e4bac6]  ; rip->0x96dcc20 (file 0x92dcc20)
0x89115a  (f 0x49115a)  803901                 cmp byte ptr [rcx], 1
0x89115d  (f 0x49115d)  750d                   jne 0x89116c
0x89115f  (f 0x49115f)  803d0c8af20800         cmp byte ptr [rip + 0x8f28a0c], 0  ; rip->0x97b9b72 (file 0x93b9b72)
0x891166  (f 0x491166)  0f84a3000000           je 0x89120f
0x89116c  (f 0x49116c)  e8af68c2ff             call 0x4b7a20
0x891171  (f 0x491171)  e9b8000000             jmp 0x89122e
0x891176  (f 0x491176)  31c0                   xor eax, eax
0x891178  (f 0x491178)  83bd6cfdffff10         cmp dword ptr [rbp - 0x294], 0x10
0x89117f  (f 0x49117f)  448bad40fdffff         mov r13d, dword ptr [rbp - 0x2c0]
0x891186  (f 0x491186)  8b4008                 mov eax, dword ptr [rax + 8]
0x891189  (f 0x491189)  0f95c1                 setne cl
0x89118c  (f 0x49118c)  2500001042             and eax, 0x42100000
0x891191  (f 0x491191)  4109c6                 or r14d, eax
0x891194  (f 0x491194)  41f7c500000004         test r13d, 0x4000000
0x89119b  (f 0x49119b)  0f94c0                 sete al
0x89119e  (f 0x49119e)  84c1                   test cl, al
0x8911a0  (f 0x4911a0)  7550                   jne 0x8911f2
0x8911a2  (f 0x4911a2)  4889df                 mov rdi, rbx
0x8911a5  (f 0x4911a5)  4489ee                 mov esi, r13d
0x8911a8  (f 0x4911a8)  e8634de3ff             call 0x6c5f10
0x8911ad  (f 0x4911ad)  8b430c                 mov eax, dword ptr [rbx + 0xc]
0x8911b0  (f 0x4911b0)  488d15598ff208         lea rdx, [rip + 0x8f28f59]  ; rip->0x97ba110 (file 0x93ba110)
0x8911b7  (f 0x4911b7)  394224                 cmp dword ptr [rdx + 0x24], eax
0x8911ba  (f 0x4911ba)  0f8ea3000000           jle 0x891263
0x8911c0  (f 0x4911c0)  488b5210               mov rdx, qword ptr [rdx + 0x10]
0x8911c4  (f 0x4911c4)  89c1                   mov ecx, eax
0x8911c6  (f 0x4911c6)  0fb7c0                 movzx eax, ax
0x8911c9  (f 0x4911c9)  488d3c40               lea rdi, [rax + rax*2]
0x8911cd  (f 0x4911cd)  c1e910                 shr ecx, 0x10
0x8911d0  (f 0x4911d0)  c1e703                 shl edi, 3
0x8911d3  (f 0x4911d3)  48033cca               add rdi, qword ptr [rdx + rcx*8]
0x8911d7  (f 0x4911d7)  4489f6                 mov esi, r14d
0x8911da  (f 0x4911da)  81e6ff3ffecf           and esi, 0xcffe3fff
0x8911e0  (f 0x4911e0)  41f7c60000504e         test r14d, 0x4e500000
0x8911e7  (f 0x4911e7)  0f858a000000           jne 0x891277
0x8911ed  (f 0x4911ed)  e999000000             jmp 0x89128b
0x8911f2  (f 0x4911f2)  4883bd60fdffff00       cmp qword ptr [rbp - 0x2a0], 0
0x8911fa  (f 0x4911fa)  0f84ab000000           je 0x8912ab
0x891200  (f 0x491200)  488b8560fdffff         mov rax, qword ptr [rbp - 0x2a0]
0x891207  (f 0x491207)  8b4008                 mov eax, dword ptr [rax + 8]
0x89120a  (f 0x49120a)  e99c000000             jmp 0x8912ab
0x89120f  (f 0x49120f)  4589ee                 mov r14d, r13d
0x891212  (f 0x491212)  4889fb                 mov rbx, rdi
0x891215  (f 0x491215)  4189f5                 mov r13d, esi
0x891218  (f 0x491218)  e8132aceff             call 0x573c30
0x89121d  (f 0x49121d)  4489ea                 mov edx, r13d
0x891220  (f 0x491220)  4889c7                 mov rdi, rax
0x891223  (f 0x491223)  4889de                 mov rsi, rbx
0x891226  (f 0x491226)  4589f5                 mov r13d, r14d
0x891229  (f 0x491229)  e81229ceff             call 0x573b40
0x89122e  (f 0x49122e)  4889c3                 mov rbx, rax
0x891231  (f 0x491231)  b8ffffffff             mov eax, 0xffffffff
0x891236  (f 0x491236)  31c9                   xor ecx, ecx
0x891238  (f 0x491238)  c78558fdffffffffffff   mov dword ptr [rbp - 0x2a8], 0xffffffff
0x891242  (f 0x491242)  c78554fdffff00000000   mov dword ptr [rbp - 0x2ac], 0
0x89124c  (f 0x49124c)  48c78548fdffff00000000 mov qword ptr [rbp - 0x2b8], 0
0x891257  (f 0x491257)  48898538fdffff         mov qword ptr [rbp - 0x2c8], rax
0x89125e  (f 0x49125e)  e962040000             jmp 0x8916c5
0x891263  (f 0x491263)  31ff                   xor edi, edi
0x891265  (f 0x491265)  4489f6                 mov esi, r14d
0x891268  (f 0x491268)  81e6ff3ffecf           and esi, 0xcffe3fff
0x89126e  (f 0x49126e)  41f7c60000504e         test r14d, 0x4e500000
0x891275  (f 0x491275)  7414                   je 0x89128b
0x891277  (f 0x491277)  e8f471edff             call 0x768470
0x89127c  (f 0x49127c)  eb17                   jmp 0x891295
0x891280  (f 0x491280)  89c1                   mov ecx, eax
0x891282  (f 0x491282)  09f1                   or ecx, esi
0x891284  (f 0x491284)  f00fb14f08             lock cmpxchg dword ptr [rdi + 8], ecx
0x891289  (f 0x491289)  740a                   je 0x891295
0x89128b  (f 0x49128b)  8b4708                 mov eax, dword ptr [rdi + 8]
0x89128e  (f 0x49128e)  c4e278f2ce             andn ecx, eax, esi
0x891293  (f 0x491293)  75eb                   jne 0x891280
0x891295  (f 0x491295)  4889df                 mov rdi, rbx
0x891298  (f 0x491298)  be00300000             mov esi, 0x3000
0x89129d  (f 0x49129d)  e88e4ce3ff             call 0x6c5f30
0x8912a2  (f 0x4912a2)  83bd6cfdffff10         cmp dword ptr [rbp - 0x294], 0x10
0x8912a9  (f 0x4912a9)  7461                   je 0x89130c
0x8912ab  (f 0x4912ab)  80bd38fdffff00         cmp byte ptr [rbp - 0x2c8], 0
0x8912b2  (f 0x4912b2)  0f84a4000000           je 0x89135c
0x8912b8  (f 0x4912b8)  8b4308                 mov eax, dword ptr [rbx + 8]
0x8912bb  (f 0x4912bb)  a810                   test al, 0x10
0x8912bd  (f 0x4912bd)  0f8599000000           jne 0x89135c
0x8912c3  (f 0x4912c3)  488b4320               mov rax, qword ptr [rbx + 0x20]
0x8912c7  (f 0x4912c7)  4885c0                 test rax, rax
0x8912ca  (f 0x4912ca)  0f848c000000           je 0x89135c
0x8912d0  (f 0x4912d0)  8b4008                 mov eax, dword ptr [rax + 8]
0x8912d3  (f 0x4912d3)  a810                   test al, 0x10
0x8912d5  (f 0x4912d5)  7517                   jne 0x8912ee
0x8912d7  (f 0x4912d7)  4889df                 mov rdi, rbx
0x8912da  (f 0x4912da)  31f6                   xor esi, esi
0x8912dc  (f 0x4912dc)  e8cf02ffff             call 0x8815b0
0x8912e1  (f 0x4912e1)  488b4b10               mov rcx, qword ptr [rbx + 0x10]
0x8912e5  (f 0x4912e5)  483b8110010000         cmp rax, qword ptr [rcx + 0x110]
0x8912ec  (f 0x4912ec)  746e                   je 0x89135c
0x8912ee  (f 0x4912ee)  4489e8                 mov eax, r13d
0x8912f1  (f 0x4912f1)  83c840                 or eax, 0x40
0x8912f4  (f 0x4912f4)  41f68424d400000008     test byte ptr [r12 + 0xd4], 8
0x8912fd  (f 0x4912fd)  410f44c5               cmove eax, r13d
0x891301  (f 0x491301)  f68534fdffff20         test byte ptr [rbp - 0x2cc], 0x20
0x891308  (f 0x491308)  440f44e8               cmove r13d, eax
0x89130c  (f 0x49130c)  4889df                 mov rdi, rbx
0x89130f  (f 0x49130f)  4489ee                 mov esi, r13d
0x891312  (f 0x491312)  e8f94be3ff             call 0x6c5f10
0x891317  (f 0x491317)  8b430c                 mov eax, dword ptr [rbx + 0xc]
0x89131a  (f 0x49131a)  488d15ef8df208         lea rdx, [rip + 0x8f28def]  ; rip->0x97ba110 (file 0x93ba110)
0x891321  (f 0x491321)  394224                 cmp dword ptr [rdx + 0x24], eax
0x891324  (f 0x491324)  0f8eb9000000           jle 0x8913e3
0x89132a  (f 0x49132a)  488b5210               mov rdx, qword ptr [rdx + 0x10]
0x89132e  (f 0x49132e)  89c1                   mov ecx, eax
0x891330  (f 0x491330)  0fb7c0                 movzx eax, ax
0x891333  (f 0x491333)  488d3c40               lea rdi, [rax + rax*2]
0x891337  (f 0x491337)  c1e910                 shr ecx, 0x10
0x89133a  (f 0x49133a)  c1e703                 shl edi, 3
0x89133d  (f 0x49133d)  48033cca               add rdi, qword ptr [rdx + rcx*8]
0x891341  (f 0x491341)  4489f6                 mov esi, r14d
0x891344  (f 0x491344)  81e6ff3ffecf           and esi, 0xcffe3fff
0x89134a  (f 0x49134a)  41f7c60000504e         test r14d, 0x4e500000
0x891351  (f 0x491351)  0f85a0000000           jne 0x8913f7
0x891357  (f 0x491357)  e9c6000000             jmp 0x891422
0x89135c  (f 0x49135c)  4c8b6530               mov r12, qword ptr [rbp + 0x30]
0x891360  (f 0x491360)  448b7b0c               mov r15d, dword ptr [rbx + 0xc]
0x891364  (f 0x491364)  4c8d2da58df208         lea r13, [rip + 0x8f28da5]  ; rip->0x97ba110 (file 0x93ba110)
0x89136b  (f 0x49136b)  45397d24               cmp dword ptr [r13 + 0x24], r15d
0x89136f  (f 0x49136f)  7e1b                   jle 0x89138c
0x891371  (f 0x491371)  498b5510               mov rdx, qword ptr [r13 + 0x10]
0x891375  (f 0x491375)  410fb7c7               movzx eax, r15w
0x891379  (f 0x491379)  4489f9                 mov ecx, r15d
0x89137c  (f 0x49137c)  488d0440               lea rax, [rax + rax*2]
0x891380  (f 0x491380)  c1e910                 shr ecx, 0x10
0x891383  (f 0x491383)  c1e003                 shl eax, 3
0x891386  (f 0x491386)  480304ca               add rax, qword ptr [rdx + rcx*8]
0x89138a  (f 0x49138a)  eb02                   jmp 0x89138e
0x89138c  (f 0x49138c)  31c0                   xor eax, eax
0x89138e  (f 0x49138e)  8b4010                 mov eax, dword ptr [rax + 0x10]
0x891391  (f 0x491391)  898554fdffff           mov dword ptr [rbp - 0x2ac], eax
0x891397  (f 0x491397)  e8f47cc2ff             call 0x4b9090
0x89139c  (f 0x49139c)  3401                   xor al, 1
0x89139e  (f 0x49139e)  4d85e4                 test r12, r12
0x8913a1  (f 0x4913a1)  0f94c1                 sete cl
0x8913a4  (f 0x4913a4)  08c1                   or cl, al
0x8913a6  (f 0x4913a6)  0f8585010000           jne 0x891531
0x8913ac  (f 0x4913ac)  8b430c                 mov eax, dword ptr [rbx + 0xc]
0x8913af  (f 0x4913af)  41394524               cmp dword ptr [r13 + 0x24], eax
0x8913b3  (f 0x4913b3)  0f8e48010000           jle 0x891501
0x8913b9  (f 0x4913b9)  498b5510               mov rdx, qword ptr [r13 + 0x10]
0x8913bd  (f 0x4913bd)  89c1                   mov ecx, eax
0x8913bf  (f 0x4913bf)  0fb7c0                 movzx eax, ax
0x8913c2  (f 0x4913c2)  488d0440               lea rax, [rax + rax*2]
0x8913c6  (f 0x4913c6)  c1e910                 shr ecx, 0x10
0x8913c9  (f 0x4913c9)  c1e003                 shl eax, 3
0x8913cc  (f 0x4913cc)  480304ca               add rax, qword ptr [rdx + rcx*8]
0x8913d0  (f 0x4913d0)  8b4008                 mov eax, dword ptr [rax + 8]
0x8913d3  (f 0x4913d3)  a900004008             test eax, 0x8400000
0x8913d8  (f 0x4913d8)  0f852f010000           jne 0x89150d
0x8913de  (f 0x4913de)  e94e010000             jmp 0x891531
0x8913e3  (f 0x4913e3)  31ff                   xor edi, edi
0x8913e5  (f 0x4913e5)  4489f6                 mov esi, r14d
0x8913e8  (f 0x4913e8)  81e6ff3ffecf           and esi, 0xcffe3fff
0x8913ee  (f 0x4913ee)  41f7c60000504e         test r14d, 0x4e500000
0x8913f5  (f 0x4913f5)  742b                   je 0x891422
0x8913f7  (f 0x4913f7)  4989dc                 mov r12, rbx
0x8913fa  (f 0x4913fa)  e87170edff             call 0x768470
0x8913ff  (f 0x4913ff)  eb2e                   jmp 0x89142f
0x891410  (f 0x491410)  89c1                   mov ecx, eax
0x891412  (f 0x491412)  4531f6                 xor r14d, r14d
0x891415  (f 0x491415)  09f1                   or ecx, esi
0x891417  (f 0x491417)  f00fb14f08             lock cmpxchg dword ptr [rdi + 8], ecx
0x89141c  (f 0x49141c)  0f84ee040000           je 0x891910
0x891422  (f 0x491422)  8b4708                 mov eax, dword ptr [rdi + 8]
0x891425  (f 0x491425)  c4e278f2ce             andn ecx, eax, esi
0x89142a  (f 0x49142a)  75e4                   jne 0x891410
0x89142c  (f 0x49142c)  4989dc                 mov r12, rbx
0x89142f  (f 0x49142f)  b001                   mov al, 1
--- calls: [('0xc91009', '0x5ad7e0'), ('0xc91011', '0x722690'), ('0xc9105c', '0x8ad780'), ('0xc910d2', '0x8b4d00'), ('0xc910e1', '0x8b4d80'), ('0xc911a8', '0x6c5f10'), ('0xc9129d', '0x6c5f30'), ('0xc912dc', '0x8815b0'), ('0xc91312', '0x6c5f10'), ('0xc913fa', '0x768470'), ('0xc91397', '0x4b9090'), ('0xc91277', '0x768470'), ('0xc9116c', '0x4b7a20'), ('0xc91218', '0x573c30'), ('0xc91229', '0x573b40'), ('0xc9111e', '0x88bab0')]
