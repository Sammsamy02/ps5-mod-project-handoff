
SLOT 0xb0 caller=0x361764 start=0x361750
0x361750 55                       push rbp
0x361751 4889e5                   mov rbp, rsp
0x361754 4156                     push r14
0x361756 53                       push rbx
0x361757 4c8b7770                 mov r14, qword ptr [rdi + 0x70]
0x36175b 4889fb                   mov rbx, rdi
0x36175e 498b06                   mov rax, qword ptr [r14]
0x361761 4c89f7                   mov rdi, r14
0x361764 ff90b0000000             call qword ptr [rax + 0xb0]
0x36176a 4c89f7                   mov rdi, r14
0x36176d e8de150800               call 0x3e2d50
0x361772 488b4370                 mov rax, qword ptr [rbx + 0x70]
0x361776 4889df                   mov rdi, rbx
0x361779 8b7030                   mov esi, dword ptr [rax + 0x30]
0x36177c e8df050800               call 0x3e1d60
0x361781 48b80002004010000000     movabs rax, 0x1040000200
0x36178b 480b4338                 or rax, qword ptr [rbx + 0x38]
0x36178f 48ba0000000000000800     movabs rdx, 0x8000000000000
0x361799 48894338                 mov qword ptr [rbx + 0x38], rax
0x36179d 488b4b70                 mov rcx, qword ptr [rbx + 0x70]
0x3617a1 48235138                 and rdx, qword ptr [rcx + 0x38]
0x3617a5 4809c2                   or rdx, rax
0x3617a8 48895338                 mov qword ptr [rbx + 0x38], rdx
0x3617ac 5b                       pop rbx
0x3617ad 415e                     pop r14
0x3617af 5d                       pop rbp
0x3617b0 c3                       ret 
0x3617b1 cc                       int3 
0x3617b2 cc                       int3 
0x3617b3 cc                       int3 
0x3617b4 cc                       int3 
0x3617b5 cc                       int3 
0x3617b6 cc                       int3 
0x3617b7 cc                       int3 
0x3617b8 cc                       int3 
0x3617b9 cc                       int3 
0x3617ba cc                       int3 
0x3617bb cc                       int3 
0x3617bc cc                       int3 
0x3617bd cc                       int3 
0x3617be cc                       int3 
0x3617bf cc                       int3 
0x3617c0 55                       push rbp
0x3617c1 4889e5                   mov rbp, rsp

SLOT 0xb0 caller=0x3c9284 start=0x3c9260
0x3c9260 55                       push rbp
0x3c9261 4889e5                   mov rbp, rsp
0x3c9264 4156                     push r14
0x3c9266 53                       push rbx
0x3c9267 448b7708                 mov r14d, dword ptr [rdi + 8]
0x3c926b 664585f6                 test r14w, r14w
0x3c926f 7819                     js 0x3c928a
0x3c9271 be00800000               mov esi, 0x8000
0x3c9276 4889fb                   mov rbx, rdi
0x3c9279 e892ccefff               call 0x2c5f10
0x3c927e 488b03                   mov rax, qword ptr [rbx]
0x3c9281 4889df                   mov rdi, rbx
0x3c9284 ff90b0000000             call qword ptr [rax + 0xb0]
0x3c928a 664585f6                 test r14w, r14w
0x3c928e 0f99c0                   setns al
0x3c9291 5b                       pop rbx
0x3c9292 415e                     pop r14
0x3c9294 5d                       pop rbp
0x3c9295 c3                       ret 
0x3c9296 cc                       int3 
0x3c9297 cc                       int3 
0x3c9298 cc                       int3 
0x3c9299 cc                       int3 
0x3c929a cc                       int3 
0x3c929b cc                       int3 
0x3c929c cc                       int3 
0x3c929d cc                       int3 
0x3c929e cc                       int3 
0x3c929f cc                       int3 
0x3c92a0 55                       push rbp
0x3c92a1 4889e5                   mov rbp, rsp
0x3c92a4 4157                     push r15
0x3c92a6 4156                     push r14
0x3c92a8 53                       push rbx
0x3c92a9 50                       push rax
0x3c92aa 448b7f08                 mov r15d, dword ptr [rdi + 8]
0x3c92ae 41f7c700000100           test r15d, 0x10000
0x3c92b5 7536                     jne 0x3c92ed
0x3c92b7 be00000100               mov esi, 0x10000
0x3c92bc 4889fb                   mov rbx, rdi
0x3c92bf e84cccefff               call 0x2c5f10
0x3c92c4 488b03                   mov rax, qword ptr [rbx]
0x3c92c7 4889df                   mov rdi, rbx
0x3c92ca ff90c0000000             call qword ptr [rax + 0xc0]
0x3c92d0 4c8d35390eff08           lea r14, [rip + 0x8ff0e39]
0x3c92d7 4889de                   mov rsi, rbx
0x3c92da 4c89f7                   mov rdi, r14
0x3c92dd e8ceac0b00               call 0x483fb0

SLOT 0xb0 caller=0x3cf19f start=0x3cf040
0x3cf040 55                       push rbp
0x3cf041 4889e5                   mov rbp, rsp
0x3cf044 4157                     push r15
0x3cf046 4156                     push r14
0x3cf048 53                       push rbx
0x3cf049 4881ec98000000           sub rsp, 0x98
0x3cf050 4c8b35b12ed608           mov r14, qword ptr [rip + 0x8d62eb1]
0x3cf057 498b06                   mov rax, qword ptr [r14]
0x3cf05a 488945e0                 mov qword ptr [rbp - 0x20], rax
0x3cf05e e82d7b0b00               call 0x486b90
0x3cf063 488d3572e84f07           lea rsi, [rip + 0x74fe872]
0x3cf06a 488dbd58ffffff           lea rdi, [rbp - 0xa8]
0x3cf071 ba01000000               mov edx, 1
0x3cf076 e815e5e6ff               call 0x23d590
0x3cf07b 4c8bbd58ffffff           mov r15, qword ptr [rbp - 0xa8]
0x3cf082 4d85ff                   test r15, r15
0x3cf085 750e                     jne 0x3cf095
0x3cf087 488d35f69f5c07           lea rsi, [rip + 0x75c9ff6]
0x3cf08e 31ff                     xor edi, edi
0x3cf090 e8eb440c00               call 0x493580
0x3cf095 e806a60000               call 0x3d96a0
0x3cf09a 488d9d60ffffff           lea rbx, [rbp - 0xa0]
0x3cf0a1 4889c6                   mov rsi, rax
0x3cf0a4 4889df                   mov rdi, rbx
0x3cf0a7 e884440c00               call 0x493530
0x3cf0ac 4889df                   mov rdi, rbx
0x3cf0af 48c78568ffffff00000000   mov qword ptr [rbp - 0x98], 0
0x3cf0ba 4c89bd70ffffff           mov qword ptr [rbp - 0x90], r15
0x3cf0c1 c78578ffffff40000000     mov dword ptr [rbp - 0x88], 0x40
0x3cf0cb c6458000                 mov byte ptr [rbp - 0x80], 0
0x3cf0cf c5f857c0                 vxorps xmm0, xmm0, xmm0
0x3cf0d3 c5f8114588               vmovups xmmword ptr [rbp - 0x78], xmm0
0x3cf0d8 48c7459800000000         mov qword ptr [rbp - 0x68], 0
0x3cf0e0 e8bb190c00               call 0x490aa0
0x3cf0e5 48837da000               cmp qword ptr [rbp - 0x60], 0
0x3cf0ea 4889c3                   mov rbx, rax
0x3cf0ed 7415                     je 0x3cf104
0x3cf0ef 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x3cf0f3 488d7db0                 lea rdi, [rbp - 0x50]
0x3cf0f7 4885c0                   test rax, rax
0x3cf0fa 480f45f8                 cmovne rdi, rax
0x3cf0fe 488b07                   mov rax, qword ptr [rdi]
0x3cf101 ff5010                   call qword ptr [rax + 0x10]
0x3cf104 48891d357ffd08           mov qword ptr [rip + 0x8fd7f35], rbx
0x3cf10b 488d0dfeaffe08           lea rcx, [rip + 0x8feaffe]
0x3cf112 8b430c                   mov eax, dword ptr [rbx + 0xc]
0x3cf115 394124                   cmp dword ptr [rcx + 0x24], eax
0x3cf118 7e19                     jle 0x3cf133
0x3cf11a 488b4910                 mov rcx, qword ptr [rcx + 0x10]
0x3cf11e 89c2                     mov edx, eax
0x3cf120 0fb7c0                   movzx eax, ax
0x3cf123 488d3c40                 lea rdi, [rax + rax*2]
0x3cf127 c1ea10                   shr edx, 0x10
0x3cf12a c1e703                   shl edi, 3
0x3cf12d 48033cd1                 add rdi, qword ptr [rcx + rdx*8]
0x3cf131 eb02                     jmp 0x3cf135
0x3cf133 31ff                     xor edi, edi
0x3cf135 be00000040               mov esi, 0x40000000
0x3cf13a e83193f9ff               call 0x368470
0x3cf13f 488d1dda63f508           lea rbx, [rip + 0x8f563da]
0x3cf146 488b3b                   mov rdi, qword ptr [rbx]
0x3cf149 4885ff                   test rdi, rdi
0x3cf14c 7508                     jne 0x3cf156
0x3cf14e e87d50ccff               call 0x941d0
0x3cf153 488b3b                   mov rdi, qword ptr [rbx]
0x3cf156 488b07                   mov rax, qword ptr [rdi]
0x3cf159 488d35b2766407           lea rsi, [rip + 0x76476b2]
0x3cf160 ba01000000               mov edx, 1
0x3cf165 ff90c0000000             call qword ptr [rax + 0xc0]
0x3cf16b 4885c0                   test rax, rax
0x3cf16e 7467                     je 0x3cf1d7
0x3cf170 4889c3                   mov rbx, rax
0x3cf173 e8a859daff               call 0x174b20
0x3cf178 488d35612f3307           lea rsi, [rip + 0x7332f61]
0x3cf17f 4889c7                   mov rdi, rax
0x3cf182 e8b9b5e0ff               call 0x1da740
0x3cf187 84c0                     test al, al
0x3cf189 741a                     je 0x3cf1a5
0x3cf18b 488b03                   mov rax, qword ptr [rbx]
0x3cf18e 488d353b173307           lea rsi, [rip + 0x733173b]
0x3cf195 4889df                   mov rdi, rbx
0x3cf198 ba0000000c               mov edx, 0xc000000
0x3cf19d 31c9                     xor ecx, ecx
0x3cf19f ff90b0000000             call qword ptr [rax + 0xb0]
0x3cf1a5 e87659daff               call 0x174b20
0x3cf1aa 488d3567ad5c07           lea rsi, [rip + 0x75cad67]
0x3cf1b1 4889c7                   mov rdi, rax
0x3cf1b4 e887b5e0ff               call 0x1da740
0x3cf1b9 84c0                     test al, al
0x3cf1bb 741a                     je 0x3cf1d7
0x3cf1bd 488b03                   mov rax, qword ptr [rbx]
0x3cf1c0 488d35afda1b07           lea rsi, [rip + 0x71bdaaf]
0x3cf1c7 4889df                   mov rdi, rbx
0x3cf1ca ba0000000c               mov edx, 0xc000000
0x3cf1cf 31c9                     xor ecx, ecx
0x3cf1d1 ff90b0000000             call qword ptr [rax + 0xb0]
0x3cf1d7 498b06                   mov rax, qword ptr [r14]
0x3cf1da 483b45e0                 cmp rax, qword ptr [rbp - 0x20]
0x3cf1de 750e                     jne 0x3cf1ee
0x3cf1e0 4881c498000000           add rsp, 0x98
0x3cf1e7 5b                       pop rbx
0x3cf1e8 415e                     pop r14
0x3cf1ea 415f                     pop r15
0x3cf1ec 5d                       pop rbp
0x3cf1ed c3                       ret 
0x3cf1ee e86d2c0a07               call 0x7471e60
0x3cf1f3 0f0b                     ud2 
0x3cf1f5 cc                       int3 
0x3cf1f6 cc                       int3 
0x3cf1f7 cc                       int3 
0x3cf1f8 cc                       int3 
0x3cf1f9 cc                       int3 
0x3cf1fa cc                       int3 
0x3cf1fb cc                       int3 
0x3cf1fc cc                       int3 
0x3cf1fd cc                       int3 
0x3cf1fe cc                       int3 

