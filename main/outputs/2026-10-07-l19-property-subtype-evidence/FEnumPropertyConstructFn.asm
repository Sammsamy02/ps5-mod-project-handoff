0x360050 (file 0x364050) 55 push rbp
0x360051 (file 0x364051) 4889e5 mov rbp, rsp
0x360054 (file 0x364054) 4157 push r15
0x360056 (file 0x364056) 4156 push r14
0x360058 (file 0x364058) 4155 push r13
0x36005a (file 0x36405a) 4154 push r12
0x36005c (file 0x36405c) 53 push rbx
0x36005d (file 0x36405d) 4883ec18 sub rsp, 0x18
0x360061 (file 0x364061) 488b05a01edd08 mov rax, qword ptr [rip + 0x8dd1ea0]
0x360068 (file 0x364068) 8955c4 mov dword ptr [rbp - 0x3c], edx
0x36006b (file 0x36406b) 4989fc mov r12, rdi
0x36006e (file 0x36406e) bf80000000 mov edi, 0x80
0x360073 (file 0x364073) 4989f7 mov r15, rsi
0x360076 (file 0x364076) 31f6 xor esi, esi
0x360078 (file 0x364078) 488b00 mov rax, qword ptr [rax]
0x36007b (file 0x36407b) 488945d0 mov qword ptr [rbp - 0x30], rax
0x36007f (file 0x36407f) e89c79d5ff call 0xb7a20
0x360084 (file 0x364084) 0fb60d95eb0309 movzx ecx, byte ptr [rip + 0x903eb95]
0x36008b (file 0x36408b) 4889c3 mov rbx, rax
0x36008e (file 0x36408e) 84c9 test cl, cl
0x360090 (file 0x364090) 0f84b6000000 je 0x36014c
0x360096 (file 0x364096) 488d0543eb0309 lea rax, [rip + 0x903eb43]
0x36009d (file 0x36409d) 4c8d2dc4c3b707 lea r13, [rip + 0x7b7c3c4]
0x3600a4 (file 0x3640a4) c5f857c0 vxorps xmm0, xmm0, xmm0
0x3600a8 (file 0x3640a8) 48894308 mov qword ptr [rbx + 8], rax
0x3600ac (file 0x3640ac) c5f8114310 vmovups xmmword ptr [rbx + 0x10], xmm0
0x3600b1 (file 0x3640b1) c5f811431c vmovups xmmword ptr [rbx + 0x1c], xmm0
0x3600b6 (file 0x3640b6) 48c7436800000000 mov qword ptr [rbx + 0x68], 0
0x3600be (file 0x3640be) 4c892b mov qword ptr [rbx], r13
0x3600c1 (file 0x3640c1) 48c7437800000000 mov qword ptr [rbx + 0x78], 0
0x3600c9 (file 0x3640c9) 4d8b2424 mov r12, qword ptr [r12]
0x3600cd (file 0x3640cd) 41f6c401 test r12b, 1
0x3600d1 (file 0x3640d1) 7431 je 0x360104
0x3600d3 (file 0x3640d3) 4d89e6 mov r14, r12
0x3600d6 (file 0x3640d6) 4983e6fe and r14, 0xfffffffffffffffe
0x3600da (file 0x3640da) 7428 je 0x360104
0x3600dc (file 0x3640dc) 803d79ed030901 cmp byte ptr [rip + 0x903ed79], 1
0x3600e3 (file 0x3640e3) 7508 jne 0x3600ed
0x3600e5 (file 0x3640e5) 4c89f7 mov rdi, r14
0x3600e8 (file 0x3640e8) e8e3810000 call 0x3682d0
0x3600ed (file 0x3640ed) 803d68ed030901 cmp byte ptr [rip + 0x903ed68], 1
0x3600f4 (file 0x3640f4) 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x3600f8 (file 0x3640f8) 750e jne 0x360108
0x3600fa (file 0x3640fa) 4c89f7 mov rdi, r14
0x3600fd (file 0x3640fd) e8ce810000 call 0x3682d0
0x360102 (file 0x364102) eb04 jmp 0x360108
0x360104 (file 0x364104) 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x360108 (file 0x364108) 8b4dc4 mov ecx, dword ptr [rbp - 0x3c]
0x36010b (file 0x36410b) 488d75c8 lea rsi, [rbp - 0x38]
0x36010f (file 0x36410f) 4889df mov rdi, rbx
0x360112 (file 0x364112) 4c89fa mov rdx, r15
0x360115 (file 0x364115) e8561c0800 call 0x3e1d70
0x36011a (file 0x36411a) 488b05e71ddd08 mov rax, qword ptr [rip + 0x8dd1de7]
0x360121 (file 0x364121) 4c892b mov qword ptr [rbx], r13
0x360124 (file 0x364124) c5f857c0 vxorps xmm0, xmm0, xmm0
0x360128 (file 0x364128) c5f8114370 vmovups xmmword ptr [rbx + 0x70], xmm0
0x36012d (file 0x36412d) 488b00 mov rax, qword ptr [rax]
0x360130 (file 0x364130) 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x360134 (file 0x364134) 0f8583000000 jne 0x3601bd
0x36013a (file 0x36413a) 4889d8 mov rax, rbx
0x36013d (file 0x36413d) 4883c418 add rsp, 0x18
0x360141 (file 0x364141) 5b pop rbx
0x360142 (file 0x364142) 415c pop r12
0x360144 (file 0x364144) 415d pop r13
0x360146 (file 0x364146) 415e pop r14
0x360148 (file 0x364148) 415f pop r15
0x36014a (file 0x36414a) 5d pop rbp
0x36014b (file 0x36414b) c3 ret 
