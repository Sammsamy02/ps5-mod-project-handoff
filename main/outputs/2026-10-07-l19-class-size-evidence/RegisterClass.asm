0x886380  (f 0x486380)  55                     push rbp
0x886381  (f 0x486381)  4889e5                 mov rbp, rsp
0x886384  (f 0x486384)  4157                   push r15
0x886386  (f 0x486386)  4156                   push r14
0x886388  (f 0x486388)  4155                   push r13
0x88638a  (f 0x48638a)  4154                   push r12
0x88638c  (f 0x48638c)  53                     push rbx
0x88638d  (f 0x48638d)  4883ec48               sub rsp, 0x48
0x886391  (f 0x486391)  488b0570bbca08         mov rax, qword ptr [rip + 0x8cabb70]  ; rip->0x9531f08 (file 0x9131f08)
0x886398  (f 0x486398)  4d89c4                 mov r12, r8
0x88639b  (f 0x48639b)  4989cf                 mov r15, rcx
0x88639e  (f 0x48639e)  4989d6                 mov r14, rdx
0x8863a1  (f 0x4863a1)  4989f5                 mov r13, rsi
0x8863a4  (f 0x4863a4)  4889fb                 mov rbx, rdi
0x8863a7  (f 0x4863a7)  488b00                 mov rax, qword ptr [rax]
0x8863aa  (f 0x4863aa)  488945d0               mov qword ptr [rbp - 0x30], rax
0x8863ae  (f 0x4863ae)  0fb605333df308         movzx eax, byte ptr [rip + 0x8f33d33]  ; rip->0x97ba0e8 (file 0x93ba0e8)
0x8863b5  (f 0x4863b5)  84c0                   test al, al
0x8863b7  (f 0x4863b7)  0f8462010000           je 0x88651f
0x8863bd  (f 0x4863bd)  8b05153df308           mov eax, dword ptr [rip + 0x8f33d15]  ; rip->0x97ba0d8 (file 0x93ba0d8)
0x8863c3  (f 0x4863c3)  3b05133df308           cmp eax, dword ptr [rip + 0x8f33d13]  ; rip->0x97ba0dc (file 0x93ba0dc)
0x8863c9  (f 0x4863c9)  7518                   jne 0x8863e3
0x8863cb  (f 0x4863cb)  488d35fe3cf308         lea rsi, [rip + 0x8f33cfe]  ; rip->0x97ba0d0 (file 0x93ba0d0)
0x8863d2  (f 0x4863d2)  488d15033df308         lea rdx, [rip + 0x8f33d03]  ; rip->0x97ba0dc (file 0x93ba0dc)
0x8863d9  (f 0x4863d9)  bf20080000             mov edi, 0x820
0x8863de  (f 0x4863de)  e8bd34baff             call 0x4298a0
0x8863e3  (f 0x4863e3)  8d4801                 lea ecx, [rax + 1]
0x8863e6  (f 0x4863e6)  4898                   cdqe 
0x8863e8  (f 0x4863e8)  4c89fe                 mov rsi, r15
0x8863eb  (f 0x4863eb)  890de73cf308           mov dword ptr [rip + 0x8f33ce7], ecx  ; rip->0x97ba0d8 (file 0x93ba0d8)
0x8863f1  (f 0x4863f1)  48c1e005               shl rax, 5
0x8863f5  (f 0x4863f5)  488b0dd43cf308         mov rcx, qword ptr [rip + 0x8f33cd4]  ; rip->0x97ba0d0 (file 0x93ba0d0)
0x8863fc  (f 0x4863fc)  48891c01               mov qword ptr [rcx + rax], rbx
0x886400  (f 0x486400)  4c896c0108             mov qword ptr [rcx + rax + 8], r13
0x886405  (f 0x486405)  4c89740110             mov qword ptr [rcx + rax + 0x10], r14
0x88640a  (f 0x48640a)  4c89640118             mov qword ptr [rcx + rax + 0x18], r12
0x88640f  (f 0x48640f)  4c8d65c0               lea r12, [rbp - 0x40]
0x886413  (f 0x486413)  4c89e7                 mov rdi, r12
0x886416  (f 0x486416)  e8d5dabdff             call 0x463ef0
0x88641b  (f 0x48641b)  4c89e7                 mov rdi, r12
0x88641e  (f 0x48641e)  ba01000000             mov edx, 1
0x886423  (f 0x486423)  31f6                   xor esi, esi
0x886425  (f 0x486425)  31c9                   xor ecx, ecx
0x886427  (f 0x486427)  e8740bbeff             call 0x466fa0
0x88642c  (f 0x48642c)  488d3573413f07         lea rsi, [rip + 0x73f4173]  ; rip->0x7c7a5a6 (file 0x787a5a6)
0x886433  (f 0x486433)  4c89e7                 mov rdi, r12
0x886436  (f 0x486436)  ba0b000000             mov edx, 0xb
0x88643b  (f 0x48643b)  b901000000             mov ecx, 1
0x886440  (f 0x486440)  e8fb08beff             call 0x466d40
0x886445  (f 0x486445)  84c0                   test al, al
0x886447  (f 0x486447)  7412                   je 0x88645b
0x886449  (f 0x486449)  488d7dc0               lea rdi, [rbp - 0x40]
0x88644d  (f 0x48644d)  ba0b000000             mov edx, 0xb
0x886452  (f 0x486452)  31f6                   xor esi, esi
0x886454  (f 0x486454)  31c9                   xor ecx, ecx
0x886456  (f 0x486456)  e8450bbeff             call 0x466fa0
0x88645b  (f 0x48645b)  837dc800               cmp dword ptr [rbp - 0x38], 0
0x88645f  (f 0x48645f)  4c8d3d3a911a07         lea r15, [rip + 0x71a913a]  ; rip->0x7a2f5a0 (file 0x762f5a0)
0x886466  (f 0x486466)  4c89fe                 mov rsi, r15
0x886469  (f 0x486469)  7404                   je 0x88646f
0x88646b  (f 0x48646b)  488b75c0               mov rsi, qword ptr [rbp - 0x40]
0x88646f  (f 0x48646f)  4c89f7                 mov rdi, r14
0x886472  (f 0x486472)  4989d8                 mov r8, rbx
0x886475  (f 0x486475)  48c7042400000000       mov qword ptr [rsp], 0
0x88647d  (f 0x48647d)  31d2                   xor edx, edx
0x88647f  (f 0x48647f)  31c9                   xor ecx, ecx
0x886481  (f 0x486481)  4531c9                 xor r9d, r9d
0x886484  (f 0x486484)  e89750e7ff             call 0x6fb520
0x886489  (f 0x486489)  4c8d65a0               lea r12, [rbp - 0x60]
0x88648d  (f 0x48648d)  488d355e173a07         lea rsi, [rip + 0x73a175e]  ; rip->0x7c27bf2 (file 0x7827bf2)
0x886494  (f 0x486494)  4c89e7                 mov rdi, r12
0x886497  (f 0x486497)  e854dabdff             call 0x463ef0
0x88649c  (f 0x48649c)  488d7db0               lea rdi, [rbp - 0x50]
0x8864a0  (f 0x4864a0)  488d55c0               lea rdx, [rbp - 0x40]
0x8864a4  (f 0x4864a4)  4c89e6                 mov rsi, r12
0x8864a7  (f 0x4864a7)  e8440ebeff             call 0x4672f0
0x8864ac  (f 0x4864ac)  837db800               cmp dword ptr [rbp - 0x48], 0
0x8864b0  (f 0x4864b0)  7404                   je 0x8864b6
0x8864b2  (f 0x4864b2)  4c8b7db0               mov r15, qword ptr [rbp - 0x50]
0x8864b6  (f 0x4864b6)  4c89f7                 mov rdi, r14
0x8864b9  (f 0x4864b9)  4c89fe                 mov rsi, r15
0x8864bc  (f 0x4864bc)  ba01000000             mov edx, 1
0x8864c1  (f 0x4864c1)  4989d8                 mov r8, rbx
0x8864c4  (f 0x4864c4)  48c7042400000000       mov qword ptr [rsp], 0
0x8864cc  (f 0x4864cc)  31c9                   xor ecx, ecx
0x8864ce  (f 0x4864ce)  4531c9                 xor r9d, r9d
0x8864d1  (f 0x4864d1)  e84a50e7ff             call 0x6fb520
0x8864d6  (f 0x4864d6)  488b7db0               mov rdi, qword ptr [rbp - 0x50]
0x8864da  (f 0x4864da)  4885ff                 test rdi, rdi
0x8864dd  (f 0x4864dd)  7405                   je 0x8864e4
0x8864df  (f 0x4864df)  e8ac15c3ff             call 0x4b7a90
0x8864e4  (f 0x4864e4)  488b7da0               mov rdi, qword ptr [rbp - 0x60]
0x8864e8  (f 0x4864e8)  4885ff                 test rdi, rdi
0x8864eb  (f 0x4864eb)  7405                   je 0x8864f2
0x8864ed  (f 0x4864ed)  e89e15c3ff             call 0x4b7a90
0x8864f2  (f 0x4864f2)  488b7dc0               mov rdi, qword ptr [rbp - 0x40]
0x8864f6  (f 0x4864f6)  4885ff                 test rdi, rdi
0x8864f9  (f 0x4864f9)  7405                   je 0x886500
0x8864fb  (f 0x4864fb)  e89015c3ff             call 0x4b7a90
0x886500  (f 0x486500)  488b0501baca08         mov rax, qword ptr [rip + 0x8caba01]  ; rip->0x9531f08 (file 0x9131f08)
0x886507  (f 0x486507)  488b00                 mov rax, qword ptr [rax]
0x88650a  (f 0x48650a)  483b45d0               cmp rax, qword ptr [rbp - 0x30]
0x88650e  (f 0x48650e)  7564                   jne 0x886574
0x886510  (f 0x486510)  4883c448               add rsp, 0x48
0x886514  (f 0x486514)  5b                     pop rbx
0x886515  (f 0x486515)  415c                   pop r12
0x886517  (f 0x486517)  415d                   pop r13
0x886519  (f 0x486519)  415e                   pop r14
0x88651b  (f 0x48651b)  415f                   pop r15
0x88651d  (f 0x48651d)  5d                     pop rbp
0x88651e  (f 0x48651e)  c3                     ret 
0x88651f  (f 0x48651f)  488d3dc23bf308         lea rdi, [rip + 0x8f33bc2]  ; rip->0x97ba0e8 (file 0x93ba0e8)
0x886526  (f 0x486526)  e885b9fe06             call 0x7871eb0
0x88652b  (f 0x48652b)  85c0                   test eax, eax
0x88652d  (f 0x48652d)  0f848afeffff           je 0x8863bd
0x886533  (f 0x486533)  488d35963bf308         lea rsi, [rip + 0x8f33b96]  ; rip->0x97ba0d0 (file 0x93ba0d0)
0x88653a  (f 0x48653a)  488d3d3fdeb7ff         lea rdi, [rip - 0x4821c1]  ; rip->0x404380 (file 0x4380)
0x886541  (f 0x486541)  488d15b85ae508         lea rdx, [rip + 0x8e55ab8]  ; rip->0x96dc000 (file 0x92dc000)
0x886548  (f 0x486548)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x88654c  (f 0x48654c)  c5f811057c3bf308       vmovups xmmword ptr [rip + 0x8f33b7c], xmm0  ; rip->0x97ba0d0 (file 0x93ba0d0)
0x886554  (f 0x486554)  c705823bf30800000000   mov dword ptr [rip + 0x8f33b82], 0  ; rip->0x97ba0e0 (file 0x93ba0e0)
0x88655e  (f 0x48655e)  e86db9fe06             call 0x7871ed0
0x886563  (f 0x486563)  488d3d7e3bf308         lea rdi, [rip + 0x8f33b7e]  ; rip->0x97ba0e8 (file 0x93ba0e8)
0x88656a  (f 0x48656a)  e851b9fe06             call 0x7871ec0
0x88656f  (f 0x48656f)  e949feffff             jmp 0x8863bd
0x886574  (f 0x486574)  e8e7b8fe06             call 0x7871e60
0x886579  (f 0x486579)  0f0b                   ud2 
--- calls: [('0xc863de', '0x4298a0'), ('0xc86416', '0x463ef0'), ('0xc86427', '0x466fa0'), ('0xc86440', '0x466d40'), ('0xc86456', '0x466fa0'), ('0xc86484', '0x6fb520'), ('0xc86497', '0x463ef0'), ('0xc864a7', '0x4672f0'), ('0xc864d1', '0x6fb520'), ('0xc864df', '0x4b7a90'), ('0xc864ed', '0x4b7a90'), ('0xc864fb', '0x4b7a90'), ('0xc86574', '0x7871e60'), ('0xc86526', '0x7871eb0'), ('0xc8655e', '0x7871ed0'), ('0xc8656a', '0x7871ec0')]
