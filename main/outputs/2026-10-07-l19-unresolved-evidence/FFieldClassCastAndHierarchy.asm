0x763e70  (f 0x363e70)  488b07                 mov rax, qword ptr [rdi]
0x763e73  (f 0x363e73)  4885c0                 test rax, rax
0x763e76  (f 0x363e76)  0f94c1                 sete cl
0x763e79  (f 0x363e79)  08c1                   or cl, al
0x763e7b  (f 0x363e7b)  f6c101                 test cl, 1
0x763e7e  (f 0x363e7e)  7403                   je 0x763e83
0x763e80  (f 0x363e80)  31c0                   xor eax, eax
0x763e82  (f 0x363e82)  c3                     ret 
0x763e83  (f 0x363e83)  55                     push rbp
0x763e84  (f 0x363e84)  4889e5                 mov rbp, rsp
0x763e87  (f 0x363e87)  488b4808               mov rcx, qword ptr [rax + 8]
0x763e8b  (f 0x363e8b)  488b4608               mov rax, qword ptr [rsi + 8]
0x763e8f  (f 0x363e8f)  4885c0                 test rax, rax
0x763e92  (f 0x363e92)  7409                   je 0x763e9d
0x763e94  (f 0x363e94)  48854110               test qword ptr [rcx + 0x10], rax
0x763e98  (f 0x363e98)  0f95c0                 setne al
0x763e9b  (f 0x363e9b)  5d                     pop rbp
0x763e9c  (f 0x363e9c)  c3                     ret 
0x763e9d  (f 0x363e9d)  4885c9                 test rcx, rcx
0x763ea0  (f 0x363ea0)  0f95c0                 setne al
0x763ea3  (f 0x363ea3)  0f94c2                 sete dl
0x763ea6  (f 0x363ea6)  4839f1                 cmp rcx, rsi
0x763ea9  (f 0x363ea9)  400f94c7               sete dil
0x763ead  (f 0x363ead)  4008d7                 or dil, dl
0x763eb0  (f 0x363eb0)  7522                   jne 0x763ed4
0x763eb2  (f 0x363eb2)  66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0x763ec0  (f 0x363ec0)  488b4920               mov rcx, qword ptr [rcx + 0x20]
0x763ec4  (f 0x363ec4)  4885c9                 test rcx, rcx
0x763ec7  (f 0x363ec7)  0f95c0                 setne al
0x763eca  (f 0x363eca)  4839f1                 cmp rcx, rsi
0x763ecd  (f 0x363ecd)  7405                   je 0x763ed4
0x763ecf  (f 0x363ecf)  4885c9                 test rcx, rcx
0x763ed2  (f 0x363ed2)  75ec                   jne 0x763ec0
0x763ed4  (f 0x363ed4)  5d                     pop rbp
0x763ed5  (f 0x363ed5)  c3                     ret 
--- calls: []