SLOT 0xb0 caller=0x3cf1d1 start=0x3cf040
0x3cf040 55                       push rbp
0x3cf041 4889e5                   mov rbp, rsp
0x3cf044 4157                     push r15
0x3cf046 4156                     push r14
0x3cf048 53                       push rbx
0x3cf049 4881ec98000000           sub rsp, 0x98
0x3cf050 4c8b35b12ed608           mov r14, qword ptr [rip + 0x8d62eb1]
0x3cf057 498b06                   mov rax, qword ptr [r14]
0x3cf05a 488945e0                 mov qword ptr [rbp - 0x20], rax
0x3cf05e e82d7b0b00               call 0x486b90
0x3cf063 488d3572e84f07           lea rsi, [rip + 0x74fe872]
0x3cf06a 488dbd58ffffff           lea rdi, [rbp - 0xa8]
0x3cf071 ba01000000               mov edx, 1
0x3cf076 e815e5e6ff               call 0x23d590
0x3cf07b 4c8bbd58ffffff           mov r15, qword ptr [rbp - 0xa8]
0x3cf082 4d85ff                   test r15, r15
0x3cf085 750e                     jne 0x3cf095
0x3cf087 488d35f69f5c07           lea rsi, [rip + 0x75c9ff6]
0x3cf08e 31ff                     xor edi, edi
0x3cf090 e8eb440c00               call 0x493580
0x3cf095 e806a60000               call 0x3d96a0
0x3cf09a 488d9d60ffffff           lea rbx, [rbp - 0xa0]
0x3cf0a1 4889c6                   mov rsi, rax
0x3cf0a4 4889df                   mov rdi, rbx
0x3cf0a7 e884440c00               call 0x493530
0x3cf0ac 4889df                   mov rdi, rbx
0x3cf0af 48c78568ffffff00000000   mov qword ptr [rbp - 0x98], 0
0x3cf0ba 4c89bd70ffffff           mov qword ptr [rbp - 0x90], r15
0x3cf0c1 c78578ffffff40000000     mov dword ptr [rbp - 0x88], 0x40
0x3cf0cb c6458000                 mov byte ptr [rbp - 0x80], 0
0x3cf0cf c5f857c0                 vxorps xmm0, xmm0, xmm0
0x3cf0d3 c5f8114588               vmovups xmmword ptr [rbp - 0x78], xmm0
0x3cf0d8 48c7459800000000         mov qword ptr [rbp - 0x68], 0
0x3cf0e0 e8bb190c00               call 0x490aa0
0x3cf0e5 48837da000               cmp qword ptr [rbp - 0x60], 0
0x3cf0ea 4889c3                   mov rbx, rax
0x3cf0ed 7415                     je 0x3cf104
0x3cf0ef 488b45c8                 mov rax, qword ptr [rbp - 0x38]
0x3cf0f3 488d7db0                 lea rdi, [rbp - 0x50]
0x3cf0f7 4885c0                   test rax, rax
0x3cf0fa 480f45f8                 cmovne rdi, rax
0x3cf0fe 488b07                   mov rax, qword ptr [rdi]
0x3cf101 ff5010                   call qword ptr [rax + 0x10]
0x3cf104 48891d357ffd08           mov qword ptr [rip + 0x8fd7f35], rbx
0x3cf10b 488d0dfeaffe08           lea rcx, [rip + 0x8feaffe]
0x3cf112 8b430c                   mov eax, dword ptr [rbx + 0xc]
0x3cf115 394124                   cmp dword ptr [rcx + 0x24], eax
0x3cf118 7e19                     jle 0x3cf133
0x3cf11a 488b4910                 mov rcx, qword ptr [rcx + 0x10]
0x3cf11e 89c2                     mov edx, eax
0x3cf120 0fb7c0                   movzx eax, ax
0x3cf123 488d3c40                 lea rdi, [rax + rax*2]
0x3cf127 c1ea10                   shr edx, 0x10
0x3cf12a c1e703                   shl edi, 3
0x3cf12d 48033cd1                 add rdi, qword ptr [rcx + rdx*8]
0x3cf131 eb02                     jmp 0x3cf135
0x3cf133 31ff                     xor edi, edi
0x3cf135 be00000040               mov esi, 0x40000000
0x3cf13a e83193f9ff               call 0x368470
0x3cf13f 488d1dda63f508           lea rbx, [rip + 0x8f563da]
0x3cf146 488b3b                   mov rdi, qword ptr [rbx]
0x3cf149 4885ff                   test rdi, rdi
0x3cf14c 7508                     jne 0x3cf156
0x3cf14e e87d50ccff               call 0x941d0
0x3cf153 488b3b                   mov rdi, qword ptr [rbx]
0x3cf156 488b07                   mov rax, qword ptr [rdi]
0x3cf159 488d35b2766407           lea rsi, [rip + 0x76476b2]
0x3cf160 ba01000000               mov edx, 1
0x3cf165 ff90c0000000             call qword ptr [rax + 0xc0]
0x3cf16b 4885c0                   test rax, rax
0x3cf16e 7467                     je 0x3cf1d7
0x3cf170 4889c3                   mov rbx, rax
0x3cf173 e8a859daff               call 0x174b20
0x3cf178 488d35612f3307           lea rsi, [rip + 0x7332f61]
0x3cf17f 4889c7                   mov rdi, rax
0x3cf182 e8b9b5e0ff               call 0x1da740
0x3cf187 84c0                     test al, al
0x3cf189 741a                     je 0x3cf1a5
0x3cf18b 488b03                   mov rax, qword ptr [rbx]
0x3cf18e 488d353b173307           lea rsi, [rip + 0x733173b]
0x3cf195 4889df                   mov rdi, rbx
0x3cf198 ba0000000c               mov edx, 0xc000000
0x3cf19d 31c9                     xor ecx, ecx
0x3cf19f ff90b0000000             call qword ptr [rax + 0xb0]
0x3cf1a5 e87659daff               call 0x174b20
0x3cf1aa 488d3567ad5c07           lea rsi, [rip + 0x75cad67]
0x3cf1b1 4889c7                   mov rdi, rax
0x3cf1b4 e887b5e0ff               call 0x1da740
0x3cf1b9 84c0                     test al, al
0x3cf1bb 741a                     je 0x3cf1d7
0x3cf1bd 488b03                   mov rax, qword ptr [rbx]
0x3cf1c0 488d35afda1b07           lea rsi, [rip + 0x71bdaaf]
0x3cf1c7 4889df                   mov rdi, rbx
0x3cf1ca ba0000000c               mov edx, 0xc000000
0x3cf1cf 31c9                     xor ecx, ecx
0x3cf1d1 ff90b0000000             call qword ptr [rax + 0xb0]
0x3cf1d7 498b06                   mov rax, qword ptr [r14]
0x3cf1da 483b45e0                 cmp rax, qword ptr [rbp - 0x20]
0x3cf1de 750e                     jne 0x3cf1ee
0x3cf1e0 4881c498000000           add rsp, 0x98
0x3cf1e7 5b                       pop rbx
0x3cf1e8 415e                     pop r14
0x3cf1ea 415f                     pop r15
0x3cf1ec 5d                       pop rbp
0x3cf1ed c3                       ret 
0x3cf1ee e86d2c0a07               call 0x7471e60
0x3cf1f3 0f0b                     ud2 
0x3cf1f5 cc                       int3 
0x3cf1f6 cc                       int3 
0x3cf1f7 cc                       int3 
0x3cf1f8 cc                       int3 
0x3cf1f9 cc                       int3 
0x3cf1fa cc                       int3 
0x3cf1fb cc                       int3 
0x3cf1fc cc                       int3 
0x3cf1fd cc                       int3 
0x3cf1fe cc                       int3 
0x3cf1ff cc                       int3 
0x3cf200 55                       push rbp
0x3cf201 4889e5                   mov rbp, rsp
0x3cf204 4157                     push r15
0x3cf206 4156                     push r14
0x3cf208 4154                     push r12
0x3cf20a 53                       push rbx
0x3cf20b 4881eca0000000           sub rsp, 0xa0
0x3cf212 4c8b25ef2cd608           mov r12, qword ptr [rip + 0x8d62cef]
0x3cf219 498b0424                 mov rax, qword ptr [r12]
0x3cf21d 488945d8                 mov qword ptr [rbp - 0x28], rax
0x3cf221 488b87e0000000           mov rax, qword ptr [rdi + 0xe0]
0x3cf228 4885c0                   test rax, rax
0x3cf22b 0f94c1                   sete cl
0x3cf22e 4839f8                   cmp rax, rdi

