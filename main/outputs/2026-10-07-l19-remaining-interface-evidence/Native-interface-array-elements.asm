0x332600 f0x336600 55                             push rbp
0x332601 f0x336601 4889e5                         mov rbp, rsp
0x332604 f0x336604 4157                           push r15
0x332606 f0x336606 4156                           push r14
0x332608 f0x336608 4155                           push r13
0x33260a f0x33660a 4154                           push r12
0x33260c f0x33660c 53                             push rbx
0x33260d f0x33660d 4883ec18                       sub rsp, 0x18
0x332611 f0x336611 488b05f0f8df08                 mov rax, qword ptr [rip + 0x8dff8f0]
0x332618 f0x336618 4989f6                         mov r14, rsi
0x33261b f0x33661b 4889fb                         mov rbx, rdi
0x33261e f0x33661e 488b00                         mov rax, qword ptr [rax]
0x332621 f0x336621 488945d0                       mov qword ptr [rbp - 0x30], rax
0x332625 f0x336625 48637608                       movsxd rsi, dword ptr [rsi + 8]
0x332629 f0x336629 4963560c                       movsxd rdx, dword ptr [r14 + 0xc]
0x33262d f0x33662d 488b07                         mov rax, qword ptr [rdi]
0x332630 f0x336630 48c1e604                       shl rsi, 4
0x332634 f0x336634 48c1e204                       shl rdx, 4
0x332638 f0x336638 ff5018                         call qword ptr [rax + 0x18]
0x33263b f0x33663b 488b4328                       mov rax, qword ptr [rbx + 0x28]
0x33263f f0x33663f 31c9                           xor ecx, ecx
0x332641 f0x336641 a801                           test al, 1
0x332643 f0x336643 7504                           jne 0x332649
0x332645 f0x336645 418b4e08                       mov ecx, dword ptr [r14 + 8]
0x332649 f0x336649 894dcc                         mov dword ptr [rbp - 0x34], ecx
0x33264c f0x33664c 488b4b08                       mov rcx, qword ptr [rbx + 8]
0x332650 f0x336650 488b11                         mov rdx, qword ptr [rcx]
0x332653 f0x336653 488d7204                       lea rsi, [rdx + 4]
0x332657 f0x336657 483b7108                       cmp rsi, qword ptr [rcx + 8]
0x33265b f0x33665b 7625                           jbe 0x332682
0x33265d f0x33665d a900000100                     test eax, 0x10000
0x332662 f0x336662 0f85a8000000                   jne 0x332710
0x332668 f0x336668 488b03                         mov rax, qword ptr [rbx]
0x33266b f0x33666b 488d75cc                       lea rsi, [rbp - 0x34]
0x33266f f0x33666f ba04000000                     mov edx, 4
0x332674 f0x336674 4889df                         mov rdi, rbx
0x332677 f0x336677 ff9088010000                   call qword ptr [rax + 0x188]
0x33267d f0x33667d e99a000000                     jmp 0x33271c
0x332682 f0x336682 8b12                           mov edx, dword ptr [rdx]
0x332684 f0x336684 8955cc                         mov dword ptr [rbp - 0x34], edx
0x332687 f0x336687 488931                         mov qword ptr [rcx], rsi
0x33268a f0x33668a 488b4328                       mov rax, qword ptr [rbx + 0x28]
0x33268e f0x33668e 4d8d7e0c                       lea r15, [r14 + 0xc]
0x332692 f0x336692 85d2                           test edx, edx
0x332694 f0x336694 0f8495000000                   je 0x33272f
0x33269a f0x33669a b909010000                     mov ecx, 0x109
0x33269f f0x33669f c4e270f7c8                     bextr ecx, eax, ecx
0x3326a4 f0x3366a4 85d2                           test edx, edx
0x3326a6 f0x3366a6 400f98c6                       sets sil
0x3326aa f0x3366aa 4008ce                         or sil, cl
0x3326ad f0x3366ad 740d                           je 0x3326bc
0x3326af f0x3366af 4889df                         mov rdi, rbx
0x3326b2 f0x3366b2 e8a98feeff                     call 0x21b660
0x3326b7 f0x3366b7 e9a0000000                     jmp 0x33275c
0x3326bc f0x3366bc a900000040                     test eax, 0x40000000
0x3326c1 f0x3366c1 0f94c1                         sete cl
0x3326c4 f0x3366c4 81fa01001000                   cmp edx, 0x100001
0x3326ca f0x3366ca 400f92c6                       setb sil
0x3326ce f0x3366ce 4008ce                         or sil, cl
0x3326d1 f0x3366d1 74dc                           je 0x3326af
0x3326d3 f0x3366d3 a801                           test al, 1
0x3326d5 f0x3366d5 0f85a7000000                   jne 0x332782
0x3326db f0x3366db 41895608                       mov dword ptr [r14 + 8], edx
0x3326df f0x3366df 4531ff                         xor r15d, r15d
0x3326e2 f0x3366e2 4531e4                         xor r12d, r12d
0x3326e5 f0x3366e5 66662e0f1f840000000000         nop word ptr cs:[rax + rax]
0x3326f0 f0x3366f0 498b36                         mov rsi, qword ptr [r14]
0x3326f3 f0x3366f3 4889df                         mov rdi, rbx
0x3326f6 f0x3366f6 4c01fe                         add rsi, r15
0x3326f9 f0x3366f9 e85220ffff                     call 0x324750
0x3326fe f0x3366fe 49634608                       movsxd rax, dword ptr [r14 + 8]
0x332702 f0x336702 49ffc4                         inc r12
0x332705 f0x336705 4983c710                       add r15, 0x10
0x332709 f0x336709 4939c4                         cmp r12, rax
0x33270c f0x33670c 7ce2                           jl 0x3326f0
0x33270e f0x33670e eb4c                           jmp 0x33275c
0x332710 f0x336710 488d75cc                       lea rsi, [rbp - 0x34]
0x332714 f0x336714 4889df                         mov rdi, rbx
0x332717 f0x336717 e894a3eeff                     call 0x21cab0
0x33271c f0x33671c 8b55cc                         mov edx, dword ptr [rbp - 0x34]
0x33271f f0x33671f 488b4328                       mov rax, qword ptr [rbx + 0x28]
0x332723 f0x336723 4d8d7e0c                       lea r15, [r14 + 0xc]
0x332727 f0x336727 85d2                           test edx, edx
0x332729 f0x336729 0f856bffffff                   jne 0x33269a
0x33272f f0x33672f a801                           test al, 1
0x332731 f0x336731 7429                           je 0x33275c
0x332733 f0x336733 41c7460800000000               mov dword ptr [r14 + 8], 0
0x33273b f0x33673b 41837e0c00                     cmp dword ptr [r14 + 0xc], 0
0x332740 f0x336740 741a                           je 0x33275c
0x332742 f0x336742 bf10000000                     mov edi, 0x10
0x332747 f0x336747 be08000000                     mov esi, 8
0x33274c f0x33674c 4c89f1                         mov rcx, r14
0x33274f f0x33674f 4d89f9                         mov r9, r15
0x332752 f0x336752 31d2                           xor edx, edx
0x332754 f0x336754 4531c0                         xor r8d, r8d
0x332757 f0x336757 e8c436cfff                     call 0x25e20
0x33275c f0x33675c 488b05a5f7df08                 mov rax, qword ptr [rip + 0x8dff7a5]
0x332763 f0x336763 488b00                         mov rax, qword ptr [rax]
0x332766 f0x336766 483b45d0                       cmp rax, qword ptr [rbp - 0x30]
0x33276a f0x33676a 0f85a2000000                   jne 0x332812
0x332770 f0x336770 4889d8                         mov rax, rbx
0x332773 f0x336773 4883c418                       add rsp, 0x18
0x332777 f0x336777 5b                             pop rbx
0x332778 f0x336778 415c                           pop r12
0x33277a f0x33677a 415d                           pop r13
0x33277c f0x33677c 415e                           pop r14
0x33277e f0x33677e 415f                           pop r15
0x332780 f0x336780 5d                             pop rbp
0x332781 f0x336781 c3                             ret 
0x332782 f0x336782 41c7460800000000               mov dword ptr [r14 + 8], 0
0x33278a f0x33678a 4139560c                       cmp dword ptr [r14 + 0xc], edx
0x33278e f0x33678e 741e                           je 0x3327ae
0x332790 f0x336790 bf10000000                     mov edi, 0x10
0x332795 f0x336795 be08000000                     mov esi, 8
0x33279a f0x33679a 4c89f1                         mov rcx, r14
0x33279d f0x33679d 4d89f9                         mov r9, r15
0x3327a0 f0x3367a0 4531c0                         xor r8d, r8d
0x3327a3 f0x3367a3 e87836cfff                     call 0x25e20
0x3327a8 f0x3367a8 837dcc00                       cmp dword ptr [rbp - 0x34], 0
0x3327ac f0x3367ac 7eae                           jle 0x33275c
0x3327ae f0x3367ae 4531ed                         xor r13d, r13d
0x3327b1 f0x3367b1 eb43                           jmp 0x3327f6
0x3327b3 f0x3367b3 666666662e0f1f840000000000     nop word ptr cs:[rax + rax]
0x3327c0 f0x3367c0 8d4801                         lea ecx, [rax + 1]
0x3327c3 f0x3367c3 4c63e0                         movsxd r12, eax
0x3327c6 f0x3367c6 be01000000                     mov esi, 1
0x3327cb f0x3367cb 41894e08                       mov dword ptr [r14 + 8], ecx
0x3327cf f0x3367cf 49c1e404                       shl r12, 4
0x3327d3 f0x3367d3 4d0326                         add r12, qword ptr [r14]
0x3327d6 f0x3367d6 4c89e7                         mov rdi, r12
0x3327d9 f0x3367d9 e842000000                     call 0x332820
0x3327de f0x3367de 4889df                         mov rdi, rbx
0x3327e1 f0x3367e1 4c89e6                         mov rsi, r12
0x3327e4 f0x3367e4 e8671fffff                     call 0x324750
0x3327e9 f0x3367e9 41ffc5                         inc r13d
0x3327ec f0x3367ec 443b6dcc                       cmp r13d, dword ptr [rbp - 0x34]
0x3327f0 f0x3367f0 0f8d66ffffff                   jge 0x33275c
0x3327f6 f0x3367f6 418b4608                       mov eax, dword ptr [r14 + 8]
0x3327fa f0x3367fa 413b460c                       cmp eax, dword ptr [r14 + 0xc]
0x3327fe f0x3367fe 75c0                           jne 0x3327c0
0x332800 f0x336800 bf10080000                     mov edi, 0x810
0x332805 f0x336805 4c89f6                         mov rsi, r14
0x332808 f0x336808 4c89fa                         mov rdx, r15
0x33280b f0x33680b e89070cfff                     call 0x298a0
0x332810 f0x336810 ebae                           jmp 0x3327c0
0x332812 f0x336812 e849f61307                     call 0x7471e60
0x332817 f0x336817 0f0b                           ud2 
0x332819 f0x336819 cc                             int3 
0x33281a f0x33681a cc                             int3 
0x33281b f0x33681b cc                             int3 
0x33281c f0x33681c cc                             int3 
0x33281d f0x33681d cc                             int3 
0x33281e f0x33681e cc                             int3 
0x33281f f0x33681f cc                             int3 
0x332820 f0x336820 85f6                           test esi, esi
0x332822 f0x336822 7427                           je 0x33284b
0x332824 f0x336824 55                             push rbp
0x332825 f0x336825 4889e5                         mov rbp, rsp
0x332828 f0x336828 0f1f840000000000               nop dword ptr [rax + rax]
0x332830 f0x336830 48c70700000000                 mov qword ptr [rdi], 0
0x332837 f0x336837 c7470800000000                 mov dword ptr [rdi + 8], 0
0x33283e f0x33683e c6470c00                       mov byte ptr [rdi + 0xc], 0
0x332842 f0x336842 4883c710                       add rdi, 0x10
0x332846 f0x336846 ffce                           dec esi
0x332848 f0x336848 75e6                           jne 0x332830
0x33284a f0x33684a 5d                             pop rbp
0x33284b f0x33684b c3                             ret 
0x33284c f0x33684c cc                             int3 
0x33284d f0x33684d cc                             int3 
0x33284e f0x33684e cc                             int3 
0x33284f f0x33684f cc                             int3 
0x332850 f0x336850 85d2                           test edx, edx
0x332852 f0x336852 7474                           je 0x3328c8
0x332854 f0x336854 55                             push rbp
0x332855 f0x336855 4889e5                         mov rbp, rsp
0x332858 f0x336858 4157                           push r15
0x33285a f0x33685a 4156                           push r14
0x33285c f0x33685c 4155                           push r13
0x33285e f0x33685e 4154                           push r12
0x332860 f0x336860 53                             push rbx
0x332861 f0x336861 50                             push rax
0x332862 f0x336862 4c8d2df3c50609                 lea r13, [rip + 0x906c5f3]
0x332869 f0x336869 89d3                           mov ebx, edx
0x33286b f0x33686b 4989f6                         mov r14, rsi
0x33286e f0x33686e 4989ff                         mov r15, rdi
0x332871 f0x336871 4531e4                         xor r12d, r12d
0x332874 f0x336874 eb27                           jmp 0x33289d
0x332876 f0x336876 662e0f1f840000000000           nop word ptr cs:[rax + rax]
0x332880 f0x336880 4b8d0426                       lea rax, [r14 + r12]
0x332884 f0x336884 4b8d0c27                       lea rcx, [r15 + r12]
0x332888 f0x336888 4983c410                       add r12, 0x10
0x33288c f0x33688c ffcb                           dec ebx
0x33288e f0x33688e 0f                             .byte 0x0f
0x33288f f0x33688f b6                             .byte 0xb6
