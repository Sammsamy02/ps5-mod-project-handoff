0x2fac8c0 file0x2fb08c0 55 push rbp
0x2fac8c1 file0x2fb08c1 4889e5 mov rbp, rsp
0x2fac8c4 file0x2fb08c4 4157 push r15
0x2fac8c6 file0x2fb08c6 4156 push r14
0x2fac8c8 file0x2fb08c8 4155 push r13
0x2fac8ca file0x2fb08ca 4154 push r12
0x2fac8cc file0x2fb08cc 53 push rbx
0x2fac8cd file0x2fb08cd 4881ec88000000 sub rsp, 0x88
0x2fac8d4 file0x2fb08d4 4c8b2d2d561806 mov r13, qword ptr [rip + 0x618562d] rip->0x9131f08
0x2fac8db file0x2fb08db c5f057c9 vxorps xmm1, xmm1, xmm1
0x2fac8df file0x2fb08df c5f82ec8 vucomiss xmm1, xmm0
0x2fac8e3 file0x2fb08e3 4189f7 mov r15d, esi
0x2fac8e6 file0x2fb08e6 4989fe mov r14, rdi
0x2fac8e9 file0x2fb08e9 498b4500 mov rax, qword ptr [r13]
0x2fac8ed file0x2fb08ed 488945d0 mov qword ptr [rbp - 0x30], rax
0x2fac8f1 file0x2fb08f1 c5fa114584 vmovss dword ptr [rbp - 0x7c], xmm0
0x2fac8f6 file0x2fb08f6 0f870f080000 ja 0x2fad10b
0x2fac8fc file0x2fb08fc 4c8d25f6c03a06 lea r12, [rip + 0x63ac0f6] rip->0x93589f9
0x2fac903 file0x2fb0903 41803c2400 cmp byte ptr [r12], 0
0x2fac908 file0x2fb0908 7543 jne 0x2fac94d
0x2fac90a file0x2fb090a 0fb605b70d4106 movzx eax, byte ptr [rip + 0x6410db7] rip->0x93bd6c8
0x2fac911 file0x2fb0911 84c0 test al, al
0x2fac913 file0x2fb0913 0f843f090000 je 0x2fad258
0x2fac919 file0x2fb0919 41803c2401 cmp byte ptr [r12], 1
0x2fac91e file0x2fb091e 7517 jne 0x2fac937
0x2fac920 file0x2fb0920 488d05e1c03a06 lea rax, [rip + 0x63ac0e1] rip->0x9358a08
0x2fac927 file0x2fb0927 803801 cmp byte ptr [rax], 1
0x2fac92a file0x2fb092a 7521 jne 0x2fac94d
0x2fac92c file0x2fb092c f6058d0d410601 test byte ptr [rip + 0x6410d8d], 1 rip->0x93bd6c0
0x2fac933 file0x2fb0933 740b je 0x2fac940
0x2fac935 file0x2fb0935 eb16 jmp 0x2fac94d
0x2fac937 file0x2fb0937 803d820d410600 cmp byte ptr [rip + 0x6410d82], 0 rip->0x93bd6c0
0x2fac93e file0x2fb093e 750d jne 0x2fac94d
0x2fac940 file0x2fb0940 4c89f7 mov rdi, r14
0x2fac943 file0x2fb0943 e8a8708900 call 0x38439f0
0x2fac948 file0x2fb0948 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2fac94d file0x2fb094d 488d05b6c03a06 lea rax, [rip + 0x63ac0b6] rip->0x9358a0a
0x2fac954 file0x2fb0954 803801 cmp byte ptr [rax], 1
0x2fac957 file0x2fb0957 750e jne 0x2fac967
0x2fac959 file0x2fb0959 4983be100c000000 cmp qword ptr [r14 + 0xc10], 0
0x2fac961 file0x2fb0961 0f84c0050000 je 0x2facf27
0x2fac967 file0x2fb0967 498bbe100c0000 mov rdi, qword ptr [r14 + 0xc10]
0x2fac96e file0x2fb096e 4885ff test rdi, rdi
0x2fac971 file0x2fb0971 7409 je 0x2fac97c
0x2fac973 file0x2fb0973 488b07 mov rax, qword ptr [rdi]
0x2fac976 file0x2fb0976 ff9020040000 call qword ptr [rax + 0x420]
0x2fac97c file0x2fb097c 0fb6059d495706 movzx eax, byte ptr [rip + 0x657499d] rip->0x9521320
0x2fac983 file0x2fb0983 84c0 test al, al
0x2fac985 file0x2fb0985 0f8490080000 je 0x2fad21b
0x2fac98b file0x2fb098b 488b1d86495706 mov rbx, qword ptr [rip + 0x6574986] rip->0x9521318
0x2fac992 file0x2fb0992 e8e91f26fd call 0x20e980
0x2fac997 file0x2fb0997 4889c7 mov rdi, rax
0x2fac99a file0x2fb099a 4889de mov rsi, rbx
0x2fac99d file0x2fb099d 31d2 xor edx, edx
0x2fac99f file0x2fb099f e87c3026fd call 0x20fa20
0x2fac9a4 file0x2fb09a4 48898568ffffff mov qword ptr [rbp - 0x98], rax
0x2fac9ab file0x2fb09ab 4885c0 test rax, rax
0x2fac9ae file0x2fb09ae 746d je 0x2faca1d
0x2fac9b0 file0x2fb09b0 4584ff test r15b, r15b
0x2fac9b3 file0x2fb09b3 7558 jne 0x2faca0d
0x2fac9b5 file0x2fb09b5 418b8650110000 mov eax, dword ptr [r14 + 0x1150]
0x2fac9bc file0x2fb09bc 85c0 test eax, eax
0x2fac9be file0x2fb09be 7e4d jle 0x2faca0d
0x2fac9c0 file0x2fb09c0 31db xor ebx, ebx
0x2fac9c2 file0x2fb09c2 eb17 jmp 0x2fac9db
0x2fac9d0 file0x2fb09d0 48ffc3 inc rbx
0x2fac9d3 file0x2fb09d3 4863c8 movsxd rcx, eax
0x2fac9d6 file0x2fb09d6 4839cb cmp rbx, rcx
0x2fac9d9 file0x2fb09d9 7d32 jge 0x2faca0d
0x2fac9db file0x2fb09db 498b8e48110000 mov rcx, qword ptr [r14 + 0x1148]
0x2fac9e2 file0x2fb09e2 488b0cd9 mov rcx, qword ptr [rcx + rbx*8]
0x2fac9e6 file0x2fb09e6 488bb9c0020000 mov rdi, qword ptr [rcx + 0x2c0]
0x2fac9ed file0x2fb09ed 4885ff test rdi, rdi
0x2fac9f0 file0x2fb09f0 74de je 0x2fac9d0
0x2fac9f2 file0x2fb09f2 f6878e01000010 test byte ptr [rdi + 0x18e], 0x10
0x2fac9f9 file0x2fb09f9 74d5 je 0x2fac9d0
0x2fac9fb file0x2fb09fb e890cc7e00 call 0x3799690
0x2faca00 file0x2fb0a00 84c0 test al, al
0x2faca02 file0x2fb0a02 7519 jne 0x2faca1d
0x2faca04 file0x2fb0a04 418b8650110000 mov eax, dword ptr [r14 + 0x1150]
0x2faca0b file0x2fb0a0b ebc3 jmp 0x2fac9d0
0x2faca0d file0x2fb0a0d 488bbd68ffffff mov rdi, qword ptr [rbp - 0x98]
0x2faca14 file0x2fb0a14 488b07 mov rax, qword ptr [rdi]
0x2faca17 file0x2fb0a17 ff90f8000000 call qword ptr [rax + 0xf8]
0x2faca1d file0x2fb0a1d 488d0524e65606 lea rax, [rip + 0x656e624] rip->0x951b048
0x2faca24 file0x2fb0a24 833800 cmp dword ptr [rax], 0
0x2faca27 file0x2fb0a27 752d jne 0x2faca56
0x2faca29 file0x2fb0a29 488d0d94773306 lea rcx, [rip + 0x6337794] rip->0x92e41c4
0x2faca30 file0x2fb0a30 488d0515e65606 lea rax, [rip + 0x656e615] rip->0x951b04c
0x2faca37 file0x2fb0a37 31ff xor edi, edi
0x2faca39 file0x2fb0a39 c5fa1001 vmovss xmm0, dword ptr [rcx]
0x2faca3d file0x2fb0a3d c5fa5e0d33c0a904 vdivss xmm1, xmm0, dword ptr [rip + 0x4a9c033] rip->0x7a48a78
0x2faca45 file0x2fb0a45 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2faca4a file0x2fb0a4a 833800 cmp dword ptr [rax], 0
0x2faca4d file0x2fb0a4d 400f95c7 setne dil
0x2faca51 file0x2fb0a51 e80aee4dfd call 0x48b860
0x2faca56 file0x2fb0a56 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2faca5b file0x2fb0a5b e830e805fd call 0xb290
0x2faca60 file0x2fb0a60 488d05d12c3a06 lea rax, [rip + 0x63a2cd1] rip->0x934f738
0x2faca67 file0x2fb0a67 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2faca6c file0x2fb0a6c 488b38 mov rdi, qword ptr [rax]
0x2faca6f file0x2fb0a6f e88cc21efd call 0x198d00
0x2faca74 file0x2fb0a74 49638650110000 movsxd rax, dword ptr [r14 + 0x1150]
0x2faca7b file0x2fb0a7b 4885c0 test rax, rax
0x2faca7e file0x2fb0a7e 0f8ebf020000 jle 0x2facd43
0x2faca84 file0x2fb0a84 488d15a5475a06 lea rdx, [rip + 0x65a47a5] rip->0x9551230
0x2faca8b file0x2fb0a8b c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2faca90 file0x2fb0a90 498b8e48110000 mov rcx, qword ptr [r14 + 0x1148]
0x2faca97 file0x2fb0a97 4531c0 xor r8d, r8d
0x2faca9a file0x2fb0a9a 31f6 xor esi, esi
0x2faca9c file0x2fb0a9c 488b12 mov rdx, qword ptr [rdx]
0x2faca9f file0x2fb0a9f 90 nop 
0x2facaa0 file0x2fb0aa0 488b3cf1 mov rdi, qword ptr [rcx + rsi*8]
0x2facaa4 file0x2fb0aa4 483997c0020000 cmp qword ptr [rdi + 0x2c0], rdx
0x2facaab file0x2fb0aab 0f84be020000 je 0x2facd6f
0x2facab1 file0x2fb0ab1 48ffc6 inc rsi
0x2facab4 file0x2fb0ab4 4839f0 cmp rax, rsi
0x2facab7 file0x2fb0ab7 75e7 jne 0x2facaa0
0x2facab9 file0x2fb0ab9 4c898558ffffff mov qword ptr [rbp - 0xa8], r8
0x2facac0 file0x2fb0ac0 85c0 test eax, eax
0x2facac2 file0x2fb0ac2 0f8ebd020000 jle 0x2facd85
0x2facac8 file0x2fb0ac8 31db xor ebx, ebx
0x2facaca file0x2fb0aca 48c78578ffffff00000000 mov qword ptr [rbp - 0x88], 0
0x2facad5 file0x2fb0ad5 4489bd74ffffff mov dword ptr [rbp - 0x8c], r15d
0x2facadc file0x2fb0adc eb1a jmp 0x2facaf8
0x2facae0 file0x2fb0ae0 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2facae5 file0x2fb0ae5 49638650110000 movsxd rax, dword ptr [r14 + 0x1150]
0x2facaec file0x2fb0aec 48ffc3 inc rbx
0x2facaef file0x2fb0aef 4839c3 cmp rbx, rax
0x2facaf2 file0x2fb0af2 0f8d98020000 jge 0x2facd90
0x2facaf8 file0x2fb0af8 498b8648110000 mov rax, qword ptr [r14 + 0x1148]
0x2facaff file0x2fb0aff 4c8b2cd8 mov r13, qword ptr [rax + rbx*8]
0x2facb03 file0x2fb0b03 498b85c0020000 mov rax, qword ptr [r13 + 0x2c0]
0x2facb0a file0x2fb0b0a 4885c0 test rax, rax
0x2facb0d file0x2fb0b0d 74d6 je 0x2facae5
0x2facb0f file0x2fb0b0f f6808e01000010 test byte ptr [rax + 0x18e], 0x10
0x2facb16 file0x2fb0b16 74cd je 0x2facae5
0x2facb18 file0x2fb0b18 488d0d11475a06 lea rcx, [rip + 0x65a4711] rip->0x9551230
0x2facb1f file0x2fb0b1f 4c89f7 mov rdi, r14
0x2facb22 file0x2fb0b22 4c89ee mov rsi, r13
0x2facb25 file0x2fb0b25 488901 mov qword ptr [rcx], rax
0x2facb28 file0x2fb0b28 498b06 mov rax, qword ptr [r14]
0x2facb2b file0x2fb0b2b ff90f0040000 call qword ptr [rax + 0x4f0]
0x2facb31 file0x2fb0b31 4584ff test r15b, r15b
0x2facb34 file0x2fb0b34 7516 jne 0x2facb4c
0x2facb36 file0x2fb0b36 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2facb3b file0x2fb0b3b 498bbdc0020000 mov rdi, qword ptr [r13 + 0x2c0]
0x2facb42 file0x2fb0b42 be02000000 mov esi, 2
0x2facb47 file0x2fb0b47 e8e4221f00 call 0x319ee30
0x2facb4c file0x2fb0b4c 41803c2400 cmp byte ptr [r12], 0
0x2facb51 file0x2fb0b51 7534 jne 0x2facb87
0x2facb53 file0x2fb0b53 498bbdc0020000 mov rdi, qword ptr [r13 + 0x2c0]
0x2facb5a file0x2fb0b5a e8318e7e00 call 0x3795990
0x2facb5f file0x2fb0b5f 84c0 test al, al
0x2facb61 file0x2fb0b61 7424 je 0x2facb87
0x2facb63 file0x2fb0b63 498bbdc0020000 mov rdi, qword ptr [r13 + 0x2c0]
0x2facb6a file0x2fb0b6a e8c1ddedff call 0x2e8a930
0x2facb6f file0x2fb0b6f 498bbdc0020000 mov rdi, qword ptr [r13 + 0x2c0]
0x2facb76 file0x2fb0b76 41b801000000 mov r8d, 1
0x2facb7c file0x2fb0b7c 31f6 xor esi, esi
0x2facb7e file0x2fb0b7e 31d2 xor edx, edx
0x2facb80 file0x2fb0b80 31c9 xor ecx, ecx
0x2facb82 file0x2fb0b82 e899bbe8ff call 0x2e38720
0x2facb87 file0x2fb0b87 498bb5c0020000 mov rsi, qword ptr [r13 + 0x2c0]
0x2facb8e file0x2fb0b8e 8b868b010000 mov eax, dword ptr [rsi + 0x18b]
0x2facb94 file0x2fb0b94 a801 test al, 1
0x2facb96 file0x2fb0b96 0f8411010000 je 0x2faccad
0x2facb9c file0x2fb0b9c 83e0fe and eax, 0xfffffffe
0x2facb9f file0x2fb0b9f 498dbdc0000000 lea rdi, [r13 + 0xc0]
0x2facba6 file0x2fb0ba6 31d2 xor edx, edx
0x2facba8 file0x2fb0ba8 89868b010000 mov dword ptr [rsi + 0x18b], eax
0x2facbae file0x2fb0bae 488d35c52d7904 lea rsi, [rip + 0x4792dc5] rip->0x773f97a
0x2facbb5 file0x2fb0bb5 e826d97700 call 0x372a4e0
0x2facbba file0x2fb0bba 4989c7 mov r15, rax
0x2facbbd file0x2fb0bbd 498b8508020000 mov rax, qword ptr [r13 + 0x208]
0x2facbc4 file0x2fb0bc4 4885c0 test rax, rax
0x2facbc7 file0x2fb0bc7 0f84bd000000 je 0x2facc8a
0x2facbcd file0x2fb0bcd 83784000 cmp dword ptr [rax + 0x40], 0
0x2facbd1 file0x2fb0bd1 0f8eb3000000 jle 0x2facc8a
0x2facbd7 file0x2fb0bd7 488d0d7e223f06 lea rcx, [rip + 0x63f227e] rip->0x939ee5c
0x2facbde file0x2fb0bde 488b4038 mov rax, qword ptr [rax + 0x38]
0x2facbe2 file0x2fb0be2 803901 cmp byte ptr [rcx], 1
0x2facbe5 file0x2fb0be5 4c8b20 mov r12, qword ptr [rax]
0x2facbe8 file0x2fb0be8 750d jne 0x2facbf7
0x2facbea file0x2fb0bea 4d85e4 test r12, r12
0x2facbed file0x2fb0bed 7408 je 0x2facbf7
0x2facbef file0x2fb0bef 4c89e7 mov rdi, r12
0x2facbf2 file0x2fb0bf2 e8c9b63bfd call 0x3682c0
0x2facbf7 file0x2fb0bf7 4d85ff test r15, r15
0x2facbfa file0x2fb0bfa 0f848a000000 je 0x2facc8a
0x2facc00 file0x2fb0c00 4d85e4 test r12, r12
0x2facc03 file0x2fb0c03 0f8481000000 je 0x2facc8a
0x2facc09 file0x2fb0c09 498b0424 mov rax, qword ptr [r12]
0x2facc0d file0x2fb0c0d 4c89e7 mov rdi, r12
0x2facc10 file0x2fb0c10 ff9088010000 call qword ptr [rax + 0x188]
0x2facc16 file0x2fb0c16 488d7d88 lea rdi, [rbp - 0x78]
0x2facc1a file0x2fb0c1a 488d3598e3b404 lea rsi, [rip + 0x4b4e398] rip->0x7afafb9
0x2facc21 file0x2fb0c21 48898560ffffff mov qword ptr [rbp - 0xa0], rax
0x2facc28 file0x2fb0c28 e823710bfd call 0x63d50
0x2facc2d file0x2fb0c2d 488d7da0 lea rdi, [rbp - 0x60]
0x2facc31 file0x2fb0c31 488d7588 lea rsi, [rbp - 0x78]
0x2facc35 file0x2fb0c35 4c89fa mov rdx, r15
0x2facc38 file0x2fb0c38 e843aa0bfd call 0x67680
0x2facc3d file0x2fb0c3d 837da800 cmp dword ptr [rbp - 0x58], 0
0x2facc41 file0x2fb0c41 4c8d3d58296804 lea r15, [rip + 0x4682958] rip->0x762f5a0
0x2facc48 file0x2fb0c48 7404 je 0x2facc4e
0x2facc4a file0x2fb0c4a 4c8b7da0 mov r15, qword ptr [rbp - 0x60]
0x2facc4e file0x2fb0c4e e82d8122fd call 0x1d4d80
0x2facc53 file0x2fb0c53 4d8b0424 mov r8, qword ptr [r12]
0x2facc57 file0x2fb0c57 488bb560ffffff mov rsi, qword ptr [rbp - 0xa0]
0x2facc5e file0x2fb0c5e 4c89e7 mov rdi, r12
0x2facc61 file0x2fb0c61 4c89fa mov rdx, r15
0x2facc64 file0x2fb0c64 4889c1 mov rcx, rax
0x2facc67 file0x2fb0c67 41ff90b8020000 call qword ptr [r8 + 0x2b8]
0x2facc6e file0x2fb0c6e 488b7da0 mov rdi, qword ptr [rbp - 0x60]
0x2facc72 file0x2fb0c72 4885ff test rdi, rdi
0x2facc75 file0x2fb0c75 7405 je 0x2facc7c
0x2facc77 file0x2fb0c77 e814ae10fd call 0xb7a90
0x2facc7c file0x2fb0c7c 488b7d88 mov rdi, qword ptr [rbp - 0x78]
0x2facc80 file0x2fb0c80 4885ff test rdi, rdi
0x2facc83 file0x2fb0c83 7405 je 0x2facc8a
0x2facc85 file0x2fb0c85 e806ae10fd call 0xb7a90
0x2facc8a file0x2fb0c8a 498b85c0020000 mov rax, qword ptr [r13 + 0x2c0]
0x2facc91 file0x2fb0c91 448bbd74ffffff mov r15d, dword ptr [rbp - 0x8c]
0x2facc98 file0x2fb0c98 4c8d255abd3a06 lea r12, [rip + 0x63abd5a] rip->0x93589f9
0x2facc9f file0x2fb0c9f 83888b01000002 or dword ptr [rax + 0x18b], 2
0x2facca6 file0x2fb0ca6 498bb5c0020000 mov rsi, qword ptr [r13 + 0x2c0]
0x2faccad file0x2fb0cad 4c89f7 mov rdi, r14
0x2faccb0 file0x2fb0cb0 e8fb388a00 call 0x38505b0
0x2faccb5 file0x2fb0cb5 498bb5c0020000 mov rsi, qword ptr [r13 + 0x2c0]
0x2faccbc file0x2fb0cbc f6868c01000040 test byte ptr [rsi + 0x18c], 0x40
0x2faccc3 file0x2fb0cc3 7419 je 0x2faccde
0x2faccc5 file0x2fb0cc5 4c89f7 mov rdi, r14
0x2faccc8 file0x2fb0cc8 e8f3378a00 call 0x38504c0
0x2facccd file0x2fb0ccd 498b85c0020000 mov rax, qword ptr [r13 + 0x2c0]
0x2faccd4 file0x2fb0cd4 81a08b010000ffbfffff and dword ptr [rax + 0x18b], 0xffffbfff
0x2faccde file0x2fb0cde 488d0526bd3a06 lea rax, [rip + 0x63abd26] rip->0x9358a0b
0x2facce5 file0x2fb0ce5 803800 cmp byte ptr [rax], 0
0x2facce8 file0x2fb0ce8 7420 je 0x2facd0a
0x2faccea file0x2fb0cea 488d0557e35606 lea rax, [rip + 0x656e357] rip->0x951b048
0x2faccf1 file0x2fb0cf1 833800 cmp dword ptr [rax], 0
0x2faccf4 file0x2fb0cf4 7514 jne 0x2facd0a
0x2faccf6 file0x2fb0cf6 498bbdc0020000 mov rdi, qword ptr [r13 + 0x2c0]
0x2faccfd file0x2fb0cfd 488d75a0 lea rsi, [rbp - 0x60]
0x2facd01 file0x2fb0d01 c645b000 mov byte ptr [rbp - 0x50], 0
0x2facd05 file0x2fb0d05 e8f67a7e00 call 0x3794800
0x2facd0a file0x2fb0d0a 4c89f7 mov rdi, r14
0x2facd0d file0x2fb0d0d 4c89ee mov rsi, r13
0x2facd10 file0x2fb0d10 e8bbd78900 call 0x384a4d0
0x2facd15 file0x2fb0d15 41807d0004 cmp byte ptr [r13], 4
0x2facd1a file0x2fb0d1a 0f84c0fdffff je 0x2facae0
0x2facd20 file0x2fb0d20 498bbdc0020000 mov rdi, qword ptr [r13 + 0x2c0]
0x2facd27 file0x2fb0d27 e814051f00 call 0x319d240
0x2facd2c file0x2fb0d2c 488b8d78ffffff mov rcx, qword ptr [rbp - 0x88]
0x2facd33 file0x2fb0d33 3401 xor al, 1
0x2facd35 file0x2fb0d35 08c1 or cl, al
0x2facd37 file0x2fb0d37 48898d78ffffff mov qword ptr [rbp - 0x88], rcx
0x2facd3e file0x2fb0d3e e99dfdffff jmp 0x2facae0
0x2facd43 file0x2fb0d43 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2facd48 file0x2fb0d48 be02000000 mov esi, 2
0x2facd4d file0x2fb0d4d 48c78578ffffff00000000 mov qword ptr [rbp - 0x88], 0
0x2facd58 file0x2fb0d58 31ff xor edi, edi
0x2facd5a file0x2fb0d5a 31d2 xor edx, edx
0x2facd5c file0x2fb0d5c e89ff07400 call 0x36fbe00
0x2facd61 file0x2fb0d61 488bbd68ffffff mov rdi, qword ptr [rbp - 0x98]
0x2facd68 file0x2fb0d68 4885ff test rdi, rdi
0x2facd6b file0x2fb0d6b 7569 jne 0x2facdd6
0x2facd6d file0x2fb0d6d eb70 jmp 0x2facddf
0x2facd6f file0x2fb0d6f 4c8b87a0000000 mov r8, qword ptr [rdi + 0xa0]
0x2facd76 file0x2fb0d76 4c898558ffffff mov qword ptr [rbp - 0xa8], r8
0x2facd7d file0x2fb0d7d 85c0 test eax, eax
0x2facd7f file0x2fb0d7f 0f8f43fdffff jg 0x2facac8
0x2facd85 file0x2fb0d85 48c78578ffffff00000000 mov qword ptr [rbp - 0x88], 0
0x2facd90 file0x2fb0d90 be02000000 mov esi, 2
0x2facd95 file0x2fb0d95 31ff xor edi, edi
0x2facd97 file0x2fb0d97 31d2 xor edx, edx
0x2facd99 file0x2fb0d99 e862f07400 call 0x36fbe00
0x2facd9e file0x2fb0d9e 488bb558ffffff mov rsi, qword ptr [rbp - 0xa8]
0x2facda5 file0x2fb0da5 4885f6 test rsi, rsi
0x2facda8 file0x2fb0da8 7419 je 0x2facdc3
0x2facdaa file0x2fb0daa 4c89f7 mov rdi, r14
0x2facdad file0x2fb0dad e80e3b8a00 call 0x38508c0
0x2facdb2 file0x2fb0db2 488b80c0020000 mov rax, qword ptr [rax + 0x2c0]
0x2facdb9 file0x2fb0db9 488d0d70445a06 lea rcx, [rip + 0x65a4470] rip->0x9551230
0x2facdc0 file0x2fb0dc0 488901 mov qword ptr [rcx], rax
0x2facdc3 file0x2fb0dc3 4c8b2d3e511806 mov r13, qword ptr [rip + 0x618513e] rip->0x9131f08
0x2facdca file0x2fb0dca 488bbd68ffffff mov rdi, qword ptr [rbp - 0x98]
0x2facdd1 file0x2fb0dd1 4885ff test rdi, rdi
0x2facdd4 file0x2fb0dd4 7409 je 0x2facddf
0x2facdd6 file0x2fb0dd6 488b07 mov rax, qword ptr [rdi]
0x2facdd9 file0x2fb0dd9 ff90e8000000 call qword ptr [rax + 0xe8]
0x2facddf file0x2fb0ddf 498bbe100c0000 mov rdi, qword ptr [r14 + 0xc10]
0x2facde6 file0x2fb0de6 4885ff test rdi, rdi
0x2facde9 file0x2fb0de9 0f94c0 sete al
0x2facdec file0x2fb0dec 4408f8 or al, r15b
0x2facdef file0x2fb0def 750e jne 0x2facdff
0x2facdf1 file0x2fb0df1 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2facdf6 file0x2fb0df6 488b07 mov rax, qword ptr [rdi]
0x2facdf9 file0x2fb0df9 ff9000040000 call qword ptr [rax + 0x400]
0x2facdff file0x2fb0dff e8bc1572fe call 0x16ce3c0
0x2face04 file0x2fb0e04 498b06 mov rax, qword ptr [r14]
0x2face07 file0x2fb0e07 4c89f7 mov rdi, r14
0x2face0a file0x2fb0e0a ff9050030000 call qword ptr [rax + 0x350]
0x2face10 file0x2fb0e10 89c3 mov ebx, eax
0x2face12 file0x2fb0e12 4584ff test r15b, r15b
0x2face15 file0x2fb0e15 750b jne 0x2face22
0x2face17 file0x2fb0e17 41803c2400 cmp byte ptr [r12], 0
0x2face1c file0x2fb0e1c 0f8492000000 je 0x2faceb4
0x2face22 file0x2fb0e22 4531ff xor r15d, r15d
0x2face25 file0x2fb0e25 488d051ce25606 lea rax, [rip + 0x656e21c] rip->0x951b048
0x2face2c file0x2fb0e2c 833800 cmp dword ptr [rax], 0
0x2face2f file0x2fb0e2f 7411 je 0x2face42
0x2face31 file0x2fb0e31 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2face36 file0x2fb0e36 498b06 mov rax, qword ptr [r14]
0x2face39 file0x2fb0e39 4c89f7 mov rdi, r14
0x2face3c file0x2fb0e3c ff90a0030000 call qword ptr [rax + 0x3a0]
0x2face42 file0x2fb0e42 4584ff test r15b, r15b
0x2face45 file0x2fb0e45 0f8429010000 je 0x2facf74
0x2face4b file0x2fb0e4b 84db test bl, bl
0x2face4d file0x2fb0e4d 0f8489000000 je 0x2facedc
0x2face53 file0x2fb0e53 488d05eee15606 lea rax, [rip + 0x656e1ee] rip->0x951b048
0x2face5a file0x2fb0e5a 833800 cmp dword ptr [rax], 0
0x2face5d file0x2fb0e5d 0f8511010000 jne 0x2facf74
0x2face63 file0x2fb0e63 498b86100c0000 mov rax, qword ptr [r14 + 0xc10]
0x2face6a file0x2fb0e6a 4885c0 test rax, rax
0x2face6d file0x2fb0e6d 0f8401010000 je 0x2facf74
0x2face73 file0x2fb0e73 488b80f0000000 mov rax, qword ptr [rax + 0xf0]
0x2face7a file0x2fb0e7a 4885c0 test rax, rax
0x2face7d file0x2fb0e7d 0f84f1000000 je 0x2facf74
0x2face83 file0x2fb0e83 488b7830 mov rdi, qword ptr [rax + 0x30]
0x2face87 file0x2fb0e87 4885ff test rdi, rdi
0x2face8a file0x2fb0e8a 0f84e4000000 je 0x2facf74
0x2face90 file0x2fb0e90 488b07 mov rax, qword ptr [rdi]
0x2face93 file0x2fb0e93 ff5038 call qword ptr [rax + 0x38]
0x2face96 file0x2fb0e96 4885c0 test rax, rax
0x2face99 file0x2fb0e99 0f84d5000000 je 0x2facf74
0x2face9f file0x2fb0e9f 488d75a0 lea rsi, [rbp - 0x60]
0x2facea3 file0x2fb0ea3 4889c7 mov rdi, rax
0x2facea6 file0x2fb0ea6 c645b000 mov byte ptr [rbp - 0x50], 0
0x2faceaa file0x2fb0eaa e851797e00 call 0x3794800
0x2faceaf file0x2fb0eaf e9c0000000 jmp 0x2facf74
0x2faceb4 file0x2fb0eb4 e8673205fd call 0x120
0x2faceb9 file0x2fb0eb9 4189c7 mov r15d, eax
0x2facebc file0x2fb0ebc 3401 xor al, 1
0x2facebe file0x2fb0ebe 08d8 or al, bl
0x2facec0 file0x2fb0ec0 0f855fffffff jne 0x2face25
0x2facec6 file0x2fb0ec6 498b06 mov rax, qword ptr [r14]
0x2facec9 file0x2fb0ec9 4c89f7 mov rdi, r14
0x2facecc file0x2fb0ecc be01000000 mov esi, 1
0x2faced1 file0x2fb0ed1 ff9000050000 call qword ptr [rax + 0x500]
0x2faced7 file0x2fb0ed7 e949ffffff jmp 0x2face25
0x2facedc file0x2fb0edc 833dd573330600 cmp dword ptr [rip + 0x63373d5], 0 rip->0x92e42b8
0x2facee3 file0x2fb0ee3 747e je 0x2facf63
0x2facee5 file0x2fb0ee5 4c89f7 mov rdi, r14
0x2facee8 file0x2fb0ee8 e8339a8900 call 0x3846920
0x2faceed file0x2fb0eed 84c0 test al, al
0x2faceef file0x2fb0eef 7472 je 0x2facf63
0x2facef1 file0x2fb0ef1 e82af610fd call 0xbc520
0x2facef6 file0x2fb0ef6 84c0 test al, al
0x2facef8 file0x2fb0ef8 0f8443020000 je 0x2fad141
0x2facefe file0x2fb0efe e80de0a5fd call 0xa0af10
0x2facf03 file0x2fb0f03 4889c7 mov rdi, rax
0x2facf06 file0x2fb0f06 be02000000 mov esi, 2
0x2facf0b file0x2fb0f0b 31d2 xor edx, edx
0x2facf0d file0x2fb0f0d e86e9d9bfd call 0x966c80
0x2facf12 file0x2fb0f12 488d052ff04306 lea rax, [rip + 0x643f02f] rip->0x93ebf48
0x2facf19 file0x2fb0f19 488b38 mov rdi, qword ptr [rax]
0x2facf1c file0x2fb0f1c 488b07 mov rax, qword ptr [rdi]
0x2facf1f file0x2fb0f1f ff9040030000 call qword ptr [rax + 0x340]
0x2facf25 file0x2fb0f25 eb4d jmp 0x2facf74
0x2facf27 file0x2fb0f27 0fb6059a074106 movzx eax, byte ptr [rip + 0x641079a] rip->0x93bd6c8
0x2facf2e file0x2fb0f2e 84c0 test al, al
0x2facf30 file0x2fb0f30 0f846b030000 je 0x2fad2a1
0x2facf36 file0x2fb0f36 41803c2401 cmp byte ptr [r12], 1
0x2facf3b file0x2fb0f3b 0f85aa010000 jne 0x2fad0eb
0x2facf41 file0x2fb0f41 488d05c0ba3a06 lea rax, [rip + 0x63abac0] rip->0x9358a08
0x2facf48 file0x2fb0f48 803801 cmp byte ptr [rax], 1
0x2facf4b file0x2fb0f4b 0f8516faffff jne 0x2fac967
0x2facf51 file0x2fb0f51 f6056807410601 test byte ptr [rip + 0x6410768], 1 rip->0x93bd6c0
0x2facf58 file0x2fb0f58 0f849a010000 je 0x2fad0f8
0x2facf5e file0x2fb0f5e e904faffff jmp 0x2fac967
0x2facf63 file0x2fb0f63 e89886fdff call 0x2f85600
0x2facf68 file0x2fb0f68 488b08 mov rcx, qword ptr [rax]
0x2facf6b file0x2fb0f6b 4889c7 mov rdi, rax
0x2facf6e file0x2fb0f6e ff9130010000 call qword ptr [rcx + 0x130]
0x2facf74 file0x2fb0f74 488d058fba3a06 lea rax, [rip + 0x63aba8f] rip->0x9358a0a
0x2facf7b file0x2fb0f7b 803801 cmp byte ptr [rax], 1
0x2facf7e file0x2fb0f7e 7515 jne 0x2facf95
0x2facf80 file0x2fb0f80 e83bc4f0ff call 0x2eb93c0
0x2facf85 file0x2fb0f85 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2facf8a file0x2fb0f8a 488b08 mov rcx, qword ptr [rax]
0x2facf8d file0x2fb0f8d 4889c7 mov rdi, rax
0x2facf90 file0x2fb0f90 31f6 xor esi, esi
0x2facf92 file0x2fb0f92 ff5110 call qword ptr [rcx + 0x10]
0x2facf95 file0x2fb0f95 488d057c695a06 lea rax, [rip + 0x65a697c] rip->0x9553918
0x2facf9c file0x2fb0f9c 488b38 mov rdi, qword ptr [rax]
0x2facf9f file0x2fb0f9f e8cc6b8900 call 0x3843b70
0x2facfa4 file0x2fb0fa4 4885c0 test rax, rax
0x2facfa7 file0x2fb0fa7 7412 je 0x2facfbb
0x2facfa9 file0x2fb0fa9 0fb6b578ffffff movzx esi, byte ptr [rbp - 0x88]
0x2facfb0 file0x2fb0fb0 4889c7 mov rdi, rax
0x2facfb3 file0x2fb0fb3 83e601 and esi, 1
0x2facfb6 file0x2fb0fb6 e8656ad1ff call 0x2cc3a20
0x2facfbb file0x2fb0fbb 488d05ff1f4506 lea rax, [rip + 0x6451fff] rip->0x93fefc1
0x2facfc2 file0x2fb0fc2 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2facfc7 file0x2fb0fc7 440fb638 movzx r15d, byte ptr [rax]
0x2facfcb file0x2fb0fcb 44887d88 mov byte ptr [rbp - 0x78], r15b
0x2facfcf file0x2fb0fcf c5fa11458c vmovss dword ptr [rbp - 0x74], xmm0
0x2facfd4 file0x2fb0fd4 e847f510fd call 0xbc520
0x2facfd9 file0x2fb0fd9 84c0 test al, al
0x2facfdb file0x2fb0fdb 744d je 0x2fad02a
0x2facfdd file0x2fb0fdd e82edfa5fd call 0xa0af10
0x2facfe2 file0x2fb0fe2 4989c6 mov r14, rax
0x2facfe5 file0x2fb0fe5 4584ff test r15b, r15b
0x2facfe8 file0x2fb0fe8 751a jne 0x2fad004
0x2facfea file0x2fb0fea c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2facfef file0x2fb0fef 488d05d21f4506 lea rax, [rip + 0x6451fd2] rip->0x93fefc8
0x2facff6 file0x2fb0ff6 c5fa1100 vmovss dword ptr [rax], xmm0
0x2facffa file0x2fb0ffa c5fa584004 vaddss xmm0, xmm0, dword ptr [rax + 4]
0x2facfff file0x2fb0fff c5fa114004 vmovss dword ptr [rax + 4], xmm0
0x2fad004 file0x2fb1004 488d3d95394506 lea rdi, [rip + 0x6453995] rip->0x94009a0
0x2fad00b file0x2fb100b e8e05aa6fd call 0xa12af0
0x2fad010 file0x2fb1010 e85b92a2fd call 0x9d6270
0x2fad015 file0x2fb1015 4c89f7 mov rdi, r14
0x2fad018 file0x2fb1018 e8937e9efd call 0x994eb0
0x2fad01d file0x2fb101d 84db test bl, bl
0x2fad01f file0x2fb101f 0f856f070000 jne 0x2fad794
0x2fad025 file0x2fb1025 e97b070000 jmp 0x2fad7a5
0x2fad02a file0x2fb102a 488d05a7354506 lea rax, [rip + 0x64535a7] rip->0x94005d8
0x2fad031 file0x2fb1031 803800 cmp byte ptr [rax], 0
0x2fad034 file0x2fb1034 750d jne 0x2fad043
0x2fad036 file0x2fb1036 e855c010fd call 0xb9090
0x2fad03b file0x2fb103b 84c0 test al, al
0x2fad03d file0x2fb103d 0f85ba010000 jne 0x2fad1fd
0x2fad043 file0x2fb1043 488d05c6354506 lea rax, [rip + 0x64535c6] rip->0x9400610
0x2fad04a file0x2fb104a 833800 cmp dword ptr [rax], 0
0x2fad04d file0x2fb104d 7471 je 0x2fad0c0
0x2fad04f file0x2fb104f 488b5588 mov rdx, qword ptr [rbp - 0x78]
0x2fad053 file0x2fb1053 488d3d36374506 lea rdi, [rip + 0x6453736] rip->0x9400790
0x2fad05a file0x2fb105a 488d05f7a44d05 lea rax, [rip + 0x54da4f7] rip->0x8487558
0x2fad061 file0x2fb1061 488d0d08810200 lea rcx, [rip + 0x28108] rip->0x2fd5170
0x2fad068 file0x2fb1068 48c745c800000000 mov qword ptr [rbp - 0x38], 0
0x2fad070 file0x2fb1070 488d35e1e57304 lea rsi, [rip + 0x473e5e1] rip->0x76eb658
0x2fad077 file0x2fb1077 488945b0 mov qword ptr [rbp - 0x50], rax
0x2fad07b file0x2fb107b 488955b8 mov qword ptr [rbp - 0x48], rdx
0x2fad07f file0x2fb107f 48894da0 mov qword ptr [rbp - 0x60], rcx
0x2fad083 file0x2fb1083 488d150a505706 lea rdx, [rip + 0x657500a] rip->0x9522094
0x2fad08a file0x2fb108a 488d4da0 lea rcx, [rbp - 0x60]
0x2fad08e file0x2fb108e e8cde0a5fd call 0xa0b160
0x2fad093 file0x2fb1093 48837da000 cmp qword ptr [rbp - 0x60], 0
0x2fad098 file0x2fb1098 0f84f2060000 je 0x2fad790
0x2fad09e file0x2fb109e 488b45c8 mov rax, qword ptr [rbp - 0x38]
0x2fad0a2 file0x2fb10a2 488d7db0 lea rdi, [rbp - 0x50]
0x2fad0a6 file0x2fb10a6 4885c0 test rax, rax
0x2fad0a9 file0x2fb10a9 480f45f8 cmovne rdi, rax
0x2fad0ad file0x2fb10ad 488b07 mov rax, qword ptr [rdi]
0x2fad0b0 file0x2fb10b0 ff5010 call qword ptr [rax + 0x10]
0x2fad0b3 file0x2fb10b3 84db test bl, bl
0x2fad0b5 file0x2fb10b5 0f85d9060000 jne 0x2fad794
0x2fad0bb file0x2fb10bb e9e5060000 jmp 0x2fad7a5
0x2fad0c0 file0x2fb10c0 66666664488b042500000000 mov rax, qword ptr fs:[0]
0x2fad0cc file0x2fb10cc 4989c7 mov r15, rax
0x2fad0cf file0x2fb10cf 4c8da078eeffff lea r12, [rax - 0x1188]
0x2fad0d6 file0x2fb10d6 488b8078eeffff mov rax, qword ptr [rax - 0x1188]
0x2fad0dd file0x2fb10dd 4885c0 test rax, rax
0x2fad0e0 file0x2fb10e0 0f853a040000 jne 0x2fad520
0x2fad0e6 file0x2fb10e6 e9d6030000 jmp 0x2fad4c1
0x2fad0eb file0x2fb10eb 803dce05410600 cmp byte ptr [rip + 0x64105ce], 0 rip->0x93bd6c0
0x2fad0f2 file0x2fb10f2 0f856ff8ffff jne 0x2fac967
0x2fad0f8 file0x2fb10f8 488d35c9fa8004 lea rsi, [rip + 0x480fac9] rip->0x77bcbc8
0x2fad0ff file0x2fb10ff 31ff xor edi, edi
0x2fad101 file0x2fb1101 e82a2806fd call 0xf930
0x2fad106 file0x2fb1106 e99a060000 jmp 0x2fad7a5
0x2fad10b file0x2fb110b 488d3d7fdeb404 lea rdi, [rip + 0x4b4de7f] rip->0x7afaf91
0x2fad112 file0x2fb1112 488d15ef1a5a04 lea rdx, [rip + 0x45a1aef] rip->0x754ec08
0x2fad119 file0x2fb1119 bee1060000 mov esi, 0x6e1
0x2fad11e file0x2fb111e 31c0 xor eax, eax
0x2fad120 file0x2fb1120 e88b7f1cfd call 0x1750b0
0x2fad125 file0x2fb1125 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2fad12a file0x2fb112a 4c8d25c8b83a06 lea r12, [rip + 0x63ab8c8] rip->0x93589f9
0x2fad131 file0x2fb1131 41803c2400 cmp byte ptr [r12], 0
0x2fad136 file0x2fb1136 0f84cef7ffff je 0x2fac90a
0x2fad13c file0x2fb113c e90cf8ffff jmp 0x2fac94d
0x2fad141 file0x2fb1141 488d0590344506 lea rax, [rip + 0x6453490] rip->0x94005d8
0x2fad148 file0x2fb1148 803800 cmp byte ptr [rax], 0
0x2fad14b file0x2fb114b 750d jne 0x2fad15a
0x2fad14d file0x2fb114d e83ebf10fd call 0xb9090
0x2fad152 file0x2fb1152 84c0 test al, al
0x2fad154 file0x2fb1154 0f85a4fdffff jne 0x2facefe
0x2fad15a file0x2fb115a 488d05af344506 lea rax, [rip + 0x64534af] rip->0x9400610
0x2fad161 file0x2fb1161 833800 cmp dword ptr [rax], 0
0x2fad164 file0x2fb1164 746c je 0x2fad1d2
0x2fad166 file0x2fb1166 488d3d23364506 lea rdi, [rip + 0x6453623] rip->0x9400790
0x2fad16d file0x2fb116d 488d056c41fa04 lea rax, [rip + 0x4fa416c] rip->0x7f512e0
0x2fad174 file0x2fb1174 488d15357f0200 lea rdx, [rip + 0x27f35] rip->0x2fd50b0
0x2fad17b file0x2fb117b 488d0dceb0a4fd lea rcx, [rip - 0x25b4f32] rip->0x9f8250
0x2fad182 file0x2fb1182 48c745c800000000 mov qword ptr [rbp - 0x38], 0
0x2fad18a file0x2fb118a 488d357d9f8304 lea rsi, [rip + 0x4839f7d] rip->0x77e710e
0x2fad191 file0x2fb1191 488945b0 mov qword ptr [rbp - 0x50], rax
0x2fad195 file0x2fb1195 488955b8 mov qword ptr [rbp - 0x48], rdx
0x2fad199 file0x2fb1199 48894da0 mov qword ptr [rbp - 0x60], rcx
0x2fad19d file0x2fb119d 488d15ec4e5706 lea rdx, [rip + 0x6574eec] rip->0x9522090
0x2fad1a4 file0x2fb11a4 488d4da0 lea rcx, [rbp - 0x60]
0x2fad1a8 file0x2fb11a8 e8b3dfa5fd call 0xa0b160
0x2fad1ad file0x2fb11ad 48837da000 cmp qword ptr [rbp - 0x60], 0
0x2fad1b2 file0x2fb11b2 0f84bcfdffff je 0x2facf74
0x2fad1b8 file0x2fb11b8 488b45c8 mov rax, qword ptr [rbp - 0x38]
0x2fad1bc file0x2fb11bc 488d7db0 lea rdi, [rbp - 0x50]
0x2fad1c0 file0x2fb11c0 4885c0 test rax, rax
0x2fad1c3 file0x2fb11c3 480f45f8 cmovne rdi, rax
0x2fad1c7 file0x2fb11c7 488b07 mov rax, qword ptr [rdi]
0x2fad1ca file0x2fb11ca ff5010 call qword ptr [rax + 0x10]
0x2fad1cd file0x2fb11cd e9a2fdffff jmp 0x2facf74
0x2fad1d2 file0x2fb11d2 66666664488b042500000000 mov rax, qword ptr fs:[0]
0x2fad1de file0x2fb11de 4989c6 mov r14, rax
0x2fad1e1 file0x2fb11e1 4c8db878eeffff lea r15, [rax - 0x1188]
0x2fad1e8 file0x2fb11e8 488b8078eeffff mov rax, qword ptr [rax - 0x1188]
0x2fad1ef file0x2fb11ef 4885c0 test rax, rax
0x2fad1f2 file0x2fb11f2 0f854f010000 jne 0x2fad347
0x2fad1f8 file0x2fb11f8 e9ed000000 jmp 0x2fad2ea
0x2fad1fd file0x2fb11fd e80edda5fd call 0xa0af10
0x2fad202 file0x2fb1202 488d7d88 lea rdi, [rbp - 0x78]
0x2fad206 file0x2fb1206 4889c6 mov rsi, rax
0x2fad209 file0x2fb1209 e8627f0200 call 0x2fd5170
0x2fad20e file0x2fb120e 84db test bl, bl
0x2fad210 file0x2fb1210 0f857e050000 jne 0x2fad794
0x2fad216 file0x2fb1216 e98a050000 jmp 0x2fad7a5
0x2fad21b file0x2fb121b 488d3dfe405706 lea rdi, [rip + 0x65740fe] rip->0x9521320
0x2fad222 file0x2fb1222 e8894c4c04 call 0x7471eb0
0x2fad227 file0x2fb1227 85c0 test eax, eax
0x2fad229 file0x2fb1229 0f845cf7ffff je 0x2fac98b
0x2fad22f file0x2fb122f 488d3de2405706 lea rdi, [rip + 0x65740e2] rip->0x9521318
0x2fad236 file0x2fb1236 488d3585066604 lea rsi, [rip + 0x4660685] rip->0x760d8c2
0x2fad23d file0x2fb123d ba01000000 mov edx, 1
0x2fad242 file0x2fb1242 e8490329fd call 0x23d590
0x2fad247 file0x2fb1247 488d3dd2405706 lea rdi, [rip + 0x65740d2] rip->0x9521320
0x2fad24e file0x2fb124e e86d4c4c04 call 0x7471ec0
0x2fad253 file0x2fb1253 e933f7ffff jmp 0x2fac98b
0x2fad258 file0x2fb1258 488d3d69044106 lea rdi, [rip + 0x6410469] rip->0x93bd6c8
0x2fad25f file0x2fb125f e84c4c4c04 call 0x7471eb0
0x2fad264 file0x2fb1264 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2fad269 file0x2fb1269 85c0 test eax, eax
0x2fad26b file0x2fb126b 0f84a8f6ffff je 0x2fac919
0x2fad271 file0x2fb1271 e8aa781cfd call 0x174b20
0x2fad276 file0x2fb1276 488d35ff768204 lea rsi, [rip + 0x48276ff] rip->0x77d497c
0x2fad27d file0x2fb127d 4889c7 mov rdi, rax
0x2fad280 file0x2fb1280 e8bbd422fd call 0x1da740
0x2fad285 file0x2fb1285 488d3d3c044106 lea rdi, [rip + 0x641043c] rip->0x93bd6c8
0x2fad28c file0x2fb128c 88052e044106 mov byte ptr [rip + 0x641042e], al rip->0x93bd6c0
0x2fad292 file0x2fb1292 e8294c4c04 call 0x7471ec0
0x2fad297 file0x2fb1297 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2fad29c file0x2fb129c e978f6ffff jmp 0x2fac919
0x2fad2a1 file0x2fb12a1 488d3d20044106 lea rdi, [rip + 0x6410420] rip->0x93bd6c8
0x2fad2a8 file0x2fb12a8 e8034c4c04 call 0x7471eb0
0x2fad2ad file0x2fb12ad c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2fad2b2 file0x2fb12b2 85c0 test eax, eax
0x2fad2b4 file0x2fb12b4 0f847cfcffff je 0x2facf36
0x2fad2ba file0x2fb12ba e861781cfd call 0x174b20
0x2fad2bf file0x2fb12bf 488d35b6768204 lea rsi, [rip + 0x48276b6] rip->0x77d497c
0x2fad2c6 file0x2fb12c6 4889c7 mov rdi, rax
0x2fad2c9 file0x2fb12c9 e872d422fd call 0x1da740
0x2fad2ce file0x2fb12ce 488d3df3034106 lea rdi, [rip + 0x64103f3] rip->0x93bd6c8
0x2fad2d5 file0x2fb12d5 8805e5034106 mov byte ptr [rip + 0x64103e5], al rip->0x93bd6c0
0x2fad2db file0x2fb12db e8e04b4c04 call 0x7471ec0
0x2fad2e0 file0x2fb12e0 c5fa104584 vmovss xmm0, dword ptr [rbp - 0x7c]
0x2fad2e5 file0x2fb12e5 e94cfcffff jmp 0x2facf36
0x2fad2ea file0x2fb12ea 4180be88eeffff00 cmp byte ptr [r14 - 0x1178], 0
0x2fad2f2 file0x2fb12f2 0f84a2010000 je 0x2fad49a
0x2fad2f8 file0x2fb12f8 4c89f0 mov rax, r14
0x2fad2fb file0x2fb12fb 4889c1 mov rcx, rax
0x2fad2fe file0x2fb12fe 488b8080eeffff mov rax, qword ptr [rax - 0x1180]
0x2fad305 file0x2fb1305 48c78180eeffff00000000 mov qword ptr [rcx - 0x1180], 0
0x2fad310 file0x2fb1310 4885c0 test rax, rax
0x2fad313 file0x2fb1313 750f jne 0x2fad324
0x2fad315 file0x2fb1315 bf00000100 mov edi, 0x10000
0x2fad31a file0x2fb131a be00000100 mov esi, 0x10000
0x2fad31f file0x2fb131f e8fca610fd call 0xb7a20
0x2fad324 file0x2fb1324 4c89ff mov rdi, r15
0x2fad327 file0x2fb1327 488d4858 lea rcx, [rax + 0x58]
0x2fad32b file0x2fb132b c700ffffffff mov dword ptr [rax], 0xffffffff
0x2fad331 file0x2fb1331 48894840 mov qword ptr [rax + 0x40], rcx
0x2fad335 file0x2fb1335 c7404800000000 mov dword ptr [rax + 0x48], 0
0x2fad33c file0x2fb133c 498907 mov qword ptr [r15], rax
0x2fad33f file0x2fb133f e83cfc07fd call 0x2cf80
0x2fad344 file0x2fb1344 498b07 mov rax, qword ptr [r15]
0x2fad347 file0x2fb1347 4c8b6040 mov r12, qword ptr [rax + 0x40]
0x2fad34b file0x2fb134b 8b4848 mov ecx, dword ptr [rax + 0x48]
0x2fad34e file0x2fb134e 488db000000100 lea rsi, [rax + 0x10000]
0x2fad355 file0x2fb1355 4983c407 add r12, 7
0x2fad359 file0x2fb1359 4983e4f8 and r12, 0xfffffffffffffff8
0x2fad35d file0x2fb135d 498d9424b0000000 lea rdx, [r12 + 0xb0]
0x2fad365 file0x2fb1365 4839f2 cmp rdx, rsi
0x2fad368 file0x2fb1368 0f8693000000 jbe 0x2fad401
0x2fad36e file0x2fb136e 48897040 mov qword ptr [rax + 0x40], rsi
0x2fad372 file0x2fb1372 f7d1 not ecx
0x2fad374 file0x2fb1374 f02908 lock sub dword ptr [rax], ecx
0x2fad377 file0x2fb1377 0f856dffffff jne 0x2fad2ea
0x2fad37d file0x2fb137d 498b07 mov rax, qword ptr [r15]
0x2fad380 file0x2fb1380 4c8d605f lea r12, [rax + 0x5f]
0x2fad384 file0x2fb1384 488d4858 lea rcx, [rax + 0x58]
0x2fad388 file0x2fb1388 488db000000100 lea rsi, [rax + 0x10000]
0x2fad38f file0x2fb138f c700ffffffff mov dword ptr [rax], 0xffffffff
0x2fad395 file0x2fb1395 4983e4f8 and r12, 0xfffffffffffffff8
0x2fad399 file0x2fb1399 48894840 mov qword ptr [rax + 0x40], rcx
0x2fad39d file0x2fb139d c7404800000000 mov dword ptr [rax + 0x48], 0
0x2fad3a4 file0x2fb13a4 31c9 xor ecx, ecx
0x2fad3a6 file0x2fb13a6 498d9424b0000000 lea rdx, [r12 + 0xb0]
0x2fad3ae file0x2fb13ae 4839f2 cmp rdx, rsi
0x2fad3b1 file0x2fb13b1 764e jbe 0x2fad401
0x2fad3b3 file0x2fb13b3 666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x2fad3c0 file0x2fb13c0 48897040 mov qword ptr [rax + 0x40], rsi
0x2fad3c4 file0x2fb13c4 f0ff00 lock inc dword ptr [rax]
0x2fad3c7 file0x2fb13c7 0f851dffffff jne 0x2fad2ea
0x2fad3cd file0x2fb13cd 498b07 mov rax, qword ptr [r15]
0x2fad3d0 file0x2fb13d0 4c8d605f lea r12, [rax + 0x5f]
0x2fad3d4 file0x2fb13d4 488d5058 lea rdx, [rax + 0x58]
0x2fad3d8 file0x2fb13d8 c700ffffffff mov dword ptr [rax], 0xffffffff
0x2fad3de file0x2fb13de 488db000000100 lea rsi, [rax + 0x10000]
0x2fad3e5 file0x2fb13e5 4983e4f8 and r12, 0xfffffffffffffff8
0x2fad3e9 file0x2fb13e9 48895040 mov qword ptr [rax + 0x40], rdx
0x2fad3ed file0x2fb13ed c7404800000000 mov dword ptr [rax + 0x48], 0
0x2fad3f4 file0x2fb13f4 498d9424b0000000 lea rdx, [r12 + 0xb0]
0x2fad3fc file0x2fb13fc 4839f2 cmp rdx, rsi
0x2fad3ff file0x2fb13ff 77bf ja 0x2fad3c0
0x2fad401 file0x2fb1401 ffc1 inc ecx
0x2fad403 file0x2fb1403 4c89e7 mov rdi, r12
0x2fad406 file0x2fb1406 48895040 mov qword ptr [rax + 0x40], rdx
0x2fad40a file0x2fb140a 31f6 xor esi, esi
0x2fad40c file0x2fb140c 894848 mov dword ptr [rax + 0x48], ecx
0x2fad40f file0x2fb140f e87ca4a4fd call 0x9f7890
0x2fad414 file0x2fb1414 488d0dc9ec3206 lea rcx, [rip + 0x632ecc9] rip->0x92dc0e4
0x2fad41b file0x2fb141b 488d050ea14d05 lea rax, [rip + 0x54da10e] rip->0x8487530
0x2fad422 file0x2fb1422 488d75a0 lea rsi, [rbp - 0x60]
0x2fad426 file0x2fb1426 488d5588 lea rdx, [rbp - 0x78]
0x2fad42a file0x2fb142a 49890424 mov qword ptr [r12], rax
0x2fad42e file0x2fb142e 8b39 mov edi, dword ptr [rcx]
0x2fad430 file0x2fb1430 e82b9628fd call 0x236a60
0x2fad435 file0x2fb1435 0fbe55a0 movsx edx, byte ptr [rbp - 0x60]
0x2fad439 file0x2fb1439 0fbe4d88 movsx ecx, byte ptr [rbp - 0x78]
0x2fad43d file0x2fb143d 488d356eac9e04 lea rsi, [rip + 0x49eac6e] rip->0x79980b2
0x2fad444 file0x2fb1444 4c89e7 mov rdi, r12
0x2fad447 file0x2fb1447 4531c0 xor r8d, r8d
0x2fad44a file0x2fb144a e801a6a4fd call 0x9f7a50
0x2fad44f file0x2fb144f 4d85e4 test r12, r12
0x2fad452 file0x2fb1452 7431 je 0x2fad485
0x2fad454 file0x2fb1454 f041ff442408 lock inc dword ptr [r12 + 8]
0x2fad45a file0x2fb145a 488d75a0 lea rsi, [rbp - 0x60]
0x2fad45e file0x2fb145e 4c89e7 mov rdi, r12
0x2fad461 file0x2fb1461 c645a001 mov byte ptr [rbp - 0x60], 1
0x2fad465 file0x2fb1465 e8c6bd08fd call 0x39230
0x2fad46a file0x2fb146a f041ff4c2408 lock dec dword ptr [r12 + 8]
0x2fad470 file0x2fb1470 0f85fefaffff jne 0x2facf74
0x2fad476 file0x2fb1476 498b0424 mov rax, qword ptr [r12]
0x2fad47a file0x2fb147a 4c89e7 mov rdi, r12
0x2fad47d file0x2fb147d ff5008 call qword ptr [rax + 8]
0x2fad480 file0x2fb1480 e9effaffff jmp 0x2facf74
0x2fad485 file0x2fb1485 488d75a0 lea rsi, [rbp - 0x60]
0x2fad489 file0x2fb1489 4c89e7 mov rdi, r12
0x2fad48c file0x2fb148c c645a001 mov byte ptr [rbp - 0x60], 1
0x2fad490 file0x2fb1490 e89bbd08fd call 0x39230
0x2fad495 file0x2fb1495 e9dafaffff jmp 0x2facf74
0x2fad49a file0x2fb149a 498db680eeffff lea rsi, [r14 - 0x1180]
0x2fad4a1 file0x2fb14a1 488d3dd86e05fd lea rdi, [rip - 0x2fa9128] rip->0x4380
0x2fad4a8 file0x2fb14a8 488d1551eb3206 lea rdx, [rip + 0x632eb51] rip->0x92dc000
0x2fad4af file0x2fb14af e86c504c04 call 0x7472520
0x2fad4b4 file0x2fb14b4 41c68688eeffff01 mov byte ptr [r14 - 0x1178], 1
0x2fad4bc file0x2fb14bc e937feffff jmp 0x2fad2f8
0x2fad4c1 file0x2fb14c1 4180bf88eeffff00 cmp byte ptr [r15 - 0x1178], 0
0x2fad4c9 file0x2fb14c9 0f8416030000 je 0x2fad7e5
0x2fad4cf file0x2fb14cf 4c89f8 mov rax, r15
0x2fad4d2 file0x2fb14d2 4889c1 mov rcx, rax
0x2fad4d5 file0x2fb14d5 488b8080eeffff mov rax, qword ptr [rax - 0x1180]
0x2fad4dc file0x2fb14dc 48c78180eeffff00000000 mov qword ptr [rcx - 0x1180], 0
0x2fad4e7 file0x2fb14e7 4885c0 test rax, rax
0x2fad4ea file0x2fb14ea 750f jne 0x2fad4fb
0x2fad4ec file0x2fb14ec bf00000100 mov edi, 0x10000
0x2fad4f1 file0x2fb14f1 be00000100 mov esi, 0x10000
0x2fad4f6 file0x2fb14f6 e825a510fd call 0xb7a20
0x2fad4fb file0x2fb14fb 4c89e7 mov rdi, r12
0x2fad4fe file0x2fb14fe 488d4858 lea rcx, [rax + 0x58]
0x2fad502 file0x2fb1502 c700ffffffff mov dword ptr [rax], 0xffffffff
0x2fad508 file0x2fb1508 48894840 mov qword ptr [rax + 0x40], rcx
0x2fad50c file0x2fb150c c7404800000000 mov dword ptr [rax + 0x48], 0
0x2fad513 file0x2fb1513 49890424 mov qword ptr [r12], rax
0x2fad517 file0x2fb1517 e864fa07fd call 0x2cf80
0x2fad51c file0x2fb151c 498b0424 mov rax, qword ptr [r12]
0x2fad520 file0x2fb1520 4c8b7040 mov r14, qword ptr [rax + 0x40]
0x2fad524 file0x2fb1524 8b4848 mov ecx, dword ptr [rax + 0x48]
0x2fad527 file0x2fb1527 488db000000100 lea rsi, [rax + 0x10000]
0x2fad52e file0x2fb152e 4983c607 add r14, 7
0x2fad532 file0x2fb1532 4983e6f8 and r14, 0xfffffffffffffff8
0x2fad536 file0x2fb1536 498d96b0000000 lea rdx, [r14 + 0xb0]
0x2fad53d file0x2fb153d 4839f2 cmp rdx, rsi
0x2fad540 file0x2fb1540 0f8698020000 jbe 0x2fad7de
0x2fad546 file0x2fb1546 48897040 mov qword ptr [rax + 0x40], rsi
0x2fad54a file0x2fb154a f7d1 not ecx
0x2fad54c file0x2fb154c f02908 lock sub dword ptr [rax], ecx
0x2fad54f file0x2fb154f 0f856cffffff jne 0x2fad4c1
0x2fad555 file0x2fb1555 498b0424 mov rax, qword ptr [r12]
0x2fad559 file0x2fb1559 4c8d705f lea r14, [rax + 0x5f]
0x2fad55d file0x2fb155d 488d4858 lea rcx, [rax + 0x58]
0x2fad561 file0x2fb1561 c700ffffffff mov dword ptr [rax], 0xffffffff
0x2fad567 file0x2fb1567 488db000000100 lea rsi, [rax + 0x10000]
0x2fad56e file0x2fb156e 4983e6f8 and r14, 0xfffffffffffffff8
0x2fad572 file0x2fb1572 48894840 mov qword ptr [rax + 0x40], rcx
0x2fad576 file0x2fb1576 b901000000 mov ecx, 1
0x2fad57b file0x2fb157b c7404800000000 mov dword ptr [rax + 0x48], 0
0x2fad582 file0x2fb1582 498d96b0000000 lea rdx, [r14 + 0xb0]
0x2fad589 file0x2fb1589 4839f2 cmp rdx, rsi
0x2fad58c file0x2fb158c 7643 jbe 0x2fad5d1
0x2fad58e file0x2fb158e 6690 nop 
0x2fad590 file0x2fb1590 48897040 mov qword ptr [rax + 0x40], rsi
0x2fad594 file0x2fb1594 f0ff00 lock inc dword ptr [rax]
0x2fad597 file0x2fb1597 0f8524ffffff jne 0x2fad4c1
0x2fad59d file0x2fb159d 498b0424 mov rax, qword ptr [r12]
0x2fad5a1 file0x2fb15a1 4c8d705f lea r14, [rax + 0x5f]
0x2fad5a5 file0x2fb15a5 488d5058 lea rdx, [rax + 0x58]
0x2fad5a9 file0x2fb15a9 c700ffffffff mov dword ptr [rax], 0xffffffff
0x2fad5af file0x2fb15af 488db000000100 lea rsi, [rax + 0x10000]
0x2fad5b6 file0x2fb15b6 4983e6f8 and r14, 0xfffffffffffffff8
0x2fad5ba file0x2fb15ba 48895040 mov qword ptr [rax + 0x40], rdx
0x2fad5be file0x2fb15be c7404800000000 mov dword ptr [rax + 0x48], 0
0x2fad5c5 file0x2fb15c5 498d96b0000000 lea rdx, [r14 + 0xb0]
0x2fad5cc file0x2fb15cc 4839f2 cmp rdx, rsi
0x2fad5cf file0x2fb15cf 77bf ja 0x2fad590
0x2fad5d1 file0x2fb15d1 48895040 mov qword ptr [rax + 0x40], rdx
0x2fad5d5 file0x2fb15d5 894848 mov dword ptr [rax + 0x48], ecx
0x2fad5d8 file0x2fb15d8 48b90100000001000000 movabs rcx, 0x100000001
0x2fad5e2 file0x2fb15e2 48be0000000001000000 movabs rsi, 0x100000000
0x2fad5ec file0x2fb15ec 488d158504f104 lea rdx, [rip + 0x4f10485] rip->0x7ebda78
0x2fad5f3 file0x2fb15f3 4d8d7e70 lea r15, [r14 + 0x70]
0x2fad5f7 file0x2fb15f7 49894e08 mov qword ptr [r14 + 8], rcx
0x2fad5fb file0x2fb15fb 488d0d66fbf004 lea rcx, [rip + 0x4f0fb66] rip->0x7ebd168
0x2fad602 file0x2fb1602 49c7461000000000 mov qword ptr [r14 + 0x10], 0
0x2fad60a file0x2fb160a 41c7461800000000 mov dword ptr [r14 + 0x18], 0
0x2fad612 file0x2fb1612 41c74620ffffffff mov dword ptr [r14 + 0x20], 0xffffffff
0x2fad61a file0x2fb161a 49c7463000000000 mov qword ptr [r14 + 0x30], 0
0x2fad622 file0x2fb1622 49897638 mov qword ptr [r14 + 0x38], rsi
0x2fad626 file0x2fb1626 41c6464001 mov byte ptr [r14 + 0x40], 1
0x2fad62b file0x2fb162b 49894e70 mov qword ptr [r14 + 0x70], rcx
0x2fad62f file0x2fb162f 48b920000000000000e8 movabs rcx, 0xe800000000000020
0x2fad639 file0x2fb1639 49c7467800000000 mov qword ptr [r14 + 0x78], 0
0x2fad641 file0x2fb1641 49898e80000000 mov qword ptr [r14 + 0x80], rcx
0x2fad648 file0x2fb1648 49c7869000000000000000 mov qword ptr [r14 + 0x90], 0
0x2fad653 file0x2fb1653 4989b698000000 mov qword ptr [r14 + 0x98], rsi
0x2fad65a file0x2fb165a 6641c786a00000000000 mov word ptr [r14 + 0xa0], 0
0x2fad664 file0x2fb1664 498916 mov qword ptr [r14], rdx
0x2fad667 file0x2fb1667 b1ff mov cl, 0xff
0x2fad669 file0x2fb1669 f0410fc04e40 lock xadd byte ptr [r14 + 0x40], cl
0x2fad66f file0x2fb166f f6c102 test cl, 2
0x2fad672 file0x2fb1672 0f8594010000 jne 0x2fad80c
0x2fad678 file0x2fb1678 488d05119f4d05 lea rax, [rip + 0x54d9f11] rip->0x8487590
0x2fad67f file0x2fb167f 488d0d5eea3206 lea rcx, [rip + 0x632ea5e] rip->0x92dc0e4
0x2fad686 file0x2fb1686 488d75a0 lea rsi, [rbp - 0x60]
0x2fad68a file0x2fb168a 488d559f lea rdx, [rbp - 0x61]
0x2fad68e file0x2fb168e 498906 mov qword ptr [r14], rax
0x2fad691 file0x2fb1691 488b4588 mov rax, qword ptr [rbp - 0x78]
0x2fad695 file0x2fb1695 498986a8000000 mov qword ptr [r14 + 0xa8], rax
0x2fad69c file0x2fb169c 8b39 mov edi, dword ptr [rcx]
0x2fad69e file0x2fb169e e8bd9328fd call 0x236a60
0x2fad6a3 file0x2fb16a3 498b4670 mov rax, qword ptr [r14 + 0x70]
0x2fad6a7 file0x2fb16a7 440fb66da0 movzx r13d, byte ptr [rbp - 0x60]
0x2fad6ac file0x2fb16ac 440fb6659f movzx r12d, byte ptr [rbp - 0x61]
0x2fad6b1 file0x2fb16b1 498d7648 lea rsi, [r14 + 0x48]
0x2fad6b5 file0x2fb16b5 4c89ff mov rdi, r15
0x2fad6b8 file0x2fb16b8 ff5018 call qword ptr [rax + 0x18]
0x2fad6bb file0x2fb16bb 48833d6548180600 cmp qword ptr [rip + 0x6184865], 0 rip->0x9131f28
0x2fad6c3 file0x2fb16c3 488d05fe03f104 lea rax, [rip + 0x4f103fe] rip->0x7ebdac8
0x2fad6ca file0x2fb16ca 49894670 mov qword ptr [r14 + 0x70], rax
0x2fad6ce file0x2fb16ce 4d897648 mov qword ptr [r14 + 0x48], r14
0x2fad6d2 file0x2fb16d2 4d897650 mov qword ptr [r14 + 0x50], r14
0x2fad6d6 file0x2fb16d6 7405 je 0x2fad6dd
0x2fad6d8 file0x2fb16d8 e8232905fd call 0
0x2fad6dd file0x2fb16dd 64488b042500000000 mov rax, qword ptr fs:[0]
0x2fad6e6 file0x2fb16e6 488d8068eeffff lea rax, [rax - 0x1198]
0x2fad6ed file0x2fb16ed 488b00 mov rax, qword ptr [rax]
0x2fad6f0 file0x2fb16f0 4885c0 test rax, rax
0x2fad6f3 file0x2fb16f3 7414 je 0x2fad709
0x2fad6f5 file0x2fb16f5 4180fd06 cmp r13b, 6
0x2fad6f9 file0x2fb16f9 7508 jne 0x2fad703
0x2fad6fb file0x2fb16fb 4c8b6838 mov r13, qword ptr [rax + 0x38]
0x2fad6ff file0x2fb16ff 49c1ed3b shr r13, 0x3b
0x2fad703 file0x2fb1703 488b4030 mov rax, qword ptr [rax + 0x30]
0x2fad707 file0x2fb1707 eb14 jmp 0x2fad71d
0x2fad709 file0x2fb1709 4180fd06 cmp r13b, 6
0x2fad70d file0x2fb170d 410fb6c5 movzx eax, r13b
0x2fad711 file0x2fb1711 41bd01000000 mov r13d, 1
0x2fad717 file0x2fb1717 440f45e8 cmovne r13d, eax
0x2fad71b file0x2fb171b 31c0 xor eax, eax
0x2fad71d file0x2fb171d 488d158ea99e04 lea rdx, [rip + 0x49ea98e] rip->0x79980b2
0x2fad724 file0x2fb1724 49894678 mov qword ptr [r14 + 0x78], rax
0x2fad728 file0x2fb1728 410fb6c5 movzx eax, r13b
0x2fad72c file0x2fb172c 48b980ffffffffffff07 movabs rcx, 0x7ffffffffffff80
0x2fad736 file0x2fb1736 48c1e206 shl rdx, 6
0x2fad73a file0x2fb173a 48c1e03b shl rax, 0x3b
0x2fad73e file0x2fb173e 4821d1 and rcx, rdx
0x2fad741 file0x2fb1741 48ba00000000000000c0 movabs rdx, 0xc000000000000000
0x2fad74b file0x2fb174b 4809c8 or rax, rcx
0x2fad74e file0x2fb174e 4809c2 or rdx, rax
0x2fad751 file0x2fb1751 49899680000000 mov qword ptr [r14 + 0x80], rdx
0x2fad758 file0x2fb1758 4588661c mov byte ptr [r14 + 0x1c], r12b
0x2fad75c file0x2fb175c 4d85f6 test r14, r14
0x2fad75f file0x2fb175f 7460 je 0x2fad7c1
0x2fad761 file0x2fb1761 f041ff4608 lock inc dword ptr [r14 + 8]
0x2fad766 file0x2fb1766 488d75a0 lea rsi, [rbp - 0x60]
0x2fad76a file0x2fb176a 4c89f7 mov rdi, r14
0x2fad76d file0x2fb176d 4d8d7e08 lea r15, [r14 + 8]
0x2fad771 file0x2fb1771 c645a001 mov byte ptr [rbp - 0x60], 1
0x2fad775 file0x2fb1775 e8b6ba08fd call 0x39230
0x2fad77a file0x2fb177a f041ff0f lock dec dword ptr [r15]
0x2fad77e file0x2fb177e 4c8b2d83471806 mov r13, qword ptr [rip + 0x6184783] rip->0x9131f08
0x2fad785 file0x2fb1785 7509 jne 0x2fad790
0x2fad787 file0x2fb1787 498b06 mov rax, qword ptr [r14]
0x2fad78a file0x2fb178a 4c89f7 mov rdi, r14
0x2fad78d file0x2fb178d ff5008 call qword ptr [rax + 8]
0x2fad790 file0x2fb1790 84db test bl, bl
0x2fad792 file0x2fb1792 7411 je 0x2fad7a5
0x2fad794 file0x2fb1794 e8677efdff call 0x2f85600
0x2fad799 file0x2fb1799 488b08 mov rcx, qword ptr [rax]
0x2fad79c file0x2fb179c 4889c7 mov rdi, rax
0x2fad79f file0x2fb179f ff9138010000 call qword ptr [rcx + 0x138]
0x2fad7a5 file0x2fb17a5 498b4500 mov rax, qword ptr [r13]
0x2fad7a9 file0x2fb17a9 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x2fad7ad file0x2fb17ad 756b jne 0x2fad81a
0x2fad7af file0x2fb17af 4881c488000000 add rsp, 0x88
0x2fad7b6 file0x2fb17b6 5b pop rbx
0x2fad7b7 file0x2fb17b7 415c pop r12
0x2fad7b9 file0x2fb17b9 415d pop r13
0x2fad7bb file0x2fb17bb 415e pop r14
0x2fad7bd file0x2fb17bd 415f pop r15
0x2fad7bf file0x2fb17bf 5d pop rbp
0x2fad7c0 file0x2fb17c0 c3 ret 
0x2fad7c1 file0x2fb17c1 488d75a0 lea rsi, [rbp - 0x60]
0x2fad7c5 file0x2fb17c5 4c89f7 mov rdi, r14
0x2fad7c8 file0x2fb17c8 c645a001 mov byte ptr [rbp - 0x60], 1
0x2fad7cc file0x2fb17cc e85fba08fd call 0x39230
0x2fad7d1 file0x2fb17d1 4c8b2d30471806 mov r13, qword ptr [rip + 0x6184730] rip->0x9131f08
0x2fad7d8 file0x2fb17d8 84db test bl, bl
0x2fad7da file0x2fb17da 75b8 jne 0x2fad794
0x2fad7dc file0x2fb17dc ebc7 jmp 0x2fad7a5
0x2fad7de file0x2fb17de ffc1 inc ecx
0x2fad7e0 file0x2fb17e0 e9ecfdffff jmp 0x2fad5d1
0x2fad7e5 file0x2fb17e5 498db780eeffff lea rsi, [r15 - 0x1180]
0x2fad7ec file0x2fb17ec 488d3d8d6b05fd lea rdi, [rip - 0x2fa9473] rip->0x4380
0x2fad7f3 file0x2fb17f3 488d1506e83206 lea rdx, [rip + 0x632e806] rip->0x92dc000
0x2fad7fa file0x2fb17fa e8214d4c04 call 0x7472520
0x2fad7ff file0x2fb17ff 41c68788eeffff01 mov byte ptr [r15 - 0x1178], 1
0x2fad807 file0x2fb1807 e9c3fcffff jmp 0x2fad4cf
0x2fad80c file0x2fb180c 498d7e40 lea rdi, [r14 + 0x40]
0x2fad810 file0x2fb1810 e8cb1f09fd call 0x3f7e0
0x2fad815 file0x2fb1815 e95efeffff jmp 0x2fad678
0x2fad81a file0x2fb181a e841464c04 call 0x7471e60
0x2fad81f file0x2fb181f 0f0b ud2 
