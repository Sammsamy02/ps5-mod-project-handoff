0x764710  (f 0x364710)  55                     push rbp
0x764711  (f 0x364711)  4889e5                 mov rbp, rsp
0x764714  (f 0x364714)  4156                   push r14
0x764716  (f 0x364716)  53                     push rbx
0x764717  (f 0x364717)  4989f6                 mov r14, rsi
0x76471a  (f 0x36471a)  4889fb                 mov rbx, rdi
0x76471d  (f 0x36471d)  488d7720               lea rsi, [rdi + 0x20]
0x764721  (f 0x364721)  498b06                 mov rax, qword ptr [r14]
0x764724  (f 0x364724)  4c89f7                 mov rdi, r14
0x764727  (f 0x364727)  ff9038010000           call qword ptr [rax + 0x138]
0x76472d  (f 0x36472d)  498b4608               mov rax, qword ptr [r14 + 8]
0x764731  (f 0x364731)  4883c328               add rbx, 0x28
0x764735  (f 0x364735)  488b10                 mov rdx, qword ptr [rax]
0x764738  (f 0x364738)  488d4a04               lea rcx, [rdx + 4]
0x76473c  (f 0x36473c)  483b4808               cmp rcx, qword ptr [rax + 8]
0x764740  (f 0x364740)  7620                   jbe 0x764762
0x764742  (f 0x364742)  41f6462a01             test byte ptr [r14 + 0x2a], 1
0x764747  (f 0x364747)  7525                   jne 0x76476e
0x764749  (f 0x364749)  498b06                 mov rax, qword ptr [r14]
0x76474c  (f 0x36474c)  ba04000000             mov edx, 4
0x764751  (f 0x364751)  4c89f7                 mov rdi, r14
0x764754  (f 0x364754)  4889de                 mov rsi, rbx
0x764757  (f 0x364757)  ff9088010000           call qword ptr [rax + 0x188]
0x76475d  (f 0x36475d)  5b                     pop rbx
0x76475e  (f 0x36475e)  415e                   pop r14
0x764760  (f 0x364760)  5d                     pop rbp
0x764761  (f 0x364761)  c3                     ret 
0x764762  (f 0x364762)  8b12                   mov edx, dword ptr [rdx]
0x764764  (f 0x364764)  8913                   mov dword ptr [rbx], edx
0x764766  (f 0x364766)  488908                 mov qword ptr [rax], rcx
0x764769  (f 0x364769)  5b                     pop rbx
0x76476a  (f 0x36476a)  415e                   pop r14
0x76476c  (f 0x36476c)  5d                     pop rbp
0x76476d  (f 0x36476d)  c3                     ret 
0x76476e  (f 0x36476e)  4c89f7                 mov rdi, r14
0x764771  (f 0x364771)  4889de                 mov rsi, rbx
0x764774  (f 0x364774)  e83783ebff             call 0x61cab0
0x764779  (f 0x364779)  5b                     pop rbx
0x76477a  (f 0x36477a)  415e                   pop r14
0x76477c  (f 0x36477c)  5d                     pop rbp
0x76477d  (f 0x36477d)  c3                     ret 
--- calls: [('0xb64774', '0x61cab0')]