SLOT 0xb0 caller=0x3e5f37 start=0x3e5f20
0x3e5f20 55                       push rbp
0x3e5f21 4889e5                   mov rbp, rsp
0x3e5f24 4157                     push r15
0x3e5f26 4156                     push r14
0x3e5f28 53                       push rbx
0x3e5f29 50                       push rax
0x3e5f2a 488b5f78                 mov rbx, qword ptr [rdi + 0x78]
0x3e5f2e 4989fe                   mov r14, rdi
0x3e5f31 488b03                   mov rax, qword ptr [rbx]
0x3e5f34 4889df                   mov rdi, rbx
0x3e5f37 ff90b0000000             call qword ptr [rax + 0xb0]
0x3e5f3d 4c8d7b10                 lea r15, [rbx + 0x10]
0x3e5f41 e86afcfdff               call 0x3c5bb0
0x3e5f46 4c89ff                   mov rdi, r15
0x3e5f49 4889c6                   mov rsi, rax
0x3e5f4c e8dfdef7ff               call 0x363e30
0x3e5f51 84c0                     test al, al
0x3e5f53 7416                     je 0x3e5f6b
0x3e5f55 498b07                   mov rax, qword ptr [r15]
0x3e5f58 4883e0fe                 and rax, 0xfffffffffffffffe
0x3e5f5c 740d                     je 0x3e5f6b
0x3e5f5e 488b4810                 mov rcx, qword ptr [rax + 0x10]
0x3e5f62 f681d800000008           test byte ptr [rcx + 0xd8], 8
0x3e5f69 7535                     jne 0x3e5fa0
0x3e5f6b 488b03                   mov rax, qword ptr [rbx]
0x3e5f6e 4889df                   mov rdi, rbx
0x3e5f71 ff9070010000             call qword ptr [rax + 0x170]
0x3e5f77 31c0                     xor eax, eax
0x3e5f79 894344                   mov dword ptr [rbx + 0x44], eax
0x3e5f7c 8b7b30                   mov edi, dword ptr [rbx + 0x30]
0x3e5f7f 0faf7b2c                 imul edi, dword ptr [rbx + 0x2c]
0x3e5f83 01c7                     add edi, eax
0x3e5f85 81ffffffff7f             cmp edi, 0x7fffffff
0x3e5f8b 732e                     jae 0x3e5fbb
0x3e5f8d 41c7463010000000         mov dword ptr [r14 + 0x30], 0x10

SLOT 0x150 caller=0x3ea881 start=0x3ea7f5
0x3ea7f5 55                       push rbp
0x3ea7f6 4889e5                   mov rbp, rsp
0x3ea7f9 4157                     push r15
0x3ea7fb 4156                     push r14
0x3ea7fd 4155                     push r13
0x3ea7ff 4154                     push r12
0x3ea801 53                       push rbx
0x3ea802 50                       push rax
0x3ea803 4989fe                   mov r14, rdi
0x3ea806 488b7f78                 mov rdi, qword ptr [rdi + 0x78]
0x3ea80a 0fb7473b                 movzx eax, word ptr [rdi + 0x3b]
0x3ea80e a940100000               test eax, 0x1040
0x3ea813 740f                     je 0x3ea824
0x3ea815 4883c408                 add rsp, 8
0x3ea819 5b                       pop rbx
0x3ea81a 415c                     pop r12
0x3ea81c 415d                     pop r13
0x3ea81e 415e                     pop r14
0x3ea820 415f                     pop r15
0x3ea822 5d                       pop rbp
0x3ea823 c3                       ret 
0x3ea824 488b07                   mov rax, qword ptr [rdi]
0x3ea827 4c637f30                 movsxd r15, dword ptr [rdi + 0x30]
0x3ea82b 4889f3                   mov rbx, rsi
0x3ea82e ff9070010000             call qword ptr [rax + 0x170]
0x3ea834 8b4308                   mov eax, dword ptr [rbx + 8]
0x3ea837 85c0                     test eax, eax
0x3ea839 7eda                     jle 0x3ea815
0x3ea83b 41f6467001               test byte ptr [r14 + 0x70], 1
0x3ea840 48ba0000004010000000     movabs rdx, 0x1040000000
0x3ea84a 754a                     jne 0x3ea896
0x3ea84c 4531e4                   xor r12d, r12d
0x3ea84f 4531ed                   xor r13d, r13d
0x3ea852 eb1a                     jmp 0x3ea86e
0x3ea854 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3ea860 49ffc5                   inc r13
0x3ea863 4863c8                   movsxd rcx, eax
0x3ea866 4d01fc                   add r12, r15
0x3ea869 4939cd                   cmp r13, rcx
0x3ea86c 7da7                     jge 0x3ea815
0x3ea86e 498b7e78                 mov rdi, qword ptr [r14 + 0x78]
0x3ea872 48855738                 test qword ptr [rdi + 0x38], rdx
0x3ea876 75e8                     jne 0x3ea860
0x3ea878 488b33                   mov rsi, qword ptr [rbx]
0x3ea87b 488b07                   mov rax, qword ptr [rdi]
0x3ea87e 4c01e6                   add rsi, r12
0x3ea881 ff9050010000             call qword ptr [rax + 0x150]
0x3ea887 8b4308                   mov eax, dword ptr [rbx + 8]
0x3ea88a 48ba0000004010000000     movabs rdx, 0x1040000000
0x3ea894 ebca                     jmp 0x3ea860
0x3ea896 4531ed                   xor r13d, r13d
0x3ea899 4531e4                   xor r12d, r12d
0x3ea89c eb14                     jmp 0x3ea8b2
0x3ea89e 6690                     nop 
0x3ea8a0 49ffc4                   inc r12
0x3ea8a3 4863c8                   movsxd rcx, eax
0x3ea8a6 4d01fd                   add r13, r15
0x3ea8a9 4939cc                   cmp r12, rcx
0x3ea8ac 0f8d63ffffff             jge 0x3ea815
0x3ea8b2 498b7e78                 mov rdi, qword ptr [r14 + 0x78]
0x3ea8b6 48855738                 test qword ptr [rdi + 0x38], rdx
0x3ea8ba 75e4                     jne 0x3ea8a0
0x3ea8bc 488b03                   mov rax, qword ptr [rbx]
0x3ea8bf 4889c6                   mov rsi, rax
0x3ea8c2 48c1fe18                 sar rsi, 0x18
0x3ea8c6 4801de                   add rsi, rbx
0x3ea8c9 a801                     test al, 1
0x3ea8cb 480f44f0                 cmove rsi, rax
0x3ea8cf 488b07                   mov rax, qword ptr [rdi]
0x3ea8d2 4c01ee                   add rsi, r13
0x3ea8d5 ff9050010000             call qword ptr [rax + 0x150]
0x3ea8db 8b4308                   mov eax, dword ptr [rbx + 8]

