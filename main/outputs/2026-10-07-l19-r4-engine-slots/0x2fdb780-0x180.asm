0x2fdb780: 55                       push rbp
0x2fdb781: 4889e5                   mov rbp, rsp
0x2fdb784: 4157                     push r15
0x2fdb786: 4156                     push r14
0x2fdb788: 4155                     push r13
0x2fdb78a: 4154                     push r12
0x2fdb78c: 53                       push rbx
0x2fdb78d: 4883ec18                 sub rsp, 0x18
0x2fdb791: 4c8b3570671506           mov r14, qword ptr [rip + 0x6156770]
0x2fdb798: 4889fb                   mov rbx, rdi
0x2fdb79b: 498b06                   mov rax, qword ptr [r14]
0x2fdb79e: 488945d0                 mov qword ptr [rbp - 0x30], rax
0x2fdb7a2: 4c8bbeb8020000           mov r15, qword ptr [rsi + 0x2b8]
0x2fdb7a9: 488b05b86f5406           mov rax, qword ptr [rip + 0x6546fb8]
0x2fdb7b0: 4d85ff                   test r15, r15
0x2fdb7b3: 0f84d9000000             je 0x2fdb892
0x2fdb7b9: 4885c0                   test rax, rax
0x2fdb7bc: 0f8588000000             jne 0x2fdb84a
0x2fdb7c2: 488d05d7e9b3ff           lea rax, [rip - 0x4c1629]
0x2fdb7c9: 4c8d15200d2cfd           lea r10, [rip - 0x2d3f2e0]
0x2fdb7d0: 4989f4                   mov r12, rsi
0x2fdb7d3: 4c8d1d069e7cfe           lea r11, [rip - 0x18361fa]
0x2fdb7da: 4c8d6dc8                 lea r13, [rbp - 0x38]
0x2fdb7de: 488d3d9d639e04           lea rdi, [rip + 0x49e639d] ; ['/Script/Engine']
0x2fdb7e5: 488d157c6f5406           lea rdx, [rip + 0x6546f7c]
0x2fdb7ec: 488d0d9dfa02fd           lea rcx, [rip - 0x2fd0563]
0x2fdb7f3: 41b8d0020000             mov r8d, 0x2d0
0x2fdb7f9: 41b908000000             mov r9d, 8
0x2fdb7ff: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x2fdb803: 488d0516c39604           lea rax, [rip + 0x496c316] ; ['GameSession']
0x2fdb80a: 4889c6                   mov rsi, rax
0x2fdb80d: 4152                     push r10
0x2fdb80f: 4153                     push r11
0x2fdb811: 4155                     push r13
0x2fdb813: 488d05c64802fd           lea rax, [rip - 0x2fdb73a]
0x2fdb81a: 50                       push rax
0x2fdb81b: 488d055e4a0200           lea rax, [rip + 0x24a5e]
0x2fdb822: 50                       push rax
0x2fdb823: 488d050c3f6504           lea rax, [rip + 0x4653f0c] ; ['Game']
0x2fdb82a: 50                       push rax
0x2fdb82b: 6a00                     push 0
0x2fdb82d: 6804000010               push 0x10000004
0x2fdb832: e8292033fd               call 0x30d860
0x2fdb837: 4c89e6                   mov rsi, r12
0x2fdb83a: 4883c440                 add rsp, 0x40
0x2fdb83e: 488b05236f5406           mov rax, qword ptr [rip + 0x6546f23]
0x2fdb845: 4885c0                   test rax, rax
0x2fdb848: 744d                     je 0x2fdb897
0x2fdb84a: 48634838                 movsxd rcx, dword ptr [rax + 0x38]
0x2fdb84e: 413b4f38                 cmp ecx, dword ptr [r15 + 0x38]
0x2fdb852: 0f8fbb000000             jg 0x2fdb913
0x2fdb858: 498b7f30                 mov rdi, qword ptr [r15 + 0x30]
0x2fdb85c: 488d5030                 lea rdx, [rax + 0x30]
0x2fdb860: 483914cf                 cmp qword ptr [rdi + rcx*8], rdx
0x2fdb864: 0f85a9000000             jne 0x2fdb913
0x2fdb86a: 488bbeb8020000           mov rdi, qword ptr [rsi + 0x2b8]
0x2fdb871: 4885ff                   test rdi, rdi
0x2fdb874: 0f8499000000             je 0x2fdb913
0x2fdb87a: 488d05db353c06           lea rax, [rip + 0x63c35db]
0x2fdb881: 48893b                   mov qword ptr [rbx], rdi
0x2fdb884: 803800                   cmp byte ptr [rax], 0
0x2fdb887: 0f859d000000             jne 0x2fdb92a
0x2fdb88d: e99d000000               jmp 0x2fdb92f
0x2fdb892: 4885c0                   test rax, rax
0x2fdb895: 757c                     jne 0x2fdb913
0x2fdb897: 488d0502e9b3ff           lea rax, [rip - 0x4c16fe]
0x2fdb89e: 4c8d153b9d7cfe           lea r10, [rip - 0x18362c5]
0x2fdb8a5: 4c8d5dc8                 lea r11, [rbp - 0x38]
0x2fdb8a9: 4c8d3d304802fd           lea r15, [rip - 0x2fdb7d0]
0x2fdb8b0: 4c8d25c9490200           lea r12, [rip + 0x249c9]
0x2fdb8b7: 4c8d2d783e6504           lea r13, [rip + 0x4653e78] ; ['Game']
0x2fdb8be: 488d3dbd629e04           lea rdi, [rip + 0x49e62bd] ; ['/Script/Engine']
0x2fdb8c5: 488d3554c29604           lea rsi, [rip + 0x496c254] ; ['GameSession']
0x2fdb8cc: 488d15956e5406           lea rdx, [rip + 0x6546e95]
0x2fdb8d3: 488d0db6f902fd           lea rcx, [rip - 0x2fd064a]
0x2fdb8da: 41b8d0020000             mov r8d, 0x2d0
0x2fdb8e0: 41b908000000             mov r9d, 8
0x2fdb8e6: 488945c8                 mov qword ptr [rbp - 0x38], rax
0x2fdb8ea: 488d05ff0b2cfd           lea rax, [rip - 0x2d3f401]
0x2fdb8f1: 50                       push rax
0x2fdb8f2: 4152                     push r10
0x2fdb8f4: 4153                     push r11
0x2fdb8f6: 4157                     push r15
0x2fdb8f8: 4154                     push r12
0x2fdb8fa: 4155                     push r13
0x2fdb8fc: 6a00                     push 0
