0x726340  (f 0x326340)  55                     push rbp
0x726341  (f 0x326341)  4889e5                 mov rbp, rsp
0x726344  (f 0x326344)  4157                   push r15
0x726346  (f 0x326346)  4156                   push r14
0x726348  (f 0x326348)  53                     push rbx
0x726349  (f 0x326349)  4881ecf8010000         sub rsp, 0x1f8
0x726350  (f 0x326350)  4c8b3db1bbe008         mov r15, qword ptr [rip + 0x8e0bbb1]  ; rip->0x9531f08 (file 0x9131f08)
0x726357  (f 0x326357)  498b07                 mov rax, qword ptr [r15]
0x72635a  (f 0x32635a)  488945e0               mov qword ptr [rbp - 0x20], rax
0x72635e  (f 0x32635e)  488b05f3810709         mov rax, qword ptr [rip + 0x90781f3]  ; rip->0x979e558 (file 0x939e558)
0x726365  (f 0x326365)  4885c0                 test rax, rax
0x726368  (f 0x326368)  0f8527010000           jne 0x726495
0x72636e  (f 0x32636e)  e8eda5ffff             call 0x720960
0x726373  (f 0x326373)  e848010000             call 0x7264c0
0x726378  (f 0x326378)  4889c7                 mov rdi, rax
0x72637b  (f 0x32637b)  be01000000             mov esi, 1
0x726380  (f 0x326380)  4889c3                 mov rbx, rax
0x726383  (f 0x326383)  e868fa1500             call 0x885df0
0x726388  (f 0x326388)  488d056139ffff         lea rax, [rip - 0xc69f]  ; rip->0x719cf0 (file 0x319cf0)
0x72638f  (f 0x32638f)  bf02000000             mov edi, 2
0x726394  (f 0x326394)  488983c0000000         mov qword ptr [rbx + 0xc0], rax
0x72639b  (f 0x32639b)  e880740700             call 0x79d820
0x7263a0  (f 0x3263a0)  bf02000000             mov edi, 2
0x7263a5  (f 0x3263a5)  488985f0fdffff         mov qword ptr [rbp - 0x210], rax
0x7263ac  (f 0x3263ac)  c785f8fdffff10010000   mov dword ptr [rbp - 0x208], 0x110
0x7263b6  (f 0x3263b6)  c685fcfdffff02         mov byte ptr [rbp - 0x204], 2
0x7263bd  (f 0x3263bd)  48c78500feffff00000000 mov qword ptr [rbp - 0x200], 0
0x7263c8  (f 0x3263c8)  e853740700             call 0x79d820
0x7263cd  (f 0x3263cd)  bf03000000             mov edi, 3
0x7263d2  (f 0x3263d2)  48898508feffff         mov qword ptr [rbp - 0x1f8], rax
0x7263d9  (f 0x3263d9)  c78510feffffe0000000   mov dword ptr [rbp - 0x1f0], 0xe0
0x7263e3  (f 0x3263e3)  c68514feffff02         mov byte ptr [rbp - 0x1ec], 2
0x7263ea  (f 0x3263ea)  48c78518feffff00000000 mov qword ptr [rbp - 0x1e8], 0
0x7263f5  (f 0x3263f5)  e826740700             call 0x79d820
0x7263fa  (f 0x3263fa)  4c8db538feffff         lea r14, [rbp - 0x1c8]
0x726401  (f 0x326401)  488d8d38feffff         lea rcx, [rbp - 0x1c8]
0x726408  (f 0x326408)  488d95f0fdffff         lea rdx, [rbp - 0x210]
0x72640f  (f 0x32640f)  be00020000             mov esi, 0x200
0x726414  (f 0x326414)  48898520feffff         mov qword ptr [rbp - 0x1e0], rax
0x72641b  (f 0x32641b)  c78528feffff00010000   mov dword ptr [rbp - 0x1d8], 0x100
0x726425  (f 0x326425)  c6852cfeffff03         mov byte ptr [rbp - 0x1d4], 3
0x72642c  (f 0x32642c)  48c78530feffff00000000 mov qword ptr [rbp - 0x1d0], 0
0x726437  (f 0x326437)  4c89f7                 mov rdi, r14
0x72643a  (f 0x32643a)  e861780700             call 0x79dca0
0x72643f  (f 0x32643f)  4c89f7                 mov rdi, r14
0x726442  (f 0x326442)  31f6                   xor esi, esi
0x726444  (f 0x326444)  e8d77f0700             call 0x79e420
0x726449  (f 0x326449)  4889df                 mov rdi, rbx
0x72644c  (f 0x32644c)  4889c6                 mov rsi, rax
0x72644f  (f 0x32644f)  e85c850700             call 0x79e9b0
0x726454  (f 0x326454)  4c89f7                 mov rdi, r14
0x726457  (f 0x326457)  e864790700             call 0x79ddc0
0x72645c  (f 0x32645c)  4c89f7                 mov rdi, r14
0x72645f  (f 0x32645f)  e89c49efff             call 0x61ae00
0x726464  (f 0x326464)  488d0565f2ba07         lea rax, [rip + 0x7baf265]  ; rip->0x82d56d0 (file 0x7ed56d0)
0x72646b  (f 0x32646b)  4889df                 mov rdi, rbx
0x72646e  (f 0x32646e)  4c89f6                 mov rsi, r14
0x726471  (f 0x326471)  31d2                   xor edx, edx
0x726473  (f 0x326473)  48898538feffff         mov qword ptr [rbp - 0x1c8], rax
0x72647a  (f 0x32647a)  488b03                 mov rax, qword ptr [rbx]
0x72647d  (f 0x32647d)  ff90d0020000           call qword ptr [rax + 0x2d0]
0x726483  (f 0x326483)  4c89f7                 mov rdi, r14
0x726486  (f 0x326486)  e88550efff             call 0x61b510
0x72648b  (f 0x32648b)  4889d8                 mov rax, rbx
0x72648e  (f 0x32648e)  48891dc3800709         mov qword ptr [rip + 0x90780c3], rbx  ; rip->0x979e558 (file 0x939e558)
0x726495  (f 0x326495)  498b0f                 mov rcx, qword ptr [r15]
0x726498  (f 0x326498)  483b4de0               cmp rcx, qword ptr [rbp - 0x20]
0x72649c  (f 0x32649c)  750e                   jne 0x7264ac
0x72649e  (f 0x32649e)  4881c4f8010000         add rsp, 0x1f8
0x7264a5  (f 0x3264a5)  5b                     pop rbx
0x7264a6  (f 0x3264a6)  415e                   pop r14
0x7264a8  (f 0x3264a8)  415f                   pop r15
0x7264aa  (f 0x3264aa)  5d                     pop rbp
0x7264ab  (f 0x3264ab)  c3                     ret 
0x7264ac  (f 0x3264ac)  e8afb91407             call 0x7871e60
0x7264b1  (f 0x3264b1)  0f0b                   ud2 
--- calls: [('0xb2636e', '0x720960'), ('0xb26373', '0x7264c0'), ('0xb26383', '0x885df0'), ('0xb2639b', '0x79d820'), ('0xb263c8', '0x79d820'), ('0xb263f5', '0x79d820'), ('0xb2643a', '0x79dca0'), ('0xb26444', '0x79e420'), ('0xb2644f', '0x79e9b0'), ('0xb26457', '0x79ddc0'), ('0xb2645f', '0x61ae00'), ('0xb26486', '0x61b510'), ('0xb264ac', '0x7871e60')]
