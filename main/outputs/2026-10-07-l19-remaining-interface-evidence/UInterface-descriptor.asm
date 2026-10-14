0x39fa20 f0x3a3a20 55                             push rbp
0x39fa21 f0x3a3a21 4889e5                         mov rbp, rsp
0x39fa24 f0x3a3a24 4157                           push r15
0x39fa26 f0x3a3a26 4156                           push r14
0x39fa28 f0x3a3a28 53                             push rbx
0x39fa29 f0x3a3a29 4883ec18                       sub rsp, 0x18
0x39fa2d f0x3a3a2d 488b1dd424d908                 mov rbx, qword ptr [rip + 0x8d924d4] rip->0x9131f08
0x39fa34 f0x3a3a34 488b03                         mov rax, qword ptr [rbx]
0x39fa37 f0x3a3a37 488945e0                       mov qword ptr [rbp - 0x20], rax
0x39fa3b f0x3a3a3b 488b05a6680009                 mov rax, qword ptr [rip + 0x90068a6] rip->0x93a62e8
0x39fa42 f0x3a3a42 4885c0                         test rax, rax
0x39fa45 f0x3a3a45 7574                           jne 0x39fabb
0x39fa47 f0x3a3a47 488d0542b8c6ff                 lea rax, [rip - 0x3947be] rip->0xb290
0x39fa4e f0x3a3a4e 4c8d55d8                       lea r10, [rbp - 0x28]
0x39fa52 f0x3a3a52 4c8d1db7000000                 lea r11, [rip + 0xb7] rip->0x39fb10
0x39fa59 f0x3a3a59 4c8d3580000000                 lea r14, [rip + 0x80] rip->0x39fae0
0x39fa60 f0x3a3a60 4c8d3d07764807                 lea r15, [rip + 0x7487607] rip->0x782706e text='Engine'
0x39fa67 f0x3a3a67 488d3d56764807                 lea rdi, [rip + 0x7487656] rip->0x78270c4 text='/Script/CoreUObject'
0x39fa6e f0x3a3a6e 488d354f9e2607                 lea rsi, [rip + 0x7269e4f] rip->0x76098c4 text='Interface'
0x39fa75 f0x3a3a75 488d156c680009                 lea rdx, [rip + 0x900686c] rip->0x93a62e8
0x39fa7c f0x3a3a7c 488d0d0db8c6ff                 lea rcx, [rip - 0x3947f3] rip->0xb290
0x39fa83 f0x3a3a83 41b828000000                   mov r8d, 0x28
0x39fa89 f0x3a3a89 41b908000000                   mov r9d, 8
0x39fa8f f0x3a3a8f 488945d8                       mov qword ptr [rbp - 0x28], rax
0x39fa93 f0x3a3a93 488d0556caefff                 lea rax, [rip - 0x1035aa] rip->0x29c4f0
0x39fa9a f0x3a3a9a 50                             push rax
0x39fa9b f0x3a3a9b 50                             push rax
0x39fa9c f0x3a3a9c 4152                           push r10
0x39fa9e f0x3a3a9e 4153                           push r11
0x39faa0 f0x3a3aa0 4156                           push r14
0x39faa2 f0x3a3aa2 4157                           push r15
0x39faa4 f0x3a3aa4 6a00                           push 0
0x39faa6 f0x3a3aa6 6801400010                     push 0x10004001
0x39faab f0x3a3aab e8b0ddf6ff                     call 0x30d860
0x39fab0 f0x3a3ab0 4883c440                       add rsp, 0x40
0x39fab4 f0x3a3ab4 488b052d680009                 mov rax, qword ptr [rip + 0x900682d] rip->0x93a62e8
0x39fabb f0x3a3abb 488b0b                         mov rcx, qword ptr [rbx]
0x39fabe f0x3a3abe 483b4de0                       cmp rcx, qword ptr [rbp - 0x20]
0x39fac2 f0x3a3ac2 750b                           jne 0x39facf
0x39fac4 f0x3a3ac4 4883c418                       add rsp, 0x18
0x39fac8 f0x3a3ac8 5b                             pop rbx
0x39fac9 f0x3a3ac9 415e                           pop r14
0x39facb f0x3a3acb 415f                           pop r15
0x39facd f0x3a3acd 5d                             pop rbp
0x39face f0x3a3ace c3                             ret 
0x39facf f0x3a3acf e88c230d07                     call 0x7471e60
0x39fad4 f0x3a3ad4 0f0b                           ud2 
0x39fad6 f0x3a3ad6 cc                             int3 
0x39fad7 f0x3a3ad7 cc                             int3 
0x39fad8 f0x3a3ad8 cc                             int3 
0x39fad9 f0x3a3ad9 cc                             int3 
0x39fada f0x3a3ada cc                             int3 
0x39fadb f0x3a3adb cc                             int3 
0x39fadc f0x3a3adc cc                             int3 
0x39fadd f0x3a3add cc                             int3 
0x39fade f0x3a3ade cc                             int3 
0x39fadf f0x3a3adf cc                             int3 
0x39fae0 f0x3a3ae0 55                             push rbp
0x39fae1 f0x3a3ae1 4889e5                         mov rbp, rsp
0x39fae4 f0x3a3ae4 53                             push rbx
0x39fae5 f0x3a3ae5 50                             push rax
0x39fae6 f0x3a3ae6 488b1f                         mov rbx, qword ptr [rdi]
0x39fae9 f0x3a3ae9 4889fe                         mov rsi, rdi
0x39faec f0x3a3aec 4889df                         mov rdi, rbx
0x39faef f0x3a3aef e82c210f00                     call 0x491c20
0x39faf4 f0x3a3af4 488d0585e5b307                 lea rax, [rip + 0x7b3e585] rip->0x7ede080
0x39fafb f0x3a3afb 488903                         mov qword ptr [rbx], rax
0x39fafe f0x3a3afe 4883c408                       add rsp, 8
0x39fb02 f0x3a3b02 5b                             pop rbx
0x39fb03 f0x3a3b03 5d                             pop rbp
0x39fb04 f0x3a3b04 c3                             ret 
0x39fb05 f0x3a3b05 cc                             int3 
0x39fb06 f0x3a3b06 cc                             int3 
0x39fb07 f0x3a3b07 cc                             int3 
0x39fb08 f0x3a3b08 cc                             int3 
0x39fb09 f0x3a3b09 cc                             int3 
0x39fb0a f0x3a3b0a cc                             int3 
0x39fb0b f0x3a3b0b cc                             int3 
0x39fb0c f0x3a3b0c cc                             int3 
0x39fb0d f0x3a3b0d cc                             int3 
0x39fb0e f0x3a3b0e cc                             int3 
0x39fb0f f0x3a3b0f cc                             int3 
0x39fb10 f0x3a3b10 55                             push rbp
0x39fb11 f0x3a3b11 4889e5                         mov rbp, rsp
0x39fb14 f0x3a3b14 e827750100                     call 0x3b7040
0x39fb19 f0x3a3b19 5d                             pop rbp
0x39fb1a f0x3a3b1a c3                             ret 
0x39fb1b f0x3a3b1b cc                             int3 
0x39fb1c f0x3a3b1c cc                             int3 
0x39fb1d f0x3a3b1d cc                             int3 
0x39fb1e f0x3a3b1e cc                             int3 
0x39fb1f f0x3a3b1f cc                             int3 
0x39fb20 f0x3a3b20 55                             push rbp
0x39fb21 f0x3a3b21 4889e5                         mov rbp, rsp
0x39fb24 f0x3a3b24 53                             push rbx
0x39fb25 f0x3a3b25 50                             push rax
0x39fb26 f0x3a3b26 488d051be2b307                 lea rax, [rip + 0x7b3e21b] rip->0x7eddd48
0x39fb2d f0x3a3b2d 4889fb                         mov rbx, rdi
0x39fb30 f0x3a3b30 488907                         mov qword ptr [rdi], rax
0x39fb33 f0x3a3b33 e8a85a0100                     call 0x3b55e0
0x39fb38 f0x3a3b38 488dbbd0000000                 lea rdi, [rbx + 0xd0]
0x39fb3f f0x3a3b3f e85c240d07                     call 0x7471fa0
0x39fb44 f0x3a3b44 488dbb80000000                 lea rdi, [rbx + 0x80]
0x39fb4b f0x3a3b4b e810760100                     call 0x3b7160
0x39fb50 f0x3a3b50 4889df                         mov rdi, rbx
0x39fb53 f0x3a3b53 e868570100                     call 0x3b52c0
0x39fb58 f0x3a3b58 4883c408                       add rsp, 8
0x39fb5c f0x3a3b5c 5b                             pop rbx
0x39fb5d f0x3a3b5d 5d                             pop rbp
0x39fb5e f0x3a3b5e c3                             ret 
0x39fb5f f0x3a3b5f cc                             int3 
0x39fb60 f0x3a3b60 55                             push rbp
0x39fb61 f0x3a3b61 4889e5                         mov rbp, rsp
0x39fb64 f0x3a3b64 4157                           push r15
0x39fb66 f0x3a3b66 4156                           push r14
0x39fb68 f0x3a3b68 53                             push rbx
0x39fb69 f0x3a3b69 50                             push rax
0x39fb6a f0x3a3b6a c705946b000900000000           mov dword ptr [rip + 0x9006b94], 0 rip->0x93a6708
0x39fb74 f0x3a3b74 488b3d856b0009                 mov rdi, qword ptr [rip + 0x9006b85] rip->0x93a6700
0x39fb7b f0x3a3b7b 4885ff                         test rdi, rdi
0x39fb7e f0x3a3b7e 7405                           je 0x39fb85
0x39fb80 f0x3a3b80 e80b7fd1ff                     call 0xb7a90
0x39fb85 f0x3a3b85 c705396b000900000000           mov dword ptr [rip + 0x9006b39], 0 rip->0x93a66c8
0x39fb8f f0x3a3b8f 833d366b000900                 cmp dword ptr [rip + 0x9006b36], 0
0x39fb96 f0x3a3b96 7422                           je 0x39fbba
0x39fb98 f0x3a3b98 488d0d216b0009                 lea rcx, [rip + 0x9006b21] rip->0x93a66c0
0x39fb9f f0x3a3b9f 4c8d0d266b0009                 lea r9, [rip + 0x9006b26] rip->0x93a66cc
0x39fba6 f0x3a3ba6 bf28000000                     mov edi, 0x28
0x39fbab f0x3a3bab be04000000                     mov esi, 4
0x39fbb0 f0x3a3bb0 31d2                           xor edx, edx
0x39fbb2 f0x3a3bb2 4531c0                         xor r8d, r8d
0x39fbb5 f0x3a3bb5 e86662c8ff                     call 0x25e20
0x39fbba f0x3a3bba b8ffffffff                     mov eax, 0xffffffff
0x39fbbf f0x3a3bbf 4889052a6b0009                 mov qword ptr [rip + 0x9006b2a], rax rip->0x93a66f0
0x39fbc6 f0x3a3bc6 c705186b000900000000           mov dword ptr [rip + 0x9006b18], 0 rip->0x93a66e8
0x39fbd0 f0x3a3bd0 813d126b000981000000           cmp dword ptr [rip + 0x9006b12], 0x81
0x39fbda f0x3a3bda 7218                           jb 0x39fbf4
0x39fbdc f0x3a3bdc 488d3ded6a0009                 lea rdi, [rip + 0x9006aed] rip->0x93a66d0
0x39fbe3 f0x3a3be3 31f6                           xor esi, esi
0x39fbe5 f0x3a3be5 c705fd6a000980000000           mov dword ptr [rip + 0x9006afd], 0x80 rip->0x93a66ec
0x39fbef f0x3a3bef e8bc62c8ff                     call 0x25eb0
0x39fbf4 f0x3a3bf4 488b3de56a0009                 mov rdi, qword ptr [rip + 0x9006ae5] rip->0x93a66e0
0x39fbfb f0x3a3bfb 4885ff                         test rdi, rdi
0x39fbfe f0x3a3bfe 7405                           je 0x39fc05
0x39fc00 f0x3a3c00 e88b7ed1ff                     call 0xb7a90
0x39fc05 f0x3a3c05 488b3db46a0009                 mov rdi, qword ptr [rip + 0x9006ab4] rip->0x93a66c0
0x39fc0c f0x3a3c0c 4885ff                         test rdi, rdi
0x39fc0f f0x3a3c0f 7405                           je 0x39fc16
0x39fc11 f0x3a3c11 e87a7ed1ff                     call 0xb7a90
0x39fc16 f0x3a3c16 49c7c630fdffff                 mov r14, 0xfffffffffffffd30
0x39fc1d f0x3a3c1d 488d1d4c6a0009                 lea rbx, [rip + 0x9006a4c] rip->0x93a6670
0x39fc24 f0x3a3c24 41bfffffffff                   mov r15d, 0xffffffff
0x39fc2a f0x3a3c2a eb12                           jmp 0x39fc3e
0x39fc2c f0x3a3c2c 0f1f4000                       nop dword ptr [rax]
0x39fc30 f0x3a3c30 4883c3b0                       add rbx, -0x50
0x39fc34 f0x3a3c34 4983c650                       add r14, 0x50
0x39fc38 f0x3a3c38 0f8487000000                   je 0x39fcc5
0x39fc3e f0x3a3c3e c7434800000000                 mov dword ptr [rbx + 0x48], 0
0x39fc45 f0x3a3c45 488b7b40                       mov rdi, qword ptr [rbx + 0x40]
0x39fc49 f0x3a3c49 4885ff                         test rdi, rdi
0x39fc4c f0x3a3c4c 7405                           je 0x39fc53
0x39fc4e f0x3a3c4e e83d7ed1ff                     call 0xb7a90
0x39fc53 f0x3a3c53 c7430800000000                 mov dword ptr [rbx + 8], 0
0x39fc5a f0x3a3c5a 837b0c00                       cmp dword ptr [rbx + 0xc], 0
0x39fc5e f0x3a3c5e 741b                           je 0x39fc7b
0x39fc60 f0x3a3c60 4c8d4b0c                       lea r9, [rbx + 0xc]
0x39fc64 f0x3a3c64 bf28000000                     mov edi, 0x28
0x39fc69 f0x3a3c69 be04000000                     mov esi, 4
0x39fc6e f0x3a3c6e 4889d9                         mov rcx, rbx
0x39fc71 f0x3a3c71 31d2                           xor edx, edx
0x39fc73 f0x3a3c73 4531c0                         xor r8d, r8d
0x39fc76 f0x3a3c76 e8a561c8ff                     call 0x25e20
0x39fc7b f0x3a3c7b 4c897b30                       mov qword ptr [rbx + 0x30], r15
0x39fc7f f0x3a3c7f c7432800000000                 mov dword ptr [rbx + 0x28], 0
0x39fc86 f0x3a3c86 817b2c81000000                 cmp dword ptr [rbx + 0x2c], 0x81
0x39fc8d f0x3a3c8d 7212                           jb 0x39fca1
0x39fc8f f0x3a3c8f 488d7b10                       lea rdi, [rbx + 0x10]
0x39fc93 f0x3a3c93 31f6                           xor esi, esi
0x39fc95 f0x3a3c95 c7432c80000000                 mov dword ptr [rbx + 0x2c], 0x80
0x39fc9c f0x3a3c9c e80f62c8ff                     call 0x25eb0
0x39fca1 f0x3a3ca1 488b7b20                       mov rdi, qword ptr [rbx + 0x20]
0x39fca5 f0x3a3ca5 4885ff                         test rdi, rdi
0x39fca8 f0x3a3ca8 7405                           je 0x39fcaf
0x39fcaa f0x3a3caa e8e17dd1ff                     call 0xb7a90
0x39fcaf f0x3a3caf 488b3b                         mov rdi, qword ptr [rbx]
0x39fcb2 f0x3a3cb2 4885ff                         test rdi, rdi
0x39fcb5 f0x3a3cb5 0f8475ffffff                   je 0x39fc30
0x39fcbb f0x3a3cbb e8d07dd1ff                     call 0xb7a90
0x39fcc0 f0x3a3cc0 e96bffffff                     jmp 0x39fc30
0x39fcc5 f0x3a3cc5 4883c408                       add rsp, 8
0x39fcc9 f0x3a3cc9 5b                             pop rbx
0x39fcca f0x3a3cca 415e                           pop r14
0x39fccc f0x3a3ccc 415f                           pop r15
0x39fcce f0x3a3cce 5d                             pop rbp
0x39fccf f0x3a3ccf c3                             ret 
0x39fcd0 f0x3a3cd0 55                             push rbp
0x39fcd1 f0x3a3cd1 4889e5                         mov rbp, rsp
0x39fcd4 f0x3a3cd4 4157                           push r15
0x39fcd6 f0x3a3cd6 4156                           push r14
0x39fcd8 f0x3a3cd8 4154                           push r12
0x39fcda f0x3a3cda 53                             push rbx
0x39fcdb f0x3a3cdb 4883ec30                       sub rsp, 0x30
0x39fcdf f0x3a3cdf 4c8b252222d908                 mov r12, qword ptr [rip + 0x8d92222] rip->0x9131f08
0x39fce6 f0x3a3ce6 4889fb                         mov rbx, rdi
0x39fce9 f0x3a3ce9 488d3d18660009                 lea rdi, [rip + 0x9006618] rip->0x93a6308
0x39fcf0 f0x3a3cf0 4989f6                         mov r14, rsi
0x39fcf3 f0x3a3cf3 498b0424                       mov rax, qword ptr [r12]
0x39fcf7 f0x3a3cf7 488945d8                       mov qword ptr [rbp - 0x28], rax
0x39fcfb f0x3a3cfb e8602c0d07                     call 0x7472960
0x39fd00 f0x3a3d00 4c393561660009                 cmp qword ptr [rip + 0x9006661], r14
0x39fd07 f0x3a3d07 0f849e000000                   je 0x39fdab
0x39fd0d f0x3a3d0d 4c8d3df4650009                 lea r15, [rip + 0x90065f4] rip->0x93a6308
0x39fd14 f0x3a3d14 4c89ff                         mov rdi, r15
0x39fd17 f0x3a3d17 e8b42b0d07                     call 0x74728d0
0x39fd1c f0x3a3d1c 4c89ff                         mov rdi, r15
0x39fd1f f0x3a3d1f e83c2c0d07                     call 0x7472960
0x39fd24 f0x3a3d24 8b05f6650009                   mov eax, dword ptr [rip + 0x90065f6] rip->0x93a6320
0x39fd2a f0x3a3d2a 3b051c660009                   cmp eax, dword ptr [rip + 0x900661c]
0x39fd30 f0x3a3d30 0f84a9000000                   je 0x39fddf
0x39fd36 f0x3a3d36 4c89f0                         mov rax, r14
0x39fd39 f0x3a3d39 488d1510660009                 lea rdx, [rip + 0x9006610] rip->0x93a6350
0x39fd40 f0x3a3d40 48c1e804                       shr rax, 4
0x39fd44 f0x3a3d44 89c1                           mov ecx, eax
0x39fd46 f0x3a3d46 c1e910                         shr ecx, 0x10
0x39fd49 f0x3a3d49 31c1                           xor ecx, eax
0x39fd4b f0x3a3d4b 69c16bcaeb85                   imul eax, ecx, 0x85ebca6b
0x39fd51 f0x3a3d51 89c1                           mov ecx, eax
0x39fd53 f0x3a3d53 c1e90d                         shr ecx, 0xd
0x39fd56 f0x3a3d56 31c1                           xor ecx, eax
0x39fd58 f0x3a3d58 69c135aeb2c2                   imul eax, ecx, 0xc2b2ae35
0x39fd5e f0x3a3d5e 89c1                           mov ecx, eax
0x39fd60 f0x3a3d60 c1e910                         shr ecx, 0x10
0x39fd63 f0x3a3d63 31c1                           xor ecx, eax
0x39fd65 f0x3a3d65 488b05ec650009                 mov rax, qword ptr [rip + 0x90065ec] rip->0x93a6358
0x39fd6c f0x3a3d6c 4885c0                         test rax, rax
0x39fd6f f0x3a3d6f 480f45d0                       cmovne rdx, rax
0x39fd73 f0x3a3d73 8b05e7650009                   mov eax, dword ptr [rip + 0x90065e7] rip->0x93a6360
0x39fd79 f0x3a3d79 ffc8                           dec eax
0x39fd7b f0x3a3d7b 21c8                           and eax, ecx
0x39fd7d f0x3a3d7d 8b0c82                         mov ecx, dword ptr [rdx + rax*4]
0x39fd80 f0x3a3d80 83f9ff                         cmp ecx, -1
0x39fd83 f0x3a3d83 745a                           je 0x39fddf
0x39fd85 f0x3a3d85 488b058c650009                 mov rax, qword ptr [rip + 0x900658c] rip->0x93a6318
0x39fd8c f0x3a3d8c 0f1f4000                       nop dword ptr [rax]
0x39fd90 f0x3a3d90 4863d1                         movsxd rdx, ecx
0x39fd93 f0x3a3d93 48c1e205                       shl rdx, 5
0x39fd97 f0x3a3d97 488d0c10                       lea rcx, [rax + rdx]
0x39fd9b f0x3a3d9b 4c393410                       cmp qword ptr [rax + rdx], r14
0x39fd9f f0x3a3d9f 7419                           je 0x39fdba
0x39fda1 f0x3a3da1 8b4918                         mov ecx, dword ptr [rcx + 0x18]
0x39fda4 f0x3a3da4 83f9ff                         cmp ecx, -1
0x39fda7 f0x3a3da7 75e7                           jne 0x39fd90
0x39fda9 f0x3a3da9 eb34                           jmp 0x39fddf
0x39fdab f0x3a3dab c5f81005bd650009               vmovups xmm0, xmmword ptr [rip + 0x90065bd]
0x39fdb3 f0x3a3db3 c5f82945b0                     vmovaps xmmword ptr [rbp - 0x50], xmm0
0x39fdb8 f0x3a3db8 eb41                           jmp 0x39fdfb
0x39fdba f0x3a3dba 4885c9                         test rcx, rcx
0x39fdbd f0x3a3dbd 7420                           je 0x39fddf
0x39fdbf f0x3a3dbf c5f8104108                     vmovups xmm0, xmmword ptr [rcx + 8]
0x39fdc4 f0x3a3dc4 4c89359d650009                 mov qword ptr [rip + 0x900659d], r14 rip->0x93a6368
0x39fdcb f0x3a3dcb c5f811059d650009               vmovups xmmword ptr [rip + 0x900659d], xmm0
0x39fdd3 f0x3a3dd3 c5f8104108                     vmovups xmm0, xmmword ptr [rcx + 8]
0x39fdd8 f0x3a3dd8 c5f82945b0                     vmovaps xmmword ptr [rbp - 0x50], xmm0
0x39fddd f0x3a3ddd eb1c                           jmp 0x39fdfb
0x39fddf f0x3a3ddf c5f857c0                       vxorps xmm0, xmm0, xmm0
0x39fde3 f0x3a3de3 4c89357e650009                 mov qword ptr [rip + 0x900657e], r14 rip->0x93a6368
0x39fdea f0x3a3dea c5f057c9                       vxorps xmm1, xmm1, xmm1
0x39fdee f0x3a3dee c5f82945b0                     vmovaps xmmword ptr [rbp - 0x50], xmm0
0x39fdf3 f0x3a3df3 c5f8110d75650009               vmovups xmmword ptr [rip + 0x9006575], xmm1
0x39fdfb f0x3a3dfb 488d3d06650009                 lea rdi, [rip + 0x9006506] rip->0x93a6308
0x39fe02 f0x3a3e02 e8c92a0d07                     call 0x74728d0
0x39fe07 f0x3a3e07 c5f82845b0                     vmovaps xmm0, xmmword ptr [rbp - 0x50]
0x39fe0c f0x3a3e0c c5f81103                       vmovups xmmword ptr [rbx], xmm0
0x39fe10 f0x3a3e10 498b0424                       mov rax, qword ptr [r12]
0x39fe14 f0x3a3e14 483b45d8                       cmp rax, qword ptr [rbp - 0x28]
0x39fe18 f0x3a3e18 750d                           jne 0x39fe27
0x39fe1a f0x3a3e1a 4883c430                       add rsp, 0x30
0x39fe1e f0x3a3e1e 5b                             pop rbx
0x39fe1f f0x3a3e1f 415c                           pop r12
0x39fe21 f0x3a3e21 415e                           pop r14
0x39fe23 f0x3a3e23 415f                           pop r15
0x39fe25 f0x3a3e25 5d                             pop rbp
0x39fe26 f0x3a3e26 c3                             ret 
0x39fe27 f0x3a3e27 e834200d07                     call 0x7471e60
0x39fe2c f0x3a3e2c 0f0b                           ud2 
0x39fe2e f0x3a3e2e cc                             int3 
0x39fe2f f0x3a3e2f cc                             int3 
0x39fe30 f0x3a3e30 55                             push rbp
0x39fe31 f0x3a3e31 4889e5                         mov rbp, rsp
0x39fe34 f0x3a3e34 4157                           push r15
0x39fe36 f0x3a3e36 4156                           push r14
0x39fe38 f0x3a3e38 4155                           push r13
0x39fe3a f0x3a3e3a 4154                           push r12
0x39fe3c f0x3a3e3c 53                             push rbx
0x39fe3d f0x3a3e3d 4883ec28                       sub rsp, 0x28
0x39fe41 f0x3a3e41 4c8b3dc020d908                 mov r15, qword ptr [rip + 0x8d920c0] rip->0x9131f08
0x39fe48 f0x3a3e48 498b07                         mov rax, qword ptr [r15]
0x39fe4b f0x3a3e4b 488945d0                       mov qword ptr [rbp - 0x30], rax
0x39fe4f f0x3a3e4f c5f81007                       vmovups xmm0, xmmword ptr [rdi]
0x39fe53 f0x3a3e53 c5f81145b0                     vmovups xmmword ptr [rbp - 0x50], xmm0
0x39fe58 f0x3a3e58 c5fa6f45b0                     vmovdqu xmm0, xmmword ptr [rbp - 0x50]
0x39fe5d f0x3a3e5d c4e27917c0                     vptest xmm0, xmm0
0x39fe62 f0x3a3e62 747f                           je 0x39fee3
0x39fe64 f0x3a3e64 4863c6                         movsxd rax, esi
0x39fe67 f0x3a3e67 4c8d75b0                       lea r14, [rbp - 0x50]
0x39fe6b f0x3a3e6b 4c8d2d7e650009                 lea r13, [rip + 0x900657e] rip->0x93a63f0
0x39fe72 f0x3a3e72 be10000000                     mov esi, 0x10
0x39fe77 f0x3a3e77 4c8d2480                       lea r12, [rax + rax*4]
0x39fe7b f0x3a3e7b 4c89f7                         mov rdi, r14
0x39fe7e f0x3a3e7e 49c1e404                       shl r12, 4
0x39fe82 f0x3a3e82 4b8d5c2500                     lea rbx, [r13 + r12]
0x39fe87 f0x3a3e87 e8b4f9d1ff                     call 0xbf840
0x39fe8c f0x3a3e8c 438b4c2c08                     mov ecx, dword ptr [r12 + r13 + 8]
0x39fe91 f0x3a3e91 433b4c2c34                     cmp ecx, dword ptr [r12 + r13 + 0x34]
0x39fe96 f0x3a3e96 7477                           je 0x39ff0f
0x39fe98 f0x3a3e98 488b5340                       mov rdx, qword ptr [rbx + 0x40]
0x39fe9c f0x3a3e9c 488d4b38                       lea rcx, [rbx + 0x38]
0x39fea0 f0x3a3ea0 4885d2                         test rdx, rdx
0x39fea3 f0x3a3ea3 480f45ca                       cmovne rcx, rdx
0x39fea7 f0x3a3ea7 8b5348                         mov edx, dword ptr [rbx + 0x48]
0x39feaa f0x3a3eaa ffca                           dec edx
0x39feac f0x3a3eac 21c2                           and edx, eax
0x39feae f0x3a3eae 8b1491                         mov edx, dword ptr [rcx + rdx*4]
0x39feb1 f0x3a3eb1 83faff                         cmp edx, -1
0x39feb4 f0x3a3eb4 7459                           je 0x39ff0f
0x39feb6 f0x3a3eb6 c5fa6f45b0                     vmovdqu xmm0, xmmword ptr [rbp - 0x50]
0x39febb f0x3a3ebb 488b0b                         mov rcx, qword ptr [rbx]
0x39febe f0x3a3ebe 6690                           nop 
0x39fec0 f0x3a3ec0 4863d2                         movsxd rdx, edx
0x39fec3 f0x3a3ec3 488d1492                       lea rdx, [rdx + rdx*4]
0x39fec7 f0x3a3ec7 c5f9ef0cd1                     vpxor xmm1, xmm0, xmmword ptr [rcx + rdx*8]
0x39fecc f0x3a3ecc 4c8d24d1                       lea r12, [rcx + rdx*8]
0x39fed0 f0x3a3ed0 c4e27917c9                     vptest xmm1, xmm1
0x39fed5 f0x3a3ed5 7412                           je 0x39fee9
0x39fed7 f0x3a3ed7 418b542420                     mov edx, dword ptr [r12 + 0x20]
0x39fedc f0x3a3edc 83faff                         cmp edx, -1
0x39fedf f0x3a3edf 75df                           jne 0x39fec0
0x39fee1 f0x3a3ee1 eb2c                           jmp 0x39ff0f
0x39fee3 f0x3a3ee3 31c0                           xor eax, eax
0x39fee5 f0x3a3ee5 31d2                           xor edx, edx
0x39fee7 f0x3a3ee7 eb5e                           jmp 0x39ff47
0x39fee9 f0x3a3ee9 4d85e4                         test r12, r12
0x39feec f0x3a3eec 7421                           je 0x39ff0f
0x39feee f0x3a3eee 4983c410                       add r12, 0x10
0x39fef2 f0x3a3ef2 c4c17a6f0424                   vmovdqu xmm0, xmmword ptr [r12]
0x39fef8 f0x3a3ef8 c4e27917c0                     vptest xmm0, xmm0
0x39fefd f0x3a3efd 753f                           jne 0x39ff3e
0x39feff f0x3a3eff e8dc05e4ff                     call 0x1e04e0
0x39ff04 f0x3a3f04 49890424                       mov qword ptr [r12], rax
0x39ff08 f0x3a3f08 4989542408                     mov qword ptr [r12 + 8], rdx
0x39ff0d f0x3a3f0d eb38                           jmp 0x39ff47
0x39ff0f f0x3a3f0f 488d55c8                       lea rdx, [rbp - 0x38]
0x39ff13 f0x3a3f13 4889df                         mov rdi, rbx
0x39ff16 f0x3a3f16 89c6                           mov esi, eax
0x39ff18 f0x3a3f18 31c9                           xor ecx, ecx
0x39ff1a f0x3a3f1a 4c8975c8                       mov qword ptr [rbp - 0x38], r14
0x39ff1e f0x3a3f1e e83d580100                     call 0x3b5760
0x39ff23 f0x3a3f23 488b0b                         mov rcx, qword ptr [rbx]
0x39ff26 f0x3a3f26 4898                           cdqe 
0x39ff28 f0x3a3f28 488d0480                       lea rax, [rax + rax*4]
0x39ff2c f0x3a3f2c 4c8d64c110                     lea r12, [rcx + rax*8 + 0x10]
0x39ff31 f0x3a3f31 c4c17a6f0424                   vmovdqu xmm0, xmmword ptr [r12]
0x39ff37 f0x3a3f37 c4e27917c0                     vptest xmm0, xmm0
0x39ff3c f0x3a3f3c 74c1                           je 0x39feff
0x39ff3e f0x3a3f3e 498b0424                       mov rax, qword ptr [r12]
0x39ff42 f0x3a3f42 498b542408                     mov rdx, qword ptr [r12 + 8]
0x39ff47 f0x3a3f47 498b0f                         mov rcx, qword ptr [r15]
0x39ff4a f0x3a3f4a 483b4dd0                       cmp rcx, qword ptr [rbp - 0x30]
0x39ff4e f0x3a3f4e 750f                           jne 0x39ff5f
0x39ff50 f0x3a3f50 4883c428                       add rsp, 0x28
0x39ff54 f0x3a3f54 5b                             pop rbx
0x39ff55 f0x3a3f55 415c                           pop r12
0x39ff57 f0x3a3f57 415d                           pop r13
0x39ff59 f0x3a3f59 415e                           pop r14
0x39ff5b f0x3a3f5b 415f                           pop r15
0x39ff5d f0x3a3f5d 5d                             pop rbp
0x39ff5e f0x3a3f5e c3                             ret 
0x39ff5f f0x3a3f5f e8fc1e0d07                     call 0x7471e60
0x39ff64 f0x3a3f64 0f0b                           ud2 
0x39ff66 f0x3a3f66 cc                             int3 
0x39ff67 f0x3a3f67 cc                             int3 
0x39ff68 f0x3a3f68 cc                             int3 
0x39ff69 f0x3a3f69 cc                             int3 
0x39ff6a f0x3a3f6a cc                             int3 
0x39ff6b f0x3a3f6b cc                             int3 
0x39ff6c f0x3a3f6c cc                             int3 
0x39ff6d f0x3a3f6d cc                             int3 
0x39ff6e f0x3a3f6e cc                             int3 
0x39ff6f f0x3a3f6f cc                             int3 
0x39ff70 f0x3a3f70 55                             push rbp
0x39ff71 f0x3a3f71 4889e5                         mov rbp, rsp
0x39ff74 f0x3a3f74 4889fe                         mov rsi, rdi
0x39ff77 f0x3a3f77 488d3d82630009                 lea rdi, [rip + 0x9006382] rip->0x93a6300
0x39ff7e f0x3a3f7e e80d000000                     call 0x39ff90
0x39ff83 f0x3a3f83 5d                             pop rbp
0x39ff84 f0x3a3f84 c3                             ret 
0x39ff85 f0x3a3f85 cc                             int3 
0x39ff86 f0x3a3f86 cc                             int3 
0x39ff87 f0x3a3f87 cc                             int3 
0x39ff88 f0x3a3f88 cc                             int3 
0x39ff89 f0x3a3f89 cc                             int3 
0x39ff8a f0x3a3f8a cc                             int3 
0x39ff8b f0x3a3f8b cc                             int3 
0x39ff8c f0x3a3f8c cc                             int3 
0x39ff8d f0x3a3f8d cc                             int3 
0x39ff8e f0x3a3f8e cc                             int3 
0x39ff8f f0x3a3f8f cc                             int3 
0x39ff90 f0x3a3f90 55                             push rbp
0x39ff91 f0x3a3f91 4889e5                         mov rbp, rsp
0x39ff94 f0x3a3f94 4157                           push r15
0x39ff96 f0x3a3f96 4156                           push r14
0x39ff98 f0x3a3f98 53                             push rbx
0x39ff99 f0x3a3f99 50                             push rax
0x39ff9a f0x3a3f9a 488d9fd0000000                 lea rbx, [rdi + 0xd0]
0x39ffa1 f0x3a3fa1 4989fe                         mov r14, rdi
0x39ffa4 f0x3a3fa4 4989f7                         mov r15, rsi
0x39ffa7 f0x3a3fa7 4889df                         mov rdi, rbx
0x39ffaa f0x3a3faa e8311f0d07                     call 0x7471ee0
0x39ffaf f0x3a3faf 85c0                           test eax, eax
0x39ffb1 f0x3a3fb1 750c                           jne 0x39ffbf
0x39ffb3 f0x3a3fb3 e8381f0d07                     call 0x7471ef0
0x39ffb8 f0x3a3fb8 498986d8000000                 mov qword ptr [r14 + 0xd8], rax
0x39ffbf f0x3a3fbf 49ff86e0000000                 inc qword ptr [r14 + 0xe0]
0x39ffc6 f0x3a3fc6 4c89ff                         mov rdi, r15
0x39ffc9 f0x3a3fc9 be10000000                     mov esi, 0x10
0x39ffce f0x3a3fce e86df8d1ff                     call 0xbf840
0x39ffd3 f0x3a3fd3 418b8e88000000                 mov ecx, dword ptr [r14 + 0x88]
0x39ffda f0x3a3fda 413b8eb4000000                 cmp ecx, dword ptr [r14 + 0xb4]
0x39ffe1 f0x3a3fe1 0f8483000000                   je 0x3a006a
0x39ffe7 f0x3a3fe7 498b96c0000000                 mov rdx, qword ptr [r14 + 0xc0]
0x39ffee f0x3a3fee 498d8eb8000000                 lea rcx, [r14 + 0xb8]
0x39fff5 f0x3a3ff5 4885d2                         test rdx, rdx
0x39fff8 f0x3a3ff8 480f45ca                       cmovne rcx, rdx
0x39fffc f0x3a3ffc 418b96c8000000                 mov edx, dword ptr [r14 + 0xc8]
0x3a0003 f0x3a4003 ffca                           dec edx
0x3a0005 f0x3a4005 21c2                           and edx, eax
0x3a0007 f0x3a4007 8b0c91                         mov ecx, dword ptr [rcx + rdx*4]
0x3a000a f0x3a400a 83f9ff                         cmp ecx, -1
0x3a000d f0x3a400d 745b                           je 0x3a006a
0x3a000f f0x3a400f c4c17a6f07                     vmovdqu xmm0, xmmword ptr [r15]
0x3a0014 f0x3a4014 498b8680000000                 mov rax, qword ptr [r14 + 0x80]
0x3a001b f0x3a401b 4531ff                         xor r15d, r15d
0x3a001e f0x3a401e 6690                           nop 
0x3a0020 f0x3a4020 4863d1                         movsxd rdx, ecx
0x3a0023 f0x3a4023 48c1e205                       shl rdx, 5
0x3a0027 f0x3a4027 c5f9ef0c10                     vpxor xmm1, xmm0, xmmword ptr [rax + rdx]
0x3a002c f0x3a402c 488d0c10                       lea rcx, [rax + rdx]
0x3a0030 f0x3a4030 c4e27917c9                     vptest xmm1, xmm1
0x3a0035 f0x3a4035 741f                           je 0x3a0056
0x3a0037 f0x3a4037 8b4918                         mov ecx, dword ptr [rcx + 0x18]
0x3a003a f0x3a403a 83f9ff                         cmp ecx, -1
0x3a003d f0x3a403d 75e1                           jne 0x3a0020
0x3a003f f0x3a403f 49ff8ee0000000                 dec qword ptr [r14 + 0xe0]
0x3a0046 f0x3a4046 742e                           je 0x3a0076
0x3a0048 f0x3a4048 4c89f8                         mov rax, r15
0x3a004b f0x3a404b 4883c408                       add rsp, 8
0x3a004f f0x3a404f 5b                             pop rbx
0x3a0050 f0x3a4050 415e                           pop r14
0x3a0052 f0x3a4052 415f                           pop r15
0x3a0054 f0x3a4054 5d                             pop rbp
0x3a0055 f0x3a4055 c3                             ret 
0x3a0056 f0x3a4056 4885c9                         test rcx, rcx
0x3a0059 f0x3a4059 740f                           je 0x3a006a
0x3a005b f0x3a405b 4c8b7910                       mov r15, qword ptr [rcx + 0x10]
0x3a005f f0x3a405f 49ff8ee0000000                 dec qword ptr [r14 + 0xe0]
0x3a0066 f0x3a4066 75e0                           jne 0x3a0048
0x3a0068 f0x3a4068 eb0c                           jmp 0x3a0076
0x3a006a f0x3a406a 4531ff                         xor r15d, r15d
0x3a006d f0x3a406d 49ff8ee0000000                 dec qword ptr [r14 + 0xe0]
0x3a0074 f0x3a4074 75d2                           jne 0x3a0048
0x3a0076 f0x3a4076 4889df                         mov rdi, rbx
0x3a0079 f0x3a4079 49c786d800000000000000         mov qword ptr [r14 + 0xd8], 0
0x3a0084 f0x3a4084 e8971e0d07                     call 0x7471f20
0x3a0089 f0x3a4089 ebbd                           jmp 0x3a0048
0x3a008b f0x3a408b cc                             int3 
0x3a008c f0x3a408c cc                             int3 
0x3a008d f0x3a408d cc                             int3 
0x3a008e f0x3a408e cc                             int3 
0x3a008f f0x3a408f cc                             int3 
0x3a0090 f0x3a4090 55                             push rbp
0x3a0091 f0x3a4091 4889e5                         mov rbp, rsp
0x3a0094 f0x3a4094 4156                           push r14
0x3a0096 f0x3a4096 53                             push rbx
0x3a0097 f0x3a4097 4883ec20                       sub rsp, 0x20
0x3a009b f0x3a409b 4c8b35661ed908                 mov r14, qword ptr [rip + 0x8d91e66] rip->0x9131f08
0x3a00a2 f0x3a40a2 498b06                         mov rax, qword ptr [r14]
0x3a00a5 f0x3a40a5 488945e8                       mov qword ptr [rbp - 0x18], rax
0x3a00a9 f0x3a40a9 488b1f                         mov rbx, qword ptr [rdi]
0x3a00ac f0x3a40ac 488d7dd0                       lea rdi, [rbp - 0x30]
0x3a00b0 f0x3a40b0 4889de                         mov rsi, rbx
0x3a00b3 f0x3a40b3 e818fcffff                     call 0x39fcd0
0x3a00b8 f0x3a40b8 c5fa6f45d0                     vmovdqu xmm0, xmmword ptr [rbp - 0x30]
0x3a00bd f0x3a40bd c4e27917c0                     vptest xmm0, xmm0
0x3a00c2 f0x3a40c2 7528                           jne 0x3a00ec
0x3a00c4 f0x3a40c4 e81704e4ff                     call 0x1e04e0
0x3a00c9 f0x3a40c9 488945d0                       mov qword ptr [rbp - 0x30], rax
0x3a00cd f0x3a40cd 488955d8                       mov qword ptr [rbp - 0x28], rdx
0x3a00d1 f0x3a40d1 488d3d28620009                 lea rdi, [rip + 0x9006228] rip->0x93a6300
0x3a00d8 f0x3a40d8 488d55d0                       lea rdx, [rbp - 0x30]
0x3a00dc f0x3a40dc 4889de                         mov rsi, rbx
0x3a00df f0x3a40df e81c710100                     call 0x3b7200
0x3a00e4 f0x3a40e4 4889df                         mov rdi, rbx
0x3a00e7 f0x3a40e7 e884720e00                     call 0x487370
0x3a00ec f0x3a40ec 488b45d0                       mov rax, qword ptr [rbp - 0x30]
0x3a00f0 f0x3a40f0 488b55d8                       mov rdx, qword ptr [rbp - 0x28]
0x3a00f4 f0x3a40f4 498b0e                         mov rcx, qword ptr [r14]
0x3a00f7 f0x3a40f7 483b4de8                       cmp rcx, qword ptr [rbp - 0x18]
0x3a00fb f0x3a40fb 7509                           jne 0x3a0106
0x3a00fd f0x3a40fd 4883c420                       add rsp, 0x20
0x3a0101 f0x3a4101 5b                             pop rbx
0x3a0102 f0x3a4102 415e                           pop r14
0x3a0104 f0x3a4104 5d                             pop rbp
0x3a0105 f0x3a4105 c3                             ret 
0x3a0106 f0x3a4106 e8551d0d07                     call 0x7471e60
0x3a010b f0x3a410b 0f0b                           ud2 
0x3a010d f0x3a410d cc                             int3 
0x3a010e f0x3a410e cc                             int3 
0x3a010f f0x3a410f cc                             int3 
0x3a0110 f0x3a4110 55                             push rbp
0x3a0111 f0x3a4111 4889e5                         mov rbp, rsp
0x3a0114 f0x3a4114 4157                           push r15
0x3a0116 f0x3a4116 4156                           push r14
0x3a0118 f0x3a4118 4155                           push r13
0x3a011a f0x3a411a 4154                           push r12
0x3a011c f0x3a411c 53                             push rbx
0x3a011d f0x3a411d 4883ec48                       sub rsp, 0x48
0x3a0121 f0x3a4121 4c8b25e01dd908                 mov r12, qword ptr [rip + 0x8d91de0] rip->0x9131f08
0x3a0128 f0x3a4128 4989fe                         mov r14, rdi
0x3a012b f0x3a412b 488d7da8                       lea rdi, [rbp - 0x58]
0x3a012f f0x3a412f 498b0424                       mov rax, qword ptr [r12]
0x3a0133 f0x3a4133 488945d0                       mov qword ptr [rbp - 0x30], rax
0x3a0137 f0x3a4137 488975a8                       mov qword ptr [rbp - 0x58], rsi
0x3a013b f0x3a413b e820d6e8ff                     call 0x22d760
0x3a0140 f0x3a4140 4889c3                         mov rbx, rax
0x3a0143 f0x3a4143 488b4028                       mov rax, qword ptr [rax + 0x28]
0x3a0147 f0x3a4147 a904000004                     test eax, 0x4000004
0x3a014c f0x3a414c 0f84c0000000                   je 0x3a0212
0x3a0152 f0x3a4152 488d3daf610009                 lea rdi, [rip + 0x90061af] rip->0x93a6308
0x3a0159 f0x3a4159 e802280d07                     call 0x7472960
0x3a015e f0x3a415e 4c393503620009                 cmp qword ptr [rip + 0x9006203], r14
0x3a0165 f0x3a4165 0f8415010000                   je 0x3a0280
0x3a016b f0x3a416b 4c8d3d96610009                 lea r15, [rip + 0x9006196] rip->0x93a6308
0x3a0172 f0x3a4172 4c89ff                         mov rdi, r15
0x3a0175 f0x3a4175 e856270d07                     call 0x74728d0
0x3a017a f0x3a417a 4c89ff                         mov rdi, r15
0x3a017d f0x3a417d e8de270d07                     call 0x7472960
0x3a0182 f0x3a4182 8b0598610009                   mov eax, dword ptr [rip + 0x9006198] rip->0x93a6320
0x3a0188 f0x3a4188 3b05be610009                   cmp eax, dword ptr [rip + 0x90061be]
0x3a018e f0x3a418e 0f84e9010000                   je 0x3a037d
0x3a0194 f0x3a4194 4c89f0                         mov rax, r14
0x3a0197 f0x3a4197 488d15b2610009                 lea rdx, [rip + 0x90061b2] rip->0x93a6350
0x3a019e f0x3a419e 48c1e804                       shr rax, 4
0x3a01a2 f0x3a41a2 89c1                           mov ecx, eax
0x3a01a4 f0x3a41a4 c1e910                         shr ecx, 0x10
0x3a01a7 f0x3a41a7 31c1                           xor ecx, eax
0x3a01a9 f0x3a41a9 69c16bcaeb85                   imul eax, ecx, 0x85ebca6b
0x3a01af f0x3a41af 89c1                           mov ecx, eax
0x3a01b1 f0x3a41b1 c1e90d                         shr ecx, 0xd
0x3a01b4 f0x3a41b4 31c1                           xor ecx, eax
0x3a01b6 f0x3a41b6 69c135aeb2c2                   imul eax, ecx, 0xc2b2ae35
0x3a01bc f0x3a41bc 89c1                           mov ecx, eax
0x3a01be f0x3a41be c1e910                         shr ecx, 0x10
0x3a01c1 f0x3a41c1 31c1                           xor ecx, eax
0x3a01c3 f0x3a41c3 488b058e610009                 mov rax, qword ptr [rip + 0x900618e] rip->0x93a6358
0x3a01ca f0x3a41ca 4885c0                         test rax, rax
0x3a01cd f0x3a41cd 480f45d0                       cmovne rdx, rax
0x3a01d1 f0x3a41d1 8b0589610009                   mov eax, dword ptr [rip + 0x9006189] rip->0x93a6360
0x3a01d7 f0x3a41d7 ffc8                           dec eax
0x3a01d9 f0x3a41d9 21c8                           and eax, ecx
0x3a01db f0x3a41db 8b0c82                         mov ecx, dword ptr [rdx + rax*4]
0x3a01de f0x3a41de 83f9ff                         cmp ecx, -1
0x3a01e1 f0x3a41e1 0f8496010000                   je 0x3a037d
0x3a01e7 f0x3a41e7 488b052a610009                 mov rax, qword ptr [rip + 0x900612a] rip->0x93a6318
0x3a01ee f0x3a41ee 6690                           nop 
0x3a01f0 f0x3a41f0 4863d1                         movsxd rdx, ecx
0x3a01f3 f0x3a41f3 48c1e205                       shl rdx, 5
0x3a01f7 f0x3a41f7 488d0c10                       lea rcx, [rax + rdx]
0x3a01fb f0x3a41fb 4c393410                       cmp qword ptr [rax + rdx], r14
0x3a01ff f0x3a41ff 0f8453010000                   je 0x3a0358
0x3a0205 f0x3a4205 8b4918                         mov ecx, dword ptr [rcx + 0x18]
0x3a0208 f0x3a4208 83f9ff                         cmp ecx, -1
0x3a020b f0x3a420b 75e3                           jne 0x3a01f0
0x3a020d f0x3a420d e96b010000                     jmp 0x3a037d
0x3a0212 f0x3a4212 a801                           test al, 1
0x3a0214 f0x3a4214 0f8437030000                   je 0x3a0551
0x3a021a f0x3a421a 488b7da8                       mov rdi, qword ptr [rbp - 0x58]
0x3a021e f0x3a421e 488b07                         mov rax, qword ptr [rdi]
0x3a0221 f0x3a4221 4c8b7808                       mov r15, qword ptr [rax + 8]
0x3a0225 f0x3a4225 498b4708                       mov rax, qword ptr [r15 + 8]
0x3a0229 f0x3a4229 488b08                         mov rcx, qword ptr [rax]
0x3a022c f0x3a422c 488d5104                       lea rdx, [rcx + 4]
0x3a0230 f0x3a4230 483b5008                       cmp rdx, qword ptr [rax + 8]
0x3a0234 f0x3a4234 765c                           jbe 0x3a0292
0x3a0236 f0x3a4236 c745b000000000                 mov dword ptr [rbp - 0x50], 0
0x3a023d f0x3a423d 488d75b0                       lea rsi, [rbp - 0x50]
0x3a0241 f0x3a4241 ba04000000                     mov edx, 4
0x3a0246 f0x3a4246 4c89ff                         mov rdi, r15
0x3a0249 f0x3a4249 498b07                         mov rax, qword ptr [r15]
0x3a024c f0x3a424c ff9088010000                   call qword ptr [rax + 0x188]
0x3a0252 f0x3a4252 8b45b0                         mov eax, dword ptr [rbp - 0x50]
0x3a0255 f0x3a4255 83f802                         cmp eax, 2
0x3a0258 f0x3a4258 720b                           jb 0x3a0265
0x3a025a f0x3a425a 4c89ff                         mov rdi, r15
0x3a025d f0x3a425d e8feb3e7ff                     call 0x21b660
0x3a0262 f0x3a4262 8b45b0                         mov eax, dword ptr [rbp - 0x50]
0x3a0265 f0x3a4265 498b4f28                       mov rcx, qword ptr [r15 + 0x28]
0x3a0269 f0x3a4269 f6c101                         test cl, 1
0x3a026c f0x3a426c 0f84df020000                   je 0x3a0551
0x3a0272 f0x3a4272 85c0                           test eax, eax
0x3a0274 f0x3a4274 0f84d7020000                   je 0x3a0551
0x3a027a f0x3a427a 488b7da8                       mov rdi, qword ptr [rbp - 0x58]
0x3a027e f0x3a427e eb1e                           jmp 0x3a029e
0x3a0280 f0x3a4280 c5f81005e8600009               vmovups xmm0, xmmword ptr [rip + 0x90060e8]
0x3a0288 f0x3a4288 c5f8294590                     vmovaps xmmword ptr [rbp - 0x70], xmm0
0x3a028d f0x3a428d e907010000                     jmp 0x3a0399
0x3a0292 f0x3a4292 833900                         cmp dword ptr [rcx], 0
0x3a0295 f0x3a4295 488910                         mov qword ptr [rax], rdx
0x3a0298 f0x3a4298 0f84b3020000                   je 0x3a0551
0x3a029e f0x3a429e 488d75b0                       lea rsi, [rbp - 0x50]
0x3a02a2 f0x3a42a2 c5f9efc0                       vpxor xmm0, xmm0, xmm0
0x3a02a6 f0x3a42a6 c5fa7f45b0                     vmovdqu xmmword ptr [rbp - 0x50], xmm0
0x3a02ab f0x3a42ab e82008e4ff                     call 0x1e0ad0
0x3a02b0 f0x3a42b0 4c89f7                         mov rdi, r14
0x3a02b3 f0x3a42b3 e8985e0e00                     call 0x486150
0x3a02b8 f0x3a42b8 f6402c20                       test byte ptr [rax + 0x2c], 0x20
0x3a02bc f0x3a42bc 0f858f020000                   jne 0x3a0551
0x3a02c2 f0x3a42c2 b800104000                     mov eax, 0x401000
0x3a02c7 f0x3a42c7 234334                         and eax, dword ptr [rbx + 0x34]
0x3a02ca f0x3a42ca 3d00100000                     cmp eax, 0x1000
0x3a02cf f0x3a42cf 0f847c020000                   je 0x3a0551
0x3a02d5 f0x3a42d5 488d3d24600009                 lea rdi, [rip + 0x9006024] rip->0x93a6300
0x3a02dc f0x3a42dc 488d75b0                       lea rsi, [rbp - 0x50]
0x3a02e0 f0x3a42e0 e8abfcffff                     call 0x39ff90
0x3a02e5 f0x3a42e5 4c39f0                         cmp rax, r14
0x3a02e8 f0x3a42e8 0f8463020000                   je 0x3a0551
0x3a02ee f0x3a42ee 4885c0                         test rax, rax
0x3a02f1 f0x3a42f1 4889c3                         mov rbx, rax
0x3a02f4 f0x3a42f4 410f95c7                       setne r15b
0x3a02f8 f0x3a42f8 e82348ddff                     call 0x174b20
0x3a02fd f0x3a42fd 488d35c8185807                 lea rsi, [rip + 0x75818c8] rip->0x7921bcc text='AssignNewMapGuids'
0x3a0304 f0x3a4304 4889c7                         mov rdi, rax
0x3a0307 f0x3a4307 e834a4e3ff                     call 0x1da740
0x3a030c f0x3a430c 4108c7                         or r15b, al
0x3a030f f0x3a430f 4180ff01                       cmp r15b, 1
0x3a0313 f0x3a4313 752b                           jne 0x3a0340
0x3a0315 f0x3a4315 4885db                         test rbx, rbx
0x3a0318 f0x3a4318 0f94c1                         sete cl
0x3a031b f0x3a431b 08c1                           or cl, al
0x3a031d f0x3a431d 0f852e020000                   jne 0x3a0551
0x3a0323 f0x3a4323 8b4308                         mov eax, dword ptr [rbx + 8]
0x3a0326 f0x3a4326 a900400000                     test eax, 0x4000
0x3a032b f0x3a432b 0f8420020000                   je 0x3a0551
0x3a0331 f0x3a4331 488d3dc85f0009                 lea rdi, [rip + 0x9005fc8] rip->0x93a6300
0x3a0338 f0x3a4338 4889de                         mov rsi, rbx
0x3a033b f0x3a433b e840020000                     call 0x3a0580
0x3a0340 f0x3a4340 488d3db95f0009                 lea rdi, [rip + 0x9005fb9] rip->0x93a6300
0x3a0347 f0x3a4347 488d55b0                       lea rdx, [rbp - 0x50]
0x3a034b f0x3a434b 4c89f6                         mov rsi, r14
0x3a034e f0x3a434e e8ad6e0100                     call 0x3b7200
0x3a0353 f0x3a4353 e9f9010000                     jmp 0x3a0551
0x3a0358 f0x3a4358 4885c9                         test rcx, rcx
0x3a035b f0x3a435b 7420                           je 0x3a037d
0x3a035d f0x3a435d c5f8104108                     vmovups xmm0, xmmword ptr [rcx + 8]
0x3a0362 f0x3a4362 4c8935ff5f0009                 mov qword ptr [rip + 0x9005fff], r14 rip->0x93a6368
0x3a0369 f0x3a4369 c5f81105ff5f0009               vmovups xmmword ptr [rip + 0x9005fff], xmm0
0x3a0371 f0x3a4371 c5f8104108                     vmovups xmm0, xmmword ptr [rcx + 8]
0x3a0376 f0x3a4376 c5f8294590                     vmovaps xmmword ptr [rbp - 0x70], xmm0
0x3a037b f0x3a437b eb1c                           jmp 0x3a0399
0x3a037d f0x3a437d c5f857c0                       vxorps xmm0, xmm0, xmm0
0x3a0381 f0x3a4381 4c8935e05f0009                 mov qword ptr [rip + 0x9005fe0], r14 rip->0x93a6368
0x3a0388 f0x3a4388 c5f1efc9                       vpxor xmm1, xmm1, xmm1
0x3a038c f0x3a438c c5f8294590                     vmovaps xmmword ptr [rbp - 0x70], xmm0
0x3a0391 f0x3a4391 c5fa7f0dd75f0009               vmovdqu xmmword ptr [rip + 0x9005fd7], xmm1
0x3a0399 f0x3a4399 488d3d685f0009                 lea rdi, [rip + 0x9005f68] rip->0x93a6308
