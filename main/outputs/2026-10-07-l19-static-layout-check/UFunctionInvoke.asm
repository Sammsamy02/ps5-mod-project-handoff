0x726ac0  (f 0x326ac0)  55                     push rbp
0x726ac1  (f 0x326ac1)  4889e5                 mov rbp, rsp
0x726ac4  (f 0x326ac4)  4157                   push r15
0x726ac6  (f 0x326ac6)  4156                   push r14
0x726ac8  (f 0x326ac8)  4155                   push r13
0x726aca  (f 0x326aca)  4154                   push r12
0x726acc  (f 0x326acc)  53                     push rbx
0x726acd  (f 0x326acd)  50                     push rax
0x726ace  (f 0x326ace)  4c8b6f20               mov r13, qword ptr [rdi + 0x20]
0x726ad2  (f 0x326ad2)  4989ce                 mov r14, rcx
0x726ad5  (f 0x326ad5)  4889d3                 mov rbx, rdx
0x726ad8  (f 0x326ad8)  4989f4                 mov r12, rsi
0x726adb  (f 0x326adb)  4989ff                 mov r15, rdi
0x726ade  (f 0x326ade)  e83d8f0700             call 0x79fa20
0x726ae3  (f 0x326ae3)  4885c0                 test rax, rax
0x726ae6  (f 0x326ae6)  7426                   je 0x726b0e
0x726ae8  (f 0x326ae8)  48634838               movsxd rcx, dword ptr [rax + 0x38]
0x726aec  (f 0x326aec)  413b4d38               cmp ecx, dword ptr [r13 + 0x38]
0x726af0  (f 0x326af0)  7f1c                   jg 0x726b0e
0x726af2  (f 0x326af2)  498b5530               mov rdx, qword ptr [r13 + 0x30]
0x726af6  (f 0x326af6)  4883c030               add rax, 0x30
0x726afa  (f 0x326afa)  483904ca               cmp qword ptr [rdx + rcx*8], rax
0x726afe  (f 0x326afe)  750e                   jne 0x726b0e
0x726b00  (f 0x326b00)  4c89e7                 mov rdi, r12
0x726b03  (f 0x326b03)  4c89ee                 mov rsi, r13
0x726b06  (f 0x326b06)  e8250c1600             call 0x887730
0x726b0b  (f 0x326b0b)  4989c4                 mov r12, rax
0x726b0e  (f 0x326b0e)  4c8bab90000000         mov r13, qword ptr [rbx + 0x90]
0x726b15  (f 0x326b15)  4c89e7                 mov rdi, r12
0x726b18  (f 0x326b18)  4889de                 mov rsi, rbx
0x726b1b  (f 0x326b1b)  4c89f2                 mov rdx, r14
0x726b1e  (f 0x326b1e)  4c89bb90000000         mov qword ptr [rbx + 0x90], r15
0x726b25  (f 0x326b25)  41ff97d8000000         call qword ptr [r15 + 0xd8]
0x726b2c  (f 0x326b2c)  4c89ab90000000         mov qword ptr [rbx + 0x90], r13
0x726b33  (f 0x326b33)  4883c408               add rsp, 8
0x726b37  (f 0x326b37)  5b                     pop rbx
0x726b38  (f 0x326b38)  415c                   pop r12
0x726b3a  (f 0x326b3a)  415d                   pop r13
0x726b3c  (f 0x326b3c)  415e                   pop r14
0x726b3e  (f 0x326b3e)  415f                   pop r15
0x726b40  (f 0x326b40)  5d                     pop rbp
0x726b41  (f 0x326b41)  c3                     ret 
--- calls: [('0xb26ade', '0x79fa20'), ('0xb26b06', '0x887730')]
