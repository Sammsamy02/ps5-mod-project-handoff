0x764220  (f 0x364220)  55                     push rbp
0x764221  (f 0x364221)  4889e5                 mov rbp, rsp
0x764224  (f 0x364224)  4157                   push r15
0x764226  (f 0x364226)  4156                   push r14
0x764228  (f 0x364228)  53                     push rbx
0x764229  (f 0x364229)  4881ec38020000         sub rsp, 0x238
0x764230  (f 0x364230)  4c8b3dd1dcdc08         mov r15, qword ptr [rip + 0x8dcdcd1]  ; rip->0x9531f08 (file 0x9131f08)
0x764237  (f 0x364237)  488d8ddafdffff         lea rcx, [rbp - 0x226]
0x76423e  (f 0x36423e)  4889fb                 mov rbx, rdi
0x764241  (f 0x364241)  488d7dda               lea rdi, [rbp - 0x26]
0x764245  (f 0x364245)  498b07                 mov rax, qword ptr [r15]
0x764248  (f 0x364248)  488945e0               mov qword ptr [rbp - 0x20], rax
0x76424c  (f 0x36424c)  c685d8fdffff00         mov byte ptr [rbp - 0x228], 0
0x764253  (f 0x364253)  48898dc0fdffff         mov qword ptr [rbp - 0x240], rcx
0x76425a  (f 0x36425a)  48898dc8fdffff         mov qword ptr [rbp - 0x238], rcx
0x764261  (f 0x364261)  488d8dc0fdffff         lea rcx, [rbp - 0x240]
0x764268  (f 0x364268)  4889bdd0fdffff         mov qword ptr [rbp - 0x230], rdi
0x76426f  (f 0x36426f)  4889f7                 mov rdi, rsi
0x764272  (f 0x364272)  4889d6                 mov rsi, rdx
0x764275  (f 0x364275)  4889ca                 mov rdx, rcx
0x764278  (f 0x364278)  e873060000             call 0x7648f0
0x76427d  (f 0x36427d)  488bb5c0fdffff         mov rsi, qword ptr [rbp - 0x240]
0x764284  (f 0x364284)  488b95c8fdffff         mov rdx, qword ptr [rbp - 0x238]
0x76428b  (f 0x36428b)  4c8db5b0fdffff         lea r14, [rbp - 0x250]
0x764292  (f 0x364292)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x764296  (f 0x364296)  c5f81103               vmovups xmmword ptr [rbx], xmm0
0x76429a  (f 0x36429a)  4c89f7                 mov rdi, r14
0x76429d  (f 0x36429d)  4829f2                 sub rdx, rsi
0x7642a0  (f 0x3642a0)  48d1ea                 shr rdx, 1
0x7642a3  (f 0x3642a3)  e86801d0ff             call 0x464410
0x7642a8  (f 0x3642a8)  4939de                 cmp r14, rbx
0x7642ab  (f 0x3642ab)  7417                   je 0x7642c4
0x7642ad  (f 0x3642ad)  488b85b0fdffff         mov rax, qword ptr [rbp - 0x250]
0x7642b4  (f 0x3642b4)  488b8db8fdffff         mov rcx, qword ptr [rbp - 0x248]
0x7642bb  (f 0x3642bb)  488903                 mov qword ptr [rbx], rax
0x7642be  (f 0x3642be)  48894b08               mov qword ptr [rbx + 8], rcx
0x7642c2  (f 0x3642c2)  eb11                   jmp 0x7642d5
0x7642c4  (f 0x3642c4)  488bbdb0fdffff         mov rdi, qword ptr [rbp - 0x250]
0x7642cb  (f 0x3642cb)  4885ff                 test rdi, rdi
0x7642ce  (f 0x3642ce)  7405                   je 0x7642d5
0x7642d0  (f 0x3642d0)  e8bb37d5ff             call 0x4b7a90
0x7642d5  (f 0x3642d5)  488dbdc0fdffff         lea rdi, [rbp - 0x240]
0x7642dc  (f 0x3642dc)  e8fffde9ff             call 0x6040e0
0x7642e1  (f 0x3642e1)  498b07                 mov rax, qword ptr [r15]
0x7642e4  (f 0x3642e4)  483b45e0               cmp rax, qword ptr [rbp - 0x20]
0x7642e8  (f 0x3642e8)  7511                   jne 0x7642fb
0x7642ea  (f 0x3642ea)  4889d8                 mov rax, rbx
0x7642ed  (f 0x3642ed)  4881c438020000         add rsp, 0x238
0x7642f4  (f 0x3642f4)  5b                     pop rbx
0x7642f5  (f 0x3642f5)  415e                   pop r14
0x7642f7  (f 0x3642f7)  415f                   pop r15
0x7642f9  (f 0x3642f9)  5d                     pop rbp
0x7642fa  (f 0x3642fa)  c3                     ret 
0x7642fb  (f 0x3642fb)  e860db1007             call 0x7871e60
0x764300  (f 0x364300)  0f0b                   ud2 
--- calls: [('0xb64278', '0x7648f0'), ('0xb642a3', '0x464410'), ('0xb642dc', '0x6040e0'), ('0xb642fb', '0x7871e60'), ('0xb642d0', '0x4b7a90')]
