0x459da0 file0x45dda0 55 push rbp
0x459da1 file0x45dda1 4889e5 mov rbp, rsp
0x459da4 file0x45dda4 4157 push r15
0x459da6 file0x45dda6 4156 push r14
0x459da8 file0x45dda8 4155 push r13
0x459daa file0x45ddaa 4154 push r12
0x459dac file0x45ddac 53 push rbx
0x459dad file0x45ddad 4883ec48 sub rsp, 0x48
0x459db1 file0x45ddb1 4c8b2d5081cd08 mov r13, qword ptr [rip + 0x8cd8150] rip->0x9131f08
0x459db8 file0x45ddb8 4989fe mov r14, rdi
0x459dbb file0x45ddbb 4889f3 mov rbx, rsi
0x459dbe file0x45ddbe 498b4500 mov rax, qword ptr [r13]
0x459dc2 file0x45ddc2 488945d0 mov qword ptr [rbp - 0x30], rax
0x459dc6 file0x45ddc6 488b4620 mov rax, qword ptr [rsi + 0x20]
0x459dca file0x45ddca 488b38 mov rdi, qword ptr [rax]
0x459dcd file0x45ddcd 4883c008 add rax, 8
0x459dd1 file0x45ddd1 48894620 mov qword ptr [rsi + 0x20], rax
0x459dd5 file0x45ddd5 48897e30 mov qword ptr [rsi + 0x30], rdi
0x459dd9 file0x45ddd9 4885ff test rdi, rdi
0x459ddc file0x45dddc 7430 je 0x459e0e
0x459dde file0x45ddde 488b4328 mov rax, qword ptr [rbx + 0x28]
0x459de2 file0x45dde2 48634f44 movsxd rcx, dword ptr [rdi + 0x44]
0x459de6 file0x45dde6 4801c1 add rcx, rax
0x459de9 file0x45dde9 48894b38 mov qword ptr [rbx + 0x38], rcx
0x459ded file0x45dded 48894340 mov qword ptr [rbx + 0x40], rax
0x459df1 file0x45ddf1 4885d2 test rdx, rdx
0x459df4 file0x45ddf4 0f8490010000 je 0x459f8a
0x459dfa file0x45ddfa 488b0f mov rcx, qword ptr [rdi]
0x459dfd file0x45ddfd 4889d6 mov rsi, rdx
0x459e00 file0x45de00 4889c2 mov rdx, rax
0x459e03 file0x45de03 ff9118010000 call qword ptr [rcx + 0x118]
0x459e09 file0x45de09 e97c010000 jmp 0x459f8a
