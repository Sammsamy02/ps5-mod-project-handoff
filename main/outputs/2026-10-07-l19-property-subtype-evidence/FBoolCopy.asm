0x3ffe80 (file 0x403e80) 85c9 test ecx, ecx
0x3ffe82 (file 0x403e82) 7e43 jle 0x3ffec7
0x3ffe84 (file 0x403e84) 55 push rbp
0x3ffe85 (file 0x403e85) 4889e5 mov rbp, rsp
0x3ffe88 (file 0x403e88) 53 push rbx
0x3ffe89 (file 0x403e89) 31c0 xor eax, eax
0x3ffe8b (file 0x403e8b) 0f1f440000 nop dword ptr [rax + rax]
0x3ffe90 (file 0x403e90) 4c634730 movsxd r8, dword ptr [rdi + 0x30]
0x3ffe94 (file 0x403e94) 4898 cdqe 
0x3ffe96 (file 0x403e96) 440fb65f73 movzx r11d, byte ptr [rdi + 0x73]
0x3ffe9b (file 0x403e9b) 440fb65771 movzx r10d, byte ptr [rdi + 0x71]
0x3ffea0 (file 0x403ea0) 4c0fafc0 imul r8, rax
0x3ffea4 (file 0x403ea4) 4489db mov ebx, r11d
0x3ffea7 (file 0x403ea7) ffc0 inc eax
0x3ffea9 (file 0x403ea9) f6d3 not bl
0x3ffeab (file 0x403eab) 4e8d0c06 lea r9, [rsi + r8]
0x3ffeaf (file 0x403eaf) 4901d0 add r8, rdx
0x3ffeb2 (file 0x403eb2) 43221c0a and bl, byte ptr [r10 + r9]
0x3ffeb6 (file 0x403eb6) 47221c02 and r11b, byte ptr [r10 + r8]
0x3ffeba (file 0x403eba) 4108db or r11b, bl
0x3ffebd (file 0x403ebd) 47881c0a mov byte ptr [r10 + r9], r11b
0x3ffec1 (file 0x403ec1) 39c1 cmp ecx, eax
0x3ffec3 (file 0x403ec3) 75cb jne 0x3ffe90
0x3ffec5 (file 0x403ec5) 5b pop rbx
0x3ffec6 (file 0x403ec6) 5d pop rbp
0x3ffec7 (file 0x403ec7) c3 ret 
0x3ffec8 (file 0x403ec8) cc int3 
0x3ffec9 (file 0x403ec9) cc int3 
0x3ffeca (file 0x403eca) cc int3 
0x3ffecb (file 0x403ecb) cc int3 
0x3ffecc (file 0x403ecc) cc int3 
0x3ffecd (file 0x403ecd) cc int3 
0x3ffece (file 0x403ece) cc int3 
0x3ffecf (file 0x403ecf) cc int3 
0x3ffed0 (file 0x403ed0) 55 push rbp
0x3ffed1 (file 0x403ed1) 4889e5 mov rbp, rsp
