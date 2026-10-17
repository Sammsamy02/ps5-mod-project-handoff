0x3e5930 (file 0x3e9930) 55 push rbp
0x3e5931 (file 0x3e9931) 4889e5 mov rbp, rsp
0x3e5934 (file 0x3e9934) 4157 push r15
0x3e5936 (file 0x3e9936) 4156 push r14
0x3e5938 (file 0x3e9938) 4155 push r13
0x3e593a (file 0x3e993a) 4154 push r12
0x3e593c (file 0x3e993c) 53 push rbx
0x3e593d (file 0x3e993d) 4883ec18 sub rsp, 0x18
0x3e5941 (file 0x3e9941) 488b05c0c5d408 mov rax, qword ptr [rip + 0x8d4c5c0]
0x3e5948 (file 0x3e9948) 8955c4 mov dword ptr [rbp - 0x3c], edx
0x3e594b (file 0x3e994b) 4989fc mov r12, rdi
0x3e594e (file 0x3e994e) bf80000000 mov edi, 0x80
0x3e5953 (file 0x3e9953) 4989f7 mov r15, rsi
0x3e5956 (file 0x3e9956) 31f6 xor esi, esi
0x3e5958 (file 0x3e9958) 488b00 mov rax, qword ptr [rax]
0x3e595b (file 0x3e995b) 488945d0 mov qword ptr [rbp - 0x30], rax
0x3e595f (file 0x3e995f) e8bc20cdff call 0xb7a20
0x3e5964 (file 0x3e9964) 0fb60d4d1cfc08 movzx ecx, byte ptr [rip + 0x8fc1c4d]
0x3e596b (file 0x3e996b) 4889c3 mov rbx, rax
0x3e596e (file 0x3e996e) 84c9 test cl, cl
0x3e5970 (file 0x3e9970) 0f8445010000 je 0x3e5abb
0x3e5976 (file 0x3e9976) 488d15fb1bfc08 lea rdx, [rip + 0x8fc1bfb]
0x3e597d (file 0x3e997d) 4889df mov rdi, rbx
0x3e5980 (file 0x3e9980) 31f6 xor esi, esi
0x3e5982 (file 0x3e9982) e839ecf7ff call 0x3645c0
0x3e5987 (file 0x3e9987) 4c8d2d924cb007 lea r13, [rip + 0x7b04c92]
0x3e598e (file 0x3e998e) 48c7436800000000 mov qword ptr [rbx + 0x68], 0
0x3e5996 (file 0x3e9996) 4c892b mov qword ptr [rbx], r13
0x3e5999 (file 0x3e9999) 4d8b2424 mov r12, qword ptr [r12]
0x3e599d (file 0x3e999d) 41f6c401 test r12b, 1
0x3e59a1 (file 0x3e99a1) 7440 je 0x3e59e3
0x3e59a3 (file 0x3e99a3) 4d89e6 mov r14, r12
0x3e59a6 (file 0x3e99a6) 4983e6fe and r14, 0xfffffffffffffffe
0x3e59aa (file 0x3e99aa) 7437 je 0x3e59e3
0x3e59ac (file 0x3e99ac) 488d05a994fb08 lea rax, [rip + 0x8fb94a9]
0x3e59b3 (file 0x3e99b3) 803801 cmp byte ptr [rax], 1
0x3e59b6 (file 0x3e99b6) 0f84a0000000 je 0x3e5a5c
0x3e59bc (file 0x3e99bc) 488d059994fb08 lea rax, [rip + 0x8fb9499]
0x3e59c3 (file 0x3e99c3) 0fb600 movzx eax, byte ptr [rax]
0x3e59c6 (file 0x3e99c6) 3c01 cmp al, 1
0x3e59c8 (file 0x3e99c8) 0f84a8000000 je 0x3e5a76
0x3e59ce (file 0x3e99ce) a801 test al, 1
0x3e59d0 (file 0x3e99d0) 0f85ba000000 jne 0x3e5a90
0x3e59d6 (file 0x3e99d6) 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x3e59da (file 0x3e99da) a801 test al, 1
0x3e59dc (file 0x3e99dc) 7409 je 0x3e59e7
0x3e59de (file 0x3e99de) e9cb000000 jmp 0x3e5aae
0x3e59e3 (file 0x3e99e3) 4c8965c8 mov qword ptr [rbp - 0x38], r12
0x3e59e7 (file 0x3e99e7) 8b4dc4 mov ecx, dword ptr [rbp - 0x3c]
0x3e59ea (file 0x3e99ea) 488d75c8 lea rsi, [rbp - 0x38]
0x3e59ee (file 0x3e99ee) 4889df mov rdi, rbx
0x3e59f1 (file 0x3e99f1) 4c89fa mov rdx, r15
0x3e59f4 (file 0x3e99f4) e8f7ebf7ff call 0x3645f0
0x3e59f9 (file 0x3e99f9) 488b0508c5d408 mov rax, qword ptr [rip + 0x8d4c508]
0x3e5a00 (file 0x3e9a00) c7432c01000000 mov dword ptr [rbx + 0x2c], 1
0x3e5a07 (file 0x3e9a07) 48c7433800000000 mov qword ptr [rbx + 0x38], 0
0x3e5a0f (file 0x3e9a0f) 66c743400000 mov word ptr [rbx + 0x40], 0
0x3e5a15 (file 0x3e9a15) c6434200 mov byte ptr [rbx + 0x42], 0
0x3e5a19 (file 0x3e9a19) c5f857c0 vxorps xmm0, xmm0, xmm0
0x3e5a1d (file 0x3e9a1d) c5fc114344 vmovups ymmword ptr [rbx + 0x44], ymm0
0x3e5a22 (file 0x3e9a22) c5fc114350 vmovups ymmword ptr [rbx + 0x50], ymm0
0x3e5a27 (file 0x3e9a27) 4c892b mov qword ptr [rbx], r13
0x3e5a2a (file 0x3e9a2a) 48c7437800000000 mov qword ptr [rbx + 0x78], 0
0x3e5a32 (file 0x3e9a32) c6437000 mov byte ptr [rbx + 0x70], 0
0x3e5a36 (file 0x3e9a36) c7433010000000 mov dword ptr [rbx + 0x30], 0x10
0x3e5a3d (file 0x3e9a3d) 488b00 mov rax, qword ptr [rax]
0x3e5a40 (file 0x3e9a40) 483b45d0 cmp rax, qword ptr [rbp - 0x30]
0x3e5a44 (file 0x3e9a44) 0f85df000000 jne 0x3e5b29
0x3e5a4a (file 0x3e9a4a) 4889d8 mov rax, rbx
0x3e5a4d (file 0x3e9a4d) 4883c418 add rsp, 0x18
0x3e5a51 (file 0x3e9a51) 5b pop rbx
0x3e5a52 (file 0x3e9a52) 415c pop r12
0x3e5a54 (file 0x3e9a54) 415d pop r13
0x3e5a56 (file 0x3e9a56) 415e pop r14
0x3e5a58 (file 0x3e9a58) 415f pop r15
0x3e5a5a (file 0x3e9a5a) 5d pop rbp
0x3e5a5b (file 0x3e9a5b) c3 ret 
