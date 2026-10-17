0x763990  (f 0x363990)  55                     push rbp
0x763991  (f 0x363991)  4889e5                 mov rbp, rsp
0x763994  (f 0x363994)  4156                   push r14
0x763996  (f 0x363996)  53                     push rbx
0x763997  (f 0x363997)  4883ec20               sub rsp, 0x20
0x76399b  (f 0x36399b)  4c8b3566e5dc08         mov r14, qword ptr [rip + 0x8dce566]  ; rip->0x9531f08 (file 0x9131f08)
0x7639a2  (f 0x3639a2)  4889fb                 mov rbx, rdi
0x7639a5  (f 0x3639a5)  4883c602               add rsi, 2
0x7639a9  (f 0x3639a9)  498b06                 mov rax, qword ptr [r14]
0x7639ac  (f 0x3639ac)  488945e8               mov qword ptr [rbp - 0x18], rax
0x7639b0  (f 0x3639b0)  48c70700000000         mov qword ptr [rdi], 0
0x7639b7  (f 0x3639b7)  48895708               mov qword ptr [rdi + 8], rdx
0x7639bb  (f 0x3639bb)  48894f10               mov qword ptr [rdi + 0x10], rcx
0x7639bf  (f 0x3639bf)  c7471800000000         mov dword ptr [rdi + 0x18], 0
0x7639c6  (f 0x3639c6)  4c894720               mov qword ptr [rdi + 0x20], r8
0x7639ca  (f 0x3639ca)  48c7472800000000       mov qword ptr [rdi + 0x28], 0
0x7639d2  (f 0x3639d2)  4c894f30               mov qword ptr [rdi + 0x30], r9
0x7639d6  (f 0x3639d6)  c7473800000000         mov dword ptr [rdi + 0x38], 0
0x7639dd  (f 0x3639dd)  488d7dd8               lea rdi, [rbp - 0x28]
0x7639e1  (f 0x3639e1)  ba01000000             mov edx, 1
0x7639e6  (f 0x3639e6)  e8a59bedff             call 0x63d590
0x7639eb  (f 0x3639eb)  488b45d8               mov rax, qword ptr [rbp - 0x28]
0x7639ef  (f 0x3639ef)  488903                 mov qword ptr [rbx], rax
0x7639f2  (f 0x3639f2)  0fb6053fb20309         movzx eax, byte ptr [rip + 0x903b23f]  ; rip->0x979ec38 (file 0x939ec38)
0x7639f9  (f 0x3639f9)  84c0                   test al, al
0x7639fb  (f 0x3639fb)  0f8483000000           je 0x763a84
0x763a01  (f 0x363a01)  8b0529b20309           mov eax, dword ptr [rip + 0x903b229]  ; rip->0x979ec30 (file 0x939ec30)
0x763a07  (f 0x363a07)  3b0527b20309           cmp eax, dword ptr [rip + 0x903b227]  ; rip->0x979ec34 (file 0x939ec34)
0x763a0d  (f 0x363a0d)  7518                   jne 0x763a27
0x763a0f  (f 0x363a0f)  488d3512b20309         lea rsi, [rip + 0x903b212]  ; rip->0x979ec28 (file 0x939ec28)
0x763a16  (f 0x363a16)  488d1517b20309         lea rdx, [rip + 0x903b217]  ; rip->0x979ec34 (file 0x939ec34)
0x763a1d  (f 0x363a1d)  bf08080000             mov edi, 0x808
--- calls: [('0xb639e6', '0x63d590')]
