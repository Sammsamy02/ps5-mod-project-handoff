2fa6f20 55 push rbp
2fa6f21 4889e5 mov rbp, rsp
2fa6f24 4157 push r15
2fa6f26 4156 push r14
2fa6f28 4154 push r12
2fa6f2a 53 push rbx
2fa6f2b 4883ec20 sub rsp, 0x20
2fa6f2f 4c8b3dd2af1806 mov r15, qword ptr [rip + 0x618afd2]
2fa6f36 4889fb mov rbx, rdi
2fa6f39 498b07 mov rax, qword ptr [r15]
2fa6f3c 488945d8 mov qword ptr [rbp - 0x28], rax
2fa6f40 e8dbac4efd call 0x491c20
2fa6f45 488d0564a62805 lea rax, [rip + 0x528a664]
2fa6f4c 488d7b30 lea rdi, [rbx + 0x30]
2fa6f50 baf0060000 mov edx, 0x6f0
2fa6f55 31f6 xor esi, esi
2fa6f57 4c8db358080000 lea r14, [rbx + 0x858]
2fa6f5e 488d4810 lea rcx, [rax + 0x10]
2fa6f62 4805b8050000 add rax, 0x5b8
2fa6f68 48890b mov qword ptr [rbx], rcx
2fa6f6b 48894328 mov qword ptr [rbx + 0x28], rax
2fa6f6f e8ccae4c04 call 0x7471e40