SLOT 0x150 caller=0x3ea8d5 start=0x3ea7f5
0x3ea7f5 55                       push rbp
0x3ea7f6 4889e5                   mov rbp, rsp
0x3ea7f9 4157                     push r15
0x3ea7fb 4156                     push r14
0x3ea7fd 4155                     push r13
0x3ea7ff 4154                     push r12
0x3ea801 53                       push rbx
0x3ea802 50                       push rax
0x3ea803 4989fe                   mov r14, rdi
0x3ea806 488b7f78                 mov rdi, qword ptr [rdi + 0x78]
0x3ea80a 0fb7473b                 movzx eax, word ptr [rdi + 0x3b]
0x3ea80e a940100000               test eax, 0x1040
0x3ea813 740f                     je 0x3ea824
0x3ea815 4883c408                 add rsp, 8
0x3ea819 5b                       pop rbx
0x3ea81a 415c                     pop r12
0x3ea81c 415d                     pop r13
0x3ea81e 415e                     pop r14
0x3ea820 415f                     pop r15
0x3ea822 5d                       pop rbp
0x3ea823 c3                       ret 
0x3ea824 488b07                   mov rax, qword ptr [rdi]
0x3ea827 4c637f30                 movsxd r15, dword ptr [rdi + 0x30]
0x3ea82b 4889f3                   mov rbx, rsi
0x3ea82e ff9070010000             call qword ptr [rax + 0x170]
0x3ea834 8b4308                   mov eax, dword ptr [rbx + 8]
0x3ea837 85c0                     test eax, eax
0x3ea839 7eda                     jle 0x3ea815
0x3ea83b 41f6467001               test byte ptr [r14 + 0x70], 1
0x3ea840 48ba0000004010000000     movabs rdx, 0x1040000000
0x3ea84a 754a                     jne 0x3ea896
0x3ea84c 4531e4                   xor r12d, r12d
0x3ea84f 4531ed                   xor r13d, r13d
0x3ea852 eb1a                     jmp 0x3ea86e
0x3ea854 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x3ea860 49ffc5                   inc r13
0x3ea863 4863c8                   movsxd rcx, eax
0x3ea866 4d01fc                   add r12, r15
0x3ea869 4939cd                   cmp r13, rcx
0x3ea86c 7da7                     jge 0x3ea815
0x3ea86e 498b7e78                 mov rdi, qword ptr [r14 + 0x78]
0x3ea872 48855738                 test qword ptr [rdi + 0x38], rdx
0x3ea876 75e8                     jne 0x3ea860
0x3ea878 488b33                   mov rsi, qword ptr [rbx]
0x3ea87b 488b07                   mov rax, qword ptr [rdi]
0x3ea87e 4c01e6                   add rsi, r12
0x3ea881 ff9050010000             call qword ptr [rax + 0x150]
0x3ea887 8b4308                   mov eax, dword ptr [rbx + 8]
0x3ea88a 48ba0000004010000000     movabs rdx, 0x1040000000
0x3ea894 ebca                     jmp 0x3ea860
0x3ea896 4531ed                   xor r13d, r13d
0x3ea899 4531e4                   xor r12d, r12d
0x3ea89c eb14                     jmp 0x3ea8b2
0x3ea89e 6690                     nop 
0x3ea8a0 49ffc4                   inc r12
0x3ea8a3 4863c8                   movsxd rcx, eax
0x3ea8a6 4d01fd                   add r13, r15
0x3ea8a9 4939cc                   cmp r12, rcx
0x3ea8ac 0f8d63ffffff             jge 0x3ea815
0x3ea8b2 498b7e78                 mov rdi, qword ptr [r14 + 0x78]
0x3ea8b6 48855738                 test qword ptr [rdi + 0x38], rdx
0x3ea8ba 75e4                     jne 0x3ea8a0
0x3ea8bc 488b03                   mov rax, qword ptr [rbx]
0x3ea8bf 4889c6                   mov rsi, rax
0x3ea8c2 48c1fe18                 sar rsi, 0x18
0x3ea8c6 4801de                   add rsi, rbx
0x3ea8c9 a801                     test al, 1
0x3ea8cb 480f44f0                 cmove rsi, rax
0x3ea8cf 488b07                   mov rax, qword ptr [rdi]
0x3ea8d2 4c01ee                   add rsi, r13
0x3ea8d5 ff9050010000             call qword ptr [rax + 0x150]
0x3ea8db 8b4308                   mov eax, dword ptr [rbx + 8]
0x3ea8de 48ba0000004010000000     movabs rdx, 0x1040000000
0x3ea8e8 ebb6                     jmp 0x3ea8a0
0x3ea8ea cc                       int3 
0x3ea8eb cc                       int3 
0x3ea8ec cc                       int3 
0x3ea8ed cc                       int3 
0x3ea8ee cc                       int3 
0x3ea8ef cc                       int3 
0x3ea8f0 55                       push rbp
0x3ea8f1 4889e5                   mov rbp, rsp
0x3ea8f4 4157                     push r15
0x3ea8f6 4156                     push r14
0x3ea8f8 4155                     push r13
0x3ea8fa 4154                     push r12
0x3ea8fc 53                       push rbx
0x3ea8fd 4883e4e0                 and rsp, 0xffffffffffffffe0
0x3ea901 4881eca0000000           sub rsp, 0xa0
0x3ea908 4889e3                   mov rbx, rsp
0x3ea90b 488b05f675d408           mov rax, qword ptr [rip + 0x8d475f6]
0x3ea912 4c894340                 mov qword ptr [rbx + 0x40], r8
0x3ea916 48894b68                 mov qword ptr [rbx + 0x68], rcx
0x3ea91a 48897b28                 mov qword ptr [rbx + 0x28], rdi
0x3ea91e 488b00                   mov rax, qword ptr [rax]
0x3ea921 48898380000000           mov qword ptr [rbx + 0x80], rax
0x3ea928 4885f6                   test rsi, rsi
0x3ea92b 0f8418020000             je 0x3eab49
0x3ea931 488b4328                 mov rax, qword ptr [rbx + 0x28]

SLOT 0x158 caller=0x3d31f6 start=0x3d3130
0x3d3130 55                       push rbp
0x3d3131 4889e5                   mov rbp, rsp
0x3d3134 4157                     push r15
0x3d3136 4156                     push r14
0x3d3138 4154                     push r12
0x3d313a 53                       push rbx
0x3d313b 4989fe                   mov r14, rdi
0x3d313e 488b7f08                 mov rdi, qword ptr [rdi + 8]
0x3d3142 4189f7                   mov r15d, esi
0x3d3145 41f6461801               test byte ptr [r14 + 0x18], 1
0x3d314a 418b5610                 mov edx, dword ptr [r14 + 0x10]
0x3d314e 418b4e14                 mov ecx, dword ptr [r14 + 0x14]
0x3d3152 48635f08                 movsxd rbx, dword ptr [rdi + 8]
0x3d3156 428d043b                 lea eax, [rbx + r15]
0x3d315a 894708                   mov dword ptr [rdi + 8], eax
0x3d315d 8b770c                   mov esi, dword ptr [rdi + 0xc]
0x3d3160 750d                     jne 0x3d316f
0x3d3162 39f0                     cmp eax, esi
0x3d3164 7e14                     jle 0x3d317a
0x3d3166 89de                     mov esi, ebx
0x3d3168 e893c1f5ff               call 0x32f300
0x3d316d eb0b                     jmp 0x3d317a
0x3d316f 39f0                     cmp eax, esi
0x3d3171 7e07                     jle 0x3d317a
0x3d3173 89de                     mov esi, ebx
0x3d3175 e8e6010000               call 0x3d3360
0x3d317a 4585ff                   test r15d, r15d
0x3d317d 0f8eb0000000             jle 0x3d3233
0x3d3183 498b4e08                 mov rcx, qword ptr [r14 + 8]
0x3d3187 83790800                 cmp dword ptr [rcx + 8], 0
0x3d318b 740c                     je 0x3d3199
0x3d318d 41f6461801               test byte ptr [r14 + 0x18], 1
0x3d3192 7513                     jne 0x3d31a7
0x3d3194 488b01                   mov rax, qword ptr [rcx]
0x3d3197 eb22                     jmp 0x3d31bb
0x3d3199 4531e4                   xor r12d, r12d
0x3d319c 498b06                   mov rax, qword ptr [r14]
0x3d319f f6403902                 test byte ptr [rax + 0x39], 2
0x3d31a3 7463                     je 0x3d3208
0x3d31a5 eb28                     jmp 0x3d31cf
0x3d31a7 488b11                   mov rdx, qword ptr [rcx]
0x3d31aa 4889d0                   mov rax, rdx
0x3d31ad 48c1f818                 sar rax, 0x18
0x3d31b1 4801c8                   add rax, rcx
0x3d31b4 f6c201                   test dl, 1
0x3d31b7 480f44c2                 cmove rax, rdx
0x3d31bb 4d636610                 movsxd r12, dword ptr [r14 + 0x10]
0x3d31bf 4c0fafe3                 imul r12, rbx
0x3d31c3 4901c4                   add r12, rax
0x3d31c6 498b06                   mov rax, qword ptr [r14]
0x3d31c9 f6403902                 test byte ptr [rax + 0x39], 2
0x3d31cd 7439                     je 0x3d3208
0x3d31cf 49635610                 movsxd rdx, dword ptr [r14 + 0x10]
0x3d31d3 4489f8                   mov eax, r15d
0x3d31d6 4c89e7                   mov rdi, r12
0x3d31d9 31f6                     xor esi, esi
0x3d31db 480fafd0                 imul rdx, rax
0x3d31df e85cec0907               call 0x7471e40
0x3d31e4 eb4d                     jmp 0x3d3233
0x3d31e6 662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x3d31f0 488b07                   mov rax, qword ptr [rdi]
0x3d31f3 4c89e6                   mov rsi, r12
0x3d31f6 ff9058010000             call qword ptr [rax + 0x158]
0x3d31fc 49634610                 movsxd rax, dword ptr [r14 + 0x10]
0x3d3200 4901c4                   add r12, rax
0x3d3203 41ffcf                   dec r15d
0x3d3206 742b                     je 0x3d3233
0x3d3208 498b3e                   mov rdi, qword ptr [r14]
0x3d320b f6473902                 test byte ptr [rdi + 0x39], 2
0x3d320f 74df                     je 0x3d31f0
0x3d3211 48634730                 movsxd rax, dword ptr [rdi + 0x30]
0x3d3215 4863572c                 movsxd rdx, dword ptr [rdi + 0x2c]
0x3d3219 4c89e7                   mov rdi, r12
0x3d321c 31f6                     xor esi, esi
0x3d321e 480fafd0                 imul rdx, rax
0x3d3222 e819ec0907               call 0x7471e40
0x3d3227 49634610                 movsxd rax, dword ptr [r14 + 0x10]
0x3d322b 4901c4                   add r12, rax
0x3d322e 41ffcf                   dec r15d
0x3d3231 75d5                     jne 0x3d3208
0x3d3233 89d8                     mov eax, ebx
0x3d3235 5b                       pop rbx
0x3d3236 415c                     pop r12
0x3d3238 415e                     pop r14
0x3d323a 415f                     pop r15
0x3d323c 5d                       pop rbp
0x3d323d c3                       ret 
0x3d323e cc                       int3 
0x3d323f cc                       int3 
0x3d3240 55                       push rbp
0x3d3241 4889e5                   mov rbp, rsp
0x3d3244 4157                     push r15
0x3d3246 4156                     push r14
0x3d3248 4155                     push r13
0x3d324a 4154                     push r12
0x3d324c 53                       push rbx
0x3d324d 50                       push rax
0x3d324e 89cb                     mov ebx, ecx
0x3d3250 4989ff                   mov r15, rdi
0x3d3253 4c63f2                   movsxd r14, edx

