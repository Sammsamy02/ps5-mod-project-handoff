0x3850f40: 55                       push rbp
0x3850f41: 4889e5                   mov rbp, rsp
0x3850f44: 4157                     push r15
0x3850f46: 4156                     push r14
0x3850f48: 4155                     push r13
0x3850f4a: 4154                     push r12
0x3850f4c: 53                       push rbx
0x3850f4d: 4881ece8080000           sub rsp, 0x8e8
0x3850f54: 488b05ad0f8e05           mov rax, qword ptr [rip + 0x58e0fad]
0x3850f5b: c5f8100dbdba1e04         vmovups xmm1, xmmword ptr [rip + 0x41ebabd]
0x3850f63: 4889f3                   mov rbx, rsi
0x3850f66: 4989ff                   mov r15, rdi
0x3850f69: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x3850f6d: 488b00                   mov rax, qword ptr [rax]
0x3850f70: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x3850f74: 48c745a000000000         mov qword ptr [rbp - 0x60], 0
0x3850f7c: c5f8114580               vmovups xmmword ptr [rbp - 0x80], xmm0
0x3850f81: c5f8114da8               vmovups xmmword ptr [rbp - 0x58], xmm1
0x3850f86: 48c745c000000000         mov qword ptr [rbp - 0x40], 0
0x3850f8e: c5f8118560f7ffff         vmovups xmmword ptr [rbp - 0x8a0], xmm0
0x3850f96: c5f8118550f7ffff         vmovups xmmword ptr [rbp - 0x8b0], xmm0
0x3850f9e: c745c800000000           mov dword ptr [rbp - 0x38], 0
0x3850fa5: c78570f7ffffffffffff     mov dword ptr [rbp - 0x890], 0xffffffff
0x3850faf: e82c7cf3ff               call 0x3788be0
0x3850fb4: 4989c6                   mov r14, rax
0x3850fb7: e8b459b8fc               call 0x3d6970
0x3850fbc: 0d00000690               or eax, 0x90060000
0x3850fc1: 488db560f7ffff           lea rsi, [rbp - 0x8a0]
0x3850fc8: 4c89f7                   mov rdi, r14
0x3850fcb: ba01000000               mov edx, 1
0x3850fd0: b910000000               mov ecx, 0x10
0x3850fd5: 4189c0                   mov r8d, eax
0x3850fd8: e8e3fcc5fc               call 0x4b0cc0
0x3850fdd: 4863b570f7ffff           movsxd rsi, dword ptr [rbp - 0x890]
0x3850fe4: 48638d68f7ffff           movsxd rcx, dword ptr [rbp - 0x898]
0x3850feb: 488bbd60f7ffff           mov rdi, qword ptr [rbp - 0x8a0]
0x3850ff2: 8d5601                   lea edx, [rsi + 1]
0x3850ff5: 4989f0                   mov r8, rsi
0x3850ff8: 39d1                     cmp ecx, edx
0x3850ffa: 0f4fd1                   cmovg edx, ecx
0x3850ffd: 49ffc0                   inc r8
0x3851000: 4939c8                   cmp r8, rcx
0x3851003: 0f8da1010000             jge 0x38511aa
0x3851009: ffc6                     inc esi
0x385100b: 4a833cc700               cmp qword ptr [rdi + r8*8], 0
0x3851010: 498d4001                 lea rax, [r8 + 1]
0x3851014: 4989c0                   mov r8, rax
0x3851017: 74e7                     je 0x3851000
0x3851019: 488d50fe                 lea rdx, [rax - 2]
0x385101d: 89b570f7ffff             mov dword ptr [rbp - 0x890], esi
0x3851023: 4883faff                 cmp rdx, -1
0x3851027: 0f8c83010000             jl 0x38511b0
0x385102d: 39f1                     cmp ecx, esi
0x385102f: 0f8e7b010000             jle 0x38511b0
0x3851035: 48ffc8                   dec rax
0x3851038: 4c8db550f7ffff           lea r14, [rbp - 0x8b0]
0x385103f: 4c8da518f7ffff           lea r12, [rbp - 0x8e8]
0x3851046: 89c0                     mov eax, eax
0x3851048: 4c8b2cc7                 mov r13, qword ptr [rdi + rax*8]
0x385104c: 410fb6858a010000         movzx eax, byte ptr [r13 + 0x18a]
0x3851054: 83f804                   cmp eax, 4
0x3851057: 0f84f3000000             je 0x3851150
0x385105d: 83f807                   cmp eax, 7
0x3851060: 0f84ea000000             je 0x3851150
0x3851066: 49638750110000           movsxd rax, dword ptr [r15 + 0x1150]
0x385106d: 4885c0                   test rax, rax
0x3851070: 746b                     je 0x38510dd
0x3851072: 498b8f48110000           mov rcx, qword ptr [r15 + 0x1148]
0x3851079: 48c1e003                 shl rax, 3
0x385107d: 31d2                     xor edx, edx
0x385107f: 90                       nop 
