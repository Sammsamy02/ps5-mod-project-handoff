0x720960  (f 0x320960)  55                     push rbp
0x720961  (f 0x320961)  4889e5                 mov rbp, rsp
0x720964  (f 0x320964)  4157                   push r15
0x720966  (f 0x320966)  4156                   push r14
0x720968  (f 0x320968)  53                     push rbx
0x720969  (f 0x320969)  4881ecf8010000         sub rsp, 0x1f8
0x720970  (f 0x320970)  4c8b3d9115e108         mov r15, qword ptr [rip + 0x8e11591]  ; rip->0x9531f08 (file 0x9131f08)
0x720977  (f 0x320977)  498b07                 mov rax, qword ptr [r15]
0x72097a  (f 0x32097a)  488945e0               mov qword ptr [rbp - 0x20], rax
0x72097e  (f 0x32097e)  488b0593da0709         mov rax, qword ptr [rip + 0x907da93]  ; rip->0x979e418 (file 0x939e418)
0x720985  (f 0x320985)  4885c0                 test rax, rax
0x720988  (f 0x320988)  0f8538010000           jne 0x720ac6
0x72098e  (f 0x32098e)  48833d6ada070900       cmp qword ptr [rip + 0x907da6a], 0  ; rip->0x979e400 (file 0x939e400)
0x720996  (f 0x320996)  750c                   jne 0x7209a4
0x720998  (f 0x320998)  e8539affff             call 0x71a3f0
0x72099d  (f 0x32099d)  4889055cda0709         mov qword ptr [rip + 0x907da5c], rax  ; rip->0x979e400 (file 0x939e400)
0x7209a4  (f 0x3209a4)  e847010000             call 0x720af0
0x7209a9  (f 0x3209a9)  4889c7                 mov rdi, rax
0x7209ac  (f 0x3209ac)  be01000000             mov esi, 1
0x7209b1  (f 0x3209b1)  4889c3                 mov rbx, rax
0x7209b4  (f 0x3209b4)  e837541600             call 0x885df0
0x7209b9  (f 0x3209b9)  488d051063ceff         lea rax, [rip - 0x319cf0]  ; rip->0x406cd0 (file 0x6cd0)
0x7209c0  (f 0x3209c0)  bf02000000             mov edi, 2
0x7209c5  (f 0x3209c5)  488983c0000000         mov qword ptr [rbx + 0xc0], rax
0x7209cc  (f 0x3209cc)  e84fce0700             call 0x79d820
0x7209d1  (f 0x3209d1)  bf02000000             mov edi, 2
0x7209d6  (f 0x3209d6)  488985f0fdffff         mov qword ptr [rbp - 0x210], rax
0x7209dd  (f 0x3209dd)  c785f8fdffff40000000   mov dword ptr [rbp - 0x208], 0x40
0x7209e7  (f 0x3209e7)  c685fcfdffff02         mov byte ptr [rbp - 0x204], 2
0x7209ee  (f 0x3209ee)  48c78500feffff00000000 mov qword ptr [rbp - 0x200], 0
0x7209f9  (f 0x3209f9)  e822ce0700             call 0x79d820
0x7209fe  (f 0x3209fe)  bf03000000             mov edi, 3
0x720a03  (f 0x320a03)  48898508feffff         mov qword ptr [rbp - 0x1f8], rax
0x720a0a  (f 0x320a0a)  c78510feffff48000000   mov dword ptr [rbp - 0x1f0], 0x48
0x720a14  (f 0x320a14)  c68514feffff02         mov byte ptr [rbp - 0x1ec], 2
0x720a1b  (f 0x320a1b)  48c78518feffff00000000 mov qword ptr [rbp - 0x1e8], 0
0x720a26  (f 0x320a26)  e8f5cd0700             call 0x79d820
0x720a2b  (f 0x320a2b)  4c8db538feffff         lea r14, [rbp - 0x1c8]
0x720a32  (f 0x320a32)  488d8d38feffff         lea rcx, [rbp - 0x1c8]
0x720a39  (f 0x320a39)  488d95f0fdffff         lea rdx, [rbp - 0x210]
0x720a40  (f 0x320a40)  beb0000000             mov esi, 0xb0
0x720a45  (f 0x320a45)  48898520feffff         mov qword ptr [rbp - 0x1e0], rax
0x720a4c  (f 0x320a4c)  c78528feffff90000000   mov dword ptr [rbp - 0x1d8], 0x90
0x720a56  (f 0x320a56)  c6852cfeffff03         mov byte ptr [rbp - 0x1d4], 3
0x720a5d  (f 0x320a5d)  48c78530feffff00000000 mov qword ptr [rbp - 0x1d0], 0
0x720a68  (f 0x320a68)  4c89f7                 mov rdi, r14
0x720a6b  (f 0x320a6b)  e830d20700             call 0x79dca0
0x720a70  (f 0x320a70)  4c89f7                 mov rdi, r14
0x720a73  (f 0x320a73)  31f6                   xor esi, esi
0x720a75  (f 0x320a75)  e8a6d90700             call 0x79e420
0x720a7a  (f 0x320a7a)  4889df                 mov rdi, rbx
0x720a7d  (f 0x320a7d)  4889c6                 mov rsi, rax
0x720a80  (f 0x320a80)  e82bdf0700             call 0x79e9b0
0x720a85  (f 0x320a85)  4c89f7                 mov rdi, r14
0x720a88  (f 0x320a88)  e833d30700             call 0x79ddc0
0x720a8d  (f 0x320a8d)  4c89f7                 mov rdi, r14
0x720a90  (f 0x320a90)  e86ba3efff             call 0x61ae00
0x720a95  (f 0x320a95)  488d05344cbb07         lea rax, [rip + 0x7bb4c34]  ; rip->0x82d56d0 (file 0x7ed56d0)
0x720a9c  (f 0x320a9c)  4889df                 mov rdi, rbx
0x720a9f  (f 0x320a9f)  4c89f6                 mov rsi, r14
0x720aa2  (f 0x320aa2)  31d2                   xor edx, edx
0x720aa4  (f 0x320aa4)  48898538feffff         mov qword ptr [rbp - 0x1c8], rax
0x720aab  (f 0x320aab)  488b03                 mov rax, qword ptr [rbx]
0x720aae  (f 0x320aae)  ff90d0020000           call qword ptr [rax + 0x2d0]
0x720ab4  (f 0x320ab4)  4c89f7                 mov rdi, r14
0x720ab7  (f 0x320ab7)  e854aaefff             call 0x61b510
0x720abc  (f 0x320abc)  4889d8                 mov rax, rbx
0x720abf  (f 0x320abf)  48891d52d90709         mov qword ptr [rip + 0x907d952], rbx  ; rip->0x979e418 (file 0x939e418)
0x720ac6  (f 0x320ac6)  498b0f                 mov rcx, qword ptr [r15]
0x720ac9  (f 0x320ac9)  483b4de0               cmp rcx, qword ptr [rbp - 0x20]
0x720acd  (f 0x320acd)  750e                   jne 0x720add
0x720acf  (f 0x320acf)  4881c4f8010000         add rsp, 0x1f8
0x720ad6  (f 0x320ad6)  5b                     pop rbx
0x720ad7  (f 0x320ad7)  415e                   pop r14
0x720ad9  (f 0x320ad9)  415f                   pop r15
0x720adb  (f 0x320adb)  5d                     pop rbp
0x720adc  (f 0x320adc)  c3                     ret 
0x720add  (f 0x320add)  e87e131507             call 0x7871e60
0x720ae2  (f 0x320ae2)  0f0b                   ud2 
--- calls: [('0xb20998', '0x71a3f0'), ('0xb209a4', '0x720af0'), ('0xb209b4', '0x885df0'), ('0xb209cc', '0x79d820'), ('0xb209f9', '0x79d820'), ('0xb20a26', '0x79d820'), ('0xb20a6b', '0x79dca0'), ('0xb20a75', '0x79e420'), ('0xb20a80', '0x79e9b0'), ('0xb20a88', '0x79ddc0'), ('0xb20a90', '0x61ae00'), ('0xb20ab7', '0x61b510'), ('0xb20add', '0x7871e60')]
