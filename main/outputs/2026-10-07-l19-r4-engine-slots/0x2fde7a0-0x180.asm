0x2fde7a0: 55                       push rbp
0x2fde7a1: 4889e5                   mov rbp, rsp
0x2fde7a4: 4157                     push r15
0x2fde7a6: 4156                     push r14
0x2fde7a8: 4155                     push r13
0x2fde7aa: 4154                     push r12
0x2fde7ac: 53                       push rbx
0x2fde7ad: 4883ec48                 sub rsp, 0x48
0x2fde7b1: 4c8b2550371506           mov r12, qword ptr [rip + 0x6153750]
0x2fde7b8: 4989f6                   mov r14, rsi
0x2fde7bb: 498b0424                 mov rax, qword ptr [r12]
0x2fde7bf: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fde7c3: e8d8bab3ff               call 0x2b1a2a0
0x2fde7c8: 4889c7                   mov rdi, rax
0x2fde7cb: ba01000000               mov edx, 1
0x2fde7d0: 31f6                     xor esi, esi
0x2fde7d2: 4989c5                   mov r13, rax
0x2fde7d5: e876eb7a00               call 0x378d350
0x2fde7da: f680b502000040           test byte ptr [rax + 0x2b5], 0x40
0x2fde7e1: 0f851d020000             jne 0x2fdea04
0x2fde7e7: 4983be4803000000         cmp qword ptr [r14 + 0x348], 0
0x2fde7ef: 744c                     je 0x2fde83d
0x2fde7f1: 4d8dbe48030000           lea r15, [r14 + 0x348]
0x2fde7f8: e8a3ef1c00               call 0x31ad7a0
0x2fde7fd: 4c89ff                   mov rdi, r15
0x2fde800: 4889c6                   mov rsi, rax
0x2fde803: e898103ffd               call 0x3cf8a0
0x2fde808: 498b0f                   mov rcx, qword ptr [r15]
0x2fde80b: 84c0                     test al, al
0x2fde80d: 740b                     je 0x2fde81a
0x2fde80f: 4885c9                   test rcx, rcx
0x2fde812: 0f85ec010000             jne 0x2fdea04
0x2fde818: eb23                     jmp 0x2fde83d
0x2fde81a: 4885c9                   test rcx, rcx
0x2fde81d: 741e                     je 0x2fde83d
0x2fde81f: e83c70a8ff               call 0x2a65860
0x2fde824: 4c89ff                   mov rdi, r15
0x2fde827: 4889c6                   mov rsi, rax
0x2fde82a: e871103ffd               call 0x3cf8a0
0x2fde82f: 84c0                     test al, al
0x2fde831: 740a                     je 0x2fde83d
0x2fde833: 49833f00                 cmp qword ptr [r15], 0
0x2fde837: 0f85c7010000             jne 0x2fdea04
0x2fde83d: 498bb538080000           mov rsi, qword ptr [r13 + 0x838]
0x2fde844: 4885f6                   test rsi, rsi
0x2fde847: 741a                     je 0x2fde863
0x2fde849: 4c89f7                   mov rdi, r14
0x2fde84c: ba01000000               mov edx, 1
0x2fde851: b901000000               mov ecx, 1
0x2fde856: e8b5264500               call 0x3430f10
0x2fde85b: 4c89f7                   mov rdi, r14
0x2fde85e: e8dd144500               call 0x342fd40
0x2fde863: 49639d98000000           movsxd rbx, dword ptr [r13 + 0x98]
0x2fde86a: 4885db                   test rbx, rbx
0x2fde86d: 0f8e44010000             jle 0x2fde9b7
0x2fde873: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fde877: c5f81145c0               vmovups xmmword ptr [rbp - 0x40], xmm0
0x2fde87c: 4c8975b0                 mov qword ptr [rbp - 0x50], r14
0x2fde880: 48c1e303                 shl rbx, 3
0x2fde884: 4c896da8                 mov qword ptr [rbp - 0x58], r13
0x2fde888: 4531ff                   xor r15d, r15d
0x2fde88b: 4d8bb590000000           mov r14, qword ptr [r13 + 0x90]
0x2fde892: 48895d98                 mov qword ptr [rbp - 0x68], rbx
0x2fde896: 4c8975a0                 mov qword ptr [rbp - 0x60], r14
0x2fde89a: e98a000000               jmp 0x2fde929
0x2fde89f: 90                       nop 
0x2fde8a0: 8d4801                   lea ecx, [rax + 1]
0x2fde8a3: 4c63f0                   movsxd r14, eax
0x2fde8a6: c5f857c0                 vxorps xmm0, xmm0, xmm0
0x2fde8aa: 4c89ef                   mov rdi, r13
0x2fde8ad: 894dc8                   mov dword ptr [rbp - 0x38], ecx
0x2fde8b0: 49c1e604                 shl r14, 4
0x2fde8b4: 488b5dc0                 mov rbx, qword ptr [rbp - 0x40]
0x2fde8b8: c4a178110433             vmovups xmmword ptr [rbx + r14], xmm0
0x2fde8be: 498b4500                 mov rax, qword ptr [r13]
0x2fde8c2: ff90e8020000             call qword ptr [rax + 0x2e8]
0x2fde8c8: 488b7db0                 mov rdi, qword ptr [rbp - 0x50]
0x2fde8cc: 4889c6                   mov rsi, rax
0x2fde8cf: 31d2                     xor edx, edx
0x2fde8d1: e85a0a4600               call 0x343f330
0x2fde8d6: 4a890433                 mov qword ptr [rbx + r14], rax
0x2fde8da: 0fb645bf                 movzx eax, byte ptr [rbp - 0x41]
0x2fde8de: 468864330c               mov byte ptr [rbx + r14 + 0xc], r12b
0x2fde8e3: 428844330d               mov byte ptr [rbx + r14 + 0xd], al
0x2fde8e8: 410fb6851a010000         movzx eax, byte ptr [r13 + 0x11a]
0x2fde8f0: c0e802                   shr al, 2
0x2fde8f3: 2401                     and al, 1
0x2fde8f5: 428844330e               mov byte ptr [rbx + r14 + 0xe], al
0x2fde8fa: 410fb6851a010000         movzx eax, byte ptr [r13 + 0x11a]
0x2fde902: c0e803                   shr al, 3
0x2fde905: 2401                     and al, 1
0x2fde907: 428844330f               mov byte ptr [rbx + r14 + 0xf], al
0x2fde90c: 418b85f4000000           mov eax, dword ptr [r13 + 0xf4]
0x2fde913: 4289443308               mov dword ptr [rbx + r14 + 8], eax
0x2fde918: 488b5d98                 mov rbx, qword ptr [rbp - 0x68]
0x2fde91c: 4c8b75a0                 mov r14, qword ptr [rbp - 0x60]