SLOT 0x158 caller=0x3d7702 start=0x3d7690
0x3d7690 55                       push rbp
0x3d7691 4889e5                   mov rbp, rsp
0x3d7694 4157                     push r15
0x3d7696 4156                     push r14
0x3d7698 4155                     push r13
0x3d769a 4154                     push r12
0x3d769c 53                       push rbx
0x3d769d 4883e4e0                 and rsp, 0xffffffffffffffe0
0x3d76a1 4883ec60                 sub rsp, 0x60
0x3d76a5 4889e3                   mov rbx, rsp
0x3d76a8 488b0559a8d508           mov rax, qword ptr [rip + 0x8d5a859]
0x3d76af 48897b10                 mov qword ptr [rbx + 0x10], rdi
0x3d76b3 488b00                   mov rax, qword ptr [rax]
0x3d76b6 48894340                 mov qword ptr [rbx + 0x40], rax
0x3d76ba 48897308                 mov qword ptr [rbx + 8], rsi
0x3d76be 488b3e                   mov rdi, qword ptr [rsi]
0x3d76c1 4885ff                   test rdi, rdi
0x3d76c4 740a                     je 0x3d76d0
0x3d76c6 488b4708                 mov rax, qword ptr [rdi + 8]
0x3d76ca f6401304                 test byte ptr [rax + 0x13], 4
0x3d76ce 753d                     jne 0x3d770d
0x3d76d0 48634624                 movsxd rax, dword ptr [rsi + 0x24]
0x3d76d4 4885c0                   test rax, rax
0x3d76d7 0f8498000000             je 0x3d7775
0x3d76dd 4883c01e                 add rax, 0x1e
0x3d76e1 4989e5                   mov r13, rsp
0x3d76e4 4883e0f0                 and rax, 0xfffffffffffffff0
0x3d76e8 4929c5                   sub r13, rax
0x3d76eb 4983e5e0                 and r13, 0xffffffffffffffe0
0x3d76ef 4c89ec                   mov rsp, r13
0x3d76f2 f6473902                 test byte ptr [rdi + 0x39], 2
0x3d76f6 0f8586000000             jne 0x3d7782
0x3d76fc 488b07                   mov rax, qword ptr [rdi]
0x3d76ff 4c89ee                   mov rsi, r13
0x3d7702 ff9058010000             call qword ptr [rax + 0x158]
0x3d7708 e98b000000               jmp 0x3d7798
0x3d770d 488b4610                 mov rax, qword ptr [rsi + 0x10]
0x3d7711 49bc00000000ffffffff     movabs r12, 0xffffffff00000000
0x3d771b 8b4808                   mov ecx, dword ptr [rax + 8]
0x3d771e 85c9                     test ecx, ecx
0x3d7720 0f8e7a020000             jle 0x3d79a0
0x3d7726 f6463001                 test byte ptr [rsi + 0x30], 1
0x3d772a 0f852b010000             jne 0x3d785b
0x3d7730 4c635828                 movsxd r11, dword ptr [rax + 0x28]
0x3d7734 488d5010                 lea rdx, [rax + 0x10]
0x3d7738 4531d2                   xor r10d, r10d
0x3d773b eb12                     jmp 0x3d774f
0x3d773d 0f1f00                   nop dword ptr [rax]
0x3d7740 49ffc2                   inc r10
0x3d7743 4d89d4                   mov r12, r10
0x3d7746 4c39d1                   cmp rcx, r10
0x3d7749 0f8451020000             je 0x3d79a0
0x3d774f 4d39da                   cmp r10, r11
0x3d7752 7dec                     jge 0x3d7740
0x3d7754 4c8b4020                 mov r8, qword ptr [rax + 0x20]
0x3d7758 4589d1                   mov r9d, r10d
0x3d775b 4d85c0                   test r8, r8
0x3d775e 4c0f44c2                 cmove r8, rdx

SLOT 0x158 caller=0x3e2e4b start=0x3e2e10
0x3e2e10 55                       push rbp
0x3e2e11 4889e5                   mov rbp, rsp
0x3e2e14 4156                     push r14
0x3e2e16 53                       push rbx
0x3e2e17 4863472c                 movsxd rax, dword ptr [rdi + 0x2c]
0x3e2e1b 4c637730                 movsxd r14, dword ptr [rdi + 0x30]
0x3e2e1f 4889fb                   mov rbx, rdi
0x3e2e22 4c0faff0                 imul r14, rax
0x3e2e26 488b07                   mov rax, qword ptr [rdi]
0x3e2e29 ff9070010000             call qword ptr [rax + 0x170]
0x3e2e2f 4c89f7                   mov rdi, r14
0x3e2e32 89c6                     mov esi, eax
0x3e2e34 e817c2cdff               call 0xbf050
0x3e2e39 f6433902                 test byte ptr [rbx + 0x39], 2
0x3e2e3d 7515                     jne 0x3e2e54
0x3e2e3f 488b0b                   mov rcx, qword ptr [rbx]
0x3e2e42 4889df                   mov rdi, rbx
0x3e2e45 4889c6                   mov rsi, rax
0x3e2e48 4889c3                   mov rbx, rax
0x3e2e4b ff9158010000             call qword ptr [rcx + 0x158]
0x3e2e51 4889d8                   mov rax, rbx
0x3e2e54 5b                       pop rbx
0x3e2e55 415e                     pop r14
0x3e2e57 5d                       pop rbp
0x3e2e58 c3                       ret 
0x3e2e59 cc                       int3 
0x3e2e5a cc                       int3 
0x3e2e5b cc                       int3 
0x3e2e5c cc                       int3 
0x3e2e5d cc                       int3 
0x3e2e5e cc                       int3 
0x3e2e5f cc                       int3 
0x3e2e60 4885f6                   test rsi, rsi
0x3e2e63 7429                     je 0x3e2e8e
0x3e2e65 55                       push rbp
0x3e2e66 4889e5                   mov rbp, rsp
0x3e2e69 53                       push rbx
0x3e2e6a 50                       push rax
0x3e2e6b f6473c10                 test byte ptr [rdi + 0x3c], 0x10
0x3e2e6f 750f                     jne 0x3e2e80
0x3e2e71 488b07                   mov rax, qword ptr [rdi]
0x3e2e74 4889f3                   mov rbx, rsi
0x3e2e77 ff9040010000             call qword ptr [rax + 0x140]
0x3e2e7d 4889de                   mov rsi, rbx
0x3e2e80 4889f7                   mov rdi, rsi
0x3e2e83 e8084ccdff               call 0xb7a90
0x3e2e88 4883c408                 add rsp, 8
0x3e2e8c 5b                       pop rbx
0x3e2e8d 5d                       pop rbp
0x3e2e8e c3                       ret 
0x3e2e8f cc                       int3 
0x3e2e90 55                       push rbp
0x3e2e91 4889e5                   mov rbp, rsp
0x3e2e94 48637730                 movsxd rsi, dword ptr [rdi + 0x30]
0x3e2e98 4863c1                   movsxd rax, ecx
0x3e2e9b 480fafc6                 imul rax, rsi
0x3e2e9f 4801d0                   add rax, rdx
0x3e2ea2 5d                       pop rbp
0x3e2ea3 c3                       ret 
0x3e2ea4 cc                       int3 
0x3e2ea5 cc                       int3 
0x3e2ea6 cc                       int3 
0x3e2ea7 cc                       int3 
0x3e2ea8 cc                       int3 
0x3e2ea9 cc                       int3 
0x3e2eaa cc                       int3 

