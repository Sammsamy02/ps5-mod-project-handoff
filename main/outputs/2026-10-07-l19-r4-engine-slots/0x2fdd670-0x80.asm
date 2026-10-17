0x2fdd670: 55                       push rbp
0x2fdd671: 4889e5                   mov rbp, rsp
0x2fdd674: 4156                     push r14
0x2fdd676: 53                       push rbx
0x2fdd677: 4883ec20                 sub rsp, 0x20
0x2fdd67b: 4c8b3586481506           mov r14, qword ptr [rip + 0x6154886]
0x2fdd682: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fdd686: 4c89c3                   mov rbx, r8
0x2fdd689: 4c8d45d0                 lea r8, [rbp - 0x30]
0x2fdd68d: 498b06                   mov rax, qword ptr [r14]
0x2fdd690: 488945e8                 mov qword ptr [rbp - 0x18], rax
0x2fdd694: c5f81145d0               vmovups xmmword ptr [rbp - 0x30], xmm0
0x2fdd699: 488b07                   mov rax, qword ptr [rdi]
0x2fdd69c: ff9068080000             call qword ptr [rax + 0x868]
0x2fdd6a2: 837b0800                 cmp dword ptr [rbx + 8], 0
0x2fdd6a6: 7412                     je 0x2fdd6ba
0x2fdd6a8: 488b3b                   mov rdi, qword ptr [rbx]
0x2fdd6ab: 4885ff                   test rdi, rdi
0x2fdd6ae: 740a                     je 0x2fdd6ba
0x2fdd6b0: 488b07                   mov rax, qword ptr [rdi]
0x2fdd6b3: 488d75d0                 lea rsi, [rbp - 0x30]
0x2fdd6b7: ff5068                   call qword ptr [rax + 0x68]
0x2fdd6ba: 488b7dd0                 mov rdi, qword ptr [rbp - 0x30]
0x2fdd6be: 4885ff                   test rdi, rdi
0x2fdd6c1: 7405                     je 0x2fdd6c8
0x2fdd6c3: e8c8a30dfd               call 0xb7a90
0x2fdd6c8: 498b06                   mov rax, qword ptr [r14]
0x2fdd6cb: 483b45e8                 cmp rax, qword ptr [rbp - 0x18]
0x2fdd6cf: 7509                     jne 0x2fdd6da
0x2fdd6d1: 4883c420                 add rsp, 0x20
0x2fdd6d5: 5b                       pop rbx
0x2fdd6d6: 415e                     pop r14
0x2fdd6d8: 5d                       pop rbp
0x2fdd6d9: c3                       ret 
0x2fdd6da: e881474904               call 0x7471e60
0x2fdd6df: 0f0b                     ud2 
0x2fdd6e1: cc                       int3 
0x2fdd6e2: cc                       int3 
0x2fdd6e3: cc                       int3 
0x2fdd6e4: cc                       int3 
0x2fdd6e5: cc                       int3 
0x2fdd6e6: cc                       int3 
0x2fdd6e7: cc                       int3 
0x2fdd6e8: cc                       int3 
0x2fdd6e9: cc                       int3 
0x2fdd6ea: cc                       int3 
0x2fdd6eb: cc                       int3 
0x2fdd6ec: cc                       int3 
0x2fdd6ed: cc                       int3 
0x2fdd6ee: cc                       int3 
0x2fdd6ef: cc                       int3 
