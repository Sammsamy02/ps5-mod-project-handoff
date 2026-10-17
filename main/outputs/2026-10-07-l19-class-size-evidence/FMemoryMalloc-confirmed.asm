0x4b7a20  (f 0xb7a20)  55                     push rbp
0x4b7a21  (f 0xb7a21)  4889e5                 mov rbp, rsp
0x4b7a24  (f 0xb7a24)  4157                   push r15
0x4b7a26  (f 0xb7a26)  4156                   push r14
0x4b7a28  (f 0xb7a28)  53                     push rbx
0x4b7a29  (f 0xb7a29)  50                     push rax
0x4b7a2a  (f 0xb7a2a)  4c8d351f8f2909         lea r14, [rip + 0x9298f1f]  ; rip->0x9750950 (file 0x9350950)
0x4b7a31  (f 0xb7a31)  4889fb                 mov rbx, rdi
0x4b7a34  (f 0xb7a34)  89f2                   mov edx, esi
0x4b7a36  (f 0xb7a36)  498b3e                 mov rdi, qword ptr [r14]
0x4b7a39  (f 0xb7a39)  4885ff                 test rdi, rdi
0x4b7a3c  (f 0xb7a3c)  750e                   jne 0x4b7a4c
0x4b7a3e  (f 0xb7a3e)  0fb60503462709         movzx eax, byte ptr [rip + 0x9274603]  ; rip->0x972c048 (file 0x932c048)
0x4b7a45  (f 0xb7a45)  84c0                   test al, al
0x4b7a47  (f 0xb7a47)  7417                   je 0x4b7a60
0x4b7a49  (f 0xb7a49)  498b3e                 mov rdi, qword ptr [r14]
0x4b7a4c  (f 0xb7a4c)  488b07                 mov rax, qword ptr [rdi]
0x4b7a4f  (f 0xb7a4f)  4889de                 mov rsi, rbx
0x4b7a52  (f 0xb7a52)  ff5030                 call qword ptr [rax + 0x30]
0x4b7a55  (f 0xb7a55)  4883c408               add rsp, 8
0x4b7a59  (f 0xb7a59)  5b                     pop rbx
0x4b7a5a  (f 0xb7a5a)  415e                   pop r14
0x4b7a5c  (f 0xb7a5c)  415f                   pop r15
0x4b7a5e  (f 0xb7a5e)  5d                     pop rbp
0x4b7a5f  (f 0xb7a5f)  c3                     ret 
0x4b7a60  (f 0xb7a60)  488d3de1452709         lea rdi, [rip + 0x92745e1]  ; rip->0x972c048 (file 0x932c048)
0x4b7a67  (f 0xb7a67)  4189d7                 mov r15d, edx
0x4b7a6a  (f 0xb7a6a)  e841a43b07             call 0x7871eb0
0x4b7a6f  (f 0xb7a6f)  4489fa                 mov edx, r15d
0x4b7a72  (f 0xb7a72)  85c0                   test eax, eax
0x4b7a74  (f 0xb7a74)  74d3                   je 0x4b7a49
0x4b7a76  (f 0xb7a76)  e825710000             call 0x4beba0
0x4b7a7b  (f 0xb7a7b)  488d3dc6452709         lea rdi, [rip + 0x92745c6]  ; rip->0x972c048 (file 0x932c048)
0x4b7a82  (f 0xb7a82)  e839a43b07             call 0x7871ec0
0x4b7a87  (f 0xb7a87)  4489fa                 mov edx, r15d
0x4b7a8a  (f 0xb7a8a)  ebbd                   jmp 0x4b7a49
--- calls: [('0x8b7a6a', '0x7871eb0'), ('0x8b7a76', '0x4beba0'), ('0x8b7a82', '0x7871ec0')]
