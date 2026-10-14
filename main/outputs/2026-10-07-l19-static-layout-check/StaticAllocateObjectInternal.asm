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
0x891431  (f 0x491431)  4531f6                 xor r14d, r14d
0x891434  (f 0x491434)  89856cfdffff           mov dword ptr [rbp - 0x294], eax
0x89143a  (f 0x49143a)  4d85ff                 test r15, r15
0x89143d  (f 0x49143d)  0f8536030000           jne 0x891779
0x891443  (f 0x491443)  e944030000             jmp 0x89178c
0x891448  (f 0x491448)  4883bd60fdffff00       cmp qword ptr [rbp - 0x2a0], 0
0x891450  (f 0x491450)  4c8d3d49e11907         lea r15, [rip + 0x719e149]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x891457  (f 0x491457)  4d89fd                 mov r13, r15
0x89145a  (f 0x49145a)  743b                   je 0x891497
0x89145c  (f 0x49145c)  b800040000             mov eax, 0x400
0x891461  (f 0x491461)  41238424d4000000       and eax, dword ptr [r12 + 0xd4]
0x891469  (f 0x491469)  4d89fd                 mov r13, r15
0x89146c  (f 0x49146c)  7429                   je 0x891497
0x89146e  (f 0x49146e)  488bbd60fdffff         mov rdi, qword ptr [rbp - 0x2a0]
0x891475  (f 0x491475)  e8d64cffff             call 0x886150
0x89147a  (f 0x49147a)  4989c5                 mov r13, rax
0x89147d  (f 0x49147d)  e81e55f3ff             call 0x7c69a0
0x891482  (f 0x491482)  4939c5                 cmp r13, rax
0x891485  (f 0x491485)  488d0582973107         lea rax, [rip + 0x7319782]  ; rip->0x7baac0e (file 0x77aac0e)
0x89148c  (f 0x49148c)  4c8d2d0de11907         lea r13, [rip + 0x719e10d]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x891493  (f 0x491493)  4c0f44e8               cmove r13, rax
0x891497  (f 0x491497)  488dbdb0fdffff         lea rdi, [rbp - 0x250]
0x89149e  (f 0x49149e)  4c89e6                 mov rsi, r12
0x8914a1  (f 0x4914a1)  31d2                   xor edx, edx
0x8914a3  (f 0x4914a3)  e83858ffff             call 0x886ce0
0x8914a8  (f 0x4914a8)  83bdb8fdffff00         cmp dword ptr [rbp - 0x248], 0
0x8914af  (f 0x4914af)  4c89f8                 mov rax, r15
0x8914b2  (f 0x4914b2)  7407                   je 0x8914bb
0x8914b4  (f 0x4914b4)  488b85b0fdffff         mov rax, qword ptr [rbp - 0x250]
0x8914bb  (f 0x4914bb)  4883bd60fdffff00       cmp qword ptr [rbp - 0x2a0], 0
0x8914c3  (f 0x4914c3)  48898528fdffff         mov qword ptr [rbp - 0x2d8], rax
0x8914ca  (f 0x4914ca)  4c89f8                 mov rax, r15
0x8914cd  (f 0x4914cd)  0f845d040000           je 0x891930
0x8914d3  (f 0x4914d3)  488bb560fdffff         mov rsi, qword ptr [rbp - 0x2a0]
0x8914da  (f 0x4914da)  488dbd98fdffff         lea rdi, [rbp - 0x268]
0x8914e1  (f 0x4914e1)  31d2                   xor edx, edx
0x8914e3  (f 0x4914e3)  e8f857ffff             call 0x886ce0
0x8914e8  (f 0x4914e8)  83bda0fdffff00         cmp dword ptr [rbp - 0x260], 0
0x8914ef  (f 0x4914ef)  0f8434040000           je 0x891929
0x8914f5  (f 0x4914f5)  488b8598fdffff         mov rax, qword ptr [rbp - 0x268]
0x8914fc  (f 0x4914fc)  e92f040000             jmp 0x891930
0x891501  (f 0x491501)  31c0                   xor eax, eax
0x891503  (f 0x491503)  8b4008                 mov eax, dword ptr [rax + 8]
0x891506  (f 0x491506)  a900004008             test eax, 0x8400000
0x89150b  (f 0x49150b)  7424                   je 0x891531
0x89150d  (f 0x49150d)  e87e6bedff             call 0x768090
0x891512  (f 0x491512)  8b4804                 mov ecx, dword ptr [rax + 4]
0x891515  (f 0x491515)  31c0                   xor eax, eax
0x891517  (f 0x491517)  85c9                   test ecx, ecx
0x891519  (f 0x491519)  7512                   jne 0x89152d
0x89151b  (f 0x49151b)  e8706bedff             call 0x768090
0x891520  (f 0x491520)  4889c7                 mov rdi, rax
0x891523  (f 0x491523)  e81886edff             call 0x769b40
0x891528  (f 0x491528)  b801000000             mov eax, 1
0x89152d  (f 0x49152d)  41890424               mov dword ptr [r12], eax
0x891531  (f 0x491531)  8b4308                 mov eax, dword ptr [rbx + 8]
0x891534  (f 0x491534)  a900000100             test eax, 0x10000
0x891539  (f 0x491539)  0f850f010000           jne 0x89164e
0x89153f  (f 0x49153f)  8b4308                 mov eax, dword ptr [rbx + 8]
0x891542  (f 0x491542)  4c8da580fdffff         lea r12, [rbp - 0x280]
0x891549  (f 0x491549)  be80000000             mov esi, 0x80
0x89154e  (f 0x49154e)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x891552  (f 0x491552)  c5f8118580fdffff       vmovups xmmword ptr [rbp - 0x280], xmm0
0x89155a  (f 0x49155a)  4c89e7                 mov rdi, r12
0x89155d  (f 0x49155d)  e87e32bdff             call 0x4647e0
0x891562  (f 0x491562)  488d85cafdffff         lea rax, [rbp - 0x236]
0x891569  (f 0x491569)  488d75ca               lea rsi, [rbp - 0x36]
0x89156d  (f 0x49156d)  4c8dadb0fdffff         lea r13, [rbp - 0x250]
0x891574  (f 0x491574)  c685c8fdffff00         mov byte ptr [rbp - 0x238], 0
0x89157b  (f 0x49157b)  4889df                 mov rdi, rbx
0x89157e  (f 0x49157e)  31d2                   xor edx, edx
0x891580  (f 0x491580)  31c9                   xor ecx, ecx
0x891582  (f 0x491582)  488985b0fdffff         mov qword ptr [rbp - 0x250], rax
0x891589  (f 0x491589)  488985b8fdffff         mov qword ptr [rbp - 0x248], rax
0x891590  (f 0x491590)  4889b5c0fdffff         mov qword ptr [rbp - 0x240], rsi
0x891597  (f 0x491597)  4c89ee                 mov rsi, r13
0x89159a  (f 0x49159a)  e8c15affff             call 0x887060
0x89159f  (f 0x49159f)  488bb5b0fdffff         mov rsi, qword ptr [rbp - 0x250]
0x8915a6  (f 0x4915a6)  488b95b8fdffff         mov rdx, qword ptr [rbp - 0x248]
0x8915ad  (f 0x4915ad)  4c89e7                 mov rdi, r12
0x8915b0  (f 0x4915b0)  4829f2                 sub rdx, rsi
0x8915b3  (f 0x4915b3)  48d1ea                 shr rdx, 1
0x8915b6  (f 0x4915b6)  e8c535bdff             call 0x464b80
0x8915bb  (f 0x4915bb)  4c89ef                 mov rdi, r13
0x8915be  (f 0x4915be)  e81d2bd7ff             call 0x6040e0
0x8915c3  (f 0x4915c3)  4889df                 mov rdi, rbx
0x8915c6  (f 0x4915c6)  e8957cf3ff             call 0x7c9260
0x8915cb  (f 0x4915cb)  488b03                 mov rax, qword ptr [rbx]
0x8915ce  (f 0x4915ce)  4889df                 mov rdi, rbx
0x8915d1  (f 0x4915d1)  ff90b8000000           call qword ptr [rax + 0xb8]
0x8915d7  (f 0x4915d7)  84c0                   test al, al
0x8915d9  (f 0x4915d9)  7553                   jne 0x89162e
0x8915db  (f 0x4915db)  4c8d25f873ec08         lea r12, [rip + 0x8ec73f8]  ; rip->0x97589da (file 0x93589da)
0x8915e2  (f 0x4915e2)  4d89fd                 mov r13, r15
0x8915e5  (f 0x4915e5)  4531ff                 xor r15d, r15d
0x8915e8  (f 0x4915e8)  eb1f                   jmp 0x891609
0x8915f0  (f 0x4915f0)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x8915f4  (f 0x4915f4)  e85767b8ff             call 0x417d50
0x8915f9  (f 0x4915f9)  488b03                 mov rax, qword ptr [rbx]
0x8915fc  (f 0x4915fc)  4889df                 mov rdi, rbx
0x8915ff  (f 0x4915ff)  ff90b8000000           call qword ptr [rax + 0xb8]
0x891605  (f 0x491605)  84c0                   test al, al
0x891607  (f 0x491607)  7517                   jne 0x891620
0x891609  (f 0x491609)  41f6c701               test r15b, 1
0x89160d  (f 0x49160d)  75e1                   jne 0x8915f0
0x89160f  (f 0x49160f)  41803c2400             cmp byte ptr [r12], 0
0x891614  (f 0x491614)  75da                   jne 0x8915f0
0x891616  (f 0x491616)  e8450dfe06             call 0x7872360
0x89161b  (f 0x49161b)  41b701                 mov r15b, 1
0x89161e  (f 0x49161e)  ebd0                   jmp 0x8915f0
0x891620  (f 0x491620)  41f6c701               test r15b, 1
0x891624  (f 0x491624)  4d89ef                 mov r15, r13
0x891627  (f 0x491627)  7405                   je 0x89162e
0x891629  (f 0x491629)  e8320dfe06             call 0x7872360
0x89162e  (f 0x49162e)  4889df                 mov rdi, rbx
0x891631  (f 0x491631)  e86a7cf3ff             call 0x7c92a0
0x891636  (f 0x491636)  488bbd80fdffff         mov rdi, qword ptr [rbp - 0x280]
0x89163d  (f 0x49163d)  4885ff                 test rdi, rdi
0x891640  (f 0x491640)  7405                   je 0x891647
0x891642  (f 0x491642)  e84964c2ff             call 0x4b7a90
0x891647  (f 0x491647)  4c8d2dc28af208         lea r13, [rip + 0x8f28ac2]  ; rip->0x97ba110 (file 0x93ba110)
0x89164e  (f 0x49164e)  4c89bd38fdffff         mov qword ptr [rbp - 0x2c8], r15
0x891655  (f 0x491655)  450fb6bd94000000       movzx r15d, byte ptr [r13 + 0x94]
0x89165d  (f 0x49165d)  4489b530fdffff         mov dword ptr [rbp - 0x2d0], r14d
0x891664  (f 0x491664)  41c6859400000000       mov byte ptr [r13 + 0x94], 0
0x89166c  (f 0x49166c)  4d89ee                 mov r14, r13
0x89166f  (f 0x49166f)  4983c530               add r13, 0x30
0x891673  (f 0x491673)  4c89ef                 mov rdi, r13
0x891676  (f 0x491676)  e86508fe06             call 0x7871ee0
0x89167b  (f 0x49167b)  85c0                   test eax, eax
0x89167d  (f 0x49167d)  7509                   jne 0x891688
0x89167f  (f 0x49167f)  e86c08fe06             call 0x7871ef0
0x891684  (f 0x491684)  49894638               mov qword ptr [r14 + 0x38], rax
0x891688  (f 0x491688)  49ff4640               inc qword ptr [r14 + 0x40]
0x89168c  (f 0x49168c)  4c89f7                 mov rdi, r14
0x89168f  (f 0x49168f)  4889de                 mov rsi, rbx
0x891692  (f 0x491692)  e8192affff             call 0x8840b0
0x891697  (f 0x491697)  49ff4e40               dec qword ptr [r14 + 0x40]
0x89169b  (f 0x49169b)  7510                   jne 0x8916ad
0x89169d  (f 0x49169d)  4c89ef                 mov rdi, r13
0x8916a0  (f 0x4916a0)  49c7463800000000       mov qword ptr [r14 + 0x38], 0
0x8916a8  (f 0x4916a8)  e87308fe06             call 0x7871f20
0x8916ad  (f 0x4916ad)  488b03                 mov rax, qword ptr [rbx]
0x8916b0  (f 0x4916b0)  4889df                 mov rdi, rbx
0x8916b3  (f 0x4916b3)  ff10                   call qword ptr [rax]
0x8916b5  (f 0x4916b5)  448bad40fdffff         mov r13d, dword ptr [rbp - 0x2c0]
0x8916bc  (f 0x4916bc)  b101                   mov cl, 1
0x8916be  (f 0x4916be)  4588be94000000         mov byte ptr [r14 + 0x94], r15b
0x8916c5  (f 0x4916c5)  83bd6cfdffff10         cmp dword ptr [rbp - 0x294], 0x10
0x8916cc  (f 0x4916cc)  448b7d20               mov r15d, dword ptr [rbp + 0x20]
0x8916d0  (f 0x4916d0)  4989dc                 mov r12, rbx
0x8916d3  (f 0x4916d3)  48898d40fdffff         mov qword ptr [rbp - 0x2c0], rcx
0x8916da  (f 0x4916da)  7509                   jne 0x8916e5
0x8916dc  (f 0x4916dc)  4c8bb520fdffff         mov r14, qword ptr [rbp - 0x2e0]
0x8916e3  (f 0x4916e3)  eb24                   jmp 0x891709
0x8916e5  (f 0x4916e5)  4c8bb520fdffff         mov r14, qword ptr [rbp - 0x2e0]
0x8916ec  (f 0x4916ec)  4489e8                 mov eax, r13d
0x8916ef  (f 0x4916ef)  83c840                 or eax, 0x40
0x8916f2  (f 0x4916f2)  41f686d400000008       test byte ptr [r14 + 0xd4], 8
0x8916fa  (f 0x4916fa)  410f44c5               cmove eax, r13d
0x8916fe  (f 0x4916fe)  f68534fdffff20         test byte ptr [rbp - 0x2cc], 0x20
0x891705  (f 0x491705)  440f44e8               cmove r13d, eax
0x891709  (f 0x491709)  48639528fdffff         movsxd rdx, dword ptr [rbp - 0x2d8]
0x891710  (f 0x491710)  4c89e7                 mov rdi, r12
0x891713  (f 0x491713)  31f6                   xor esi, esi
0x891715  (f 0x491715)  c7856cfdffff00000000   mov dword ptr [rbp - 0x294], 0
0x89171f  (f 0x49171f)  e81c07fe06             call 0x7871e40
0x891724  (f 0x491724)  8b8554fdffff           mov eax, dword ptr [rbp - 0x2ac]
0x89172a  (f 0x49172a)  4c8b8da8fdffff         mov r9, qword ptr [rbp - 0x258]
0x891731  (f 0x491731)  4181cd00020000         or r13d, 0x200
0x891738  (f 0x491738)  85c0                   test eax, eax
0x89173a  (f 0x49173a)  440f45f8               cmovne r15d, eax
0x89173e  (f 0x49173e)  4883ec10               sub rsp, 0x10
0x891742  (f 0x491742)  8b8d30fdffff           mov ecx, dword ptr [rbp - 0x2d0]
0x891748  (f 0x491748)  4c8b8560fdffff         mov r8, qword ptr [rbp - 0x2a0]
0x89174f  (f 0x49174f)  4c89e7                 mov rdi, r12
0x891752  (f 0x491752)  4c89f6                 mov rsi, r14
0x891755  (f 0x491755)  4489ea                 mov edx, r13d
0x891758  (f 0x491758)  4157                   push r15
0x89175a  (f 0x49175a)  ffb538fdffff           push qword ptr [rbp - 0x2c8]
0x891760  (f 0x491760)  e8cb3cffff             call 0x885430
0x891765  (f 0x491765)  4883c420               add rsp, 0x20
0x891769  (f 0x491769)  4c8b7d18               mov r15, qword ptr [rbp + 0x18]
0x89176d  (f 0x49176d)  4c8bb540fdffff         mov r14, qword ptr [rbp - 0x2c0]
0x891774  (f 0x491774)  4d85ff                 test r15, r15
0x891777  (f 0x491777)  7413                   je 0x89178c
0x891779  (f 0x491779)  49837c242000           cmp qword ptr [r12 + 0x20], 0
0x89177f  (f 0x49177f)  740b                   je 0x89178c
0x891781  (f 0x491781)  4c89e7                 mov rdi, r12
0x891784  (f 0x491784)  4c89fe                 mov rsi, r15
0x891787  (f 0x491787)  e854200200             call 0x8b37e0
0x89178c  (f 0x49178c)  448bad6cfdffff         mov r13d, dword ptr [rbp - 0x294]
0x891793  (f 0x491793)  4584f6                 test r14b, r14b
0x891796  (f 0x491796)  743a                   je 0x8917d2
0x891798  (f 0x491798)  4c8bb548fdffff         mov r14, qword ptr [rbp - 0x2b8]
0x89179f  (f 0x49179f)  8b9558fdffff           mov edx, dword ptr [rbp - 0x2a8]
0x8917a5  (f 0x4917a5)  4c89e7                 mov rdi, r12
0x8917a8  (f 0x4917a8)  31c9                   xor ecx, ecx
0x8917aa  (f 0x4917aa)  4c89f6                 mov rsi, r14
0x8917ad  (f 0x4917ad)  e85e310200             call 0x8b4910
0x8917b2  (f 0x4917b2)  4d85f6                 test r14, r14
0x8917b5  (f 0x4917b5)  741b                   je 0x8917d2
0x8917b7  (f 0x4917b7)  488b8548fdffff         mov rax, qword ptr [rbp - 0x2b8]
0x8917be  (f 0x4917be)  48638d58fdffff         movsxd rcx, dword ptr [rbp - 0x2a8]
0x8917c5  (f 0x4917c5)  488b4018               mov rax, qword ptr [rax + 0x18]
0x8917c9  (f 0x4917c9)  486bc968               imul rcx, rcx, 0x68
0x8917cd  (f 0x4917cd)  4c89640840             mov qword ptr [rax + rcx + 0x40], r12
0x8917d2  (f 0x4917d2)  488d05bfaae408         lea rax, [rip + 0x8e4aabf]  ; rip->0x96dc298 (file 0x92dc298)
0x8917d9  (f 0x4917d9)  4c8b7510               mov r14, qword ptr [rbp + 0x10]
0x8917dd  (f 0x4917dd)  ff10                   call qword ptr [rax]
0x8917df  (f 0x4917df)  84c0                   test al, al
0x8917e1  (f 0x4917e1)  7452                   je 0x891835
0x8917e3  (f 0x4917e3)  488d8580fdffff         lea rax, [rbp - 0x280]
0x8917ea  (f 0x4917ea)  488d0d9f58e6ff         lea rcx, [rip - 0x19a761]  ; rip->0x6f7090 (file 0x2f7090)
0x8917f1  (f 0x4917f1)  488985b8fdffff         mov qword ptr [rbp - 0x248], rax
0x8917f8  (f 0x4917f8)  48898db0fdffff         mov qword ptr [rbp - 0x250], rcx
0x8917ff  (f 0x4917ff)  e8bc390200             call 0x8b51c0
0x891804  (f 0x491804)  488dbdb0fdffff         lea rdi, [rbp - 0x250]
0x89180b  (f 0x49180b)  4889c6                 mov rsi, rax
0x89180e  (f 0x49180e)  e84dcfc2ff             call 0x4be760
0x891813  (f 0x491813)  488b7848               mov rdi, qword ptr [rax + 0x48]
0x891817  (f 0x491817)  4885ff                 test rdi, rdi
0x89181a  (f 0x49181a)  0f84bf000000           je 0x8918df
0x891820  (f 0x491820)  488b07                 mov rax, qword ptr [rdi]
0x891823  (f 0x491823)  410fb6d5               movzx edx, r13b
0x891827  (f 0x491827)  4c89e6                 mov rsi, r12
0x89182a  (f 0x49182a)  ff90a8000000           call qword ptr [rax + 0xa8]
0x891830  (f 0x491830)  e9aa000000             jmp 0x8918df
0x891835  (f 0x491835)  418b44240c             mov eax, dword ptr [r12 + 0xc]
0x89183a  (f 0x49183a)  4c8d3dcf88f208         lea r15, [rip + 0x8f288cf]  ; rip->0x97ba110 (file 0x93ba110)
0x891841  (f 0x491841)  41394724               cmp dword ptr [r15 + 0x24], eax
0x891845  (f 0x491845)  7e19                   jle 0x891860
0x891847  (f 0x491847)  498b5710               mov rdx, qword ptr [r15 + 0x10]
0x89184b  (f 0x49184b)  89c1                   mov ecx, eax
0x89184d  (f 0x49184d)  0fb7c0                 movzx eax, ax
0x891850  (f 0x491850)  488d3c40               lea rdi, [rax + rax*2]
0x891854  (f 0x491854)  c1e910                 shr ecx, 0x10
0x891857  (f 0x491857)  c1e703                 shl edi, 3
0x89185a  (f 0x49185a)  48033cca               add rdi, qword ptr [rdx + rcx*8]
0x89185e  (f 0x49185e)  eb02                   jmp 0x891862
0x891860  (f 0x491860)  31ff                   xor edi, edi
0x891862  (f 0x491862)  be00004008             mov esi, 0x8400000
0x891867  (f 0x491867)  e8246dedff             call 0x768590
0x89186c  (f 0x49186c)  418b44240c             mov eax, dword ptr [r12 + 0xc]
0x891871  (f 0x491871)  41394724               cmp dword ptr [r15 + 0x24], eax
0x891875  (f 0x491875)  7e23                   jle 0x89189a
0x891877  (f 0x491877)  498b5710               mov rdx, qword ptr [r15 + 0x10]
0x89187b  (f 0x49187b)  89c1                   mov ecx, eax
0x89187d  (f 0x49187d)  0fb7c0                 movzx eax, ax
0x891880  (f 0x491880)  488d0440               lea rax, [rax + rax*2]
0x891884  (f 0x491884)  c1e910                 shr ecx, 0x10
0x891887  (f 0x491887)  c1e003                 shl eax, 3
0x89188a  (f 0x49188a)  480304ca               add rax, qword ptr [rdx + rcx*8]
0x89188e  (f 0x49188e)  8b4008                 mov eax, dword ptr [rax + 8]
0x891891  (f 0x491891)  a900000004             test eax, 0x4000000
0x891896  (f 0x491896)  750e                   jne 0x8918a6
0x891898  (f 0x491898)  eb45                   jmp 0x8918df
0x89189a  (f 0x49189a)  31c0                   xor eax, eax
0x89189c  (f 0x49189c)  8b4008                 mov eax, dword ptr [rax + 8]
0x89189f  (f 0x49189f)  a900000004             test eax, 0x4000000
0x8918a4  (f 0x4918a4)  7439                   je 0x8918df
0x8918a6  (f 0x4918a6)  e8e577c2ff             call 0x4b9090
0x8918ab  (f 0x4918ab)  84c0                   test al, al
0x8918ad  (f 0x4918ad)  7430                   je 0x8918df
0x8918af  (f 0x4918af)  418b44240c             mov eax, dword ptr [r12 + 0xc]
0x8918b4  (f 0x4918b4)  41394724               cmp dword ptr [r15 + 0x24], eax
0x8918b8  (f 0x4918b8)  7e19                   jle 0x8918d3
0x8918ba  (f 0x4918ba)  498b5710               mov rdx, qword ptr [r15 + 0x10]
0x8918be  (f 0x4918be)  89c1                   mov ecx, eax
0x8918c0  (f 0x4918c0)  0fb7c0                 movzx eax, ax
0x8918c3  (f 0x4918c3)  488d3c40               lea rdi, [rax + rax*2]
0x8918c7  (f 0x4918c7)  c1e910                 shr ecx, 0x10
0x8918ca  (f 0x4918ca)  c1e703                 shl edi, 3
0x8918cd  (f 0x4918cd)  48033cca               add rdi, qword ptr [rdx + rcx*8]
0x8918d1  (f 0x4918d1)  eb02                   jmp 0x8918d5
0x8918d3  (f 0x4918d3)  31ff                   xor edi, edi
0x8918d5  (f 0x4918d5)  be00000004             mov esi, 0x4000000
0x8918da  (f 0x4918da)  e8b16cedff             call 0x768590
0x8918df  (f 0x4918df)  4d85f6                 test r14, r14
0x8918e2  (f 0x4918e2)  7403                   je 0x8918e7
0x8918e4  (f 0x4918e4)  45882e                 mov byte ptr [r14], r13b
0x8918e7  (f 0x4918e7)  488b051a06ca08         mov rax, qword ptr [rip + 0x8ca061a]  ; rip->0x9531f08 (file 0x9131f08)
0x8918ee  (f 0x4918ee)  488b00                 mov rax, qword ptr [rax]
0x8918f1  (f 0x4918f1)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x8918f5  (f 0x4918f5)  0f85a6010000           jne 0x891aa1
0x8918fb  (f 0x4918fb)  4c89e0                 mov rax, r12
0x8918fe  (f 0x4918fe)  4881c4b8020000         add rsp, 0x2b8
0x891905  (f 0x491905)  5b                     pop rbx
0x891906  (f 0x491906)  415c                   pop r12
0x891908  (f 0x491908)  415d                   pop r13
0x89190a  (f 0x49190a)  415e                   pop r14
0x89190c  (f 0x49190c)  415f                   pop r15
0x89190e  (f 0x49190e)  5d                     pop rbp
0x89190f  (f 0x49190f)  c3                     ret 
0x891910  (f 0x491910)  4989dc                 mov r12, rbx
0x891913  (f 0x491913)  b001                   mov al, 1
0x891915  (f 0x491915)  89856cfdffff           mov dword ptr [rbp - 0x294], eax
0x89191b  (f 0x49191b)  4d85ff                 test r15, r15
0x89191e  (f 0x49191e)  0f8555feffff           jne 0x891779
0x891924  (f 0x491924)  e963feffff             jmp 0x89178c
0x891929  (f 0x491929)  488d0570dc1907         lea rax, [rip + 0x719dc70]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x891930  (f 0x491930)  488dbd80fdffff         lea rdi, [rbp - 0x280]
0x891937  (f 0x491937)  488db5a8fdffff         lea rsi, [rbp - 0x258]
0x89193e  (f 0x49193e)  4c89ad58fdffff         mov qword ptr [rbp - 0x2a8], r13
0x891945  (f 0x491945)  4989c5                 mov r13, rax
0x891948  (f 0x491948)  e873d7daff             call 0x63f0c0
0x89194d  (f 0x49194d)  83bd88fdffff00         cmp dword ptr [rbp - 0x278], 0
0x891954  (f 0x491954)  4c89f8                 mov rax, r15
0x891957  (f 0x491957)  7407                   je 0x891960
0x891959  (f 0x491959)  488b8580fdffff         mov rax, qword ptr [rbp - 0x280]
0x891960  (f 0x491960)  488dbd70fdffff         lea rdi, [rbp - 0x290]
0x891967  (f 0x491967)  4889de                 mov rsi, rbx
0x89196a  (f 0x49196a)  b901000000             mov ecx, 1
0x89196f  (f 0x49196f)  31d2                   xor edx, edx
0x891971  (f 0x491971)  48898548fdffff         mov qword ptr [rbp - 0x2b8], rax
0x891978  (f 0x491978)  e81356ffff             call 0x886f90
0x89197d  (f 0x49197d)  83bd78fdffff00         cmp dword ptr [rbp - 0x288], 0
0x891984  (f 0x491984)  7407                   je 0x89198d
0x891986  (f 0x491986)  4c8bbd70fdffff         mov r15, qword ptr [rbp - 0x290]
0x89198d  (f 0x49198d)  8b430c                 mov eax, dword ptr [rbx + 0xc]
0x891990  (f 0x491990)  488d0d7987f208         lea rcx, [rip + 0x8f28779]  ; rip->0x97ba110 (file 0x93ba110)
0x891997  (f 0x491997)  4c8b8528fdffff         mov r8, qword ptr [rbp - 0x2d8]
0x89199e  (f 0x49199e)  394124                 cmp dword ptr [rcx + 0x24], eax
0x8919a1  (f 0x4919a1)  7e3a                   jle 0x8919dd
0x8919a3  (f 0x4919a3)  488b4910               mov rcx, qword ptr [rcx + 0x10]
0x8919a7  (f 0x4919a7)  89c2                   mov edx, eax
0x8919a9  (f 0x4919a9)  0fb7c0                 movzx eax, ax
0x8919ac  (f 0x4919ac)  4c8d15f7fb3b07         lea r10, [rip + 0x73bfbf7]  ; rip->0x7c515aa (file 0x78515aa)
0x8919b3  (f 0x4919b3)  c1ea10                 shr edx, 0x10
0x8919b6  (f 0x4919b6)  488b0cd1               mov rcx, qword ptr [rcx + rdx*8]
0x8919ba  (f 0x4919ba)  488d1440               lea rdx, [rax + rax*2]
0x8919be  (f 0x4919be)  488d04d1               lea rax, [rcx + rdx*8]
0x8919c2  (f 0x4919c2)  8b4cd108               mov ecx, dword ptr [rcx + rdx*8 + 8]
0x8919c6  (f 0x4919c6)  448b5b08               mov r11d, dword ptr [rbx + 8]
0x8919ca  (f 0x4919ca)  488d15cfdb1907         lea rdx, [rip + 0x719dbcf]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x8919d1  (f 0x4919d1)  f7c100002000           test ecx, 0x200000
0x8919d7  (f 0x4919d7)  4c0f44d2               cmove r10, rdx
0x8919db  (f 0x4919db)  eb24                   jmp 0x891a01
0x8919dd  (f 0x4919dd)  8b042508000000         mov eax, dword ptr [8]
0x8919e4  (f 0x4919e4)  448b5b08               mov r11d, dword ptr [rbx + 8]
0x8919e8  (f 0x4919e8)  4c8d15bbfb3b07         lea r10, [rip + 0x73bfbbb]  ; rip->0x7c515aa (file 0x78515aa)
0x8919ef  (f 0x4919ef)  a900002000             test eax, 0x200000
0x8919f4  (f 0x4919f4)  488d05a5db1907         lea rax, [rip + 0x719dba5]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x8919fb  (f 0x4919fb)  4c0f44d0               cmove r10, rax
0x8919ff  (f 0x4919ff)  31c0                   xor eax, eax
0x891a01  (f 0x491a01)  488b8d58fdffff         mov rcx, qword ptr [rbp - 0x2a8]
0x891a08  (f 0x491a08)  448b6008               mov r12d, dword ptr [rax + 8]
0x891a0c  (f 0x491a0c)  4883ec08               sub rsp, 8
0x891a10  (f 0x491a10)  488d3d2b9a6307         lea rdi, [rip + 0x7639a2b]  ; rip->0x7ecb442 (file 0x7acb442)
0x891a17  (f 0x491a17)  488d1526ce2e07         lea rdx, [rip + 0x72ece26]  ; rip->0x7b7e844 (file 0x777e844)
0x891a1e  (f 0x491a1e)  be3f0e0000             mov esi, 0xe3f
0x891a23  (f 0x491a23)  4d89e9                 mov r9, r13
0x891a26  (f 0x491a26)  31c0                   xor eax, eax
0x891a28  (f 0x491a28)  4154                   push r12
0x891a2a  (f 0x491a2a)  4153                   push r11
0x891a2c  (f 0x491a2c)  4152                   push r10
0x891a2e  (f 0x491a2e)  4157                   push r15
0x891a30  (f 0x491a30)  ffb548fdffff           push qword ptr [rbp - 0x2b8]
0x891a36  (f 0x491a36)  e87536ceff             call 0x5750b0
0x891a3b  (f 0x491a3b)  4883c430               add rsp, 0x30
0x891a3f  (f 0x491a3f)  488bbd70fdffff         mov rdi, qword ptr [rbp - 0x290]
0x891a46  (f 0x491a46)  4885ff                 test rdi, rdi
0x891a49  (f 0x491a49)  7405                   je 0x891a50
0x891a4b  (f 0x491a4b)  e84060c2ff             call 0x4b7a90
0x891a50  (f 0x491a50)  488bbd80fdffff         mov rdi, qword ptr [rbp - 0x280]
0x891a57  (f 0x491a57)  4c8b7d18               mov r15, qword ptr [rbp + 0x18]
0x891a5b  (f 0x491a5b)  4c8ba520fdffff         mov r12, qword ptr [rbp - 0x2e0]
0x891a62  (f 0x491a62)  4885ff                 test rdi, rdi
0x891a65  (f 0x491a65)  7405                   je 0x891a6c
0x891a67  (f 0x491a67)  e82460c2ff             call 0x4b7a90
0x891a6c  (f 0x491a6c)  4883bd60fdffff00       cmp qword ptr [rbp - 0x2a0], 0
0x891a74  (f 0x491a74)  7411                   je 0x891a87
0x891a76  (f 0x491a76)  488bbd98fdffff         mov rdi, qword ptr [rbp - 0x268]
0x891a7d  (f 0x491a7d)  4885ff                 test rdi, rdi
0x891a80  (f 0x491a80)  7405                   je 0x891a87
0x891a82  (f 0x491a82)  e80960c2ff             call 0x4b7a90
0x891a87  (f 0x491a87)  488bbdb0fdffff         mov rdi, qword ptr [rbp - 0x250]
0x891a8e  (f 0x491a8e)  4885ff                 test rdi, rdi
0x891a91  (f 0x491a91)  0f840bf6ffff           je 0x8910a2
0x891a97  (f 0x491a97)  e8f45fc2ff             call 0x4b7a90
0x891a9c  (f 0x491a9c)  e901f6ffff             jmp 0x8910a2
0x891aa1  (f 0x491aa1)  e8ba03fe06             call 0x7871e60
0x891aa6  (f 0x491aa6)  0f0b                   ud2 
--- calls: [('0xc91009', '0x5ad7e0'), ('0xc91011', '0x722690'), ('0xc9105c', '0x8ad780'), ('0xc910d2', '0x8b4d00'), ('0xc910e1', '0x8b4d80'), ('0xc911a8', '0x6c5f10'), ('0xc9129d', '0x6c5f30'), ('0xc912dc', '0x8815b0'), ('0xc91312', '0x6c5f10'), ('0xc917ad', '0x8b4910'), ('0xc917ff', '0x8b51c0'), ('0xc9180e', '0x4be760'), ('0xc91aa1', '0x7871e60'), ('0xc91867', '0x768590'), ('0xc918a6', '0x4b9090'), ('0xc918da', '0x768590'), ('0xc91787', '0x8b37e0'), ('0xc913fa', '0x768470'), ('0xc91397', '0x4b9090'), ('0xc9155d', '0x4647e0'), ('0xc9159a', '0x887060'), ('0xc915b6', '0x464b80'), ('0xc915be', '0x6040e0'), ('0xc915c6', '0x7c9260'), ('0xc91616', '0x7872360'), ('0xc915f4', '0x417d50'), ('0xc91629', '0x7872360'), ('0xc91631', '0x7c92a0'), ('0xc91642', '0x4b7a90'), ('0xc91676', '0x7871ee0'), ('0xc9167f', '0x7871ef0'), ('0xc91692', '0x8840b0'), ('0xc916a8', '0x7871f20'), ('0xc9171f', '0x7871e40'), ('0xc91760', '0x885430'), ('0xc9150d', '0x768090'), ('0xc9151b', '0x768090'), ('0xc91523', '0x769b40'), ('0xc91277', '0x768470'), ('0xc91475', '0x886150'), ('0xc9147d', '0x7c69a0'), ('0xc914a3', '0x886ce0'), ('0xc914e3', '0x886ce0'), ('0xc91948', '0x63f0c0'), ('0xc91978', '0x886f90'), ('0xc91a36', '0x5750b0'), ('0xc91a4b', '0x4b7a90'), ('0xc91a67', '0x4b7a90'), ('0xc91a82', '0x4b7a90'), ('0xc91a97', '0x4b7a90'), ('0xc9116c', '0x4b7a20'), ('0xc91218', '0x573c30'), ('0xc91229', '0x573b40'), ('0xc9111e', '0x88bab0')]