SLOT 0x158 caller=0x3e995b start=0x3e98f0
0x3e98f0 55                       push rbp
0x3e98f1 4889e5                   mov rbp, rsp
0x3e98f4 4157                     push r15
0x3e98f6 4156                     push r14
0x3e98f8 4155                     push r13
0x3e98fa 4154                     push r12
0x3e98fc 53                       push rbx
0x3e98fd 4883ec18                 sub rsp, 0x18
0x3e9901 4d89c7                   mov r15, r8
0x3e9904 4989d5                   mov r13, rdx
0x3e9907 4889fb                   mov rbx, rdi
0x3e990a 4c894dc8                 mov qword ptr [rbp - 0x38], r9
0x3e990e 488975d0                 mov qword ptr [rbp - 0x30], rsi
0x3e9912 83f901                   cmp ecx, 1
0x3e9915 756e                     jne 0x3e9985
0x3e9917 488b03                   mov rax, qword ptr [rbx]
0x3e991a 4889df                   mov rdi, rbx
0x3e991d ff9080000000             call qword ptr [rax + 0x80]
0x3e9923 84c0                     test al, al
0x3e9925 7463                     je 0x3e998a
0x3e9927 4863432c                 movsxd rax, dword ptr [rbx + 0x2c]
0x3e992b 4c637330                 movsxd r14, dword ptr [rbx + 0x30]
0x3e992f 4889df                   mov rdi, rbx
0x3e9932 4c0faff0                 imul r14, rax
0x3e9936 488b03                   mov rax, qword ptr [rbx]
0x3e9939 ff9070010000             call qword ptr [rax + 0x170]
0x3e993f 4c89f7                   mov rdi, r14
0x3e9942 89c6                     mov esi, eax
0x3e9944 e80757cdff               call 0xbf050
0x3e9949 f6433902                 test byte ptr [rbx + 0x39], 2
0x3e994d 4989c6                   mov r14, rax
0x3e9950 750f                     jne 0x3e9961
0x3e9952 488b03                   mov rax, qword ptr [rbx]
0x3e9955 4889df                   mov rdi, rbx
0x3e9958 4c89f6                   mov rsi, r14
0x3e995b ff9058010000             call qword ptr [rax + 0x158]
0x3e9961 488b03                   mov rax, qword ptr [rbx]
0x3e9964 4889df                   mov rdi, rbx
0x3e9967 ff9080000000             call qword ptr [rax + 0x80]
0x3e996d 84c0                     test al, al
0x3e996f 7425                     je 0x3e9996
0x3e9971 488b03                   mov rax, qword ptr [rbx]
0x3e9974 4889df                   mov rdi, rbx
0x3e9977 4c89ee                   mov rsi, r13
0x3e997a 4c89f2                   mov rdx, r14
0x3e997d ff9098000000             call qword ptr [rax + 0x98]
0x3e9983 eb51                     jmp 0x3e99d6
0x3e9985 4531f6                   xor r14d, r14d
0x3e9988 eb4f                     jmp 0x3e99d9
0x3e998a 48634344                 movsxd rax, dword ptr [rbx + 0x44]
0x3e998e 4531f6                   xor r14d, r14d
0x3e9991 4901c5                   add r13, rax
0x3e9994 eb43                     jmp 0x3e99d9
0x3e9996 48634344                 movsxd rax, dword ptr [rbx + 0x44]
0x3e999a 4901c5                   add r13, rax
0x3e999d 4d39ee                   cmp r14, r13
0x3e99a0 7434                     je 0x3e99d6
0x3e99a2 f6433b40                 test byte ptr [rbx + 0x3b], 0x40
0x3e99a6 7517                     jne 0x3e99bf
0x3e99a8 8b4b2c                   mov ecx, dword ptr [rbx + 0x2c]
0x3e99ab 488b03                   mov rax, qword ptr [rbx]
0x3e99ae 4889df                   mov rdi, rbx
0x3e99b1 4c89f6                   mov rsi, r14
0x3e99b4 4c89ea                   mov rdx, r13

