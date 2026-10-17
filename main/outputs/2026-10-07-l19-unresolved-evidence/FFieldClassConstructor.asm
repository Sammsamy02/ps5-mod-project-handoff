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
0x763a22  (f 0x363a22)  e8795eccff             call 0x4298a0
0x763a27  (f 0x363a27)  8d4801                 lea ecx, [rax + 1]
0x763a2a  (f 0x363a2a)  4898                   cdqe 
0x763a2c  (f 0x363a2c)  890dfeb10309           mov dword ptr [rip + 0x903b1fe], ecx  ; rip->0x979ec30 (file 0x939ec30)
0x763a32  (f 0x363a32)  488b0defb10309         mov rcx, qword ptr [rip + 0x903b1ef]  ; rip->0x979ec28 (file 0x939ec28)
0x763a39  (f 0x363a39)  48891cc1               mov qword ptr [rcx + rax*8], rbx
0x763a3d  (f 0x363a3d)  0fb6054cb20309         movzx eax, byte ptr [rip + 0x903b24c]  ; rip->0x979ec90 (file 0x939ec90)
0x763a44  (f 0x363a44)  84c0                   test al, al
0x763a46  (f 0x363a46)  0f8483000000           je 0x763acf
0x763a4c  (f 0x363a4c)  488d45d0               lea rax, [rbp - 0x30]
0x763a50  (f 0x363a50)  488d3de9b10309         lea rdi, [rip + 0x903b1e9]  ; rip->0x979ec40 (file 0x939ec40)
0x763a57  (f 0x363a57)  488d75d8               lea rsi, [rbp - 0x28]
0x763a5b  (f 0x363a5b)  48895dd8               mov qword ptr [rbp - 0x28], rbx
0x763a5f  (f 0x363a5f)  31d2                   xor edx, edx
0x763a61  (f 0x363a61)  48895dd0               mov qword ptr [rbp - 0x30], rbx
0x763a65  (f 0x363a65)  488945e0               mov qword ptr [rbp - 0x20], rax
0x763a69  (f 0x363a69)  e8a20fcdff             call 0x434a10
0x763a6e  (f 0x363a6e)  498b06                 mov rax, qword ptr [r14]
0x763a71  (f 0x363a71)  483b45e8               cmp rax, qword ptr [rbp - 0x18]
0x763a75  (f 0x363a75)  0f85cf000000           jne 0x763b4a
0x763a7b  (f 0x363a7b)  4883c420               add rsp, 0x20
0x763a7f  (f 0x363a7f)  5b                     pop rbx
0x763a80  (f 0x363a80)  415e                   pop r14
0x763a82  (f 0x363a82)  5d                     pop rbp
0x763a83  (f 0x363a83)  c3                     ret 
0x763a84  (f 0x363a84)  488d3dadb10309         lea rdi, [rip + 0x903b1ad]  ; rip->0x979ec38 (file 0x939ec38)
0x763a8b  (f 0x363a8b)  e820e41007             call 0x7871eb0
0x763a90  (f 0x363a90)  85c0                   test eax, eax
0x763a92  (f 0x363a92)  0f8469ffffff           je 0x763a01
0x763a98  (f 0x363a98)  488d3589b10309         lea rsi, [rip + 0x903b189]  ; rip->0x979ec28 (file 0x939ec28)
0x763a9f  (f 0x363a9f)  488d3dda08caff         lea rdi, [rip - 0x35f726]  ; rip->0x404380 (file 0x4380)
0x763aa6  (f 0x363aa6)  488d155385f708         lea rdx, [rip + 0x8f78553]  ; rip->0x96dc000 (file 0x92dc000)
0x763aad  (f 0x363aad)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x763ab1  (f 0x363ab1)  c5f811056fb10309       vmovups xmmword ptr [rip + 0x903b16f], xmm0  ; rip->0x979ec28 (file 0x939ec28)
0x763ab9  (f 0x363ab9)  e812e41007             call 0x7871ed0
0x763abe  (f 0x363abe)  488d3d73b10309         lea rdi, [rip + 0x903b173]  ; rip->0x979ec38 (file 0x939ec38)
0x763ac5  (f 0x363ac5)  e8f6e31007             call 0x7871ec0
0x763aca  (f 0x363aca)  e932ffffff             jmp 0x763a01
0x763acf  (f 0x363acf)  488d3dbab10309         lea rdi, [rip + 0x903b1ba]  ; rip->0x979ec90 (file 0x939ec90)
0x763ad6  (f 0x363ad6)  e8d5e31007             call 0x7871eb0
0x763adb  (f 0x363adb)  85c0                   test eax, eax
0x763add  (f 0x363add)  0f8469ffffff           je 0x763a4c
0x763ae3  (f 0x363ae3)  c5f8100d358f6d07       vmovups xmm1, xmmword ptr [rip + 0x76d8f35]  ; rip->0x7e3ca20 (file 0x7a3ca20)
0x763aeb  (f 0x363aeb)  488d354eb10309         lea rsi, [rip + 0x903b14e]  ; rip->0x979ec40 (file 0x939ec40)
0x763af2  (f 0x363af2)  488d3d0743cbff         lea rdi, [rip - 0x34bcf9]  ; rip->0x417e00 (file 0x17e00)
0x763af9  (f 0x363af9)  488d150085f708         lea rdx, [rip + 0x8f78500]  ; rip->0x96dc000 (file 0x92dc000)
0x763b00  (f 0x363b00)  48c70555b1030900000000 mov qword ptr [rip + 0x903b155], 0  ; rip->0x979ec60 (file 0x939ec60)
0x763b0b  (f 0x363b0b)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x763b0f  (f 0x363b0f)  c5f8110529b10309       vmovups xmmword ptr [rip + 0x903b129], xmm0  ; rip->0x979ec40 (file 0x939ec40)
0x763b17  (f 0x363b17)  c5f8110d49b10309       vmovups xmmword ptr [rip + 0x903b149], xmm1  ; rip->0x979ec68 (file 0x939ec68)
0x763b1f  (f 0x363b1f)  48c70556b1030900000000 mov qword ptr [rip + 0x903b156], 0  ; rip->0x979ec80 (file 0x939ec80)
0x763b2a  (f 0x363b2a)  c70554b1030900000000   mov dword ptr [rip + 0x903b154], 0  ; rip->0x979ec88 (file 0x939ec88)
0x763b34  (f 0x363b34)  e897e31007             call 0x7871ed0
0x763b39  (f 0x363b39)  488d3d50b10309         lea rdi, [rip + 0x903b150]  ; rip->0x979ec90 (file 0x939ec90)
0x763b40  (f 0x363b40)  e87be31007             call 0x7871ec0
0x763b45  (f 0x363b45)  e902ffffff             jmp 0x763a4c
0x763b4a  (f 0x363b4a)  e811e31007             call 0x7871e60
0x763b4f  (f 0x363b4f)  0f0b                   ud2 
--- calls: [('0xb639e6', '0x63d590'), ('0xb63a22', '0x4298a0'), ('0xb63a69', '0x434a10'), ('0xb63b4a', '0x7871e60'), ('0xb63ad6', '0x7871eb0'), ('0xb63b34', '0x7871ed0'), ('0xb63b40', '0x7871ec0'), ('0xb63a8b', '0x7871eb0'), ('0xb63ab9', '0x7871ed0'), ('0xb63ac5', '0x7871ec0')]
