0xb7a20 (file 0xbba20) 55 push rbp
0xb7a21 (file 0xbba21) 4889e5 mov rbp, rsp
0xb7a24 (file 0xbba24) 4157 push r15
0xb7a26 (file 0xbba26) 4156 push r14
0xb7a28 (file 0xbba28) 53 push rbx
0xb7a29 (file 0xbba29) 50 push rax
0xb7a2a (file 0xbba2a) 4c8d351f8f2909 lea r14, [rip + 0x9298f1f]
0xb7a31 (file 0xbba31) 4889fb mov rbx, rdi
0xb7a34 (file 0xbba34) 89f2 mov edx, esi
0xb7a36 (file 0xbba36) 498b3e mov rdi, qword ptr [r14]
0xb7a39 (file 0xbba39) 4885ff test rdi, rdi
0xb7a3c (file 0xbba3c) 750e jne 0xb7a4c
0xb7a3e (file 0xbba3e) 0fb60503462709 movzx eax, byte ptr [rip + 0x9274603]
0xb7a45 (file 0xbba45) 84c0 test al, al
0xb7a47 (file 0xbba47) 7417 je 0xb7a60
0xb7a49 (file 0xbba49) 498b3e mov rdi, qword ptr [r14]
0xb7a4c (file 0xbba4c) 488b07 mov rax, qword ptr [rdi]
0xb7a4f (file 0xbba4f) 4889de mov rsi, rbx
0xb7a52 (file 0xbba52) ff5030 call qword ptr [rax + 0x30]
0xb7a55 (file 0xbba55) 4883c408 add rsp, 8