SLOT 0x158 caller=0x3e9cf9 start=0x3e9ab0
0x3e9ab0 55                       push rbp
0x3e9ab1 4889e5                   mov rbp, rsp
0x3e9ab4 4157                     push r15
0x3e9ab6 4156                     push r14
0x3e9ab8 4155                     push r13
0x3e9aba 4154                     push r12
0x3e9abc 53                       push rbx
0x3e9abd 4883ec58                 sub rsp, 0x58
0x3e9ac1 4c8b3d4084d408           mov r15, qword ptr [rip + 0x8d48440]
0x3e9ac8 44894db4                 mov dword ptr [rbp - 0x4c], r9d
0x3e9acc 48895588                 mov qword ptr [rbp - 0x78], rdx
0x3e9ad0 448b7518                 mov r14d, dword ptr [rbp + 0x18]
0x3e9ad4 4d89c5                   mov r13, r8
0x3e9ad7 4989f4                   mov r12, rsi
0x3e9ada 4889fb                   mov rbx, rdi
0x3e9add 498b07                   mov rax, qword ptr [r15]
0x3e9ae0 488945d0                 mov qword ptr [rbp - 0x30], rax
0x3e9ae4 894db8                   mov dword ptr [rbp - 0x48], ecx
0x3e9ae7 4885f6                   test rsi, rsi
0x3e9aea 740f                     je 0x3e9afb
0x3e9aec 498b442408               mov rax, qword ptr [r12 + 8]
0x3e9af1 f6401210                 test byte ptr [rax + 0x12], 0x10
0x3e9af5 0f8598010000             jne 0x3e9c93
0x3e9afb 41f6c640                 test r14b, 0x40
0x3e9aff b001                     mov al, 1
0x3e9b01 48c7459000000000         mov qword ptr [rbp - 0x70], 0
0x3e9b09 48c745a000000000         mov qword ptr [rbp - 0x60], 0
0x3e9b11 0f94c2                   sete dl
0x3e9b14 48894598                 mov qword ptr [rbp - 0x68], rax
0x3e9b18 85c9                     test ecx, ecx
0x3e9b1a 0f8e1d040000             jle 0x3e9f3d
0x3e9b20 4489f0                   mov eax, r14d
0x3e9b23 83c801                   or eax, 1
0x3e9b26 488945a8                 mov qword ptr [rbp - 0x58], rax
0x3e9b2a 41f7c600000002           test r14d, 0x2000000
0x3e9b31 0f84d7010000             je 0x3e9d0e
0x3e9b37 488d3512822407           lea rsi, [rip + 0x7248212]
0x3e9b3e 488d7dc0                 lea rdi, [rbp - 0x40]
0x3e9b42 8855bf                   mov byte ptr [rbp - 0x41], dl
0x3e9b45 4531f6                   xor r14d, r14d
0x3e9b48 31d2                     xor edx, edx
0x3e9b4a 31c0                     xor eax, eax
0x3e9b4c e82f1fc8ff               call 0x6ba80
0x3e9b51 8b55c8                   mov edx, dword ptr [rbp - 0x38]
0x3e9b54 488b75c0                 mov rsi, qword ptr [rbp - 0x40]
0x3e9b58 4889df                   mov rdi, rbx
0x3e9b5b 83ea01                   sub edx, 1
0x3e9b5e 410f42d6                 cmovb edx, r14d
0x3e9b62 e819b0c7ff               call 0x64b80
0x3e9b67 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x3e9b6b 4885ff                   test rdi, rdi
0x3e9b6e 7405                     je 0x3e9b75
0x3e9b70 e81bdfccff               call 0xb7a90
0x3e9b75 4531f6                   xor r14d, r14d
0x3e9b78 837db400                 cmp dword ptr [rbp - 0x4c], 0
0x3e9b7c 41b800000000             mov r8d, 0
0x3e9b82 488b5588                 mov rdx, qword ptr [rbp - 0x78]
0x3e9b86 498b0424                 mov rax, qword ptr [r12]
0x3e9b8a 4c8b4d10                 mov r9, qword ptr [rbp + 0x10]
0x3e9b8e 4c89e7                   mov rdi, r12
0x3e9b91 4889de                   mov rsi, rbx
0x3e9b94 4d0f4fc5                 cmovg r8, r13
0x3e9b98 807d9800                 cmp byte ptr [rbp - 0x68], 0
0x3e9b9c 4c0f4445a0               cmove r8, qword ptr [rbp - 0x60]
0x3e9ba1 807dbf00                 cmp byte ptr [rbp - 0x41], 0
0x3e9ba5 4c0f44c2                 cmove r8, rdx
0x3e9ba9 31c9                     xor ecx, ecx
0x3e9bab ff7520                   push qword ptr [rbp + 0x20]
0x3e9bae ff75a8                   push qword ptr [rbp - 0x58]
0x3e9bb1 ff90e8000000             call qword ptr [rax + 0xe8]
0x3e9bb7 4883c410                 add rsp, 0x10
0x3e9bbb 837db801                 cmp dword ptr [rbp - 0x48], 1
0x3e9bbf 0f8478030000             je 0x3e9f3d
0x3e9bc5 41bf01000000             mov r15d, 1
0x3e9bcb eb77                     jmp 0x3e9c44
0x3e9bcd 0f1f00                   nop dword ptr [rax]
0x3e9bd0 4963442430               movsxd rax, dword ptr [r12 + 0x30]
0x3e9bd5 4d63c7                   movsxd r8, r15d
0x3e9bd8 488b5588                 mov rdx, qword ptr [rbp - 0x78]
0x3e9bdc 4c8b4d10                 mov r9, qword ptr [rbp + 0x10]
0x3e9be0 458d7701                 lea r14d, [r15 + 1]
0x3e9be4 4c89e7                   mov rdi, r12
0x3e9be7 4889de                   mov rsi, rbx
0x3e9bea 4c0fafc0                 imul r8, rax
0x3e9bee b800000000               mov eax, 0
0x3e9bf3 4c01c2                   add rdx, r8
0x3e9bf6 4d01e8                   add r8, r13
0x3e9bf9 443b7db4                 cmp r15d, dword ptr [rbp - 0x4c]
0x3e9bfd 4c0f4dc0                 cmovge r8, rax
0x3e9c01 4d85ed                   test r13, r13
0x3e9c04 498b0424                 mov rax, qword ptr [r12]
0x3e9c08 4d0f44c5                 cmove r8, r13
0x3e9c0c 807d9800                 cmp byte ptr [rbp - 0x68], 0
0x3e9c10 4c0f4445a0               cmove r8, qword ptr [rbp - 0x60]
0x3e9c15 807dbf00                 cmp byte ptr [rbp - 0x41], 0
0x3e9c19 4c0f44c2                 cmove r8, rdx
0x3e9c1d 31c9                     xor ecx, ecx
0x3e9c1f ff7520                   push qword ptr [rbp + 0x20]
0x3e9c22 ff75a8                   push qword ptr [rbp - 0x58]
0x3e9c25 ff90e8000000             call qword ptr [rax + 0xe8]
0x3e9c2b 4883c410                 add rsp, 0x10
0x3e9c2f 8b45b8                   mov eax, dword ptr [rbp - 0x48]
0x3e9c32 4589f7                   mov r15d, r14d
0x3e9c35 4439f0                   cmp eax, r14d
0x3e9c38 41be00000000             mov r14d, 0
0x3e9c3e 0f84ca020000             je 0x3e9f0e
0x3e9c44 4889df                   mov rdi, rbx
0x3e9c47 be0a000000               mov esi, 0xa
0x3e9c4c e84facc7ff               call 0x648a0
0x3e9c51 488d7dc0                 lea rdi, [rbp - 0x40]
0x3e9c55 488d35f4802407           lea rsi, [rip + 0x72480f4]
0x3e9c5c 4489fa                   mov edx, r15d
0x3e9c5f 31c0                     xor eax, eax
0x3e9c61 e81a1ec8ff               call 0x6ba80
0x3e9c66 8b55c8                   mov edx, dword ptr [rbp - 0x38]
0x3e9c69 488b75c0                 mov rsi, qword ptr [rbp - 0x40]
0x3e9c6d 4889df                   mov rdi, rbx
0x3e9c70 83ea01                   sub edx, 1
0x3e9c73 410f42d6                 cmovb edx, r14d
0x3e9c77 e804afc7ff               call 0x64b80
0x3e9c7c 488b7dc0                 mov rdi, qword ptr [rbp - 0x40]
0x3e9c80 4885ff                   test rdi, rdi
0x3e9c83 0f8447ffffff             je 0x3e9bd0
0x3e9c89 e802deccff               call 0xb7a90
0x3e9c8e e93dffffff               jmp 0x3e9bd0
0x3e9c93 b800000000               mov eax, 0
0x3e9c98 48c745a000000000         mov qword ptr [rbp - 0x60], 0
0x3e9ca0 48894598                 mov qword ptr [rbp - 0x68], rax
0x3e9ca4 41f6c640                 test r14b, 0x40
0x3e9ca8 0f94c2                   sete dl
0x3e9cab 0f85d3010000             jne 0x3e9e84
0x3e9cb1 41837c242c01             cmp dword ptr [r12 + 0x2c], 1
0x3e9cb7 4c896590                 mov qword ptr [rbp - 0x70], r12
0x3e9cbb 0f8557feffff             jne 0x3e9b18
0x3e9cc1 498b442470               mov rax, qword ptr [r12 + 0x70]
0x3e9cc6 31f6                     xor esi, esi
0x3e9cc8 8b4858                   mov ecx, dword ptr [rax + 0x58]
0x3e9ccb 0fbf405c                 movsx eax, word ptr [rax + 0x5c]
0x3e9ccf 8d4c01ff                 lea ecx, [rcx + rax - 1]
0x3e9cd3 f7d8                     neg eax
0x3e9cd5 21c8                     and eax, ecx
0x3e9cd7 4863f8                   movsxd rdi, eax
0x3e9cda e841ddccff               call 0xb7a20
0x3e9cdf 41f644243902             test byte ptr [r12 + 0x39], 2
0x3e9ce5 4889c6                   mov rsi, rax
0x3e9ce8 488945a0                 mov qword ptr [rbp - 0x60], rax
0x3e9cec 0f858a020000             jne 0x3e9f7c
0x3e9cf2 498b0424                 mov rax, qword ptr [r12]
0x3e9cf6 4c89e7                   mov rdi, r12
0x3e9cf9 ff9058010000             call qword ptr [rax + 0x158]
0x3e9cff b201                     mov dl, 1
0x3e9d01 48c7459800000000         mov qword ptr [rbp - 0x68], 0
0x3e9d09 e990020000               jmp 0x3e9f9e
0x3e9d0e 4c8b7598                 mov r14, qword ptr [rbp - 0x68]
0x3e9d12 4889df                   mov rdi, rbx
0x3e9d15 be28000000               mov esi, 0x28
0x3e9d1a 4189d7                   mov r15d, edx
0x3e9d1d 4120d6                   and r14b, dl
0x3e9d20 e87babc7ff               call 0x648a0
0x3e9d25 498b0424                 mov rax, qword ptr [r12]
0x3e9d29 488b80e8000000           mov rax, qword ptr [rax + 0xe8]
0x3e9d30 4180fe01                 cmp r14b, 1
0x3e9d34 0f85b1000000             jne 0x3e9deb
0x3e9d3a 4531c0                   xor r8d, r8d
0x3e9d3d 837db400                 cmp dword ptr [rbp - 0x4c], 0
0x3e9d41 4c8b5520                 mov r10, qword ptr [rbp + 0x20]
0x3e9d45 488b5588                 mov rdx, qword ptr [rbp - 0x78]
0x3e9d49 4c8b4d10                 mov r9, qword ptr [rbp + 0x10]
0x3e9d4d 4c89e7                   mov rdi, r12
0x3e9d50 4889de                   mov rsi, rbx
0x3e9d53 4d0f4fc5                 cmovg r8, r13
0x3e9d57 31c9                     xor ecx, ecx

SLOT 0x158 caller=0x3ea03b start=0x3e9fc0
0x3e9fc0 55                       push rbp
0x3e9fc1 4889e5                   mov rbp, rsp
0x3e9fc4 4157                     push r15
0x3e9fc6 4156                     push r14
0x3e9fc8 4155                     push r13
0x3e9fca 4154                     push r12
0x3e9fcc 53                       push rbx
0x3e9fcd 4883ec58                 sub rsp, 0x58
0x3e9fd1 488b05307fd408           mov rax, qword ptr [rip + 0x8d47f30]
0x3e9fd8 44894dac                 mov dword ptr [rbp - 0x54], r9d
0x3e9fdc 4c894590                 mov qword ptr [rbp - 0x70], r8
0x3e9fe0 48897598                 mov qword ptr [rbp - 0x68], rsi
0x3e9fe4 4989d4                   mov r12, rdx
0x3e9fe7 4889fb                   mov rbx, rdi
0x3e9fea 488b00                   mov rax, qword ptr [rax]
0x3e9fed 488945d0                 mov qword ptr [rbp - 0x30], rax
0x3e9ff1 488955a0                 mov qword ptr [rbp - 0x60], rdx
0x3e9ff5 83f901                   cmp ecx, 1
0x3e9ff8 754c                     jne 0x3ea046
0x3e9ffa 488b03                   mov rax, qword ptr [rbx]
0x3e9ffd 4889df                   mov rdi, rbx
0x3ea000 ff5078                   call qword ptr [rax + 0x78]
0x3ea003 84c0                     test al, al
0x3ea005 7447                     je 0x3ea04e
0x3ea007 4863432c                 movsxd rax, dword ptr [rbx + 0x2c]
0x3ea00b 4c637330                 movsxd r14, dword ptr [rbx + 0x30]
0x3ea00f 4889df                   mov rdi, rbx
0x3ea012 4c0faff0                 imul r14, rax
0x3ea016 488b03                   mov rax, qword ptr [rbx]
0x3ea019 ff9070010000             call qword ptr [rax + 0x170]
0x3ea01f 4c89f7                   mov rdi, r14
0x3ea022 89c6                     mov esi, eax
0x3ea024 e82750cdff               call 0xbf050
0x3ea029 f6433902                 test byte ptr [rbx + 0x39], 2
0x3ea02d 4989c7                   mov r15, rax
0x3ea030 750f                     jne 0x3ea041
0x3ea032 488b03                   mov rax, qword ptr [rbx]
0x3ea035 4889df                   mov rdi, rbx
0x3ea038 4c89fe                   mov rsi, r15
0x3ea03b ff9058010000             call qword ptr [rax + 0x158]
0x3ea041 4d89fe                   mov r14, r15
0x3ea044 eb12                     jmp 0x3ea058
0x3ea046 4531ff                   xor r15d, r15d
0x3ea049 4d89e6                   mov r14, r12
0x3ea04c eb0a                     jmp 0x3ea058
0x3ea04e 4c637344                 movsxd r14, dword ptr [rbx + 0x44]
0x3ea052 4531ff                   xor r15d, r15d
0x3ea055 4d01e6                   add r14, r12
0x3ea058 4c8b6378                 mov r12, qword ptr [rbx + 0x78]
0x3ea05c 498b0424                 mov rax, qword ptr [r12]
0x3ea060 458b6c2430               mov r13d, dword ptr [r12 + 0x30]
0x3ea065 4c89e7                   mov rdi, r12
0x3ea068 ff9070010000             call qword ptr [rax + 0x170]
0x3ea06e 0fb64b70                 movzx ecx, byte ptr [rbx + 0x70]
0x3ea072 4c8965b0                 mov qword ptr [rbp - 0x50], r12
0x3ea076 44896dc0                 mov dword ptr [rbp - 0x40], r13d
0x3ea07a 8945c4                   mov dword ptr [rbp - 0x3c], eax
0x3ea07d 488b4510                 mov rax, qword ptr [rbp + 0x10]
0x3ea081 488b7d98                 mov rdi, qword ptr [rbp - 0x68]
0x3ea085 4c8b4590                 mov r8, qword ptr [rbp - 0x70]
0x3ea089 4c8d4db0                 lea r9, [rbp - 0x50]
0x3ea08d 4c89f2                   mov rdx, r14
0x3ea090 884dc8                   mov byte ptr [rbp - 0x38], cl
0x3ea093 4c8975b8                 mov qword ptr [rbp - 0x48], r14
0x3ea097 8b4dac                   mov ecx, dword ptr [rbp - 0x54]

SLOT 0x158 caller=0x3ef012 start=0x3eeff0
0x3eeff0 55                       push rbp
0x3eeff1 4889e5                   mov rbp, rsp
0x3eeff4 4157                     push r15
0x3eeff6 4156                     push r14
0x3eeff8 53                       push rbx
0x3eeff9 50                       push rax
0x3eeffa 4989fe                   mov r14, rdi
0x3eeffd 488b3f                   mov rdi, qword ptr [rdi]
0x3ef000 4c8b3e                   mov r15, qword ptr [rsi]
0x3ef003 488b1a                   mov rbx, qword ptr [rdx]
0x3ef006 f6473902                 test byte ptr [rdi + 0x39], 2
0x3ef00a 7517                     jne 0x3ef023
0x3ef00c 488b07                   mov rax, qword ptr [rdi]
0x3ef00f 4c89fe                   mov rsi, r15
0x3ef012 ff9058010000             call qword ptr [rax + 0x158]
0x3ef018 498b5610                 mov rdx, qword ptr [r14 + 0x10]
0x3ef01c 4c39fa                   cmp rdx, r15
0x3ef01f 7521                     jne 0x3ef042
0x3ef021 eb39                     jmp 0x3ef05c
0x3ef023 4863472c                 movsxd rax, dword ptr [rdi + 0x2c]
0x3ef027 48635730                 movsxd rdx, dword ptr [rdi + 0x30]
0x3ef02b 4c89ff                   mov rdi, r15
0x3ef02e 31f6                     xor esi, esi
0x3ef030 480fafd0                 imul rdx, rax
0x3ef034 e8072e0807               call 0x7471e40
0x3ef039 498b5610                 mov rdx, qword ptr [r14 + 0x10]
0x3ef03d 4c39fa                   cmp rdx, r15
0x3ef040 741a                     je 0x3ef05c
0x3ef042 498b3e                   mov rdi, qword ptr [r14]
0x3ef045 f6473b40                 test byte ptr [rdi + 0x3b], 0x40
0x3ef049 7529                     jne 0x3ef074
0x3ef04b 488b07                   mov rax, qword ptr [rdi]
0x3ef04e 4c89fe                   mov rsi, r15
0x3ef051 b901000000               mov ecx, 1
0x3ef056 ff90f8000000             call qword ptr [rax + 0xf8]
0x3ef05c 498b7e08                 mov rdi, qword ptr [r14 + 8]
0x3ef060 f6473902                 test byte ptr [rdi + 0x39], 2
0x3ef064 752a                     jne 0x3ef090
0x3ef066 488b07                   mov rax, qword ptr [rdi]
0x3ef069 4889de                   mov rsi, rbx
0x3ef06c ff9058010000             call qword ptr [rax + 0x158]

SLOT 0x158 caller=0x3ef06c start=0x3eeff0
0x3eeff0 55                       push rbp
0x3eeff1 4889e5                   mov rbp, rsp
0x3eeff4 4157                     push r15
0x3eeff6 4156                     push r14
0x3eeff8 53                       push rbx
0x3eeff9 50                       push rax
0x3eeffa 4989fe                   mov r14, rdi
0x3eeffd 488b3f                   mov rdi, qword ptr [rdi]
0x3ef000 4c8b3e                   mov r15, qword ptr [rsi]
0x3ef003 488b1a                   mov rbx, qword ptr [rdx]
0x3ef006 f6473902                 test byte ptr [rdi + 0x39], 2
0x3ef00a 7517                     jne 0x3ef023
0x3ef00c 488b07                   mov rax, qword ptr [rdi]
0x3ef00f 4c89fe                   mov rsi, r15
0x3ef012 ff9058010000             call qword ptr [rax + 0x158]
0x3ef018 498b5610                 mov rdx, qword ptr [r14 + 0x10]
0x3ef01c 4c39fa                   cmp rdx, r15
0x3ef01f 7521                     jne 0x3ef042
0x3ef021 eb39                     jmp 0x3ef05c
0x3ef023 4863472c                 movsxd rax, dword ptr [rdi + 0x2c]
0x3ef027 48635730                 movsxd rdx, dword ptr [rdi + 0x30]
0x3ef02b 4c89ff                   mov rdi, r15
0x3ef02e 31f6                     xor esi, esi
0x3ef030 480fafd0                 imul rdx, rax
0x3ef034 e8072e0807               call 0x7471e40
0x3ef039 498b5610                 mov rdx, qword ptr [r14 + 0x10]
0x3ef03d 4c39fa                   cmp rdx, r15
0x3ef040 741a                     je 0x3ef05c
0x3ef042 498b3e                   mov rdi, qword ptr [r14]
0x3ef045 f6473b40                 test byte ptr [rdi + 0x3b], 0x40
0x3ef049 7529                     jne 0x3ef074
0x3ef04b 488b07                   mov rax, qword ptr [rdi]
0x3ef04e 4c89fe                   mov rsi, r15
0x3ef051 b901000000               mov ecx, 1
0x3ef056 ff90f8000000             call qword ptr [rax + 0xf8]
0x3ef05c 498b7e08                 mov rdi, qword ptr [r14 + 8]
0x3ef060 f6473902                 test byte ptr [rdi + 0x39], 2
0x3ef064 752a                     jne 0x3ef090
0x3ef066 488b07                   mov rax, qword ptr [rdi]
0x3ef069 4889de                   mov rsi, rbx
0x3ef06c ff9058010000             call qword ptr [rax + 0x158]
0x3ef072 eb32                     jmp 0x3ef0a6
0x3ef074 48634730                 movsxd rax, dword ptr [rdi + 0x30]
0x3ef078 4889d6                   mov rsi, rdx
0x3ef07b 4c89ff                   mov rdi, r15
0x3ef07e 4889c2                   mov rdx, rax
0x3ef081 e8ca2d0807               call 0x7471e50
0x3ef086 498b7e08                 mov rdi, qword ptr [r14 + 8]
0x3ef08a f6473902                 test byte ptr [rdi + 0x39], 2
0x3ef08e 74d6                     je 0x3ef066
0x3ef090 4863472c                 movsxd rax, dword ptr [rdi + 0x2c]
0x3ef094 48635730                 movsxd rdx, dword ptr [rdi + 0x30]
0x3ef098 4889df                   mov rdi, rbx
0x3ef09b 31f6                     xor esi, esi
0x3ef09d 480fafd0                 imul rdx, rax
0x3ef0a1 e89a2d0807               call 0x7471e40
0x3ef0a6 4883c408                 add rsp, 8
0x3ef0aa 5b                       pop rbx
0x3ef0ab 415e                     pop r14
0x3ef0ad 415f                     pop r15
0x3ef0af 5d                       pop rbp
0x3ef0b0 c3                       ret 
0x3ef0b1 cc                       int3 
0x3ef0b2 cc                       int3 
0x3ef0b3 cc                       int3 
0x3ef0b4 cc                       int3 
0x3ef0b5 cc                       int3 
0x3ef0b6 cc                       int3 
0x3ef0b7 cc                       int3 
0x3ef0b8 cc                       int3 
0x3ef0b9 cc                       int3 
0x3ef0ba cc                       int3 
0x3ef0bb cc                       int3 
0x3ef0bc cc                       int3 
0x3ef0bd cc                       int3 
0x3ef0be cc                       int3 
0x3ef0bf cc                       int3 
0x3ef0c0 55                       push rbp
0x3ef0c1 4889e5                   mov rbp, rsp
0x3ef0c4 4157                     push r15
0x3ef0c6 4156                     push r14
0x3ef0c8 4155                     push r13
0x3ef0ca 4154                     push r12
