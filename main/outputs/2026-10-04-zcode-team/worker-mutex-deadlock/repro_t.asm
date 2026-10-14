
repro_t:	file format mach-o 64-bit x86-64

Disassembly of section __TEXT,__text:

0000000100000650 <_mb_win_workers_invoke>:
100000650: 55                          	pushq	%rbp
100000651: 48 89 e5                    	movq	%rsp, %rbp
100000654: 48 83 ec 20                 	subq	$0x20, %rsp
100000658: 48 89 f1                    	movq	%rsi, %rcx
10000065b: ff d7                       	callq	*%rdi
10000065d: 48 83 c4 20                 	addq	$0x20, %rsp
100000661: 5d                          	popq	%rbp
100000662: c3                          	retq
100000663: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)

0000000100000670 <_main>:
100000670: 55                          	pushq	%rbp
100000671: 48 89 e5                    	movq	%rsp, %rbp
100000674: 41 57                       	pushq	%r15
100000676: 41 56                       	pushq	%r14
100000678: 41 55                       	pushq	%r13
10000067a: 41 54                       	pushq	%r12
10000067c: 53                          	pushq	%rbx
10000067d: 48 81 ec 98 00 00 00        	subq	$0x98, %rsp
100000684: bf 28 00 00 00              	movl	$0x28, %edi
100000689: e8 88 7f 00 00              	callq	0x100008616 <_sysconf+0x100008616>
10000068e: 48 8d 35 7b 0f 00 00        	leaq	0xf7b(%rip), %rsi       ## 0x100001610 <_on_alarm>
100000695: bf 0e 00 00 00              	movl	$0xe, %edi
10000069a: e8 3d 80 00 00              	callq	0x1000086dc <_sysconf+0x1000086dc>
10000069f: 4c 8b 3d 72 99 00 00        	movq	0x9972(%rip), %r15      ## 0x10000a018 <_sysconf+0x10000a018>
1000006a6: 49 8b 3f                    	movq	(%r15), %rdi
1000006a9: 31 f6                       	xorl	%esi, %esi
1000006ab: e8 26 80 00 00              	callq	0x1000086d6 <_sysconf+0x1000086d6>
1000006b0: c7 45 d4 00 00 00 00        	movl	$0x0, -0x2c(%rbp)
1000006b7: 49 8b 3f                    	movq	(%r15), %rdi
1000006ba: 48 8d 35 97 85 00 00        	leaq	0x8597(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
1000006c1: 48 8d 15 20 80 00 00        	leaq	0x8020(%rip), %rdx      ## 0x1000086e8 <_sysconf+0x1000086e8>
1000006c8: 31 c0                       	xorl	%eax, %eax
1000006ca: e8 65 7f 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000006cf: 49 8b 3f                    	movq	(%r15), %rdi
1000006d2: e8 57 7f 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000006d7: 48 8d 3d 2a a9 00 00        	leaq	0xa92a(%rip), %rdi      ## 0x10000b008 <_domain>
1000006de: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
1000006e2: e8 59 13 00 00              	callq	0x100001a40 <_mb_win_threads_open>
1000006e7: 85 c0                       	testl	%eax, %eax
1000006e9: 0f 85 81 0b 00 00           	jne	0x100001270 <_main+0xc00>
1000006ef: 48 8d 3d 12 a9 00 00        	leaq	0xa912(%rip), %rdi      ## 0x10000b008 <_domain>
1000006f6: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
1000006fa: e8 91 14 00 00              	callq	0x100001b90 <_mb_win_threads_enter>
1000006ff: 49 8b 3f                    	movq	(%r15), %rdi
100000702: 85 c0                       	testl	%eax, %eax
100000704: 0f 85 7c 0b 00 00           	jne	0x100001286 <_main+0xc16>
10000070a: 48 8d 35 47 85 00 00        	leaq	0x8547(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000711: 48 8d 15 fe 7f 00 00        	leaq	0x7ffe(%rip), %rdx      ## 0x100008716 <_sysconf+0x100008716>
100000718: 31 c0                       	xorl	%eax, %eax
10000071a: e8 15 7f 00 00              	callq	0x100008634 <_sysconf+0x100008634>
10000071f: 49 8b 3f                    	movq	(%r15), %rdi
100000722: e8 07 7f 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000727: 48 8b 05 3a 9a 00 00        	movq	0x9a3a(%rip), %rax      ## 0x10000a168 <_sysconf+0x10000a168>
10000072e: 48 89 85 70 ff ff ff        	movq	%rax, -0x90(%rbp)
100000735: 0f 10 05 1c 9a 00 00        	movups	0x9a1c(%rip), %xmm0     ## 0x10000a158 <_sysconf+0x10000a158>
10000073c: 0f 29 85 60 ff ff ff        	movaps	%xmm0, -0xa0(%rbp)
100000743: 0f 10 05 fe 99 00 00        	movups	0x99fe(%rip), %xmm0     ## 0x10000a148 <_sysconf+0x10000a148>
10000074a: 0f 29 85 50 ff ff ff        	movaps	%xmm0, -0xb0(%rbp)
100000751: 48 8d 3d b8 a8 00 00        	leaq	0xa8b8(%rip), %rdi      ## 0x10000b010 <_workers>
100000758: 48 8d b5 50 ff ff ff        	leaq	-0xb0(%rbp), %rsi
10000075f: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
100000763: e8 d8 1d 00 00              	callq	0x100002540 <_mb_win_workers_open>
100000768: 85 c0                       	testl	%eax, %eax
10000076a: 0f 85 29 0b 00 00           	jne	0x100001299 <_main+0xc29>
100000770: 48 8d 3d a1 a8 00 00        	leaq	0xa8a1(%rip), %rdi      ## 0x10000b018 <_critical>
100000777: 48 8d 35 8a a8 00 00        	leaq	0xa88a(%rip), %rsi      ## 0x10000b008 <_domain>
10000077e: 48 8d 4d d4                 	leaq	-0x2c(%rbp), %rcx
100000782: ba 10 00 00 00              	movl	$0x10, %edx
100000787: e8 24 30 00 00              	callq	0x1000037b0 <_mb_win_critical_open>
10000078c: 85 c0                       	testl	%eax, %eax
10000078e: 0f 85 1b 0b 00 00           	jne	0x1000012af <_main+0xc3f>
100000794: 48 8d 3d 85 a8 00 00        	leaq	0xa885(%rip), %rdi      ## 0x10000b020 <_address_wait>
10000079b: 48 8d 35 66 a8 00 00        	leaq	0xa866(%rip), %rsi      ## 0x10000b008 <_domain>
1000007a2: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
1000007a6: e8 15 3b 00 00              	callq	0x1000042c0 <_mb_win_address_wait_open>
1000007ab: 85 c0                       	testl	%eax, %eax
1000007ad: 0f 85 12 0b 00 00           	jne	0x1000012c5 <_main+0xc55>
1000007b3: 0f 10 05 b6 99 00 00        	movups	0x99b6(%rip), %xmm0     ## 0x10000a170 <_sysconf+0x10000a170>
1000007ba: 0f 29 45 80                 	movaps	%xmm0, -0x80(%rbp)
1000007be: 48 8d 3d 63 a8 00 00        	leaq	0xa863(%rip), %rdi      ## 0x10000b028 <_srw>
1000007c5: 48 8d 75 80                 	leaq	-0x80(%rbp), %rsi
1000007c9: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
1000007cd: e8 6e 44 00 00              	callq	0x100004c40 <_mb_win_srw_open>
1000007d2: 85 c0                       	testl	%eax, %eax
1000007d4: 0f 85 01 0b 00 00           	jne	0x1000012db <_main+0xc6b>
1000007da: 0f 10 05 9f 99 00 00        	movups	0x999f(%rip), %xmm0     ## 0x10000a180 <_sysconf+0x10000a180>
1000007e1: 0f 29 45 90                 	movaps	%xmm0, -0x70(%rbp)
1000007e5: 48 8d 3d 44 a8 00 00        	leaq	0xa844(%rip), %rdi      ## 0x10000b030 <_init_once>
1000007ec: 48 8d 75 90                 	leaq	-0x70(%rbp), %rsi
1000007f0: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
1000007f4: e8 57 57 00 00              	callq	0x100005f50 <_mb_win_init_once_open>
1000007f9: 85 c0                       	testl	%eax, %eax
1000007fb: 0f 85 f0 0a 00 00           	jne	0x1000012f1 <_main+0xc81>
100000801: 0f 10 05 88 99 00 00        	movups	0x9988(%rip), %xmm0     ## 0x10000a190 <_sysconf+0x10000a190>
100000808: 0f 29 45 a0                 	movaps	%xmm0, -0x60(%rbp)
10000080c: 48 8d 3d 25 a8 00 00        	leaq	0xa825(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000813: 48 8d 75 a0                 	leaq	-0x60(%rbp), %rsi
100000817: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
10000081b: e8 60 63 00 00              	callq	0x100006b80 <_mb_win_mutex_open>
100000820: 49 8b 3f                    	movq	(%r15), %rdi
100000823: 85 c0                       	testl	%eax, %eax
100000825: 0f 85 dc 0a 00 00           	jne	0x100001307 <_main+0xc97>
10000082b: 48 8d 35 26 84 00 00        	leaq	0x8426(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000832: 48 8d 15 71 7f 00 00        	leaq	0x7f71(%rip), %rdx      ## 0x1000087aa <_sysconf+0x1000087aa>
100000839: 31 c0                       	xorl	%eax, %eax
10000083b: e8 f4 7d 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000840: 49 8b 3f                    	movq	(%r15), %rdi
100000843: e8 e6 7d 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000848: 0f 57 c0                    	xorps	%xmm0, %xmm0
10000084b: 0f 11 05 ee a7 00 00        	movups	%xmm0, 0xa7ee(%rip)     ## 0x10000b040 <_S>
100000852: 0f 11 05 f7 a7 00 00        	movups	%xmm0, 0xa7f7(%rip)     ## 0x10000b050 <_S+0x10>
100000859: 0f 11 05 00 a8 00 00        	movups	%xmm0, 0xa800(%rip)     ## 0x10000b060 <_S+0x20>
100000860: 0f 11 05 09 a8 00 00        	movups	%xmm0, 0xa809(%rip)     ## 0x10000b070 <_S+0x30>
100000867: 0f 11 05 12 a8 00 00        	movups	%xmm0, 0xa812(%rip)     ## 0x10000b080 <_S+0x40>
10000086e: 0f 11 05 1b a8 00 00        	movups	%xmm0, 0xa81b(%rip)     ## 0x10000b090 <_S+0x50>
100000875: 48 8d 35 c4 a7 00 00        	leaq	0xa7c4(%rip), %rsi      ## 0x10000b040 <_S>
10000087c: 48 8d 3d 95 a7 00 00        	leaq	0xa795(%rip), %rdi      ## 0x10000b018 <_critical>
100000883: ba 0b 00 00 00              	movl	$0xb, %edx
100000888: e8 d3 33 00 00              	callq	0x100003c60 <_mb_win_critical_initialize>
10000088d: 85 c0                       	testl	%eax, %eax
10000088f: 0f 85 85 0a 00 00           	jne	0x10000131a <_main+0xcaa>
100000895: 48 8d 3d 7c a7 00 00        	leaq	0xa77c(%rip), %rdi      ## 0x10000b018 <_critical>
10000089c: 48 8d 35 9d a7 00 00        	leaq	0xa79d(%rip), %rsi      ## 0x10000b040 <_S>
1000008a3: e8 a8 35 00 00              	callq	0x100003e50 <_mb_win_critical_enter>
1000008a8: 85 c0                       	testl	%eax, %eax
1000008aa: 0f 85 80 0a 00 00           	jne	0x100001330 <_main+0xcc0>
1000008b0: 48 8d 1d 71 a7 00 00        	leaq	0xa771(%rip), %rbx      ## 0x10000b028 <_srw>
1000008b7: 48 8d 35 aa a7 00 00        	leaq	0xa7aa(%rip), %rsi      ## 0x10000b068 <_S+0x28>
1000008be: 48 89 df                    	movq	%rbx, %rdi
1000008c1: e8 1a 47 00 00              	callq	0x100004fe0 <_mb_win_srw_initialize>
1000008c6: 48 8d 35 a3 a7 00 00        	leaq	0xa7a3(%rip), %rsi      ## 0x10000b070 <_S+0x30>
1000008cd: 48 89 df                    	movq	%rbx, %rdi
1000008d0: e8 9b 47 00 00              	callq	0x100005070 <_mb_win_srw_condition_initialize>
1000008d5: 49 8b 3f                    	movq	(%r15), %rdi
1000008d8: 48 8d 35 79 83 00 00        	leaq	0x8379(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
1000008df: 48 8d 15 13 7f 00 00        	leaq	0x7f13(%rip), %rdx      ## 0x1000087f9 <_sysconf+0x1000087f9>
1000008e6: 31 c0                       	xorl	%eax, %eax
1000008e8: e8 47 7d 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000008ed: 49 8b 3f                    	movq	(%r15), %rdi
1000008f0: e8 39 7d 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000008f5: c7 45 d0 00 00 00 00        	movl	$0x0, -0x30(%rbp)
1000008fc: 48 c7 45 c0 00 00 00 00     	movq	$0x0, -0x40(%rbp)
100000904: 48 8d 3d 25 a7 00 00        	leaq	0xa725(%rip), %rdi      ## 0x10000b030 <_init_once>
10000090b: 48 8d 35 66 a7 00 00        	leaq	0xa766(%rip), %rsi      ## 0x10000b078 <_S+0x38>
100000912: 48 8d 4d d0                 	leaq	-0x30(%rbp), %rcx
100000916: 4c 8d 45 c0                 	leaq	-0x40(%rbp), %r8
10000091a: 31 d2                       	xorl	%edx, %edx
10000091c: e8 ff 59 00 00              	callq	0x100006320 <_mb_win_init_once_begin>
100000921: 85 c0                       	testl	%eax, %eax
100000923: 0f 84 1d 0a 00 00           	je	0x100001346 <_main+0xcd6>
100000929: 83 7d d0 01                 	cmpl	$0x1, -0x30(%rbp)
10000092d: 0f 85 13 0a 00 00           	jne	0x100001346 <_main+0xcd6>
100000933: 48 bb 01 00 5a 5a ef be d1 51       	movabsq	$0x51d1beef5a5a0001, %rbx ## imm = 0x51D1BEEF5A5A0001
10000093d: 48 8d 3d ec a6 00 00        	leaq	0xa6ec(%rip), %rdi      ## 0x10000b030 <_init_once>
100000944: 48 8d 35 2d a7 00 00        	leaq	0xa72d(%rip), %rsi      ## 0x10000b078 <_S+0x38>
10000094b: 31 d2                       	xorl	%edx, %edx
10000094d: 48 89 d9                    	movq	%rbx, %rcx
100000950: e8 3b 5f 00 00              	callq	0x100006890 <_mb_win_init_once_complete>
100000955: 85 c0                       	testl	%eax, %eax
100000957: 0f 84 ff 09 00 00           	je	0x10000135c <_main+0xcec>
10000095d: 48 8d 3d cc a6 00 00        	leaq	0xa6cc(%rip), %rdi      ## 0x10000b030 <_init_once>
100000964: 48 8d 35 0d a7 00 00        	leaq	0xa70d(%rip), %rsi      ## 0x10000b078 <_S+0x38>
10000096b: 48 8d 4d d0                 	leaq	-0x30(%rbp), %rcx
10000096f: 4c 8d 45 c0                 	leaq	-0x40(%rbp), %r8
100000973: ba 01 00 00 00              	movl	$0x1, %edx
100000978: e8 a3 59 00 00              	callq	0x100006320 <_mb_win_init_once_begin>
10000097d: 85 c0                       	testl	%eax, %eax
10000097f: 0f 84 ed 09 00 00           	je	0x100001372 <_main+0xd02>
100000985: 83 7d d0 00                 	cmpl	$0x0, -0x30(%rbp)
100000989: 0f 85 e3 09 00 00           	jne	0x100001372 <_main+0xd02>
10000098f: 48 39 5d c0                 	cmpq	%rbx, -0x40(%rbp)
100000993: 0f 85 d9 09 00 00           	jne	0x100001372 <_main+0xd02>
100000999: 48 8d 3d 90 a6 00 00        	leaq	0xa690(%rip), %rdi      ## 0x10000b030 <_init_once>
1000009a0: 48 8d 35 d1 a6 00 00        	leaq	0xa6d1(%rip), %rsi      ## 0x10000b078 <_S+0x38>
1000009a7: 31 d2                       	xorl	%edx, %edx
1000009a9: 48 89 d9                    	movq	%rbx, %rcx
1000009ac: e8 df 5e 00 00              	callq	0x100006890 <_mb_win_init_once_complete>
1000009b1: 85 c0                       	testl	%eax, %eax
1000009b3: 0f 85 cf 09 00 00           	jne	0x100001388 <_main+0xd18>
1000009b9: 48 8d 3d 70 a6 00 00        	leaq	0xa670(%rip), %rdi      ## 0x10000b030 <_init_once>
1000009c0: 48 8d 35 b9 a6 00 00        	leaq	0xa6b9(%rip), %rsi      ## 0x10000b080 <_S+0x40>
1000009c7: 48 8d 5d d0                 	leaq	-0x30(%rbp), %rbx
1000009cb: 4c 8d 45 c0                 	leaq	-0x40(%rbp), %r8
1000009cf: ba 01 00 00 00              	movl	$0x1, %edx
1000009d4: 48 89 d9                    	movq	%rbx, %rcx
1000009d7: e8 44 59 00 00              	callq	0x100006320 <_mb_win_init_once_begin>
1000009dc: 85 c0                       	testl	%eax, %eax
1000009de: 0f 85 1e 08 00 00           	jne	0x100001202 <_main+0xb92>
1000009e4: 48 8d 3d 1d a6 00 00        	leaq	0xa61d(%rip), %rdi      ## 0x10000b008 <_domain>
1000009eb: e8 30 17 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
1000009f0: 81 38 e5 03 00 00           	cmpl	$0x3e5, (%rax)          ## imm = 0x3E5
1000009f6: 0f 85 06 08 00 00           	jne	0x100001202 <_main+0xb92>
1000009fc: 48 c7 05 71 a6 00 00 00 00 00 00    	movq	$0x0, 0xa671(%rip) ## 0x10000b078 <_S+0x38>
100000a07: 49 8b 3f                    	movq	(%r15), %rdi
100000a0a: 48 8d 35 47 82 00 00        	leaq	0x8247(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000a11: 48 8d 15 36 7e 00 00        	leaq	0x7e36(%rip), %rdx      ## 0x10000884e <_sysconf+0x10000884e>
100000a18: 31 c0                       	xorl	%eax, %eax
100000a1a: e8 15 7c 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000a1f: 49 8b 3f                    	movq	(%r15), %rdi
100000a22: e8 07 7c 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000a27: 48 8d 3d da a5 00 00        	leaq	0xa5da(%rip), %rdi      ## 0x10000b008 <_domain>
100000a2e: e8 ed 16 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000a33: c7 00 ce 8a 46 c2           	movl	$0xc2468ace, (%rax)     ## imm = 0xC2468ACE
100000a39: 48 8d 3d f8 a5 00 00        	leaq	0xa5f8(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000a40: 31 f6                       	xorl	%esi, %esi
100000a42: 31 d2                       	xorl	%edx, %edx
100000a44: e8 87 66 00 00              	callq	0x1000070d0 <_mb_win_mutex_create>
100000a49: 48 85 c0                    	testq	%rax, %rax
100000a4c: 0f 84 4c 09 00 00           	je	0x10000139e <_main+0xd2e>
100000a52: 49 89 c6                    	movq	%rax, %r14
100000a55: 48 8d 3d dc a5 00 00        	leaq	0xa5dc(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000a5c: 48 89 c6                    	movq	%rax, %rsi
100000a5f: ba 01 00 00 00              	movl	$0x1, %edx
100000a64: 31 c9                       	xorl	%ecx, %ecx
100000a66: e8 b5 70 00 00              	callq	0x100007b20 <_mb_win_mutex_wait_ex>
100000a6b: 85 c0                       	testl	%eax, %eax
100000a6d: 0f 85 41 09 00 00           	jne	0x1000013b4 <_main+0xd44>
100000a73: 48 8d 3d be a5 00 00        	leaq	0xa5be(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000a7a: 4c 89 f6                    	movq	%r14, %rsi
100000a7d: e8 6e 6e 00 00              	callq	0x1000078f0 <_mb_win_mutex_release>
100000a82: 83 f8 01                    	cmpl	$0x1, %eax
100000a85: 0f 85 3f 09 00 00           	jne	0x1000013ca <_main+0xd5a>
100000a8b: 48 8d 3d a6 a5 00 00        	leaq	0xa5a6(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000a92: 4c 89 f6                    	movq	%r14, %rsi
100000a95: 31 d2                       	xorl	%edx, %edx
100000a97: b9 01 00 00 00              	movl	$0x1, %ecx
100000a9c: e8 7f 70 00 00              	callq	0x100007b20 <_mb_win_mutex_wait_ex>
100000aa1: 83 f8 ff                    	cmpl	$-0x1, %eax
100000aa4: 0f 85 6e 07 00 00           	jne	0x100001218 <_main+0xba8>
100000aaa: 48 8d 3d 57 a5 00 00        	leaq	0xa557(%rip), %rdi      ## 0x10000b008 <_domain>
100000ab1: e8 6a 16 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000ab6: 83 38 32                    	cmpl	$0x32, (%rax)
100000ab9: 0f 85 59 07 00 00           	jne	0x100001218 <_main+0xba8>
100000abf: 48 8d 3d 72 a5 00 00        	leaq	0xa572(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000ac6: 48 8d 35 33 86 00 00        	leaq	0x8633(%rip), %rsi      ## 0x100009100 <_named>
100000acd: 31 d2                       	xorl	%edx, %edx
100000acf: e8 fc 65 00 00              	callq	0x1000070d0 <_mb_win_mutex_create>
100000ad4: 48 85 c0                    	testq	%rax, %rax
100000ad7: 0f 84 03 09 00 00           	je	0x1000013e0 <_main+0xd70>
100000add: 48 8d 3d 54 a5 00 00        	leaq	0xa554(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000ae4: 48 8d 35 35 86 00 00        	leaq	0x8635(%rip), %rsi      ## 0x100009120 <_named_folded>
100000aeb: e8 50 6c 00 00              	callq	0x100007740 <_mb_win_mutex_open_named>
100000af0: 48 85 c0                    	testq	%rax, %rax
100000af3: 0f 84 fd 08 00 00           	je	0x1000013f6 <_main+0xd86>
100000af9: 48 8d 3d 38 a5 00 00        	leaq	0xa538(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000b00: 48 8d 35 f9 85 00 00        	leaq	0x85f9(%rip), %rsi      ## 0x100009100 <_named>
100000b07: ba 01 00 00 00              	movl	$0x1, %edx
100000b0c: e8 bf 65 00 00              	callq	0x1000070d0 <_mb_win_mutex_create>
100000b11: 48 85 c0                    	testq	%rax, %rax
100000b14: 0f 84 14 07 00 00           	je	0x10000122e <_main+0xbbe>
100000b1a: 49 89 c6                    	movq	%rax, %r14
100000b1d: 48 8d 3d e4 a4 00 00        	leaq	0xa4e4(%rip), %rdi      ## 0x10000b008 <_domain>
100000b24: e8 f7 15 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000b29: 81 38 b7 00 00 00           	cmpl	$0xb7, (%rax)
100000b2f: 0f 85 f9 06 00 00           	jne	0x10000122e <_main+0xbbe>
100000b35: 48 8d 3d fc a4 00 00        	leaq	0xa4fc(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000b3c: 4c 89 f6                    	movq	%r14, %rsi
100000b3f: e8 ac 6d 00 00              	callq	0x1000078f0 <_mb_win_mutex_release>
100000b44: 85 c0                       	testl	%eax, %eax
100000b46: 0f 85 f8 06 00 00           	jne	0x100001244 <_main+0xbd4>
100000b4c: 48 8d 3d b5 a4 00 00        	leaq	0xa4b5(%rip), %rdi      ## 0x10000b008 <_domain>
100000b53: e8 c8 15 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000b58: 81 38 20 01 00 00           	cmpl	$0x120, (%rax)          ## imm = 0x120
100000b5e: 0f 85 e0 06 00 00           	jne	0x100001244 <_main+0xbd4>
100000b64: 4c 8d 35 cd a4 00 00        	leaq	0xa4cd(%rip), %r14      ## 0x10000b038 <_mutex_owner>
100000b6b: 48 8d 35 ae 85 00 00        	leaq	0x85ae(%rip), %rsi      ## 0x100009120 <_named_folded>
100000b72: 4c 89 f7                    	movq	%r14, %rdi
100000b75: e8 c6 6b 00 00              	callq	0x100007740 <_mb_win_mutex_open_named>
100000b7a: 4c 89 f7                    	movq	%r14, %rdi
100000b7d: 48 89 c6                    	movq	%rax, %rsi
100000b80: 31 d2                       	xorl	%edx, %edx
100000b82: 31 c9                       	xorl	%ecx, %ecx
100000b84: e8 97 6f 00 00              	callq	0x100007b20 <_mb_win_mutex_wait_ex>
100000b89: 85 c0                       	testl	%eax, %eax
100000b8b: 0f 85 7b 08 00 00           	jne	0x10000140c <_main+0xd9c>
100000b91: 48 8d 3d a0 a4 00 00        	leaq	0xa4a0(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000b98: 48 8d 35 a1 85 00 00        	leaq	0x85a1(%rip), %rsi      ## 0x100009140 <_backslash>
100000b9f: 31 d2                       	xorl	%edx, %edx
100000ba1: e8 2a 65 00 00              	callq	0x1000070d0 <_mb_win_mutex_create>
100000ba6: 48 85 c0                    	testq	%rax, %rax
100000ba9: 0f 85 ab 06 00 00           	jne	0x10000125a <_main+0xbea>
100000baf: 48 8d 3d 52 a4 00 00        	leaq	0xa452(%rip), %rdi      ## 0x10000b008 <_domain>
100000bb6: e8 65 15 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000bbb: 83 38 57                    	cmpl	$0x57, (%rax)
100000bbe: 0f 85 96 06 00 00           	jne	0x10000125a <_main+0xbea>
100000bc4: 49 8b 3f                    	movq	(%r15), %rdi
100000bc7: 48 8d 35 8a 80 00 00        	leaq	0x808a(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000bce: 48 8d 15 ad 7c 00 00        	leaq	0x7cad(%rip), %rdx      ## 0x100008882 <_sysconf+0x100008882>
100000bd5: 31 c0                       	xorl	%eax, %eax
100000bd7: e8 58 7a 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000bdc: 49 8b 3f                    	movq	(%r15), %rdi
100000bdf: e8 4a 7a 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000be4: 48 8d 3d 25 a4 00 00        	leaq	0xa425(%rip), %rdi      ## 0x10000b010 <_workers>
100000beb: e8 d0 1d 00 00              	callq	0x1000029c0 <_mb_win_workers_enter_call>
100000bf0: 85 c0                       	testl	%eax, %eax
100000bf2: 0f 85 2a 08 00 00           	jne	0x100001422 <_main+0xdb2>
100000bf8: c7 45 d0 00 00 00 00        	movl	$0x0, -0x30(%rbp)
100000bff: 48 89 1c 24                 	movq	%rbx, (%rsp)
100000c03: 48 8d 3d 06 a4 00 00        	leaq	0xa406(%rip), %rdi      ## 0x10000b010 <_workers>
100000c0a: 48 8d 0d 6f 0a 00 00        	leaq	0xa6f(%rip), %rcx       ## 0x100001680 <_worker_start>
100000c11: 4c 8d 05 28 a4 00 00        	leaq	0xa428(%rip), %r8       ## 0x10000b040 <_S>
100000c18: 31 db                       	xorl	%ebx, %ebx
100000c1a: 31 f6                       	xorl	%esi, %esi
100000c1c: 31 d2                       	xorl	%edx, %edx
100000c1e: 45 31 c9                    	xorl	%r9d, %r9d
100000c21: e8 7a 21 00 00              	callq	0x100002da0 <_mb_win_workers_create>
100000c26: 48 89 05 73 a4 00 00        	movq	%rax, 0xa473(%rip)      ## 0x10000b0a0 <_thread_handle>
100000c2d: 48 85 c0                    	testq	%rax, %rax
100000c30: 0f 84 02 08 00 00           	je	0x100001438 <_main+0xdc8>
100000c36: 48 8d 3d eb a3 00 00        	leaq	0xa3eb(%rip), %rdi      ## 0x10000b028 <_srw>
100000c3d: 48 8d 35 24 a4 00 00        	leaq	0xa424(%rip), %rsi      ## 0x10000b068 <_S+0x28>
100000c44: ba 01 00 00 00              	movl	$0x1, %edx
100000c49: e8 32 44 00 00              	callq	0x100005080 <_mb_win_srw_acquire>
100000c4e: 8b 05 38 a4 00 00           	movl	0xa438(%rip), %eax      ## 0x10000b08c <_S+0x4c>
100000c54: 85 c0                       	testl	%eax, %eax
100000c56: 0f 84 02 05 00 00           	je	0x10000115e <_main+0xaee>
100000c5c: 48 8d 3d c5 a3 00 00        	leaq	0xa3c5(%rip), %rdi      ## 0x10000b028 <_srw>
100000c63: 48 8d 35 fe a3 00 00        	leaq	0xa3fe(%rip), %rsi      ## 0x10000b068 <_S+0x28>
100000c6a: ba 01 00 00 00              	movl	$0x1, %edx
100000c6f: e8 2c 49 00 00              	callq	0x1000055a0 <_mb_win_srw_release>
100000c74: 48 8d 35 c5 a3 00 00        	leaq	0xa3c5(%rip), %rsi      ## 0x10000b040 <_S>
100000c7b: 48 8d 3d 96 a3 00 00        	leaq	0xa396(%rip), %rdi      ## 0x10000b018 <_critical>
100000c82: e8 b9 34 00 00              	callq	0x100004140 <_mb_win_critical_leave>
100000c87: 4c 8b 2d 8a 93 00 00        	movq	0x938a(%rip), %r13      ## 0x10000a018 <_sysconf+0x10000a018>
100000c8e: 49 8b 7d 00                 	movq	(%r13), %rdi
100000c92: 48 8d 35 45 7c 00 00        	leaq	0x7c45(%rip), %rsi      ## 0x1000088de <_sysconf+0x1000088de>
100000c99: 89 da                       	movl	%ebx, %edx
100000c9b: 31 c0                       	xorl	%eax, %eax
100000c9d: e8 92 79 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000ca2: 48 8b 35 f7 a3 00 00        	movq	0xa3f7(%rip), %rsi      ## 0x10000b0a0 <_thread_handle>
100000ca9: 48 8d 3d 60 a3 00 00        	leaq	0xa360(%rip), %rdi      ## 0x10000b010 <_workers>
100000cb0: 31 d2                       	xorl	%edx, %edx
100000cb2: e8 f9 24 00 00              	callq	0x1000031b0 <_mb_win_workers_wait>
100000cb7: 3d 02 01 00 00              	cmpl	$0x102, %eax            ## imm = 0x102
100000cbc: 0f 85 93 07 00 00           	jne	0x100001455 <_main+0xde5>
100000cc2: c7 45 cc 00 00 00 00        	movl	$0x0, -0x34(%rbp)
100000cc9: 48 8b 35 d0 a3 00 00        	movq	0xa3d0(%rip), %rsi      ## 0x10000b0a0 <_thread_handle>
100000cd0: 48 8d 3d 39 a3 00 00        	leaq	0xa339(%rip), %rdi      ## 0x10000b010 <_workers>
100000cd7: 48 8d 55 cc                 	leaq	-0x34(%rbp), %rdx
100000cdb: e8 60 28 00 00              	callq	0x100003540 <_mb_win_workers_exit_code>
100000ce0: 85 c0                       	testl	%eax, %eax
100000ce2: 0f 84 84 07 00 00           	je	0x10000146c <_main+0xdfc>
100000ce8: 81 7d cc 03 01 00 00        	cmpl	$0x103, -0x34(%rbp)     ## imm = 0x103
100000cef: 0f 85 77 07 00 00           	jne	0x10000146c <_main+0xdfc>
100000cf5: 48 8d 1d 14 a3 00 00        	leaq	0xa314(%rip), %rbx      ## 0x10000b010 <_workers>
100000cfc: 48 89 df                    	movq	%rbx, %rdi
100000cff: 31 f6                       	xorl	%esi, %esi
100000d01: e8 aa 29 00 00              	callq	0x1000036b0 <_mb_win_workers_sleep>
100000d06: 49 8b 7d 00                 	movq	(%r13), %rdi
100000d0a: 4c 8d 35 47 7f 00 00        	leaq	0x7f47(%rip), %r14      ## 0x100008c58 <_sysconf+0x100008c58>
100000d11: 48 8d 15 04 7c 00 00        	leaq	0x7c04(%rip), %rdx      ## 0x10000891c <_sysconf+0x10000891c>
100000d18: 4c 89 f6                    	movq	%r14, %rsi
100000d1b: 31 c0                       	xorl	%eax, %eax
100000d1d: e8 12 79 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000d22: 49 8b 7d 00                 	movq	(%r13), %rdi
100000d26: e8 03 79 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000d2b: b8 01 00 00 00              	movl	$0x1, %eax
100000d30: 87 05 52 a3 00 00           	xchgl	%eax, 0xa352(%rip)      ## 0x10000b088 <_S+0x48>
100000d36: 4c 8d 3d 4b a3 00 00        	leaq	0xa34b(%rip), %r15      ## 0x10000b088 <_S+0x48>
100000d3d: 4c 8d 25 dc a2 00 00        	leaq	0xa2dc(%rip), %r12      ## 0x10000b020 <_address_wait>
100000d44: 4c 89 e7                    	movq	%r12, %rdi
100000d47: 4c 89 fe                    	movq	%r15, %rsi
100000d4a: e8 01 3e 00 00              	callq	0x100004b50 <_mb_win_address_wait_wake_single>
100000d4f: 4c 89 e7                    	movq	%r12, %rdi
100000d52: 4c 89 fe                    	movq	%r15, %rsi
100000d55: e8 d6 3e 00 00              	callq	0x100004c30 <_mb_win_address_wait_wake_all>
100000d5a: 48 8d 3d a7 a2 00 00        	leaq	0xa2a7(%rip), %rdi      ## 0x10000b008 <_domain>
100000d61: e8 ba 13 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000d66: c7 00 ce 8a 46 c2           	movl	$0xc2468ace, (%rax)     ## imm = 0xC2468ACE
100000d6c: 49 8b 7d 00                 	movq	(%r13), %rdi
100000d70: 48 8d 15 c1 7b 00 00        	leaq	0x7bc1(%rip), %rdx      ## 0x100008938 <_sysconf+0x100008938>
100000d77: 4c 89 f6                    	movq	%r14, %rsi
100000d7a: 31 c0                       	xorl	%eax, %eax
100000d7c: e8 b3 78 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000d81: 49 8b 7d 00                 	movq	(%r13), %rdi
100000d85: e8 a4 78 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000d8a: 48 8b 35 0f a3 00 00        	movq	0xa30f(%rip), %rsi      ## 0x10000b0a0 <_thread_handle>
100000d91: 48 89 df                    	movq	%rbx, %rdi
100000d94: ba ff ff ff ff              	movl	$0xffffffff, %edx       ## imm = 0xFFFFFFFF
100000d99: e8 12 24 00 00              	callq	0x1000031b0 <_mb_win_workers_wait>
100000d9e: 49 8b 7d 00                 	movq	(%r13), %rdi
100000da2: 85 c0                       	testl	%eax, %eax
100000da4: 0f 85 d9 06 00 00           	jne	0x100001483 <_main+0xe13>
100000daa: 48 8d 35 a7 7e 00 00        	leaq	0x7ea7(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000db1: 48 8d 15 b4 7b 00 00        	leaq	0x7bb4(%rip), %rdx      ## 0x10000896c <_sysconf+0x10000896c>
100000db8: 31 c0                       	xorl	%eax, %eax
100000dba: e8 75 78 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000dbf: 49 8b 7d 00                 	movq	(%r13), %rdi
100000dc3: e8 66 78 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000dc8: 48 8b 35 d1 a2 00 00        	movq	0xa2d1(%rip), %rsi      ## 0x10000b0a0 <_thread_handle>
100000dcf: 48 8d 3d 3a a2 00 00        	leaq	0xa23a(%rip), %rdi      ## 0x10000b010 <_workers>
100000dd6: 48 8d 55 cc                 	leaq	-0x34(%rbp), %rdx
100000dda: e8 61 27 00 00              	callq	0x100003540 <_mb_win_workers_exit_code>
100000ddf: 49 8b 5d 00                 	movq	(%r13), %rbx
100000de3: 85 c0                       	testl	%eax, %eax
100000de5: 0f 84 ab 06 00 00           	je	0x100001496 <_main+0xe26>
100000deb: 44 8b 75 cc                 	movl	-0x34(%rbp), %r14d
100000def: 41 81 fe 67 45 23 81        	cmpl	$0x81234567, %r14d      ## imm = 0x81234567
100000df6: 0f 85 9a 06 00 00           	jne	0x100001496 <_main+0xe26>
100000dfc: 44 8b 3d 99 a2 00 00        	movl	0xa299(%rip), %r15d     ## 0x10000b09c <_S+0x5c>
100000e03: 48 8b 35 96 a2 00 00        	movq	0xa296(%rip), %rsi      ## 0x10000b0a0 <_thread_handle>
100000e0a: 4c 8d 25 ff a1 00 00        	leaq	0xa1ff(%rip), %r12      ## 0x10000b010 <_workers>
100000e11: 4c 89 e7                    	movq	%r12, %rdi
100000e14: 31 d2                       	xorl	%edx, %edx
100000e16: e8 95 23 00 00              	callq	0x1000031b0 <_mb_win_workers_wait>
100000e1b: 48 8d 35 7d 7b 00 00        	leaq	0x7b7d(%rip), %rsi      ## 0x10000899f <_sysconf+0x10000899f>
100000e22: 48 89 df                    	movq	%rbx, %rdi
100000e25: 44 89 f2                    	movl	%r14d, %edx
100000e28: 44 89 f9                    	movl	%r15d, %ecx
100000e2b: 41 89 c0                    	movl	%eax, %r8d
100000e2e: 31 c0                       	xorl	%eax, %eax
100000e30: e8 ff 77 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000e35: 48 8b 35 64 a2 00 00        	movq	0xa264(%rip), %rsi      ## 0x10000b0a0 <_thread_handle>
100000e3c: 4c 89 e7                    	movq	%r12, %rdi
100000e3f: e8 bc 27 00 00              	callq	0x100003600 <_mb_win_workers_close_handle>
100000e44: 49 8b 7d 00                 	movq	(%r13), %rdi
100000e48: 85 c0                       	testl	%eax, %eax
100000e4a: 0f 84 5c 06 00 00           	je	0x1000014ac <_main+0xe3c>
100000e50: 48 8d 35 01 7e 00 00        	leaq	0x7e01(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000e57: 48 8d 15 7b 7b 00 00        	leaq	0x7b7b(%rip), %rdx      ## 0x1000089d9 <_sysconf+0x1000089d9>
100000e5e: 31 c0                       	xorl	%eax, %eax
100000e60: e8 cf 77 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000e65: 49 8b 7d 00                 	movq	(%r13), %rdi
100000e69: e8 c0 77 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000e6e: 48 8d 3d a3 a1 00 00        	leaq	0xa1a3(%rip), %rdi      ## 0x10000b018 <_critical>
100000e75: 48 8d 35 c4 a1 00 00        	leaq	0xa1c4(%rip), %rsi      ## 0x10000b040 <_S>
100000e7c: e8 4f 33 00 00              	callq	0x1000041d0 <_mb_win_critical_delete>
100000e81: c7 45 bc 00 00 00 00        	movl	$0x0, -0x44(%rbp)
100000e88: 48 c7 45 c0 00 00 00 00     	movq	$0x0, -0x40(%rbp)
100000e90: 48 8d 3d 99 a1 00 00        	leaq	0xa199(%rip), %rdi      ## 0x10000b030 <_init_once>
100000e97: 48 8d 35 e2 a1 00 00        	leaq	0xa1e2(%rip), %rsi      ## 0x10000b080 <_S+0x40>
100000e9e: 48 8d 4d bc                 	leaq	-0x44(%rbp), %rcx
100000ea2: 4c 8d 45 c0                 	leaq	-0x40(%rbp), %r8
100000ea6: ba 01 00 00 00              	movl	$0x1, %edx
100000eab: e8 70 54 00 00              	callq	0x100006320 <_mb_win_init_once_begin>
100000eb0: 85 c0                       	testl	%eax, %eax
100000eb2: 0f 84 07 06 00 00           	je	0x1000014bf <_main+0xe4f>
100000eb8: 83 7d bc 00                 	cmpl	$0x0, -0x44(%rbp)
100000ebc: 0f 85 fd 05 00 00           	jne	0x1000014bf <_main+0xe4f>
100000ec2: 48 b8 02 00 5a 5a ef be d2 51       	movabsq	$0x51d2beef5a5a0002, %rax ## imm = 0x51D2BEEF5A5A0002
100000ecc: 48 39 45 c0                 	cmpq	%rax, -0x40(%rbp)
100000ed0: 0f 85 e9 05 00 00           	jne	0x1000014bf <_main+0xe4f>
100000ed6: 49 8b 7d 00                 	movq	(%r13), %rdi
100000eda: 48 8d 35 77 7d 00 00        	leaq	0x7d77(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000ee1: 48 8d 15 1f 7b 00 00        	leaq	0x7b1f(%rip), %rdx      ## 0x100008a07 <_sysconf+0x100008a07>
100000ee8: 31 c0                       	xorl	%eax, %eax
100000eea: e8 45 77 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000eef: 49 8b 7d 00                 	movq	(%r13), %rdi
100000ef3: e8 36 77 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000ef8: 48 8d 3d 39 a1 00 00        	leaq	0xa139(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000eff: 48 8d 35 ca 81 00 00        	leaq	0x81ca(%rip), %rsi      ## 0x1000090d0 <_abandon_folded>
100000f06: e8 35 68 00 00              	callq	0x100007740 <_mb_win_mutex_open_named>
100000f0b: 48 85 c0                    	testq	%rax, %rax
100000f0e: 0f 84 c2 05 00 00           	je	0x1000014d6 <_main+0xe66>
100000f14: 48 89 c3                    	movq	%rax, %rbx
100000f17: 48 8d 3d 1a a1 00 00        	leaq	0xa11a(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000f1e: 48 89 c6                    	movq	%rax, %rsi
100000f21: ba 88 13 00 00              	movl	$0x1388, %edx           ## imm = 0x1388
100000f26: 31 c9                       	xorl	%ecx, %ecx
100000f28: e8 f3 6b 00 00              	callq	0x100007b20 <_mb_win_mutex_wait_ex>
100000f2d: 41 89 c6                    	movl	%eax, %r14d
100000f30: 49 8b 7d 00                 	movq	(%r13), %rdi
100000f34: 48 8d 35 17 7b 00 00        	leaq	0x7b17(%rip), %rsi      ## 0x100008a52 <_sysconf+0x100008a52>
100000f3b: 89 c2                       	movl	%eax, %edx
100000f3d: 31 c0                       	xorl	%eax, %eax
100000f3f: e8 f0 76 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000f44: 41 81 fe 80 00 00 00        	cmpl	$0x80, %r14d
100000f4b: 0f 85 9c 05 00 00           	jne	0x1000014ed <_main+0xe7d>
100000f51: 48 8d 3d e0 a0 00 00        	leaq	0xa0e0(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000f58: 48 89 de                    	movq	%rbx, %rsi
100000f5b: e8 90 69 00 00              	callq	0x1000078f0 <_mb_win_mutex_release>
100000f60: 83 f8 01                    	cmpl	$0x1, %eax
100000f63: 0f 85 9b 05 00 00           	jne	0x100001504 <_main+0xe94>
100000f69: 48 8d 3d c8 a0 00 00        	leaq	0xa0c8(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000f70: 48 89 de                    	movq	%rbx, %rsi
100000f73: 31 d2                       	xorl	%edx, %edx
100000f75: 31 c9                       	xorl	%ecx, %ecx
100000f77: e8 a4 6b 00 00              	callq	0x100007b20 <_mb_win_mutex_wait_ex>
100000f7c: 85 c0                       	testl	%eax, %eax
100000f7e: 0f 85 97 05 00 00           	jne	0x10000151b <_main+0xeab>
100000f84: 48 8d 3d ad a0 00 00        	leaq	0xa0ad(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
100000f8b: 48 89 de                    	movq	%rbx, %rsi
100000f8e: e8 5d 69 00 00              	callq	0x1000078f0 <_mb_win_mutex_release>
100000f93: 49 8b 7d 00                 	movq	(%r13), %rdi
100000f97: 83 f8 01                    	cmpl	$0x1, %eax
100000f9a: 0f 85 92 05 00 00           	jne	0x100001532 <_main+0xec2>
100000fa0: 48 8d 35 b1 7c 00 00        	leaq	0x7cb1(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000fa7: 48 8d 15 04 7b 00 00        	leaq	0x7b04(%rip), %rdx      ## 0x100008ab2 <_sysconf+0x100008ab2>
100000fae: 31 c0                       	xorl	%eax, %eax
100000fb0: e8 7f 76 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100000fb5: 49 8b 7d 00                 	movq	(%r13), %rdi
100000fb9: e8 70 76 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100000fbe: 48 8d 3d 4b a0 00 00        	leaq	0xa04b(%rip), %rdi      ## 0x10000b010 <_workers>
100000fc5: e8 b6 1a 00 00              	callq	0x100002a80 <_mb_win_workers_leave_call>
100000fca: 85 c0                       	testl	%eax, %eax
100000fcc: 0f 85 73 05 00 00           	jne	0x100001545 <_main+0xed5>
100000fd2: 48 8d 3d 37 a0 00 00        	leaq	0xa037(%rip), %rdi      ## 0x10000b010 <_workers>
100000fd9: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100000fdd: e8 be 1b 00 00              	callq	0x100002ba0 <_mb_win_workers_collect>
100000fe2: 49 8b 7d 00                 	movq	(%r13), %rdi
100000fe6: 85 c0                       	testl	%eax, %eax
100000fe8: 0f 85 6e 05 00 00           	jne	0x10000155c <_main+0xeec>
100000fee: 48 8d 35 63 7c 00 00        	leaq	0x7c63(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
100000ff5: 48 8d 15 fd 7a 00 00        	leaq	0x7afd(%rip), %rdx      ## 0x100008af9 <_sysconf+0x100008af9>
100000ffc: 31 c0                       	xorl	%eax, %eax
100000ffe: e8 31 76 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100001003: 49 8b 7d 00                 	movq	(%r13), %rdi
100001007: e8 22 76 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
10000100c: 48 8d 3d fd 9f 00 00        	leaq	0x9ffd(%rip), %rdi      ## 0x10000b010 <_workers>
100001013: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100001017: e8 a4 16 00 00              	callq	0x1000026c0 <_mb_win_workers_close>
10000101c: 85 c0                       	testl	%eax, %eax
10000101e: 0f 85 4b 05 00 00           	jne	0x10000156f <_main+0xeff>
100001024: 48 8d 3d fd 9f 00 00        	leaq	0x9ffd(%rip), %rdi      ## 0x10000b028 <_srw>
10000102b: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
10000102f: e8 1c 3d 00 00              	callq	0x100004d50 <_mb_win_srw_close>
100001034: 85 c0                       	testl	%eax, %eax
100001036: 0f 85 47 05 00 00           	jne	0x100001583 <_main+0xf13>
10000103c: 48 8d 3d dd 9f 00 00        	leaq	0x9fdd(%rip), %rdi      ## 0x10000b020 <_address_wait>
100001043: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100001047: e8 c4 33 00 00              	callq	0x100004410 <_mb_win_address_wait_close>
10000104c: 85 c0                       	testl	%eax, %eax
10000104e: 0f 85 43 05 00 00           	jne	0x100001597 <_main+0xf27>
100001054: 48 8d 3d bd 9f 00 00        	leaq	0x9fbd(%rip), %rdi      ## 0x10000b018 <_critical>
10000105b: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
10000105f: e8 ec 28 00 00              	callq	0x100003950 <_mb_win_critical_close>
100001064: 85 c0                       	testl	%eax, %eax
100001066: 0f 85 3f 05 00 00           	jne	0x1000015ab <_main+0xf3b>
10000106c: 48 8d 3d bd 9f 00 00        	leaq	0x9fbd(%rip), %rdi      ## 0x10000b030 <_init_once>
100001073: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100001077: e8 e4 4f 00 00              	callq	0x100006060 <_mb_win_init_once_close>
10000107c: 85 c0                       	testl	%eax, %eax
10000107e: 0f 85 3b 05 00 00           	jne	0x1000015bf <_main+0xf4f>
100001084: 48 8d 3d ad 9f 00 00        	leaq	0x9fad(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
10000108b: 4c 8d 75 d4                 	leaq	-0x2c(%rbp), %r14
10000108f: 4c 89 f6                    	movq	%r14, %rsi
100001092: e8 39 5d 00 00              	callq	0x100006dd0 <_mb_win_mutex_close>
100001097: 89 c3                       	movl	%eax, %ebx
100001099: 49 8b 7d 00                 	movq	(%r13), %rdi
10000109d: 85 c0                       	testl	%eax, %eax
10000109f: 48 8d 05 0e 7b 00 00        	leaq	0x7b0e(%rip), %rax      ## 0x100008bb4 <_sysconf+0x100008bb4>
1000010a6: 48 8d 0d 02 7b 00 00        	leaq	0x7b02(%rip), %rcx      ## 0x100008baf <_sysconf+0x100008baf>
1000010ad: 48 0f 44 c8                 	cmoveq	%rax, %rcx
1000010b1: 48 8d 05 0b 7b 00 00        	leaq	0x7b0b(%rip), %rax      ## 0x100008bc3 <_sysconf+0x100008bc3>
1000010b8: 4c 8d 05 f8 7a 00 00        	leaq	0x7af8(%rip), %r8       ## 0x100008bb7 <_sysconf+0x100008bb7>
1000010bf: 4c 0f 44 c0                 	cmoveq	%rax, %r8
1000010c3: 48 8d 35 9d 7a 00 00        	leaq	0x7a9d(%rip), %rsi      ## 0x100008b67 <_sysconf+0x100008b67>
1000010ca: 89 da                       	movl	%ebx, %edx
1000010cc: 31 c0                       	xorl	%eax, %eax
1000010ce: e8 61 75 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000010d3: 49 8b 7d 00                 	movq	(%r13), %rdi
1000010d7: 48 8d 35 7a 7b 00 00        	leaq	0x7b7a(%rip), %rsi      ## 0x100008c58 <_sysconf+0x100008c58>
1000010de: 48 8d 15 e6 7a 00 00        	leaq	0x7ae6(%rip), %rdx      ## 0x100008bcb <_sysconf+0x100008bcb>
1000010e5: 31 c0                       	xorl	%eax, %eax
1000010e7: e8 48 75 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000010ec: 49 8b 7d 00                 	movq	(%r13), %rdi
1000010f0: e8 39 75 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000010f5: 48 8d 3d 0c 9f 00 00        	leaq	0x9f0c(%rip), %rdi      ## 0x10000b008 <_domain>
1000010fc: 4c 89 f6                    	movq	%r14, %rsi
1000010ff: e8 bc 0b 00 00              	callq	0x100001cc0 <_mb_win_threads_leave>
100001104: 85 c0                       	testl	%eax, %eax
100001106: 0f 85 c7 04 00 00           	jne	0x1000015d3 <_main+0xf63>
10000110c: 48 8d 3d f5 9e 00 00        	leaq	0x9ef5(%rip), %rdi      ## 0x10000b008 <_domain>
100001113: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100001117: e8 c4 0c 00 00              	callq	0x100001de0 <_mb_win_threads_close>
10000111c: 49 8b 7d 00                 	movq	(%r13), %rdi
100001120: 85 c0                       	testl	%eax, %eax
100001122: 0f 85 bf 04 00 00           	jne	0x1000015e7 <_main+0xf77>
100001128: 85 db                       	testl	%ebx, %ebx
10000112a: 48 8d 05 05 7b 00 00        	leaq	0x7b05(%rip), %rax      ## 0x100008c36 <_sysconf+0x100008c36>
100001131: 48 8d 15 e3 7a 00 00        	leaq	0x7ae3(%rip), %rdx      ## 0x100008c1b <_sysconf+0x100008c1b>
100001138: 48 0f 44 d0                 	cmoveq	%rax, %rdx
10000113c: 48 8d 35 b8 7a 00 00        	leaq	0x7ab8(%rip), %rsi      ## 0x100008bfb <_sysconf+0x100008bfb>
100001143: 31 c0                       	xorl	%eax, %eax
100001145: e8 ea 74 00 00              	callq	0x100008634 <_sysconf+0x100008634>
10000114a: 31 c0                       	xorl	%eax, %eax
10000114c: 48 81 c4 98 00 00 00        	addq	$0x98, %rsp
100001153: 5b                          	popq	%rbx
100001154: 41 5c                       	popq	%r12
100001156: 41 5d                       	popq	%r13
100001158: 41 5e                       	popq	%r14
10000115a: 41 5f                       	popq	%r15
10000115c: 5d                          	popq	%rbp
10000115d: c3                          	retq
10000115e: 31 db                       	xorl	%ebx, %ebx
100001160: 4c 8d 35 c1 9e 00 00        	leaq	0x9ec1(%rip), %r14      ## 0x10000b028 <_srw>
100001167: 4c 8d 3d 02 9f 00 00        	leaq	0x9f02(%rip), %r15      ## 0x10000b070 <_S+0x30>
10000116e: 4c 8d 25 f3 9e 00 00        	leaq	0x9ef3(%rip), %r12      ## 0x10000b068 <_S+0x28>
100001175: 4c 8d 2d 94 9e 00 00        	leaq	0x9e94(%rip), %r13      ## 0x10000b010 <_workers>
10000117c: eb 10                       	jmp	0x10000118e <_main+0xb1e>
10000117e: 66 90                       	nop
100001180: 8b 05 06 9f 00 00           	movl	0x9f06(%rip), %eax      ## 0x10000b08c <_S+0x4c>
100001186: 85 c0                       	testl	%eax, %eax
100001188: 0f 85 ce fa ff ff           	jne	0x100000c5c <_main+0x5ec>
10000118e: 4c 89 f7                    	movq	%r14, %rdi
100001191: 4c 89 fe                    	movq	%r15, %rsi
100001194: 4c 89 e2                    	movq	%r12, %rdx
100001197: b9 64 00 00 00              	movl	$0x64, %ecx
10000119c: 45 31 c0                    	xorl	%r8d, %r8d
10000119f: e8 7c 45 00 00              	callq	0x100005720 <_mb_win_srw_sleep>
1000011a4: 8b 05 e2 9e 00 00           	movl	0x9ee2(%rip), %eax      ## 0x10000b08c <_S+0x4c>
1000011aa: 85 c0                       	testl	%eax, %eax
1000011ac: 75 d2                       	jne	0x100001180 <_main+0xb10>
1000011ae: 48 8b 35 eb 9e 00 00        	movq	0x9eeb(%rip), %rsi      ## 0x10000b0a0 <_thread_handle>
1000011b5: 4c 89 ef                    	movq	%r13, %rdi
1000011b8: 31 d2                       	xorl	%edx, %edx
1000011ba: e8 f1 1f 00 00              	callq	0x1000031b0 <_mb_win_workers_wait>
1000011bf: 3d 02 01 00 00              	cmpl	$0x102, %eax            ## imm = 0x102
1000011c4: 75 07                       	jne	0x1000011cd <_main+0xb5d>
1000011c6: ff c3                       	incl	%ebx
1000011c8: 83 fb 14                    	cmpl	$0x14, %ebx
1000011cb: 75 b3                       	jne	0x100001180 <_main+0xb10>
1000011cd: 48 8d 3d 54 9e 00 00        	leaq	0x9e54(%rip), %rdi      ## 0x10000b028 <_srw>
1000011d4: 48 8d 35 8d 9e 00 00        	leaq	0x9e8d(%rip), %rsi      ## 0x10000b068 <_S+0x28>
1000011db: ba 01 00 00 00              	movl	$0x1, %edx
1000011e0: e8 bb 43 00 00              	callq	0x1000055a0 <_mb_win_srw_release>
1000011e5: 48 8b 05 2c 8e 00 00        	movq	0x8e2c(%rip), %rax      ## 0x10000a018 <_sysconf+0x10000a018>
1000011ec: 48 8b 38                    	movq	(%rax), %rdi
1000011ef: 48 8d 35 6d 7a 00 00        	leaq	0x7a6d(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000011f6: 48 8d 15 d1 76 00 00        	leaq	0x76d1(%rip), %rdx      ## 0x1000088ce <_sysconf+0x1000088ce>
1000011fd: e9 f3 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001202: 49 8b 3f                    	movq	(%r15), %rdi
100001205: 48 8d 35 57 7a 00 00        	leaq	0x7a57(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000120c: 48 8d 15 2a 76 00 00        	leaq	0x762a(%rip), %rdx      ## 0x10000883d <_sysconf+0x10000883d>
100001213: e9 dd 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001218: 49 8b 3f                    	movq	(%r15), %rdi
10000121b: 48 8d 35 41 7a 00 00        	leaq	0x7a41(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001222: 48 8d 15 8b 7a 00 00        	leaq	0x7a8b(%rip), %rdx      ## 0x100008cb4 <_sysconf+0x100008cb4>
100001229: e9 c7 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000122e: 49 8b 3f                    	movq	(%r15), %rdi
100001231: 48 8d 35 2b 7a 00 00        	leaq	0x7a2b(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001238: 48 8d 15 a9 7a 00 00        	leaq	0x7aa9(%rip), %rdx      ## 0x100008ce8 <_sysconf+0x100008ce8>
10000123f: e9 b1 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001244: 49 8b 3f                    	movq	(%r15), %rdi
100001247: 48 8d 35 15 7a 00 00        	leaq	0x7a15(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000124e: 48 8d 15 aa 7a 00 00        	leaq	0x7aaa(%rip), %rdx      ## 0x100008cff <_sysconf+0x100008cff>
100001255: e9 9b 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000125a: 49 8b 3f                    	movq	(%r15), %rdi
10000125d: 48 8d 35 ff 79 00 00        	leaq	0x79ff(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001264: 48 8d 15 b8 7a 00 00        	leaq	0x7ab8(%rip), %rdx      ## 0x100008d23 <_sysconf+0x100008d23>
10000126b: e9 85 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001270: 49 8b 3f                    	movq	(%r15), %rdi
100001273: 48 8d 35 e9 79 00 00        	leaq	0x79e9(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000127a: 48 8d 15 7a 74 00 00        	leaq	0x747a(%rip), %rdx      ## 0x1000086fb <_sysconf+0x1000086fb>
100001281: e9 6f 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001286: 48 8d 35 d6 79 00 00        	leaq	0x79d6(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000128d: 48 8d 15 74 74 00 00        	leaq	0x7474(%rip), %rdx      ## 0x100008708 <_sysconf+0x100008708>
100001294: e9 5c 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001299: 49 8b 3f                    	movq	(%r15), %rdi
10000129c: 48 8d 35 c0 79 00 00        	leaq	0x79c0(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000012a3: 48 8d 15 b0 74 00 00        	leaq	0x74b0(%rip), %rdx      ## 0x10000875a <_sysconf+0x10000875a>
1000012aa: e9 46 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000012af: 49 8b 3f                    	movq	(%r15), %rdi
1000012b2: 48 8d 35 aa 79 00 00        	leaq	0x79aa(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000012b9: 48 8d 15 a7 74 00 00        	leaq	0x74a7(%rip), %rdx      ## 0x100008767 <_sysconf+0x100008767>
1000012c0: e9 30 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000012c5: 49 8b 3f                    	movq	(%r15), %rdi
1000012c8: 48 8d 35 94 79 00 00        	leaq	0x7994(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000012cf: 48 8d 15 9f 74 00 00        	leaq	0x749f(%rip), %rdx      ## 0x100008775 <_sysconf+0x100008775>
1000012d6: e9 1a 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000012db: 49 8b 3f                    	movq	(%r15), %rdi
1000012de: 48 8d 35 7e 79 00 00        	leaq	0x797e(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000012e5: 48 8d 15 9b 74 00 00        	leaq	0x749b(%rip), %rdx      ## 0x100008787 <_sysconf+0x100008787>
1000012ec: e9 04 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000012f1: 49 8b 3f                    	movq	(%r15), %rdi
1000012f4: 48 8d 35 68 79 00 00        	leaq	0x7968(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000012fb: 48 8d 15 8e 74 00 00        	leaq	0x748e(%rip), %rdx      ## 0x100008790 <_sysconf+0x100008790>
100001302: e9 ee 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001307: 48 8d 35 55 79 00 00        	leaq	0x7955(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000130e: 48 8d 15 8a 74 00 00        	leaq	0x748a(%rip), %rdx      ## 0x10000879f <_sysconf+0x10000879f>
100001315: e9 db 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000131a: 49 8b 3f                    	movq	(%r15), %rdi
10000131d: 48 8d 35 3f 79 00 00        	leaq	0x793f(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001324: 48 8d 15 ab 74 00 00        	leaq	0x74ab(%rip), %rdx      ## 0x1000087d6 <_sysconf+0x1000087d6>
10000132b: e9 c5 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001330: 49 8b 3f                    	movq	(%r15), %rdi
100001333: 48 8d 35 29 79 00 00        	leaq	0x7929(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000133a: 48 8d 15 a9 74 00 00        	leaq	0x74a9(%rip), %rdx      ## 0x1000087ea <_sysconf+0x1000087ea>
100001341: e9 af 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001346: 49 8b 3f                    	movq	(%r15), %rdi
100001349: 48 8d 35 13 79 00 00        	leaq	0x7913(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001350: 48 8d 15 b3 74 00 00        	leaq	0x74b3(%rip), %rdx      ## 0x10000880a <_sysconf+0x10000880a>
100001357: e9 99 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000135c: 49 8b 3f                    	movq	(%r15), %rdi
10000135f: 48 8d 35 fd 78 00 00        	leaq	0x78fd(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001366: 48 8d 15 a8 74 00 00        	leaq	0x74a8(%rip), %rdx      ## 0x100008815 <_sysconf+0x100008815>
10000136d: e9 83 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001372: 49 8b 3f                    	movq	(%r15), %rdi
100001375: 48 8d 35 e7 78 00 00        	leaq	0x78e7(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000137c: 48 8d 15 a0 74 00 00        	leaq	0x74a0(%rip), %rdx      ## 0x100008823 <_sysconf+0x100008823>
100001383: e9 6d 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001388: 49 8b 3f                    	movq	(%r15), %rdi
10000138b: 48 8d 35 d1 78 00 00        	leaq	0x78d1(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001392: 48 8d 15 95 74 00 00        	leaq	0x7495(%rip), %rdx      ## 0x10000882e <_sysconf+0x10000882e>
100001399: e9 57 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000139e: 49 8b 3f                    	movq	(%r15), %rdi
1000013a1: 48 8d 35 bb 78 00 00        	leaq	0x78bb(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000013a8: 48 8d 15 c7 78 00 00        	leaq	0x78c7(%rip), %rdx      ## 0x100008c76 <_sysconf+0x100008c76>
1000013af: e9 41 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000013b4: 49 8b 3f                    	movq	(%r15), %rdi
1000013b7: 48 8d 35 a5 78 00 00        	leaq	0x78a5(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000013be: 48 8d 15 c8 78 00 00        	leaq	0x78c8(%rip), %rdx      ## 0x100008c8d <_sysconf+0x100008c8d>
1000013c5: e9 2b 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000013ca: 49 8b 3f                    	movq	(%r15), %rdi
1000013cd: 48 8d 35 8f 78 00 00        	leaq	0x788f(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000013d4: 48 8d 15 c9 78 00 00        	leaq	0x78c9(%rip), %rdx      ## 0x100008ca4 <_sysconf+0x100008ca4>
1000013db: e9 15 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000013e0: 49 8b 3f                    	movq	(%r15), %rdi
1000013e3: 48 8d 35 79 78 00 00        	leaq	0x7879(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000013ea: 48 8d 15 d5 78 00 00        	leaq	0x78d5(%rip), %rdx      ## 0x100008cc6 <_sysconf+0x100008cc6>
1000013f1: e9 ff 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000013f6: 49 8b 3f                    	movq	(%r15), %rdi
1000013f9: 48 8d 35 63 78 00 00        	leaq	0x7863(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001400: 48 8d 15 cd 78 00 00        	leaq	0x78cd(%rip), %rdx      ## 0x100008cd4 <_sysconf+0x100008cd4>
100001407: e9 e9 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000140c: 49 8b 3f                    	movq	(%r15), %rdi
10000140f: 48 8d 35 4d 78 00 00        	leaq	0x784d(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001416: 48 8d 15 f4 78 00 00        	leaq	0x78f4(%rip), %rdx      ## 0x100008d11 <_sysconf+0x100008d11>
10000141d: e9 d3 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001422: 49 8b 3f                    	movq	(%r15), %rdi
100001425: 48 8d 35 37 78 00 00        	leaq	0x7837(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000142c: 48 8d 15 82 74 00 00        	leaq	0x7482(%rip), %rdx      ## 0x1000088b5 <_sysconf+0x1000088b5>
100001433: e9 bd 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001438: 48 8b 05 d9 8b 00 00        	movq	0x8bd9(%rip), %rax      ## 0x10000a018 <_sysconf+0x10000a018>
10000143f: 48 8b 38                    	movq	(%rax), %rdi
100001442: 48 8d 35 1a 78 00 00        	leaq	0x781a(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001449: 48 8d 15 70 74 00 00        	leaq	0x7470(%rip), %rdx      ## 0x1000088c0 <_sysconf+0x1000088c0>
100001450: e9 a0 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001455: 49 8b 7d 00                 	movq	(%r13), %rdi
100001459: 48 8d 35 03 78 00 00        	leaq	0x7803(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001460: 48 8d 15 a2 74 00 00        	leaq	0x74a2(%rip), %rdx      ## 0x100008909 <_sysconf+0x100008909>
100001467: e9 89 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000146c: 49 8b 7d 00                 	movq	(%r13), %rdi
100001470: 48 8d 35 ec 77 00 00        	leaq	0x77ec(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001477: 48 8d 15 95 74 00 00        	leaq	0x7495(%rip), %rdx      ## 0x100008913 <_sysconf+0x100008913>
10000147e: e9 72 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001483: 48 8d 35 d9 77 00 00        	leaq	0x77d9(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000148a: 48 8d 15 cd 74 00 00        	leaq	0x74cd(%rip), %rdx      ## 0x10000895e <_sysconf+0x10000895e>
100001491: e9 5f 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001496: 48 8d 35 c6 77 00 00        	leaq	0x77c6(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000149d: 48 8d 15 f1 74 00 00        	leaq	0x74f1(%rip), %rdx      ## 0x100008995 <_sysconf+0x100008995>
1000014a4: 48 89 df                    	movq	%rbx, %rdi
1000014a7: e9 49 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000014ac: 48 8d 35 b0 77 00 00        	leaq	0x77b0(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000014b3: 48 8d 15 12 75 00 00        	leaq	0x7512(%rip), %rdx      ## 0x1000089cc <_sysconf+0x1000089cc>
1000014ba: e9 36 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000014bf: 49 8b 7d 00                 	movq	(%r13), %rdi
1000014c3: 48 8d 35 99 77 00 00        	leaq	0x7799(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000014ca: 48 8d 15 26 75 00 00        	leaq	0x7526(%rip), %rdx      ## 0x1000089f7 <_sysconf+0x1000089f7>
1000014d1: e9 1f 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000014d6: 49 8b 7d 00                 	movq	(%r13), %rdi
1000014da: 48 8d 35 82 77 00 00        	leaq	0x7782(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000014e1: 48 8d 15 5d 75 00 00        	leaq	0x755d(%rip), %rdx      ## 0x100008a45 <_sysconf+0x100008a45>
1000014e8: e9 08 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000014ed: 49 8b 7d 00                 	movq	(%r13), %rdi
1000014f1: 48 8d 35 6b 77 00 00        	leaq	0x776b(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000014f8: 48 8d 15 7c 75 00 00        	leaq	0x757c(%rip), %rdx      ## 0x100008a7b <_sysconf+0x100008a7b>
1000014ff: e9 f1 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001504: 49 8b 7d 00                 	movq	(%r13), %rdi
100001508: 48 8d 35 54 77 00 00        	leaq	0x7754(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000150f: 48 8d 15 6f 75 00 00        	leaq	0x756f(%rip), %rdx      ## 0x100008a85 <_sysconf+0x100008a85>
100001516: e9 da 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000151b: 49 8b 7d 00                 	movq	(%r13), %rdi
10000151f: 48 8d 35 3d 77 00 00        	leaq	0x773d(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001526: 48 8d 15 69 75 00 00        	leaq	0x7569(%rip), %rdx      ## 0x100008a96 <_sysconf+0x100008a96>
10000152d: e9 c3 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001532: 48 8d 35 2a 77 00 00        	leaq	0x772a(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001539: 48 8d 15 60 75 00 00        	leaq	0x7560(%rip), %rdx      ## 0x100008aa0 <_sysconf+0x100008aa0>
100001540: e9 b0 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001545: 49 8b 7d 00                 	movq	(%r13), %rdi
100001549: 48 8d 35 13 77 00 00        	leaq	0x7713(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001550: 48 8d 15 87 75 00 00        	leaq	0x7587(%rip), %rdx      ## 0x100008ade <_sysconf+0x100008ade>
100001557: e9 99 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000155c: 48 8d 35 00 77 00 00        	leaq	0x7700(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
100001563: 48 8d 15 7f 75 00 00        	leaq	0x757f(%rip), %rdx      ## 0x100008ae9 <_sysconf+0x100008ae9>
10000156a: e9 86 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000156f: 49 8b 7d 00                 	movq	(%r13), %rdi
100001573: 48 8d 35 e9 76 00 00        	leaq	0x76e9(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000157a: 48 8d 15 9c 75 00 00        	leaq	0x759c(%rip), %rdx      ## 0x100008b1d <_sysconf+0x100008b1d>
100001581: eb 72                       	jmp	0x1000015f5 <_main+0xf85>
100001583: 49 8b 7d 00                 	movq	(%r13), %rdi
100001587: 48 8d 35 d5 76 00 00        	leaq	0x76d5(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
10000158e: 48 8d 15 96 75 00 00        	leaq	0x7596(%rip), %rdx      ## 0x100008b2b <_sysconf+0x100008b2b>
100001595: eb 5e                       	jmp	0x1000015f5 <_main+0xf85>
100001597: 49 8b 7d 00                 	movq	(%r13), %rdi
10000159b: 48 8d 35 c1 76 00 00        	leaq	0x76c1(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000015a2: 48 8d 15 8c 75 00 00        	leaq	0x758c(%rip), %rdx      ## 0x100008b35 <_sysconf+0x100008b35>
1000015a9: eb 4a                       	jmp	0x1000015f5 <_main+0xf85>
1000015ab: 49 8b 7d 00                 	movq	(%r13), %rdi
1000015af: 48 8d 35 ad 76 00 00        	leaq	0x76ad(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000015b6: 48 8d 15 8b 75 00 00        	leaq	0x758b(%rip), %rdx      ## 0x100008b48 <_sysconf+0x100008b48>
1000015bd: eb 36                       	jmp	0x1000015f5 <_main+0xf85>
1000015bf: 49 8b 7d 00                 	movq	(%r13), %rdi
1000015c3: 48 8d 35 99 76 00 00        	leaq	0x7699(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000015ca: 48 8d 15 86 75 00 00        	leaq	0x7586(%rip), %rdx      ## 0x100008b57 <_sysconf+0x100008b57>
1000015d1: eb 22                       	jmp	0x1000015f5 <_main+0xf85>
1000015d3: 49 8b 7d 00                 	movq	(%r13), %rdi
1000015d7: 48 8d 35 85 76 00 00        	leaq	0x7685(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000015de: 48 8d 15 fa 75 00 00        	leaq	0x75fa(%rip), %rdx      ## 0x100008bdf <_sysconf+0x100008bdf>
1000015e5: eb 0e                       	jmp	0x1000015f5 <_main+0xf85>
1000015e7: 48 8d 35 75 76 00 00        	leaq	0x7675(%rip), %rsi      ## 0x100008c63 <_sysconf+0x100008c63>
1000015ee: 48 8d 15 f8 75 00 00        	leaq	0x75f8(%rip), %rdx      ## 0x100008bed <_sysconf+0x100008bed>
1000015f5: 31 c0                       	xorl	%eax, %eax
1000015f7: e8 38 70 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000015fc: bf 02 00 00 00              	movl	$0x2, %edi
100001601: e8 22 70 00 00              	callq	0x100008628 <_sysconf+0x100008628>
100001606: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)

0000000100001610 <_on_alarm>:
100001610: 55                          	pushq	%rbp
100001611: 48 89 e5                    	movq	%rsp, %rbp
100001614: 48 8b 05 fd 89 00 00        	movq	0x89fd(%rip), %rax      ## 0x10000a018 <_sysconf+0x10000a018>
10000161b: 48 8b 08                    	movq	(%rax), %rcx
10000161e: 48 8d 3d 12 76 00 00        	leaq	0x7612(%rip), %rdi      ## 0x100008c37 <_sysconf+0x100008c37>
100001625: be 20 00 00 00              	movl	$0x20, %esi
10000162a: ba 01 00 00 00              	movl	$0x1, %edx
10000162f: e8 0c 70 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100001634: bf 63 00 00 00              	movl	$0x63, %edi
100001639: e8 cc 6f 00 00              	callq	0x10000860a <_sysconf+0x10000860a>
10000163e: 66 90                       	nop

0000000100001640 <_code_acquire>:
100001640: 55                          	pushq	%rbp
100001641: 48 89 e5                    	movq	%rsp, %rbp
100001644: 53                          	pushq	%rbx
100001645: 50                          	pushq	%rax
100001646: 48 89 d3                    	movq	%rdx, %rbx
100001649: bf 08 00 00 00              	movl	$0x8, %edi
10000164e: e8 f3 6f 00 00              	callq	0x100008646 <_sysconf+0x100008646>
100001653: 48 89 03                    	movq	%rax, (%rbx)
100001656: 31 c9                       	xorl	%ecx, %ecx
100001658: 48 83 f8 01                 	cmpq	$0x1, %rax
10000165c: 19 c9                       	sbbl	%ecx, %ecx
10000165e: 89 c8                       	movl	%ecx, %eax
100001660: 48 83 c4 08                 	addq	$0x8, %rsp
100001664: 5b                          	popq	%rbx
100001665: 5d                          	popq	%rbp
100001666: c3                          	retq
100001667: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)

0000000100001670 <_code_release>:
100001670: 55                          	pushq	%rbp
100001671: 48 89 e5                    	movq	%rsp, %rbp
100001674: 48 89 f7                    	movq	%rsi, %rdi
100001677: e8 be 6f 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
10000167c: 31 c0                       	xorl	%eax, %eax
10000167e: 5d                          	popq	%rbp
10000167f: c3                          	retq

0000000100001680 <_worker_start>:
100001680: 55                          	pushq	%rbp
100001681: 48 89 e5                    	movq	%rsp, %rbp
100001684: 41 57                       	pushq	%r15
100001686: 41 56                       	pushq	%r14
100001688: 41 55                       	pushq	%r13
10000168a: 41 54                       	pushq	%r12
10000168c: 56                          	pushq	%rsi
10000168d: 57                          	pushq	%rdi
10000168e: 53                          	pushq	%rbx
10000168f: 48 81 ec c8 00 00 00        	subq	$0xc8, %rsp
100001696: 44 0f 29 7d b0              	movaps	%xmm15, -0x50(%rbp)
10000169b: 44 0f 29 75 a0              	movaps	%xmm14, -0x60(%rbp)
1000016a0: 44 0f 29 6d 90              	movaps	%xmm13, -0x70(%rbp)
1000016a5: 44 0f 29 65 80              	movaps	%xmm12, -0x80(%rbp)
1000016aa: 44 0f 29 9d 70 ff ff ff     	movaps	%xmm11, -0x90(%rbp)
1000016b2: 44 0f 29 95 60 ff ff ff     	movaps	%xmm10, -0xa0(%rbp)
1000016ba: 44 0f 29 8d 50 ff ff ff     	movaps	%xmm9, -0xb0(%rbp)
1000016c2: 44 0f 29 85 40 ff ff ff     	movaps	%xmm8, -0xc0(%rbp)
1000016ca: 0f 29 bd 30 ff ff ff        	movaps	%xmm7, -0xd0(%rbp)
1000016d1: 0f 29 b5 20 ff ff ff        	movaps	%xmm6, -0xe0(%rbp)
1000016d8: 48 89 cb                    	movq	%rcx, %rbx
1000016db: 4c 8b 2d 36 89 00 00        	movq	0x8936(%rip), %r13      ## 0x10000a018 <_sysconf+0x10000a018>
1000016e2: 49 8b 4d 00                 	movq	(%r13), %rcx
1000016e6: 48 8d 3d 48 76 00 00        	leaq	0x7648(%rip), %rdi      ## 0x100008d35 <_sysconf+0x100008d35>
1000016ed: be 0f 00 00 00              	movl	$0xf, %esi
1000016f2: ba 01 00 00 00              	movl	$0x1, %edx
1000016f7: e8 44 6f 00 00              	callq	0x100008640 <_sysconf+0x100008640>
1000016fc: 48 8d 3d 05 99 00 00        	leaq	0x9905(%rip), %rdi      ## 0x10000b008 <_domain>
100001703: e8 18 0a 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100001708: c7 00 ab 95 73 e1           	movl	$0xe17395ab, (%rax)     ## imm = 0xE17395AB
10000170e: c7 85 14 ff ff ff 00 00 00 00       	movl	$0x0, -0xec(%rbp)
100001718: 48 c7 85 00 ff ff ff 00 00 00 00    	movq	$0x0, -0x100(%rbp)
100001723: 4c 8d 73 40                 	leaq	0x40(%rbx), %r14
100001727: 48 8d 3d 02 99 00 00        	leaq	0x9902(%rip), %rdi      ## 0x10000b030 <_init_once>
10000172e: 48 8d 8d 14 ff ff ff        	leaq	-0xec(%rbp), %rcx
100001735: 4c 8d 85 00 ff ff ff        	leaq	-0x100(%rbp), %r8
10000173c: 4c 89 f6                    	movq	%r14, %rsi
10000173f: 31 d2                       	xorl	%edx, %edx
100001741: e8 da 4b 00 00              	callq	0x100006320 <_mb_win_init_once_begin>
100001746: 85 c0                       	testl	%eax, %eax
100001748: 74 09                       	je	0x100001753 <_worker_start+0xd3>
10000174a: 83 bd 14 ff ff ff 01        	cmpl	$0x1, -0xec(%rbp)
100001751: 74 08                       	je	0x10000175b <_worker_start+0xdb>
100001753: b8 01 00 00 00              	movl	$0x1, %eax
100001758: 87 43 58                    	xchgl	%eax, 0x58(%rbx)
10000175b: 49 8b 4d 00                 	movq	(%r13), %rcx
10000175f: 48 8d 3d df 75 00 00        	leaq	0x75df(%rip), %rdi      ## 0x100008d45 <_sysconf+0x100008d45>
100001766: be 1d 00 00 00              	movl	$0x1d, %esi
10000176b: ba 01 00 00 00              	movl	$0x1, %edx
100001770: e8 cb 6e 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100001775: 48 8d 3d bc 98 00 00        	leaq	0x98bc(%rip), %rdi      ## 0x10000b038 <_mutex_owner>
10000177c: 48 8d 35 dd 79 00 00        	leaq	0x79dd(%rip), %rsi      ## 0x100009160 <_abandon>
100001783: 41 bf 01 00 00 00           	movl	$0x1, %r15d
100001789: ba 01 00 00 00              	movl	$0x1, %edx
10000178e: e8 3d 59 00 00              	callq	0x1000070d0 <_mb_win_mutex_create>
100001793: 48 85 c0                    	testq	%rax, %rax
100001796: 75 08                       	jne	0x1000017a0 <_worker_start+0x120>
100001798: b9 01 00 00 00              	movl	$0x1, %ecx
10000179d: 87 4b 58                    	xchgl	%ecx, 0x58(%rbx)
1000017a0: 49 8b 7d 00                 	movq	(%r13), %rdi
1000017a4: 48 8d 35 b8 75 00 00        	leaq	0x75b8(%rip), %rsi      ## 0x100008d63 <_sysconf+0x100008d63>
1000017ab: 48 89 c2                    	movq	%rax, %rdx
1000017ae: 31 c0                       	xorl	%eax, %eax
1000017b0: e8 7f 6e 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000017b5: 44 87 7b 50                 	xchgl	%r15d, 0x50(%rbx)
1000017b9: 4c 8d 3d 50 98 00 00        	leaq	0x9850(%rip), %r15      ## 0x10000b010 <_workers>
1000017c0: 4c 89 ff                    	movq	%r15, %rdi
1000017c3: 31 f6                       	xorl	%esi, %esi
1000017c5: e8 e6 1e 00 00              	callq	0x1000036b0 <_mb_win_workers_sleep>
1000017ca: 4c 89 ff                    	movq	%r15, %rdi
1000017cd: be 01 00 00 00              	movl	$0x1, %esi
1000017d2: e8 d9 1e 00 00              	callq	0x1000036b0 <_mb_win_workers_sleep>
1000017d7: c7 85 1c ff ff ff 00 00 00 00       	movl	$0x0, -0xe4(%rbp)
1000017e1: 48 8d 3d 30 98 00 00        	leaq	0x9830(%rip), %rdi      ## 0x10000b018 <_critical>
1000017e8: 48 8d 95 1c ff ff ff        	leaq	-0xe4(%rbp), %rdx
1000017ef: 48 89 de                    	movq	%rbx, %rsi
1000017f2: e8 89 28 00 00              	callq	0x100004080 <_mb_win_critical_try_enter>
1000017f7: 83 bd 1c ff ff ff 00        	cmpl	$0x0, -0xe4(%rbp)
1000017fe: 74 0f                       	je	0x10000180f <_worker_start+0x18f>
100001800: 48 8d 3d 11 98 00 00        	leaq	0x9811(%rip), %rdi      ## 0x10000b018 <_critical>
100001807: 48 89 de                    	movq	%rbx, %rsi
10000180a: e8 31 29 00 00              	callq	0x100004140 <_mb_win_critical_leave>
10000180f: 4c 8d 7b 28                 	leaq	0x28(%rbx), %r15
100001813: 4c 8d 25 0e 98 00 00        	leaq	0x980e(%rip), %r12      ## 0x10000b028 <_srw>
10000181a: 4c 89 e7                    	movq	%r12, %rdi
10000181d: 4c 89 fe                    	movq	%r15, %rsi
100001820: 31 d2                       	xorl	%edx, %edx
100001822: e8 59 38 00 00              	callq	0x100005080 <_mb_win_srw_acquire>
100001827: 83 bd 1c ff ff ff 01        	cmpl	$0x1, -0xe4(%rbp)
10000182e: b8 02 00 00 00              	movl	$0x2, %eax
100001833: 83 d8 00                    	sbbl	$0x0, %eax
100001836: 87 43 4c                    	xchgl	%eax, 0x4c(%rbx)
100001839: 48 8d 73 30                 	leaq	0x30(%rbx), %rsi
10000183d: 4c 89 e7                    	movq	%r12, %rdi
100001840: ba 01 00 00 00              	movl	$0x1, %edx
100001845: e8 26 46 00 00              	callq	0x100005e70 <_mb_win_srw_wake>
10000184a: 4c 89 e7                    	movq	%r12, %rdi
10000184d: 4c 89 fe                    	movq	%r15, %rsi
100001850: 31 d2                       	xorl	%edx, %edx
100001852: e8 49 3d 00 00              	callq	0x1000055a0 <_mb_win_srw_release>
100001857: 49 8b 7d 00                 	movq	(%r13), %rdi
10000185b: 8b 53 4c                    	movl	0x4c(%rbx), %edx
10000185e: 48 8d 35 2f 75 00 00        	leaq	0x752f(%rip), %rsi      ## 0x100008d94 <_sysconf+0x100008d94>
100001865: 31 c0                       	xorl	%eax, %eax
100001867: e8 c8 6d 00 00              	callq	0x100008634 <_sysconf+0x100008634>
10000186c: c7 85 10 ff ff ff 00 00 00 00       	movl	$0x0, -0xf0(%rbp)
100001876: 4c 8d 7b 48                 	leaq	0x48(%rbx), %r15
10000187a: 4c 8d 25 9f 97 00 00        	leaq	0x979f(%rip), %r12      ## 0x10000b020 <_address_wait>
100001881: 4c 8d ad 10 ff ff ff        	leaq	-0xf0(%rbp), %r13
100001888: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)
100001890: 41 8b 07                    	movl	(%r15), %eax
100001893: 85 c0                       	testl	%eax, %eax
100001895: 75 25                       	jne	0x1000018bc <_worker_start+0x23c>
100001897: b9 04 00 00 00              	movl	$0x4, %ecx
10000189c: 4c 89 e7                    	movq	%r12, %rdi
10000189f: 4c 89 fe                    	movq	%r15, %rsi
1000018a2: 4c 89 ea                    	movq	%r13, %rdx
1000018a5: 41 b8 ff ff ff ff           	movl	$0xffffffff, %r8d       ## imm = 0xFFFFFFFF
1000018ab: e8 50 2e 00 00              	callq	0x100004700 <_mb_win_address_wait_wait>
1000018b0: 85 c0                       	testl	%eax, %eax
1000018b2: 75 dc                       	jne	0x100001890 <_worker_start+0x210>
1000018b4: b8 01 00 00 00              	movl	$0x1, %eax
1000018b9: 87 43 58                    	xchgl	%eax, 0x58(%rbx)
1000018bc: 49 bf 02 00 5a 5a ef be d2 51       	movabsq	$0x51d2beef5a5a0002, %r15 ## imm = 0x51D2BEEF5A5A0002
1000018c6: 4c 8b 2d 4b 87 00 00        	movq	0x874b(%rip), %r13      ## 0x10000a018 <_sysconf+0x10000a018>
1000018cd: 49 8b 4d 00                 	movq	(%r13), %rcx
1000018d1: 48 8d 3d e0 74 00 00        	leaq	0x74e0(%rip), %rdi      ## 0x100008db8 <_sysconf+0x100008db8>
1000018d8: be 23 00 00 00              	movl	$0x23, %esi
1000018dd: ba 01 00 00 00              	movl	$0x1, %edx
1000018e2: e8 59 6d 00 00              	callq	0x100008640 <_sysconf+0x100008640>
1000018e7: 4c 8d 25 2a 97 00 00        	leaq	0x972a(%rip), %r12      ## 0x10000b018 <_critical>
1000018ee: 4c 89 e7                    	movq	%r12, %rdi
1000018f1: 48 89 de                    	movq	%rbx, %rsi
1000018f4: e8 57 25 00 00              	callq	0x100003e50 <_mb_win_critical_enter>
1000018f9: 48 8d 3d 08 97 00 00        	leaq	0x9708(%rip), %rdi      ## 0x10000b008 <_domain>
100001900: e8 1b 08 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100001905: 8b 00                       	movl	(%rax), %eax
100001907: 89 43 5c                    	movl	%eax, 0x5c(%rbx)
10000190a: 4c 89 e7                    	movq	%r12, %rdi
10000190d: 48 89 de                    	movq	%rbx, %rsi
100001910: e8 2b 28 00 00              	callq	0x100004140 <_mb_win_critical_leave>
100001915: 49 8b 7d 00                 	movq	(%r13), %rdi
100001919: 8b 53 5c                    	movl	0x5c(%rbx), %edx
10000191c: 48 8d 35 b9 74 00 00        	leaq	0x74b9(%rip), %rsi      ## 0x100008ddc <_sysconf+0x100008ddc>
100001923: 31 c0                       	xorl	%eax, %eax
100001925: e8 0a 6d 00 00              	callq	0x100008634 <_sysconf+0x100008634>
10000192a: 48 8d 3d ff 96 00 00        	leaq	0x96ff(%rip), %rdi      ## 0x10000b030 <_init_once>
100001931: 4c 89 f6                    	movq	%r14, %rsi
100001934: 31 d2                       	xorl	%edx, %edx
100001936: 4c 89 f9                    	movq	%r15, %rcx
100001939: e8 52 4f 00 00              	callq	0x100006890 <_mb_win_init_once_complete>
10000193e: 85 c0                       	testl	%eax, %eax
100001940: 75 08                       	jne	0x10000194a <_worker_start+0x2ca>
100001942: b8 01 00 00 00              	movl	$0x1, %eax
100001947: 87 43 58                    	xchgl	%eax, 0x58(%rbx)
10000194a: c7 85 18 ff ff ff 00 00 00 00       	movl	$0x0, -0xe8(%rbp)
100001954: 48 c7 85 08 ff ff ff 00 00 00 00    	movq	$0x0, -0xf8(%rbp)
10000195f: 48 8d 3d ca 96 00 00        	leaq	0x96ca(%rip), %rdi      ## 0x10000b030 <_init_once>
100001966: 41 bc 01 00 00 00           	movl	$0x1, %r12d
10000196c: 48 8d 8d 18 ff ff ff        	leaq	-0xe8(%rbp), %rcx
100001973: 4c 8d 85 08 ff ff ff        	leaq	-0xf8(%rbp), %r8
10000197a: 4c 89 f6                    	movq	%r14, %rsi
10000197d: ba 01 00 00 00              	movl	$0x1, %edx
100001982: e8 99 49 00 00              	callq	0x100006320 <_mb_win_init_once_begin>
100001987: 85 c0                       	testl	%eax, %eax
100001989: 74 12                       	je	0x10000199d <_worker_start+0x31d>
10000198b: 83 bd 18 ff ff ff 00        	cmpl	$0x0, -0xe8(%rbp)
100001992: 75 09                       	jne	0x10000199d <_worker_start+0x31d>
100001994: 4c 39 bd 08 ff ff ff        	cmpq	%r15, -0xf8(%rbp)
10000199b: 74 04                       	je	0x1000019a1 <_worker_start+0x321>
10000199d: 44 87 63 58                 	xchgl	%r12d, 0x58(%rbx)
1000019a1: b8 01 00 00 00              	movl	$0x1, %eax
1000019a6: 87 43 54                    	xchgl	%eax, 0x54(%rbx)
1000019a9: 49 8b 7d 00                 	movq	(%r13), %rdi
1000019ad: 8b 43 58                    	movl	0x58(%rbx), %eax
1000019b0: 85 c0                       	testl	%eax, %eax
1000019b2: 41 bf 67 45 23 81           	movl	$0x81234567, %r15d      ## imm = 0x81234567
1000019b8: 41 be 02 00 01 f0           	movl	$0xf0010002, %r14d      ## imm = 0xF0010002
1000019be: ba 02 00 01 f0              	movl	$0xf0010002, %edx       ## imm = 0xF0010002
1000019c3: 41 0f 44 d7                 	cmovel	%r15d, %edx
1000019c7: 48 8d 35 2a 74 00 00        	leaq	0x742a(%rip), %rsi      ## 0x100008df8 <_sysconf+0x100008df8>
1000019ce: 31 c0                       	xorl	%eax, %eax
1000019d0: e8 5f 6c 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000019d5: 8b 43 58                    	movl	0x58(%rbx), %eax
1000019d8: 85 c0                       	testl	%eax, %eax
1000019da: 45 0f 44 f7                 	cmovel	%r15d, %r14d
1000019de: 44 89 f0                    	movl	%r14d, %eax
1000019e1: 0f 28 b5 20 ff ff ff        	movaps	-0xe0(%rbp), %xmm6
1000019e8: 0f 28 bd 30 ff ff ff        	movaps	-0xd0(%rbp), %xmm7
1000019ef: 44 0f 28 85 40 ff ff ff     	movaps	-0xc0(%rbp), %xmm8
1000019f7: 44 0f 28 8d 50 ff ff ff     	movaps	-0xb0(%rbp), %xmm9
1000019ff: 44 0f 28 95 60 ff ff ff     	movaps	-0xa0(%rbp), %xmm10
100001a07: 44 0f 28 9d 70 ff ff ff     	movaps	-0x90(%rbp), %xmm11
100001a0f: 44 0f 28 65 80              	movaps	-0x80(%rbp), %xmm12
100001a14: 44 0f 28 6d 90              	movaps	-0x70(%rbp), %xmm13
100001a19: 44 0f 28 75 a0              	movaps	-0x60(%rbp), %xmm14
100001a1e: 44 0f 28 7d b0              	movaps	-0x50(%rbp), %xmm15
100001a23: 48 81 c4 c8 00 00 00        	addq	$0xc8, %rsp
100001a2a: 5b                          	popq	%rbx
100001a2b: 5f                          	popq	%rdi
100001a2c: 5e                          	popq	%rsi
100001a2d: 41 5c                       	popq	%r12
100001a2f: 41 5d                       	popq	%r13
100001a31: 41 5e                       	popq	%r14
100001a33: 41 5f                       	popq	%r15
100001a35: 5d                          	popq	%rbp
100001a36: c3                          	retq
		...
100001a3f: 00 48 85                    	addb	%cl, -0x7b(%rax)

0000000100001a40 <_mb_win_threads_open>:
100001a40: 48 85 ff                    	testq	%rdi, %rdi
100001a43: 74 0c                       	je	0x100001a51 <_mb_win_threads_open+0x11>
100001a45: b8 02 00 00 00              	movl	$0x2, %eax
100001a4a: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
100001a4e: 74 07                       	je	0x100001a57 <_mb_win_threads_open+0x17>
100001a50: c3                          	retq
100001a51: b8 01 00 00 00              	movl	$0x1, %eax
100001a56: c3                          	retq
100001a57: 55                          	pushq	%rbp
100001a58: 48 89 e5                    	movq	%rsp, %rbp
100001a5b: 41 57                       	pushq	%r15
100001a5d: 41 56                       	pushq	%r14
100001a5f: 41 54                       	pushq	%r12
100001a61: 53                          	pushq	%rbx
100001a62: 49 89 fc                    	movq	%rdi, %r12
100001a65: 49 89 f7                    	movq	%rsi, %r15
100001a68: bf 01 00 00 00              	movl	$0x1, %edi
100001a6d: be 00 01 00 00              	movl	$0x100, %esi            ## imm = 0x100
100001a72: e8 a5 6b 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
100001a77: 48 85 c0                    	testq	%rax, %rax
100001a7a: 74 1d                       	je	0x100001a99 <_mb_win_threads_open+0x59>
100001a7c: 49 89 c6                    	movq	%rax, %r14
100001a7f: 48 89 c7                    	movq	%rax, %rdi
100001a82: 31 f6                       	xorl	%esi, %esi
100001a84: e8 29 6c 00 00              	callq	0x1000086b2 <_sysconf+0x1000086b2>
100001a89: 85 c0                       	testl	%eax, %eax
100001a8b: 74 27                       	je	0x100001ab4 <_mb_win_threads_open+0x74>
100001a8d: 89 c3                       	movl	%eax, %ebx
100001a8f: 4c 89 f7                    	movq	%r14, %rdi
100001a92: e8 a3 6b 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100001a97: eb 05                       	jmp	0x100001a9e <_mb_win_threads_open+0x5e>
100001a99: bb 0c 00 00 00              	movl	$0xc, %ebx
100001a9e: b8 04 00 00 00              	movl	$0x4, %eax
100001aa3: 4d 85 ff                    	testq	%r15, %r15
100001aa6: 74 03                       	je	0x100001aab <_mb_win_threads_open+0x6b>
100001aa8: 41 89 1f                    	movl	%ebx, (%r15)
100001aab: 5b                          	popq	%rbx
100001aac: 41 5c                       	popq	%r12
100001aae: 41 5e                       	popq	%r14
100001ab0: 41 5f                       	popq	%r15
100001ab2: 5d                          	popq	%rbp
100001ab3: c3                          	retq
100001ab4: 4c 89 f7                    	movq	%r14, %rdi
100001ab7: 48 83 c7 40                 	addq	$0x40, %rdi
100001abb: 48 8d 35 4e 00 00 00        	leaq	0x4e(%rip), %rsi        ## 0x100001b10 <_thread_exited>
100001ac2: e8 d9 6b 00 00              	callq	0x1000086a0 <_sysconf+0x1000086a0>
100001ac7: 85 c0                       	testl	%eax, %eax
100001ac9: 74 1f                       	je	0x100001aea <_mb_win_threads_open+0xaa>
100001acb: 89 c3                       	movl	%eax, %ebx
100001acd: 4c 89 f7                    	movq	%r14, %rdi
100001ad0: e8 d7 6b 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100001ad5: 85 c0                       	testl	%eax, %eax
100001ad7: 74 b6                       	je	0x100001a8f <_mb_win_threads_open+0x4f>
100001ad9: 41 c7 86 f8 00 00 00 01 00 00 00    	movl	$0x1, 0xf8(%r14)
100001ae4: 4d 89 34 24                 	movq	%r14, (%r12)
100001ae8: eb b4                       	jmp	0x100001a9e <_mb_win_threads_open+0x5e>
100001aea: 41 c7 86 fc 00 00 00 01 00 00 00    	movl	$0x1, 0xfc(%r14)
100001af5: 4d 89 34 24                 	movq	%r14, (%r12)
100001af9: 31 db                       	xorl	%ebx, %ebx
100001afb: b8 00 00 00 00              	movl	$0x0, %eax
100001b00: 4d 85 ff                    	testq	%r15, %r15
100001b03: 75 a3                       	jne	0x100001aa8 <_mb_win_threads_open+0x68>
100001b05: eb a4                       	jmp	0x100001aab <_mb_win_threads_open+0x6b>
100001b07: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)

0000000100001b10 <_thread_exited>:
100001b10: 55                          	pushq	%rbp
100001b11: 48 89 e5                    	movq	%rsp, %rbp
100001b14: 41 56                       	pushq	%r14
100001b16: 53                          	pushq	%rbx
100001b17: 4c 8b 37                    	movq	(%rdi), %r14
100001b1a: 4d 85 f6                    	testq	%r14, %r14
100001b1d: 74 66                       	je	0x100001b85 <_thread_exited+0x75>
100001b1f: 48 89 fb                    	movq	%rdi, %rbx
100001b22: 4c 89 f7                    	movq	%r14, %rdi
100001b25: e8 8e 6b 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100001b2a: 85 c0                       	testl	%eax, %eax
100001b2c: 75 57                       	jne	0x100001b85 <_thread_exited+0x75>
100001b2e: 49 8b 46 48                 	movq	0x48(%r14), %rax
100001b32: 48 85 c0                    	testq	%rax, %rax
100001b35: 74 4e                       	je	0x100001b85 <_thread_exited+0x75>
100001b37: 4c 39 33                    	cmpq	%r14, (%rbx)
100001b3a: 75 49                       	jne	0x100001b85 <_thread_exited+0x75>
100001b3c: 48 8b 4b 08                 	movq	0x8(%rbx), %rcx
100001b40: 49 8d 96 e8 00 00 00        	leaq	0xe8(%r14), %rdx
100001b47: 48 8d 71 10                 	leaq	0x10(%rcx), %rsi
100001b4b: 48 85 c9                    	testq	%rcx, %rcx
100001b4e: 48 0f 44 f2                 	cmoveq	%rdx, %rsi
100001b52: 48 8b 53 10                 	movq	0x10(%rbx), %rdx
100001b56: 48 89 16                    	movq	%rdx, (%rsi)
100001b59: 48 8b 53 10                 	movq	0x10(%rbx), %rdx
100001b5d: 48 85 d2                    	testq	%rdx, %rdx
100001b60: 74 04                       	je	0x100001b66 <_thread_exited+0x56>
100001b62: 48 89 4a 08                 	movq	%rcx, 0x8(%rdx)
100001b66: 48 ff c8                    	decq	%rax
100001b69: 49 89 46 48                 	movq	%rax, 0x48(%r14)
100001b6d: 4c 89 f7                    	movq	%r14, %rdi
100001b70: e8 49 6b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001b75: 85 c0                       	testl	%eax, %eax
100001b77: 75 0c                       	jne	0x100001b85 <_thread_exited+0x75>
100001b79: 48 89 df                    	movq	%rbx, %rdi
100001b7c: 5b                          	popq	%rbx
100001b7d: 41 5e                       	popq	%r14
100001b7f: 5d                          	popq	%rbp
100001b80: e9 b5 6a 00 00              	jmp	0x10000863a <_sysconf+0x10000863a>
100001b85: e8 86 6a 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100001b8a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100001b90 <_mb_win_threads_enter>:
100001b90: 55                          	pushq	%rbp
100001b91: 48 89 e5                    	movq	%rsp, %rbp
100001b94: 41 57                       	pushq	%r15
100001b96: 41 56                       	pushq	%r14
100001b98: 41 54                       	pushq	%r12
100001b9a: 53                          	pushq	%rbx
100001b9b: 48 85 ff                    	testq	%rdi, %rdi
100001b9e: 74 3b                       	je	0x100001bdb <_mb_win_threads_enter+0x4b>
100001ba0: 48 8b 1f                    	movq	(%rdi), %rbx
100001ba3: b8 02 00 00 00              	movl	$0x2, %eax
100001ba8: 48 85 db                    	testq	%rbx, %rbx
100001bab: 74 33                       	je	0x100001be0 <_mb_win_threads_enter+0x50>
100001bad: 49 89 f4                    	movq	%rsi, %r12
100001bb0: 48 89 df                    	movq	%rbx, %rdi
100001bb3: e8 00 6b 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100001bb8: 85 c0                       	testl	%eax, %eax
100001bba: 75 1a                       	jne	0x100001bd6 <_mb_win_threads_enter+0x46>
100001bbc: 83 bb f8 00 00 00 00        	cmpl	$0x0, 0xf8(%rbx)
100001bc3: 74 24                       	je	0x100001be9 <_mb_win_threads_enter+0x59>
100001bc5: 48 89 df                    	movq	%rbx, %rdi
100001bc8: e8 f1 6a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001bcd: 85 c0                       	testl	%eax, %eax
100001bcf: b8 02 00 00 00              	movl	$0x2, %eax
100001bd4: 74 0a                       	je	0x100001be0 <_mb_win_threads_enter+0x50>
100001bd6: e8 35 6a 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100001bdb: b8 01 00 00 00              	movl	$0x1, %eax
100001be0: 5b                          	popq	%rbx
100001be1: 41 5c                       	popq	%r12
100001be3: 41 5e                       	popq	%r14
100001be5: 41 5f                       	popq	%r15
100001be7: 5d                          	popq	%rbp
100001be8: c3                          	retq
100001be9: 83 bb fc 00 00 00 00        	cmpl	$0x0, 0xfc(%rbx)
100001bf0: 74 d3                       	je	0x100001bc5 <_mb_win_threads_enter+0x35>
100001bf2: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100001bf6: e8 99 6a 00 00              	callq	0x100008694 <_sysconf+0x100008694>
100001bfb: 48 85 c0                    	testq	%rax, %rax
100001bfe: 74 1b                       	je	0x100001c1b <_mb_win_threads_enter+0x8b>
100001c00: 48 89 df                    	movq	%rbx, %rdi
100001c03: e8 b6 6a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001c08: 85 c0                       	testl	%eax, %eax
100001c0a: 75 ca                       	jne	0x100001bd6 <_mb_win_threads_enter+0x46>
100001c0c: 45 31 f6                    	xorl	%r14d, %r14d
100001c0f: b8 00 00 00 00              	movl	$0x0, %eax
100001c14: 4d 85 e4                    	testq	%r12, %r12
100001c17: 75 73                       	jne	0x100001c8c <_mb_win_threads_enter+0xfc>
100001c19: eb c5                       	jmp	0x100001be0 <_mb_win_threads_enter+0x50>
100001c1b: 48 83 7b 48 ff              	cmpq	$-0x1, 0x48(%rbx)
100001c20: 74 46                       	je	0x100001c68 <_mb_win_threads_enter+0xd8>
100001c22: bf 01 00 00 00              	movl	$0x1, %edi
100001c27: be 20 22 00 00              	movl	$0x2220, %esi           ## imm = 0x2220
100001c2c: e8 eb 69 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
100001c31: 48 85 c0                    	testq	%rax, %rax
100001c34: 74 32                       	je	0x100001c68 <_mb_win_threads_enter+0xd8>
100001c36: 49 89 c7                    	movq	%rax, %r15
100001c39: 48 89 18                    	movq	%rbx, (%rax)
100001c3c: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100001c40: 48 89 c6                    	movq	%rax, %rsi
100001c43: e8 82 6a 00 00              	callq	0x1000086ca <_sysconf+0x1000086ca>
100001c48: 85 c0                       	testl	%eax, %eax
100001c4a: 74 49                       	je	0x100001c95 <_mb_win_threads_enter+0x105>
100001c4c: 41 89 c6                    	movl	%eax, %r14d
100001c4f: 4c 89 ff                    	movq	%r15, %rdi
100001c52: e8 e3 69 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100001c57: 48 89 df                    	movq	%rbx, %rdi
100001c5a: e8 5f 6a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001c5f: 85 c0                       	testl	%eax, %eax
100001c61: 74 1b                       	je	0x100001c7e <_mb_win_threads_enter+0xee>
100001c63: e9 6e ff ff ff              	jmp	0x100001bd6 <_mb_win_threads_enter+0x46>
100001c68: 48 89 df                    	movq	%rbx, %rdi
100001c6b: e8 4e 6a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001c70: 85 c0                       	testl	%eax, %eax
100001c72: 0f 85 5e ff ff ff           	jne	0x100001bd6 <_mb_win_threads_enter+0x46>
100001c78: 41 be 0c 00 00 00           	movl	$0xc, %r14d
100001c7e: b8 04 00 00 00              	movl	$0x4, %eax
100001c83: 4d 85 e4                    	testq	%r12, %r12
100001c86: 0f 84 54 ff ff ff           	je	0x100001be0 <_mb_win_threads_enter+0x50>
100001c8c: 45 89 34 24                 	movl	%r14d, (%r12)
100001c90: e9 4b ff ff ff              	jmp	0x100001be0 <_mb_win_threads_enter+0x50>
100001c95: 48 8b 83 e8 00 00 00        	movq	0xe8(%rbx), %rax
100001c9c: 49 89 47 10                 	movq	%rax, 0x10(%r15)
100001ca0: 48 85 c0                    	testq	%rax, %rax
100001ca3: 74 04                       	je	0x100001ca9 <_mb_win_threads_enter+0x119>
100001ca5: 4c 89 78 08                 	movq	%r15, 0x8(%rax)
100001ca9: 4c 89 bb e8 00 00 00        	movq	%r15, 0xe8(%rbx)
100001cb0: 48 ff 43 48                 	incq	0x48(%rbx)
100001cb4: e9 47 ff ff ff              	jmp	0x100001c00 <_mb_win_threads_enter+0x70>
100001cb9: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100001cc0 <_mb_win_threads_leave>:
100001cc0: 55                          	pushq	%rbp
100001cc1: 48 89 e5                    	movq	%rsp, %rbp
100001cc4: 41 57                       	pushq	%r15
100001cc6: 41 56                       	pushq	%r14
100001cc8: 41 54                       	pushq	%r12
100001cca: 53                          	pushq	%rbx
100001ccb: 48 85 ff                    	testq	%rdi, %rdi
100001cce: 74 3b                       	je	0x100001d0b <_mb_win_threads_leave+0x4b>
100001cd0: 48 8b 1f                    	movq	(%rdi), %rbx
100001cd3: b8 02 00 00 00              	movl	$0x2, %eax
100001cd8: 48 85 db                    	testq	%rbx, %rbx
100001cdb: 74 33                       	je	0x100001d10 <_mb_win_threads_leave+0x50>
100001cdd: 49 89 f4                    	movq	%rsi, %r12
100001ce0: 48 89 df                    	movq	%rbx, %rdi
100001ce3: e8 d0 69 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100001ce8: 85 c0                       	testl	%eax, %eax
100001cea: 75 1a                       	jne	0x100001d06 <_mb_win_threads_leave+0x46>
100001cec: 83 bb f8 00 00 00 00        	cmpl	$0x0, 0xf8(%rbx)
100001cf3: 74 24                       	je	0x100001d19 <_mb_win_threads_leave+0x59>
100001cf5: 48 89 df                    	movq	%rbx, %rdi
100001cf8: e8 c1 69 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001cfd: 85 c0                       	testl	%eax, %eax
100001cff: b8 02 00 00 00              	movl	$0x2, %eax
100001d04: 74 0a                       	je	0x100001d10 <_mb_win_threads_leave+0x50>
100001d06: e8 05 69 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100001d0b: b8 01 00 00 00              	movl	$0x1, %eax
100001d10: 5b                          	popq	%rbx
100001d11: 41 5c                       	popq	%r12
100001d13: 41 5e                       	popq	%r14
100001d15: 41 5f                       	popq	%r15
100001d17: 5d                          	popq	%rbp
100001d18: c3                          	retq
100001d19: 83 bb fc 00 00 00 00        	cmpl	$0x0, 0xfc(%rbx)
100001d20: 74 d3                       	je	0x100001cf5 <_mb_win_threads_leave+0x35>
100001d22: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100001d26: e8 69 69 00 00              	callq	0x100008694 <_sysconf+0x100008694>
100001d2b: 48 85 c0                    	testq	%rax, %rax
100001d2e: 74 28                       	je	0x100001d58 <_mb_win_threads_leave+0x98>
100001d30: 49 89 c6                    	movq	%rax, %r14
100001d33: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100001d37: 31 f6                       	xorl	%esi, %esi
100001d39: e8 8c 69 00 00              	callq	0x1000086ca <_sysconf+0x1000086ca>
100001d3e: 85 c0                       	testl	%eax, %eax
100001d40: 74 24                       	je	0x100001d66 <_mb_win_threads_leave+0xa6>
100001d42: 41 89 c7                    	movl	%eax, %r15d
100001d45: 48 89 df                    	movq	%rbx, %rdi
100001d48: e8 71 69 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001d4d: 85 c0                       	testl	%eax, %eax
100001d4f: 75 b5                       	jne	0x100001d06 <_mb_win_threads_leave+0x46>
100001d51: b8 04 00 00 00              	movl	$0x4, %eax
100001d56: eb 6d                       	jmp	0x100001dc5 <_mb_win_threads_leave+0x105>
100001d58: 48 89 df                    	movq	%rbx, %rdi
100001d5b: e8 5e 69 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001d60: 85 c0                       	testl	%eax, %eax
100001d62: 74 59                       	je	0x100001dbd <_mb_win_threads_leave+0xfd>
100001d64: eb a0                       	jmp	0x100001d06 <_mb_win_threads_leave+0x46>
100001d66: 48 8b 43 48                 	movq	0x48(%rbx), %rax
100001d6a: 48 85 c0                    	testq	%rax, %rax
100001d6d: 74 97                       	je	0x100001d06 <_mb_win_threads_leave+0x46>
100001d6f: 49 39 1e                    	cmpq	%rbx, (%r14)
100001d72: 75 92                       	jne	0x100001d06 <_mb_win_threads_leave+0x46>
100001d74: 49 8b 4e 08                 	movq	0x8(%r14), %rcx
100001d78: 48 8d 93 e8 00 00 00        	leaq	0xe8(%rbx), %rdx
100001d7f: 48 8d 71 10                 	leaq	0x10(%rcx), %rsi
100001d83: 48 85 c9                    	testq	%rcx, %rcx
100001d86: 48 0f 44 f2                 	cmoveq	%rdx, %rsi
100001d8a: 49 8b 56 10                 	movq	0x10(%r14), %rdx
100001d8e: 48 89 16                    	movq	%rdx, (%rsi)
100001d91: 49 8b 56 10                 	movq	0x10(%r14), %rdx
100001d95: 48 85 d2                    	testq	%rdx, %rdx
100001d98: 74 04                       	je	0x100001d9e <_mb_win_threads_leave+0xde>
100001d9a: 48 89 4a 08                 	movq	%rcx, 0x8(%rdx)
100001d9e: 48 ff c8                    	decq	%rax
100001da1: 48 89 43 48                 	movq	%rax, 0x48(%rbx)
100001da5: 48 89 df                    	movq	%rbx, %rdi
100001da8: e8 11 69 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001dad: 85 c0                       	testl	%eax, %eax
100001daf: 0f 85 51 ff ff ff           	jne	0x100001d06 <_mb_win_threads_leave+0x46>
100001db5: 4c 89 f7                    	movq	%r14, %rdi
100001db8: e8 7d 68 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100001dbd: 45 31 ff                    	xorl	%r15d, %r15d
100001dc0: b8 00 00 00 00              	movl	$0x0, %eax
100001dc5: 4d 85 e4                    	testq	%r12, %r12
100001dc8: 0f 84 42 ff ff ff           	je	0x100001d10 <_mb_win_threads_leave+0x50>
100001dce: 45 89 3c 24                 	movl	%r15d, (%r12)
100001dd2: e9 39 ff ff ff              	jmp	0x100001d10 <_mb_win_threads_leave+0x50>
100001dd7: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)

0000000100001de0 <_mb_win_threads_close>:
100001de0: 55                          	pushq	%rbp
100001de1: 48 89 e5                    	movq	%rsp, %rbp
100001de4: 41 57                       	pushq	%r15
100001de6: 41 56                       	pushq	%r14
100001de8: 53                          	pushq	%rbx
100001de9: 50                          	pushq	%rax
100001dea: 48 85 ff                    	testq	%rdi, %rdi
100001ded: 74 46                       	je	0x100001e35 <_mb_win_threads_close+0x55>
100001def: 48 8b 1f                    	movq	(%rdi), %rbx
100001df2: 48 85 db                    	testq	%rbx, %rbx
100001df5: 74 45                       	je	0x100001e3c <_mb_win_threads_close+0x5c>
100001df7: 49 89 ff                    	movq	%rdi, %r15
100001dfa: 49 89 f6                    	movq	%rsi, %r14
100001dfd: 48 89 df                    	movq	%rbx, %rdi
100001e00: e8 b3 68 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100001e05: 85 c0                       	testl	%eax, %eax
100001e07: 75 27                       	jne	0x100001e30 <_mb_win_threads_close+0x50>
100001e09: 48 83 7b 48 00              	cmpq	$0x0, 0x48(%rbx)
100001e0e: 75 0d                       	jne	0x100001e1d <_mb_win_threads_close+0x3d>
100001e10: 48 83 7b 50 00              	cmpq	$0x0, 0x50(%rbx)
100001e15: 75 06                       	jne	0x100001e1d <_mb_win_threads_close+0x3d>
100001e17: 83 7b 58 00                 	cmpl	$0x0, 0x58(%rbx)
100001e1b: 74 38                       	je	0x100001e55 <_mb_win_threads_close+0x75>
100001e1d: 48 89 df                    	movq	%rbx, %rdi
100001e20: e8 99 68 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001e25: 89 c1                       	movl	%eax, %ecx
100001e27: b8 03 00 00 00              	movl	$0x3, %eax
100001e2c: 85 c9                       	testl	%ecx, %ecx
100001e2e: 74 1a                       	je	0x100001e4a <_mb_win_threads_close+0x6a>
100001e30: e8 db 67 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100001e35: b8 01 00 00 00              	movl	$0x1, %eax
100001e3a: eb 0e                       	jmp	0x100001e4a <_mb_win_threads_close+0x6a>
100001e3c: 31 c9                       	xorl	%ecx, %ecx
100001e3e: b8 00 00 00 00              	movl	$0x0, %eax
100001e43: 48 85 f6                    	testq	%rsi, %rsi
100001e46: 74 02                       	je	0x100001e4a <_mb_win_threads_close+0x6a>
100001e48: 89 0e                       	movl	%ecx, (%rsi)
100001e4a: 48 83 c4 08                 	addq	$0x8, %rsp
100001e4e: 5b                          	popq	%rbx
100001e4f: 41 5e                       	popq	%r14
100001e51: 41 5f                       	popq	%r15
100001e53: 5d                          	popq	%rbp
100001e54: c3                          	retq
100001e55: c7 83 f8 00 00 00 01 00 00 00       	movl	$0x1, 0xf8(%rbx)
100001e5f: 48 89 df                    	movq	%rbx, %rdi
100001e62: e8 57 68 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001e67: 85 c0                       	testl	%eax, %eax
100001e69: 75 c5                       	jne	0x100001e30 <_mb_win_threads_close+0x50>
100001e6b: 83 bb fc 00 00 00 00        	cmpl	$0x0, 0xfc(%rbx)
100001e72: 74 2c                       	je	0x100001ea0 <_mb_win_threads_close+0xc0>
100001e74: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100001e78: e8 29 68 00 00              	callq	0x1000086a6 <_sysconf+0x1000086a6>
100001e7d: 89 c1                       	movl	%eax, %ecx
100001e7f: 85 c0                       	testl	%eax, %eax
100001e81: 74 0a                       	je	0x100001e8d <_mb_win_threads_close+0xad>
100001e83: 4d 85 f6                    	testq	%r14, %r14
100001e86: 74 0f                       	je	0x100001e97 <_mb_win_threads_close+0xb7>
100001e88: 41 89 0e                    	movl	%ecx, (%r14)
100001e8b: eb 0a                       	jmp	0x100001e97 <_mb_win_threads_close+0xb7>
100001e8d: c7 83 fc 00 00 00 00 00 00 00       	movl	$0x0, 0xfc(%rbx)
100001e97: b8 04 00 00 00              	movl	$0x4, %eax
100001e9c: 85 c9                       	testl	%ecx, %ecx
100001e9e: 75 aa                       	jne	0x100001e4a <_mb_win_threads_close+0x6a>
100001ea0: 48 89 df                    	movq	%rbx, %rdi
100001ea3: e8 04 68 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100001ea8: 85 c0                       	testl	%eax, %eax
100001eaa: 74 09                       	je	0x100001eb5 <_mb_win_threads_close+0xd5>
100001eac: 89 c1                       	movl	%eax, %ecx
100001eae: b8 04 00 00 00              	movl	$0x4, %eax
100001eb3: eb 16                       	jmp	0x100001ecb <_mb_win_threads_close+0xeb>
100001eb5: 49 c7 07 00 00 00 00        	movq	$0x0, (%r15)
100001ebc: 48 89 df                    	movq	%rbx, %rdi
100001ebf: e8 76 67 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100001ec4: 31 c9                       	xorl	%ecx, %ecx
100001ec6: b8 00 00 00 00              	movl	$0x0, %eax
100001ecb: 4c 89 f6                    	movq	%r14, %rsi
100001ece: 4d 85 f6                    	testq	%r14, %r14
100001ed1: 0f 85 71 ff ff ff           	jne	0x100001e48 <_mb_win_threads_close+0x68>
100001ed7: e9 6e ff ff ff              	jmp	0x100001e4a <_mb_win_threads_close+0x6a>
100001edc: 0f 1f 40 00                 	nopl	(%rax)

0000000100001ee0 <_mb_win_threads_errno>:
100001ee0: 48 85 ff                    	testq	%rdi, %rdi
100001ee3: 74 1a                       	je	0x100001eff <_mb_win_threads_errno+0x1f>
100001ee5: 48 8b 07                    	movq	(%rdi), %rax
100001ee8: 48 85 c0                    	testq	%rax, %rax
100001eeb: 74 12                       	je	0x100001eff <_mb_win_threads_errno+0x1f>
100001eed: 83 b8 fc 00 00 00 00        	cmpl	$0x0, 0xfc(%rax)
100001ef4: 74 09                       	je	0x100001eff <_mb_win_threads_errno+0x1f>
100001ef6: 83 b8 f8 00 00 00 00        	cmpl	$0x0, 0xf8(%rax)
100001efd: 74 03                       	je	0x100001f02 <_mb_win_threads_errno+0x22>
100001eff: 31 c0                       	xorl	%eax, %eax
100001f01: c3                          	retq
100001f02: 55                          	pushq	%rbp
100001f03: 48 89 e5                    	movq	%rsp, %rbp
100001f06: 48 8b 78 40                 	movq	0x40(%rax), %rdi
100001f0a: e8 85 67 00 00              	callq	0x100008694 <_sysconf+0x100008694>
100001f0f: 48 89 c1                    	movq	%rax, %rcx
100001f12: 48 83 c0 18                 	addq	$0x18, %rax
100001f16: 48 85 c9                    	testq	%rcx, %rcx
100001f19: 48 0f 44 c1                 	cmoveq	%rcx, %rax
100001f1d: 5d                          	popq	%rbp
100001f1e: c3                          	retq
100001f1f: 90                          	nop

0000000100001f20 <_mb_win_threads_lock>:
100001f20: 55                          	pushq	%rbp
100001f21: 48 89 e5                    	movq	%rsp, %rbp
100001f24: 48 85 ff                    	testq	%rdi, %rdi
100001f27: 74 13                       	je	0x100001f3c <_mb_win_threads_lock+0x1c>
100001f29: 48 8b 3f                    	movq	(%rdi), %rdi
100001f2c: 48 85 ff                    	testq	%rdi, %rdi
100001f2f: 74 0b                       	je	0x100001f3c <_mb_win_threads_lock+0x1c>
100001f31: e8 82 67 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100001f36: 85 c0                       	testl	%eax, %eax
100001f38: 75 02                       	jne	0x100001f3c <_mb_win_threads_lock+0x1c>
100001f3a: 5d                          	popq	%rbp
100001f3b: c3                          	retq
100001f3c: e8 cf 66 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100001f41: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)

0000000100001f50 <_mb_win_threads_unlock>:
100001f50: 55                          	pushq	%rbp
100001f51: 48 89 e5                    	movq	%rsp, %rbp
100001f54: 48 85 ff                    	testq	%rdi, %rdi
100001f57: 74 13                       	je	0x100001f6c <_mb_win_threads_unlock+0x1c>
100001f59: 48 8b 3f                    	movq	(%rdi), %rdi
100001f5c: 48 85 ff                    	testq	%rdi, %rdi
100001f5f: 74 0b                       	je	0x100001f6c <_mb_win_threads_unlock+0x1c>
100001f61: e8 58 67 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001f66: 85 c0                       	testl	%eax, %eax
100001f68: 75 02                       	jne	0x100001f6c <_mb_win_threads_unlock+0x1c>
100001f6a: 5d                          	popq	%rbp
100001f6b: c3                          	retq
100001f6c: e8 9f 66 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100001f71: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)

0000000100001f80 <_mb_win_threads_allocate>:
100001f80: 55                          	pushq	%rbp
100001f81: 48 89 e5                    	movq	%rsp, %rbp
100001f84: 48 89 f7                    	movq	%rsi, %rdi
100001f87: 5d                          	popq	%rbp
100001f88: e9 b9 66 00 00              	jmp	0x100008646 <_sysconf+0x100008646>
100001f8d: 0f 1f 00                    	nopl	(%rax)

0000000100001f90 <_mb_win_threads_release>:
100001f90: 55                          	pushq	%rbp
100001f91: 48 89 e5                    	movq	%rsp, %rbp
100001f94: 48 89 f7                    	movq	%rsi, %rdi
100001f97: 5d                          	popq	%rbp
100001f98: e9 9d 66 00 00              	jmp	0x10000863a <_sysconf+0x10000863a>
100001f9d: 0f 1f 00                    	nopl	(%rax)

0000000100001fa0 <_mb_win_threads_acquire>:
100001fa0: 55                          	pushq	%rbp
100001fa1: 48 89 e5                    	movq	%rsp, %rbp
100001fa4: 41 57                       	pushq	%r15
100001fa6: 41 56                       	pushq	%r14
100001fa8: 53                          	pushq	%rbx
100001fa9: 50                          	pushq	%rax
100001faa: 48 85 ff                    	testq	%rdi, %rdi
100001fad: 0f 94 c1                    	sete	%cl
100001fb0: 48 85 f6                    	testq	%rsi, %rsi
100001fb3: 0f 94 c2                    	sete	%dl
100001fb6: b8 01 00 00 00              	movl	$0x1, %eax
100001fbb: 08 ca                       	orb	%cl, %dl
100001fbd: 75 3d                       	jne	0x100001ffc <_mb_win_threads_acquire+0x5c>
100001fbf: 48 8b 1f                    	movq	(%rdi), %rbx
100001fc2: b8 02 00 00 00              	movl	$0x2, %eax
100001fc7: 48 85 db                    	testq	%rbx, %rbx
100001fca: 74 30                       	je	0x100001ffc <_mb_win_threads_acquire+0x5c>
100001fcc: 49 89 fe                    	movq	%rdi, %r14
100001fcf: 49 89 f7                    	movq	%rsi, %r15
100001fd2: 48 89 df                    	movq	%rbx, %rdi
100001fd5: e8 de 66 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100001fda: 85 c0                       	testl	%eax, %eax
100001fdc: 0f 85 85 00 00 00           	jne	0x100002067 <_mb_win_threads_acquire+0xc7>
100001fe2: 83 bb f8 00 00 00 00        	cmpl	$0x0, 0xf8(%rbx)
100001fe9: 74 1c                       	je	0x100002007 <_mb_win_threads_acquire+0x67>
100001feb: 48 89 df                    	movq	%rbx, %rdi
100001fee: e8 cb 66 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100001ff3: 85 c0                       	testl	%eax, %eax
100001ff5: b8 02 00 00 00              	movl	$0x2, %eax
100001ffa: 75 6b                       	jne	0x100002067 <_mb_win_threads_acquire+0xc7>
100001ffc: 48 83 c4 08                 	addq	$0x8, %rsp
100002000: 5b                          	popq	%rbx
100002001: 41 5e                       	popq	%r14
100002003: 41 5f                       	popq	%r15
100002005: 5d                          	popq	%rbp
100002006: c3                          	retq
100002007: 83 bb fc 00 00 00 00        	cmpl	$0x0, 0xfc(%rbx)
10000200e: 74 db                       	je	0x100001feb <_mb_win_threads_acquire+0x4b>
100002010: 4c 89 f8                    	movq	%r15, %rax
100002013: 49 83 3f 00                 	cmpq	$0x0, (%r15)
100002017: 75 d2                       	jne	0x100001feb <_mb_win_threads_acquire+0x4b>
100002019: 48 83 78 08 00              	cmpq	$0x0, 0x8(%rax)
10000201e: 75 cb                       	jne	0x100001feb <_mb_win_threads_acquire+0x4b>
100002020: 48 8b 4b 50                 	movq	0x50(%rbx), %rcx
100002024: 48 83 f9 ff                 	cmpq	$-0x1, %rcx
100002028: 74 2a                       	je	0x100002054 <_mb_win_threads_acquire+0xb4>
10000202a: 4c 89 30                    	movq	%r14, (%rax)
10000202d: 48 8b 93 f0 00 00 00        	movq	0xf0(%rbx), %rdx
100002034: 48 89 50 08                 	movq	%rdx, 0x8(%rax)
100002038: 48 89 83 f0 00 00 00        	movq	%rax, 0xf0(%rbx)
10000203f: 48 ff c1                    	incq	%rcx
100002042: 48 89 4b 50                 	movq	%rcx, 0x50(%rbx)
100002046: 48 89 df                    	movq	%rbx, %rdi
100002049: e8 70 66 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000204e: 89 c1                       	movl	%eax, %ecx
100002050: 31 c0                       	xorl	%eax, %eax
100002052: eb 0f                       	jmp	0x100002063 <_mb_win_threads_acquire+0xc3>
100002054: 48 89 df                    	movq	%rbx, %rdi
100002057: e8 62 66 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000205c: 89 c1                       	movl	%eax, %ecx
10000205e: b8 04 00 00 00              	movl	$0x4, %eax
100002063: 85 c9                       	testl	%ecx, %ecx
100002065: 74 95                       	je	0x100001ffc <_mb_win_threads_acquire+0x5c>
100002067: e8 a4 65 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000206c: 0f 1f 40 00                 	nopl	(%rax)

0000000100002070 <_mb_win_threads_release_pin>:
100002070: 55                          	pushq	%rbp
100002071: 48 89 e5                    	movq	%rsp, %rbp
100002074: 41 56                       	pushq	%r14
100002076: 53                          	pushq	%rbx
100002077: 48 85 ff                    	testq	%rdi, %rdi
10000207a: 74 7c                       	je	0x1000020f8 <_mb_win_threads_release_pin+0x88>
10000207c: 48 8b 0f                    	movq	(%rdi), %rcx
10000207f: b8 02 00 00 00              	movl	$0x2, %eax
100002084: 48 85 c9                    	testq	%rcx, %rcx
100002087: 0f 84 83 00 00 00           	je	0x100002110 <_mb_win_threads_release_pin+0xa0>
10000208d: 48 8b 19                    	movq	(%rcx), %rbx
100002090: 48 85 db                    	testq	%rbx, %rbx
100002093: 74 7b                       	je	0x100002110 <_mb_win_threads_release_pin+0xa0>
100002095: 49 89 fe                    	movq	%rdi, %r14
100002098: 48 89 df                    	movq	%rbx, %rdi
10000209b: e8 18 66 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000020a0: 85 c0                       	testl	%eax, %eax
1000020a2: 75 71                       	jne	0x100002115 <_mb_win_threads_release_pin+0xa5>
1000020a4: 48 8d b3 f0 00 00 00        	leaq	0xf0(%rbx), %rsi
1000020ab: 4c 89 f0                    	movq	%r14, %rax
1000020ae: 66 90                       	nop
1000020b0: 48 89 f1                    	movq	%rsi, %rcx
1000020b3: 48 8b 16                    	movq	(%rsi), %rdx
1000020b6: 48 85 d2                    	testq	%rdx, %rdx
1000020b9: 74 09                       	je	0x1000020c4 <_mb_win_threads_release_pin+0x54>
1000020bb: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
1000020bf: 48 39 c2                    	cmpq	%rax, %rdx
1000020c2: 75 ec                       	jne	0x1000020b0 <_mb_win_threads_release_pin+0x40>
1000020c4: 48 85 d2                    	testq	%rdx, %rdx
1000020c7: 74 36                       	je	0x1000020ff <_mb_win_threads_release_pin+0x8f>
1000020c9: 48 8b 53 50                 	movq	0x50(%rbx), %rdx
1000020cd: 48 85 d2                    	testq	%rdx, %rdx
1000020d0: 74 2d                       	je	0x1000020ff <_mb_win_threads_release_pin+0x8f>
1000020d2: 48 8b 70 08                 	movq	0x8(%rax), %rsi
1000020d6: 48 89 31                    	movq	%rsi, (%rcx)
1000020d9: 48 ff ca                    	decq	%rdx
1000020dc: 48 89 53 50                 	movq	%rdx, 0x50(%rbx)
1000020e0: 0f 57 c0                    	xorps	%xmm0, %xmm0
1000020e3: 0f 11 00                    	movups	%xmm0, (%rax)
1000020e6: 48 89 df                    	movq	%rbx, %rdi
1000020e9: e8 d0 65 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000020ee: 89 c1                       	movl	%eax, %ecx
1000020f0: 31 c0                       	xorl	%eax, %eax
1000020f2: 85 c9                       	testl	%ecx, %ecx
1000020f4: 74 1a                       	je	0x100002110 <_mb_win_threads_release_pin+0xa0>
1000020f6: eb 1d                       	jmp	0x100002115 <_mb_win_threads_release_pin+0xa5>
1000020f8: b8 01 00 00 00              	movl	$0x1, %eax
1000020fd: eb 11                       	jmp	0x100002110 <_mb_win_threads_release_pin+0xa0>
1000020ff: 48 89 df                    	movq	%rbx, %rdi
100002102: e8 b7 65 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002107: 85 c0                       	testl	%eax, %eax
100002109: b8 02 00 00 00              	movl	$0x2, %eax
10000210e: 75 05                       	jne	0x100002115 <_mb_win_threads_release_pin+0xa5>
100002110: 5b                          	popq	%rbx
100002111: 41 5e                       	popq	%r14
100002113: 5d                          	popq	%rbp
100002114: c3                          	retq
100002115: e8 f6 64 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000211a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100002120 <_mb_win_threads_last_error>:
100002120: 55                          	pushq	%rbp
100002121: 48 89 e5                    	movq	%rsp, %rbp
100002124: 53                          	pushq	%rbx
100002125: 50                          	pushq	%rax
100002126: 48 85 ff                    	testq	%rdi, %rdi
100002129: 74 38                       	je	0x100002163 <_mb_win_threads_last_error+0x43>
10000212b: 48 8b 1f                    	movq	(%rdi), %rbx
10000212e: 48 85 db                    	testq	%rbx, %rbx
100002131: 74 30                       	je	0x100002163 <_mb_win_threads_last_error+0x43>
100002133: 83 bb fc 00 00 00 00        	cmpl	$0x0, 0xfc(%rbx)
10000213a: 74 27                       	je	0x100002163 <_mb_win_threads_last_error+0x43>
10000213c: 83 bb f8 00 00 00 00        	cmpl	$0x0, 0xf8(%rbx)
100002143: 75 1e                       	jne	0x100002163 <_mb_win_threads_last_error+0x43>
100002145: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100002149: e8 46 65 00 00              	callq	0x100008694 <_sysconf+0x100008694>
10000214e: 48 85 c0                    	testq	%rax, %rax
100002151: 74 10                       	je	0x100002163 <_mb_win_threads_last_error+0x43>
100002153: 48 39 18                    	cmpq	%rbx, (%rax)
100002156: 75 0b                       	jne	0x100002163 <_mb_win_threads_last_error+0x43>
100002158: 48 83 c0 1c                 	addq	$0x1c, %rax
10000215c: 48 83 c4 08                 	addq	$0x8, %rsp
100002160: 5b                          	popq	%rbx
100002161: 5d                          	popq	%rbp
100002162: c3                          	retq
100002163: e8 a8 64 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002168: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100002170 <_mb_win_threads_tls_alloc>:
100002170: 55                          	pushq	%rbp
100002171: 48 89 e5                    	movq	%rsp, %rbp
100002174: 41 57                       	pushq	%r15
100002176: 41 56                       	pushq	%r14
100002178: 53                          	pushq	%rbx
100002179: 50                          	pushq	%rax
10000217a: 48 85 ff                    	testq	%rdi, %rdi
10000217d: 0f 84 f9 00 00 00           	je	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
100002183: 4c 8b 37                    	movq	(%rdi), %r14
100002186: 4d 85 f6                    	testq	%r14, %r14
100002189: 0f 84 ed 00 00 00           	je	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
10000218f: 41 83 be fc 00 00 00 00     	cmpl	$0x0, 0xfc(%r14)
100002197: 0f 84 df 00 00 00           	je	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
10000219d: 41 83 be f8 00 00 00 00     	cmpl	$0x0, 0xf8(%r14)
1000021a5: 0f 85 d1 00 00 00           	jne	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
1000021ab: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000021af: e8 e0 64 00 00              	callq	0x100008694 <_sysconf+0x100008694>
1000021b4: 48 85 c0                    	testq	%rax, %rax
1000021b7: 0f 84 bf 00 00 00           	je	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
1000021bd: 49 89 c7                    	movq	%rax, %r15
1000021c0: 48 8b 18                    	movq	(%rax), %rbx
1000021c3: 4c 39 f3                    	cmpq	%r14, %rbx
1000021c6: 0f 85 b0 00 00 00           	jne	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
1000021cc: 48 85 db                    	testq	%rbx, %rbx
1000021cf: 0f 84 a7 00 00 00           	je	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
1000021d5: 48 89 df                    	movq	%rbx, %rdi
1000021d8: e8 db 64 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000021dd: 85 c0                       	testl	%eax, %eax
1000021df: 0f 85 97 00 00 00           	jne	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
1000021e5: 45 31 f6                    	xorl	%r14d, %r14d
1000021e8: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)
1000021f0: 44 89 f0                    	movl	%r14d, %eax
1000021f3: c1 e8 06                    	shrl	$0x6, %eax
1000021f6: 48 8b 54 c3 60              	movq	0x60(%rbx,%rax,8), %rdx
1000021fb: be 01 00 00 00              	movl	$0x1, %esi
100002200: 44 89 f1                    	movl	%r14d, %ecx
100002203: 48 d3 e6                    	shlq	%cl, %rsi
100002206: 4c 0f a3 f2                 	btq	%r14, %rdx
10000220a: 73 24                       	jae	0x100002230 <_mb_win_threads_tls_alloc+0xc0>
10000220c: 49 ff c6                    	incq	%r14
10000220f: 41 81 fe 40 04 00 00        	cmpl	$0x440, %r14d           ## imm = 0x440
100002216: 75 d8                       	jne	0x1000021f0 <_mb_win_threads_tls_alloc+0x80>
100002218: 41 c7 47 1c 08 00 00 00     	movl	$0x8, 0x1c(%r15)
100002220: 48 89 df                    	movq	%rbx, %rdi
100002223: e8 96 64 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002228: 41 be ff ff ff ff           	movl	$0xffffffff, %r14d      ## imm = 0xFFFFFFFF
10000222e: eb 3a                       	jmp	0x10000226a <_mb_win_threads_tls_alloc+0xfa>
100002230: 48 09 f2                    	orq	%rsi, %rdx
100002233: 48 89 54 c3 60              	movq	%rdx, 0x60(%rbx,%rax,8)
100002238: ff 43 58                    	incl	0x58(%rbx)
10000223b: 48 8b 83 e8 00 00 00        	movq	0xe8(%rbx), %rax
100002242: 48 85 c0                    	testq	%rax, %rax
100002245: 74 1b                       	je	0x100002262 <_mb_win_threads_tls_alloc+0xf2>
100002247: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100002250: 4a c7 44 f0 20 00 00 00 00  	movq	$0x0, 0x20(%rax,%r14,8)
100002259: 48 8b 40 10                 	movq	0x10(%rax), %rax
10000225d: 48 85 c0                    	testq	%rax, %rax
100002260: 75 ee                       	jne	0x100002250 <_mb_win_threads_tls_alloc+0xe0>
100002262: 48 89 df                    	movq	%rbx, %rdi
100002265: e8 54 64 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000226a: 85 c0                       	testl	%eax, %eax
10000226c: 75 0e                       	jne	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
10000226e: 44 89 f0                    	movl	%r14d, %eax
100002271: 48 83 c4 08                 	addq	$0x8, %rsp
100002275: 5b                          	popq	%rbx
100002276: 41 5e                       	popq	%r14
100002278: 41 5f                       	popq	%r15
10000227a: 5d                          	popq	%rbp
10000227b: c3                          	retq
10000227c: e8 8f 63 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002281: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)

0000000100002290 <_mb_win_threads_tls_free>:
100002290: 55                          	pushq	%rbp
100002291: 48 89 e5                    	movq	%rsp, %rbp
100002294: 41 57                       	pushq	%r15
100002296: 41 56                       	pushq	%r14
100002298: 41 54                       	pushq	%r12
10000229a: 53                          	pushq	%rbx
10000229b: 48 85 ff                    	testq	%rdi, %rdi
10000229e: 0f 84 ba 00 00 00           	je	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022a4: 4c 8b 27                    	movq	(%rdi), %r12
1000022a7: 4d 85 e4                    	testq	%r12, %r12
1000022aa: 0f 84 ae 00 00 00           	je	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022b0: 41 83 bc 24 fc 00 00 00 00  	cmpl	$0x0, 0xfc(%r12)
1000022b9: 0f 84 9f 00 00 00           	je	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022bf: 41 83 bc 24 f8 00 00 00 00  	cmpl	$0x0, 0xf8(%r12)
1000022c8: 0f 85 90 00 00 00           	jne	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022ce: 89 f3                       	movl	%esi, %ebx
1000022d0: 49 8b 7c 24 40              	movq	0x40(%r12), %rdi
1000022d5: e8 ba 63 00 00              	callq	0x100008694 <_sysconf+0x100008694>
1000022da: 48 85 c0                    	testq	%rax, %rax
1000022dd: 74 7f                       	je	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022df: 49 89 c7                    	movq	%rax, %r15
1000022e2: 4c 8b 30                    	movq	(%rax), %r14
1000022e5: 4d 39 e6                    	cmpq	%r12, %r14
1000022e8: 75 74                       	jne	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022ea: 4d 85 f6                    	testq	%r14, %r14
1000022ed: 74 6f                       	je	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022ef: 4c 89 f7                    	movq	%r14, %rdi
1000022f2: e8 c1 63 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000022f7: 85 c0                       	testl	%eax, %eax
1000022f9: 75 63                       	jne	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022fb: 81 fb 3f 04 00 00           	cmpl	$0x43f, %ebx            ## imm = 0x43F
100002301: 77 3a                       	ja	0x10000233d <_mb_win_threads_tls_free+0xad>
100002303: 89 d8                       	movl	%ebx, %eax
100002305: c1 e8 06                    	shrl	$0x6, %eax
100002308: 49 8b 54 c6 60              	movq	0x60(%r14,%rax,8), %rdx
10000230d: be 01 00 00 00              	movl	$0x1, %esi
100002312: 89 d9                       	movl	%ebx, %ecx
100002314: 48 d3 e6                    	shlq	%cl, %rsi
100002317: 48 0f a3 da                 	btq	%rbx, %rdx
10000231b: 73 20                       	jae	0x10000233d <_mb_win_threads_tls_free+0xad>
10000231d: 48 f7 d6                    	notq	%rsi
100002320: 48 21 f2                    	andq	%rsi, %rdx
100002323: 49 89 54 c6 60              	movq	%rdx, 0x60(%r14,%rax,8)
100002328: 41 ff 4e 58                 	decl	0x58(%r14)
10000232c: 4c 89 f7                    	movq	%r14, %rdi
10000232f: e8 8a 63 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002334: 89 c1                       	movl	%eax, %ecx
100002336: b8 01 00 00 00              	movl	$0x1, %eax
10000233b: eb 14                       	jmp	0x100002351 <_mb_win_threads_tls_free+0xc1>
10000233d: 41 c7 47 1c 57 00 00 00     	movl	$0x57, 0x1c(%r15)
100002345: 4c 89 f7                    	movq	%r14, %rdi
100002348: e8 71 63 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000234d: 89 c1                       	movl	%eax, %ecx
10000234f: 31 c0                       	xorl	%eax, %eax
100002351: 85 c9                       	testl	%ecx, %ecx
100002353: 75 09                       	jne	0x10000235e <_mb_win_threads_tls_free+0xce>
100002355: 5b                          	popq	%rbx
100002356: 41 5c                       	popq	%r12
100002358: 41 5e                       	popq	%r14
10000235a: 41 5f                       	popq	%r15
10000235c: 5d                          	popq	%rbp
10000235d: c3                          	retq
10000235e: e8 ad 62 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002363: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)

0000000100002370 <_mb_win_threads_tls_get>:
100002370: 55                          	pushq	%rbp
100002371: 48 89 e5                    	movq	%rsp, %rbp
100002374: 41 57                       	pushq	%r15
100002376: 41 56                       	pushq	%r14
100002378: 41 54                       	pushq	%r12
10000237a: 53                          	pushq	%rbx
10000237b: 48 85 ff                    	testq	%rdi, %rdi
10000237e: 0f 84 b3 00 00 00           	je	0x100002437 <_mb_win_threads_tls_get+0xc7>
100002384: 4c 8b 27                    	movq	(%rdi), %r12
100002387: 4d 85 e4                    	testq	%r12, %r12
10000238a: 0f 84 a7 00 00 00           	je	0x100002437 <_mb_win_threads_tls_get+0xc7>
100002390: 41 83 bc 24 fc 00 00 00 00  	cmpl	$0x0, 0xfc(%r12)
100002399: 0f 84 98 00 00 00           	je	0x100002437 <_mb_win_threads_tls_get+0xc7>
10000239f: 41 83 bc 24 f8 00 00 00 00  	cmpl	$0x0, 0xf8(%r12)
1000023a8: 0f 85 89 00 00 00           	jne	0x100002437 <_mb_win_threads_tls_get+0xc7>
1000023ae: 89 f3                       	movl	%esi, %ebx
1000023b0: 49 8b 7c 24 40              	movq	0x40(%r12), %rdi
1000023b5: e8 da 62 00 00              	callq	0x100008694 <_sysconf+0x100008694>
1000023ba: 48 85 c0                    	testq	%rax, %rax
1000023bd: 74 78                       	je	0x100002437 <_mb_win_threads_tls_get+0xc7>
1000023bf: 49 89 c6                    	movq	%rax, %r14
1000023c2: 4c 8b 38                    	movq	(%rax), %r15
1000023c5: 4d 39 e7                    	cmpq	%r12, %r15
1000023c8: 75 6d                       	jne	0x100002437 <_mb_win_threads_tls_get+0xc7>
1000023ca: 4d 85 ff                    	testq	%r15, %r15
1000023cd: 74 68                       	je	0x100002437 <_mb_win_threads_tls_get+0xc7>
1000023cf: 4c 89 ff                    	movq	%r15, %rdi
1000023d2: e8 e1 62 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000023d7: 85 c0                       	testl	%eax, %eax
1000023d9: 75 5c                       	jne	0x100002437 <_mb_win_threads_tls_get+0xc7>
1000023db: 83 fb 40                    	cmpl	$0x40, %ebx
1000023de: 72 18                       	jb	0x1000023f8 <_mb_win_threads_tls_get+0x88>
1000023e0: 81 fb 3f 04 00 00           	cmpl	$0x43f, %ebx            ## imm = 0x43F
1000023e6: 77 2d                       	ja	0x100002415 <_mb_win_threads_tls_get+0xa5>
1000023e8: 89 d8                       	movl	%ebx, %eax
1000023ea: c1 e8 06                    	shrl	$0x6, %eax
1000023ed: 49 8b 44 c7 60              	movq	0x60(%r15,%rax,8), %rax
1000023f2: 48 0f a3 d8                 	btq	%rbx, %rax
1000023f6: 73 1d                       	jae	0x100002415 <_mb_win_threads_tls_get+0xa5>
1000023f8: 89 d8                       	movl	%ebx, %eax
1000023fa: 49 8b 5c c6 20              	movq	0x20(%r14,%rax,8), %rbx
1000023ff: 41 c7 46 1c 00 00 00 00     	movl	$0x0, 0x1c(%r14)
100002407: 4c 89 ff                    	movq	%r15, %rdi
10000240a: e8 af 62 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000240f: 85 c0                       	testl	%eax, %eax
100002411: 74 18                       	je	0x10000242b <_mb_win_threads_tls_get+0xbb>
100002413: eb 22                       	jmp	0x100002437 <_mb_win_threads_tls_get+0xc7>
100002415: 41 c7 46 1c 57 00 00 00     	movl	$0x57, 0x1c(%r14)
10000241d: 4c 89 ff                    	movq	%r15, %rdi
100002420: e8 99 62 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002425: 85 c0                       	testl	%eax, %eax
100002427: 75 0e                       	jne	0x100002437 <_mb_win_threads_tls_get+0xc7>
100002429: 31 db                       	xorl	%ebx, %ebx
10000242b: 48 89 d8                    	movq	%rbx, %rax
10000242e: 5b                          	popq	%rbx
10000242f: 41 5c                       	popq	%r12
100002431: 41 5e                       	popq	%r14
100002433: 41 5f                       	popq	%r15
100002435: 5d                          	popq	%rbp
100002436: c3                          	retq
100002437: e8 d4 61 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000243c: 0f 1f 40 00                 	nopl	(%rax)

0000000100002440 <_mb_win_threads_tls_set>:
100002440: 55                          	pushq	%rbp
100002441: 48 89 e5                    	movq	%rsp, %rbp
100002444: 41 57                       	pushq	%r15
100002446: 41 56                       	pushq	%r14
100002448: 41 55                       	pushq	%r13
10000244a: 41 54                       	pushq	%r12
10000244c: 53                          	pushq	%rbx
10000244d: 50                          	pushq	%rax
10000244e: 48 85 ff                    	testq	%rdi, %rdi
100002451: 0f 84 ba 00 00 00           	je	0x100002511 <_mb_win_threads_tls_set+0xd1>
100002457: 4c 8b 2f                    	movq	(%rdi), %r13
10000245a: 4d 85 ed                    	testq	%r13, %r13
10000245d: 0f 84 ae 00 00 00           	je	0x100002511 <_mb_win_threads_tls_set+0xd1>
100002463: 41 83 bd fc 00 00 00 00     	cmpl	$0x0, 0xfc(%r13)
10000246b: 0f 84 a0 00 00 00           	je	0x100002511 <_mb_win_threads_tls_set+0xd1>
100002471: 41 83 bd f8 00 00 00 00     	cmpl	$0x0, 0xf8(%r13)
100002479: 0f 85 92 00 00 00           	jne	0x100002511 <_mb_win_threads_tls_set+0xd1>
10000247f: 48 89 d3                    	movq	%rdx, %rbx
100002482: 41 89 f6                    	movl	%esi, %r14d
100002485: 49 8b 7d 40                 	movq	0x40(%r13), %rdi
100002489: e8 06 62 00 00              	callq	0x100008694 <_sysconf+0x100008694>
10000248e: 48 85 c0                    	testq	%rax, %rax
100002491: 74 7e                       	je	0x100002511 <_mb_win_threads_tls_set+0xd1>
100002493: 49 89 c7                    	movq	%rax, %r15
100002496: 4c 8b 20                    	movq	(%rax), %r12
100002499: 4d 39 ec                    	cmpq	%r13, %r12
10000249c: 75 73                       	jne	0x100002511 <_mb_win_threads_tls_set+0xd1>
10000249e: 4d 85 e4                    	testq	%r12, %r12
1000024a1: 74 6e                       	je	0x100002511 <_mb_win_threads_tls_set+0xd1>
1000024a3: 4c 89 e7                    	movq	%r12, %rdi
1000024a6: e8 0d 62 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000024ab: 85 c0                       	testl	%eax, %eax
1000024ad: 75 62                       	jne	0x100002511 <_mb_win_threads_tls_set+0xd1>
1000024af: 41 83 fe 40                 	cmpl	$0x40, %r14d
1000024b3: 72 1a                       	jb	0x1000024cf <_mb_win_threads_tls_set+0x8f>
1000024b5: 41 81 fe 3f 04 00 00        	cmpl	$0x43f, %r14d           ## imm = 0x43F
1000024bc: 77 3b                       	ja	0x1000024f9 <_mb_win_threads_tls_set+0xb9>
1000024be: 44 89 f0                    	movl	%r14d, %eax
1000024c1: c1 e8 06                    	shrl	$0x6, %eax
1000024c4: 49 8b 44 c4 60              	movq	0x60(%r12,%rax,8), %rax
1000024c9: 4c 0f a3 f0                 	btq	%r14, %rax
1000024cd: 73 2a                       	jae	0x1000024f9 <_mb_win_threads_tls_set+0xb9>
1000024cf: 44 89 f0                    	movl	%r14d, %eax
1000024d2: 49 89 5c c7 20              	movq	%rbx, 0x20(%r15,%rax,8)
1000024d7: 4c 89 e7                    	movq	%r12, %rdi
1000024da: e8 df 61 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000024df: 89 c1                       	movl	%eax, %ecx
1000024e1: b8 01 00 00 00              	movl	$0x1, %eax
1000024e6: 85 c9                       	testl	%ecx, %ecx
1000024e8: 75 27                       	jne	0x100002511 <_mb_win_threads_tls_set+0xd1>
1000024ea: 48 83 c4 08                 	addq	$0x8, %rsp
1000024ee: 5b                          	popq	%rbx
1000024ef: 41 5c                       	popq	%r12
1000024f1: 41 5d                       	popq	%r13
1000024f3: 41 5e                       	popq	%r14
1000024f5: 41 5f                       	popq	%r15
1000024f7: 5d                          	popq	%rbp
1000024f8: c3                          	retq
1000024f9: 41 c7 47 1c 57 00 00 00     	movl	$0x57, 0x1c(%r15)
100002501: 4c 89 e7                    	movq	%r12, %rdi
100002504: e8 b5 61 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002509: 89 c1                       	movl	%eax, %ecx
10000250b: 31 c0                       	xorl	%eax, %eax
10000250d: 85 c9                       	testl	%ecx, %ecx
10000250f: 74 d9                       	je	0x1000024ea <_mb_win_threads_tls_set+0xaa>
100002511: e8 fa 60 00 00              	callq	0x100008610 <_sysconf+0x100008610>
		...
10000251e: 00 00                       	addb	%al, (%rax)

0000000100002520 <_mb_win_workers_domain>:
100002520: 48 85 ff                    	testq	%rdi, %rdi
100002523: 74 15                       	je	0x10000253a <_mb_win_workers_domain+0x1a>
100002525: 48 8b 07                    	movq	(%rdi), %rax
100002528: 48 85 c0                    	testq	%rax, %rax
10000252b: 74 0d                       	je	0x10000253a <_mb_win_workers_domain+0x1a>
10000252d: 55                          	pushq	%rbp
10000252e: 48 89 e5                    	movq	%rsp, %rbp
100002531: 48 8b 80 80 00 00 00        	movq	0x80(%rax), %rax
100002538: 5d                          	popq	%rbp
100002539: c3                          	retq
10000253a: 31 c0                       	xorl	%eax, %eax
10000253c: c3                          	retq
10000253d: 0f 1f 00                    	nopl	(%rax)

0000000100002540 <_mb_win_workers_open>:
100002540: 48 85 ff                    	testq	%rdi, %rdi
100002543: 0f 94 c1                    	sete	%cl
100002546: 48 85 f6                    	testq	%rsi, %rsi
100002549: 41 0f 94 c0                 	sete	%r8b
10000254d: b8 01 00 00 00              	movl	$0x1, %eax
100002552: 41 08 c8                    	orb	%cl, %r8b
100002555: 75 44                       	jne	0x10000259b <_mb_win_workers_open+0x5b>
100002557: 55                          	pushq	%rbp
100002558: 48 89 e5                    	movq	%rsp, %rbp
10000255b: 41 57                       	pushq	%r15
10000255d: 41 56                       	pushq	%r14
10000255f: 41 55                       	pushq	%r13
100002561: 41 54                       	pushq	%r12
100002563: 53                          	pushq	%rbx
100002564: 50                          	pushq	%rax
100002565: 4c 8b 36                    	movq	(%rsi), %r14
100002568: 4d 85 f6                    	testq	%r14, %r14
10000256b: 74 20                       	je	0x10000258d <_mb_win_workers_open+0x4d>
10000256d: 48 83 7e 10 00              	cmpq	$0x0, 0x10(%rsi)
100002572: 74 19                       	je	0x10000258d <_mb_win_workers_open+0x4d>
100002574: 48 83 7e 18 00              	cmpq	$0x0, 0x18(%rsi)
100002579: 74 12                       	je	0x10000258d <_mb_win_workers_open+0x4d>
10000257b: 48 83 7e 20 00              	cmpq	$0x0, 0x20(%rsi)
100002580: 74 0b                       	je	0x10000258d <_mb_win_workers_open+0x4d>
100002582: b8 02 00 00 00              	movl	$0x2, %eax
100002587: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
10000258b: 74 0f                       	je	0x10000259c <_mb_win_workers_open+0x5c>
10000258d: 48 83 c4 08                 	addq	$0x8, %rsp
100002591: 5b                          	popq	%rbx
100002592: 41 5c                       	popq	%r12
100002594: 41 5d                       	popq	%r13
100002596: 41 5e                       	popq	%r14
100002598: 41 5f                       	popq	%r15
10000259a: 5d                          	popq	%rbp
10000259b: c3                          	retq
10000259c: 49 89 f5                    	movq	%rsi, %r13
10000259f: 49 89 fc                    	movq	%rdi, %r12
1000025a2: 49 89 d7                    	movq	%rdx, %r15
1000025a5: bf 01 00 00 00              	movl	$0x1, %edi
1000025aa: be f0 00 00 00              	movl	$0xf0, %esi
1000025af: e8 68 60 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
1000025b4: 48 85 c0                    	testq	%rax, %rax
1000025b7: 74 4c                       	je	0x100002605 <_mb_win_workers_open+0xc5>
1000025b9: 48 89 c3                    	movq	%rax, %rbx
1000025bc: 49 8b 45 20                 	movq	0x20(%r13), %rax
1000025c0: 48 89 83 a0 00 00 00        	movq	%rax, 0xa0(%rbx)
1000025c7: 41 0f 10 45 00              	movups	(%r13), %xmm0
1000025cc: 41 0f 10 4d 10              	movups	0x10(%r13), %xmm1
1000025d1: 0f 11 8b 90 00 00 00        	movups	%xmm1, 0x90(%rbx)
1000025d8: 0f 11 83 80 00 00 00        	movups	%xmm0, 0x80(%rbx)
1000025df: 4c 89 63 78                 	movq	%r12, 0x78(%rbx)
1000025e3: 48 8d b3 a8 00 00 00        	leaq	0xa8(%rbx), %rsi
1000025ea: 4c 89 f7                    	movq	%r14, %rdi
1000025ed: e8 ae f9 ff ff              	callq	0x100001fa0 <_mb_win_threads_acquire>
1000025f2: 85 c0                       	testl	%eax, %eax
1000025f4: 74 29                       	je	0x10000261f <_mb_win_workers_open+0xdf>
1000025f6: 48 89 df                    	movq	%rbx, %rdi
1000025f9: e8 3c 60 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
1000025fe: b8 02 00 00 00              	movl	$0x2, %eax
100002603: eb 88                       	jmp	0x10000258d <_mb_win_workers_open+0x4d>
100002605: b8 04 00 00 00              	movl	$0x4, %eax
10000260a: 4d 85 ff                    	testq	%r15, %r15
10000260d: 0f 84 7a ff ff ff           	je	0x10000258d <_mb_win_workers_open+0x4d>
100002613: 41 c7 07 0c 00 00 00        	movl	$0xc, (%r15)
10000261a: e9 6e ff ff ff              	jmp	0x10000258d <_mb_win_workers_open+0x4d>
10000261f: 48 89 df                    	movq	%rbx, %rdi
100002622: 31 f6                       	xorl	%esi, %esi
100002624: e8 89 60 00 00              	callq	0x1000086b2 <_sysconf+0x1000086b2>
100002629: 85 c0                       	testl	%eax, %eax
10000262b: 74 39                       	je	0x100002666 <_mb_win_workers_open+0x126>
10000262d: 85 c0                       	testl	%eax, %eax
10000262f: 74 4e                       	je	0x10000267f <_mb_win_workers_open+0x13f>
100002631: 85 c0                       	testl	%eax, %eax
100002633: 74 66                       	je	0x10000269b <_mb_win_workers_open+0x15b>
100002635: 41 89 c6                    	movl	%eax, %r14d
100002638: 4c 89 e7                    	movq	%r12, %rdi
10000263b: 49 89 1c 24                 	movq	%rbx, (%r12)
10000263f: c7 83 e0 00 00 00 01 00 00 00       	movl	$0x1, 0xe0(%rbx)
100002649: 31 f6                       	xorl	%esi, %esi
10000264b: e8 70 00 00 00              	callq	0x1000026c0 <_mb_win_workers_close>
100002650: b8 04 00 00 00              	movl	$0x4, %eax
100002655: 4d 85 ff                    	testq	%r15, %r15
100002658: 0f 84 2f ff ff ff           	je	0x10000258d <_mb_win_workers_open+0x4d>
10000265e: 45 89 37                    	movl	%r14d, (%r15)
100002661: e9 27 ff ff ff              	jmp	0x10000258d <_mb_win_workers_open+0x4d>
100002666: c7 83 e4 00 00 00 01 00 00 00       	movl	$0x1, 0xe4(%rbx)
100002670: 48 8d 7b 40                 	leaq	0x40(%rbx), %rdi
100002674: 31 f6                       	xorl	%esi, %esi
100002676: e8 e9 5f 00 00              	callq	0x100008664 <_sysconf+0x100008664>
10000267b: 85 c0                       	testl	%eax, %eax
10000267d: 75 b2                       	jne	0x100002631 <_mb_win_workers_open+0xf1>
10000267f: c7 83 e8 00 00 00 01 00 00 00       	movl	$0x1, 0xe8(%rbx)
100002689: 48 89 df                    	movq	%rbx, %rdi
10000268c: 48 83 c7 70                 	addq	$0x70, %rdi
100002690: 31 f6                       	xorl	%esi, %esi
100002692: e8 09 60 00 00              	callq	0x1000086a0 <_sysconf+0x1000086a0>
100002697: 85 c0                       	testl	%eax, %eax
100002699: 75 9a                       	jne	0x100002635 <_mb_win_workers_open+0xf5>
10000269b: c7 83 ec 00 00 00 01 00 00 00       	movl	$0x1, 0xec(%rbx)
1000026a5: 49 89 1c 24                 	movq	%rbx, (%r12)
1000026a9: 31 c0                       	xorl	%eax, %eax
1000026ab: 4d 85 ff                    	testq	%r15, %r15
1000026ae: 0f 84 d9 fe ff ff           	je	0x10000258d <_mb_win_workers_open+0x4d>
1000026b4: 41 c7 07 00 00 00 00        	movl	$0x0, (%r15)
1000026bb: e9 cd fe ff ff              	jmp	0x10000258d <_mb_win_workers_open+0x4d>

00000001000026c0 <_mb_win_workers_close>:
1000026c0: 55                          	pushq	%rbp
1000026c1: 48 89 e5                    	movq	%rsp, %rbp
1000026c4: 41 57                       	pushq	%r15
1000026c6: 41 56                       	pushq	%r14
1000026c8: 53                          	pushq	%rbx
1000026c9: 50                          	pushq	%rax
1000026ca: 48 85 ff                    	testq	%rdi, %rdi
1000026cd: 74 62                       	je	0x100002731 <_mb_win_workers_close+0x71>
1000026cf: 48 89 f3                    	movq	%rsi, %rbx
1000026d2: 4c 8b 37                    	movq	(%rdi), %r14
1000026d5: 4d 85 f6                    	testq	%r14, %r14
1000026d8: 0f 84 3f 01 00 00           	je	0x10000281d <_mb_win_workers_close+0x15d>
1000026de: 49 89 ff                    	movq	%rdi, %r15
1000026e1: 41 83 be e4 00 00 00 00     	cmpl	$0x0, 0xe4(%r14)
1000026e9: 74 67                       	je	0x100002752 <_mb_win_workers_close+0x92>
1000026eb: 4c 89 f7                    	movq	%r14, %rdi
1000026ee: e8 c5 5f 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000026f3: 85 c0                       	testl	%eax, %eax
1000026f5: 75 35                       	jne	0x10000272c <_mb_win_workers_close+0x6c>
1000026f7: 49 83 be c8 00 00 00 00     	cmpq	$0x0, 0xc8(%r14)
1000026ff: 75 14                       	jne	0x100002715 <_mb_win_workers_close+0x55>
100002701: 49 83 be d0 00 00 00 00     	cmpq	$0x0, 0xd0(%r14)
100002709: 75 0a                       	jne	0x100002715 <_mb_win_workers_close+0x55>
10000270b: 49 83 be d8 00 00 00 00     	cmpq	$0x0, 0xd8(%r14)
100002713: 74 26                       	je	0x10000273b <_mb_win_workers_close+0x7b>
100002715: 4c 89 f7                    	movq	%r14, %rdi
100002718: e8 a1 5f 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000271d: 89 c1                       	movl	%eax, %ecx
10000271f: b8 03 00 00 00              	movl	$0x3, %eax
100002724: 85 c9                       	testl	%ecx, %ecx
100002726: 0f 84 fe 00 00 00           	je	0x10000282a <_mb_win_workers_close+0x16a>
10000272c: e8 df 5e 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002731: b8 01 00 00 00              	movl	$0x1, %eax
100002736: e9 ef 00 00 00              	jmp	0x10000282a <_mb_win_workers_close+0x16a>
10000273b: 41 c7 86 e0 00 00 00 01 00 00 00    	movl	$0x1, 0xe0(%r14)
100002746: 4c 89 f7                    	movq	%r14, %rdi
100002749: e8 70 5f 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000274e: 85 c0                       	testl	%eax, %eax
100002750: 75 da                       	jne	0x10000272c <_mb_win_workers_close+0x6c>
100002752: 41 83 be ec 00 00 00 00     	cmpl	$0x0, 0xec(%r14)
10000275a: 74 30                       	je	0x10000278c <_mb_win_workers_close+0xcc>
10000275c: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002760: e8 41 5f 00 00              	callq	0x1000086a6 <_sysconf+0x1000086a6>
100002765: 89 c1                       	movl	%eax, %ecx
100002767: 85 c0                       	testl	%eax, %eax
100002769: 74 09                       	je	0x100002774 <_mb_win_workers_close+0xb4>
10000276b: 48 85 db                    	testq	%rbx, %rbx
10000276e: 74 0f                       	je	0x10000277f <_mb_win_workers_close+0xbf>
100002770: 89 0b                       	movl	%ecx, (%rbx)
100002772: eb 0b                       	jmp	0x10000277f <_mb_win_workers_close+0xbf>
100002774: 41 c7 86 ec 00 00 00 00 00 00 00    	movl	$0x0, 0xec(%r14)
10000277f: b8 04 00 00 00              	movl	$0x4, %eax
100002784: 85 c9                       	testl	%ecx, %ecx
100002786: 0f 85 9e 00 00 00           	jne	0x10000282a <_mb_win_workers_close+0x16a>
10000278c: 41 83 be e8 00 00 00 00     	cmpl	$0x0, 0xe8(%r14)
100002794: 74 2c                       	je	0x1000027c2 <_mb_win_workers_close+0x102>
100002796: 49 8d 7e 40                 	leaq	0x40(%r14), %rdi
10000279a: e8 bf 5e 00 00              	callq	0x10000865e <_sysconf+0x10000865e>
10000279f: 89 c1                       	movl	%eax, %ecx
1000027a1: 85 c0                       	testl	%eax, %eax
1000027a3: 74 09                       	je	0x1000027ae <_mb_win_workers_close+0xee>
1000027a5: 48 85 db                    	testq	%rbx, %rbx
1000027a8: 74 0f                       	je	0x1000027b9 <_mb_win_workers_close+0xf9>
1000027aa: 89 0b                       	movl	%ecx, (%rbx)
1000027ac: eb 0b                       	jmp	0x1000027b9 <_mb_win_workers_close+0xf9>
1000027ae: 41 c7 86 e8 00 00 00 00 00 00 00    	movl	$0x0, 0xe8(%r14)
1000027b9: b8 04 00 00 00              	movl	$0x4, %eax
1000027be: 85 c9                       	testl	%ecx, %ecx
1000027c0: 75 68                       	jne	0x10000282a <_mb_win_workers_close+0x16a>
1000027c2: 41 83 be e4 00 00 00 00     	cmpl	$0x0, 0xe4(%r14)
1000027ca: 74 2b                       	je	0x1000027f7 <_mb_win_workers_close+0x137>
1000027cc: 4c 89 f7                    	movq	%r14, %rdi
1000027cf: e8 d8 5e 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
1000027d4: 89 c1                       	movl	%eax, %ecx
1000027d6: 85 c0                       	testl	%eax, %eax
1000027d8: 74 09                       	je	0x1000027e3 <_mb_win_workers_close+0x123>
1000027da: 48 85 db                    	testq	%rbx, %rbx
1000027dd: 74 0f                       	je	0x1000027ee <_mb_win_workers_close+0x12e>
1000027df: 89 0b                       	movl	%ecx, (%rbx)
1000027e1: eb 0b                       	jmp	0x1000027ee <_mb_win_workers_close+0x12e>
1000027e3: 41 c7 86 e4 00 00 00 00 00 00 00    	movl	$0x0, 0xe4(%r14)
1000027ee: b8 04 00 00 00              	movl	$0x4, %eax
1000027f3: 85 c9                       	testl	%ecx, %ecx
1000027f5: 75 33                       	jne	0x10000282a <_mb_win_workers_close+0x16a>
1000027f7: 49 8d be a8 00 00 00        	leaq	0xa8(%r14), %rdi
1000027fe: e8 6d f8 ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100002803: 89 c1                       	movl	%eax, %ecx
100002805: b8 02 00 00 00              	movl	$0x2, %eax
10000280a: 85 c9                       	testl	%ecx, %ecx
10000280c: 75 1c                       	jne	0x10000282a <_mb_win_workers_close+0x16a>
10000280e: 4c 89 f7                    	movq	%r14, %rdi
100002811: e8 24 5e 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100002816: 49 c7 07 00 00 00 00        	movq	$0x0, (%r15)
10000281d: 48 85 db                    	testq	%rbx, %rbx
100002820: 74 06                       	je	0x100002828 <_mb_win_workers_close+0x168>
100002822: c7 03 00 00 00 00           	movl	$0x0, (%rbx)
100002828: 31 c0                       	xorl	%eax, %eax
10000282a: 48 83 c4 08                 	addq	$0x8, %rsp
10000282e: 5b                          	popq	%rbx
10000282f: 41 5e                       	popq	%r14
100002831: 41 5f                       	popq	%r15
100002833: 5d                          	popq	%rbp
100002834: c3                          	retq
100002835: 66 66 2e 0f 1f 84 00 00 00 00 00    	nopw	%cs:(%rax,%rax)

0000000100002840 <_mb_win_workers_acquire>:
100002840: 55                          	pushq	%rbp
100002841: 48 89 e5                    	movq	%rsp, %rbp
100002844: 41 57                       	pushq	%r15
100002846: 41 56                       	pushq	%r14
100002848: 53                          	pushq	%rbx
100002849: 50                          	pushq	%rax
10000284a: 48 85 ff                    	testq	%rdi, %rdi
10000284d: 0f 94 c1                    	sete	%cl
100002850: 48 85 f6                    	testq	%rsi, %rsi
100002853: 0f 94 c2                    	sete	%dl
100002856: b8 01 00 00 00              	movl	$0x1, %eax
10000285b: 08 ca                       	orb	%cl, %dl
10000285d: 75 49                       	jne	0x1000028a8 <_mb_win_workers_acquire+0x68>
10000285f: 48 8b 1f                    	movq	(%rdi), %rbx
100002862: b8 02 00 00 00              	movl	$0x2, %eax
100002867: 48 85 db                    	testq	%rbx, %rbx
10000286a: 74 3c                       	je	0x1000028a8 <_mb_win_workers_acquire+0x68>
10000286c: 49 89 ff                    	movq	%rdi, %r15
10000286f: 49 89 f6                    	movq	%rsi, %r14
100002872: 48 89 df                    	movq	%rbx, %rdi
100002875: e8 3e 5e 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
10000287a: 85 c0                       	testl	%eax, %eax
10000287c: 75 71                       	jne	0x1000028ef <_mb_win_workers_acquire+0xaf>
10000287e: 83 bb e0 00 00 00 00        	cmpl	$0x0, 0xe0(%rbx)
100002885: 75 10                       	jne	0x100002897 <_mb_win_workers_acquire+0x57>
100002887: 4c 89 f0                    	movq	%r14, %rax
10000288a: 49 83 3e 00                 	cmpq	$0x0, (%r14)
10000288e: 75 07                       	jne	0x100002897 <_mb_win_workers_acquire+0x57>
100002890: 48 83 78 08 00              	cmpq	$0x0, 0x8(%rax)
100002895: 74 1c                       	je	0x1000028b3 <_mb_win_workers_acquire+0x73>
100002897: 48 89 df                    	movq	%rbx, %rdi
10000289a: e8 1f 5e 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000289f: 85 c0                       	testl	%eax, %eax
1000028a1: b8 02 00 00 00              	movl	$0x2, %eax
1000028a6: 75 47                       	jne	0x1000028ef <_mb_win_workers_acquire+0xaf>
1000028a8: 48 83 c4 08                 	addq	$0x8, %rsp
1000028ac: 5b                          	popq	%rbx
1000028ad: 41 5e                       	popq	%r14
1000028af: 41 5f                       	popq	%r15
1000028b1: 5d                          	popq	%rbp
1000028b2: c3                          	retq
1000028b3: 48 8b 93 d0 00 00 00        	movq	0xd0(%rbx), %rdx
1000028ba: 48 83 fa ff                 	cmpq	$-0x1, %rdx
1000028be: 74 d7                       	je	0x100002897 <_mb_win_workers_acquire+0x57>
1000028c0: 4c 89 38                    	movq	%r15, (%rax)
1000028c3: 48 8b 8b b8 00 00 00        	movq	0xb8(%rbx), %rcx
1000028ca: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
1000028ce: 48 89 83 b8 00 00 00        	movq	%rax, 0xb8(%rbx)
1000028d5: 48 ff c2                    	incq	%rdx
1000028d8: 48 89 93 d0 00 00 00        	movq	%rdx, 0xd0(%rbx)
1000028df: 48 89 df                    	movq	%rbx, %rdi
1000028e2: e8 d7 5d 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000028e7: 89 c1                       	movl	%eax, %ecx
1000028e9: 31 c0                       	xorl	%eax, %eax
1000028eb: 85 c9                       	testl	%ecx, %ecx
1000028ed: 74 b9                       	je	0x1000028a8 <_mb_win_workers_acquire+0x68>
1000028ef: e8 1c 5d 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000028f4: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100002900 <_mb_win_workers_release_pin>:
100002900: 55                          	pushq	%rbp
100002901: 48 89 e5                    	movq	%rsp, %rbp
100002904: 41 56                       	pushq	%r14
100002906: 53                          	pushq	%rbx
100002907: 48 85 ff                    	testq	%rdi, %rdi
10000290a: 0f 84 8e 00 00 00           	je	0x10000299e <_mb_win_workers_release_pin+0x9e>
100002910: 48 8b 0f                    	movq	(%rdi), %rcx
100002913: b8 02 00 00 00              	movl	$0x2, %eax
100002918: 48 85 c9                    	testq	%rcx, %rcx
10000291b: 0f 84 95 00 00 00           	je	0x1000029b6 <_mb_win_workers_release_pin+0xb6>
100002921: 48 8b 19                    	movq	(%rcx), %rbx
100002924: 48 85 db                    	testq	%rbx, %rbx
100002927: 0f 84 89 00 00 00           	je	0x1000029b6 <_mb_win_workers_release_pin+0xb6>
10000292d: 49 89 fe                    	movq	%rdi, %r14
100002930: 48 89 df                    	movq	%rbx, %rdi
100002933: e8 80 5d 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100002938: 85 c0                       	testl	%eax, %eax
10000293a: 75 7f                       	jne	0x1000029bb <_mb_win_workers_release_pin+0xbb>
10000293c: 48 8d b3 b8 00 00 00        	leaq	0xb8(%rbx), %rsi
100002943: 4c 89 f0                    	movq	%r14, %rax
100002946: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100002950: 48 89 f1                    	movq	%rsi, %rcx
100002953: 48 8b 16                    	movq	(%rsi), %rdx
100002956: 48 85 d2                    	testq	%rdx, %rdx
100002959: 74 09                       	je	0x100002964 <_mb_win_workers_release_pin+0x64>
10000295b: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
10000295f: 48 39 c2                    	cmpq	%rax, %rdx
100002962: 75 ec                       	jne	0x100002950 <_mb_win_workers_release_pin+0x50>
100002964: 48 85 d2                    	testq	%rdx, %rdx
100002967: 74 3c                       	je	0x1000029a5 <_mb_win_workers_release_pin+0xa5>
100002969: 48 8b 93 d0 00 00 00        	movq	0xd0(%rbx), %rdx
100002970: 48 85 d2                    	testq	%rdx, %rdx
100002973: 74 30                       	je	0x1000029a5 <_mb_win_workers_release_pin+0xa5>
100002975: 48 8b 70 08                 	movq	0x8(%rax), %rsi
100002979: 48 89 31                    	movq	%rsi, (%rcx)
10000297c: 48 ff ca                    	decq	%rdx
10000297f: 48 89 93 d0 00 00 00        	movq	%rdx, 0xd0(%rbx)
100002986: 0f 57 c0                    	xorps	%xmm0, %xmm0
100002989: 0f 11 00                    	movups	%xmm0, (%rax)
10000298c: 48 89 df                    	movq	%rbx, %rdi
10000298f: e8 2a 5d 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002994: 89 c1                       	movl	%eax, %ecx
100002996: 31 c0                       	xorl	%eax, %eax
100002998: 85 c9                       	testl	%ecx, %ecx
10000299a: 74 1a                       	je	0x1000029b6 <_mb_win_workers_release_pin+0xb6>
10000299c: eb 1d                       	jmp	0x1000029bb <_mb_win_workers_release_pin+0xbb>
10000299e: b8 01 00 00 00              	movl	$0x1, %eax
1000029a3: eb 11                       	jmp	0x1000029b6 <_mb_win_workers_release_pin+0xb6>
1000029a5: 48 89 df                    	movq	%rbx, %rdi
1000029a8: e8 11 5d 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000029ad: 85 c0                       	testl	%eax, %eax
1000029af: b8 02 00 00 00              	movl	$0x2, %eax
1000029b4: 75 05                       	jne	0x1000029bb <_mb_win_workers_release_pin+0xbb>
1000029b6: 5b                          	popq	%rbx
1000029b7: 41 5e                       	popq	%r14
1000029b9: 5d                          	popq	%rbp
1000029ba: c3                          	retq
1000029bb: e8 50 5c 00 00              	callq	0x100008610 <_sysconf+0x100008610>

00000001000029c0 <_mb_win_workers_enter_call>:
1000029c0: 55                          	pushq	%rbp
1000029c1: 48 89 e5                    	movq	%rsp, %rbp
1000029c4: 41 57                       	pushq	%r15
1000029c6: 41 56                       	pushq	%r14
1000029c8: 41 54                       	pushq	%r12
1000029ca: 53                          	pushq	%rbx
1000029cb: 48 85 ff                    	testq	%rdi, %rdi
1000029ce: 0f 84 a5 00 00 00           	je	0x100002a79 <_mb_win_workers_enter_call+0xb9>
1000029d4: 48 8b 1f                    	movq	(%rdi), %rbx
1000029d7: 48 85 db                    	testq	%rbx, %rbx
1000029da: 0f 84 99 00 00 00           	je	0x100002a79 <_mb_win_workers_enter_call+0xb9>
1000029e0: 48 8b bb 80 00 00 00        	movq	0x80(%rbx), %rdi
1000029e7: e8 34 f7 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000029ec: 48 8b 7b 70                 	movq	0x70(%rbx), %rdi
1000029f0: e8 9f 5c 00 00              	callq	0x100008694 <_sysconf+0x100008694>
1000029f5: 49 89 c7                    	movq	%rax, %r15
1000029f8: 41 be 02 00 00 00           	movl	$0x2, %r14d
1000029fe: 41 83 ff ff                 	cmpl	$-0x1, %r15d
100002a02: 74 69                       	je	0x100002a6d <_mb_win_workers_enter_call+0xad>
100002a04: 48 89 df                    	movq	%rbx, %rdi
100002a07: e8 ac 5c 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100002a0c: 85 c0                       	testl	%eax, %eax
100002a0e: 75 69                       	jne	0x100002a79 <_mb_win_workers_enter_call+0xb9>
100002a10: 83 bb e0 00 00 00 00        	cmpl	$0x0, 0xe0(%rbx)
100002a17: 74 0e                       	je	0x100002a27 <_mb_win_workers_enter_call+0x67>
100002a19: 48 89 df                    	movq	%rbx, %rdi
100002a1c: e8 9d 5c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002a21: 85 c0                       	testl	%eax, %eax
100002a23: 74 48                       	je	0x100002a6d <_mb_win_workers_enter_call+0xad>
100002a25: eb 52                       	jmp	0x100002a79 <_mb_win_workers_enter_call+0xb9>
100002a27: 45 85 ff                    	testl	%r15d, %r15d
100002a2a: 75 0a                       	jne	0x100002a36 <_mb_win_workers_enter_call+0x76>
100002a2c: 48 83 bb d8 00 00 00 ff     	cmpq	$-0x1, 0xd8(%rbx)
100002a34: 74 e3                       	je	0x100002a19 <_mb_win_workers_enter_call+0x59>
100002a36: 48 8b 7b 70                 	movq	0x70(%rbx), %rdi
100002a3a: 44 89 fe                    	movl	%r15d, %esi
100002a3d: ff c6                       	incl	%esi
100002a3f: e8 86 5c 00 00              	callq	0x1000086ca <_sysconf+0x1000086ca>
100002a44: 41 89 c4                    	movl	%eax, %r12d
100002a47: 44 09 f8                    	orl	%r15d, %eax
100002a4a: 75 07                       	jne	0x100002a53 <_mb_win_workers_enter_call+0x93>
100002a4c: 48 ff 83 d8 00 00 00        	incq	0xd8(%rbx)
100002a53: 48 89 df                    	movq	%rbx, %rdi
100002a56: e8 63 5c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002a5b: 85 c0                       	testl	%eax, %eax
100002a5d: 75 1a                       	jne	0x100002a79 <_mb_win_workers_enter_call+0xb9>
100002a5f: 45 31 f6                    	xorl	%r14d, %r14d
100002a62: 45 85 e4                    	testl	%r12d, %r12d
100002a65: 41 0f 95 c6                 	setne	%r14b
100002a69: 41 c1 e6 02                 	shll	$0x2, %r14d
100002a6d: 44 89 f0                    	movl	%r14d, %eax
100002a70: 5b                          	popq	%rbx
100002a71: 41 5c                       	popq	%r12
100002a73: 41 5e                       	popq	%r14
100002a75: 41 5f                       	popq	%r15
100002a77: 5d                          	popq	%rbp
100002a78: c3                          	retq
100002a79: e8 92 5b 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002a7e: 66 90                       	nop

0000000100002a80 <_mb_win_workers_leave_call>:
100002a80: 55                          	pushq	%rbp
100002a81: 48 89 e5                    	movq	%rsp, %rbp
100002a84: 41 57                       	pushq	%r15
100002a86: 41 56                       	pushq	%r14
100002a88: 53                          	pushq	%rbx
100002a89: 50                          	pushq	%rax
100002a8a: 48 85 ff                    	testq	%rdi, %rdi
100002a8d: 0f 84 80 00 00 00           	je	0x100002b13 <_mb_win_workers_leave_call+0x93>
100002a93: 48 8b 1f                    	movq	(%rdi), %rbx
100002a96: 48 85 db                    	testq	%rbx, %rbx
100002a99: 74 78                       	je	0x100002b13 <_mb_win_workers_leave_call+0x93>
100002a9b: 48 8b 7b 70                 	movq	0x70(%rbx), %rdi
100002a9f: e8 f0 5b 00 00              	callq	0x100008694 <_sysconf+0x100008694>
100002aa4: 49 89 c7                    	movq	%rax, %r15
100002aa7: 45 85 ff                    	testl	%r15d, %r15d
100002aaa: 74 57                       	je	0x100002b03 <_mb_win_workers_leave_call+0x83>
100002aac: 48 89 df                    	movq	%rbx, %rdi
100002aaf: e8 04 5c 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100002ab4: 85 c0                       	testl	%eax, %eax
100002ab6: 75 5b                       	jne	0x100002b13 <_mb_win_workers_leave_call+0x93>
100002ab8: 48 8b 7b 70                 	movq	0x70(%rbx), %rdi
100002abc: 44 89 fe                    	movl	%r15d, %esi
100002abf: ff ce                       	decl	%esi
100002ac1: e8 04 5c 00 00              	callq	0x1000086ca <_sysconf+0x1000086ca>
100002ac6: 41 89 c6                    	movl	%eax, %r14d
100002ac9: 41 83 ff 01                 	cmpl	$0x1, %r15d
100002acd: 75 1b                       	jne	0x100002aea <_mb_win_workers_leave_call+0x6a>
100002acf: 45 85 f6                    	testl	%r14d, %r14d
100002ad2: 75 16                       	jne	0x100002aea <_mb_win_workers_leave_call+0x6a>
100002ad4: 48 8b 83 d8 00 00 00        	movq	0xd8(%rbx), %rax
100002adb: 48 85 c0                    	testq	%rax, %rax
100002ade: 74 33                       	je	0x100002b13 <_mb_win_workers_leave_call+0x93>
100002ae0: 48 ff c8                    	decq	%rax
100002ae3: 48 89 83 d8 00 00 00        	movq	%rax, 0xd8(%rbx)
100002aea: 48 89 df                    	movq	%rbx, %rdi
100002aed: e8 cc 5b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002af2: 85 c0                       	testl	%eax, %eax
100002af4: 75 1d                       	jne	0x100002b13 <_mb_win_workers_leave_call+0x93>
100002af6: 31 c0                       	xorl	%eax, %eax
100002af8: 45 85 f6                    	testl	%r14d, %r14d
100002afb: 0f 95 c0                    	setne	%al
100002afe: c1 e0 02                    	shll	$0x2, %eax
100002b01: eb 05                       	jmp	0x100002b08 <_mb_win_workers_leave_call+0x88>
100002b03: b8 02 00 00 00              	movl	$0x2, %eax
100002b08: 48 83 c4 08                 	addq	$0x8, %rsp
100002b0c: 5b                          	popq	%rbx
100002b0d: 41 5e                       	popq	%r14
100002b0f: 41 5f                       	popq	%r15
100002b11: 5d                          	popq	%rbp
100002b12: c3                          	retq
100002b13: e8 f8 5a 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002b18: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100002b20 <_mb_win_workers_suspend_calls>:
100002b20: 55                          	pushq	%rbp
100002b21: 48 89 e5                    	movq	%rsp, %rbp
100002b24: 41 56                       	pushq	%r14
100002b26: 53                          	pushq	%rbx
100002b27: 48 85 ff                    	testq	%rdi, %rdi
100002b2a: 74 2a                       	je	0x100002b56 <_mb_win_workers_suspend_calls+0x36>
100002b2c: 4c 8b 37                    	movq	(%rdi), %r14
100002b2f: 4d 85 f6                    	testq	%r14, %r14
100002b32: 74 22                       	je	0x100002b56 <_mb_win_workers_suspend_calls+0x36>
100002b34: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002b38: e8 57 5b 00 00              	callq	0x100008694 <_sysconf+0x100008694>
100002b3d: 48 89 c3                    	movq	%rax, %rbx
100002b40: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002b44: 31 f6                       	xorl	%esi, %esi
100002b46: e8 7f 5b 00 00              	callq	0x1000086ca <_sysconf+0x1000086ca>
100002b4b: 85 c0                       	testl	%eax, %eax
100002b4d: 75 07                       	jne	0x100002b56 <_mb_win_workers_suspend_calls+0x36>
100002b4f: 89 d8                       	movl	%ebx, %eax
100002b51: 5b                          	popq	%rbx
100002b52: 41 5e                       	popq	%r14
100002b54: 5d                          	popq	%rbp
100002b55: c3                          	retq
100002b56: e8 b5 5a 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002b5b: 0f 1f 44 00 00              	nopl	(%rax,%rax)

0000000100002b60 <_mb_win_workers_restore_calls>:
100002b60: 55                          	pushq	%rbp
100002b61: 48 89 e5                    	movq	%rsp, %rbp
100002b64: 41 56                       	pushq	%r14
100002b66: 53                          	pushq	%rbx
100002b67: 48 85 ff                    	testq	%rdi, %rdi
100002b6a: 74 2b                       	je	0x100002b97 <_mb_win_workers_restore_calls+0x37>
100002b6c: 4c 8b 37                    	movq	(%rdi), %r14
100002b6f: 4d 85 f6                    	testq	%r14, %r14
100002b72: 74 23                       	je	0x100002b97 <_mb_win_workers_restore_calls+0x37>
100002b74: 89 f3                       	movl	%esi, %ebx
100002b76: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002b7a: e8 15 5b 00 00              	callq	0x100008694 <_sysconf+0x100008694>
100002b7f: 85 c0                       	testl	%eax, %eax
100002b81: 75 14                       	jne	0x100002b97 <_mb_win_workers_restore_calls+0x37>
100002b83: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002b87: 89 de                       	movl	%ebx, %esi
100002b89: e8 3c 5b 00 00              	callq	0x1000086ca <_sysconf+0x1000086ca>
100002b8e: 85 c0                       	testl	%eax, %eax
100002b90: 75 05                       	jne	0x100002b97 <_mb_win_workers_restore_calls+0x37>
100002b92: 5b                          	popq	%rbx
100002b93: 41 5e                       	popq	%r14
100002b95: 5d                          	popq	%rbp
100002b96: c3                          	retq
100002b97: e8 74 5a 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002b9c: 0f 1f 40 00                 	nopl	(%rax)

0000000100002ba0 <_mb_win_workers_collect>:
100002ba0: 55                          	pushq	%rbp
100002ba1: 48 89 e5                    	movq	%rsp, %rbp
100002ba4: 41 57                       	pushq	%r15
100002ba6: 41 56                       	pushq	%r14
100002ba8: 41 55                       	pushq	%r13
100002baa: 41 54                       	pushq	%r12
100002bac: 53                          	pushq	%rbx
100002bad: 48 83 ec 18                 	subq	$0x18, %rsp
100002bb1: 48 85 ff                    	testq	%rdi, %rdi
100002bb4: 0f 84 a9 01 00 00           	je	0x100002d63 <_mb_win_workers_collect+0x1c3>
100002bba: 48 8b 1f                    	movq	(%rdi), %rbx
100002bbd: 48 85 db                    	testq	%rbx, %rbx
100002bc0: 0f 84 a5 01 00 00           	je	0x100002d6b <_mb_win_workers_collect+0x1cb>
100002bc6: 49 89 f6                    	movq	%rsi, %r14
100002bc9: 48 89 df                    	movq	%rbx, %rdi
100002bcc: e8 e7 5a 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100002bd1: 85 c0                       	testl	%eax, %eax
100002bd3: 0f 85 bb 01 00 00           	jne	0x100002d94 <_mb_win_workers_collect+0x1f4>
100002bd9: 4c 8b a3 c0 00 00 00        	movq	0xc0(%rbx), %r12
100002be0: 4d 85 e4                    	testq	%r12, %r12
100002be3: 0f 84 8a 01 00 00           	je	0x100002d73 <_mb_win_workers_collect+0x1d3>
100002be9: 4c 89 75 c8                 	movq	%r14, -0x38(%rbp)
100002bed: 48 8d 83 c0 00 00 00        	leaq	0xc0(%rbx), %rax
100002bf4: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
100002bf8: 45 31 ff                    	xorl	%r15d, %r15d
100002bfb: 0f 1f 44 00 00              	nopl	(%rax,%rax)
100002c00: 41 83 7c 24 48 00           	cmpl	$0x0, 0x48(%r12)
100002c06: 75 08                       	jne	0x100002c10 <_mb_win_workers_collect+0x70>
100002c08: 41 83 7c 24 68 00           	cmpl	$0x0, 0x68(%r12)
100002c0e: 74 15                       	je	0x100002c25 <_mb_win_workers_collect+0x85>
100002c10: 4d 8b 64 24 08              	movq	0x8(%r12), %r12
100002c15: 41 bf 03 00 00 00           	movl	$0x3, %r15d
100002c1b: 4d 85 e4                    	testq	%r12, %r12
100002c1e: 75 e0                       	jne	0x100002c00 <_mb_win_workers_collect+0x60>
100002c20: e9 51 01 00 00              	jmp	0x100002d76 <_mb_win_workers_collect+0x1d6>
100002c25: 41 83 7c 24 5c 00           	cmpl	$0x0, 0x5c(%r12)
100002c2b: 74 e3                       	je	0x100002c10 <_mb_win_workers_collect+0x70>
100002c2d: 41 c7 44 24 68 01 00 00 00  	movl	$0x1, 0x68(%r12)
100002c36: 48 89 df                    	movq	%rbx, %rdi
100002c39: e8 80 5a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002c3e: 85 c0                       	testl	%eax, %eax
100002c40: 0f 85 4e 01 00 00           	jne	0x100002d94 <_mb_win_workers_collect+0x1f4>
100002c46: 45 31 ed                    	xorl	%r13d, %r13d
100002c49: 41 83 7c 24 4c 00           	cmpl	$0x0, 0x4c(%r12)
100002c4f: 74 08                       	je	0x100002c59 <_mb_win_workers_collect+0xb9>
100002c51: 41 83 7c 24 60 00           	cmpl	$0x0, 0x60(%r12)
100002c57: 74 0a                       	je	0x100002c63 <_mb_win_workers_collect+0xc3>
100002c59: 45 85 ed                    	testl	%r13d, %r13d
100002c5c: 74 19                       	je	0x100002c77 <_mb_win_workers_collect+0xd7>
100002c5e: 45 31 f6                    	xorl	%r14d, %r14d
100002c61: eb 31                       	jmp	0x100002c94 <_mb_win_workers_collect+0xf4>
100002c63: 49 8b 7c 24 10              	movq	0x10(%r12), %rdi
100002c68: 31 f6                       	xorl	%esi, %esi
100002c6a: e8 2b 5a 00 00              	callq	0x10000869a <_sysconf+0x10000869a>
100002c6f: 41 89 c5                    	movl	%eax, %r13d
100002c72: 45 85 ed                    	testl	%r13d, %r13d
100002c75: 75 e7                       	jne	0x100002c5e <_mb_win_workers_collect+0xbe>
100002c77: 49 8b 74 24 28              	movq	0x28(%r12), %rsi
100002c7c: 48 85 f6                    	testq	%rsi, %rsi
100002c7f: 74 dd                       	je	0x100002c5e <_mb_win_workers_collect+0xbe>
100002c81: 48 8b bb 88 00 00 00        	movq	0x88(%rbx), %rdi
100002c88: ff 93 98 00 00 00           	callq	*0x98(%rbx)
100002c8e: 85 c0                       	testl	%eax, %eax
100002c90: 41 0f 95 c6                 	setne	%r14b
100002c94: 48 89 df                    	movq	%rbx, %rdi
100002c97: e8 1c 5a 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100002c9c: 85 c0                       	testl	%eax, %eax
100002c9e: 0f 85 f0 00 00 00           	jne	0x100002d94 <_mb_win_workers_collect+0x1f4>
100002ca4: 45 85 ed                    	testl	%r13d, %r13d
100002ca7: 74 25                       	je	0x100002cce <_mb_win_workers_collect+0x12e>
100002ca9: 41 c7 44 24 68 00 00 00 00  	movl	$0x0, 0x68(%r12)
100002cb2: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
100002cb6: 48 85 c0                    	testq	%rax, %rax
100002cb9: 74 03                       	je	0x100002cbe <_mb_win_workers_collect+0x11e>
100002cbb: 44 89 28                    	movl	%r13d, (%rax)
100002cbe: 4d 8b 64 24 08              	movq	0x8(%r12), %r12
100002cc3: 41 bf 04 00 00 00           	movl	$0x4, %r15d
100002cc9: e9 4d ff ff ff              	jmp	0x100002c1b <_mb_win_workers_collect+0x7b>
100002cce: 41 c7 44 24 60 01 00 00 00  	movl	$0x1, 0x60(%r12)
100002cd7: 45 84 f6                    	testb	%r14b, %r14b
100002cda: 74 19                       	je	0x100002cf5 <_mb_win_workers_collect+0x155>
100002cdc: 41 c7 44 24 68 00 00 00 00  	movl	$0x0, 0x68(%r12)
100002ce5: 4d 8b 64 24 08              	movq	0x8(%r12), %r12
100002cea: 41 bf 05 00 00 00           	movl	$0x5, %r15d
100002cf0: e9 26 ff ff ff              	jmp	0x100002c1b <_mb_win_workers_collect+0x7b>
100002cf5: 49 c7 44 24 28 00 00 00 00  	movq	$0x0, 0x28(%r12)
100002cfe: 41 c7 44 24 68 00 00 00 00  	movl	$0x0, 0x68(%r12)
100002d07: 41 83 7c 24 64 00           	cmpl	$0x0, 0x64(%r12)
100002d0d: 75 08                       	jne	0x100002d17 <_mb_win_workers_collect+0x177>
100002d0f: 49 83 7c 24 40 00           	cmpq	$0x0, 0x40(%r12)
100002d15: 74 0a                       	je	0x100002d21 <_mb_win_workers_collect+0x181>
100002d17: 4d 8b 64 24 08              	movq	0x8(%r12), %r12
100002d1c: e9 fa fe ff ff              	jmp	0x100002c1b <_mb_win_workers_collect+0x7b>
100002d21: 48 8b 4d d0                 	movq	-0x30(%rbp), %rcx
100002d25: 66 66 2e 0f 1f 84 00 00 00 00 00    	nopw	%cs:(%rax,%rax)
100002d30: 48 89 c8                    	movq	%rcx, %rax
100002d33: 48 8b 11                    	movq	(%rcx), %rdx
100002d36: 48 8d 4a 08                 	leaq	0x8(%rdx), %rcx
100002d3a: 4c 39 e2                    	cmpq	%r12, %rdx
100002d3d: 75 f1                       	jne	0x100002d30 <_mb_win_workers_collect+0x190>
100002d3f: 49 8b 4c 24 08              	movq	0x8(%r12), %rcx
100002d44: 48 89 08                    	movq	%rcx, (%rax)
100002d47: 48 ff 8b c8 00 00 00        	decq	0xc8(%rbx)
100002d4e: 4d 8b 74 24 08              	movq	0x8(%r12), %r14
100002d53: 4c 89 e7                    	movq	%r12, %rdi
100002d56: e8 df 58 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100002d5b: 4d 89 f4                    	movq	%r14, %r12
100002d5e: e9 b8 fe ff ff              	jmp	0x100002c1b <_mb_win_workers_collect+0x7b>
100002d63: 41 bf 01 00 00 00           	movl	$0x1, %r15d
100002d69: eb 17                       	jmp	0x100002d82 <_mb_win_workers_collect+0x1e2>
100002d6b: 41 bf 02 00 00 00           	movl	$0x2, %r15d
100002d71: eb 0f                       	jmp	0x100002d82 <_mb_win_workers_collect+0x1e2>
100002d73: 45 31 ff                    	xorl	%r15d, %r15d
100002d76: 48 89 df                    	movq	%rbx, %rdi
100002d79: e8 40 59 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002d7e: 85 c0                       	testl	%eax, %eax
100002d80: 75 12                       	jne	0x100002d94 <_mb_win_workers_collect+0x1f4>
100002d82: 44 89 f8                    	movl	%r15d, %eax
100002d85: 48 83 c4 18                 	addq	$0x18, %rsp
100002d89: 5b                          	popq	%rbx
100002d8a: 41 5c                       	popq	%r12
100002d8c: 41 5d                       	popq	%r13
100002d8e: 41 5e                       	popq	%r14
100002d90: 41 5f                       	popq	%r15
100002d92: 5d                          	popq	%rbp
100002d93: c3                          	retq
100002d94: e8 77 58 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002d99: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100002da0 <_mb_win_workers_create>:
100002da0: 55                          	pushq	%rbp
100002da1: 48 89 e5                    	movq	%rsp, %rbp
100002da4: 41 57                       	pushq	%r15
100002da6: 41 56                       	pushq	%r14
100002da8: 41 55                       	pushq	%r13
100002daa: 41 54                       	pushq	%r12
100002dac: 53                          	pushq	%rbx
100002dad: 48 83 ec 18                 	subq	$0x18, %rsp
100002db1: 48 89 4d d0                 	movq	%rcx, -0x30(%rbp)
100002db5: 48 85 ff                    	testq	%rdi, %rdi
100002db8: 0f 84 30 02 00 00           	je	0x100002fee <_mb_win_workers_create+0x24e>
100002dbe: 4c 8b 37                    	movq	(%rdi), %r14
100002dc1: 4d 85 f6                    	testq	%r14, %r14
100002dc4: 0f 84 24 02 00 00           	je	0x100002fee <_mb_win_workers_create+0x24e>
100002dca: 45 89 cf                    	movl	%r9d, %r15d
100002dcd: 4d 89 c5                    	movq	%r8, %r13
100002dd0: 49 89 d4                    	movq	%rdx, %r12
100002dd3: 48 89 f3                    	movq	%rsi, %rbx
100002dd6: 48 89 7d c8                 	movq	%rdi, -0x38(%rbp)
100002dda: 49 8b be 80 00 00 00        	movq	0x80(%r14), %rdi
100002de1: e8 3a f3 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100002de6: 4c 09 e3                    	orq	%r12, %rbx
100002de9: 0f 94 c1                    	sete	%cl
100002dec: 45 85 ff                    	testl	%r15d, %r15d
100002def: 0f 94 c2                    	sete	%dl
100002df2: 84 d1                       	testb	%dl, %cl
100002df4: 75 0b                       	jne	0x100002e01 <_mb_win_workers_create+0x61>
100002df6: c7 00 32 00 00 00           	movl	$0x32, (%rax)
100002dfc: e9 83 01 00 00              	jmp	0x100002f84 <_mb_win_workers_create+0x1e4>
100002e01: 48 83 7d d0 00              	cmpq	$0x0, -0x30(%rbp)
100002e06: 0f 84 9b 00 00 00           	je	0x100002ea7 <_mb_win_workers_create+0x107>
100002e0c: 49 89 c4                    	movq	%rax, %r12
100002e0f: 8b 18                       	movl	(%rax), %ebx
100002e11: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002e15: e8 7a 58 00 00              	callq	0x100008694 <_sysconf+0x100008694>
100002e1a: 85 c0                       	testl	%eax, %eax
100002e1c: 0f 84 90 00 00 00           	je	0x100002eb2 <_mb_win_workers_create+0x112>
100002e22: bf 01 00 00 00              	movl	$0x1, %edi
100002e27: be 70 00 00 00              	movl	$0x70, %esi
100002e2c: e8 eb 57 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
100002e31: 48 85 c0                    	testq	%rax, %rax
100002e34: 0f 84 42 01 00 00           	je	0x100002f7c <_mb_win_workers_create+0x1dc>
100002e3a: 49 89 c7                    	movq	%rax, %r15
100002e3d: 4c 89 30                    	movq	%r14, (%rax)
100002e40: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
100002e44: 49 89 47 18                 	movq	%rax, 0x18(%r15)
100002e48: 4d 89 6f 20                 	movq	%r13, 0x20(%r15)
100002e4c: 41 c7 47 48 01 00 00 00     	movl	$0x1, 0x48(%r15)
100002e54: 4c 89 f7                    	movq	%r14, %rdi
100002e57: e8 5c 58 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100002e5c: 85 c0                       	testl	%eax, %eax
100002e5e: 0f 85 8a 01 00 00           	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002e64: 41 83 be e0 00 00 00 00     	cmpl	$0x0, 0xe0(%r14)
100002e6c: 0f 85 f6 00 00 00           	jne	0x100002f68 <_mb_win_workers_create+0x1c8>
100002e72: 49 8b 86 c8 00 00 00        	movq	0xc8(%r14), %rax
100002e79: 49 3b 86 a0 00 00 00        	cmpq	0xa0(%r14), %rax
100002e80: 0f 83 e2 00 00 00           	jae	0x100002f68 <_mb_win_workers_create+0x1c8>
100002e86: 8b 05 74 81 00 00           	movl	0x8174(%rip), %eax      ## 0x10000b000 <_next_identity>
100002e8c: 0f 1f 40 00                 	nopl	(%rax)
100002e90: 8d 48 ff                    	leal	-0x1(%rax), %ecx
100002e93: 83 f9 fd                    	cmpl	$-0x3, %ecx
100002e96: 77 27                       	ja	0x100002ebf <_mb_win_workers_create+0x11f>
100002e98: 8d 48 01                    	leal	0x1(%rax), %ecx
100002e9b: f0                          	lock
100002e9c: 0f b1 0d 5d 81 00 00        	cmpxchgl	%ecx, 0x815d(%rip)      ## 0x10000b000 <_next_identity>
100002ea3: 75 eb                       	jne	0x100002e90 <_mb_win_workers_create+0xf0>
100002ea5: eb 1a                       	jmp	0x100002ec1 <_mb_win_workers_create+0x121>
100002ea7: c7 00 57 00 00 00           	movl	$0x57, (%rax)
100002ead: e9 d2 00 00 00              	jmp	0x100002f84 <_mb_win_workers_create+0x1e4>
100002eb2: 41 c7 04 24 aa 00 00 00     	movl	$0xaa, (%r12)
100002eba: e9 c5 00 00 00              	jmp	0x100002f84 <_mb_win_workers_create+0x1e4>
100002ebf: 31 c0                       	xorl	%eax, %eax
100002ec1: 41 89 47 38                 	movl	%eax, 0x38(%r15)
100002ec5: 85 c0                       	testl	%eax, %eax
100002ec7: 0f 84 9b 00 00 00           	je	0x100002f68 <_mb_win_workers_create+0x1c8>
100002ecd: 89 c0                       	movl	%eax, %eax
100002ecf: 48 8d 04 85 01 00 00 00     	leaq	0x1(,%rax,4), %rax
100002ed7: 49 89 47 30                 	movq	%rax, 0x30(%r15)
100002edb: 49 8b 86 c0 00 00 00        	movq	0xc0(%r14), %rax
100002ee2: 49 89 47 08                 	movq	%rax, 0x8(%r15)
100002ee6: 4d 89 be c0 00 00 00        	movq	%r15, 0xc0(%r14)
100002eed: 49 ff 86 c8 00 00 00        	incq	0xc8(%r14)
100002ef4: 4c 89 f7                    	movq	%r14, %rdi
100002ef7: e8 c2 57 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002efc: 85 c0                       	testl	%eax, %eax
100002efe: 0f 85 ea 00 00 00           	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002f04: 49 8b be 88 00 00 00        	movq	0x88(%r14), %rdi
100002f0b: 4d 8d 6f 28                 	leaq	0x28(%r15), %r13
100002f0f: 48 8b 75 d0                 	movq	-0x30(%rbp), %rsi
100002f13: 4c 89 ea                    	movq	%r13, %rdx
100002f16: 41 ff 96 90 00 00 00        	callq	*0x90(%r14)
100002f1d: 85 c0                       	testl	%eax, %eax
100002f1f: 74 74                       	je	0x100002f95 <_mb_win_workers_create+0x1f5>
100002f21: 4c 89 f7                    	movq	%r14, %rdi
100002f24: e8 8f 57 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100002f29: 85 c0                       	testl	%eax, %eax
100002f2b: 0f 85 bd 00 00 00           	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002f31: 41 c7 47 5c 01 00 00 00     	movl	$0x1, 0x5c(%r15)
100002f39: 41 c7 47 48 00 00 00 00     	movl	$0x0, 0x48(%r15)
100002f41: 4c 89 f7                    	movq	%r14, %rdi
100002f44: e8 75 57 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002f49: 85 c0                       	testl	%eax, %eax
100002f4b: 0f 85 9d 00 00 00           	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002f51: 48 8b 7d c8                 	movq	-0x38(%rbp), %rdi
100002f55: 31 f6                       	xorl	%esi, %esi
100002f57: e8 44 fc ff ff              	callq	0x100002ba0 <_mb_win_workers_collect>
100002f5c: 31 c0                       	xorl	%eax, %eax
100002f5e: 41 c7 04 24 08 00 00 00     	movl	$0x8, (%r12)
100002f66: eb 1e                       	jmp	0x100002f86 <_mb_win_workers_create+0x1e6>
100002f68: 4c 89 f7                    	movq	%r14, %rdi
100002f6b: e8 4e 57 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100002f70: 85 c0                       	testl	%eax, %eax
100002f72: 75 7a                       	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002f74: 4c 89 ff                    	movq	%r15, %rdi
100002f77: e8 be 56 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100002f7c: 41 c7 04 24 08 00 00 00     	movl	$0x8, (%r12)
100002f84: 31 c0                       	xorl	%eax, %eax
100002f86: 48 83 c4 18                 	addq	$0x18, %rsp
100002f8a: 5b                          	popq	%rbx
100002f8b: 41 5c                       	popq	%r12
100002f8d: 41 5d                       	popq	%r13
100002f8f: 41 5e                       	popq	%r14
100002f91: 41 5f                       	popq	%r15
100002f93: 5d                          	popq	%rbp
100002f94: c3                          	retq
100002f95: 49 83 7d 00 00              	cmpq	$0x0, (%r13)
100002f9a: 74 85                       	je	0x100002f21 <_mb_win_workers_create+0x181>
100002f9c: 4c 89 ff                    	movq	%r15, %rdi
100002f9f: 48 83 c7 10                 	addq	$0x10, %rdi
100002fa3: 48 8d 15 b6 00 00 00        	leaq	0xb6(%rip), %rdx        ## 0x100003060 <_run_worker>
100002faa: 31 f6                       	xorl	%esi, %esi
100002fac: 4c 89 f9                    	movq	%r15, %rcx
100002faf: e8 d4 56 00 00              	callq	0x100008688 <_sysconf+0x100008688>
100002fb4: 41 89 c5                    	movl	%eax, %r13d
100002fb7: 4c 89 f7                    	movq	%r14, %rdi
100002fba: e8 f9 56 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100002fbf: 85 c0                       	testl	%eax, %eax
100002fc1: 75 2b                       	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002fc3: 45 85 ed                    	testl	%r13d, %r13d
100002fc6: 0f 85 65 ff ff ff           	jne	0x100002f31 <_mb_win_workers_create+0x191>
100002fcc: 41 c7 47 4c 01 00 00 00     	movl	$0x1, 0x4c(%r15)
100002fd4: 4d 8d 6e 40                 	leaq	0x40(%r14), %r13
100002fd8: 41 83 7f 50 00              	cmpl	$0x0, 0x50(%r15)
100002fdd: 75 14                       	jne	0x100002ff3 <_mb_win_workers_create+0x253>
100002fdf: 4c 89 ef                    	movq	%r13, %rdi
100002fe2: 4c 89 f6                    	movq	%r14, %rsi
100002fe5: e8 8c 56 00 00              	callq	0x100008676 <_sysconf+0x100008676>
100002fea: 85 c0                       	testl	%eax, %eax
100002fec: 74 ea                       	je	0x100002fd8 <_mb_win_workers_create+0x238>
100002fee: e8 1d 56 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100002ff3: 41 83 7f 54 00              	cmpl	$0x0, 0x54(%r15)
100002ff8: 0f 85 3b ff ff ff           	jne	0x100002f39 <_mb_win_workers_create+0x199>
100002ffe: 41 c7 47 64 01 00 00 00     	movl	$0x1, 0x64(%r15)
100003006: 41 c7 47 58 01 00 00 00     	movl	$0x1, 0x58(%r15)
10000300e: 41 c7 47 48 00 00 00 00     	movl	$0x0, 0x48(%r15)
100003016: 49 8b 47 30                 	movq	0x30(%r15), %rax
10000301a: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
10000301e: 45 8b 7f 38                 	movl	0x38(%r15), %r15d
100003022: 4c 89 ef                    	movq	%r13, %rdi
100003025: e8 2e 56 00 00              	callq	0x100008658 <_sysconf+0x100008658>
10000302a: 85 c0                       	testl	%eax, %eax
10000302c: 75 c0                       	jne	0x100002fee <_mb_win_workers_create+0x24e>
10000302e: 4c 89 f7                    	movq	%r14, %rdi
100003031: e8 88 56 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003036: 85 c0                       	testl	%eax, %eax
100003038: 75 b4                       	jne	0x100002fee <_mb_win_workers_create+0x24e>
10000303a: 48 8b 4d 10                 	movq	0x10(%rbp), %rcx
10000303e: 48 85 c9                    	testq	%rcx, %rcx
100003041: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
100003045: 74 03                       	je	0x10000304a <_mb_win_workers_create+0x2aa>
100003047: 44 89 39                    	movl	%r15d, (%rcx)
10000304a: 41 89 1c 24                 	movl	%ebx, (%r12)
10000304e: e9 33 ff ff ff              	jmp	0x100002f86 <_mb_win_workers_create+0x1e6>
100003053: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)

0000000100003060 <_run_worker>:
100003060: 55                          	pushq	%rbp
100003061: 48 89 e5                    	movq	%rsp, %rbp
100003064: 41 57                       	pushq	%r15
100003066: 41 56                       	pushq	%r14
100003068: 41 55                       	pushq	%r13
10000306a: 41 54                       	pushq	%r12
10000306c: 53                          	pushq	%rbx
10000306d: 50                          	pushq	%rax
10000306e: 49 89 fe                    	movq	%rdi, %r14
100003071: 48 8b 1f                    	movq	(%rdi), %rbx
100003074: c7 45 d4 00 00 00 00        	movl	$0x0, -0x2c(%rbp)
10000307b: 48 8b bb 80 00 00 00        	movq	0x80(%rbx), %rdi
100003082: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100003086: e8 05 eb ff ff              	callq	0x100001b90 <_mb_win_threads_enter>
10000308b: 41 89 c7                    	movl	%eax, %r15d
10000308e: 85 c0                       	testl	%eax, %eax
100003090: 74 05                       	je	0x100003097 <_run_worker+0x37>
100003092: 45 31 ed                    	xorl	%r13d, %r13d
100003095: eb 0f                       	jmp	0x1000030a6 <_run_worker+0x46>
100003097: 48 8b 7b 78                 	movq	0x78(%rbx), %rdi
10000309b: e8 20 f9 ff ff              	callq	0x1000029c0 <_mb_win_workers_enter_call>
1000030a0: 85 c0                       	testl	%eax, %eax
1000030a2: 41 0f 94 c5                 	sete	%r13b
1000030a6: 48 89 df                    	movq	%rbx, %rdi
1000030a9: e8 0a 56 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000030ae: 85 c0                       	testl	%eax, %eax
1000030b0: 75 54                       	jne	0x100003106 <_run_worker+0xa6>
1000030b2: 41 c7 46 50 01 00 00 00     	movl	$0x1, 0x50(%r14)
1000030ba: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
1000030bd: 85 c0                       	testl	%eax, %eax
1000030bf: b9 0c 00 00 00              	movl	$0xc, %ecx
1000030c4: 0f 45 c8                    	cmovnel	%eax, %ecx
1000030c7: 31 c0                       	xorl	%eax, %eax
1000030c9: 45 84 ed                    	testb	%r13b, %r13b
1000030cc: 0f 44 c1                    	cmovel	%ecx, %eax
1000030cf: 41 89 46 54                 	movl	%eax, 0x54(%r14)
1000030d3: 4c 8d 63 40                 	leaq	0x40(%rbx), %r12
1000030d7: 4c 89 e7                    	movq	%r12, %rdi
1000030da: e8 79 55 00 00              	callq	0x100008658 <_sysconf+0x100008658>
1000030df: 85 c0                       	testl	%eax, %eax
1000030e1: 75 23                       	jne	0x100003106 <_run_worker+0xa6>
1000030e3: 45 84 ed                    	testb	%r13b, %r13b
1000030e6: 74 23                       	je	0x10000310b <_run_worker+0xab>
1000030e8: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)
1000030f0: 41 83 7e 58 00              	cmpl	$0x0, 0x58(%r14)
1000030f5: 75 22                       	jne	0x100003119 <_run_worker+0xb9>
1000030f7: 4c 89 e7                    	movq	%r12, %rdi
1000030fa: 48 89 de                    	movq	%rbx, %rsi
1000030fd: e8 74 55 00 00              	callq	0x100008676 <_sysconf+0x100008676>
100003102: 85 c0                       	testl	%eax, %eax
100003104: 74 ea                       	je	0x1000030f0 <_run_worker+0x90>
100003106: e8 05 55 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000310b: 48 89 df                    	movq	%rbx, %rdi
10000310e: e8 ab 55 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003113: 85 c0                       	testl	%eax, %eax
100003115: 74 76                       	je	0x10000318d <_run_worker+0x12d>
100003117: eb ed                       	jmp	0x100003106 <_run_worker+0xa6>
100003119: 48 89 df                    	movq	%rbx, %rdi
10000311c: e8 9d 55 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003121: 85 c0                       	testl	%eax, %eax
100003123: 75 e1                       	jne	0x100003106 <_run_worker+0xa6>
100003125: 45 84 ed                    	testb	%r13b, %r13b
100003128: 74 63                       	je	0x10000318d <_run_worker+0x12d>
10000312a: 49 8b 7e 18                 	movq	0x18(%r14), %rdi
10000312e: 49 8b 76 20                 	movq	0x20(%r14), %rsi
100003132: e8 19 d5 ff ff              	callq	0x100000650 <_mb_win_workers_invoke>
100003137: 41 89 c5                    	movl	%eax, %r13d
10000313a: 48 8b 7b 78                 	movq	0x78(%rbx), %rdi
10000313e: e8 3d f9 ff ff              	callq	0x100002a80 <_mb_win_workers_leave_call>
100003143: 85 c0                       	testl	%eax, %eax
100003145: 75 bf                       	jne	0x100003106 <_run_worker+0xa6>
100003147: 45 85 ff                    	testl	%r15d, %r15d
10000314a: 74 49                       	je	0x100003195 <_run_worker+0x135>
10000314c: 48 89 df                    	movq	%rbx, %rdi
10000314f: e8 64 55 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003154: 85 c0                       	testl	%eax, %eax
100003156: 75 ae                       	jne	0x100003106 <_run_worker+0xa6>
100003158: 45 89 6e 3c                 	movl	%r13d, 0x3c(%r14)
10000315c: 41 c7 46 5c 01 00 00 00     	movl	$0x1, 0x5c(%r14)
100003164: 4c 89 e7                    	movq	%r12, %rdi
100003167: e8 ec 54 00 00              	callq	0x100008658 <_sysconf+0x100008658>
10000316c: 85 c0                       	testl	%eax, %eax
10000316e: 75 96                       	jne	0x100003106 <_run_worker+0xa6>
100003170: 48 89 df                    	movq	%rbx, %rdi
100003173: e8 46 55 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003178: 85 c0                       	testl	%eax, %eax
10000317a: 75 8a                       	jne	0x100003106 <_run_worker+0xa6>
10000317c: 31 c0                       	xorl	%eax, %eax
10000317e: 48 83 c4 08                 	addq	$0x8, %rsp
100003182: 5b                          	popq	%rbx
100003183: 41 5c                       	popq	%r12
100003185: 41 5d                       	popq	%r13
100003187: 41 5e                       	popq	%r14
100003189: 41 5f                       	popq	%r15
10000318b: 5d                          	popq	%rbp
10000318c: c3                          	retq
10000318d: 45 31 ed                    	xorl	%r13d, %r13d
100003190: 45 85 ff                    	testl	%r15d, %r15d
100003193: 75 b7                       	jne	0x10000314c <_run_worker+0xec>
100003195: 48 8b bb 80 00 00 00        	movq	0x80(%rbx), %rdi
10000319c: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
1000031a0: e8 1b eb ff ff              	callq	0x100001cc0 <_mb_win_threads_leave>
1000031a5: eb a5                       	jmp	0x10000314c <_run_worker+0xec>
1000031a7: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)

00000001000031b0 <_mb_win_workers_wait>:
1000031b0: 55                          	pushq	%rbp
1000031b1: 48 89 e5                    	movq	%rsp, %rbp
1000031b4: 41 57                       	pushq	%r15
1000031b6: 41 56                       	pushq	%r14
1000031b8: 41 55                       	pushq	%r13
1000031ba: 41 54                       	pushq	%r12
1000031bc: 53                          	pushq	%rbx
1000031bd: 48 83 ec 38                 	subq	$0x38, %rsp
1000031c1: 48 85 ff                    	testq	%rdi, %rdi
1000031c4: 0f 84 b9 02 00 00           	je	0x100003483 <_mb_win_workers_wait+0x2d3>
1000031ca: 4c 8b 37                    	movq	(%rdi), %r14
1000031cd: 4d 85 f6                    	testq	%r14, %r14
1000031d0: 0f 84 ad 02 00 00           	je	0x100003483 <_mb_win_workers_wait+0x2d3>
1000031d6: 41 89 d7                    	movl	%edx, %r15d
1000031d9: 49 89 f4                    	movq	%rsi, %r12
1000031dc: 49 8b be 80 00 00 00        	movq	0x80(%r14), %rdi
1000031e3: e8 38 ef ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000031e8: 49 89 c5                    	movq	%rax, %r13
1000031eb: 8b 00                       	movl	(%rax), %eax
1000031ed: 89 45 c8                    	movl	%eax, -0x38(%rbp)
1000031f0: 4c 89 f7                    	movq	%r14, %rdi
1000031f3: e8 c0 54 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000031f8: 85 c0                       	testl	%eax, %eax
1000031fa: 0f 85 83 02 00 00           	jne	0x100003483 <_mb_win_workers_wait+0x2d3>
100003200: 49 8b 9e c0 00 00 00        	movq	0xc0(%r14), %rbx
100003207: eb 0b                       	jmp	0x100003214 <_mb_win_workers_wait+0x64>
100003209: 0f 1f 80 00 00 00 00        	nopl	(%rax)
100003210: 48 8b 5b 08                 	movq	0x8(%rbx), %rbx
100003214: 48 85 db                    	testq	%rbx, %rbx
100003217: 74 0c                       	je	0x100003225 <_mb_win_workers_wait+0x75>
100003219: 83 7b 64 00                 	cmpl	$0x0, 0x64(%rbx)
10000321d: 74 f1                       	je	0x100003210 <_mb_win_workers_wait+0x60>
10000321f: 4c 39 63 30                 	cmpq	%r12, 0x30(%rbx)
100003223: 75 eb                       	jne	0x100003210 <_mb_win_workers_wait+0x60>
100003225: 48 85 db                    	testq	%rbx, %rbx
100003228: 4c 89 6d a0                 	movq	%r13, -0x60(%rbp)
10000322c: 74 60                       	je	0x10000328e <_mb_win_workers_wait+0xde>
10000322e: 48 ff 43 40                 	incq	0x40(%rbx)
100003232: 41 8d 47 ff                 	leal	-0x1(%r15), %eax
100003236: 48 c7 45 d0 00 00 00 00     	movq	$0x0, -0x30(%rbp)
10000323e: 49 bc 09 fa 82 4b 04 00 00 00       	movabsq	$0x44b82fa09, %r12 ## imm = 0x44B82FA09
100003248: 83 f8 fd                    	cmpl	$-0x3, %eax
10000324b: 77 61                       	ja	0x1000032ae <_mb_win_workers_wait+0xfe>
10000324d: 83 7b 5c 00                 	cmpl	$0x0, 0x5c(%rbx)
100003251: 41 bd 00 00 00 00           	movl	$0x0, %r13d
100003257: 0f 85 9d 00 00 00           	jne	0x1000032fa <_mb_win_workers_wait+0x14a>
10000325d: 48 8d 75 b0                 	leaq	-0x50(%rbp), %rsi
100003261: bf 06 00 00 00              	movl	$0x6, %edi
100003266: e8 b7 53 00 00              	callq	0x100008622 <_sysconf+0x100008622>
10000326b: 85 c0                       	testl	%eax, %eax
10000326d: 74 44                       	je	0x1000032b3 <_mb_win_workers_wait+0x103>
10000326f: e8 8a 53 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003274: 83 38 00                    	cmpl	$0x0, (%rax)
100003277: 74 57                       	je	0x1000032d0 <_mb_win_workers_wait+0x120>
100003279: e8 80 53 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
10000327e: 83 38 00                    	cmpl	$0x0, (%rax)
100003281: 0f 95 c0                    	setne	%al
100003284: 48 c7 45 d0 00 00 00 00     	movq	$0x0, -0x30(%rbp)
10000328c: eb 65                       	jmp	0x1000032f3 <_mb_win_workers_wait+0x143>
10000328e: 4c 89 f7                    	movq	%r14, %rdi
100003291: e8 28 54 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003296: 85 c0                       	testl	%eax, %eax
100003298: 0f 85 e5 01 00 00           	jne	0x100003483 <_mb_win_workers_wait+0x2d3>
10000329e: 41 bd ff ff ff ff           	movl	$0xffffffff, %r13d      ## imm = 0xFFFFFFFF
1000032a4: b9 06 00 00 00              	movl	$0x6, %ecx
1000032a9: e9 bd 01 00 00              	jmp	0x10000346b <_mb_win_workers_wait+0x2bb>
1000032ae: 45 31 ed                    	xorl	%r13d, %r13d
1000032b1: eb 47                       	jmp	0x1000032fa <_mb_win_workers_wait+0x14a>
1000032b3: 48 8b 4d b0                 	movq	-0x50(%rbp), %rcx
1000032b7: 48 8b 45 b8                 	movq	-0x48(%rbp), %rax
1000032bb: 48 3d 00 ca 9a 3b           	cmpq	$0x3b9aca00, %rax       ## imm = 0x3B9ACA00
1000032c1: 0f 93 c2                    	setae	%dl
1000032c4: 4c 39 e1                    	cmpq	%r12, %rcx
1000032c7: 40 0f 97 c6                 	seta	%sil
1000032cb: 40 08 d6                    	orb	%dl, %sil
1000032ce: 74 0c                       	je	0x1000032dc <_mb_win_workers_wait+0x12c>
1000032d0: b0 01                       	movb	$0x1, %al
1000032d2: 48 c7 45 d0 00 00 00 00     	movq	$0x0, -0x30(%rbp)
1000032da: eb 17                       	jmp	0x1000032f3 <_mb_win_workers_wait+0x143>
1000032dc: 48 69 c9 00 ca 9a 3b        	imulq	$0x3b9aca00, %rcx, %rcx ## imm = 0x3B9ACA00
1000032e3: 31 d2                       	xorl	%edx, %edx
1000032e5: 48 01 c1                    	addq	%rax, %rcx
1000032e8: 0f 92 c0                    	setb	%al
1000032eb: 48 0f 43 d1                 	cmovaeq	%rcx, %rdx
1000032ef: 48 89 55 d0                 	movq	%rdx, -0x30(%rbp)
1000032f3: 44 0f b6 e8                 	movzbl	%al, %r13d
1000032f7: 41 f7 dd                    	negl	%r13d
1000032fa: 4d 8d 66 40                 	leaq	0x40(%r14), %r12
1000032fe: 44 89 f8                    	movl	%r15d, %eax
100003301: 48 69 c0 40 42 0f 00        	imulq	$0xf4240, %rax, %rax    ## imm = 0xF4240
100003308: 48 89 45 a8                 	movq	%rax, -0x58(%rbp)
10000330c: 0f 1f 40 00                 	nopl	(%rax)
100003310: 45 85 ed                    	testl	%r13d, %r13d
100003313: 0f 85 33 01 00 00           	jne	0x10000344c <_mb_win_workers_wait+0x29c>
100003319: 83 7b 5c 00                 	cmpl	$0x0, 0x5c(%rbx)
10000331d: 0f 85 1e 01 00 00           	jne	0x100003441 <_mb_win_workers_wait+0x291>
100003323: 41 83 ff ff                 	cmpl	$-0x1, %r15d
100003327: 75 17                       	jne	0x100003340 <_mb_win_workers_wait+0x190>
100003329: 4c 89 e7                    	movq	%r12, %rdi
10000332c: 4c 89 f6                    	movq	%r14, %rsi
10000332f: e8 42 53 00 00              	callq	0x100008676 <_sysconf+0x100008676>
100003334: 45 31 ed                    	xorl	%r13d, %r13d
100003337: 85 c0                       	testl	%eax, %eax
100003339: 74 d5                       	je	0x100003310 <_mb_win_workers_wait+0x160>
10000333b: e9 06 01 00 00              	jmp	0x100003446 <_mb_win_workers_wait+0x296>
100003340: 41 bd 02 01 00 00           	movl	$0x102, %r13d           ## imm = 0x102
100003346: 45 85 ff                    	testl	%r15d, %r15d
100003349: 0f 84 fd 00 00 00           	je	0x10000344c <_mb_win_workers_wait+0x29c>
10000334f: bf 06 00 00 00              	movl	$0x6, %edi
100003354: 48 8d 75 b0                 	leaq	-0x50(%rbp), %rsi
100003358: e8 c5 52 00 00              	callq	0x100008622 <_sysconf+0x100008622>
10000335d: 85 c0                       	testl	%eax, %eax
10000335f: 74 17                       	je	0x100003378 <_mb_win_workers_wait+0x1c8>
100003361: e8 98 52 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003366: 83 38 00                    	cmpl	$0x0, (%rax)
100003369: 74 51                       	je	0x1000033bc <_mb_win_workers_wait+0x20c>
10000336b: e8 8e 52 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003370: 83 38 00                    	cmpl	$0x0, (%rax)
100003373: 0f 95 c1                    	setne	%cl
100003376: eb 46                       	jmp	0x1000033be <_mb_win_workers_wait+0x20e>
100003378: 48 8b 55 b0                 	movq	-0x50(%rbp), %rdx
10000337c: 48 8b 45 b8                 	movq	-0x48(%rbp), %rax
100003380: 48 3d 00 ca 9a 3b           	cmpq	$0x3b9aca00, %rax       ## imm = 0x3B9ACA00
100003386: 0f 93 c1                    	setae	%cl
100003389: 48 be 09 fa 82 4b 04 00 00 00       	movabsq	$0x44b82fa09, %rsi ## imm = 0x44B82FA09
100003393: 48 39 f2                    	cmpq	%rsi, %rdx
100003396: 40 0f 97 c6                 	seta	%sil
10000339a: 40 08 ce                    	orb	%cl, %sil
10000339d: b1 01                       	movb	$0x1, %cl
10000339f: 75 1d                       	jne	0x1000033be <_mb_win_workers_wait+0x20e>
1000033a1: 48 69 d2 00 ca 9a 3b        	imulq	$0x3b9aca00, %rdx, %rdx ## imm = 0x3B9ACA00
1000033a8: 48 01 c2                    	addq	%rax, %rdx
1000033ab: 0f 92 c1                    	setb	%cl
1000033ae: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
1000033b2: 48 0f 43 c2                 	cmovaeq	%rdx, %rax
1000033b6: 48 89 45 c0                 	movq	%rax, -0x40(%rbp)
1000033ba: eb 02                       	jmp	0x1000033be <_mb_win_workers_wait+0x20e>
1000033bc: b1 01                       	movb	$0x1, %cl
1000033be: 41 bd ff ff ff ff           	movl	$0xffffffff, %r13d      ## imm = 0xFFFFFFFF
1000033c4: 31 c0                       	xorl	%eax, %eax
1000033c6: 84 c9                       	testb	%cl, %cl
1000033c8: 75 5b                       	jne	0x100003425 <_mb_win_workers_wait+0x275>
1000033ca: 48 8b 4d c0                 	movq	-0x40(%rbp), %rcx
1000033ce: 48 2b 4d d0                 	subq	-0x30(%rbp), %rcx
1000033d2: 72 51                       	jb	0x100003425 <_mb_win_workers_wait+0x275>
1000033d4: 4c 8b 6d a8                 	movq	-0x58(%rbp), %r13
1000033d8: 49 29 cd                    	subq	%rcx, %r13
1000033db: 76 52                       	jbe	0x10000342f <_mb_win_workers_wait+0x27f>
1000033dd: 4c 89 f7                    	movq	%r14, %rdi
1000033e0: e8 d9 52 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000033e5: 85 c0                       	testl	%eax, %eax
1000033e7: 0f 85 96 00 00 00           	jne	0x100003483 <_mb_win_workers_wait+0x2d3>
1000033ed: 49 81 fd 40 42 0f 00        	cmpq	$0xf4240, %r13          ## imm = 0xF4240
1000033f4: b8 40 42 0f 00              	movl	$0xf4240, %eax          ## imm = 0xF4240
1000033f9: 4c 0f 43 e8                 	cmovaeq	%rax, %r13
1000033fd: 4c 89 ef                    	movq	%r13, %rdi
100003400: e8 8b 00 00 00              	callq	0x100003490 <_pause_ns>
100003405: 89 45 cc                    	movl	%eax, -0x34(%rbp)
100003408: 4c 89 f7                    	movq	%r14, %rdi
10000340b: e8 a8 52 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003410: 85 c0                       	testl	%eax, %eax
100003412: 75 6f                       	jne	0x100003483 <_mb_win_workers_wait+0x2d3>
100003414: 45 31 ed                    	xorl	%r13d, %r13d
100003417: 83 7d cc 00                 	cmpl	$0x0, -0x34(%rbp)
10000341b: 0f 94 c0                    	sete	%al
10000341e: 41 0f 95 c5                 	setne	%r13b
100003422: 41 f7 dd                    	negl	%r13d
100003425: 84 c0                       	testb	%al, %al
100003427: 0f 85 e3 fe ff ff           	jne	0x100003310 <_mb_win_workers_wait+0x160>
10000342d: eb 1d                       	jmp	0x10000344c <_mb_win_workers_wait+0x29c>
10000342f: 41 bd 02 01 00 00           	movl	$0x102, %r13d           ## imm = 0x102
100003435: 31 c0                       	xorl	%eax, %eax
100003437: 84 c0                       	testb	%al, %al
100003439: 0f 85 d1 fe ff ff           	jne	0x100003310 <_mb_win_workers_wait+0x160>
10000343f: eb 0b                       	jmp	0x10000344c <_mb_win_workers_wait+0x29c>
100003441: 45 31 ed                    	xorl	%r13d, %r13d
100003444: eb 06                       	jmp	0x10000344c <_mb_win_workers_wait+0x29c>
100003446: 41 bd ff ff ff ff           	movl	$0xffffffff, %r13d      ## imm = 0xFFFFFFFF
10000344c: 48 ff 4b 40                 	decq	0x40(%rbx)
100003450: 4c 89 f7                    	movq	%r14, %rdi
100003453: e8 66 52 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003458: 85 c0                       	testl	%eax, %eax
10000345a: 75 27                       	jne	0x100003483 <_mb_win_workers_wait+0x2d3>
10000345c: 41 83 fd ff                 	cmpl	$-0x1, %r13d
100003460: b8 1f 00 00 00              	movl	$0x1f, %eax
100003465: 8b 4d c8                    	movl	-0x38(%rbp), %ecx
100003468: 0f 44 c8                    	cmovel	%eax, %ecx
10000346b: 48 8b 45 a0                 	movq	-0x60(%rbp), %rax
10000346f: 89 08                       	movl	%ecx, (%rax)
100003471: 44 89 e8                    	movl	%r13d, %eax
100003474: 48 83 c4 38                 	addq	$0x38, %rsp
100003478: 5b                          	popq	%rbx
100003479: 41 5c                       	popq	%r12
10000347b: 41 5d                       	popq	%r13
10000347d: 41 5e                       	popq	%r14
10000347f: 41 5f                       	popq	%r15
100003481: 5d                          	popq	%rbp
100003482: c3                          	retq
100003483: e8 88 51 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100003488: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100003490 <_pause_ns>:
100003490: 55                          	pushq	%rbp
100003491: 48 89 e5                    	movq	%rsp, %rbp
100003494: 41 57                       	pushq	%r15
100003496: 41 56                       	pushq	%r14
100003498: 53                          	pushq	%rbx
100003499: 48 83 ec 28                 	subq	$0x28, %rsp
10000349d: 48 89 f8                    	movq	%rdi, %rax
1000034a0: 48 c1 e8 09                 	shrq	$0x9, %rax
1000034a4: 48 b9 53 5a 9b a0 2f b8 44 00       	movabsq	$0x44b82fa09b5a53, %rcx ## imm = 0x44B82FA09B5A53
1000034ae: 48 f7 e1                    	mulq	%rcx
1000034b1: 48 c1 ea 0b                 	shrq	$0xb, %rdx
1000034b5: 48 89 55 d0                 	movq	%rdx, -0x30(%rbp)
1000034b9: 48 69 c2 00 ca 9a 3b        	imulq	$0x3b9aca00, %rdx, %rax ## imm = 0x3B9ACA00
1000034c0: 48 29 c7                    	subq	%rax, %rdi
1000034c3: 48 89 7d d8                 	movq	%rdi, -0x28(%rbp)
1000034c7: 48 8d 7d d0                 	leaq	-0x30(%rbp), %rdi
1000034cb: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
1000034cf: e8 7e 51 00 00              	callq	0x100008652 <_sysconf+0x100008652>
1000034d4: 31 db                       	xorl	%ebx, %ebx
1000034d6: 85 c0                       	testl	%eax, %eax
1000034d8: 74 51                       	je	0x10000352b <_pause_ns+0x9b>
1000034da: 4c 8d 75 d0                 	leaq	-0x30(%rbp), %r14
1000034de: 4c 8d 7d c0                 	leaq	-0x40(%rbp), %r15
1000034e2: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
1000034f0: e8 09 51 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
1000034f5: 83 38 04                    	cmpl	$0x4, (%rax)
1000034f8: 75 19                       	jne	0x100003513 <_pause_ns+0x83>
1000034fa: 0f 10 45 c0                 	movups	-0x40(%rbp), %xmm0
1000034fe: 0f 29 45 d0                 	movaps	%xmm0, -0x30(%rbp)
100003502: 4c 89 f7                    	movq	%r14, %rdi
100003505: 4c 89 fe                    	movq	%r15, %rsi
100003508: e8 45 51 00 00              	callq	0x100008652 <_sysconf+0x100008652>
10000350d: 85 c0                       	testl	%eax, %eax
10000350f: 75 df                       	jne	0x1000034f0 <_pause_ns+0x60>
100003511: eb 18                       	jmp	0x10000352b <_pause_ns+0x9b>
100003513: e8 e6 50 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003518: 83 38 00                    	cmpl	$0x0, (%rax)
10000351b: 74 09                       	je	0x100003526 <_pause_ns+0x96>
10000351d: e8 dc 50 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003522: 8b 18                       	movl	(%rax), %ebx
100003524: eb 05                       	jmp	0x10000352b <_pause_ns+0x9b>
100003526: bb 05 00 00 00              	movl	$0x5, %ebx
10000352b: 89 d8                       	movl	%ebx, %eax
10000352d: 48 83 c4 28                 	addq	$0x28, %rsp
100003531: 5b                          	popq	%rbx
100003532: 41 5e                       	popq	%r14
100003534: 41 5f                       	popq	%r15
100003536: 5d                          	popq	%rbp
100003537: c3                          	retq
100003538: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100003540 <_mb_win_workers_exit_code>:
100003540: 55                          	pushq	%rbp
100003541: 48 89 e5                    	movq	%rsp, %rbp
100003544: 41 57                       	pushq	%r15
100003546: 41 56                       	pushq	%r14
100003548: 41 54                       	pushq	%r12
10000354a: 53                          	pushq	%rbx
10000354b: 48 85 ff                    	testq	%rdi, %rdi
10000354e: 0f 84 a7 00 00 00           	je	0x1000035fb <_mb_win_workers_exit_code+0xbb>
100003554: 4c 8b 37                    	movq	(%rdi), %r14
100003557: 4d 85 f6                    	testq	%r14, %r14
10000355a: 0f 84 9b 00 00 00           	je	0x1000035fb <_mb_win_workers_exit_code+0xbb>
100003560: 48 89 d3                    	movq	%rdx, %rbx
100003563: 49 89 f4                    	movq	%rsi, %r12
100003566: 49 8b be 80 00 00 00        	movq	0x80(%r14), %rdi
10000356d: e8 ae eb ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100003572: 49 89 c7                    	movq	%rax, %r15
100003575: 48 85 db                    	testq	%rbx, %rbx
100003578: 74 3c                       	je	0x1000035b6 <_mb_win_workers_exit_code+0x76>
10000357a: 4c 89 f7                    	movq	%r14, %rdi
10000357d: e8 36 51 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003582: 85 c0                       	testl	%eax, %eax
100003584: 75 75                       	jne	0x1000035fb <_mb_win_workers_exit_code+0xbb>
100003586: 49 8b 86 c0 00 00 00        	movq	0xc0(%r14), %rax
10000358d: eb 05                       	jmp	0x100003594 <_mb_win_workers_exit_code+0x54>
10000358f: 90                          	nop
100003590: 48 8b 40 08                 	movq	0x8(%rax), %rax
100003594: 48 85 c0                    	testq	%rax, %rax
100003597: 74 0c                       	je	0x1000035a5 <_mb_win_workers_exit_code+0x65>
100003599: 83 78 64 00                 	cmpl	$0x0, 0x64(%rax)
10000359d: 74 f1                       	je	0x100003590 <_mb_win_workers_exit_code+0x50>
10000359f: 4c 39 60 30                 	cmpq	%r12, 0x30(%rax)
1000035a3: 75 eb                       	jne	0x100003590 <_mb_win_workers_exit_code+0x50>
1000035a5: 48 85 c0                    	testq	%rax, %rax
1000035a8: 74 17                       	je	0x1000035c1 <_mb_win_workers_exit_code+0x81>
1000035aa: 83 78 5c 00                 	cmpl	$0x0, 0x5c(%rax)
1000035ae: 74 28                       	je	0x1000035d8 <_mb_win_workers_exit_code+0x98>
1000035b0: 44 8b 78 3c                 	movl	0x3c(%rax), %r15d
1000035b4: eb 28                       	jmp	0x1000035de <_mb_win_workers_exit_code+0x9e>
1000035b6: 41 c7 07 57 00 00 00        	movl	$0x57, (%r15)
1000035bd: 31 c0                       	xorl	%eax, %eax
1000035bf: eb 31                       	jmp	0x1000035f2 <_mb_win_workers_exit_code+0xb2>
1000035c1: 4c 89 f7                    	movq	%r14, %rdi
1000035c4: e8 f5 50 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000035c9: 85 c0                       	testl	%eax, %eax
1000035cb: 75 2e                       	jne	0x1000035fb <_mb_win_workers_exit_code+0xbb>
1000035cd: 41 c7 07 06 00 00 00        	movl	$0x6, (%r15)
1000035d4: 31 c0                       	xorl	%eax, %eax
1000035d6: eb 1a                       	jmp	0x1000035f2 <_mb_win_workers_exit_code+0xb2>
1000035d8: 41 bf 03 01 00 00           	movl	$0x103, %r15d           ## imm = 0x103
1000035de: 4c 89 f7                    	movq	%r14, %rdi
1000035e1: e8 d8 50 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000035e6: 85 c0                       	testl	%eax, %eax
1000035e8: 75 11                       	jne	0x1000035fb <_mb_win_workers_exit_code+0xbb>
1000035ea: 44 89 3b                    	movl	%r15d, (%rbx)
1000035ed: b8 01 00 00 00              	movl	$0x1, %eax
1000035f2: 5b                          	popq	%rbx
1000035f3: 41 5c                       	popq	%r12
1000035f5: 41 5e                       	popq	%r14
1000035f7: 41 5f                       	popq	%r15
1000035f9: 5d                          	popq	%rbp
1000035fa: c3                          	retq
1000035fb: e8 10 50 00 00              	callq	0x100008610 <_sysconf+0x100008610>

0000000100003600 <_mb_win_workers_close_handle>:
100003600: 55                          	pushq	%rbp
100003601: 48 89 e5                    	movq	%rsp, %rbp
100003604: 41 57                       	pushq	%r15
100003606: 41 56                       	pushq	%r14
100003608: 53                          	pushq	%rbx
100003609: 50                          	pushq	%rax
10000360a: 48 85 ff                    	testq	%rdi, %rdi
10000360d: 0f 84 93 00 00 00           	je	0x1000036a6 <_mb_win_workers_close_handle+0xa6>
100003613: 48 8b 1f                    	movq	(%rdi), %rbx
100003616: 48 85 db                    	testq	%rbx, %rbx
100003619: 0f 84 87 00 00 00           	je	0x1000036a6 <_mb_win_workers_close_handle+0xa6>
10000361f: 49 89 f7                    	movq	%rsi, %r15
100003622: 48 8b bb 80 00 00 00        	movq	0x80(%rbx), %rdi
100003629: e8 f2 ea ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000362e: 49 89 c6                    	movq	%rax, %r14
100003631: 48 89 df                    	movq	%rbx, %rdi
100003634: e8 7f 50 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003639: 85 c0                       	testl	%eax, %eax
10000363b: 75 69                       	jne	0x1000036a6 <_mb_win_workers_close_handle+0xa6>
10000363d: 48 8b 83 c0 00 00 00        	movq	0xc0(%rbx), %rax
100003644: eb 0e                       	jmp	0x100003654 <_mb_win_workers_close_handle+0x54>
100003646: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100003650: 48 8b 40 08                 	movq	0x8(%rax), %rax
100003654: 48 85 c0                    	testq	%rax, %rax
100003657: 74 0c                       	je	0x100003665 <_mb_win_workers_close_handle+0x65>
100003659: 83 78 64 00                 	cmpl	$0x0, 0x64(%rax)
10000365d: 74 f1                       	je	0x100003650 <_mb_win_workers_close_handle+0x50>
10000365f: 4c 39 78 30                 	cmpq	%r15, 0x30(%rax)
100003663: 75 eb                       	jne	0x100003650 <_mb_win_workers_close_handle+0x50>
100003665: 48 85 c0                    	testq	%rax, %rax
100003668: 74 1c                       	je	0x100003686 <_mb_win_workers_close_handle+0x86>
10000366a: c7 40 64 00 00 00 00        	movl	$0x0, 0x64(%rax)
100003671: 48 89 df                    	movq	%rbx, %rdi
100003674: e8 45 50 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003679: 89 c1                       	movl	%eax, %ecx
10000367b: b8 01 00 00 00              	movl	$0x1, %eax
100003680: 85 c9                       	testl	%ecx, %ecx
100003682: 74 17                       	je	0x10000369b <_mb_win_workers_close_handle+0x9b>
100003684: eb 20                       	jmp	0x1000036a6 <_mb_win_workers_close_handle+0xa6>
100003686: 48 89 df                    	movq	%rbx, %rdi
100003689: e8 30 50 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000368e: 85 c0                       	testl	%eax, %eax
100003690: 75 14                       	jne	0x1000036a6 <_mb_win_workers_close_handle+0xa6>
100003692: 41 c7 06 06 00 00 00        	movl	$0x6, (%r14)
100003699: 31 c0                       	xorl	%eax, %eax
10000369b: 48 83 c4 08                 	addq	$0x8, %rsp
10000369f: 5b                          	popq	%rbx
1000036a0: 41 5e                       	popq	%r14
1000036a2: 41 5f                       	popq	%r15
1000036a4: 5d                          	popq	%rbp
1000036a5: c3                          	retq
1000036a6: e8 65 4f 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000036ab: 0f 1f 44 00 00              	nopl	(%rax,%rax)

00000001000036b0 <_mb_win_workers_sleep>:
1000036b0: 55                          	pushq	%rbp
1000036b1: 48 89 e5                    	movq	%rsp, %rbp
1000036b4: 41 57                       	pushq	%r15
1000036b6: 41 56                       	pushq	%r14
1000036b8: 53                          	pushq	%rbx
1000036b9: 48 83 ec 28                 	subq	$0x28, %rsp
1000036bd: 48 85 ff                    	testq	%rdi, %rdi
1000036c0: 0f 84 be 00 00 00           	je	0x100003784 <_mb_win_workers_sleep+0xd4>
1000036c6: 48 8b 07                    	movq	(%rdi), %rax
1000036c9: 48 85 c0                    	testq	%rax, %rax
1000036cc: 0f 84 b2 00 00 00           	je	0x100003784 <_mb_win_workers_sleep+0xd4>
1000036d2: 41 89 f6                    	movl	%esi, %r14d
1000036d5: 48 8b b8 80 00 00 00        	movq	0x80(%rax), %rdi
1000036dc: e8 3f ea ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000036e1: 48 89 c3                    	movq	%rax, %rbx
1000036e4: 44 8b 38                    	movl	(%rax), %r15d
1000036e7: 45 85 f6                    	testl	%r14d, %r14d
1000036ea: 74 17                       	je	0x100003703 <_mb_win_workers_sleep+0x53>
1000036ec: 41 83 fe ff                 	cmpl	$-0x1, %r14d
1000036f0: 74 28                       	je	0x10000371a <_mb_win_workers_sleep+0x6a>
1000036f2: 44 89 f0                    	movl	%r14d, %eax
1000036f5: 48 69 f8 40 42 0f 00        	imulq	$0xf4240, %rax, %rdi    ## imm = 0xF4240
1000036fc: e8 8f fd ff ff              	callq	0x100003490 <_pause_ns>
100003701: eb 05                       	jmp	0x100003708 <_mb_win_workers_sleep+0x58>
100003703: e8 c8 4f 00 00              	callq	0x1000086d0 <_sysconf+0x1000086d0>
100003708: 85 c0                       	testl	%eax, %eax
10000370a: 75 78                       	jne	0x100003784 <_mb_win_workers_sleep+0xd4>
10000370c: 44 89 3b                    	movl	%r15d, (%rbx)
10000370f: 48 83 c4 28                 	addq	$0x28, %rsp
100003713: 5b                          	popq	%rbx
100003714: 41 5e                       	popq	%r14
100003716: 41 5f                       	popq	%r15
100003718: 5d                          	popq	%rbp
100003719: c3                          	retq
10000371a: 48 8d 5d d8                 	leaq	-0x28(%rbp), %rbx
10000371e: 4c 8d 75 c8                 	leaq	-0x38(%rbp), %r14
100003722: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100003730: 48 c7 45 d8 10 0e 00 00     	movq	$0xe10, -0x28(%rbp)     ## imm = 0xE10
100003738: 48 c7 45 e0 00 00 00 00     	movq	$0x0, -0x20(%rbp)
100003740: 48 89 df                    	movq	%rbx, %rdi
100003743: 4c 89 f6                    	movq	%r14, %rsi
100003746: e8 07 4f 00 00              	callq	0x100008652 <_sysconf+0x100008652>
10000374b: 85 c0                       	testl	%eax, %eax
10000374d: 74 e1                       	je	0x100003730 <_mb_win_workers_sleep+0x80>
10000374f: e8 aa 4e 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003754: 83 38 04                    	cmpl	$0x4, (%rax)
100003757: 75 17                       	jne	0x100003770 <_mb_win_workers_sleep+0xc0>
100003759: 48 89 df                    	movq	%rbx, %rdi
10000375c: 4c 89 f6                    	movq	%r14, %rsi
10000375f: e8 7c 4b 00 00              	callq	0x1000082e0 <_mb_win_workers_sleep.cold.1>
100003764: a8 01                       	testb	$0x1, %al
100003766: 74 e7                       	je	0x10000374f <_mb_win_workers_sleep+0x9f>
100003768: eb c6                       	jmp	0x100003730 <_mb_win_workers_sleep+0x80>
10000376a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)
100003770: e8 89 4e 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003775: 83 38 00                    	cmpl	$0x0, (%rax)
100003778: 74 0a                       	je	0x100003784 <_mb_win_workers_sleep+0xd4>
10000377a: e8 7f 4e 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
10000377f: 83 38 00                    	cmpl	$0x0, (%rax)
100003782: 74 ac                       	je	0x100003730 <_mb_win_workers_sleep+0x80>
100003784: e8 87 4e 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100003789: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100003790 <_mb_win_critical_domain>:
100003790: 48 85 ff                    	testq	%rdi, %rdi
100003793: 74 12                       	je	0x1000037a7 <_mb_win_critical_domain+0x17>
100003795: 48 8b 07                    	movq	(%rdi), %rax
100003798: 48 85 c0                    	testq	%rax, %rax
10000379b: 74 0a                       	je	0x1000037a7 <_mb_win_critical_domain+0x17>
10000379d: 55                          	pushq	%rbp
10000379e: 48 89 e5                    	movq	%rsp, %rbp
1000037a1: 48 8b 40 70                 	movq	0x70(%rax), %rax
1000037a5: 5d                          	popq	%rbp
1000037a6: c3                          	retq
1000037a7: 31 c0                       	xorl	%eax, %eax
1000037a9: c3                          	retq
1000037aa: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

00000001000037b0 <_mb_win_critical_open>:
1000037b0: b8 01 00 00 00              	movl	$0x1, %eax
1000037b5: 48 85 ff                    	testq	%rdi, %rdi
1000037b8: 74 26                       	je	0x1000037e0 <_mb_win_critical_open+0x30>
1000037ba: 48 85 f6                    	testq	%rsi, %rsi
1000037bd: 74 21                       	je	0x1000037e0 <_mb_win_critical_open+0x30>
1000037bf: 49 b8 9e 99 99 99 99 99 99 f9       	movabsq	$-0x666666666666662, %r8 ## imm = 0xF99999999999999E
1000037c9: 4e 8d 0c 02                 	leaq	(%rdx,%r8), %r9
1000037cd: 49 ff c0                    	incq	%r8
1000037d0: 4d 39 c1                    	cmpq	%r8, %r9
1000037d3: 72 0b                       	jb	0x1000037e0 <_mb_win_critical_open+0x30>
1000037d5: b8 02 00 00 00              	movl	$0x2, %eax
1000037da: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
1000037de: 74 01                       	je	0x1000037e1 <_mb_win_critical_open+0x31>
1000037e0: c3                          	retq
1000037e1: 55                          	pushq	%rbp
1000037e2: 48 89 e5                    	movq	%rsp, %rbp
1000037e5: 41 57                       	pushq	%r15
1000037e7: 41 56                       	pushq	%r14
1000037e9: 41 55                       	pushq	%r13
1000037eb: 41 54                       	pushq	%r12
1000037ed: 53                          	pushq	%rbx
1000037ee: 50                          	pushq	%rax
1000037ef: 49 89 d4                    	movq	%rdx, %r12
1000037f2: 49 89 f5                    	movq	%rsi, %r13
1000037f5: 48 89 7d d0                 	movq	%rdi, -0x30(%rbp)
1000037f9: 49 89 cf                    	movq	%rcx, %r15
1000037fc: e8 fd 4d 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003801: c7 00 00 00 00 00           	movl	$0x0, (%rax)
100003807: bf 3a 00 00 00              	movl	$0x3a, %edi
10000380c: e8 d1 4e 00 00              	callq	0x1000086e2 <_sysconf+0x1000086e2>
100003811: 48 85 c0                    	testq	%rax, %rax
100003814: 7e 5d                       	jle	0x100003873 <_mb_win_critical_open+0xc3>
100003816: 49 89 c6                    	movq	%rax, %r14
100003819: 4b 8d 04 a4                 	leaq	(%r12,%r12,4), %rax
10000381d: 48 8d 34 c5 b8 00 00 00     	leaq	0xb8(,%rax,8), %rsi
100003825: bf 01 00 00 00              	movl	$0x1, %edi
10000382a: e8 ed 4d 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
10000382f: 48 85 c0                    	testq	%rax, %rax
100003832: 74 6b                       	je	0x10000389f <_mb_win_critical_open+0xef>
100003834: 48 89 c3                    	movq	%rax, %rbx
100003837: 4c 89 a0 90 00 00 00        	movq	%r12, 0x90(%rax)
10000383e: 4c 89 ef                    	movq	%r13, %rdi
100003841: 4c 89 68 70                 	movq	%r13, 0x70(%rax)
100003845: 31 c0                       	xorl	%eax, %eax
100003847: 49 83 fe 01                 	cmpq	$0x1, %r14
10000384b: 0f 95 c0                    	setne	%al
10000384e: 89 83 a8 00 00 00           	movl	%eax, 0xa8(%rbx)
100003854: 48 8d 73 78                 	leaq	0x78(%rbx), %rsi
100003858: e8 43 e7 ff ff              	callq	0x100001fa0 <_mb_win_threads_acquire>
10000385d: 85 c0                       	testl	%eax, %eax
10000385f: 74 58                       	je	0x1000038b9 <_mb_win_critical_open+0x109>
100003861: 48 89 df                    	movq	%rbx, %rdi
100003864: e8 d1 4d 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100003869: b8 02 00 00 00              	movl	$0x2, %eax
10000386e: e9 cc 00 00 00              	jmp	0x10000393f <_mb_win_critical_open+0x18f>
100003873: e8 86 4d 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003878: 83 38 00                    	cmpl	$0x0, (%rax)
10000387b: 0f 84 8c 00 00 00           	je	0x10000390d <_mb_win_critical_open+0x15d>
100003881: e8 78 4d 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100003886: 8b 00                       	movl	(%rax), %eax
100003888: 4d 85 ff                    	testq	%r15, %r15
10000388b: 74 03                       	je	0x100003890 <_mb_win_critical_open+0xe0>
10000388d: 41 89 07                    	movl	%eax, (%r15)
100003890: 31 c9                       	xorl	%ecx, %ecx
100003892: 85 c0                       	testl	%eax, %eax
100003894: 0f 95 c1                    	setne	%cl
100003897: 8d 04 89                    	leal	(%rcx,%rcx,4), %eax
10000389a: e9 a0 00 00 00              	jmp	0x10000393f <_mb_win_critical_open+0x18f>
10000389f: b8 05 00 00 00              	movl	$0x5, %eax
1000038a4: 4d 85 ff                    	testq	%r15, %r15
1000038a7: 0f 84 92 00 00 00           	je	0x10000393f <_mb_win_critical_open+0x18f>
1000038ad: 41 c7 07 0c 00 00 00        	movl	$0xc, (%r15)
1000038b4: e9 86 00 00 00              	jmp	0x10000393f <_mb_win_critical_open+0x18f>
1000038b9: 48 89 df                    	movq	%rbx, %rdi
1000038bc: 31 f6                       	xorl	%esi, %esi
1000038be: e8 ef 4d 00 00              	callq	0x1000086b2 <_sysconf+0x1000086b2>
1000038c3: 85 c0                       	testl	%eax, %eax
1000038c5: 75 18                       	jne	0x1000038df <_mb_win_critical_open+0x12f>
1000038c7: c7 83 b0 00 00 00 01 00 00 00       	movl	$0x1, 0xb0(%rbx)
1000038d1: 48 89 df                    	movq	%rbx, %rdi
1000038d4: 48 83 c7 40                 	addq	$0x40, %rdi
1000038d8: 31 f6                       	xorl	%esi, %esi
1000038da: e8 85 4d 00 00              	callq	0x100008664 <_sysconf+0x100008664>
1000038df: 85 c0                       	testl	%eax, %eax
1000038e1: 74 3d                       	je	0x100003920 <_mb_win_critical_open+0x170>
1000038e3: 41 89 c6                    	movl	%eax, %r14d
1000038e6: 48 8b 7d d0                 	movq	-0x30(%rbp), %rdi
1000038ea: 48 89 1f                    	movq	%rbx, (%rdi)
1000038ed: c7 83 ac 00 00 00 01 00 00 00       	movl	$0x1, 0xac(%rbx)
1000038f7: 31 f6                       	xorl	%esi, %esi
1000038f9: e8 52 00 00 00              	callq	0x100003950 <_mb_win_critical_close>
1000038fe: b8 05 00 00 00              	movl	$0x5, %eax
100003903: 4d 85 ff                    	testq	%r15, %r15
100003906: 74 37                       	je	0x10000393f <_mb_win_critical_open+0x18f>
100003908: 45 89 37                    	movl	%r14d, (%r15)
10000390b: eb 32                       	jmp	0x10000393f <_mb_win_critical_open+0x18f>
10000390d: b8 16 00 00 00              	movl	$0x16, %eax
100003912: 4d 85 ff                    	testq	%r15, %r15
100003915: 0f 85 72 ff ff ff           	jne	0x10000388d <_mb_win_critical_open+0xdd>
10000391b: e9 70 ff ff ff              	jmp	0x100003890 <_mb_win_critical_open+0xe0>
100003920: c7 83 b4 00 00 00 01 00 00 00       	movl	$0x1, 0xb4(%rbx)
10000392a: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
10000392e: 48 89 18                    	movq	%rbx, (%rax)
100003931: 31 c0                       	xorl	%eax, %eax
100003933: 4d 85 ff                    	testq	%r15, %r15
100003936: 74 07                       	je	0x10000393f <_mb_win_critical_open+0x18f>
100003938: 41 c7 07 00 00 00 00        	movl	$0x0, (%r15)
10000393f: 48 83 c4 08                 	addq	$0x8, %rsp
100003943: 5b                          	popq	%rbx
100003944: 41 5c                       	popq	%r12
100003946: 41 5d                       	popq	%r13
100003948: 41 5e                       	popq	%r14
10000394a: 41 5f                       	popq	%r15
10000394c: 5d                          	popq	%rbp
10000394d: c3                          	retq
10000394e: 66 90                       	nop

0000000100003950 <_mb_win_critical_close>:
100003950: 55                          	pushq	%rbp
100003951: 48 89 e5                    	movq	%rsp, %rbp
100003954: 41 57                       	pushq	%r15
100003956: 41 56                       	pushq	%r14
100003958: 41 54                       	pushq	%r12
10000395a: 53                          	pushq	%rbx
10000395b: 48 85 ff                    	testq	%rdi, %rdi
10000395e: 74 5c                       	je	0x1000039bc <_mb_win_critical_close+0x6c>
100003960: 48 89 f3                    	movq	%rsi, %rbx
100003963: 49 89 fe                    	movq	%rdi, %r14
100003966: 4c 8b 3f                    	movq	(%rdi), %r15
100003969: 4d 85 ff                    	testq	%r15, %r15
10000396c: 0f 84 4c 01 00 00           	je	0x100003abe <_mb_win_critical_close+0x16e>
100003972: 41 83 bf b0 00 00 00 00     	cmpl	$0x0, 0xb0(%r15)
10000397a: 0f 84 8c 00 00 00           	je	0x100003a0c <_mb_win_critical_close+0xbc>
100003980: 4c 89 ff                    	movq	%r15, %rdi
100003983: e8 30 4d 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003988: 85 c0                       	testl	%eax, %eax
10000398a: 75 2b                       	jne	0x1000039b7 <_mb_win_critical_close+0x67>
10000398c: 49 83 bf a0 00 00 00 00     	cmpq	$0x0, 0xa0(%r15)
100003994: 75 0a                       	jne	0x1000039a0 <_mb_win_critical_close+0x50>
100003996: 49 83 bf 98 00 00 00 00     	cmpq	$0x0, 0x98(%r15)
10000399e: 74 26                       	je	0x1000039c6 <_mb_win_critical_close+0x76>
1000039a0: 4c 89 ff                    	movq	%r15, %rdi
1000039a3: e8 16 4d 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000039a8: 89 c1                       	movl	%eax, %ecx
1000039aa: b8 03 00 00 00              	movl	$0x3, %eax
1000039af: 85 c9                       	testl	%ecx, %ecx
1000039b1: 0f 84 14 01 00 00           	je	0x100003acb <_mb_win_critical_close+0x17b>
1000039b7: e8 54 4c 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000039bc: b8 01 00 00 00              	movl	$0x1, %eax
1000039c1: e9 05 01 00 00              	jmp	0x100003acb <_mb_win_critical_close+0x17b>
1000039c6: 49 8b 87 90 00 00 00        	movq	0x90(%r15), %rax
1000039cd: 48 85 c0                    	testq	%rax, %rax
1000039d0: 74 23                       	je	0x1000039f5 <_mb_win_critical_close+0xa5>
1000039d2: 49 8d 8f d8 00 00 00        	leaq	0xd8(%r15), %rcx
1000039d9: 83 79 f0 00                 	cmpl	$0x0, -0x10(%rcx)
1000039dd: 75 c1                       	jne	0x1000039a0 <_mb_win_critical_close+0x50>
1000039df: 48 83 79 f8 00              	cmpq	$0x0, -0x8(%rcx)
1000039e4: 75 ba                       	jne	0x1000039a0 <_mb_win_critical_close+0x50>
1000039e6: 48 83 39 00                 	cmpq	$0x0, (%rcx)
1000039ea: 75 b4                       	jne	0x1000039a0 <_mb_win_critical_close+0x50>
1000039ec: 48 83 c1 28                 	addq	$0x28, %rcx
1000039f0: 48 ff c8                    	decq	%rax
1000039f3: 75 e4                       	jne	0x1000039d9 <_mb_win_critical_close+0x89>
1000039f5: 41 c7 87 ac 00 00 00 01 00 00 00    	movl	$0x1, 0xac(%r15)
100003a00: 4c 89 ff                    	movq	%r15, %rdi
100003a03: e8 b6 4c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003a08: 85 c0                       	testl	%eax, %eax
100003a0a: 75 ab                       	jne	0x1000039b7 <_mb_win_critical_close+0x67>
100003a0c: 41 83 bf b4 00 00 00 00     	cmpl	$0x0, 0xb4(%r15)
100003a14: 74 2f                       	je	0x100003a45 <_mb_win_critical_close+0xf5>
100003a16: 49 8d 7f 40                 	leaq	0x40(%r15), %rdi
100003a1a: e8 3f 4c 00 00              	callq	0x10000865e <_sysconf+0x10000865e>
100003a1f: 89 c1                       	movl	%eax, %ecx
100003a21: 85 c0                       	testl	%eax, %eax
100003a23: 74 4e                       	je	0x100003a73 <_mb_win_critical_close+0x123>
100003a25: b8 05 00 00 00              	movl	$0x5, %eax
100003a2a: 48 85 db                    	testq	%rbx, %rbx
100003a2d: 74 02                       	je	0x100003a31 <_mb_win_critical_close+0xe1>
100003a2f: 89 0b                       	movl	%ecx, (%rbx)
100003a31: 85 c9                       	testl	%ecx, %ecx
100003a33: 0f 85 92 00 00 00           	jne	0x100003acb <_mb_win_critical_close+0x17b>
100003a39: 41 83 bf b0 00 00 00 00     	cmpl	$0x0, 0xb0(%r15)
100003a41: 75 11                       	jne	0x100003a54 <_mb_win_critical_close+0x104>
100003a43: eb 56                       	jmp	0x100003a9b <_mb_win_critical_close+0x14b>
100003a45: b8 03 00 00 00              	movl	$0x3, %eax
100003a4a: 41 83 bf b0 00 00 00 00     	cmpl	$0x0, 0xb0(%r15)
100003a52: 74 47                       	je	0x100003a9b <_mb_win_critical_close+0x14b>
100003a54: 41 89 c4                    	movl	%eax, %r12d
100003a57: 4c 89 ff                    	movq	%r15, %rdi
100003a5a: e8 4d 4c 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100003a5f: 89 c1                       	movl	%eax, %ecx
100003a61: 85 c0                       	testl	%eax, %eax
100003a63: 74 24                       	je	0x100003a89 <_mb_win_critical_close+0x139>
100003a65: b8 05 00 00 00              	movl	$0x5, %eax
100003a6a: 48 85 db                    	testq	%rbx, %rbx
100003a6d: 74 28                       	je	0x100003a97 <_mb_win_critical_close+0x147>
100003a6f: 89 0b                       	movl	%ecx, (%rbx)
100003a71: eb 24                       	jmp	0x100003a97 <_mb_win_critical_close+0x147>
100003a73: 41 c7 87 b4 00 00 00 00 00 00 00    	movl	$0x0, 0xb4(%r15)
100003a7e: b8 03 00 00 00              	movl	$0x3, %eax
100003a83: 85 c9                       	testl	%ecx, %ecx
100003a85: 75 44                       	jne	0x100003acb <_mb_win_critical_close+0x17b>
100003a87: eb b0                       	jmp	0x100003a39 <_mb_win_critical_close+0xe9>
100003a89: 41 c7 87 b0 00 00 00 00 00 00 00    	movl	$0x0, 0xb0(%r15)
100003a94: 44 89 e0                    	movl	%r12d, %eax
100003a97: 85 c9                       	testl	%ecx, %ecx
100003a99: 75 30                       	jne	0x100003acb <_mb_win_critical_close+0x17b>
100003a9b: 49 8d 7f 78                 	leaq	0x78(%r15), %rdi
100003a9f: e8 cc e5 ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100003aa4: 89 c1                       	movl	%eax, %ecx
100003aa6: b8 02 00 00 00              	movl	$0x2, %eax
100003aab: 85 c9                       	testl	%ecx, %ecx
100003aad: 75 1c                       	jne	0x100003acb <_mb_win_critical_close+0x17b>
100003aaf: 4c 89 ff                    	movq	%r15, %rdi
100003ab2: e8 83 4b 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100003ab7: 49 c7 06 00 00 00 00        	movq	$0x0, (%r14)
100003abe: 48 85 db                    	testq	%rbx, %rbx
100003ac1: 74 06                       	je	0x100003ac9 <_mb_win_critical_close+0x179>
100003ac3: c7 03 00 00 00 00           	movl	$0x0, (%rbx)
100003ac9: 31 c0                       	xorl	%eax, %eax
100003acb: 5b                          	popq	%rbx
100003acc: 41 5c                       	popq	%r12
100003ace: 41 5e                       	popq	%r14
100003ad0: 41 5f                       	popq	%r15
100003ad2: 5d                          	popq	%rbp
100003ad3: c3                          	retq
100003ad4: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100003ae0 <_mb_win_critical_acquire>:
100003ae0: 55                          	pushq	%rbp
100003ae1: 48 89 e5                    	movq	%rsp, %rbp
100003ae4: 41 57                       	pushq	%r15
100003ae6: 41 56                       	pushq	%r14
100003ae8: 53                          	pushq	%rbx
100003ae9: 50                          	pushq	%rax
100003aea: 48 85 ff                    	testq	%rdi, %rdi
100003aed: 0f 94 c1                    	sete	%cl
100003af0: 48 85 f6                    	testq	%rsi, %rsi
100003af3: 0f 94 c2                    	sete	%dl
100003af6: b8 01 00 00 00              	movl	$0x1, %eax
100003afb: 08 ca                       	orb	%cl, %dl
100003afd: 75 49                       	jne	0x100003b48 <_mb_win_critical_acquire+0x68>
100003aff: 48 8b 1f                    	movq	(%rdi), %rbx
100003b02: b8 02 00 00 00              	movl	$0x2, %eax
100003b07: 48 85 db                    	testq	%rbx, %rbx
100003b0a: 74 3c                       	je	0x100003b48 <_mb_win_critical_acquire+0x68>
100003b0c: 83 bb ac 00 00 00 00        	cmpl	$0x0, 0xac(%rbx)
100003b13: 75 33                       	jne	0x100003b48 <_mb_win_critical_acquire+0x68>
100003b15: 49 89 fe                    	movq	%rdi, %r14
100003b18: 49 89 f7                    	movq	%rsi, %r15
100003b1b: 48 89 df                    	movq	%rbx, %rdi
100003b1e: e8 95 4b 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003b23: 85 c0                       	testl	%eax, %eax
100003b25: 75 68                       	jne	0x100003b8f <_mb_win_critical_acquire+0xaf>
100003b27: 49 83 3f 00                 	cmpq	$0x0, (%r15)
100003b2b: 75 0a                       	jne	0x100003b37 <_mb_win_critical_acquire+0x57>
100003b2d: 4c 89 f8                    	movq	%r15, %rax
100003b30: 49 83 7f 08 00              	cmpq	$0x0, 0x8(%r15)
100003b35: 74 1c                       	je	0x100003b53 <_mb_win_critical_acquire+0x73>
100003b37: 48 89 df                    	movq	%rbx, %rdi
100003b3a: e8 7f 4b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003b3f: 85 c0                       	testl	%eax, %eax
100003b41: b8 02 00 00 00              	movl	$0x2, %eax
100003b46: 75 47                       	jne	0x100003b8f <_mb_win_critical_acquire+0xaf>
100003b48: 48 83 c4 08                 	addq	$0x8, %rsp
100003b4c: 5b                          	popq	%rbx
100003b4d: 41 5e                       	popq	%r14
100003b4f: 41 5f                       	popq	%r15
100003b51: 5d                          	popq	%rbp
100003b52: c3                          	retq
100003b53: 48 8b 93 98 00 00 00        	movq	0x98(%rbx), %rdx
100003b5a: 48 83 fa ff                 	cmpq	$-0x1, %rdx
100003b5e: 74 d7                       	je	0x100003b37 <_mb_win_critical_acquire+0x57>
100003b60: 4c 89 30                    	movq	%r14, (%rax)
100003b63: 48 8b 8b 88 00 00 00        	movq	0x88(%rbx), %rcx
100003b6a: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
100003b6e: 48 89 83 88 00 00 00        	movq	%rax, 0x88(%rbx)
100003b75: 48 ff c2                    	incq	%rdx
100003b78: 48 89 93 98 00 00 00        	movq	%rdx, 0x98(%rbx)
100003b7f: 48 89 df                    	movq	%rbx, %rdi
100003b82: e8 37 4b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003b87: 89 c1                       	movl	%eax, %ecx
100003b89: 31 c0                       	xorl	%eax, %eax
100003b8b: 85 c9                       	testl	%ecx, %ecx
100003b8d: 74 b9                       	je	0x100003b48 <_mb_win_critical_acquire+0x68>
100003b8f: e8 7c 4a 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100003b94: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100003ba0 <_mb_win_critical_release_pin>:
100003ba0: 55                          	pushq	%rbp
100003ba1: 48 89 e5                    	movq	%rsp, %rbp
100003ba4: 41 56                       	pushq	%r14
100003ba6: 53                          	pushq	%rbx
100003ba7: 48 85 ff                    	testq	%rdi, %rdi
100003baa: 0f 84 8e 00 00 00           	je	0x100003c3e <_mb_win_critical_release_pin+0x9e>
100003bb0: 48 8b 0f                    	movq	(%rdi), %rcx
100003bb3: b8 02 00 00 00              	movl	$0x2, %eax
100003bb8: 48 85 c9                    	testq	%rcx, %rcx
100003bbb: 0f 84 95 00 00 00           	je	0x100003c56 <_mb_win_critical_release_pin+0xb6>
100003bc1: 48 8b 19                    	movq	(%rcx), %rbx
100003bc4: 48 85 db                    	testq	%rbx, %rbx
100003bc7: 0f 84 89 00 00 00           	je	0x100003c56 <_mb_win_critical_release_pin+0xb6>
100003bcd: 49 89 fe                    	movq	%rdi, %r14
100003bd0: 48 89 df                    	movq	%rbx, %rdi
100003bd3: e8 e0 4a 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003bd8: 85 c0                       	testl	%eax, %eax
100003bda: 75 7f                       	jne	0x100003c5b <_mb_win_critical_release_pin+0xbb>
100003bdc: 48 8d b3 88 00 00 00        	leaq	0x88(%rbx), %rsi
100003be3: 4c 89 f0                    	movq	%r14, %rax
100003be6: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100003bf0: 48 89 f1                    	movq	%rsi, %rcx
100003bf3: 48 8b 16                    	movq	(%rsi), %rdx
100003bf6: 48 85 d2                    	testq	%rdx, %rdx
100003bf9: 74 09                       	je	0x100003c04 <_mb_win_critical_release_pin+0x64>
100003bfb: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
100003bff: 48 39 c2                    	cmpq	%rax, %rdx
100003c02: 75 ec                       	jne	0x100003bf0 <_mb_win_critical_release_pin+0x50>
100003c04: 48 85 d2                    	testq	%rdx, %rdx
100003c07: 74 3c                       	je	0x100003c45 <_mb_win_critical_release_pin+0xa5>
100003c09: 48 8b 93 98 00 00 00        	movq	0x98(%rbx), %rdx
100003c10: 48 85 d2                    	testq	%rdx, %rdx
100003c13: 74 30                       	je	0x100003c45 <_mb_win_critical_release_pin+0xa5>
100003c15: 48 8b 70 08                 	movq	0x8(%rax), %rsi
100003c19: 48 89 31                    	movq	%rsi, (%rcx)
100003c1c: 48 ff ca                    	decq	%rdx
100003c1f: 48 89 93 98 00 00 00        	movq	%rdx, 0x98(%rbx)
100003c26: 0f 57 c0                    	xorps	%xmm0, %xmm0
100003c29: 0f 11 00                    	movups	%xmm0, (%rax)
100003c2c: 48 89 df                    	movq	%rbx, %rdi
100003c2f: e8 8a 4a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003c34: 89 c1                       	movl	%eax, %ecx
100003c36: 31 c0                       	xorl	%eax, %eax
100003c38: 85 c9                       	testl	%ecx, %ecx
100003c3a: 74 1a                       	je	0x100003c56 <_mb_win_critical_release_pin+0xb6>
100003c3c: eb 1d                       	jmp	0x100003c5b <_mb_win_critical_release_pin+0xbb>
100003c3e: b8 01 00 00 00              	movl	$0x1, %eax
100003c43: eb 11                       	jmp	0x100003c56 <_mb_win_critical_release_pin+0xb6>
100003c45: 48 89 df                    	movq	%rbx, %rdi
100003c48: e8 71 4a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003c4d: 85 c0                       	testl	%eax, %eax
100003c4f: b8 02 00 00 00              	movl	$0x2, %eax
100003c54: 75 05                       	jne	0x100003c5b <_mb_win_critical_release_pin+0xbb>
100003c56: 5b                          	popq	%rbx
100003c57: 41 5e                       	popq	%r14
100003c59: 5d                          	popq	%rbp
100003c5a: c3                          	retq
100003c5b: e8 b0 49 00 00              	callq	0x100008610 <_sysconf+0x100008610>

0000000100003c60 <_mb_win_critical_initialize>:
100003c60: 55                          	pushq	%rbp
100003c61: 48 89 e5                    	movq	%rsp, %rbp
100003c64: 41 57                       	pushq	%r15
100003c66: 41 56                       	pushq	%r14
100003c68: 41 55                       	pushq	%r13
100003c6a: 41 54                       	pushq	%r12
100003c6c: 53                          	pushq	%rbx
100003c6d: 48 83 ec 18                 	subq	$0x18, %rsp
100003c71: b8 01 00 00 00              	movl	$0x1, %eax
100003c76: 48 85 ff                    	testq	%rdi, %rdi
100003c79: 0f 84 b1 01 00 00           	je	0x100003e30 <_mb_win_critical_initialize+0x1d0>
100003c7f: 49 89 f6                    	movq	%rsi, %r14
100003c82: 48 83 fe d7                 	cmpq	$-0x29, %rsi
100003c86: 0f 87 a4 01 00 00           	ja	0x100003e30 <_mb_win_critical_initialize+0x1d0>
100003c8c: 4d 85 f6                    	testq	%r14, %r14
100003c8f: 0f 84 9b 01 00 00           	je	0x100003e30 <_mb_win_critical_initialize+0x1d0>
100003c95: 44 89 f1                    	movl	%r14d, %ecx
100003c98: 83 e1 07                    	andl	$0x7, %ecx
100003c9b: 0f 85 8f 01 00 00           	jne	0x100003e30 <_mb_win_critical_initialize+0x1d0>
100003ca1: 4c 8b 3f                    	movq	(%rdi), %r15
100003ca4: b8 02 00 00 00              	movl	$0x2, %eax
100003ca9: 4d 85 ff                    	testq	%r15, %r15
100003cac: 0f 84 7e 01 00 00           	je	0x100003e30 <_mb_win_critical_initialize+0x1d0>
100003cb2: 41 83 bf ac 00 00 00 00     	cmpl	$0x0, 0xac(%r15)
100003cba: 0f 85 70 01 00 00           	jne	0x100003e30 <_mb_win_critical_initialize+0x1d0>
100003cc0: 89 d3                       	movl	%edx, %ebx
100003cc2: 4c 89 ff                    	movq	%r15, %rdi
100003cc5: e8 ee 49 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003cca: 85 c0                       	testl	%eax, %eax
100003ccc: 0f 85 6d 01 00 00           	jne	0x100003e3f <_mb_win_critical_initialize+0x1df>
100003cd2: 89 5d cc                    	movl	%ebx, -0x34(%rbp)
100003cd5: 49 83 bf 90 00 00 00 00     	cmpq	$0x0, 0x90(%r15)
100003cdd: 41 0f 95 c5                 	setne	%r13b
100003ce1: 0f 84 e8 00 00 00           	je	0x100003dcf <_mb_win_critical_initialize+0x16f>
100003ce7: 4d 8d a7 b8 00 00 00        	leaq	0xb8(%r15), %r12
100003cee: 41 b5 01                    	movb	$0x1, %r13b
100003cf1: 48 c7 45 d0 00 00 00 00     	movq	$0x0, -0x30(%rbp)
100003cf9: 31 db                       	xorl	%ebx, %ebx
100003cfb: 0f 1f 44 00 00              	nopl	(%rax,%rax)
100003d00: 49 8b 0c 24                 	movq	(%r12), %rcx
100003d04: 4c 39 f1                    	cmpq	%r14, %rcx
100003d07: 74 27                       	je	0x100003d30 <_mb_win_critical_initialize+0xd0>
100003d09: 48 85 c9                    	testq	%rcx, %rcx
100003d0c: 74 52                       	je	0x100003d60 <_mb_win_critical_initialize+0x100>
100003d0e: 48 89 ca                    	movq	%rcx, %rdx
100003d11: 4c 29 f2                    	subq	%r14, %rdx
100003d14: 76 65                       	jbe	0x100003d7b <_mb_win_critical_initialize+0x11b>
100003d16: 48 83 fa 28                 	cmpq	$0x28, %rdx
100003d1a: 72 6b                       	jb	0x100003d87 <_mb_win_critical_initialize+0x127>
100003d1c: 31 d2                       	xorl	%edx, %edx
100003d1e: f6 c2 03                    	testb	$0x3, %dl
100003d21: 0f 84 8e 00 00 00           	je	0x100003db5 <_mb_win_critical_initialize+0x155>
100003d27: e9 ab 00 00 00              	jmp	0x100003dd7 <_mb_win_critical_initialize+0x177>
100003d2c: 0f 1f 40 00                 	nopl	(%rax)
100003d30: 4c 89 ff                    	movq	%r15, %rdi
100003d33: e8 86 49 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003d38: 85 c0                       	testl	%eax, %eax
100003d3a: 0f 85 ff 00 00 00           	jne	0x100003e3f <_mb_win_critical_initialize+0x1df>
100003d40: b8 02 00 00 00              	movl	$0x2, %eax
100003d45: ba 01 00 00 00              	movl	$0x1, %edx
100003d4a: f6 c2 03                    	testb	$0x3, %dl
100003d4d: 74 66                       	je	0x100003db5 <_mb_win_critical_initialize+0x155>
100003d4f: e9 83 00 00 00              	jmp	0x100003dd7 <_mb_win_critical_initialize+0x177>
100003d54: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
100003d60: 48 8b 4d d0                 	movq	-0x30(%rbp), %rcx
100003d64: 48 85 c9                    	testq	%rcx, %rcx
100003d67: 49 0f 44 cc                 	cmoveq	%r12, %rcx
100003d6b: 48 89 4d d0                 	movq	%rcx, -0x30(%rbp)
100003d6f: ba 04 00 00 00              	movl	$0x4, %edx
100003d74: f6 c2 03                    	testb	$0x3, %dl
100003d77: 74 3c                       	je	0x100003db5 <_mb_win_critical_initialize+0x155>
100003d79: eb 5c                       	jmp	0x100003dd7 <_mb_win_critical_initialize+0x177>
100003d7b: 4c 89 f2                    	movq	%r14, %rdx
100003d7e: 48 29 ca                    	subq	%rcx, %rdx
100003d81: 48 83 fa 27                 	cmpq	$0x27, %rdx
100003d85: 77 95                       	ja	0x100003d1c <_mb_win_critical_initialize+0xbc>
100003d87: 4c 89 ff                    	movq	%r15, %rdi
100003d8a: e8 2f 49 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003d8f: 89 c1                       	movl	%eax, %ecx
100003d91: ba 01 00 00 00              	movl	$0x1, %edx
100003d96: b8 01 00 00 00              	movl	$0x1, %eax
100003d9b: 85 c9                       	testl	%ecx, %ecx
100003d9d: 0f 85 9c 00 00 00           	jne	0x100003e3f <_mb_win_critical_initialize+0x1df>
100003da3: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)
100003db0: f6 c2 03                    	testb	$0x3, %dl
100003db3: 75 22                       	jne	0x100003dd7 <_mb_win_critical_initialize+0x177>
100003db5: 48 ff c3                    	incq	%rbx
100003db8: 49 83 c4 28                 	addq	$0x28, %r12
100003dbc: 49 3b 9f 90 00 00 00        	cmpq	0x90(%r15), %rbx
100003dc3: 41 0f 92 c5                 	setb	%r13b
100003dc7: 0f 82 33 ff ff ff           	jb	0x100003d00 <_mb_win_critical_initialize+0xa0>
100003dcd: eb 08                       	jmp	0x100003dd7 <_mb_win_critical_initialize+0x177>
100003dcf: 48 c7 45 d0 00 00 00 00     	movq	$0x0, -0x30(%rbp)
100003dd7: 41 f6 c5 01                 	testb	$0x1, %r13b
100003ddb: 8b 4d cc                    	movl	-0x34(%rbp), %ecx
100003dde: 75 50                       	jne	0x100003e30 <_mb_win_critical_initialize+0x1d0>
100003de0: 48 8b 55 d0                 	movq	-0x30(%rbp), %rdx
100003de4: 48 85 d2                    	testq	%rdx, %rdx
100003de7: 74 34                       	je	0x100003e1d <_mb_win_critical_initialize+0x1bd>
100003de9: 0f 57 c0                    	xorps	%xmm0, %xmm0
100003dec: 41 0f 11 46 10              	movups	%xmm0, 0x10(%r14)
100003df1: 41 0f 11 06                 	movups	%xmm0, (%r14)
100003df5: 49 c7 46 20 00 00 00 00     	movq	$0x0, 0x20(%r14)
100003dfd: 4c 89 32                    	movq	%r14, (%rdx)
100003e00: 41 8b 87 a8 00 00 00        	movl	0xa8(%r15), %eax
100003e07: 85 c0                       	testl	%eax, %eax
100003e09: 0f 44 c8                    	cmovel	%eax, %ecx
100003e0c: 89 4a 14                    	movl	%ecx, 0x14(%rdx)
100003e0f: 4c 89 ff                    	movq	%r15, %rdi
100003e12: e8 a7 48 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003e17: 89 c1                       	movl	%eax, %ecx
100003e19: 31 c0                       	xorl	%eax, %eax
100003e1b: eb 0f                       	jmp	0x100003e2c <_mb_win_critical_initialize+0x1cc>
100003e1d: 4c 89 ff                    	movq	%r15, %rdi
100003e20: e8 99 48 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003e25: 89 c1                       	movl	%eax, %ecx
100003e27: b8 04 00 00 00              	movl	$0x4, %eax
100003e2c: 85 c9                       	testl	%ecx, %ecx
100003e2e: 75 0f                       	jne	0x100003e3f <_mb_win_critical_initialize+0x1df>
100003e30: 48 83 c4 18                 	addq	$0x18, %rsp
100003e34: 5b                          	popq	%rbx
100003e35: 41 5c                       	popq	%r12
100003e37: 41 5d                       	popq	%r13
100003e39: 41 5e                       	popq	%r14
100003e3b: 41 5f                       	popq	%r15
100003e3d: 5d                          	popq	%rbp
100003e3e: c3                          	retq
100003e3f: e8 cc 47 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100003e44: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100003e50 <_mb_win_critical_enter>:
100003e50: 55                          	pushq	%rbp
100003e51: 48 89 e5                    	movq	%rsp, %rbp
100003e54: 41 57                       	pushq	%r15
100003e56: 41 56                       	pushq	%r14
100003e58: 41 55                       	pushq	%r13
100003e5a: 41 54                       	pushq	%r12
100003e5c: 53                          	pushq	%rbx
100003e5d: 48 83 ec 18                 	subq	$0x18, %rsp
100003e61: 48 8d 55 c0                 	leaq	-0x40(%rbp), %rdx
100003e65: 48 8d 4d c8                 	leaq	-0x38(%rbp), %rcx
100003e69: e8 52 01 00 00              	callq	0x100003fc0 <_acquire_state>
100003e6e: 41 89 c6                    	movl	%eax, %r14d
100003e71: 85 c0                       	testl	%eax, %eax
100003e73: 0f 85 e7 00 00 00           	jne	0x100003f60 <_mb_win_critical_enter+0x110>
100003e79: 48 8b 5d c0                 	movq	-0x40(%rbp), %rbx
100003e7d: 48 8b 83 a0 00 00 00        	movq	0xa0(%rbx), %rax
100003e84: 48 83 f8 ff                 	cmpq	$-0x1, %rax
100003e88: 0f 84 c0 00 00 00           	je	0x100003f4e <_mb_win_critical_enter+0xfe>
100003e8e: 4c 8b 6d c8                 	movq	-0x38(%rbp), %r13
100003e92: 49 83 7d 18 ff              	cmpq	$-0x1, 0x18(%r13)
100003e97: 0f 84 b1 00 00 00           	je	0x100003f4e <_mb_win_critical_enter+0xfe>
100003e9d: 48 ff c0                    	incq	%rax
100003ea0: 48 89 83 a0 00 00 00        	movq	%rax, 0xa0(%rbx)
100003ea7: 49 ff 45 18                 	incq	0x18(%r13)
100003eab: e8 14 48 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100003eb0: 49 89 c7                    	movq	%rax, %r15
100003eb3: 45 8b 65 14                 	movl	0x14(%r13), %r12d
100003eb7: 48 8d 43 40                 	leaq	0x40(%rbx), %rax
100003ebb: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
100003ebf: 45 31 f6                    	xorl	%r14d, %r14d
100003ec2: 41 ff c4                    	incl	%r12d
100003ec5: 66 66 2e 0f 1f 84 00 00 00 00 00    	nopw	%cs:(%rax,%rax)
100003ed0: 41 83 7d 10 00              	cmpl	$0x0, 0x10(%r13)
100003ed5: 0f 84 9d 00 00 00           	je	0x100003f78 <_mb_win_critical_enter+0x128>
100003edb: 49 8b 7d 08                 	movq	0x8(%r13), %rdi
100003edf: 4c 89 fe                    	movq	%r15, %rsi
100003ee2: e8 a7 47 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100003ee7: 85 c0                       	testl	%eax, %eax
100003ee9: 0f 85 89 00 00 00           	jne	0x100003f78 <_mb_win_critical_enter+0x128>
100003eef: 41 ff cc                    	decl	%r12d
100003ef2: 74 23                       	je	0x100003f17 <_mb_win_critical_enter+0xc7>
100003ef4: 48 89 df                    	movq	%rbx, %rdi
100003ef7: e8 c2 47 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003efc: 85 c0                       	testl	%eax, %eax
100003efe: 0f 85 b2 00 00 00           	jne	0x100003fb6 <_mb_win_critical_enter+0x166>
100003f04: f3 90                       	pause
100003f06: 48 89 df                    	movq	%rbx, %rdi
100003f09: e8 aa 47 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100003f0e: 85 c0                       	testl	%eax, %eax
100003f10: 74 be                       	je	0x100003ed0 <_mb_win_critical_enter+0x80>
100003f12: e9 9f 00 00 00              	jmp	0x100003fb6 <_mb_win_critical_enter+0x166>
100003f17: 49 8b 45 20                 	movq	0x20(%r13), %rax
100003f1b: 48 83 f8 ff                 	cmpq	$-0x1, %rax
100003f1f: 74 51                       	je	0x100003f72 <_mb_win_critical_enter+0x122>
100003f21: 48 ff c0                    	incq	%rax
100003f24: 49 89 45 20                 	movq	%rax, 0x20(%r13)
100003f28: 48 8b 7d d0                 	movq	-0x30(%rbp), %rdi
100003f2c: 48 89 de                    	movq	%rbx, %rsi
100003f2f: e8 42 47 00 00              	callq	0x100008676 <_sysconf+0x100008676>
100003f34: 49 ff 4d 20                 	decq	0x20(%r13)
100003f38: 45 31 e4                    	xorl	%r12d, %r12d
100003f3b: 85 c0                       	testl	%eax, %eax
100003f3d: b8 05 00 00 00              	movl	$0x5, %eax
100003f42: 44 0f 45 f0                 	cmovnel	%eax, %r14d
100003f46: 0f 84 76 ff ff ff           	je	0x100003ec2 <_mb_win_critical_enter+0x72>
100003f4c: eb 2a                       	jmp	0x100003f78 <_mb_win_critical_enter+0x128>
100003f4e: 48 89 df                    	movq	%rbx, %rdi
100003f51: e8 68 47 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003f56: 41 be 04 00 00 00           	movl	$0x4, %r14d
100003f5c: 85 c0                       	testl	%eax, %eax
100003f5e: 75 56                       	jne	0x100003fb6 <_mb_win_critical_enter+0x166>
100003f60: 44 89 f0                    	movl	%r14d, %eax
100003f63: 48 83 c4 18                 	addq	$0x18, %rsp
100003f67: 5b                          	popq	%rbx
100003f68: 41 5c                       	popq	%r12
100003f6a: 41 5d                       	popq	%r13
100003f6c: 41 5e                       	popq	%r14
100003f6e: 41 5f                       	popq	%r15
100003f70: 5d                          	popq	%rbp
100003f71: c3                          	retq
100003f72: 41 be 04 00 00 00           	movl	$0x4, %r14d
100003f78: 45 85 f6                    	testl	%r14d, %r14d
100003f7b: 75 22                       	jne	0x100003f9f <_mb_win_critical_enter+0x14f>
100003f7d: 41 8b 45 10                 	movl	0x10(%r13), %eax
100003f81: 83 f8 ff                    	cmpl	$-0x1, %eax
100003f84: 74 13                       	je	0x100003f99 <_mb_win_critical_enter+0x149>
100003f86: 85 c0                       	testl	%eax, %eax
100003f88: 75 04                       	jne	0x100003f8e <_mb_win_critical_enter+0x13e>
100003f8a: 4d 89 7d 08                 	movq	%r15, 0x8(%r13)
100003f8e: ff c0                       	incl	%eax
100003f90: 41 89 45 10                 	movl	%eax, 0x10(%r13)
100003f94: 45 31 f6                    	xorl	%r14d, %r14d
100003f97: eb 06                       	jmp	0x100003f9f <_mb_win_critical_enter+0x14f>
100003f99: 41 be 04 00 00 00           	movl	$0x4, %r14d
100003f9f: 49 ff 4d 18                 	decq	0x18(%r13)
100003fa3: 48 ff 8b a0 00 00 00        	decq	0xa0(%rbx)
100003faa: 48 89 df                    	movq	%rbx, %rdi
100003fad: e8 0c 47 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100003fb2: 85 c0                       	testl	%eax, %eax
100003fb4: 74 aa                       	je	0x100003f60 <_mb_win_critical_enter+0x110>
100003fb6: e8 55 46 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100003fbb: 0f 1f 44 00 00              	nopl	(%rax,%rax)

0000000100003fc0 <_acquire_state>:
100003fc0: 55                          	pushq	%rbp
100003fc1: 48 89 e5                    	movq	%rsp, %rbp
100003fc4: 41 57                       	pushq	%r15
100003fc6: 41 56                       	pushq	%r14
100003fc8: 41 54                       	pushq	%r12
100003fca: 53                          	pushq	%rbx
100003fcb: b8 01 00 00 00              	movl	$0x1, %eax
100003fd0: 48 85 ff                    	testq	%rdi, %rdi
100003fd3: 0f 84 97 00 00 00           	je	0x100004070 <_acquire_state+0xb0>
100003fd9: 48 83 fe d7                 	cmpq	$-0x29, %rsi
100003fdd: 0f 87 8d 00 00 00           	ja	0x100004070 <_acquire_state+0xb0>
100003fe3: 48 85 f6                    	testq	%rsi, %rsi
100003fe6: 0f 84 84 00 00 00           	je	0x100004070 <_acquire_state+0xb0>
100003fec: 41 89 f0                    	movl	%esi, %r8d
100003fef: 41 83 e0 07                 	andl	$0x7, %r8d
100003ff3: 75 7b                       	jne	0x100004070 <_acquire_state+0xb0>
100003ff5: 48 8b 1f                    	movq	(%rdi), %rbx
100003ff8: b8 02 00 00 00              	movl	$0x2, %eax
100003ffd: 48 85 db                    	testq	%rbx, %rbx
100004000: 74 6e                       	je	0x100004070 <_acquire_state+0xb0>
100004002: 83 bb ac 00 00 00 00        	cmpl	$0x0, 0xac(%rbx)
100004009: 75 65                       	jne	0x100004070 <_acquire_state+0xb0>
10000400b: 49 89 f6                    	movq	%rsi, %r14
10000400e: 49 89 d4                    	movq	%rdx, %r12
100004011: 49 89 cf                    	movq	%rcx, %r15
100004014: 48 89 df                    	movq	%rbx, %rdi
100004017: e8 9c 46 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
10000401c: 85 c0                       	testl	%eax, %eax
10000401e: 75 59                       	jne	0x100004079 <_acquire_state+0xb9>
100004020: 48 8b b3 90 00 00 00        	movq	0x90(%rbx), %rsi
100004027: 48 85 f6                    	testq	%rsi, %rsi
10000402a: 4c 89 f8                    	movq	%r15, %rax
10000402d: 4c 89 e1                    	movq	%r12, %rcx
100004030: 74 1c                       	je	0x10000404e <_acquire_state+0x8e>
100004032: 4c 89 f7                    	movq	%r14, %rdi
100004035: 48 8d 93 b8 00 00 00        	leaq	0xb8(%rbx), %rdx
10000403c: 0f 1f 40 00                 	nopl	(%rax)
100004040: 48 39 3a                    	cmpq	%rdi, (%rdx)
100004043: 74 0b                       	je	0x100004050 <_acquire_state+0x90>
100004045: 48 83 c2 28                 	addq	$0x28, %rdx
100004049: 48 ff ce                    	decq	%rsi
10000404c: 75 f2                       	jne	0x100004040 <_acquire_state+0x80>
10000404e: 31 d2                       	xorl	%edx, %edx
100004050: 48 85 d2                    	testq	%rdx, %rdx
100004053: 74 0a                       	je	0x10000405f <_acquire_state+0x9f>
100004055: 48 89 19                    	movq	%rbx, (%rcx)
100004058: 48 89 10                    	movq	%rdx, (%rax)
10000405b: 31 c0                       	xorl	%eax, %eax
10000405d: eb 11                       	jmp	0x100004070 <_acquire_state+0xb0>
10000405f: 48 89 df                    	movq	%rbx, %rdi
100004062: e8 57 46 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004067: 85 c0                       	testl	%eax, %eax
100004069: b8 02 00 00 00              	movl	$0x2, %eax
10000406e: 75 09                       	jne	0x100004079 <_acquire_state+0xb9>
100004070: 5b                          	popq	%rbx
100004071: 41 5c                       	popq	%r12
100004073: 41 5e                       	popq	%r14
100004075: 41 5f                       	popq	%r15
100004077: 5d                          	popq	%rbp
100004078: c3                          	retq
100004079: e8 92 45 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000407e: 66 90                       	nop

0000000100004080 <_mb_win_critical_try_enter>:
100004080: 55                          	pushq	%rbp
100004081: 48 89 e5                    	movq	%rsp, %rbp
100004084: 41 57                       	pushq	%r15
100004086: 41 56                       	pushq	%r14
100004088: 41 54                       	pushq	%r12
10000408a: 53                          	pushq	%rbx
10000408b: 48 83 ec 10                 	subq	$0x10, %rsp
10000408f: 48 85 d2                    	testq	%rdx, %rdx
100004092: 74 5c                       	je	0x1000040f0 <_mb_win_critical_try_enter+0x70>
100004094: 48 89 d3                    	movq	%rdx, %rbx
100004097: 48 8d 55 d0                 	leaq	-0x30(%rbp), %rdx
10000409b: 48 8d 4d d8                 	leaq	-0x28(%rbp), %rcx
10000409f: e8 1c ff ff ff              	callq	0x100003fc0 <_acquire_state>
1000040a4: 85 c0                       	testl	%eax, %eax
1000040a6: 75 7b                       	jne	0x100004123 <_mb_win_critical_try_enter+0xa3>
1000040a8: e8 17 46 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
1000040ad: 49 89 c6                    	movq	%rax, %r14
1000040b0: 4c 8b 7d d8                 	movq	-0x28(%rbp), %r15
1000040b4: 41 83 7f 10 00              	cmpl	$0x0, 0x10(%r15)
1000040b9: 74 10                       	je	0x1000040cb <_mb_win_critical_try_enter+0x4b>
1000040bb: 49 8b 7f 08                 	movq	0x8(%r15), %rdi
1000040bf: 4c 89 f6                    	movq	%r14, %rsi
1000040c2: e8 c7 45 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
1000040c7: 85 c0                       	testl	%eax, %eax
1000040c9: 74 37                       	je	0x100004102 <_mb_win_critical_try_enter+0x82>
1000040cb: 41 8b 47 10                 	movl	0x10(%r15), %eax
1000040cf: 83 f8 ff                    	cmpl	$-0x1, %eax
1000040d2: 74 23                       	je	0x1000040f7 <_mb_win_critical_try_enter+0x77>
1000040d4: 85 c0                       	testl	%eax, %eax
1000040d6: 75 04                       	jne	0x1000040dc <_mb_win_critical_try_enter+0x5c>
1000040d8: 4d 89 77 08                 	movq	%r14, 0x8(%r15)
1000040dc: ff c0                       	incl	%eax
1000040de: 41 89 47 10                 	movl	%eax, 0x10(%r15)
1000040e2: 41 bc 01 00 00 00           	movl	$0x1, %r12d
1000040e8: 45 31 f6                    	xorl	%r14d, %r14d
1000040eb: 41 b7 01                    	movb	$0x1, %r15b
1000040ee: eb 1b                       	jmp	0x10000410b <_mb_win_critical_try_enter+0x8b>
1000040f0: b8 01 00 00 00              	movl	$0x1, %eax
1000040f5: eb 2c                       	jmp	0x100004123 <_mb_win_critical_try_enter+0xa3>
1000040f7: 45 31 ff                    	xorl	%r15d, %r15d
1000040fa: 41 be 04 00 00 00           	movl	$0x4, %r14d
100004100: eb 06                       	jmp	0x100004108 <_mb_win_critical_try_enter+0x88>
100004102: 45 31 f6                    	xorl	%r14d, %r14d
100004105: 41 b7 01                    	movb	$0x1, %r15b
100004108: 45 31 e4                    	xorl	%r12d, %r12d
10000410b: 48 8b 7d d0                 	movq	-0x30(%rbp), %rdi
10000410f: e8 aa 45 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004114: 85 c0                       	testl	%eax, %eax
100004116: 75 18                       	jne	0x100004130 <_mb_win_critical_try_enter+0xb0>
100004118: 45 84 ff                    	testb	%r15b, %r15b
10000411b: 74 03                       	je	0x100004120 <_mb_win_critical_try_enter+0xa0>
10000411d: 44 89 23                    	movl	%r12d, (%rbx)
100004120: 44 89 f0                    	movl	%r14d, %eax
100004123: 48 83 c4 10                 	addq	$0x10, %rsp
100004127: 5b                          	popq	%rbx
100004128: 41 5c                       	popq	%r12
10000412a: 41 5e                       	popq	%r14
10000412c: 41 5f                       	popq	%r15
10000412e: 5d                          	popq	%rbp
10000412f: c3                          	retq
100004130: e8 db 44 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004135: 66 66 2e 0f 1f 84 00 00 00 00 00    	nopw	%cs:(%rax,%rax)

0000000100004140 <_mb_win_critical_leave>:
100004140: 55                          	pushq	%rbp
100004141: 48 89 e5                    	movq	%rsp, %rbp
100004144: 41 56                       	pushq	%r14
100004146: 53                          	pushq	%rbx
100004147: 48 83 ec 10                 	subq	$0x10, %rsp
10000414b: 48 8d 55 e8                 	leaq	-0x18(%rbp), %rdx
10000414f: 48 8d 4d e0                 	leaq	-0x20(%rbp), %rcx
100004153: e8 68 fe ff ff              	callq	0x100003fc0 <_acquire_state>
100004158: 85 c0                       	testl	%eax, %eax
10000415a: 75 5d                       	jne	0x1000041b9 <_mb_win_critical_leave+0x79>
10000415c: 4c 8b 75 e0                 	movq	-0x20(%rbp), %r14
100004160: 41 83 7e 10 00              	cmpl	$0x0, 0x10(%r14)
100004165: 74 3e                       	je	0x1000041a5 <_mb_win_critical_leave+0x65>
100004167: 49 8b 5e 08                 	movq	0x8(%r14), %rbx
10000416b: e8 54 45 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100004170: 48 89 df                    	movq	%rbx, %rdi
100004173: 48 89 c6                    	movq	%rax, %rsi
100004176: e8 13 45 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
10000417b: 85 c0                       	testl	%eax, %eax
10000417d: 74 26                       	je	0x1000041a5 <_mb_win_critical_leave+0x65>
10000417f: 41 ff 4e 10                 	decl	0x10(%r14)
100004183: 75 11                       	jne	0x100004196 <_mb_win_critical_leave+0x56>
100004185: 48 8b 7d e8                 	movq	-0x18(%rbp), %rdi
100004189: 48 83 c7 40                 	addq	$0x40, %rdi
10000418d: e8 c6 44 00 00              	callq	0x100008658 <_sysconf+0x100008658>
100004192: 85 c0                       	testl	%eax, %eax
100004194: 75 2c                       	jne	0x1000041c2 <_mb_win_critical_leave+0x82>
100004196: 48 8b 7d e8                 	movq	-0x18(%rbp), %rdi
10000419a: e8 1f 45 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000419f: 89 c1                       	movl	%eax, %ecx
1000041a1: 31 c0                       	xorl	%eax, %eax
1000041a3: eb 10                       	jmp	0x1000041b5 <_mb_win_critical_leave+0x75>
1000041a5: 48 8b 7d e8                 	movq	-0x18(%rbp), %rdi
1000041a9: e8 10 45 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000041ae: 89 c1                       	movl	%eax, %ecx
1000041b0: b8 02 00 00 00              	movl	$0x2, %eax
1000041b5: 85 c9                       	testl	%ecx, %ecx
1000041b7: 75 09                       	jne	0x1000041c2 <_mb_win_critical_leave+0x82>
1000041b9: 48 83 c4 10                 	addq	$0x10, %rsp
1000041bd: 5b                          	popq	%rbx
1000041be: 41 5e                       	popq	%r14
1000041c0: 5d                          	popq	%rbp
1000041c1: c3                          	retq
1000041c2: e8 49 44 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000041c7: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)

00000001000041d0 <_mb_win_critical_delete>:
1000041d0: 55                          	pushq	%rbp
1000041d1: 48 89 e5                    	movq	%rsp, %rbp
1000041d4: 48 83 ec 10                 	subq	$0x10, %rsp
1000041d8: 48 8d 55 f8                 	leaq	-0x8(%rbp), %rdx
1000041dc: 48 8d 4d f0                 	leaq	-0x10(%rbp), %rcx
1000041e0: e8 db fd ff ff              	callq	0x100003fc0 <_acquire_state>
1000041e5: 85 c0                       	testl	%eax, %eax
1000041e7: 75 2c                       	jne	0x100004215 <_mb_win_critical_delete+0x45>
1000041e9: 48 8b 45 f0                 	movq	-0x10(%rbp), %rax
1000041ed: 83 78 10 00                 	cmpl	$0x0, 0x10(%rax)
1000041f1: 75 0e                       	jne	0x100004201 <_mb_win_critical_delete+0x31>
1000041f3: 48 83 78 18 00              	cmpq	$0x0, 0x18(%rax)
1000041f8: 75 07                       	jne	0x100004201 <_mb_win_critical_delete+0x31>
1000041fa: 48 83 78 20 00              	cmpq	$0x0, 0x20(%rax)
1000041ff: 74 1a                       	je	0x10000421b <_mb_win_critical_delete+0x4b>
100004201: 48 8b 7d f8                 	movq	-0x8(%rbp), %rdi
100004205: e8 b4 44 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000420a: 89 c1                       	movl	%eax, %ecx
10000420c: b8 03 00 00 00              	movl	$0x3, %eax
100004211: 85 c9                       	testl	%ecx, %ecx
100004213: 75 29                       	jne	0x10000423e <_mb_win_critical_delete+0x6e>
100004215: 48 83 c4 10                 	addq	$0x10, %rsp
100004219: 5d                          	popq	%rbp
10000421a: c3                          	retq
10000421b: 0f 57 c0                    	xorps	%xmm0, %xmm0
10000421e: 0f 11 40 10                 	movups	%xmm0, 0x10(%rax)
100004222: 0f 11 00                    	movups	%xmm0, (%rax)
100004225: 48 c7 40 20 00 00 00 00     	movq	$0x0, 0x20(%rax)
10000422d: 48 8b 7d f8                 	movq	-0x8(%rbp), %rdi
100004231: e8 88 44 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004236: 89 c1                       	movl	%eax, %ecx
100004238: 31 c0                       	xorl	%eax, %eax
10000423a: 85 c9                       	testl	%ecx, %ecx
10000423c: 74 d7                       	je	0x100004215 <_mb_win_critical_delete+0x45>
10000423e: e8 cd 43 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004243: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)

0000000100004250 <_mb_win_critical_set_spin>:
100004250: 55                          	pushq	%rbp
100004251: 48 89 e5                    	movq	%rsp, %rbp
100004254: 41 57                       	pushq	%r15
100004256: 41 56                       	pushq	%r14
100004258: 53                          	pushq	%rbx
100004259: 48 83 ec 18                 	subq	$0x18, %rsp
10000425d: 48 85 c9                    	testq	%rcx, %rcx
100004260: 74 43                       	je	0x1000042a5 <_mb_win_critical_set_spin+0x55>
100004262: 48 89 cb                    	movq	%rcx, %rbx
100004265: 41 89 d6                    	movl	%edx, %r14d
100004268: 48 8d 55 d8                 	leaq	-0x28(%rbp), %rdx
10000426c: 48 8d 4d e0                 	leaq	-0x20(%rbp), %rcx
100004270: e8 4b fd ff ff              	callq	0x100003fc0 <_acquire_state>
100004275: 85 c0                       	testl	%eax, %eax
100004277: 75 31                       	jne	0x1000042aa <_mb_win_critical_set_spin+0x5a>
100004279: 48 8b 45 e0                 	movq	-0x20(%rbp), %rax
10000427d: 44 8b 78 14                 	movl	0x14(%rax), %r15d
100004281: 48 8b 7d d8                 	movq	-0x28(%rbp), %rdi
100004285: 8b 8f a8 00 00 00           	movl	0xa8(%rdi), %ecx
10000428b: 85 c9                       	testl	%ecx, %ecx
10000428d: 44 0f 44 f1                 	cmovel	%ecx, %r14d
100004291: 44 89 70 14                 	movl	%r14d, 0x14(%rax)
100004295: e8 24 44 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000429a: 85 c0                       	testl	%eax, %eax
10000429c: 75 17                       	jne	0x1000042b5 <_mb_win_critical_set_spin+0x65>
10000429e: 44 89 3b                    	movl	%r15d, (%rbx)
1000042a1: 31 c0                       	xorl	%eax, %eax
1000042a3: eb 05                       	jmp	0x1000042aa <_mb_win_critical_set_spin+0x5a>
1000042a5: b8 01 00 00 00              	movl	$0x1, %eax
1000042aa: 48 83 c4 18                 	addq	$0x18, %rsp
1000042ae: 5b                          	popq	%rbx
1000042af: 41 5e                       	popq	%r14
1000042b1: 41 5f                       	popq	%r15
1000042b3: 5d                          	popq	%rbp
1000042b4: c3                          	retq
1000042b5: e8 56 43 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000042ba: 00 00                       	addb	%al, (%rax)
1000042bc: 00 00                       	addb	%al, (%rax)
1000042be: 00 00                       	addb	%al, (%rax)

00000001000042c0 <_mb_win_address_wait_open>:
1000042c0: 48 85 ff                    	testq	%rdi, %rdi
1000042c3: 0f 94 c1                    	sete	%cl
1000042c6: 48 85 f6                    	testq	%rsi, %rsi
1000042c9: 41 0f 94 c0                 	sete	%r8b
1000042cd: b8 01 00 00 00              	movl	$0x1, %eax
1000042d2: 41 08 c8                    	orb	%cl, %r8b
1000042d5: 75 0b                       	jne	0x1000042e2 <_mb_win_address_wait_open+0x22>
1000042d7: b8 02 00 00 00              	movl	$0x2, %eax
1000042dc: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
1000042e0: 74 01                       	je	0x1000042e3 <_mb_win_address_wait_open+0x23>
1000042e2: c3                          	retq
1000042e3: 55                          	pushq	%rbp
1000042e4: 48 89 e5                    	movq	%rsp, %rbp
1000042e7: 41 57                       	pushq	%r15
1000042e9: 41 56                       	pushq	%r14
1000042eb: 41 54                       	pushq	%r12
1000042ed: 53                          	pushq	%rbx
1000042ee: 49 89 f4                    	movq	%rsi, %r12
1000042f1: 49 89 ff                    	movq	%rdi, %r15
1000042f4: 49 89 d6                    	movq	%rdx, %r14
1000042f7: bf 01 00 00 00              	movl	$0x1, %edi
1000042fc: be d0 00 00 00              	movl	$0xd0, %esi
100004301: e8 16 43 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
100004306: 48 85 c0                    	testq	%rax, %rax
100004309: 74 2f                       	je	0x10000433a <_mb_win_address_wait_open+0x7a>
10000430b: 48 89 c3                    	movq	%rax, %rbx
10000430e: 4c 89 e7                    	movq	%r12, %rdi
100004311: 4c 89 a0 80 00 00 00        	movq	%r12, 0x80(%rax)
100004318: 48 8d b0 88 00 00 00        	leaq	0x88(%rax), %rsi
10000431f: e8 7c dc ff ff              	callq	0x100001fa0 <_mb_win_threads_acquire>
100004324: 85 c0                       	testl	%eax, %eax
100004326: 74 2c                       	je	0x100004354 <_mb_win_address_wait_open+0x94>
100004328: 48 89 df                    	movq	%rbx, %rdi
10000432b: e8 0a 43 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100004330: b8 05 00 00 00              	movl	$0x5, %eax
100004335: e9 c5 00 00 00              	jmp	0x1000043ff <_mb_win_address_wait_open+0x13f>
10000433a: b8 04 00 00 00              	movl	$0x4, %eax
10000433f: 4d 85 f6                    	testq	%r14, %r14
100004342: 0f 84 b7 00 00 00           	je	0x1000043ff <_mb_win_address_wait_open+0x13f>
100004348: 41 c7 06 0c 00 00 00        	movl	$0xc, (%r14)
10000434f: e9 ab 00 00 00              	jmp	0x1000043ff <_mb_win_address_wait_open+0x13f>
100004354: 48 89 df                    	movq	%rbx, %rdi
100004357: 31 f6                       	xorl	%esi, %esi
100004359: e8 54 43 00 00              	callq	0x1000086b2 <_sysconf+0x1000086b2>
10000435e: 85 c0                       	testl	%eax, %eax
100004360: 74 0a                       	je	0x10000436c <_mb_win_address_wait_open+0xac>
100004362: 85 c0                       	testl	%eax, %eax
100004364: 74 1d                       	je	0x100004383 <_mb_win_address_wait_open+0xc3>
100004366: 85 c0                       	testl	%eax, %eax
100004368: 75 5a                       	jne	0x1000043c4 <_mb_win_address_wait_open+0x104>
10000436a: eb 32                       	jmp	0x10000439e <_mb_win_address_wait_open+0xde>
10000436c: c7 83 c0 00 00 00 01 00 00 00       	movl	$0x1, 0xc0(%rbx)
100004376: 48 8d 7b 70                 	leaq	0x70(%rbx), %rdi
10000437a: e8 03 43 00 00              	callq	0x100008682 <_sysconf+0x100008682>
10000437f: 85 c0                       	testl	%eax, %eax
100004381: 75 e3                       	jne	0x100004366 <_mb_win_address_wait_open+0xa6>
100004383: c7 83 c8 00 00 00 01 00 00 00       	movl	$0x1, 0xc8(%rbx)
10000438d: 48 8d 7b 40                 	leaq	0x40(%rbx), %rdi
100004391: 48 8d 73 70                 	leaq	0x70(%rbx), %rsi
100004395: e8 ca 42 00 00              	callq	0x100008664 <_sysconf+0x100008664>
10000439a: 85 c0                       	testl	%eax, %eax
10000439c: 75 26                       	jne	0x1000043c4 <_mb_win_address_wait_open+0x104>
10000439e: c7 83 c4 00 00 00 01 00 00 00       	movl	$0x1, 0xc4(%rbx)
1000043a8: 48 89 df                    	movq	%rbx, %rdi
1000043ab: 48 83 c7 70                 	addq	$0x70, %rdi
1000043af: e8 c8 42 00 00              	callq	0x10000867c <_sysconf+0x10000867c>
1000043b4: 85 c0                       	testl	%eax, %eax
1000043b6: 75 0c                       	jne	0x1000043c4 <_mb_win_address_wait_open+0x104>
1000043b8: c7 83 c8 00 00 00 00 00 00 00       	movl	$0x0, 0xc8(%rbx)
1000043c2: 31 c0                       	xorl	%eax, %eax
1000043c4: 49 89 1f                    	movq	%rbx, (%r15)
1000043c7: 85 c0                       	testl	%eax, %eax
1000043c9: 74 26                       	je	0x1000043f1 <_mb_win_address_wait_open+0x131>
1000043cb: 4c 89 ff                    	movq	%r15, %rdi
1000043ce: 41 89 c7                    	movl	%eax, %r15d
1000043d1: c7 83 cc 00 00 00 01 00 00 00       	movl	$0x1, 0xcc(%rbx)
1000043db: 31 f6                       	xorl	%esi, %esi
1000043dd: e8 2e 00 00 00              	callq	0x100004410 <_mb_win_address_wait_close>
1000043e2: b8 04 00 00 00              	movl	$0x4, %eax
1000043e7: 4d 85 f6                    	testq	%r14, %r14
1000043ea: 74 13                       	je	0x1000043ff <_mb_win_address_wait_open+0x13f>
1000043ec: 45 89 3e                    	movl	%r15d, (%r14)
1000043ef: eb 0e                       	jmp	0x1000043ff <_mb_win_address_wait_open+0x13f>
1000043f1: 31 c0                       	xorl	%eax, %eax
1000043f3: 4d 85 f6                    	testq	%r14, %r14
1000043f6: 74 07                       	je	0x1000043ff <_mb_win_address_wait_open+0x13f>
1000043f8: 41 c7 06 00 00 00 00        	movl	$0x0, (%r14)
1000043ff: 5b                          	popq	%rbx
100004400: 41 5c                       	popq	%r12
100004402: 41 5e                       	popq	%r14
100004404: 41 5f                       	popq	%r15
100004406: 5d                          	popq	%rbp
100004407: c3                          	retq
100004408: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100004410 <_mb_win_address_wait_close>:
100004410: 55                          	pushq	%rbp
100004411: 48 89 e5                    	movq	%rsp, %rbp
100004414: 41 57                       	pushq	%r15
100004416: 41 56                       	pushq	%r14
100004418: 53                          	pushq	%rbx
100004419: 50                          	pushq	%rax
10000441a: 48 85 ff                    	testq	%rdi, %rdi
10000441d: 74 58                       	je	0x100004477 <_mb_win_address_wait_close+0x67>
10000441f: 48 89 f3                    	movq	%rsi, %rbx
100004422: 4c 8b 37                    	movq	(%rdi), %r14
100004425: 4d 85 f6                    	testq	%r14, %r14
100004428: 0f 84 35 01 00 00           	je	0x100004563 <_mb_win_address_wait_close+0x153>
10000442e: 49 89 ff                    	movq	%rdi, %r15
100004431: 41 83 be c0 00 00 00 00     	cmpl	$0x0, 0xc0(%r14)
100004439: 74 5d                       	je	0x100004498 <_mb_win_address_wait_close+0x88>
10000443b: 4c 89 f7                    	movq	%r14, %rdi
10000443e: e8 75 42 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100004443: 85 c0                       	testl	%eax, %eax
100004445: 75 2b                       	jne	0x100004472 <_mb_win_address_wait_close+0x62>
100004447: 49 83 be b0 00 00 00 00     	cmpq	$0x0, 0xb0(%r14)
10000444f: 75 0a                       	jne	0x10000445b <_mb_win_address_wait_close+0x4b>
100004451: 49 83 be b8 00 00 00 00     	cmpq	$0x0, 0xb8(%r14)
100004459: 74 26                       	je	0x100004481 <_mb_win_address_wait_close+0x71>
10000445b: 4c 89 f7                    	movq	%r14, %rdi
10000445e: e8 5b 42 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004463: 89 c1                       	movl	%eax, %ecx
100004465: b8 03 00 00 00              	movl	$0x3, %eax
10000446a: 85 c9                       	testl	%ecx, %ecx
10000446c: 0f 84 fe 00 00 00           	je	0x100004570 <_mb_win_address_wait_close+0x160>
100004472: e8 99 41 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004477: b8 01 00 00 00              	movl	$0x1, %eax
10000447c: e9 ef 00 00 00              	jmp	0x100004570 <_mb_win_address_wait_close+0x160>
100004481: 41 c7 86 cc 00 00 00 01 00 00 00    	movl	$0x1, 0xcc(%r14)
10000448c: 4c 89 f7                    	movq	%r14, %rdi
10000448f: e8 2a 42 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004494: 85 c0                       	testl	%eax, %eax
100004496: 75 da                       	jne	0x100004472 <_mb_win_address_wait_close+0x62>
100004498: 41 83 be c4 00 00 00 00     	cmpl	$0x0, 0xc4(%r14)
1000044a0: 74 30                       	je	0x1000044d2 <_mb_win_address_wait_close+0xc2>
1000044a2: 49 8d 7e 40                 	leaq	0x40(%r14), %rdi
1000044a6: e8 b3 41 00 00              	callq	0x10000865e <_sysconf+0x10000865e>
1000044ab: 89 c1                       	movl	%eax, %ecx
1000044ad: 85 c0                       	testl	%eax, %eax
1000044af: 74 09                       	je	0x1000044ba <_mb_win_address_wait_close+0xaa>
1000044b1: 48 85 db                    	testq	%rbx, %rbx
1000044b4: 74 0f                       	je	0x1000044c5 <_mb_win_address_wait_close+0xb5>
1000044b6: 89 0b                       	movl	%ecx, (%rbx)
1000044b8: eb 0b                       	jmp	0x1000044c5 <_mb_win_address_wait_close+0xb5>
1000044ba: 41 c7 86 c4 00 00 00 00 00 00 00    	movl	$0x0, 0xc4(%r14)
1000044c5: b8 04 00 00 00              	movl	$0x4, %eax
1000044ca: 85 c9                       	testl	%ecx, %ecx
1000044cc: 0f 85 9e 00 00 00           	jne	0x100004570 <_mb_win_address_wait_close+0x160>
1000044d2: 41 83 be c8 00 00 00 00     	cmpl	$0x0, 0xc8(%r14)
1000044da: 74 2c                       	je	0x100004508 <_mb_win_address_wait_close+0xf8>
1000044dc: 49 8d 7e 70                 	leaq	0x70(%r14), %rdi
1000044e0: e8 97 41 00 00              	callq	0x10000867c <_sysconf+0x10000867c>
1000044e5: 89 c1                       	movl	%eax, %ecx
1000044e7: 85 c0                       	testl	%eax, %eax
1000044e9: 74 09                       	je	0x1000044f4 <_mb_win_address_wait_close+0xe4>
1000044eb: 48 85 db                    	testq	%rbx, %rbx
1000044ee: 74 0f                       	je	0x1000044ff <_mb_win_address_wait_close+0xef>
1000044f0: 89 0b                       	movl	%ecx, (%rbx)
1000044f2: eb 0b                       	jmp	0x1000044ff <_mb_win_address_wait_close+0xef>
1000044f4: 41 c7 86 c8 00 00 00 00 00 00 00    	movl	$0x0, 0xc8(%r14)
1000044ff: b8 04 00 00 00              	movl	$0x4, %eax
100004504: 85 c9                       	testl	%ecx, %ecx
100004506: 75 68                       	jne	0x100004570 <_mb_win_address_wait_close+0x160>
100004508: 41 83 be c0 00 00 00 00     	cmpl	$0x0, 0xc0(%r14)
100004510: 74 2b                       	je	0x10000453d <_mb_win_address_wait_close+0x12d>
100004512: 4c 89 f7                    	movq	%r14, %rdi
100004515: e8 92 41 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
10000451a: 89 c1                       	movl	%eax, %ecx
10000451c: 85 c0                       	testl	%eax, %eax
10000451e: 74 09                       	je	0x100004529 <_mb_win_address_wait_close+0x119>
100004520: 48 85 db                    	testq	%rbx, %rbx
100004523: 74 0f                       	je	0x100004534 <_mb_win_address_wait_close+0x124>
100004525: 89 0b                       	movl	%ecx, (%rbx)
100004527: eb 0b                       	jmp	0x100004534 <_mb_win_address_wait_close+0x124>
100004529: 41 c7 86 c0 00 00 00 00 00 00 00    	movl	$0x0, 0xc0(%r14)
100004534: b8 04 00 00 00              	movl	$0x4, %eax
100004539: 85 c9                       	testl	%ecx, %ecx
10000453b: 75 33                       	jne	0x100004570 <_mb_win_address_wait_close+0x160>
10000453d: 49 8d be 88 00 00 00        	leaq	0x88(%r14), %rdi
100004544: e8 27 db ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100004549: 89 c1                       	movl	%eax, %ecx
10000454b: b8 05 00 00 00              	movl	$0x5, %eax
100004550: 85 c9                       	testl	%ecx, %ecx
100004552: 75 1c                       	jne	0x100004570 <_mb_win_address_wait_close+0x160>
100004554: 4c 89 f7                    	movq	%r14, %rdi
100004557: e8 de 40 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
10000455c: 49 c7 07 00 00 00 00        	movq	$0x0, (%r15)
100004563: 48 85 db                    	testq	%rbx, %rbx
100004566: 74 06                       	je	0x10000456e <_mb_win_address_wait_close+0x15e>
100004568: c7 03 00 00 00 00           	movl	$0x0, (%rbx)
10000456e: 31 c0                       	xorl	%eax, %eax
100004570: 48 83 c4 08                 	addq	$0x8, %rsp
100004574: 5b                          	popq	%rbx
100004575: 41 5e                       	popq	%r14
100004577: 41 5f                       	popq	%r15
100004579: 5d                          	popq	%rbp
10000457a: c3                          	retq
10000457b: 0f 1f 44 00 00              	nopl	(%rax,%rax)

0000000100004580 <_mb_win_address_wait_acquire>:
100004580: 55                          	pushq	%rbp
100004581: 48 89 e5                    	movq	%rsp, %rbp
100004584: 41 57                       	pushq	%r15
100004586: 41 56                       	pushq	%r14
100004588: 53                          	pushq	%rbx
100004589: 50                          	pushq	%rax
10000458a: 48 85 ff                    	testq	%rdi, %rdi
10000458d: 0f 94 c1                    	sete	%cl
100004590: 48 85 f6                    	testq	%rsi, %rsi
100004593: 0f 94 c2                    	sete	%dl
100004596: b8 01 00 00 00              	movl	$0x1, %eax
10000459b: 08 ca                       	orb	%cl, %dl
10000459d: 75 49                       	jne	0x1000045e8 <_mb_win_address_wait_acquire+0x68>
10000459f: 48 8b 1f                    	movq	(%rdi), %rbx
1000045a2: b8 02 00 00 00              	movl	$0x2, %eax
1000045a7: 48 85 db                    	testq	%rbx, %rbx
1000045aa: 74 3c                       	je	0x1000045e8 <_mb_win_address_wait_acquire+0x68>
1000045ac: 83 bb cc 00 00 00 00        	cmpl	$0x0, 0xcc(%rbx)
1000045b3: 75 33                       	jne	0x1000045e8 <_mb_win_address_wait_acquire+0x68>
1000045b5: 49 89 fe                    	movq	%rdi, %r14
1000045b8: 49 89 f7                    	movq	%rsi, %r15
1000045bb: 48 89 df                    	movq	%rbx, %rdi
1000045be: e8 f5 40 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000045c3: 85 c0                       	testl	%eax, %eax
1000045c5: 75 68                       	jne	0x10000462f <_mb_win_address_wait_acquire+0xaf>
1000045c7: 49 83 3f 00                 	cmpq	$0x0, (%r15)
1000045cb: 75 0a                       	jne	0x1000045d7 <_mb_win_address_wait_acquire+0x57>
1000045cd: 4c 89 f8                    	movq	%r15, %rax
1000045d0: 49 83 7f 08 00              	cmpq	$0x0, 0x8(%r15)
1000045d5: 74 1c                       	je	0x1000045f3 <_mb_win_address_wait_acquire+0x73>
1000045d7: 48 89 df                    	movq	%rbx, %rdi
1000045da: e8 df 40 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000045df: 85 c0                       	testl	%eax, %eax
1000045e1: b8 02 00 00 00              	movl	$0x2, %eax
1000045e6: 75 47                       	jne	0x10000462f <_mb_win_address_wait_acquire+0xaf>
1000045e8: 48 83 c4 08                 	addq	$0x8, %rsp
1000045ec: 5b                          	popq	%rbx
1000045ed: 41 5e                       	popq	%r14
1000045ef: 41 5f                       	popq	%r15
1000045f1: 5d                          	popq	%rbp
1000045f2: c3                          	retq
1000045f3: 48 8b 93 b8 00 00 00        	movq	0xb8(%rbx), %rdx
1000045fa: 48 83 fa ff                 	cmpq	$-0x1, %rdx
1000045fe: 74 d7                       	je	0x1000045d7 <_mb_win_address_wait_acquire+0x57>
100004600: 4c 89 30                    	movq	%r14, (%rax)
100004603: 48 8b 8b 98 00 00 00        	movq	0x98(%rbx), %rcx
10000460a: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
10000460e: 48 89 83 98 00 00 00        	movq	%rax, 0x98(%rbx)
100004615: 48 ff c2                    	incq	%rdx
100004618: 48 89 93 b8 00 00 00        	movq	%rdx, 0xb8(%rbx)
10000461f: 48 89 df                    	movq	%rbx, %rdi
100004622: e8 97 40 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004627: 89 c1                       	movl	%eax, %ecx
100004629: 31 c0                       	xorl	%eax, %eax
10000462b: 85 c9                       	testl	%ecx, %ecx
10000462d: 74 b9                       	je	0x1000045e8 <_mb_win_address_wait_acquire+0x68>
10000462f: e8 dc 3f 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004634: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100004640 <_mb_win_address_wait_release_pin>:
100004640: 55                          	pushq	%rbp
100004641: 48 89 e5                    	movq	%rsp, %rbp
100004644: 41 56                       	pushq	%r14
100004646: 53                          	pushq	%rbx
100004647: 48 85 ff                    	testq	%rdi, %rdi
10000464a: 0f 84 8e 00 00 00           	je	0x1000046de <_mb_win_address_wait_release_pin+0x9e>
100004650: 48 8b 0f                    	movq	(%rdi), %rcx
100004653: b8 02 00 00 00              	movl	$0x2, %eax
100004658: 48 85 c9                    	testq	%rcx, %rcx
10000465b: 0f 84 95 00 00 00           	je	0x1000046f6 <_mb_win_address_wait_release_pin+0xb6>
100004661: 48 8b 19                    	movq	(%rcx), %rbx
100004664: 48 85 db                    	testq	%rbx, %rbx
100004667: 0f 84 89 00 00 00           	je	0x1000046f6 <_mb_win_address_wait_release_pin+0xb6>
10000466d: 49 89 fe                    	movq	%rdi, %r14
100004670: 48 89 df                    	movq	%rbx, %rdi
100004673: e8 40 40 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100004678: 85 c0                       	testl	%eax, %eax
10000467a: 75 7f                       	jne	0x1000046fb <_mb_win_address_wait_release_pin+0xbb>
10000467c: 48 8d b3 98 00 00 00        	leaq	0x98(%rbx), %rsi
100004683: 4c 89 f0                    	movq	%r14, %rax
100004686: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100004690: 48 89 f1                    	movq	%rsi, %rcx
100004693: 48 8b 16                    	movq	(%rsi), %rdx
100004696: 48 85 d2                    	testq	%rdx, %rdx
100004699: 74 09                       	je	0x1000046a4 <_mb_win_address_wait_release_pin+0x64>
10000469b: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
10000469f: 48 39 c2                    	cmpq	%rax, %rdx
1000046a2: 75 ec                       	jne	0x100004690 <_mb_win_address_wait_release_pin+0x50>
1000046a4: 48 85 d2                    	testq	%rdx, %rdx
1000046a7: 74 3c                       	je	0x1000046e5 <_mb_win_address_wait_release_pin+0xa5>
1000046a9: 48 8b 93 b8 00 00 00        	movq	0xb8(%rbx), %rdx
1000046b0: 48 85 d2                    	testq	%rdx, %rdx
1000046b3: 74 30                       	je	0x1000046e5 <_mb_win_address_wait_release_pin+0xa5>
1000046b5: 48 8b 70 08                 	movq	0x8(%rax), %rsi
1000046b9: 48 89 31                    	movq	%rsi, (%rcx)
1000046bc: 48 ff ca                    	decq	%rdx
1000046bf: 48 89 93 b8 00 00 00        	movq	%rdx, 0xb8(%rbx)
1000046c6: 0f 57 c0                    	xorps	%xmm0, %xmm0
1000046c9: 0f 11 00                    	movups	%xmm0, (%rax)
1000046cc: 48 89 df                    	movq	%rbx, %rdi
1000046cf: e8 ea 3f 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000046d4: 89 c1                       	movl	%eax, %ecx
1000046d6: 31 c0                       	xorl	%eax, %eax
1000046d8: 85 c9                       	testl	%ecx, %ecx
1000046da: 74 1a                       	je	0x1000046f6 <_mb_win_address_wait_release_pin+0xb6>
1000046dc: eb 1d                       	jmp	0x1000046fb <_mb_win_address_wait_release_pin+0xbb>
1000046de: b8 01 00 00 00              	movl	$0x1, %eax
1000046e3: eb 11                       	jmp	0x1000046f6 <_mb_win_address_wait_release_pin+0xb6>
1000046e5: 48 89 df                    	movq	%rbx, %rdi
1000046e8: e8 d1 3f 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000046ed: 85 c0                       	testl	%eax, %eax
1000046ef: b8 02 00 00 00              	movl	$0x2, %eax
1000046f4: 75 05                       	jne	0x1000046fb <_mb_win_address_wait_release_pin+0xbb>
1000046f6: 5b                          	popq	%rbx
1000046f7: 41 5e                       	popq	%r14
1000046f9: 5d                          	popq	%rbp
1000046fa: c3                          	retq
1000046fb: e8 10 3f 00 00              	callq	0x100008610 <_sysconf+0x100008610>

0000000100004700 <_mb_win_address_wait_wait>:
100004700: 55                          	pushq	%rbp
100004701: 48 89 e5                    	movq	%rsp, %rbp
100004704: 41 57                       	pushq	%r15
100004706: 41 56                       	pushq	%r14
100004708: 41 55                       	pushq	%r13
10000470a: 41 54                       	pushq	%r12
10000470c: 53                          	pushq	%rbx
10000470d: 48 83 ec 58                 	subq	$0x58, %rsp
100004711: 48 85 ff                    	testq	%rdi, %rdi
100004714: 0f 84 2e 04 00 00           	je	0x100004b48 <_mb_win_address_wait_wait+0x448>
10000471a: 4c 8b 37                    	movq	(%rdi), %r14
10000471d: 4d 85 f6                    	testq	%r14, %r14
100004720: 0f 84 22 04 00 00           	je	0x100004b48 <_mb_win_address_wait_wait+0x448>
100004726: 41 83 be cc 00 00 00 00     	cmpl	$0x0, 0xcc(%r14)
10000472e: 0f 85 14 04 00 00           	jne	0x100004b48 <_mb_win_address_wait_wait+0x448>
100004734: 45 89 c7                    	movl	%r8d, %r15d
100004737: 49 89 cd                    	movq	%rcx, %r13
10000473a: 48 89 d3                    	movq	%rdx, %rbx
10000473d: 49 89 f4                    	movq	%rsi, %r12
100004740: 49 8b be 80 00 00 00        	movq	0x80(%r14), %rdi
100004747: e8 d4 d9 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000474c: 49 8d 55 ff                 	leaq	-0x1(%r13), %rdx
100004750: 48 83 fa 02                 	cmpq	$0x2, %rdx
100004754: 40 0f 92 c6                 	setb	%sil
100004758: 49 8d 4d fc                 	leaq	-0x4(%r13), %rcx
10000475c: 48 f7 c1 fb ff ff ff        	testq	$-0x5, %rcx
100004763: 0f 94 c1                    	sete	%cl
100004766: 4c 85 e2                    	testq	%r12, %rdx
100004769: 75 4d                       	jne	0x1000047b8 <_mb_win_address_wait_wait+0xb8>
10000476b: 48 85 db                    	testq	%rbx, %rbx
10000476e: 74 48                       	je	0x1000047b8 <_mb_win_address_wait_wait+0xb8>
100004770: 4d 85 e4                    	testq	%r12, %r12
100004773: 74 43                       	je	0x1000047b8 <_mb_win_address_wait_wait+0xb8>
100004775: 40 08 f1                    	orb	%sil, %cl
100004778: 74 3e                       	je	0x1000047b8 <_mb_win_address_wait_wait+0xb8>
10000477a: 4c 89 65 d0                 	movq	%r12, -0x30(%rbp)
10000477e: 44 89 7d cc                 	movl	%r15d, -0x34(%rbp)
100004782: 49 89 c4                    	movq	%rax, %r12
100004785: 4d 85 ed                    	testq	%r13, %r13
100004788: 74 3b                       	je	0x1000047c5 <_mb_win_address_wait_wait+0xc5>
10000478a: 31 c9                       	xorl	%ecx, %ecx
10000478c: 31 c0                       	xorl	%eax, %eax
10000478e: 45 31 ff                    	xorl	%r15d, %r15d
100004791: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
1000047a0: 0f b6 14 03                 	movzbl	(%rbx,%rax), %edx
1000047a4: 48 d3 e2                    	shlq	%cl, %rdx
1000047a7: 49 09 d7                    	orq	%rdx, %r15
1000047aa: 48 ff c0                    	incq	%rax
1000047ad: 48 83 c1 08                 	addq	$0x8, %rcx
1000047b1: 49 39 c5                    	cmpq	%rax, %r13
1000047b4: 75 ea                       	jne	0x1000047a0 <_mb_win_address_wait_wait+0xa0>
1000047b6: eb 10                       	jmp	0x1000047c8 <_mb_win_address_wait_wait+0xc8>
1000047b8: c7 00 57 00 00 00           	movl	$0x57, (%rax)
1000047be: 31 c0                       	xorl	%eax, %eax
1000047c0: e9 f8 02 00 00              	jmp	0x100004abd <_mb_win_address_wait_wait+0x3bd>
1000047c5: 45 31 ff                    	xorl	%r15d, %r15d
1000047c8: 41 8b 1c 24                 	movl	(%r12), %ebx
1000047cc: 4c 89 f7                    	movq	%r14, %rdi
1000047cf: e8 e4 3e 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000047d4: 85 c0                       	testl	%eax, %eax
1000047d6: 0f 85 6c 03 00 00           	jne	0x100004b48 <_mb_win_address_wait_wait+0x448>
1000047dc: 49 83 fd 04                 	cmpq	$0x4, %r13
1000047e0: 0f 84 af 02 00 00           	je	0x100004a95 <_mb_win_address_wait_wait+0x395>
1000047e6: 49 83 fd 02                 	cmpq	$0x2, %r13
1000047ea: 74 1e                       	je	0x10000480a <_mb_win_address_wait_wait+0x10a>
1000047ec: 49 83 fd 01                 	cmpq	$0x1, %r13
1000047f0: 0f 85 d6 02 00 00           	jne	0x100004acc <_mb_win_address_wait_wait+0x3cc>
1000047f6: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
1000047fa: 0f b6 00                    	movzbl	(%rax), %eax
1000047fd: 0f b6 c0                    	movzbl	%al, %eax
100004800: 4c 39 f8                    	cmpq	%r15, %rax
100004803: 74 18                       	je	0x10000481d <_mb_win_address_wait_wait+0x11d>
100004805: e9 9a 02 00 00              	jmp	0x100004aa4 <_mb_win_address_wait_wait+0x3a4>
10000480a: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
10000480e: 0f b7 00                    	movzwl	(%rax), %eax
100004811: 0f b7 c0                    	movzwl	%ax, %eax
100004814: 4c 39 f8                    	cmpq	%r15, %rax
100004817: 0f 85 87 02 00 00           	jne	0x100004aa4 <_mb_win_address_wait_wait+0x3a4>
10000481d: 8b 45 cc                    	movl	-0x34(%rbp), %eax
100004820: 85 c0                       	testl	%eax, %eax
100004822: 4d 89 e5                    	movq	%r12, %r13
100004825: 0f 84 b3 02 00 00           	je	0x100004ade <_mb_win_address_wait_wait+0x3de>
10000482b: 83 f8 ff                    	cmpl	$-0x1, %eax
10000482e: 0f 84 c3 02 00 00           	je	0x100004af7 <_mb_win_address_wait_wait+0x3f7>
100004834: 89 c0                       	movl	%eax, %eax
100004836: 4c 69 f8 40 42 0f 00        	imulq	$0xf4240, %rax, %r15    ## imm = 0xF4240
10000483d: 48 8d 75 a0                 	leaq	-0x60(%rbp), %rsi
100004841: bf 06 00 00 00              	movl	$0x6, %edi
100004846: e8 d7 3d 00 00              	callq	0x100008622 <_sysconf+0x100008622>
10000484b: b1 01                       	movb	$0x1, %cl
10000484d: 85 c0                       	testl	%eax, %eax
10000484f: 75 2c                       	jne	0x10000487d <_mb_win_address_wait_wait+0x17d>
100004851: 48 8b 55 a0                 	movq	-0x60(%rbp), %rdx
100004855: 48 8b 45 a8                 	movq	-0x58(%rbp), %rax
100004859: 48 3d 00 ca 9a 3b           	cmpq	$0x3b9aca00, %rax       ## imm = 0x3B9ACA00
10000485f: 40 0f 93 c6                 	setae	%sil
100004863: 48 bf 09 fa 82 4b 04 00 00 00       	movabsq	$0x44b82fa09, %rdi ## imm = 0x44B82FA09
10000486d: 48 39 fa                    	cmpq	%rdi, %rdx
100004870: 40 0f 97 c7                 	seta	%dil
100004874: 40 08 f7                    	orb	%sil, %dil
100004877: 0f 84 b0 02 00 00           	je	0x100004b2d <_mb_win_address_wait_wait+0x42d>
10000487d: 41 89 dc                    	movl	%ebx, %r12d
100004880: 31 f6                       	xorl	%esi, %esi
100004882: 48 89 75 c0                 	movq	%rsi, -0x40(%rbp)
100004886: 49 01 f7                    	addq	%rsi, %r15
100004889: 0f 92 c3                    	setb	%bl
10000488c: 08 cb                       	orb	%cl, %bl
10000488e: f6 c3 01                    	testb	$0x1, %bl
100004891: 74 1b                       	je	0x1000048ae <_mb_win_address_wait_wait+0x1ae>
100004893: 4c 89 f7                    	movq	%r14, %rdi
100004896: e8 23 3e 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000489b: 85 c0                       	testl	%eax, %eax
10000489d: 0f 85 a5 02 00 00           	jne	0x100004b48 <_mb_win_address_wait_wait+0x448>
1000048a3: 41 c7 45 00 1f 00 00 00     	movl	$0x1f, (%r13)
1000048ab: 45 31 ff                    	xorl	%r15d, %r15d
1000048ae: 31 c0                       	xorl	%eax, %eax
1000048b0: f6 c3 01                    	testb	$0x1, %bl
1000048b3: 44 89 e3                    	movl	%r12d, %ebx
1000048b6: 48 8b 4d d0                 	movq	-0x30(%rbp), %rcx
1000048ba: 0f 85 fd 01 00 00           	jne	0x100004abd <_mb_win_address_wait_wait+0x3bd>
1000048c0: 49 83 be b0 00 00 00 ff     	cmpq	$-0x1, 0xb0(%r14)
1000048c8: 0f 84 46 02 00 00           	je	0x100004b14 <_mb_win_address_wait_wait+0x414>
1000048ce: 4c 89 6d 88                 	movq	%r13, -0x78(%rbp)
1000048d2: 89 5d bc                    	movl	%ebx, -0x44(%rbp)
1000048d5: 48 c7 45 a0 00 00 00 00     	movq	$0x0, -0x60(%rbp)
1000048dd: 48 89 4d a8                 	movq	%rcx, -0x58(%rbp)
1000048e1: c7 45 b0 00 00 00 00        	movl	$0x0, -0x50(%rbp)
1000048e8: 49 8b 86 a8 00 00 00        	movq	0xa8(%r14), %rax
1000048ef: 4d 8d a6 a0 00 00 00        	leaq	0xa0(%r14), %r12
1000048f6: 48 85 c0                    	testq	%rax, %rax
1000048f9: 49 0f 44 c4                 	cmoveq	%r12, %rax
1000048fd: 4c 8d 6d a0                 	leaq	-0x60(%rbp), %r13
100004901: 4c 89 28                    	movq	%r13, (%rax)
100004904: 4d 89 ae a8 00 00 00        	movq	%r13, 0xa8(%r14)
10000490b: 49 ff 86 b0 00 00 00        	incq	0xb0(%r14)
100004912: 49 8d 46 40                 	leaq	0x40(%r14), %rax
100004916: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
10000491a: 31 db                       	xorl	%ebx, %ebx
10000491c: eb 0c                       	jmp	0x10000492a <_mb_win_address_wait_wait+0x22a>
10000491e: 66 90                       	nop
100004920: b0 01                       	movb	$0x1, %al
100004922: 84 c0                       	testb	%al, %al
100004924: 0f 84 05 01 00 00           	je	0x100004a2f <_mb_win_address_wait_wait+0x32f>
10000492a: 83 7d b0 00                 	cmpl	$0x0, -0x50(%rbp)
10000492e: 0f 85 fb 00 00 00           	jne	0x100004a2f <_mb_win_address_wait_wait+0x32f>
100004934: 83 7d cc ff                 	cmpl	$-0x1, -0x34(%rbp)
100004938: 0f 84 92 00 00 00           	je	0x1000049d0 <_mb_win_address_wait_wait+0x2d0>
10000493e: bf 06 00 00 00              	movl	$0x6, %edi
100004943: 48 8d 75 90                 	leaq	-0x70(%rbp), %rsi
100004947: e8 d6 3c 00 00              	callq	0x100008622 <_sysconf+0x100008622>
10000494c: 85 c0                       	testl	%eax, %eax
10000494e: 75 27                       	jne	0x100004977 <_mb_win_address_wait_wait+0x277>
100004950: 48 8b 4d 90                 	movq	-0x70(%rbp), %rcx
100004954: 48 8b 45 98                 	movq	-0x68(%rbp), %rax
100004958: 48 3d 00 ca 9a 3b           	cmpq	$0x3b9aca00, %rax       ## imm = 0x3B9ACA00
10000495e: 0f 93 c2                    	setae	%dl
100004961: 48 be 09 fa 82 4b 04 00 00 00       	movabsq	$0x44b82fa09, %rsi ## imm = 0x44B82FA09
10000496b: 48 39 f1                    	cmpq	%rsi, %rcx
10000496e: 40 0f 97 c6                 	seta	%sil
100004972: 40 08 d6                    	orb	%dl, %sil
100004975: 74 7f                       	je	0x1000049f6 <_mb_win_address_wait_wait+0x2f6>
100004977: 31 c0                       	xorl	%eax, %eax
100004979: 84 c0                       	testb	%al, %al
10000497b: 0f 84 9f 00 00 00           	je	0x100004a20 <_mb_win_address_wait_wait+0x320>
100004981: 4c 89 f9                    	movq	%r15, %rcx
100004984: 48 2b 4d c0                 	subq	-0x40(%rbp), %rcx
100004988: 76 65                       	jbe	0x1000049ef <_mb_win_address_wait_wait+0x2ef>
10000498a: 48 89 c8                    	movq	%rcx, %rax
10000498d: 48 c1 e8 09                 	shrq	$0x9, %rax
100004991: 48 ba 53 5a 9b a0 2f b8 44 00       	movabsq	$0x44b82fa09b5a53, %rdx ## imm = 0x44B82FA09B5A53
10000499b: 48 f7 e2                    	mulq	%rdx
10000499e: 48 c1 ea 0b                 	shrq	$0xb, %rdx
1000049a2: 48 89 55 90                 	movq	%rdx, -0x70(%rbp)
1000049a6: 48 69 c2 00 ca 9a 3b        	imulq	$0x3b9aca00, %rdx, %rax ## imm = 0x3B9ACA00
1000049ad: 48 29 c1                    	subq	%rax, %rcx
1000049b0: 48 89 4d 98                 	movq	%rcx, -0x68(%rbp)
1000049b4: 48 8b 7d d0                 	movq	-0x30(%rbp), %rdi
1000049b8: 4c 89 f6                    	movq	%r14, %rsi
1000049bb: 48 8d 55 90                 	leaq	-0x70(%rbp), %rdx
1000049bf: e8 ac 3c 00 00              	callq	0x100008670 <_sysconf+0x100008670>
1000049c4: 83 f8 3c                    	cmpl	$0x3c, %eax
1000049c7: 75 1c                       	jne	0x1000049e5 <_mb_win_address_wait_wait+0x2e5>
1000049c9: e9 52 ff ff ff              	jmp	0x100004920 <_mb_win_address_wait_wait+0x220>
1000049ce: 66 90                       	nop
1000049d0: 48 8b 7d d0                 	movq	-0x30(%rbp), %rdi
1000049d4: 4c 89 f6                    	movq	%r14, %rsi
1000049d7: e8 9a 3c 00 00              	callq	0x100008676 <_sysconf+0x100008676>
1000049dc: 83 f8 3c                    	cmpl	$0x3c, %eax
1000049df: 0f 84 3b ff ff ff           	je	0x100004920 <_mb_win_address_wait_wait+0x220>
1000049e5: 85 c0                       	testl	%eax, %eax
1000049e7: 0f 84 33 ff ff ff           	je	0x100004920 <_mb_win_address_wait_wait+0x220>
1000049ed: eb 31                       	jmp	0x100004a20 <_mb_win_address_wait_wait+0x320>
1000049ef: bb b4 05 00 00              	movl	$0x5b4, %ebx            ## imm = 0x5B4
1000049f4: eb 2f                       	jmp	0x100004a25 <_mb_win_address_wait_wait+0x325>
1000049f6: 48 69 c9 00 ca 9a 3b        	imulq	$0x3b9aca00, %rcx, %rcx ## imm = 0x3B9ACA00
1000049fd: 48 89 ca                    	movq	%rcx, %rdx
100004a00: 48 f7 d2                    	notq	%rdx
100004a03: 48 01 c1                    	addq	%rax, %rcx
100004a06: 48 39 d0                    	cmpq	%rdx, %rax
100004a09: 0f 96 c0                    	setbe	%al
100004a0c: 48 8b 55 c0                 	movq	-0x40(%rbp), %rdx
100004a10: 48 0f 46 d1                 	cmovbeq	%rcx, %rdx
100004a14: 48 89 55 c0                 	movq	%rdx, -0x40(%rbp)
100004a18: 84 c0                       	testb	%al, %al
100004a1a: 0f 85 61 ff ff ff           	jne	0x100004981 <_mb_win_address_wait_wait+0x281>
100004a20: bb 1f 00 00 00              	movl	$0x1f, %ebx
100004a25: 31 c0                       	xorl	%eax, %eax
100004a27: 84 c0                       	testb	%al, %al
100004a29: 0f 85 fb fe ff ff           	jne	0x10000492a <_mb_win_address_wait_wait+0x22a>
100004a2f: 31 d2                       	xorl	%edx, %edx
100004a31: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
100004a40: 48 89 d0                    	movq	%rdx, %rax
100004a43: 4c 89 e1                    	movq	%r12, %rcx
100004a46: 4d 8b 24 24                 	movq	(%r12), %r12
100004a4a: 4c 89 e2                    	movq	%r12, %rdx
100004a4d: 4d 39 ec                    	cmpq	%r13, %r12
100004a50: 75 ee                       	jne	0x100004a40 <_mb_win_address_wait_wait+0x340>
100004a52: 48 8b 55 a0                 	movq	-0x60(%rbp), %rdx
100004a56: 48 89 11                    	movq	%rdx, (%rcx)
100004a59: 4d 39 ae a8 00 00 00        	cmpq	%r13, 0xa8(%r14)
100004a60: 75 07                       	jne	0x100004a69 <_mb_win_address_wait_wait+0x369>
100004a62: 49 89 86 a8 00 00 00        	movq	%rax, 0xa8(%r14)
100004a69: 49 ff 8e b0 00 00 00        	decq	0xb0(%r14)
100004a70: 4c 89 f7                    	movq	%r14, %rdi
100004a73: e8 46 3c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004a78: 85 c0                       	testl	%eax, %eax
100004a7a: 48 8b 55 88                 	movq	-0x78(%rbp), %rdx
100004a7e: 0f 85 c4 00 00 00           	jne	0x100004b48 <_mb_win_address_wait_wait+0x448>
100004a84: 31 c0                       	xorl	%eax, %eax
100004a86: 85 db                       	testl	%ebx, %ebx
100004a88: 0f 94 c0                    	sete	%al
100004a8b: 8b 4d bc                    	movl	-0x44(%rbp), %ecx
100004a8e: 0f 45 cb                    	cmovnel	%ebx, %ecx
100004a91: 89 0a                       	movl	%ecx, (%rdx)
100004a93: eb 28                       	jmp	0x100004abd <_mb_win_address_wait_wait+0x3bd>
100004a95: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
100004a99: 8b 00                       	movl	(%rax), %eax
100004a9b: 4c 39 f8                    	cmpq	%r15, %rax
100004a9e: 0f 84 79 fd ff ff           	je	0x10000481d <_mb_win_address_wait_wait+0x11d>
100004aa4: 4c 89 f7                    	movq	%r14, %rdi
100004aa7: e8 12 3c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004aac: 85 c0                       	testl	%eax, %eax
100004aae: 0f 85 94 00 00 00           	jne	0x100004b48 <_mb_win_address_wait_wait+0x448>
100004ab4: 41 89 1c 24                 	movl	%ebx, (%r12)
100004ab8: b8 01 00 00 00              	movl	$0x1, %eax
100004abd: 48 83 c4 58                 	addq	$0x58, %rsp
100004ac1: 5b                          	popq	%rbx
100004ac2: 41 5c                       	popq	%r12
100004ac4: 41 5d                       	popq	%r13
100004ac6: 41 5e                       	popq	%r14
100004ac8: 41 5f                       	popq	%r15
100004aca: 5d                          	popq	%rbp
100004acb: c3                          	retq
100004acc: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
100004ad0: 48 8b 00                    	movq	(%rax), %rax
100004ad3: 4c 39 f8                    	cmpq	%r15, %rax
100004ad6: 0f 84 41 fd ff ff           	je	0x10000481d <_mb_win_address_wait_wait+0x11d>
100004adc: eb c6                       	jmp	0x100004aa4 <_mb_win_address_wait_wait+0x3a4>
100004ade: 4c 89 f7                    	movq	%r14, %rdi
100004ae1: e8 d8 3b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004ae6: 85 c0                       	testl	%eax, %eax
100004ae8: 75 5e                       	jne	0x100004b48 <_mb_win_address_wait_wait+0x448>
100004aea: 41 c7 45 00 b4 05 00 00     	movl	$0x5b4, (%r13)          ## imm = 0x5B4
100004af2: e9 c7 fc ff ff              	jmp	0x1000047be <_mb_win_address_wait_wait+0xbe>
100004af7: 48 c7 45 c0 00 00 00 00     	movq	$0x0, -0x40(%rbp)
100004aff: 45 31 ff                    	xorl	%r15d, %r15d
100004b02: 48 8b 4d d0                 	movq	-0x30(%rbp), %rcx
100004b06: 49 83 be b0 00 00 00 ff     	cmpq	$-0x1, 0xb0(%r14)
100004b0e: 0f 85 ba fd ff ff           	jne	0x1000048ce <_mb_win_address_wait_wait+0x1ce>
100004b14: 4c 89 f7                    	movq	%r14, %rdi
100004b17: e8 a2 3b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004b1c: 85 c0                       	testl	%eax, %eax
100004b1e: 75 28                       	jne	0x100004b48 <_mb_win_address_wait_wait+0x448>
100004b20: 41 c7 45 00 08 00 00 00     	movl	$0x8, (%r13)
100004b28: e9 91 fc ff ff              	jmp	0x1000047be <_mb_win_address_wait_wait+0xbe>
100004b2d: 41 89 dc                    	movl	%ebx, %r12d
100004b30: 48 69 d2 00 ca 9a 3b        	imulq	$0x3b9aca00, %rdx, %rdx ## imm = 0x3B9ACA00
100004b37: 31 f6                       	xorl	%esi, %esi
100004b39: 48 01 c2                    	addq	%rax, %rdx
100004b3c: 0f 92 c1                    	setb	%cl
100004b3f: 48 0f 43 f2                 	cmovaeq	%rdx, %rsi
100004b43: e9 3a fd ff ff              	jmp	0x100004882 <_mb_win_address_wait_wait+0x182>
100004b48: e8 c3 3a 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004b4d: 0f 1f 00                    	nopl	(%rax)

0000000100004b50 <_mb_win_address_wait_wake_single>:
100004b50: 55                          	pushq	%rbp
100004b51: 48 89 e5                    	movq	%rsp, %rbp
100004b54: 31 d2                       	xorl	%edx, %edx
100004b56: 5d                          	popq	%rbp
100004b57: e9 04 00 00 00              	jmp	0x100004b60 <_wake>
100004b5c: 0f 1f 40 00                 	nopl	(%rax)

0000000100004b60 <_wake>:
100004b60: 55                          	pushq	%rbp
100004b61: 48 89 e5                    	movq	%rsp, %rbp
100004b64: 41 57                       	pushq	%r15
100004b66: 41 56                       	pushq	%r14
100004b68: 41 55                       	pushq	%r13
100004b6a: 41 54                       	pushq	%r12
100004b6c: 53                          	pushq	%rbx
100004b6d: 50                          	pushq	%rax
100004b6e: 48 85 ff                    	testq	%rdi, %rdi
100004b71: 0f 84 af 00 00 00           	je	0x100004c26 <_wake+0xc6>
100004b77: 48 8b 1f                    	movq	(%rdi), %rbx
100004b7a: 48 85 db                    	testq	%rbx, %rbx
100004b7d: 0f 84 a3 00 00 00           	je	0x100004c26 <_wake+0xc6>
100004b83: 83 bb cc 00 00 00 00        	cmpl	$0x0, 0xcc(%rbx)
100004b8a: 0f 85 96 00 00 00           	jne	0x100004c26 <_wake+0xc6>
100004b90: 41 89 d7                    	movl	%edx, %r15d
100004b93: 49 89 f4                    	movq	%rsi, %r12
100004b96: 48 8b bb 80 00 00 00        	movq	0x80(%rbx), %rdi
100004b9d: e8 7e d5 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100004ba2: 49 89 c6                    	movq	%rax, %r14
100004ba5: 44 8b 28                    	movl	(%rax), %r13d
100004ba8: 48 89 df                    	movq	%rbx, %rdi
100004bab: e8 08 3b 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100004bb0: 85 c0                       	testl	%eax, %eax
100004bb2: 75 72                       	jne	0x100004c26 <_wake+0xc6>
100004bb4: 48 8b 83 a0 00 00 00        	movq	0xa0(%rbx), %rax
100004bbb: 48 85 c0                    	testq	%rax, %rax
100004bbe: 74 48                       	je	0x100004c08 <_wake+0xa8>
100004bc0: 31 c9                       	xorl	%ecx, %ecx
100004bc2: eb 14                       	jmp	0x100004bd8 <_wake+0x78>
100004bc4: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
100004bd0: 48 8b 00                    	movq	(%rax), %rax
100004bd3: 48 85 c0                    	testq	%rax, %rax
100004bd6: 74 1f                       	je	0x100004bf7 <_wake+0x97>
100004bd8: 4c 39 60 08                 	cmpq	%r12, 0x8(%rax)
100004bdc: 75 f2                       	jne	0x100004bd0 <_wake+0x70>
100004bde: 83 78 10 00                 	cmpl	$0x0, 0x10(%rax)
100004be2: 75 ec                       	jne	0x100004bd0 <_wake+0x70>
100004be4: c7 40 10 01 00 00 00        	movl	$0x1, 0x10(%rax)
100004beb: b9 01 00 00 00              	movl	$0x1, %ecx
100004bf0: 45 85 ff                    	testl	%r15d, %r15d
100004bf3: 75 db                       	jne	0x100004bd0 <_wake+0x70>
100004bf5: eb 04                       	jmp	0x100004bfb <_wake+0x9b>
100004bf7: 85 c9                       	testl	%ecx, %ecx
100004bf9: 74 0d                       	je	0x100004c08 <_wake+0xa8>
100004bfb: 48 8d 7b 40                 	leaq	0x40(%rbx), %rdi
100004bff: e8 54 3a 00 00              	callq	0x100008658 <_sysconf+0x100008658>
100004c04: 85 c0                       	testl	%eax, %eax
100004c06: 75 1e                       	jne	0x100004c26 <_wake+0xc6>
100004c08: 48 89 df                    	movq	%rbx, %rdi
100004c0b: e8 ae 3a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004c10: 85 c0                       	testl	%eax, %eax
100004c12: 75 12                       	jne	0x100004c26 <_wake+0xc6>
100004c14: 45 89 2e                    	movl	%r13d, (%r14)
100004c17: 48 83 c4 08                 	addq	$0x8, %rsp
100004c1b: 5b                          	popq	%rbx
100004c1c: 41 5c                       	popq	%r12
100004c1e: 41 5d                       	popq	%r13
100004c20: 41 5e                       	popq	%r14
100004c22: 41 5f                       	popq	%r15
100004c24: 5d                          	popq	%rbp
100004c25: c3                          	retq
100004c26: e8 e5 39 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004c2b: 0f 1f 44 00 00              	nopl	(%rax,%rax)

0000000100004c30 <_mb_win_address_wait_wake_all>:
100004c30: 55                          	pushq	%rbp
100004c31: 48 89 e5                    	movq	%rsp, %rbp
100004c34: ba 01 00 00 00              	movl	$0x1, %edx
100004c39: 5d                          	popq	%rbp
100004c3a: e9 21 ff ff ff              	jmp	0x100004b60 <_wake>
100004c3f: 00 55 48                    	addb	%dl, 0x48(%rbp)

0000000100004c40 <_mb_win_srw_open>:
100004c40: 55                          	pushq	%rbp
100004c41: 48 89 e5                    	movq	%rsp, %rbp
100004c44: 41 57                       	pushq	%r15
100004c46: 41 56                       	pushq	%r14
100004c48: 41 55                       	pushq	%r13
100004c4a: 41 54                       	pushq	%r12
100004c4c: 53                          	pushq	%rbx
100004c4d: 50                          	pushq	%rax
100004c4e: 48 85 ff                    	testq	%rdi, %rdi
100004c51: 0f 94 c1                    	sete	%cl
100004c54: 48 85 f6                    	testq	%rsi, %rsi
100004c57: 41 0f 94 c0                 	sete	%r8b
100004c5b: b8 01 00 00 00              	movl	$0x1, %eax
100004c60: 41 08 c8                    	orb	%cl, %r8b
100004c63: 75 18                       	jne	0x100004c7d <_mb_win_srw_open+0x3d>
100004c65: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100004c69: 74 12                       	je	0x100004c7d <_mb_win_srw_open+0x3d>
100004c6b: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100004c70: 74 0b                       	je	0x100004c7d <_mb_win_srw_open+0x3d>
100004c72: b8 02 00 00 00              	movl	$0x2, %eax
100004c77: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
100004c7b: 74 0f                       	je	0x100004c8c <_mb_win_srw_open+0x4c>
100004c7d: 48 83 c4 08                 	addq	$0x8, %rsp
100004c81: 5b                          	popq	%rbx
100004c82: 41 5c                       	popq	%r12
100004c84: 41 5d                       	popq	%r13
100004c86: 41 5e                       	popq	%r14
100004c88: 41 5f                       	popq	%r15
100004c8a: 5d                          	popq	%rbp
100004c8b: c3                          	retq
100004c8c: 49 89 f5                    	movq	%rsi, %r13
100004c8f: 49 89 fc                    	movq	%rdi, %r12
100004c92: 49 89 d7                    	movq	%rdx, %r15
100004c95: bf 01 00 00 00              	movl	$0x1, %edi
100004c9a: be 80 00 00 00              	movl	$0x80, %esi
100004c9f: e8 78 39 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
100004ca4: 48 85 c0                    	testq	%rax, %rax
100004ca7: 74 2b                       	je	0x100004cd4 <_mb_win_srw_open+0x94>
100004ca9: 48 89 c3                    	movq	%rax, %rbx
100004cac: 48 89 c7                    	movq	%rax, %rdi
100004caf: 31 f6                       	xorl	%esi, %esi
100004cb1: e8 fc 39 00 00              	callq	0x1000086b2 <_sysconf+0x1000086b2>
100004cb6: 85 c0                       	testl	%eax, %eax
100004cb8: 74 2d                       	je	0x100004ce7 <_mb_win_srw_open+0xa7>
100004cba: 41 89 c6                    	movl	%eax, %r14d
100004cbd: 48 89 df                    	movq	%rbx, %rdi
100004cc0: e8 75 39 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100004cc5: b8 04 00 00 00              	movl	$0x4, %eax
100004cca: 4d 85 ff                    	testq	%r15, %r15
100004ccd: 74 ae                       	je	0x100004c7d <_mb_win_srw_open+0x3d>
100004ccf: 45 89 37                    	movl	%r14d, (%r15)
100004cd2: eb a9                       	jmp	0x100004c7d <_mb_win_srw_open+0x3d>
100004cd4: b8 04 00 00 00              	movl	$0x4, %eax
100004cd9: 4d 85 ff                    	testq	%r15, %r15
100004cdc: 74 9f                       	je	0x100004c7d <_mb_win_srw_open+0x3d>
100004cde: 41 c7 07 0c 00 00 00        	movl	$0xc, (%r15)
100004ce5: eb 96                       	jmp	0x100004c7d <_mb_win_srw_open+0x3d>
100004ce7: 49 8b 7d 00                 	movq	(%r13), %rdi
100004ceb: 48 89 de                    	movq	%rbx, %rsi
100004cee: 48 83 c6 48                 	addq	$0x48, %rsi
100004cf2: e8 a9 d2 ff ff              	callq	0x100001fa0 <_mb_win_threads_acquire>
100004cf7: 85 c0                       	testl	%eax, %eax
100004cf9: 74 1e                       	je	0x100004d19 <_mb_win_srw_open+0xd9>
100004cfb: 48 89 df                    	movq	%rbx, %rdi
100004cfe: e8 a9 39 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100004d03: 85 c0                       	testl	%eax, %eax
100004d05: 75 3d                       	jne	0x100004d44 <_mb_win_srw_open+0x104>
100004d07: 48 89 df                    	movq	%rbx, %rdi
100004d0a: e8 2b 39 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100004d0f: b8 02 00 00 00              	movl	$0x2, %eax
100004d14: e9 64 ff ff ff              	jmp	0x100004c7d <_mb_win_srw_open+0x3d>
100004d19: 49 8b 45 00                 	movq	(%r13), %rax
100004d1d: 48 89 43 40                 	movq	%rax, 0x40(%rbx)
100004d21: 49 8b 45 08                 	movq	0x8(%r13), %rax
100004d25: 48 89 43 70                 	movq	%rax, 0x70(%rbx)
100004d29: 49 89 1c 24                 	movq	%rbx, (%r12)
100004d2d: 31 c0                       	xorl	%eax, %eax
100004d2f: 4d 85 ff                    	testq	%r15, %r15
100004d32: 0f 84 45 ff ff ff           	je	0x100004c7d <_mb_win_srw_open+0x3d>
100004d38: 41 c7 07 00 00 00 00        	movl	$0x0, (%r15)
100004d3f: e9 39 ff ff ff              	jmp	0x100004c7d <_mb_win_srw_open+0x3d>
100004d44: e8 c7 38 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004d49: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100004d50 <_mb_win_srw_close>:
100004d50: 55                          	pushq	%rbp
100004d51: 48 89 e5                    	movq	%rsp, %rbp
100004d54: 41 57                       	pushq	%r15
100004d56: 41 56                       	pushq	%r14
100004d58: 41 54                       	pushq	%r12
100004d5a: 53                          	pushq	%rbx
100004d5b: 48 85 ff                    	testq	%rdi, %rdi
100004d5e: 74 3d                       	je	0x100004d9d <_mb_win_srw_close+0x4d>
100004d60: 48 8b 1f                    	movq	(%rdi), %rbx
100004d63: 48 85 db                    	testq	%rbx, %rbx
100004d66: 74 3c                       	je	0x100004da4 <_mb_win_srw_close+0x54>
100004d68: 49 89 ff                    	movq	%rdi, %r15
100004d6b: 49 89 f6                    	movq	%rsi, %r14
100004d6e: 48 89 df                    	movq	%rbx, %rdi
100004d71: e8 42 39 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100004d76: 85 c0                       	testl	%eax, %eax
100004d78: 0f 85 c3 00 00 00           	jne	0x100004e41 <_mb_win_srw_close+0xf1>
100004d7e: 48 83 7b 58 00              	cmpq	$0x0, 0x58(%rbx)
100004d83: 74 26                       	je	0x100004dab <_mb_win_srw_close+0x5b>
100004d85: 48 89 df                    	movq	%rbx, %rdi
100004d88: e8 31 39 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004d8d: 89 c7                       	movl	%eax, %edi
100004d8f: b8 03 00 00 00              	movl	$0x3, %eax
100004d94: 85 ff                       	testl	%edi, %edi
100004d96: 74 4c                       	je	0x100004de4 <_mb_win_srw_close+0x94>
100004d98: e8 63 35 00 00              	callq	0x100008300 <_mb_win_srw_close.cold.1>
100004d9d: b8 01 00 00 00              	movl	$0x1, %eax
100004da2: eb 40                       	jmp	0x100004de4 <_mb_win_srw_close+0x94>
100004da4: b8 02 00 00 00              	movl	$0x2, %eax
100004da9: eb 39                       	jmp	0x100004de4 <_mb_win_srw_close+0x94>
100004dab: 4c 8b 63 60                 	movq	0x60(%rbx), %r12
100004daf: 48 89 df                    	movq	%rbx, %rdi
100004db2: e8 07 39 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004db7: 85 c0                       	testl	%eax, %eax
100004db9: 0f 85 87 00 00 00           	jne	0x100004e46 <_mb_win_srw_close+0xf6>
100004dbf: b8 03 00 00 00              	movl	$0x3, %eax
100004dc4: 4d 85 e4                    	testq	%r12, %r12
100004dc7: 75 1b                       	jne	0x100004de4 <_mb_win_srw_close+0x94>
100004dc9: 48 89 df                    	movq	%rbx, %rdi
100004dcc: e8 db 38 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100004dd1: 85 c0                       	testl	%eax, %eax
100004dd3: 74 18                       	je	0x100004ded <_mb_win_srw_close+0x9d>
100004dd5: 89 c1                       	movl	%eax, %ecx
100004dd7: b8 04 00 00 00              	movl	$0x4, %eax
100004ddc: 4d 85 f6                    	testq	%r14, %r14
100004ddf: 74 03                       	je	0x100004de4 <_mb_win_srw_close+0x94>
100004de1: 41 89 0e                    	movl	%ecx, (%r14)
100004de4: 5b                          	popq	%rbx
100004de5: 41 5c                       	popq	%r12
100004de7: 41 5e                       	popq	%r14
100004de9: 41 5f                       	popq	%r15
100004deb: 5d                          	popq	%rbp
100004dec: c3                          	retq
100004ded: 4c 8b 25 24 52 00 00        	movq	0x5224(%rip), %r12      ## 0x10000a018 <_sysconf+0x10000a018>
100004df4: 49 8b 0c 24                 	movq	(%r12), %rcx
100004df8: 48 8d 3d 39 40 00 00        	leaq	0x4039(%rip), %rdi      ## 0x100008e38 <_sysconf+0x100008e38>
100004dff: be 1a 00 00 00              	movl	$0x1a, %esi
100004e04: ba 01 00 00 00              	movl	$0x1, %edx
100004e09: e8 32 38 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100004e0e: 49 8b 3c 24                 	movq	(%r12), %rdi
100004e12: e8 17 38 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100004e17: 48 8d 7b 48                 	leaq	0x48(%rbx), %rdi
100004e1b: e8 50 d2 ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100004e20: 85 c0                       	testl	%eax, %eax
100004e22: 75 1d                       	jne	0x100004e41 <_mb_win_srw_close+0xf1>
100004e24: 48 89 df                    	movq	%rbx, %rdi
100004e27: e8 0e 38 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100004e2c: 49 c7 07 00 00 00 00        	movq	$0x0, (%r15)
100004e33: 31 c9                       	xorl	%ecx, %ecx
100004e35: b8 00 00 00 00              	movl	$0x0, %eax
100004e3a: 4d 85 f6                    	testq	%r14, %r14
100004e3d: 75 a2                       	jne	0x100004de1 <_mb_win_srw_close+0x91>
100004e3f: eb a3                       	jmp	0x100004de4 <_mb_win_srw_close+0x94>
100004e41: e8 ca 37 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004e46: 89 c7                       	movl	%eax, %edi
100004e48: e8 e3 34 00 00              	callq	0x100008330 <_mb_win_srw_close.cold.2>
100004e4d: 0f 1f 00                    	nopl	(%rax)

0000000100004e50 <_mb_win_srw_acquire_pin>:
100004e50: 55                          	pushq	%rbp
100004e51: 48 89 e5                    	movq	%rsp, %rbp
100004e54: 41 57                       	pushq	%r15
100004e56: 41 56                       	pushq	%r14
100004e58: 53                          	pushq	%rbx
100004e59: 50                          	pushq	%rax
100004e5a: 48 85 ff                    	testq	%rdi, %rdi
100004e5d: 0f 94 c1                    	sete	%cl
100004e60: 48 85 f6                    	testq	%rsi, %rsi
100004e63: 0f 94 c2                    	sete	%dl
100004e66: b8 01 00 00 00              	movl	$0x1, %eax
100004e6b: 08 ca                       	orb	%cl, %dl
100004e6d: 0f 85 82 00 00 00           	jne	0x100004ef5 <_mb_win_srw_acquire_pin+0xa5>
100004e73: 48 8b 1f                    	movq	(%rdi), %rbx
100004e76: b8 02 00 00 00              	movl	$0x2, %eax
100004e7b: 48 85 db                    	testq	%rbx, %rbx
100004e7e: 74 75                       	je	0x100004ef5 <_mb_win_srw_acquire_pin+0xa5>
100004e80: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100004e84: 75 6f                       	jne	0x100004ef5 <_mb_win_srw_acquire_pin+0xa5>
100004e86: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100004e8b: 75 68                       	jne	0x100004ef5 <_mb_win_srw_acquire_pin+0xa5>
100004e8d: 49 89 ff                    	movq	%rdi, %r15
100004e90: 49 89 f6                    	movq	%rsi, %r14
100004e93: 48 89 df                    	movq	%rbx, %rdi
100004e96: e8 1d 38 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100004e9b: 85 c0                       	testl	%eax, %eax
100004e9d: 75 61                       	jne	0x100004f00 <_mb_win_srw_acquire_pin+0xb0>
100004e9f: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100004ea3: 48 85 c9                    	testq	%rcx, %rcx
100004ea6: 4c 89 f0                    	movq	%r14, %rax
100004ea9: 4c 89 fa                    	movq	%r15, %rdx
100004eac: 74 10                       	je	0x100004ebe <_mb_win_srw_acquire_pin+0x6e>
100004eae: 66 90                       	nop
100004eb0: 48 39 c1                    	cmpq	%rax, %rcx
100004eb3: 74 2d                       	je	0x100004ee2 <_mb_win_srw_acquire_pin+0x92>
100004eb5: 48 8b 49 08                 	movq	0x8(%rcx), %rcx
100004eb9: 48 85 c9                    	testq	%rcx, %rcx
100004ebc: 75 f2                       	jne	0x100004eb0 <_mb_win_srw_acquire_pin+0x60>
100004ebe: 48 89 10                    	movq	%rdx, (%rax)
100004ec1: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100004ec5: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
100004ec9: 48 89 43 58                 	movq	%rax, 0x58(%rbx)
100004ecd: 48 89 df                    	movq	%rbx, %rdi
100004ed0: e8 e9 37 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004ed5: 89 c7                       	movl	%eax, %edi
100004ed7: 31 c0                       	xorl	%eax, %eax
100004ed9: 85 ff                       	testl	%edi, %edi
100004edb: 74 18                       	je	0x100004ef5 <_mb_win_srw_acquire_pin+0xa5>
100004edd: e8 ae 34 00 00              	callq	0x100008390 <_mb_win_srw_acquire_pin.cold.2>
100004ee2: 48 89 df                    	movq	%rbx, %rdi
100004ee5: e8 d4 37 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004eea: 89 c7                       	movl	%eax, %edi
100004eec: 85 c0                       	testl	%eax, %eax
100004eee: b8 02 00 00 00              	movl	$0x2, %eax
100004ef3: 75 10                       	jne	0x100004f05 <_mb_win_srw_acquire_pin+0xb5>
100004ef5: 48 83 c4 08                 	addq	$0x8, %rsp
100004ef9: 5b                          	popq	%rbx
100004efa: 41 5e                       	popq	%r14
100004efc: 41 5f                       	popq	%r15
100004efe: 5d                          	popq	%rbp
100004eff: c3                          	retq
100004f00: e8 0b 37 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004f05: e8 56 34 00 00              	callq	0x100008360 <_mb_win_srw_acquire_pin.cold.1>
100004f0a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100004f10 <_mb_win_srw_release_pin>:
100004f10: 55                          	pushq	%rbp
100004f11: 48 89 e5                    	movq	%rsp, %rbp
100004f14: 41 56                       	pushq	%r14
100004f16: 53                          	pushq	%rbx
100004f17: 48 85 ff                    	testq	%rdi, %rdi
100004f1a: 74 6f                       	je	0x100004f8b <_mb_win_srw_release_pin+0x7b>
100004f1c: 48 8b 0f                    	movq	(%rdi), %rcx
100004f1f: b8 02 00 00 00              	movl	$0x2, %eax
100004f24: 48 85 c9                    	testq	%rcx, %rcx
100004f27: 74 67                       	je	0x100004f90 <_mb_win_srw_release_pin+0x80>
100004f29: 48 8b 19                    	movq	(%rcx), %rbx
100004f2c: 48 85 db                    	testq	%rbx, %rbx
100004f2f: 74 5f                       	je	0x100004f90 <_mb_win_srw_release_pin+0x80>
100004f31: 49 89 fe                    	movq	%rdi, %r14
100004f34: 48 89 df                    	movq	%rbx, %rdi
100004f37: e8 7c 37 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100004f3c: 85 c0                       	testl	%eax, %eax
100004f3e: 75 6d                       	jne	0x100004fad <_mb_win_srw_release_pin+0x9d>
100004f40: 48 8d 73 58                 	leaq	0x58(%rbx), %rsi
100004f44: 4c 89 f0                    	movq	%r14, %rax
100004f47: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100004f50: 48 89 f1                    	movq	%rsi, %rcx
100004f53: 48 8b 16                    	movq	(%rsi), %rdx
100004f56: 48 85 d2                    	testq	%rdx, %rdx
100004f59: 74 09                       	je	0x100004f64 <_mb_win_srw_release_pin+0x54>
100004f5b: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
100004f5f: 48 39 c2                    	cmpq	%rax, %rdx
100004f62: 75 ec                       	jne	0x100004f50 <_mb_win_srw_release_pin+0x40>
100004f64: 48 85 d2                    	testq	%rdx, %rdx
100004f67: 74 2c                       	je	0x100004f95 <_mb_win_srw_release_pin+0x85>
100004f69: 48 8b 50 08                 	movq	0x8(%rax), %rdx
100004f6d: 48 89 11                    	movq	%rdx, (%rcx)
100004f70: 0f 57 c0                    	xorps	%xmm0, %xmm0
100004f73: 0f 11 00                    	movups	%xmm0, (%rax)
100004f76: 48 89 df                    	movq	%rbx, %rdi
100004f79: e8 40 37 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004f7e: 89 c7                       	movl	%eax, %edi
100004f80: 31 c0                       	xorl	%eax, %eax
100004f82: 85 ff                       	testl	%edi, %edi
100004f84: 74 0a                       	je	0x100004f90 <_mb_win_srw_release_pin+0x80>
100004f86: e8 35 34 00 00              	callq	0x1000083c0 <_mb_win_srw_release_pin.cold.1>
100004f8b: b8 01 00 00 00              	movl	$0x1, %eax
100004f90: 5b                          	popq	%rbx
100004f91: 41 5e                       	popq	%r14
100004f93: 5d                          	popq	%rbp
100004f94: c3                          	retq
100004f95: 48 89 df                    	movq	%rbx, %rdi
100004f98: e8 21 37 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100004f9d: 89 c7                       	movl	%eax, %edi
100004f9f: 85 c0                       	testl	%eax, %eax
100004fa1: b8 02 00 00 00              	movl	$0x2, %eax
100004fa6: 74 e8                       	je	0x100004f90 <_mb_win_srw_release_pin+0x80>
100004fa8: e8 43 34 00 00              	callq	0x1000083f0 <_mb_win_srw_release_pin.cold.2>
100004fad: e8 5e 36 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100004fb2: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)

0000000100004fc0 <_mb_win_srw_domain>:
100004fc0: 48 85 ff                    	testq	%rdi, %rdi
100004fc3: 74 12                       	je	0x100004fd7 <_mb_win_srw_domain+0x17>
100004fc5: 48 8b 07                    	movq	(%rdi), %rax
100004fc8: 48 85 c0                    	testq	%rax, %rax
100004fcb: 74 0a                       	je	0x100004fd7 <_mb_win_srw_domain+0x17>
100004fcd: 55                          	pushq	%rbp
100004fce: 48 89 e5                    	movq	%rsp, %rbp
100004fd1: 48 8b 40 40                 	movq	0x40(%rax), %rax
100004fd5: 5d                          	popq	%rbp
100004fd6: c3                          	retq
100004fd7: 31 c0                       	xorl	%eax, %eax
100004fd9: c3                          	retq
100004fda: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100004fe0 <_mb_win_srw_initialize>:
100004fe0: 55                          	pushq	%rbp
100004fe1: 48 89 e5                    	movq	%rsp, %rbp
100004fe4: 5d                          	popq	%rbp
100004fe5: e9 06 00 00 00              	jmp	0x100004ff0 <_initialize>
100004fea: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100004ff0 <_initialize>:
100004ff0: 55                          	pushq	%rbp
100004ff1: 48 89 e5                    	movq	%rsp, %rbp
100004ff4: 41 56                       	pushq	%r14
100004ff6: 53                          	pushq	%rbx
100004ff7: 48 85 ff                    	testq	%rdi, %rdi
100004ffa: 74 42                       	je	0x10000503e <_initialize+0x4e>
100004ffc: 4c 8b 37                    	movq	(%rdi), %r14
100004fff: 4d 85 f6                    	testq	%r14, %r14
100005002: 74 3a                       	je	0x10000503e <_initialize+0x4e>
100005004: 48 89 f3                    	movq	%rsi, %rbx
100005007: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
10000500b: e8 10 d1 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100005010: 4c 89 f7                    	movq	%r14, %rdi
100005013: e8 a0 36 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100005018: 85 c0                       	testl	%eax, %eax
10000501a: 75 22                       	jne	0x10000503e <_initialize+0x4e>
10000501c: 48 85 db                    	testq	%rbx, %rbx
10000501f: 74 1d                       	je	0x10000503e <_initialize+0x4e>
100005021: 89 d8                       	movl	%ebx, %eax
100005023: 83 e0 07                    	andl	$0x7, %eax
100005026: 75 16                       	jne	0x10000503e <_initialize+0x4e>
100005028: 49 8d 46 60                 	leaq	0x60(%r14), %rax
10000502c: 0f 1f 40 00                 	nopl	(%rax)
100005030: 48 8b 00                    	movq	(%rax), %rax
100005033: 48 85 c0                    	testq	%rax, %rax
100005036: 74 0b                       	je	0x100005043 <_initialize+0x53>
100005038: 48 39 58 08                 	cmpq	%rbx, 0x8(%rax)
10000503c: 75 f2                       	jne	0x100005030 <_initialize+0x40>
10000503e: e8 cd 35 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100005043: 48 c7 03 00 00 00 00        	movq	$0x0, (%rbx)
10000504a: 4c 89 f7                    	movq	%r14, %rdi
10000504d: e8 6c 36 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100005052: 85 c0                       	testl	%eax, %eax
100005054: 75 05                       	jne	0x10000505b <_initialize+0x6b>
100005056: 5b                          	popq	%rbx
100005057: 41 5e                       	popq	%r14
100005059: 5d                          	popq	%rbp
10000505a: c3                          	retq
10000505b: 89 c7                       	movl	%eax, %edi
10000505d: e8 be 33 00 00              	callq	0x100008420 <_initialize.cold.1>
100005062: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)

0000000100005070 <_mb_win_srw_condition_initialize>:
100005070: 55                          	pushq	%rbp
100005071: 48 89 e5                    	movq	%rsp, %rbp
100005074: 5d                          	popq	%rbp
100005075: e9 76 ff ff ff              	jmp	0x100004ff0 <_initialize>
10000507a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100005080 <_mb_win_srw_acquire>:
100005080: 55                          	pushq	%rbp
100005081: 48 89 e5                    	movq	%rsp, %rbp
100005084: 41 57                       	pushq	%r15
100005086: 41 56                       	pushq	%r14
100005088: 41 54                       	pushq	%r12
10000508a: 53                          	pushq	%rbx
10000508b: 89 d3                       	movl	%edx, %ebx
10000508d: 49 89 f6                    	movq	%rsi, %r14
100005090: 49 89 ff                    	movq	%rdi, %r15
100005093: 4c 8b 25 7e 4f 00 00        	movq	0x4f7e(%rip), %r12      ## 0x10000a018 <_sysconf+0x10000a018>
10000509a: 49 8b 0c 24                 	movq	(%r12), %rcx
10000509e: 48 8d 3d ae 3d 00 00        	leaq	0x3dae(%rip), %rdi      ## 0x100008e53 <_sysconf+0x100008e53>
1000050a5: be 1a 00 00 00              	movl	$0x1a, %esi
1000050aa: ba 01 00 00 00              	movl	$0x1, %edx
1000050af: e8 8c 35 00 00              	callq	0x100008640 <_sysconf+0x100008640>
1000050b4: 49 8b 3c 24                 	movq	(%r12), %rdi
1000050b8: e8 71 35 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000050bd: 4d 85 ff                    	testq	%r15, %r15
1000050c0: 74 61                       	je	0x100005123 <_mb_win_srw_acquire+0xa3>
1000050c2: 4d 8b 3f                    	movq	(%r15), %r15
1000050c5: 4d 85 ff                    	testq	%r15, %r15
1000050c8: 74 59                       	je	0x100005123 <_mb_win_srw_acquire+0xa3>
1000050ca: 49 8b 7f 40                 	movq	0x40(%r15), %rdi
1000050ce: e8 4d d0 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000050d3: 4c 89 ff                    	movq	%r15, %rdi
1000050d6: e8 dd 35 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000050db: 85 c0                       	testl	%eax, %eax
1000050dd: 75 44                       	jne	0x100005123 <_mb_win_srw_acquire+0xa3>
1000050df: 83 fb 02                    	cmpl	$0x2, %ebx
1000050e2: 73 3f                       	jae	0x100005123 <_mb_win_srw_acquire+0xa3>
1000050e4: 4c 89 ff                    	movq	%r15, %rdi
1000050e7: 4c 89 f6                    	movq	%r14, %rsi
1000050ea: ba 01 00 00 00              	movl	$0x1, %edx
1000050ef: b9 01 00 00 00              	movl	$0x1, %ecx
1000050f4: 45 31 c0                    	xorl	%r8d, %r8d
1000050f7: e8 34 00 00 00              	callq	0x100005130 <_lookup>
1000050fc: 48 85 c0                    	testq	%rax, %rax
1000050ff: 74 22                       	je	0x100005123 <_mb_win_srw_acquire+0xa3>
100005101: 4c 89 ff                    	movq	%r15, %rdi
100005104: 48 89 c6                    	movq	%rax, %rsi
100005107: 89 da                       	movl	%ebx, %edx
100005109: e8 c2 01 00 00              	callq	0x1000052d0 <_take>
10000510e: 4c 89 ff                    	movq	%r15, %rdi
100005111: e8 a8 35 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100005116: 85 c0                       	testl	%eax, %eax
100005118: 75 0e                       	jne	0x100005128 <_mb_win_srw_acquire+0xa8>
10000511a: 5b                          	popq	%rbx
10000511b: 41 5c                       	popq	%r12
10000511d: 41 5e                       	popq	%r14
10000511f: 41 5f                       	popq	%r15
100005121: 5d                          	popq	%rbp
100005122: c3                          	retq
100005123: e8 e8 34 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100005128: 89 c7                       	movl	%eax, %edi
10000512a: e8 21 33 00 00              	callq	0x100008450 <_mb_win_srw_acquire.cold.1>
10000512f: 90                          	nop

0000000100005130 <_lookup>:
100005130: 55                          	pushq	%rbp
100005131: 48 89 e5                    	movq	%rsp, %rbp
100005134: 41 57                       	pushq	%r15
100005136: 41 56                       	pushq	%r14
100005138: 41 55                       	pushq	%r13
10000513a: 41 54                       	pushq	%r12
10000513c: 53                          	pushq	%rbx
10000513d: 48 83 ec 28                 	subq	$0x28, %rsp
100005141: 4c 89 45 b8                 	movq	%r8, -0x48(%rbp)
100005145: 48 8b 05 c4 4e 00 00        	movq	0x4ec4(%rip), %rax      ## 0x10000a010 <_sysconf+0x10000a010>
10000514c: 48 8b 00                    	movq	(%rax), %rax
10000514f: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
100005153: 48 85 f6                    	testq	%rsi, %rsi
100005156: 0f 84 60 01 00 00           	je	0x1000052bc <_lookup+0x18c>
10000515c: 48 89 f3                    	movq	%rsi, %rbx
10000515f: 89 d8                       	movl	%ebx, %eax
100005161: 83 e0 07                    	andl	$0x7, %eax
100005164: 0f 85 52 01 00 00           	jne	0x1000052bc <_lookup+0x18c>
10000516a: 41 89 cd                    	movl	%ecx, %r13d
10000516d: 41 89 d4                    	movl	%edx, %r12d
100005170: 49 89 fe                    	movq	%rdi, %r14
100005173: 4c 8b 3d 9e 4e 00 00        	movq	0x4e9e(%rip), %r15      ## 0x10000a018 <_sysconf+0x10000a018>
10000517a: 49 8b 0f                    	movq	(%r15), %rcx
10000517d: 48 8d 3d a5 3e 00 00        	leaq	0x3ea5(%rip), %rdi      ## 0x100009029 <_sysconf+0x100009029>
100005184: be 1a 00 00 00              	movl	$0x1a, %esi
100005189: ba 01 00 00 00              	movl	$0x1, %edx
10000518e: e8 ad 34 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005193: 49 8b 3f                    	movq	(%r15), %rdi
100005196: e8 93 34 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
10000519b: 49 8d 46 60                 	leaq	0x60(%r14), %rax
10000519f: 90                          	nop
1000051a0: 48 8b 00                    	movq	(%rax), %rax
1000051a3: 48 85 c0                    	testq	%rax, %rax
1000051a6: 74 1e                       	je	0x1000051c6 <_lookup+0x96>
1000051a8: 48 39 58 08                 	cmpq	%rbx, 0x8(%rax)
1000051ac: 75 f2                       	jne	0x1000051a0 <_lookup+0x70>
1000051ae: 44 39 60 18                 	cmpl	%r12d, 0x18(%rax)
1000051b2: 0f 85 04 01 00 00           	jne	0x1000052bc <_lookup+0x18c>
1000051b8: 48 8b 0b                    	movq	(%rbx), %rcx
1000051bb: 48 3b 48 10                 	cmpq	0x10(%rax), %rcx
1000051bf: 74 71                       	je	0x100005232 <_lookup+0x102>
1000051c1: e9 f6 00 00 00              	jmp	0x1000052bc <_lookup+0x18c>
1000051c6: 49 8b 0f                    	movq	(%r15), %rcx
1000051c9: 48 8d 3d 74 3e 00 00        	leaq	0x3e74(%rip), %rdi      ## 0x100009044 <_sysconf+0x100009044>
1000051d0: be 1a 00 00 00              	movl	$0x1a, %esi
1000051d5: ba 01 00 00 00              	movl	$0x1, %edx
1000051da: e8 61 34 00 00              	callq	0x100008640 <_sysconf+0x100008640>
1000051df: 49 8b 3f                    	movq	(%r15), %rdi
1000051e2: e8 47 34 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000051e7: 48 83 3b 00                 	cmpq	$0x0, (%rbx)
1000051eb: 0f 85 cb 00 00 00           	jne	0x1000052bc <_lookup+0x18c>
1000051f1: 45 85 ed                    	testl	%r13d, %r13d
1000051f4: 74 3a                       	je	0x100005230 <_lookup+0x100>
1000051f6: 49 8b 46 68                 	movq	0x68(%r14), %rax
1000051fa: 49 3b 46 70                 	cmpq	0x70(%r14), %rax
1000051fe: 74 30                       	je	0x100005230 <_lookup+0x100>
100005200: 49 83 7e 78 ff              	cmpq	$-0x1, 0x78(%r14)
100005205: 74 29                       	je	0x100005230 <_lookup+0x100>
100005207: bf 01 00 00 00              	movl	$0x1, %edi
10000520c: be 80 00 00 00              	movl	$0x80, %esi
100005211: e8 06 34 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
100005216: 48 85 c0                    	testq	%rax, %rax
100005219: 74 15                       	je	0x100005230 <_lookup+0x100>
10000521b: 49 89 c7                    	movq	%rax, %r15
10000521e: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
100005222: e8 5b 34 00 00              	callq	0x100008682 <_sysconf+0x100008682>
100005227: 85 c0                       	testl	%eax, %eax
100005229: 74 26                       	je	0x100005251 <_lookup+0x121>
10000522b: 41 89 c5                    	movl	%eax, %r13d
10000522e: eb 3e                       	jmp	0x10000526e <_lookup+0x13e>
100005230: 31 c0                       	xorl	%eax, %eax
100005232: 48 8b 0d d7 4d 00 00        	movq	0x4dd7(%rip), %rcx      ## 0x10000a010 <_sysconf+0x10000a010>
100005239: 48 8b 09                    	movq	(%rcx), %rcx
10000523c: 48 3b 4d d0                 	cmpq	-0x30(%rbp), %rcx
100005240: 75 7f                       	jne	0x1000052c1 <_lookup+0x191>
100005242: 48 83 c4 28                 	addq	$0x28, %rsp
100005246: 5b                          	popq	%rbx
100005247: 41 5c                       	popq	%r12
100005249: 41 5d                       	popq	%r13
10000524b: 41 5e                       	popq	%r14
10000524d: 41 5f                       	popq	%r15
10000524f: 5d                          	popq	%rbp
100005250: c3                          	retq
100005251: 49 8d 7f 40                 	leaq	0x40(%r15), %rdi
100005255: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
100005259: e8 06 34 00 00              	callq	0x100008664 <_sysconf+0x100008664>
10000525e: 41 89 c5                    	movl	%eax, %r13d
100005261: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
100005265: e8 12 34 00 00              	callq	0x10000867c <_sysconf+0x10000867c>
10000526a: 85 c0                       	testl	%eax, %eax
10000526c: 75 4e                       	jne	0x1000052bc <_lookup+0x18c>
10000526e: 4c 89 f8                    	movq	%r15, %rax
100005271: 45 85 ed                    	testl	%r13d, %r13d
100005274: 74 18                       	je	0x10000528e <_lookup+0x15e>
100005276: 48 8b 4d b8                 	movq	-0x48(%rbp), %rcx
10000527a: 48 85 c9                    	testq	%rcx, %rcx
10000527d: 74 03                       	je	0x100005282 <_lookup+0x152>
10000527f: 44 89 29                    	movl	%r13d, (%rcx)
100005282: 48 89 c7                    	movq	%rax, %rdi
100005285: e8 b0 33 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
10000528a: 31 c0                       	xorl	%eax, %eax
10000528c: eb a4                       	jmp	0x100005232 <_lookup+0x102>
10000528e: 48 89 58 08                 	movq	%rbx, 0x8(%rax)
100005292: 44 89 60 18                 	movl	%r12d, 0x18(%rax)
100005296: 49 8b 4e 78                 	movq	0x78(%r14), %rcx
10000529a: 48 ff c1                    	incq	%rcx
10000529d: 49 89 4e 78                 	movq	%rcx, 0x78(%r14)
1000052a1: 48 89 48 10                 	movq	%rcx, 0x10(%rax)
1000052a5: 49 8b 56 60                 	movq	0x60(%r14), %rdx
1000052a9: 48 89 10                    	movq	%rdx, (%rax)
1000052ac: 49 89 46 60                 	movq	%rax, 0x60(%r14)
1000052b0: 49 ff 46 68                 	incq	0x68(%r14)
1000052b4: 48 89 0b                    	movq	%rcx, (%rbx)
1000052b7: e9 76 ff ff ff              	jmp	0x100005232 <_lookup+0x102>
1000052bc: e8 4f 33 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000052c1: e8 3e 33 00 00              	callq	0x100008604 <_sysconf+0x100008604>
1000052c6: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)

00000001000052d0 <_take>:
1000052d0: 55                          	pushq	%rbp
1000052d1: 48 89 e5                    	movq	%rsp, %rbp
1000052d4: 41 57                       	pushq	%r15
1000052d6: 41 56                       	pushq	%r14
1000052d8: 41 54                       	pushq	%r12
1000052da: 53                          	pushq	%rbx
1000052db: 41 89 d6                    	movl	%edx, %r14d
1000052de: 48 89 f3                    	movq	%rsi, %rbx
1000052e1: 49 89 ff                    	movq	%rdi, %r15
1000052e4: 4c 8b 25 2d 4d 00 00        	movq	0x4d2d(%rip), %r12      ## 0x10000a018 <_sysconf+0x10000a018>
1000052eb: 49 8b 0c 24                 	movq	(%r12), %rcx
1000052ef: 48 8d 3d 69 3d 00 00        	leaq	0x3d69(%rip), %rdi      ## 0x10000905f <_sysconf+0x10000905f>
1000052f6: be 1a 00 00 00              	movl	$0x1a, %esi
1000052fb: ba 01 00 00 00              	movl	$0x1, %edx
100005300: e8 3b 33 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005305: 49 8b 3c 24                 	movq	(%r12), %rdi
100005309: e8 20 33 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
10000530e: 48 8b 43 28                 	movq	0x28(%rbx), %rax
100005312: 48 83 f8 ff                 	cmpq	$-0x1, %rax
100005316: 0f 84 7c 00 00 00           	je	0x100005398 <_take+0xc8>
10000531c: 48 83 7b 20 ff              	cmpq	$-0x1, 0x20(%rbx)
100005321: 74 75                       	je	0x100005398 <_take+0xc8>
100005323: 48 8b 4b 30                 	movq	0x30(%rbx), %rcx
100005327: 48 83 f9 ff                 	cmpq	$-0x1, %rcx
10000532b: 74 6b                       	je	0x100005398 <_take+0xc8>
10000532d: 48 ff c0                    	incq	%rax
100005330: 48 89 43 28                 	movq	%rax, 0x28(%rbx)
100005334: 45 85 f6                    	testl	%r14d, %r14d
100005337: 75 07                       	jne	0x100005340 <_take+0x70>
100005339: 48 ff c1                    	incq	%rcx
10000533c: 48 89 4b 30                 	movq	%rcx, 0x30(%rbx)
100005340: 4c 8d 63 40                 	leaq	0x40(%rbx), %r12
100005344: eb 19                       	jmp	0x10000535f <_take+0x8f>
100005346: 48 83 7b 20 00              	cmpq	$0x0, 0x20(%rbx)
10000534b: 74 2a                       	je	0x100005377 <_take+0xa7>
10000534d: 0f 1f 00                    	nopl	(%rax)
100005350: 4c 89 e7                    	movq	%r12, %rdi
100005353: 4c 89 fe                    	movq	%r15, %rsi
100005356: e8 1b 33 00 00              	callq	0x100008676 <_sysconf+0x100008676>
10000535b: 85 c0                       	testl	%eax, %eax
10000535d: 75 39                       	jne	0x100005398 <_take+0xc8>
10000535f: 83 7b 1c 00                 	cmpl	$0x0, 0x1c(%rbx)
100005363: 75 eb                       	jne	0x100005350 <_take+0x80>
100005365: 45 85 f6                    	testl	%r14d, %r14d
100005368: 74 dc                       	je	0x100005346 <_take+0x76>
10000536a: 48 83 7b 30 00              	cmpq	$0x0, 0x30(%rbx)
10000536f: 75 df                       	jne	0x100005350 <_take+0x80>
100005371: 48 ff 43 20                 	incq	0x20(%rbx)
100005375: eb 14                       	jmp	0x10000538b <_take+0xbb>
100005377: 48 ff 4b 30                 	decq	0x30(%rbx)
10000537b: c7 43 1c 01 00 00 00        	movl	$0x1, 0x1c(%rbx)
100005382: e8 3d 33 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100005387: 48 89 43 38                 	movq	%rax, 0x38(%rbx)
10000538b: 48 ff 4b 28                 	decq	0x28(%rbx)
10000538f: 5b                          	popq	%rbx
100005390: 41 5c                       	popq	%r12
100005392: 41 5e                       	popq	%r14
100005394: 41 5f                       	popq	%r15
100005396: 5d                          	popq	%rbp
100005397: c3                          	retq
100005398: e8 73 32 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000539d: 0f 1f 00                    	nopl	(%rax)

00000001000053a0 <_mb_win_srw_try_acquire>:
1000053a0: 55                          	pushq	%rbp
1000053a1: 48 89 e5                    	movq	%rsp, %rbp
1000053a4: 41 57                       	pushq	%r15
1000053a6: 41 56                       	pushq	%r14
1000053a8: 41 55                       	pushq	%r13
1000053aa: 41 54                       	pushq	%r12
1000053ac: 53                          	pushq	%rbx
1000053ad: 50                          	pushq	%rax
1000053ae: 89 d3                       	movl	%edx, %ebx
1000053b0: 49 89 f7                    	movq	%rsi, %r15
1000053b3: 49 89 fe                    	movq	%rdi, %r14
1000053b6: 4c 8b 25 5b 4c 00 00        	movq	0x4c5b(%rip), %r12      ## 0x10000a018 <_sysconf+0x10000a018>
1000053bd: 49 8b 0c 24                 	movq	(%r12), %rcx
1000053c1: 48 8d 3d a6 3a 00 00        	leaq	0x3aa6(%rip), %rdi      ## 0x100008e6e <_sysconf+0x100008e6e>
1000053c8: be 1b 00 00 00              	movl	$0x1b, %esi
1000053cd: ba 01 00 00 00              	movl	$0x1, %edx
1000053d2: e8 69 32 00 00              	callq	0x100008640 <_sysconf+0x100008640>
1000053d7: 49 8b 3c 24                 	movq	(%r12), %rdi
1000053db: e8 4e 32 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000053e0: 4d 85 f6                    	testq	%r14, %r14
1000053e3: 0f 84 eb 00 00 00           	je	0x1000054d4 <_mb_win_srw_try_acquire+0x134>
1000053e9: 4d 8b 36                    	movq	(%r14), %r14
1000053ec: 4d 85 f6                    	testq	%r14, %r14
1000053ef: 0f 84 df 00 00 00           	je	0x1000054d4 <_mb_win_srw_try_acquire+0x134>
1000053f5: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000053f9: e8 22 cd ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000053fe: 4c 89 f7                    	movq	%r14, %rdi
100005401: e8 b2 32 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100005406: 85 c0                       	testl	%eax, %eax
100005408: 0f 85 c6 00 00 00           	jne	0x1000054d4 <_mb_win_srw_try_acquire+0x134>
10000540e: 83 fb 02                    	cmpl	$0x2, %ebx
100005411: 0f 83 bd 00 00 00           	jae	0x1000054d4 <_mb_win_srw_try_acquire+0x134>
100005417: 4c 89 f7                    	movq	%r14, %rdi
10000541a: 4c 89 fe                    	movq	%r15, %rsi
10000541d: ba 01 00 00 00              	movl	$0x1, %edx
100005422: b9 01 00 00 00              	movl	$0x1, %ecx
100005427: 45 31 c0                    	xorl	%r8d, %r8d
10000542a: e8 01 fd ff ff              	callq	0x100005130 <_lookup>
10000542f: 48 85 c0                    	testq	%rax, %rax
100005432: 0f 84 9c 00 00 00           	je	0x1000054d4 <_mb_win_srw_try_acquire+0x134>
100005438: 49 89 c7                    	movq	%rax, %r15
10000543b: 83 78 1c 00                 	cmpl	$0x0, 0x1c(%rax)
10000543f: 74 05                       	je	0x100005446 <_mb_win_srw_try_acquire+0xa6>
100005441: 45 31 ed                    	xorl	%r13d, %r13d
100005444: eb 14                       	jmp	0x10000545a <_mb_win_srw_try_acquire+0xba>
100005446: 31 c0                       	xorl	%eax, %eax
100005448: 85 db                       	testl	%ebx, %ebx
10000544a: 0f 95 c0                    	setne	%al
10000544d: c1 e0 04                    	shll	$0x4, %eax
100005450: 49 83 7c 07 20 00           	cmpq	$0x0, 0x20(%r15,%rax)
100005456: 41 0f 94 c5                 	sete	%r13b
10000545a: 49 8b 0c 24                 	movq	(%r12), %rcx
10000545e: 48 8d 3d 25 3a 00 00        	leaq	0x3a25(%rip), %rdi      ## 0x100008e8a <_sysconf+0x100008e8a>
100005465: be 1b 00 00 00              	movl	$0x1b, %esi
10000546a: ba 01 00 00 00              	movl	$0x1, %edx
10000546f: e8 cc 31 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005474: 49 8b 3c 24                 	movq	(%r12), %rdi
100005478: e8 b1 31 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
10000547d: 45 84 ed                    	testb	%r13b, %r13b
100005480: 74 28                       	je	0x1000054aa <_mb_win_srw_try_acquire+0x10a>
100005482: 49 8b 47 20                 	movq	0x20(%r15), %rax
100005486: 48 83 f8 ff                 	cmpq	$-0x1, %rax
10000548a: 74 48                       	je	0x1000054d4 <_mb_win_srw_try_acquire+0x134>
10000548c: 85 db                       	testl	%ebx, %ebx
10000548e: 74 09                       	je	0x100005499 <_mb_win_srw_try_acquire+0xf9>
100005490: 48 ff c0                    	incq	%rax
100005493: 49 89 47 20                 	movq	%rax, 0x20(%r15)
100005497: eb 11                       	jmp	0x1000054aa <_mb_win_srw_try_acquire+0x10a>
100005499: 41 c7 47 1c 01 00 00 00     	movl	$0x1, 0x1c(%r15)
1000054a1: e8 1e 32 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
1000054a6: 49 89 47 38                 	movq	%rax, 0x38(%r15)
1000054aa: 4c 89 f7                    	movq	%r14, %rdi
1000054ad: 4c 89 fe                    	movq	%r15, %rsi
1000054b0: e8 2b 00 00 00              	callq	0x1000054e0 <_retire>
1000054b5: 4c 89 f7                    	movq	%r14, %rdi
1000054b8: e8 01 32 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000054bd: 85 c0                       	testl	%eax, %eax
1000054bf: 75 18                       	jne	0x1000054d9 <_mb_win_srw_try_acquire+0x139>
1000054c1: 41 0f b6 c5                 	movzbl	%r13b, %eax
1000054c5: 48 83 c4 08                 	addq	$0x8, %rsp
1000054c9: 5b                          	popq	%rbx
1000054ca: 41 5c                       	popq	%r12
1000054cc: 41 5d                       	popq	%r13
1000054ce: 41 5e                       	popq	%r14
1000054d0: 41 5f                       	popq	%r15
1000054d2: 5d                          	popq	%rbp
1000054d3: c3                          	retq
1000054d4: e8 37 31 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000054d9: 89 c7                       	movl	%eax, %edi
1000054db: e8 a0 2f 00 00              	callq	0x100008480 <_mb_win_srw_try_acquire.cold.1>

00000001000054e0 <_retire>:
1000054e0: 55                          	pushq	%rbp
1000054e1: 48 89 e5                    	movq	%rsp, %rbp
1000054e4: 41 57                       	pushq	%r15
1000054e6: 41 56                       	pushq	%r14
1000054e8: 53                          	pushq	%rbx
1000054e9: 50                          	pushq	%rax
1000054ea: 48 83 7e 28 00              	cmpq	$0x0, 0x28(%rsi)
1000054ef: 75 1b                       	jne	0x10000550c <_retire+0x2c>
1000054f1: 48 83 7e 20 00              	cmpq	$0x0, 0x20(%rsi)
1000054f6: 75 14                       	jne	0x10000550c <_retire+0x2c>
1000054f8: 83 7e 1c 00                 	cmpl	$0x0, 0x1c(%rsi)
1000054fc: 75 0e                       	jne	0x10000550c <_retire+0x2c>
1000054fe: 48 83 7e 70 00              	cmpq	$0x0, 0x70(%rsi)
100005503: 75 07                       	jne	0x10000550c <_retire+0x2c>
100005505: 48 83 7e 30 00              	cmpq	$0x0, 0x30(%rsi)
10000550a: 74 0b                       	je	0x100005517 <_retire+0x37>
10000550c: 48 83 c4 08                 	addq	$0x8, %rsp
100005510: 5b                          	popq	%rbx
100005511: 41 5e                       	popq	%r14
100005513: 41 5f                       	popq	%r15
100005515: 5d                          	popq	%rbp
100005516: c3                          	retq
100005517: 4c 8b 3d fa 4a 00 00        	movq	0x4afa(%rip), %r15      ## 0x10000a018 <_sysconf+0x10000a018>
10000551e: 49 8b 0f                    	movq	(%r15), %rcx
100005521: 48 8d 05 52 3b 00 00        	leaq	0x3b52(%rip), %rax      ## 0x10000907a <_sysconf+0x10000907a>
100005528: 49 89 f6                    	movq	%rsi, %r14
10000552b: be 1a 00 00 00              	movl	$0x1a, %esi
100005530: ba 01 00 00 00              	movl	$0x1, %edx
100005535: 48 89 fb                    	movq	%rdi, %rbx
100005538: 48 89 c7                    	movq	%rax, %rdi
10000553b: e8 00 31 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005540: 49 8b 3f                    	movq	(%r15), %rdi
100005543: e8 e6 30 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005548: 4c 89 f0                    	movq	%r14, %rax
10000554b: 48 8d 4b 60                 	leaq	0x60(%rbx), %rcx
10000554f: 90                          	nop
100005550: 49 89 cf                    	movq	%rcx, %r15
100005553: 48 8b 09                    	movq	(%rcx), %rcx
100005556: 48 85 c9                    	testq	%rcx, %rcx
100005559: 74 05                       	je	0x100005560 <_retire+0x80>
10000555b: 48 39 c1                    	cmpq	%rax, %rcx
10000555e: 75 f0                       	jne	0x100005550 <_retire+0x70>
100005560: 48 85 c9                    	testq	%rcx, %rcx
100005563: 74 34                       	je	0x100005599 <_retire+0xb9>
100005565: 48 8d 78 40                 	leaq	0x40(%rax), %rdi
100005569: e8 f0 30 00 00              	callq	0x10000865e <_sysconf+0x10000865e>
10000556e: 85 c0                       	testl	%eax, %eax
100005570: 75 27                       	jne	0x100005599 <_retire+0xb9>
100005572: 4c 89 f7                    	movq	%r14, %rdi
100005575: 49 8b 46 08                 	movq	0x8(%r14), %rax
100005579: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
100005580: 49 8b 06                    	movq	(%r14), %rax
100005583: 49 89 07                    	movq	%rax, (%r15)
100005586: 48 ff 4b 68                 	decq	0x68(%rbx)
10000558a: 48 83 c4 08                 	addq	$0x8, %rsp
10000558e: 5b                          	popq	%rbx
10000558f: 41 5e                       	popq	%r14
100005591: 41 5f                       	popq	%r15
100005593: 5d                          	popq	%rbp
100005594: e9 a1 30 00 00              	jmp	0x10000863a <_sysconf+0x10000863a>
100005599: e8 72 30 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000559e: 66 90                       	nop

00000001000055a0 <_mb_win_srw_release>:
1000055a0: 55                          	pushq	%rbp
1000055a1: 48 89 e5                    	movq	%rsp, %rbp
1000055a4: 41 57                       	pushq	%r15
1000055a6: 41 56                       	pushq	%r14
1000055a8: 41 54                       	pushq	%r12
1000055aa: 53                          	pushq	%rbx
1000055ab: 89 d3                       	movl	%edx, %ebx
1000055ad: 49 89 f7                    	movq	%rsi, %r15
1000055b0: 49 89 fe                    	movq	%rdi, %r14
1000055b3: 4c 8b 25 5e 4a 00 00        	movq	0x4a5e(%rip), %r12      ## 0x10000a018 <_sysconf+0x10000a018>
1000055ba: 49 8b 0c 24                 	movq	(%r12), %rcx
1000055be: 48 8d 3d e1 38 00 00        	leaq	0x38e1(%rip), %rdi      ## 0x100008ea6 <_sysconf+0x100008ea6>
1000055c5: be 1b 00 00 00              	movl	$0x1b, %esi
1000055ca: ba 01 00 00 00              	movl	$0x1, %edx
1000055cf: e8 6c 30 00 00              	callq	0x100008640 <_sysconf+0x100008640>
1000055d4: 49 8b 3c 24                 	movq	(%r12), %rdi
1000055d8: e8 51 30 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000055dd: 4d 85 f6                    	testq	%r14, %r14
1000055e0: 74 69                       	je	0x10000564b <_mb_win_srw_release+0xab>
1000055e2: 4d 8b 36                    	movq	(%r14), %r14
1000055e5: 4d 85 f6                    	testq	%r14, %r14
1000055e8: 74 61                       	je	0x10000564b <_mb_win_srw_release+0xab>
1000055ea: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000055ee: e8 2d cb ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000055f3: 4c 89 f7                    	movq	%r14, %rdi
1000055f6: e8 bd 30 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000055fb: 85 c0                       	testl	%eax, %eax
1000055fd: 75 4c                       	jne	0x10000564b <_mb_win_srw_release+0xab>
1000055ff: 83 fb 02                    	cmpl	$0x2, %ebx
100005602: 73 47                       	jae	0x10000564b <_mb_win_srw_release+0xab>
100005604: 4c 89 f7                    	movq	%r14, %rdi
100005607: 4c 89 fe                    	movq	%r15, %rsi
10000560a: ba 01 00 00 00              	movl	$0x1, %edx
10000560f: 31 c9                       	xorl	%ecx, %ecx
100005611: 45 31 c0                    	xorl	%r8d, %r8d
100005614: e8 17 fb ff ff              	callq	0x100005130 <_lookup>
100005619: 48 85 c0                    	testq	%rax, %rax
10000561c: 74 2d                       	je	0x10000564b <_mb_win_srw_release+0xab>
10000561e: 49 89 c7                    	movq	%rax, %r15
100005621: 48 89 c7                    	movq	%rax, %rdi
100005624: 89 de                       	movl	%ebx, %esi
100005626: e8 35 00 00 00              	callq	0x100005660 <_drop>
10000562b: 4c 89 f7                    	movq	%r14, %rdi
10000562e: 4c 89 fe                    	movq	%r15, %rsi
100005631: e8 aa fe ff ff              	callq	0x1000054e0 <_retire>
100005636: 4c 89 f7                    	movq	%r14, %rdi
100005639: e8 80 30 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000563e: 85 c0                       	testl	%eax, %eax
100005640: 75 0e                       	jne	0x100005650 <_mb_win_srw_release+0xb0>
100005642: 5b                          	popq	%rbx
100005643: 41 5c                       	popq	%r12
100005645: 41 5e                       	popq	%r14
100005647: 41 5f                       	popq	%r15
100005649: 5d                          	popq	%rbp
10000564a: c3                          	retq
10000564b: e8 c0 2f 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100005650: 89 c7                       	movl	%eax, %edi
100005652: e8 59 2e 00 00              	callq	0x1000084b0 <_mb_win_srw_release.cold.1>
100005657: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)

0000000100005660 <_drop>:
100005660: 55                          	pushq	%rbp
100005661: 48 89 e5                    	movq	%rsp, %rbp
100005664: 41 57                       	pushq	%r15
100005666: 41 56                       	pushq	%r14
100005668: 53                          	pushq	%rbx
100005669: 50                          	pushq	%rax
10000566a: 41 89 f6                    	movl	%esi, %r14d
10000566d: 48 89 fb                    	movq	%rdi, %rbx
100005670: 4c 8b 3d a1 49 00 00        	movq	0x49a1(%rip), %r15      ## 0x10000a018 <_sysconf+0x10000a018>
100005677: 49 8b 0f                    	movq	(%r15), %rcx
10000567a: 48 8d 3d 14 3a 00 00        	leaq	0x3a14(%rip), %rdi      ## 0x100009095 <_sysconf+0x100009095>
100005681: be 1a 00 00 00              	movl	$0x1a, %esi
100005686: ba 01 00 00 00              	movl	$0x1, %edx
10000568b: e8 b0 2f 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005690: 49 8b 3f                    	movq	(%r15), %rdi
100005693: e8 96 2f 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005698: 45 85 f6                    	testl	%r14d, %r14d
10000569b: 74 18                       	je	0x1000056b5 <_drop+0x55>
10000569d: 48 8b 43 20                 	movq	0x20(%rbx), %rax
1000056a1: 48 85 c0                    	testq	%rax, %rax
1000056a4: 74 70                       	je	0x100005716 <_drop+0xb6>
1000056a6: 83 7b 1c 00                 	cmpl	$0x0, 0x1c(%rbx)
1000056aa: 75 6a                       	jne	0x100005716 <_drop+0xb6>
1000056ac: 48 ff c8                    	decq	%rax
1000056af: 48 89 43 20                 	movq	%rax, 0x20(%rbx)
1000056b3: eb 46                       	jmp	0x1000056fb <_drop+0x9b>
1000056b5: 49 8b 0f                    	movq	(%r15), %rcx
1000056b8: 48 8d 3d f1 39 00 00        	leaq	0x39f1(%rip), %rdi      ## 0x1000090b0 <_sysconf+0x1000090b0>
1000056bf: be 1a 00 00 00              	movl	$0x1a, %esi
1000056c4: ba 01 00 00 00              	movl	$0x1, %edx
1000056c9: e8 72 2f 00 00              	callq	0x100008640 <_sysconf+0x100008640>
1000056ce: 49 8b 3f                    	movq	(%r15), %rdi
1000056d1: e8 58 2f 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000056d6: 83 7b 1c 00                 	cmpl	$0x0, 0x1c(%rbx)
1000056da: 74 3a                       	je	0x100005716 <_drop+0xb6>
1000056dc: 4c 8b 73 38                 	movq	0x38(%rbx), %r14
1000056e0: e8 df 2f 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
1000056e5: 4c 89 f7                    	movq	%r14, %rdi
1000056e8: 48 89 c6                    	movq	%rax, %rsi
1000056eb: e8 9e 2f 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
1000056f0: 85 c0                       	testl	%eax, %eax
1000056f2: 74 22                       	je	0x100005716 <_drop+0xb6>
1000056f4: c7 43 1c 00 00 00 00        	movl	$0x0, 0x1c(%rbx)
1000056fb: 48 83 c3 40                 	addq	$0x40, %rbx
1000056ff: 48 89 df                    	movq	%rbx, %rdi
100005702: e8 51 2f 00 00              	callq	0x100008658 <_sysconf+0x100008658>
100005707: 85 c0                       	testl	%eax, %eax
100005709: 75 0b                       	jne	0x100005716 <_drop+0xb6>
10000570b: 48 83 c4 08                 	addq	$0x8, %rsp
10000570f: 5b                          	popq	%rbx
100005710: 41 5e                       	popq	%r14
100005712: 41 5f                       	popq	%r15
100005714: 5d                          	popq	%rbp
100005715: c3                          	retq
100005716: e8 f5 2e 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000571b: 0f 1f 44 00 00              	nopl	(%rax,%rax)

0000000100005720 <_mb_win_srw_sleep>:
100005720: 55                          	pushq	%rbp
100005721: 48 89 e5                    	movq	%rsp, %rbp
100005724: 41 57                       	pushq	%r15
100005726: 41 56                       	pushq	%r14
100005728: 41 55                       	pushq	%r13
10000572a: 41 54                       	pushq	%r12
10000572c: 53                          	pushq	%rbx
10000572d: 48 81 ec b8 00 00 00        	subq	$0xb8, %rsp
100005734: 45 89 c6                    	movl	%r8d, %r14d
100005737: 41 89 cc                    	movl	%ecx, %r12d
10000573a: 49 89 d5                    	movq	%rdx, %r13
10000573d: 48 89 f3                    	movq	%rsi, %rbx
100005740: 49 89 ff                    	movq	%rdi, %r15
100005743: 48 8b 05 c6 48 00 00        	movq	0x48c6(%rip), %rax      ## 0x10000a010 <_sysconf+0x10000a010>
10000574a: 48 8b 00                    	movq	(%rax), %rax
10000574d: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
100005751: 48 8b 05 c0 48 00 00        	movq	0x48c0(%rip), %rax      ## 0x10000a018 <_sysconf+0x10000a018>
100005758: 48 85 d2                    	testq	%rdx, %rdx
10000575b: 74 06                       	je	0x100005763 <_mb_win_srw_sleep+0x43>
10000575d: 4d 8b 45 00                 	movq	(%r13), %r8
100005761: eb 03                       	jmp	0x100005766 <_mb_win_srw_sleep+0x46>
100005763: 45 31 c0                    	xorl	%r8d, %r8d
100005766: 48 8b 38                    	movq	(%rax), %rdi
100005769: 48 85 db                    	testq	%rbx, %rbx
10000576c: 74 05                       	je	0x100005773 <_mb_win_srw_sleep+0x53>
10000576e: 4c 8b 0b                    	movq	(%rbx), %r9
100005771: eb 03                       	jmp	0x100005776 <_mb_win_srw_sleep+0x56>
100005773: 45 31 c9                    	xorl	%r9d, %r9d
100005776: 48 8d 35 45 37 00 00        	leaq	0x3745(%rip), %rsi      ## 0x100008ec2 <_sysconf+0x100008ec2>
10000577d: 44 89 e2                    	movl	%r12d, %edx
100005780: 44 89 f1                    	movl	%r14d, %ecx
100005783: 31 c0                       	xorl	%eax, %eax
100005785: e8 aa 2e 00 00              	callq	0x100008634 <_sysconf+0x100008634>
10000578a: 48 8b 05 87 48 00 00        	movq	0x4887(%rip), %rax      ## 0x10000a018 <_sysconf+0x10000a018>
100005791: 48 8b 38                    	movq	(%rax), %rdi
100005794: e8 95 2e 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005799: 4d 85 ff                    	testq	%r15, %r15
10000579c: 0f 84 9f 06 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
1000057a2: 4d 8b 3f                    	movq	(%r15), %r15
1000057a5: 4d 85 ff                    	testq	%r15, %r15
1000057a8: 0f 84 93 06 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
1000057ae: 49 8b 7f 40                 	movq	0x40(%r15), %rdi
1000057b2: e8 69 c9 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000057b7: 4c 89 ff                    	movq	%r15, %rdi
1000057ba: e8 f9 2e 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000057bf: 85 c0                       	testl	%eax, %eax
1000057c1: 0f 85 7a 06 00 00           	jne	0x100005e41 <_mb_win_srw_sleep+0x721>
1000057c7: 49 8b 7f 40                 	movq	0x40(%r15), %rdi
1000057cb: e8 50 c9 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000057d0: 41 83 fe 02                 	cmpl	$0x2, %r14d
1000057d4: 72 20                       	jb	0x1000057f6 <_mb_win_srw_sleep+0xd6>
1000057d6: 48 89 c3                    	movq	%rax, %rbx
1000057d9: 4c 89 ff                    	movq	%r15, %rdi
1000057dc: e8 dd 2e 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000057e1: 85 c0                       	testl	%eax, %eax
1000057e3: 0f 85 62 06 00 00           	jne	0x100005e4b <_mb_win_srw_sleep+0x72b>
1000057e9: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
1000057ef: 31 c0                       	xorl	%eax, %eax
1000057f1: e9 de 04 00 00              	jmp	0x100005cd4 <_mb_win_srw_sleep+0x5b4>
1000057f6: 44 89 b5 78 ff ff ff        	movl	%r14d, -0x88(%rbp)
1000057fd: 48 89 85 48 ff ff ff        	movq	%rax, -0xb8(%rbp)
100005804: 8b 00                       	movl	(%rax), %eax
100005806: 89 85 44 ff ff ff           	movl	%eax, -0xbc(%rbp)
10000580c: 4c 8b 35 05 48 00 00        	movq	0x4805(%rip), %r14      ## 0x10000a018 <_sysconf+0x10000a018>
100005813: 49 8b 0e                    	movq	(%r14), %rcx
100005816: 48 8d 3d e7 36 00 00        	leaq	0x36e7(%rip), %rdi      ## 0x100008f04 <_sysconf+0x100008f04>
10000581d: be 1b 00 00 00              	movl	$0x1b, %esi
100005822: ba 01 00 00 00              	movl	$0x1, %edx
100005827: e8 14 2e 00 00              	callq	0x100008640 <_sysconf+0x100008640>
10000582c: 49 8b 3e                    	movq	(%r14), %rdi
10000582f: e8 fa 2d 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005834: 4c 89 ff                    	movq	%r15, %rdi
100005837: 4c 89 ee                    	movq	%r13, %rsi
10000583a: ba 01 00 00 00              	movl	$0x1, %edx
10000583f: 31 c9                       	xorl	%ecx, %ecx
100005841: 45 31 c0                    	xorl	%r8d, %r8d
100005844: e8 e7 f8 ff ff              	callq	0x100005130 <_lookup>
100005849: 48 85 c0                    	testq	%rax, %rax
10000584c: 0f 84 ef 05 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
100005852: 49 89 c5                    	movq	%rax, %r13
100005855: 49 8b 0e                    	movq	(%r14), %rcx
100005858: 48 8d 3d c1 36 00 00        	leaq	0x36c1(%rip), %rdi      ## 0x100008f20 <_sysconf+0x100008f20>
10000585f: be 1b 00 00 00              	movl	$0x1b, %esi
100005864: ba 01 00 00 00              	movl	$0x1, %edx
100005869: e8 d2 2d 00 00              	callq	0x100008640 <_sysconf+0x100008640>
10000586e: 49 8b 3e                    	movq	(%r14), %rdi
100005871: e8 b8 2d 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005876: 83 bd 78 ff ff ff 00        	cmpl	$0x0, -0x88(%rbp)
10000587d: 74 17                       	je	0x100005896 <_mb_win_srw_sleep+0x176>
10000587f: 49 83 7d 20 00              	cmpq	$0x0, 0x20(%r13)
100005884: 0f 84 b7 05 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
10000588a: 41 83 7d 1c 00              	cmpl	$0x0, 0x1c(%r13)
10000588f: 74 32                       	je	0x1000058c3 <_mb_win_srw_sleep+0x1a3>
100005891: e9 ab 05 00 00              	jmp	0x100005e41 <_mb_win_srw_sleep+0x721>
100005896: 41 83 7d 1c 00              	cmpl	$0x0, 0x1c(%r13)
10000589b: 0f 84 a0 05 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
1000058a1: 4d 89 ee                    	movq	%r13, %r14
1000058a4: 4d 8b 6d 38                 	movq	0x38(%r13), %r13
1000058a8: e8 17 2e 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
1000058ad: 4c 89 ef                    	movq	%r13, %rdi
1000058b0: 4d 89 f5                    	movq	%r14, %r13
1000058b3: 48 89 c6                    	movq	%rax, %rsi
1000058b6: e8 d3 2d 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
1000058bb: 85 c0                       	testl	%eax, %eax
1000058bd: 0f 84 7e 05 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
1000058c3: 0f 57 c0                    	xorps	%xmm0, %xmm0
1000058c6: 0f 29 85 60 ff ff ff        	movaps	%xmm0, -0xa0(%rbp)
1000058cd: c7 85 7c ff ff ff 00 00 00 00       	movl	$0x0, -0x84(%rbp)
1000058d7: 41 83 fc ff                 	cmpl	$-0x1, %r12d
1000058db: 0f 84 bc 00 00 00           	je	0x10000599d <_mb_win_srw_sleep+0x27d>
1000058e1: 48 8d b5 60 ff ff ff        	leaq	-0xa0(%rbp), %rsi
1000058e8: bf 06 00 00 00              	movl	$0x6, %edi
1000058ed: e8 30 2d 00 00              	callq	0x100008622 <_sysconf+0x100008622>
1000058f2: 85 c0                       	testl	%eax, %eax
1000058f4: 74 09                       	je	0x1000058ff <_mb_win_srw_sleep+0x1df>
1000058f6: e8 03 2d 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
1000058fb: 8b 00                       	movl	(%rax), %eax
1000058fd: eb 02                       	jmp	0x100005901 <_mb_win_srw_sleep+0x1e1>
1000058ff: 31 c0                       	xorl	%eax, %eax
100005901: 89 85 7c ff ff ff           	movl	%eax, -0x84(%rbp)
100005907: 85 c0                       	testl	%eax, %eax
100005909: 75 2a                       	jne	0x100005935 <_mb_win_srw_sleep+0x215>
10000590b: 48 b8 c7 76 be ff ff ff ff 7f       	movabsq	$0x7fffffffffbe76c7, %rax ## imm = 0x7FFFFFFFFFBE76C7
100005915: 48 39 85 60 ff ff ff        	cmpq	%rax, -0xa0(%rbp)
10000591c: 77 0d                       	ja	0x10000592b <_mb_win_srw_sleep+0x20b>
10000591e: 48 81 bd 68 ff ff ff 00 ca 9a 3b    	cmpq	$0x3b9aca00, -0x98(%rbp) ## imm = 0x3B9ACA00
100005929: 72 0a                       	jb	0x100005935 <_mb_win_srw_sleep+0x215>
10000592b: c7 85 7c ff ff ff 54 00 00 00       	movl	$0x54, -0x84(%rbp)
100005935: 83 bd 7c ff ff ff 00        	cmpl	$0x0, -0x84(%rbp)
10000593c: 75 5f                       	jne	0x10000599d <_mb_win_srw_sleep+0x27d>
10000593e: 44 89 e0                    	movl	%r12d, %eax
100005941: 48 69 c8 d3 4d 62 10        	imulq	$0x10624dd3, %rax, %rcx ## imm = 0x10624DD3
100005948: 48 c1 e9 26                 	shrq	$0x26, %rcx
10000594c: 48 8b 85 60 ff ff ff        	movq	-0xa0(%rbp), %rax
100005953: 48 01 c8                    	addq	%rcx, %rax
100005956: 48 89 85 60 ff ff ff        	movq	%rax, -0xa0(%rbp)
10000595d: 69 c9 e8 03 00 00           	imull	$0x3e8, %ecx, %ecx      ## imm = 0x3E8
100005963: 44 89 e2                    	movl	%r12d, %edx
100005966: 29 ca                       	subl	%ecx, %edx
100005968: 69 ca 40 42 0f 00           	imull	$0xf4240, %edx, %ecx    ## imm = 0xF4240
10000596e: 48 03 8d 68 ff ff ff        	addq	-0x98(%rbp), %rcx
100005975: 48 89 8d 68 ff ff ff        	movq	%rcx, -0x98(%rbp)
10000597c: 48 81 f9 00 ca 9a 3b        	cmpq	$0x3b9aca00, %rcx       ## imm = 0x3B9ACA00
100005983: 7c 18                       	jl	0x10000599d <_mb_win_srw_sleep+0x27d>
100005985: 48 ff c0                    	incq	%rax
100005988: 48 89 85 60 ff ff ff        	movq	%rax, -0xa0(%rbp)
10000598f: 48 81 c1 00 36 65 c4        	addq	$-0x3b9aca00, %rcx      ## imm = 0xC4653600
100005996: 48 89 8d 68 ff ff ff        	movq	%rcx, -0x98(%rbp)
10000599d: 83 bd 7c ff ff ff 00        	cmpl	$0x0, -0x84(%rbp)
1000059a4: 74 24                       	je	0x1000059ca <_mb_win_srw_sleep+0x2aa>
1000059a6: 4c 89 ff                    	movq	%r15, %rdi
1000059a9: e8 10 2d 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000059ae: 85 c0                       	testl	%eax, %eax
1000059b0: 0f 85 9c 04 00 00           	jne	0x100005e52 <_mb_win_srw_sleep+0x732>
1000059b6: 48 8b 85 48 ff ff ff        	movq	-0xb8(%rbp), %rax
1000059bd: c7 00 1f 00 00 00           	movl	$0x1f, (%rax)
1000059c3: 31 c0                       	xorl	%eax, %eax
1000059c5: e9 0a 03 00 00              	jmp	0x100005cd4 <_mb_win_srw_sleep+0x5b4>
1000059ca: 4c 8d 85 7c ff ff ff        	leaq	-0x84(%rbp), %r8
1000059d1: 4c 89 ff                    	movq	%r15, %rdi
1000059d4: 48 89 de                    	movq	%rbx, %rsi
1000059d7: ba 02 00 00 00              	movl	$0x2, %edx
1000059dc: b9 01 00 00 00              	movl	$0x1, %ecx
1000059e1: e8 4a f7 ff ff              	callq	0x100005130 <_lookup>
1000059e6: 49 89 c6                    	movq	%rax, %r14
1000059e9: 48 8b 1d 28 46 00 00        	movq	0x4628(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
1000059f0: 48 8b 3b                    	movq	(%rbx), %rdi
1000059f3: 8b 8d 7c ff ff ff           	movl	-0x84(%rbp), %ecx
1000059f9: 48 8d 35 3c 35 00 00        	leaq	0x353c(%rip), %rsi      ## 0x100008f3c <_sysconf+0x100008f3c>
100005a00: 48 89 c2                    	movq	%rax, %rdx
100005a03: 31 c0                       	xorl	%eax, %eax
100005a05: e8 2a 2c 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100005a0a: 48 8b 3b                    	movq	(%rbx), %rdi
100005a0d: e8 1c 2c 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005a12: 4d 85 f6                    	testq	%r14, %r14
100005a15: 0f 84 8d 00 00 00           	je	0x100005aa8 <_mb_win_srw_sleep+0x388>
100005a1b: 0f 57 c0                    	xorps	%xmm0, %xmm0
100005a1e: 0f 29 45 b0                 	movaps	%xmm0, -0x50(%rbp)
100005a22: 0f 29 45 a0                 	movaps	%xmm0, -0x60(%rbp)
100005a26: 0f 29 45 90                 	movaps	%xmm0, -0x70(%rbp)
100005a2a: 0f 29 45 80                 	movaps	%xmm0, -0x80(%rbp)
100005a2e: 48 8d 5d 88                 	leaq	-0x78(%rbp), %rbx
100005a32: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
100005a36: e8 47 2c 00 00              	callq	0x100008682 <_sysconf+0x100008682>
100005a3b: 85 c0                       	testl	%eax, %eax
100005a3d: 75 37                       	jne	0x100005a76 <_mb_win_srw_sleep+0x356>
100005a3f: 4c 89 b5 50 ff ff ff        	movq	%r14, -0xb0(%rbp)
100005a46: 4d 89 ee                    	movq	%r13, %r14
100005a49: 4c 8d 6d c0                 	leaq	-0x40(%rbp), %r13
100005a4d: 48 89 df                    	movq	%rbx, %rdi
100005a50: 4c 89 ee                    	movq	%r13, %rsi
100005a53: e8 0c 2c 00 00              	callq	0x100008664 <_sysconf+0x100008664>
100005a58: 89 c3                       	movl	%eax, %ebx
100005a5a: 4c 89 ef                    	movq	%r13, %rdi
100005a5d: e8 1a 2c 00 00              	callq	0x10000867c <_sysconf+0x10000867c>
100005a62: 85 c0                       	testl	%eax, %eax
100005a64: 0f 85 d7 03 00 00           	jne	0x100005e41 <_mb_win_srw_sleep+0x721>
100005a6a: 89 d8                       	movl	%ebx, %eax
100005a6c: 4d 89 f5                    	movq	%r14, %r13
100005a6f: 4c 8b b5 50 ff ff ff        	movq	-0xb0(%rbp), %r14
100005a76: 89 85 7c ff ff ff           	movl	%eax, -0x84(%rbp)
100005a7c: 85 c0                       	testl	%eax, %eax
100005a7e: 74 5c                       	je	0x100005adc <_mb_win_srw_sleep+0x3bc>
100005a80: 4c 89 ff                    	movq	%r15, %rdi
100005a83: 4c 89 f6                    	movq	%r14, %rsi
100005a86: e8 55 fa ff ff              	callq	0x1000054e0 <_retire>
100005a8b: 4c 89 ff                    	movq	%r15, %rdi
100005a8e: e8 2b 2c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100005a93: 85 c0                       	testl	%eax, %eax
100005a95: 0f 85 be 03 00 00           	jne	0x100005e59 <_mb_win_srw_sleep+0x739>
100005a9b: 31 c0                       	xorl	%eax, %eax
100005a9d: 41 bc 1f 00 00 00           	movl	$0x1f, %r12d
100005aa3: e9 22 02 00 00              	jmp	0x100005cca <_mb_win_srw_sleep+0x5aa>
100005aa8: 4c 89 ff                    	movq	%r15, %rdi
100005aab: e8 0e 2c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100005ab0: 85 c0                       	testl	%eax, %eax
100005ab2: 0f 85 a8 03 00 00           	jne	0x100005e60 <_mb_win_srw_sleep+0x740>
100005ab8: 83 bd 7c ff ff ff 00        	cmpl	$0x0, -0x84(%rbp)
100005abf: b8 08 00 00 00              	movl	$0x8, %eax
100005ac4: b9 1f 00 00 00              	movl	$0x1f, %ecx
100005ac9: 0f 44 c8                    	cmovel	%eax, %ecx
100005acc: 48 8b 85 48 ff ff ff        	movq	-0xb8(%rbp), %rax
100005ad3: 89 08                       	movl	%ecx, (%rax)
100005ad5: 31 c0                       	xorl	%eax, %eax
100005ad7: e9 f8 01 00 00              	jmp	0x100005cd4 <_mb_win_srw_sleep+0x5b4>
100005adc: 49 8b 46 78                 	movq	0x78(%r14), %rax
100005ae0: 4c 89 f3                    	movq	%r14, %rbx
100005ae3: 48 83 c3 70                 	addq	$0x70, %rbx
100005ae7: 48 85 c0                    	testq	%rax, %rax
100005aea: 48 0f 44 c3                 	cmoveq	%rbx, %rax
100005aee: 48 8d 4d 80                 	leaq	-0x80(%rbp), %rcx
100005af2: 48 89 08                    	movq	%rcx, (%rax)
100005af5: 49 89 4e 78                 	movq	%rcx, 0x78(%r14)
100005af9: 48 8b 05 18 45 00 00        	movq	0x4518(%rip), %rax      ## 0x10000a018 <_sysconf+0x10000a018>
100005b00: 48 8b 08                    	movq	(%rax), %rcx
100005b03: 48 8d 3d 5b 34 00 00        	leaq	0x345b(%rip), %rdi      ## 0x100008f65 <_sysconf+0x100008f65>
100005b0a: be 1b 00 00 00              	movl	$0x1b, %esi
100005b0f: ba 01 00 00 00              	movl	$0x1, %edx
100005b14: e8 27 2b 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005b19: 48 8b 05 f8 44 00 00        	movq	0x44f8(%rip), %rax      ## 0x10000a018 <_sysconf+0x10000a018>
100005b20: 48 8b 38                    	movq	(%rax), %rdi
100005b23: e8 06 2b 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005b28: 4c 89 b5 50 ff ff ff        	movq	%r14, -0xb0(%rbp)
100005b2f: 49 8b 46 28                 	movq	0x28(%r14), %rax
100005b33: 48 83 f8 ff                 	cmpq	$-0x1, %rax
100005b37: 0f 84 04 03 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
100005b3d: 49 83 7d 28 ff              	cmpq	$-0x1, 0x28(%r13)
100005b42: 0f 84 f9 02 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
100005b48: 48 ff c0                    	incq	%rax
100005b4b: 48 8b 8d 50 ff ff ff        	movq	-0xb0(%rbp), %rcx
100005b52: 48 89 41 28                 	movq	%rax, 0x28(%rcx)
100005b56: 49 ff 45 28                 	incq	0x28(%r13)
100005b5a: 4c 89 ad 28 ff ff ff        	movq	%r13, -0xd8(%rbp)
100005b61: 4c 89 ef                    	movq	%r13, %rdi
100005b64: 8b b5 78 ff ff ff           	movl	-0x88(%rbp), %esi
100005b6a: e8 f1 fa ff ff              	callq	0x100005660 <_drop>
100005b6f: 83 7d b8 00                 	cmpl	$0x0, -0x48(%rbp)
100005b73: 4c 8d 75 88                 	leaq	-0x78(%rbp), %r14
100005b77: 0f 84 7d 01 00 00           	je	0x100005cfa <_mb_win_srw_sleep+0x5da>
100005b7d: 8b 45 b8                    	movl	-0x48(%rbp), %eax
100005b80: 89 85 5c ff ff ff           	movl	%eax, -0xa4(%rbp)
100005b86: 44 8b a5 7c ff ff ff        	movl	-0x84(%rbp), %r12d
100005b8d: 4c 8b 2d 84 44 00 00        	movq	0x4484(%rip), %r13      ## 0x10000a018 <_sysconf+0x10000a018>
100005b94: 49 8b 4d 00                 	movq	(%r13), %rcx
100005b98: 48 8d 3d e2 33 00 00        	leaq	0x33e2(%rip), %rdi      ## 0x100008f81 <_sysconf+0x100008f81>
100005b9f: be 1b 00 00 00              	movl	$0x1b, %esi
100005ba4: ba 01 00 00 00              	movl	$0x1, %edx
100005ba9: e8 92 2a 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005bae: 49 8b 7d 00                 	movq	(%r13), %rdi
100005bb2: e8 77 2a 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005bb7: 31 d2                       	xorl	%edx, %edx
100005bb9: 4c 8d 45 80                 	leaq	-0x80(%rbp), %r8
100005bbd: 0f 1f 00                    	nopl	(%rax)
100005bc0: 48 89 d0                    	movq	%rdx, %rax
100005bc3: 48 89 d9                    	movq	%rbx, %rcx
100005bc6: 48 8b 1b                    	movq	(%rbx), %rbx
100005bc9: 4c 39 c3                    	cmpq	%r8, %rbx
100005bcc: 40 0f 94 c6                 	sete	%sil
100005bd0: 48 85 db                    	testq	%rbx, %rbx
100005bd3: 74 08                       	je	0x100005bdd <_mb_win_srw_sleep+0x4bd>
100005bd5: 48 89 da                    	movq	%rbx, %rdx
100005bd8: 40 84 f6                    	testb	%sil, %sil
100005bdb: 74 e3                       	je	0x100005bc0 <_mb_win_srw_sleep+0x4a0>
100005bdd: 83 bd 5c ff ff ff 00        	cmpl	$0x0, -0xa4(%rbp)
100005be4: 41 0f 95 c5                 	setne	%r13b
100005be8: 41 83 fc 3c                 	cmpl	$0x3c, %r12d
100005bec: ba b4 05 00 00              	movl	$0x5b4, %edx            ## imm = 0x5B4
100005bf1: 41 bc 1f 00 00 00           	movl	$0x1f, %r12d
100005bf7: 44 0f 44 e2                 	cmovel	%edx, %r12d
100005bfb: 48 85 db                    	testq	%rbx, %rbx
100005bfe: 0f 84 3d 02 00 00           	je	0x100005e41 <_mb_win_srw_sleep+0x721>
100005c04: 4c 89 f7                    	movq	%r14, %rdi
100005c07: 48 8b 55 80                 	movq	-0x80(%rbp), %rdx
100005c0b: 48 89 11                    	movq	%rdx, (%rcx)
100005c0e: 4c 8b b5 50 ff ff ff        	movq	-0xb0(%rbp), %r14
100005c15: 4d 39 46 78                 	cmpq	%r8, 0x78(%r14)
100005c19: 75 04                       	jne	0x100005c1f <_mb_win_srw_sleep+0x4ff>
100005c1b: 49 89 46 78                 	movq	%rax, 0x78(%r14)
100005c1f: 49 ff 4e 28                 	decq	0x28(%r14)
100005c23: e8 36 2a 00 00              	callq	0x10000865e <_sysconf+0x10000865e>
100005c28: 85 c0                       	testl	%eax, %eax
100005c2a: 0f 85 11 02 00 00           	jne	0x100005e41 <_mb_win_srw_sleep+0x721>
100005c30: 31 db                       	xorl	%ebx, %ebx
100005c32: 44 88 eb                    	movb	%r13b, %bl
100005c35: 4c 89 ff                    	movq	%r15, %rdi
100005c38: 4c 89 f6                    	movq	%r14, %rsi
100005c3b: e8 a0 f8 ff ff              	callq	0x1000054e0 <_retire>
100005c40: 4c 8b 35 d1 43 00 00        	movq	0x43d1(%rip), %r14      ## 0x10000a018 <_sysconf+0x10000a018>
100005c47: 49 8b 3e                    	movq	(%r14), %rdi
100005c4a: 8b 8d 7c ff ff ff           	movl	-0x84(%rbp), %ecx
100005c50: 48 8d 35 46 33 00 00        	leaq	0x3346(%rip), %rsi      ## 0x100008f9d <_sysconf+0x100008f9d>
100005c57: 41 89 dd                    	movl	%ebx, %r13d
100005c5a: 89 da                       	movl	%ebx, %edx
100005c5c: 31 c0                       	xorl	%eax, %eax
100005c5e: e8 d1 29 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100005c63: 49 8b 3e                    	movq	(%r14), %rdi
100005c66: e8 c3 29 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005c6b: 4c 89 ff                    	movq	%r15, %rdi
100005c6e: 48 8b 9d 28 ff ff ff        	movq	-0xd8(%rbp), %rbx
100005c75: 48 89 de                    	movq	%rbx, %rsi
100005c78: 8b 95 78 ff ff ff           	movl	-0x88(%rbp), %edx
100005c7e: e8 4d f6 ff ff              	callq	0x1000052d0 <_take>
100005c83: 48 ff 4b 28                 	decq	0x28(%rbx)
100005c87: 49 8b 0e                    	movq	(%r14), %rcx
100005c8a: 48 8d 3d 40 33 00 00        	leaq	0x3340(%rip), %rdi      ## 0x100008fd1 <_sysconf+0x100008fd1>
100005c91: be 21 00 00 00              	movl	$0x21, %esi
100005c96: ba 01 00 00 00              	movl	$0x1, %edx
100005c9b: e8 a0 29 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005ca0: 49 8b 3e                    	movq	(%r14), %rdi
100005ca3: e8 86 29 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005ca8: 4c 89 ff                    	movq	%r15, %rdi
100005cab: e8 0e 2a 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100005cb0: 85 c0                       	testl	%eax, %eax
100005cb2: 0f 85 af 01 00 00           	jne	0x100005e67 <_mb_win_srw_sleep+0x747>
100005cb8: 83 bd 5c ff ff ff 00        	cmpl	$0x0, -0xa4(%rbp)
100005cbf: 44 0f 45 a5 44 ff ff ff     	cmovnel	-0xbc(%rbp), %r12d
100005cc7: 44 89 e8                    	movl	%r13d, %eax
100005cca: 48 8b 8d 48 ff ff ff        	movq	-0xb8(%rbp), %rcx
100005cd1: 44 89 21                    	movl	%r12d, (%rcx)
100005cd4: 48 8b 0d 35 43 00 00        	movq	0x4335(%rip), %rcx      ## 0x10000a010 <_sysconf+0x10000a010>
100005cdb: 48 8b 09                    	movq	(%rcx), %rcx
100005cde: 48 3b 4d d0                 	cmpq	-0x30(%rbp), %rcx
100005ce2: 0f 85 5e 01 00 00           	jne	0x100005e46 <_mb_win_srw_sleep+0x726>
100005ce8: 48 81 c4 b8 00 00 00        	addq	$0xb8, %rsp
100005cef: 5b                          	popq	%rbx
100005cf0: 41 5c                       	popq	%r12
100005cf2: 41 5d                       	popq	%r13
100005cf4: 41 5e                       	popq	%r14
100005cf6: 41 5f                       	popq	%r15
100005cf8: 5d                          	popq	%rbp
100005cf9: c3                          	retq
100005cfa: 4c 8d 6d c0                 	leaq	-0x40(%rbp), %r13
100005cfe: eb 0a                       	jmp	0x100005d0a <_mb_win_srw_sleep+0x5ea>
100005d00: 83 7d b8 00                 	cmpl	$0x0, -0x48(%rbp)
100005d04: 0f 85 73 fe ff ff           	jne	0x100005b7d <_mb_win_srw_sleep+0x45d>
100005d0a: 41 83 fc ff                 	cmpl	$-0x1, %r12d
100005d0e: 74 22                       	je	0x100005d32 <_mb_win_srw_sleep+0x612>
100005d10: bf 06 00 00 00              	movl	$0x6, %edi
100005d15: 4c 89 ee                    	movq	%r13, %rsi
100005d18: e8 05 29 00 00              	callq	0x100008622 <_sysconf+0x100008622>
100005d1d: 85 c0                       	testl	%eax, %eax
100005d1f: 74 1e                       	je	0x100005d3f <_mb_win_srw_sleep+0x61f>
100005d21: e8 d8 28 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100005d26: 8b 00                       	movl	(%rax), %eax
100005d28: 85 c0                       	testl	%eax, %eax
100005d2a: 0f 85 80 00 00 00           	jne	0x100005db0 <_mb_win_srw_sleep+0x690>
100005d30: eb 13                       	jmp	0x100005d45 <_mb_win_srw_sleep+0x625>
100005d32: 4c 89 f7                    	movq	%r14, %rdi
100005d35: 4c 89 fe                    	movq	%r15, %rsi
100005d38: e8 39 29 00 00              	callq	0x100008676 <_sysconf+0x100008676>
100005d3d: eb 71                       	jmp	0x100005db0 <_mb_win_srw_sleep+0x690>
100005d3f: 31 c0                       	xorl	%eax, %eax
100005d41: 85 c0                       	testl	%eax, %eax
100005d43: 75 6b                       	jne	0x100005db0 <_mb_win_srw_sleep+0x690>
100005d45: 48 8b 8d 60 ff ff ff        	movq	-0xa0(%rbp), %rcx
100005d4c: 48 2b 4d c0                 	subq	-0x40(%rbp), %rcx
100005d50: 7e 46                       	jle	0x100005d98 <_mb_win_srw_sleep+0x678>
100005d52: 48 89 8d 30 ff ff ff        	movq	%rcx, -0xd0(%rbp)
100005d59: 48 8b 85 68 ff ff ff        	movq	-0x98(%rbp), %rax
100005d60: 48 2b 45 c8                 	subq	-0x38(%rbp), %rax
100005d64: 48 89 85 38 ff ff ff        	movq	%rax, -0xc8(%rbp)
100005d6b: 79 17                       	jns	0x100005d84 <_mb_win_srw_sleep+0x664>
100005d6d: 48 ff c9                    	decq	%rcx
100005d70: 48 89 8d 30 ff ff ff        	movq	%rcx, -0xd0(%rbp)
100005d77: 48 05 00 ca 9a 3b           	addq	$0x3b9aca00, %rax       ## imm = 0x3B9ACA00
100005d7d: 48 89 85 38 ff ff ff        	movq	%rax, -0xc8(%rbp)
100005d84: 4c 89 f7                    	movq	%r14, %rdi
100005d87: 4c 89 fe                    	movq	%r15, %rsi
100005d8a: 48 8d 95 30 ff ff ff        	leaq	-0xd0(%rbp), %rdx
100005d91: e8 da 28 00 00              	callq	0x100008670 <_sysconf+0x100008670>
100005d96: eb 18                       	jmp	0x100005db0 <_mb_win_srw_sleep+0x690>
100005d98: b8 3c 00 00 00              	movl	$0x3c, %eax
100005d9d: 75 11                       	jne	0x100005db0 <_mb_win_srw_sleep+0x690>
100005d9f: 48 8b 55 c8                 	movq	-0x38(%rbp), %rdx
100005da3: 48 3b 95 68 ff ff ff        	cmpq	-0x98(%rbp), %rdx
100005daa: 7c a6                       	jl	0x100005d52 <_mb_win_srw_sleep+0x632>
100005dac: 0f 1f 40 00                 	nopl	(%rax)
100005db0: 89 85 7c ff ff ff           	movl	%eax, -0x84(%rbp)
100005db6: 85 c0                       	testl	%eax, %eax
100005db8: 0f 84 42 ff ff ff           	je	0x100005d00 <_mb_win_srw_sleep+0x5e0>
100005dbe: 83 f8 3c                    	cmpl	$0x3c, %eax
100005dc1: 0f 85 b6 fd ff ff           	jne	0x100005b7d <_mb_win_srw_sleep+0x45d>
100005dc7: bf 06 00 00 00              	movl	$0x6, %edi
100005dcc: 4c 89 ee                    	movq	%r13, %rsi
100005dcf: e8 4e 28 00 00              	callq	0x100008622 <_sysconf+0x100008622>
100005dd4: 85 c0                       	testl	%eax, %eax
100005dd6: 74 1c                       	je	0x100005df4 <_mb_win_srw_sleep+0x6d4>
100005dd8: e8 21 28 00 00              	callq	0x1000085fe <_sysconf+0x1000085fe>
100005ddd: 8b 00                       	movl	(%rax), %eax
100005ddf: 85 c0                       	testl	%eax, %eax
100005de1: 74 17                       	je	0x100005dfa <_mb_win_srw_sleep+0x6da>
100005de3: 89 85 7c ff ff ff           	movl	%eax, -0x84(%rbp)
100005de9: 31 c0                       	xorl	%eax, %eax
100005deb: 84 c0                       	testb	%al, %al
100005ded: 75 2f                       	jne	0x100005e1e <_mb_win_srw_sleep+0x6fe>
100005def: e9 89 fd ff ff              	jmp	0x100005b7d <_mb_win_srw_sleep+0x45d>
100005df4: 31 c0                       	xorl	%eax, %eax
100005df6: 85 c0                       	testl	%eax, %eax
100005df8: 75 e9                       	jne	0x100005de3 <_mb_win_srw_sleep+0x6c3>
100005dfa: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
100005dfe: 48 8b 8d 60 ff ff ff        	movq	-0xa0(%rbp), %rcx
100005e05: 48 39 c8                    	cmpq	%rcx, %rax
100005e08: 7d 26                       	jge	0x100005e30 <_mb_win_srw_sleep+0x710>
100005e0a: c7 85 7c ff ff ff 00 00 00 00       	movl	$0x0, -0x84(%rbp)
100005e14: b0 01                       	movb	$0x1, %al
100005e16: 84 c0                       	testb	%al, %al
100005e18: 0f 84 5f fd ff ff           	je	0x100005b7d <_mb_win_srw_sleep+0x45d>
100005e1e: 83 bd 7c ff ff ff 00        	cmpl	$0x0, -0x84(%rbp)
100005e25: 0f 84 d5 fe ff ff           	je	0x100005d00 <_mb_win_srw_sleep+0x5e0>
100005e2b: e9 4d fd ff ff              	jmp	0x100005b7d <_mb_win_srw_sleep+0x45d>
100005e30: 75 b7                       	jne	0x100005de9 <_mb_win_srw_sleep+0x6c9>
100005e32: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
100005e36: 48 3b 85 68 ff ff ff        	cmpq	-0x98(%rbp), %rax
100005e3d: 7c cb                       	jl	0x100005e0a <_mb_win_srw_sleep+0x6ea>
100005e3f: eb a8                       	jmp	0x100005de9 <_mb_win_srw_sleep+0x6c9>
100005e41: e8 ca 27 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100005e46: e8 b9 27 00 00              	callq	0x100008604 <_sysconf+0x100008604>
100005e4b: 89 c7                       	movl	%eax, %edi
100005e4d: e8 4e 27 00 00              	callq	0x1000085a0 <_mb_win_srw_sleep.cold.5>
100005e52: 89 c7                       	movl	%eax, %edi
100005e54: e8 87 26 00 00              	callq	0x1000084e0 <_mb_win_srw_sleep.cold.1>
100005e59: 89 c7                       	movl	%eax, %edi
100005e5b: e8 b0 26 00 00              	callq	0x100008510 <_mb_win_srw_sleep.cold.2>
100005e60: 89 c7                       	movl	%eax, %edi
100005e62: e8 09 27 00 00              	callq	0x100008570 <_mb_win_srw_sleep.cold.4>
100005e67: 89 c7                       	movl	%eax, %edi
100005e69: e8 d2 26 00 00              	callq	0x100008540 <_mb_win_srw_sleep.cold.3>
100005e6e: 66 90                       	nop

0000000100005e70 <_mb_win_srw_wake>:
100005e70: 55                          	pushq	%rbp
100005e71: 48 89 e5                    	movq	%rsp, %rbp
100005e74: 41 57                       	pushq	%r15
100005e76: 41 56                       	pushq	%r14
100005e78: 41 54                       	pushq	%r12
100005e7a: 53                          	pushq	%rbx
100005e7b: 89 d3                       	movl	%edx, %ebx
100005e7d: 49 89 f7                    	movq	%rsi, %r15
100005e80: 49 89 fe                    	movq	%rdi, %r14
100005e83: 4c 8b 25 8e 41 00 00        	movq	0x418e(%rip), %r12      ## 0x10000a018 <_sysconf+0x10000a018>
100005e8a: 49 8b 0c 24                 	movq	(%r12), %rcx
100005e8e: 48 8d 3d 5e 31 00 00        	leaq	0x315e(%rip), %rdi      ## 0x100008ff3 <_sysconf+0x100008ff3>
100005e95: be 1b 00 00 00              	movl	$0x1b, %esi
100005e9a: ba 01 00 00 00              	movl	$0x1, %edx
100005e9f: e8 9c 27 00 00              	callq	0x100008640 <_sysconf+0x100008640>
100005ea4: 49 8b 3c 24                 	movq	(%r12), %rdi
100005ea8: e8 81 27 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100005ead: 4d 85 f6                    	testq	%r14, %r14
100005eb0: 0f 84 87 00 00 00           	je	0x100005f3d <_mb_win_srw_wake+0xcd>
100005eb6: 4d 8b 36                    	movq	(%r14), %r14
100005eb9: 4d 85 f6                    	testq	%r14, %r14
100005ebc: 0f 84 7b 00 00 00           	je	0x100005f3d <_mb_win_srw_wake+0xcd>
100005ec2: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100005ec6: e8 55 c2 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100005ecb: 4c 89 f7                    	movq	%r14, %rdi
100005ece: e8 e5 27 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100005ed3: 85 c0                       	testl	%eax, %eax
100005ed5: 75 66                       	jne	0x100005f3d <_mb_win_srw_wake+0xcd>
100005ed7: 83 fb 02                    	cmpl	$0x2, %ebx
100005eda: 73 61                       	jae	0x100005f3d <_mb_win_srw_wake+0xcd>
100005edc: 4c 89 f7                    	movq	%r14, %rdi
100005edf: 4c 89 fe                    	movq	%r15, %rsi
100005ee2: ba 02 00 00 00              	movl	$0x2, %edx
100005ee7: 31 c9                       	xorl	%ecx, %ecx
100005ee9: 45 31 c0                    	xorl	%r8d, %r8d
100005eec: e8 3f f2 ff ff              	callq	0x100005130 <_lookup>
100005ef1: 48 85 c0                    	testq	%rax, %rax
100005ef4: 74 32                       	je	0x100005f28 <_mb_win_srw_wake+0xb8>
100005ef6: 4c 8b 78 70                 	movq	0x70(%rax), %r15
100005efa: eb 07                       	jmp	0x100005f03 <_mb_win_srw_wake+0x93>
100005efc: 0f 1f 40 00                 	nopl	(%rax)
100005f00: 4d 8b 3f                    	movq	(%r15), %r15
100005f03: 4d 85 ff                    	testq	%r15, %r15
100005f06: 74 20                       	je	0x100005f28 <_mb_win_srw_wake+0xb8>
100005f08: 41 83 7f 38 00              	cmpl	$0x0, 0x38(%r15)
100005f0d: 75 f1                       	jne	0x100005f00 <_mb_win_srw_wake+0x90>
100005f0f: 41 c7 47 38 01 00 00 00     	movl	$0x1, 0x38(%r15)
100005f17: 49 8d 7f 08                 	leaq	0x8(%r15), %rdi
100005f1b: e8 4a 27 00 00              	callq	0x10000866a <_sysconf+0x10000866a>
100005f20: 85 c0                       	testl	%eax, %eax
100005f22: 75 19                       	jne	0x100005f3d <_mb_win_srw_wake+0xcd>
100005f24: 85 db                       	testl	%ebx, %ebx
100005f26: 75 d8                       	jne	0x100005f00 <_mb_win_srw_wake+0x90>
100005f28: 4c 89 f7                    	movq	%r14, %rdi
100005f2b: e8 8e 27 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100005f30: 85 c0                       	testl	%eax, %eax
100005f32: 75 0e                       	jne	0x100005f42 <_mb_win_srw_wake+0xd2>
100005f34: 5b                          	popq	%rbx
100005f35: 41 5c                       	popq	%r12
100005f37: 41 5e                       	popq	%r14
100005f39: 41 5f                       	popq	%r15
100005f3b: 5d                          	popq	%rbp
100005f3c: c3                          	retq
100005f3d: e8 ce 26 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100005f42: 89 c7                       	movl	%eax, %edi
100005f44: e8 87 26 00 00              	callq	0x1000085d0 <_mb_win_srw_wake.cold.1>
100005f49: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100005f50 <_mb_win_init_once_open>:
100005f50: 55                          	pushq	%rbp
100005f51: 48 89 e5                    	movq	%rsp, %rbp
100005f54: 41 57                       	pushq	%r15
100005f56: 41 56                       	pushq	%r14
100005f58: 41 55                       	pushq	%r13
100005f5a: 41 54                       	pushq	%r12
100005f5c: 53                          	pushq	%rbx
100005f5d: 50                          	pushq	%rax
100005f5e: 48 85 ff                    	testq	%rdi, %rdi
100005f61: 0f 94 c1                    	sete	%cl
100005f64: 48 85 f6                    	testq	%rsi, %rsi
100005f67: 41 0f 94 c0                 	sete	%r8b
100005f6b: b8 01 00 00 00              	movl	$0x1, %eax
100005f70: 41 08 c8                    	orb	%cl, %r8b
100005f73: 75 18                       	jne	0x100005f8d <_mb_win_init_once_open+0x3d>
100005f75: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100005f79: 74 12                       	je	0x100005f8d <_mb_win_init_once_open+0x3d>
100005f7b: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100005f80: 74 0b                       	je	0x100005f8d <_mb_win_init_once_open+0x3d>
100005f82: b8 02 00 00 00              	movl	$0x2, %eax
100005f87: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
100005f8b: 74 0f                       	je	0x100005f9c <_mb_win_init_once_open+0x4c>
100005f8d: 48 83 c4 08                 	addq	$0x8, %rsp
100005f91: 5b                          	popq	%rbx
100005f92: 41 5c                       	popq	%r12
100005f94: 41 5d                       	popq	%r13
100005f96: 41 5e                       	popq	%r14
100005f98: 41 5f                       	popq	%r15
100005f9a: 5d                          	popq	%rbp
100005f9b: c3                          	retq
100005f9c: 49 89 f5                    	movq	%rsi, %r13
100005f9f: 49 89 fc                    	movq	%rdi, %r12
100005fa2: 49 89 d7                    	movq	%rdx, %r15
100005fa5: bf 01 00 00 00              	movl	$0x1, %edi
100005faa: be 80 00 00 00              	movl	$0x80, %esi
100005faf: e8 68 26 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
100005fb4: 48 85 c0                    	testq	%rax, %rax
100005fb7: 74 2b                       	je	0x100005fe4 <_mb_win_init_once_open+0x94>
100005fb9: 48 89 c3                    	movq	%rax, %rbx
100005fbc: 48 89 c7                    	movq	%rax, %rdi
100005fbf: 31 f6                       	xorl	%esi, %esi
100005fc1: e8 ec 26 00 00              	callq	0x1000086b2 <_sysconf+0x1000086b2>
100005fc6: 85 c0                       	testl	%eax, %eax
100005fc8: 74 2d                       	je	0x100005ff7 <_mb_win_init_once_open+0xa7>
100005fca: 41 89 c6                    	movl	%eax, %r14d
100005fcd: 48 89 df                    	movq	%rbx, %rdi
100005fd0: e8 65 26 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100005fd5: b8 04 00 00 00              	movl	$0x4, %eax
100005fda: 4d 85 ff                    	testq	%r15, %r15
100005fdd: 74 ae                       	je	0x100005f8d <_mb_win_init_once_open+0x3d>
100005fdf: 45 89 37                    	movl	%r14d, (%r15)
100005fe2: eb a9                       	jmp	0x100005f8d <_mb_win_init_once_open+0x3d>
100005fe4: b8 04 00 00 00              	movl	$0x4, %eax
100005fe9: 4d 85 ff                    	testq	%r15, %r15
100005fec: 74 9f                       	je	0x100005f8d <_mb_win_init_once_open+0x3d>
100005fee: 41 c7 07 0c 00 00 00        	movl	$0xc, (%r15)
100005ff5: eb 96                       	jmp	0x100005f8d <_mb_win_init_once_open+0x3d>
100005ff7: 49 8b 7d 00                 	movq	(%r13), %rdi
100005ffb: 48 89 de                    	movq	%rbx, %rsi
100005ffe: 48 83 c6 48                 	addq	$0x48, %rsi
100006002: e8 99 bf ff ff              	callq	0x100001fa0 <_mb_win_threads_acquire>
100006007: 85 c0                       	testl	%eax, %eax
100006009: 74 1e                       	je	0x100006029 <_mb_win_init_once_open+0xd9>
10000600b: 48 89 df                    	movq	%rbx, %rdi
10000600e: e8 99 26 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100006013: 85 c0                       	testl	%eax, %eax
100006015: 75 3d                       	jne	0x100006054 <_mb_win_init_once_open+0x104>
100006017: 48 89 df                    	movq	%rbx, %rdi
10000601a: e8 1b 26 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
10000601f: b8 02 00 00 00              	movl	$0x2, %eax
100006024: e9 64 ff ff ff              	jmp	0x100005f8d <_mb_win_init_once_open+0x3d>
100006029: 49 8b 45 00                 	movq	(%r13), %rax
10000602d: 48 89 43 40                 	movq	%rax, 0x40(%rbx)
100006031: 49 8b 45 08                 	movq	0x8(%r13), %rax
100006035: 48 89 43 70                 	movq	%rax, 0x70(%rbx)
100006039: 49 89 1c 24                 	movq	%rbx, (%r12)
10000603d: 31 c0                       	xorl	%eax, %eax
10000603f: 4d 85 ff                    	testq	%r15, %r15
100006042: 0f 84 45 ff ff ff           	je	0x100005f8d <_mb_win_init_once_open+0x3d>
100006048: 41 c7 07 00 00 00 00        	movl	$0x0, (%r15)
10000604f: e9 39 ff ff ff              	jmp	0x100005f8d <_mb_win_init_once_open+0x3d>
100006054: e8 b7 25 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100006059: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100006060 <_mb_win_init_once_close>:
100006060: 55                          	pushq	%rbp
100006061: 48 89 e5                    	movq	%rsp, %rbp
100006064: 41 57                       	pushq	%r15
100006066: 41 56                       	pushq	%r14
100006068: 41 55                       	pushq	%r13
10000606a: 41 54                       	pushq	%r12
10000606c: 53                          	pushq	%rbx
10000606d: 50                          	pushq	%rax
10000606e: 48 85 ff                    	testq	%rdi, %rdi
100006071: 74 39                       	je	0x1000060ac <_mb_win_init_once_close+0x4c>
100006073: 48 8b 1f                    	movq	(%rdi), %rbx
100006076: 48 85 db                    	testq	%rbx, %rbx
100006079: 74 38                       	je	0x1000060b3 <_mb_win_init_once_close+0x53>
10000607b: 49 89 fc                    	movq	%rdi, %r12
10000607e: 49 89 f7                    	movq	%rsi, %r15
100006081: 48 89 df                    	movq	%rbx, %rdi
100006084: e8 2f 26 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100006089: 85 c0                       	testl	%eax, %eax
10000608b: 75 1a                       	jne	0x1000060a7 <_mb_win_init_once_close+0x47>
10000608d: 48 83 7b 58 00              	cmpq	$0x0, 0x58(%rbx)
100006092: 74 33                       	je	0x1000060c7 <_mb_win_init_once_close+0x67>
100006094: 48 89 df                    	movq	%rbx, %rdi
100006097: e8 22 26 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000609c: 89 c1                       	movl	%eax, %ecx
10000609e: b8 03 00 00 00              	movl	$0x3, %eax
1000060a3: 85 c9                       	testl	%ecx, %ecx
1000060a5: 74 11                       	je	0x1000060b8 <_mb_win_init_once_close+0x58>
1000060a7: e8 64 25 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000060ac: b8 01 00 00 00              	movl	$0x1, %eax
1000060b1: eb 05                       	jmp	0x1000060b8 <_mb_win_init_once_close+0x58>
1000060b3: b8 02 00 00 00              	movl	$0x2, %eax
1000060b8: 48 83 c4 08                 	addq	$0x8, %rsp
1000060bc: 5b                          	popq	%rbx
1000060bd: 41 5c                       	popq	%r12
1000060bf: 41 5d                       	popq	%r13
1000060c1: 41 5e                       	popq	%r14
1000060c3: 41 5f                       	popq	%r15
1000060c5: 5d                          	popq	%rbp
1000060c6: c3                          	retq
1000060c7: 4c 8d 6b 60                 	leaq	0x60(%rbx), %r13
1000060cb: 4c 89 e8                    	movq	%r13, %rax
1000060ce: 66 90                       	nop
1000060d0: 48 8b 00                    	movq	(%rax), %rax
1000060d3: 48 85 c0                    	testq	%rax, %rax
1000060d6: 74 16                       	je	0x1000060ee <_mb_win_init_once_close+0x8e>
1000060d8: 83 78 18 03                 	cmpl	$0x3, 0x18(%rax)
1000060dc: 75 b6                       	jne	0x100006094 <_mb_win_init_once_close+0x34>
1000060de: 48 83 78 20 00              	cmpq	$0x0, 0x20(%rax)
1000060e3: 75 af                       	jne	0x100006094 <_mb_win_init_once_close+0x34>
1000060e5: 48 83 78 38 00              	cmpq	$0x0, 0x38(%rax)
1000060ea: 74 e4                       	je	0x1000060d0 <_mb_win_init_once_close+0x70>
1000060ec: eb a6                       	jmp	0x100006094 <_mb_win_init_once_close+0x34>
1000060ee: 4d 8b 75 00                 	movq	(%r13), %r14
1000060f2: 4d 85 f6                    	testq	%r14, %r14
1000060f5: 74 54                       	je	0x10000614b <_mb_win_init_once_close+0xeb>
1000060f7: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100006100: 49 8b 06                    	movq	(%r14), %rax
100006103: 49 89 45 00                 	movq	%rax, (%r13)
100006107: 49 83 7e 20 00              	cmpq	$0x0, 0x20(%r14)
10000610c: 74 0e                       	je	0x10000611c <_mb_win_init_once_close+0xbc>
10000610e: eb 97                       	jmp	0x1000060a7 <_mb_win_init_once_close+0x47>
100006110: 48 8b 07                    	movq	(%rdi), %rax
100006113: 49 89 46 38                 	movq	%rax, 0x38(%r14)
100006117: e8 1e 25 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
10000611c: 49 8b 7e 38                 	movq	0x38(%r14), %rdi
100006120: 48 85 ff                    	testq	%rdi, %rdi
100006123: 75 eb                       	jne	0x100006110 <_mb_win_init_once_close+0xb0>
100006125: 49 8d 7e 48                 	leaq	0x48(%r14), %rdi
100006129: e8 30 25 00 00              	callq	0x10000865e <_sysconf+0x10000865e>
10000612e: 85 c0                       	testl	%eax, %eax
100006130: 0f 85 71 ff ff ff           	jne	0x1000060a7 <_mb_win_init_once_close+0x47>
100006136: 48 ff 4b 68                 	decq	0x68(%rbx)
10000613a: 4c 89 f7                    	movq	%r14, %rdi
10000613d: e8 f8 24 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006142: 4c 8b 73 60                 	movq	0x60(%rbx), %r14
100006146: 4d 85 f6                    	testq	%r14, %r14
100006149: 75 b5                       	jne	0x100006100 <_mb_win_init_once_close+0xa0>
10000614b: 48 89 df                    	movq	%rbx, %rdi
10000614e: e8 6b 25 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006153: 85 c0                       	testl	%eax, %eax
100006155: 0f 85 4c ff ff ff           	jne	0x1000060a7 <_mb_win_init_once_close+0x47>
10000615b: 48 89 df                    	movq	%rbx, %rdi
10000615e: e8 49 25 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100006163: 85 c0                       	testl	%eax, %eax
100006165: 74 09                       	je	0x100006170 <_mb_win_init_once_close+0x110>
100006167: 89 c1                       	movl	%eax, %ecx
100006169: b8 04 00 00 00              	movl	$0x4, %eax
10000616e: eb 28                       	jmp	0x100006198 <_mb_win_init_once_close+0x138>
100006170: 48 8d 7b 48                 	leaq	0x48(%rbx), %rdi
100006174: e8 f7 be ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100006179: 85 c0                       	testl	%eax, %eax
10000617b: 0f 85 26 ff ff ff           	jne	0x1000060a7 <_mb_win_init_once_close+0x47>
100006181: 48 89 df                    	movq	%rbx, %rdi
100006184: e8 b1 24 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006189: 49 c7 04 24 00 00 00 00     	movq	$0x0, (%r12)
100006191: 31 c9                       	xorl	%ecx, %ecx
100006193: b8 00 00 00 00              	movl	$0x0, %eax
100006198: 4d 85 ff                    	testq	%r15, %r15
10000619b: 0f 84 17 ff ff ff           	je	0x1000060b8 <_mb_win_init_once_close+0x58>
1000061a1: 41 89 0f                    	movl	%ecx, (%r15)
1000061a4: e9 0f ff ff ff              	jmp	0x1000060b8 <_mb_win_init_once_close+0x58>
1000061a9: 0f 1f 80 00 00 00 00        	nopl	(%rax)

00000001000061b0 <_mb_win_init_once_acquire_pin>:
1000061b0: 55                          	pushq	%rbp
1000061b1: 48 89 e5                    	movq	%rsp, %rbp
1000061b4: 41 57                       	pushq	%r15
1000061b6: 41 56                       	pushq	%r14
1000061b8: 53                          	pushq	%rbx
1000061b9: 50                          	pushq	%rax
1000061ba: 48 85 ff                    	testq	%rdi, %rdi
1000061bd: 0f 94 c1                    	sete	%cl
1000061c0: 48 85 f6                    	testq	%rsi, %rsi
1000061c3: 0f 94 c2                    	sete	%dl
1000061c6: b8 01 00 00 00              	movl	$0x1, %eax
1000061cb: 08 ca                       	orb	%cl, %dl
1000061cd: 75 6e                       	jne	0x10000623d <_mb_win_init_once_acquire_pin+0x8d>
1000061cf: 48 8b 1f                    	movq	(%rdi), %rbx
1000061d2: b8 02 00 00 00              	movl	$0x2, %eax
1000061d7: 48 85 db                    	testq	%rbx, %rbx
1000061da: 74 61                       	je	0x10000623d <_mb_win_init_once_acquire_pin+0x8d>
1000061dc: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
1000061e0: 75 5b                       	jne	0x10000623d <_mb_win_init_once_acquire_pin+0x8d>
1000061e2: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
1000061e7: 75 54                       	jne	0x10000623d <_mb_win_init_once_acquire_pin+0x8d>
1000061e9: 49 89 ff                    	movq	%rdi, %r15
1000061ec: 49 89 f6                    	movq	%rsi, %r14
1000061ef: 48 89 df                    	movq	%rbx, %rdi
1000061f2: e8 c1 24 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000061f7: 85 c0                       	testl	%eax, %eax
1000061f9: 75 5e                       	jne	0x100006259 <_mb_win_init_once_acquire_pin+0xa9>
1000061fb: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
1000061ff: 48 85 c9                    	testq	%rcx, %rcx
100006202: 4c 89 f0                    	movq	%r14, %rax
100006205: 4c 89 fa                    	movq	%r15, %rdx
100006208: 74 14                       	je	0x10000621e <_mb_win_init_once_acquire_pin+0x6e>
10000620a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)
100006210: 48 39 c1                    	cmpq	%rax, %rcx
100006213: 74 33                       	je	0x100006248 <_mb_win_init_once_acquire_pin+0x98>
100006215: 48 8b 49 08                 	movq	0x8(%rcx), %rcx
100006219: 48 85 c9                    	testq	%rcx, %rcx
10000621c: 75 f2                       	jne	0x100006210 <_mb_win_init_once_acquire_pin+0x60>
10000621e: 48 89 10                    	movq	%rdx, (%rax)
100006221: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100006225: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
100006229: 48 89 43 58                 	movq	%rax, 0x58(%rbx)
10000622d: 48 89 df                    	movq	%rbx, %rdi
100006230: e8 89 24 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006235: 89 c1                       	movl	%eax, %ecx
100006237: 31 c0                       	xorl	%eax, %eax
100006239: 85 c9                       	testl	%ecx, %ecx
10000623b: 75 1c                       	jne	0x100006259 <_mb_win_init_once_acquire_pin+0xa9>
10000623d: 48 83 c4 08                 	addq	$0x8, %rsp
100006241: 5b                          	popq	%rbx
100006242: 41 5e                       	popq	%r14
100006244: 41 5f                       	popq	%r15
100006246: 5d                          	popq	%rbp
100006247: c3                          	retq
100006248: 48 89 df                    	movq	%rbx, %rdi
10000624b: e8 6e 24 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006250: 85 c0                       	testl	%eax, %eax
100006252: b8 02 00 00 00              	movl	$0x2, %eax
100006257: 74 e4                       	je	0x10000623d <_mb_win_init_once_acquire_pin+0x8d>
100006259: e8 b2 23 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000625e: 66 90                       	nop

0000000100006260 <_mb_win_init_once_release_pin>:
100006260: 55                          	pushq	%rbp
100006261: 48 89 e5                    	movq	%rsp, %rbp
100006264: 41 56                       	pushq	%r14
100006266: 53                          	pushq	%rbx
100006267: 48 85 ff                    	testq	%rdi, %rdi
10000626a: 74 6c                       	je	0x1000062d8 <_mb_win_init_once_release_pin+0x78>
10000626c: 48 8b 0f                    	movq	(%rdi), %rcx
10000626f: b8 02 00 00 00              	movl	$0x2, %eax
100006274: 48 85 c9                    	testq	%rcx, %rcx
100006277: 74 64                       	je	0x1000062dd <_mb_win_init_once_release_pin+0x7d>
100006279: 48 8b 19                    	movq	(%rcx), %rbx
10000627c: 48 85 db                    	testq	%rbx, %rbx
10000627f: 74 5c                       	je	0x1000062dd <_mb_win_init_once_release_pin+0x7d>
100006281: 49 89 fe                    	movq	%rdi, %r14
100006284: 48 89 df                    	movq	%rbx, %rdi
100006287: e8 2c 24 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
10000628c: 85 c0                       	testl	%eax, %eax
10000628e: 75 63                       	jne	0x1000062f3 <_mb_win_init_once_release_pin+0x93>
100006290: 48 8d 73 58                 	leaq	0x58(%rbx), %rsi
100006294: 4c 89 f0                    	movq	%r14, %rax
100006297: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
1000062a0: 48 89 f1                    	movq	%rsi, %rcx
1000062a3: 48 8b 16                    	movq	(%rsi), %rdx
1000062a6: 48 85 d2                    	testq	%rdx, %rdx
1000062a9: 74 09                       	je	0x1000062b4 <_mb_win_init_once_release_pin+0x54>
1000062ab: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
1000062af: 48 39 c2                    	cmpq	%rax, %rdx
1000062b2: 75 ec                       	jne	0x1000062a0 <_mb_win_init_once_release_pin+0x40>
1000062b4: 48 85 d2                    	testq	%rdx, %rdx
1000062b7: 74 29                       	je	0x1000062e2 <_mb_win_init_once_release_pin+0x82>
1000062b9: 48 8b 50 08                 	movq	0x8(%rax), %rdx
1000062bd: 48 89 11                    	movq	%rdx, (%rcx)
1000062c0: 0f 57 c0                    	xorps	%xmm0, %xmm0
1000062c3: 0f 11 00                    	movups	%xmm0, (%rax)
1000062c6: 48 89 df                    	movq	%rbx, %rdi
1000062c9: e8 f0 23 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000062ce: 89 c1                       	movl	%eax, %ecx
1000062d0: 31 c0                       	xorl	%eax, %eax
1000062d2: 85 c9                       	testl	%ecx, %ecx
1000062d4: 74 07                       	je	0x1000062dd <_mb_win_init_once_release_pin+0x7d>
1000062d6: eb 1b                       	jmp	0x1000062f3 <_mb_win_init_once_release_pin+0x93>
1000062d8: b8 01 00 00 00              	movl	$0x1, %eax
1000062dd: 5b                          	popq	%rbx
1000062de: 41 5e                       	popq	%r14
1000062e0: 5d                          	popq	%rbp
1000062e1: c3                          	retq
1000062e2: 48 89 df                    	movq	%rbx, %rdi
1000062e5: e8 d4 23 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000062ea: 85 c0                       	testl	%eax, %eax
1000062ec: b8 02 00 00 00              	movl	$0x2, %eax
1000062f1: 74 ea                       	je	0x1000062dd <_mb_win_init_once_release_pin+0x7d>
1000062f3: e8 18 23 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000062f8: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100006300 <_mb_win_init_once_domain>:
100006300: 48 85 ff                    	testq	%rdi, %rdi
100006303: 74 12                       	je	0x100006317 <_mb_win_init_once_domain+0x17>
100006305: 48 8b 07                    	movq	(%rdi), %rax
100006308: 48 85 c0                    	testq	%rax, %rax
10000630b: 74 0a                       	je	0x100006317 <_mb_win_init_once_domain+0x17>
10000630d: 55                          	pushq	%rbp
10000630e: 48 89 e5                    	movq	%rsp, %rbp
100006311: 48 8b 40 40                 	movq	0x40(%rax), %rax
100006315: 5d                          	popq	%rbp
100006316: c3                          	retq
100006317: 31 c0                       	xorl	%eax, %eax
100006319: c3                          	retq
10000631a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100006320 <_mb_win_init_once_begin>:
100006320: 55                          	pushq	%rbp
100006321: 48 89 e5                    	movq	%rsp, %rbp
100006324: 41 57                       	pushq	%r15
100006326: 41 56                       	pushq	%r14
100006328: 41 55                       	pushq	%r13
10000632a: 41 54                       	pushq	%r12
10000632c: 53                          	pushq	%rbx
10000632d: 48 83 ec 48                 	subq	$0x48, %rsp
100006331: 4c 89 45 b8                 	movq	%r8, -0x48(%rbp)
100006335: 48 85 ff                    	testq	%rdi, %rdi
100006338: 0f 84 cf 03 00 00           	je	0x10000670d <_mb_win_init_once_begin+0x3ed>
10000633e: 4c 8b 37                    	movq	(%rdi), %r14
100006341: 4d 85 f6                    	testq	%r14, %r14
100006344: 0f 84 c3 03 00 00           	je	0x10000670d <_mb_win_init_once_begin+0x3ed>
10000634a: 49 89 cd                    	movq	%rcx, %r13
10000634d: 41 89 d7                    	movl	%edx, %r15d
100006350: 49 89 f4                    	movq	%rsi, %r12
100006353: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100006357: e8 c4 bd ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000635c: 4c 89 f7                    	movq	%r14, %rdi
10000635f: e8 54 23 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100006364: 85 c0                       	testl	%eax, %eax
100006366: 0f 85 a1 03 00 00           	jne	0x10000670d <_mb_win_init_once_begin+0x3ed>
10000636c: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100006370: e8 ab bd ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100006375: 48 89 c3                    	movq	%rax, %rbx
100006378: 41 83 ff 04                 	cmpl	$0x4, %r15d
10000637c: 0f 83 a0 00 00 00           	jae	0x100006422 <_mb_win_init_once_begin+0x102>
100006382: 4c 89 6d c0                 	movq	%r13, -0x40(%rbp)
100006386: 48 89 5d c8                 	movq	%rbx, -0x38(%rbp)
10000638a: 8b 03                       	movl	(%rbx), %eax
10000638c: 89 45 d4                    	movl	%eax, -0x2c(%rbp)
10000638f: 44 89 fb                    	movl	%r15d, %ebx
100006392: 83 e3 01                    	andl	$0x1, %ebx
100006395: 89 da                       	movl	%ebx, %edx
100006397: 83 f2 01                    	xorl	$0x1, %edx
10000639a: 4c 89 f7                    	movq	%r14, %rdi
10000639d: 4c 89 e6                    	movq	%r12, %rsi
1000063a0: e8 8b 03 00 00              	callq	0x100006730 <_lookup>
1000063a5: 49 89 c5                    	movq	%rax, %r13
1000063a8: 49 89 c4                    	movq	%rax, %r12
1000063ab: 85 db                       	testl	%ebx, %ebx
1000063ad: 0f 85 8c 00 00 00           	jne	0x10000643f <_mb_win_init_once_begin+0x11f>
1000063b3: 4d 85 ed                    	testq	%r13, %r13
1000063b6: 0f 84 b5 00 00 00           	je	0x100006471 <_mb_win_init_once_begin+0x151>
1000063bc: 41 8b 44 24 18              	movl	0x18(%r12), %eax
1000063c1: 48 83 f8 03                 	cmpq	$0x3, %rax
1000063c5: 0f 87 4d 01 00 00           	ja	0x100006518 <_mb_win_init_once_begin+0x1f8>
1000063cb: 48 8d 0d 42 03 00 00        	leaq	0x342(%rip), %rcx       ## 0x100006714 <_mb_win_init_once_begin+0x3f4>
1000063d2: 48 63 14 81                 	movslq	(%rcx,%rax,4), %rdx
1000063d6: 48 01 ca                    	addq	%rcx, %rdx
1000063d9: ff e2                       	jmpq	*%rdx
1000063db: 45 85 ff                    	testl	%r15d, %r15d
1000063de: 0f 84 91 02 00 00           	je	0x100006675 <_mb_win_init_once_begin+0x355>
1000063e4: 41 c7 44 24 18 02 00 00 00  	movl	$0x2, 0x18(%r12)
1000063ed: bf 10 00 00 00              	movl	$0x10, %edi
1000063f2: e8 4f 22 00 00              	callq	0x100008646 <_sysconf+0x100008646>
1000063f7: 48 85 c0                    	testq	%rax, %rax
1000063fa: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000063fe: 0f 84 b8 02 00 00           	je	0x1000066bc <_mb_win_init_once_begin+0x39c>
100006404: 49 89 c7                    	movq	%rax, %r15
100006407: e8 b8 22 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
10000640c: 49 89 47 08                 	movq	%rax, 0x8(%r15)
100006410: 49 8b 44 24 38              	movq	0x38(%r12), %rax
100006415: 49 89 07                    	movq	%rax, (%r15)
100006418: 4d 89 7c 24 38              	movq	%r15, 0x38(%r12)
10000641d: e9 6a 02 00 00              	jmp	0x10000668c <_mb_win_init_once_begin+0x36c>
100006422: 4c 89 f7                    	movq	%r14, %rdi
100006425: e8 94 22 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000642a: 85 c0                       	testl	%eax, %eax
10000642c: 0f 85 db 02 00 00           	jne	0x10000670d <_mb_win_init_once_begin+0x3ed>
100006432: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
100006438: 31 c0                       	xorl	%eax, %eax
10000643a: e9 f3 01 00 00              	jmp	0x100006632 <_mb_win_init_once_begin+0x312>
10000643f: 4d 85 ed                    	testq	%r13, %r13
100006442: 74 0c                       	je	0x100006450 <_mb_win_init_once_begin+0x130>
100006444: 41 83 7c 24 18 03           	cmpl	$0x3, 0x18(%r12)
10000644a: 0f 84 8f 01 00 00           	je	0x1000065df <_mb_win_init_once_begin+0x2bf>
100006450: 4c 89 f7                    	movq	%r14, %rdi
100006453: e8 66 22 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006458: 85 c0                       	testl	%eax, %eax
10000645a: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
10000645e: 0f 85 a9 02 00 00           	jne	0x10000670d <_mb_win_init_once_begin+0x3ed>
100006464: c7 00 e5 03 00 00           	movl	$0x3e5, (%rax)          ## imm = 0x3E5
10000646a: 31 c0                       	xorl	%eax, %eax
10000646c: e9 c1 01 00 00              	jmp	0x100006632 <_mb_win_init_once_begin+0x312>
100006471: 4c 89 f7                    	movq	%r14, %rdi
100006474: e8 45 22 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006479: 85 c0                       	testl	%eax, %eax
10000647b: 0f 85 8c 02 00 00           	jne	0x10000670d <_mb_win_init_once_begin+0x3ed>
100006481: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
100006485: c7 00 08 00 00 00           	movl	$0x8, (%rax)
10000648b: 31 c0                       	xorl	%eax, %eax
10000648d: e9 a0 01 00 00              	jmp	0x100006632 <_mb_win_init_once_begin+0x312>
100006492: 45 85 ff                    	testl	%r15d, %r15d
100006495: 74 56                       	je	0x1000064ed <_mb_win_init_once_begin+0x1cd>
100006497: bf 10 00 00 00              	movl	$0x10, %edi
10000649c: e8 a5 21 00 00              	callq	0x100008646 <_sysconf+0x100008646>
1000064a1: 48 85 c0                    	testq	%rax, %rax
1000064a4: 0f 84 fc 01 00 00           	je	0x1000066a6 <_mb_win_init_once_begin+0x386>
1000064aa: 49 89 c7                    	movq	%rax, %r15
1000064ad: e8 12 22 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
1000064b2: 49 89 47 08                 	movq	%rax, 0x8(%r15)
1000064b6: 49 8b 44 24 38              	movq	0x38(%r12), %rax
1000064bb: 49 89 07                    	movq	%rax, (%r15)
1000064be: 4d 89 7c 24 38              	movq	%r15, 0x38(%r12)
1000064c3: e9 3d 01 00 00              	jmp	0x100006605 <_mb_win_init_once_begin+0x2e5>
1000064c8: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
1000064cc: 48 85 c0                    	testq	%rax, %rax
1000064cf: 74 06                       	je	0x1000064d7 <_mb_win_init_once_begin+0x1b7>
1000064d1: c7 00 00 00 00 00           	movl	$0x0, (%rax)
1000064d7: 48 8b 4d b8                 	movq	-0x48(%rbp), %rcx
1000064db: 48 85 c9                    	testq	%rcx, %rcx
1000064de: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000064e2: 0f 85 13 01 00 00           	jne	0x1000065fb <_mb_win_init_once_begin+0x2db>
1000064e8: e9 2b 01 00 00              	jmp	0x100006618 <_mb_win_init_once_begin+0x2f8>
1000064ed: 83 f8 02                    	cmpl	$0x2, %eax
1000064f0: 0f 84 ea 01 00 00           	je	0x1000066e0 <_mb_win_init_once_begin+0x3c0>
1000064f6: 83 f8 01                    	cmpl	$0x1, %eax
1000064f9: 75 1d                       	jne	0x100006518 <_mb_win_init_once_begin+0x1f8>
1000064fb: 49 8b 5c 24 30              	movq	0x30(%r12), %rbx
100006500: e8 bf 21 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100006505: 48 89 df                    	movq	%rbx, %rdi
100006508: 48 89 c6                    	movq	%rax, %rsi
10000650b: e8 7e 21 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100006510: 85 c0                       	testl	%eax, %eax
100006512: 0f 85 f5 01 00 00           	jne	0x10000670d <_mb_win_init_once_begin+0x3ed>
100006518: 48 c7 45 98 00 00 00 00     	movq	$0x0, -0x68(%rbp)
100006520: e8 9f 21 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100006525: 48 89 45 a0                 	movq	%rax, -0x60(%rbp)
100006529: 41 d1 ef                    	shrl	%r15d
10000652c: 44 89 7d a8                 	movl	%r15d, -0x58(%rbp)
100006530: 49 8b 44 24 28              	movq	0x28(%r12), %rax
100006535: 49 8d 5d 20                 	leaq	0x20(%r13), %rbx
100006539: 48 85 c0                    	testq	%rax, %rax
10000653c: 48 0f 44 c3                 	cmoveq	%rbx, %rax
100006540: 48 8d 4d 98                 	leaq	-0x68(%rbp), %rcx
100006544: 48 89 08                    	movq	%rcx, (%rax)
100006547: 49 89 4c 24 28              	movq	%rcx, 0x28(%r12)
10000654c: 49 83 c5 48                 	addq	$0x48, %r13
100006550: eb 33                       	jmp	0x100006585 <_mb_win_init_once_begin+0x265>
100006552: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100006560: 49 8b 7c 24 30              	movq	0x30(%r12), %rdi
100006565: 48 8b 75 a0                 	movq	-0x60(%rbp), %rsi
100006569: e8 20 21 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
10000656e: 85 c0                       	testl	%eax, %eax
100006570: 75 22                       	jne	0x100006594 <_mb_win_init_once_begin+0x274>
100006572: 4c 89 ef                    	movq	%r13, %rdi
100006575: 4c 89 f6                    	movq	%r14, %rsi
100006578: e8 f9 20 00 00              	callq	0x100008676 <_sysconf+0x100008676>
10000657d: 85 c0                       	testl	%eax, %eax
10000657f: 0f 85 88 01 00 00           	jne	0x10000670d <_mb_win_init_once_begin+0x3ed>
100006585: 41 8b 44 24 18              	movl	0x18(%r12), %eax
10000658a: 83 f8 01                    	cmpl	$0x1, %eax
10000658d: 74 d1                       	je	0x100006560 <_mb_win_init_once_begin+0x240>
10000658f: 83 f8 03                    	cmpl	$0x3, %eax
100006592: 75 de                       	jne	0x100006572 <_mb_win_init_once_begin+0x252>
100006594: 49 8b 44 24 20              	movq	0x20(%r12), %rax
100006599: 48 85 c0                    	testq	%rax, %rax
10000659c: 74 18                       	je	0x1000065b6 <_mb_win_init_once_begin+0x296>
10000659e: 4c 8b 7d a0                 	movq	-0x60(%rbp), %r15
1000065a2: 48 8b 78 08                 	movq	0x8(%rax), %rdi
1000065a6: 4c 89 fe                    	movq	%r15, %rsi
1000065a9: e8 e0 20 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
1000065ae: 85 c0                       	testl	%eax, %eax
1000065b0: 0f 84 8b 00 00 00           	je	0x100006641 <_mb_win_init_once_begin+0x321>
1000065b6: 45 31 ed                    	xorl	%r13d, %r13d
1000065b9: 48 8b 03                    	movq	(%rbx), %rax
1000065bc: 48 85 c0                    	testq	%rax, %rax
1000065bf: 0f 84 48 01 00 00           	je	0x10000670d <_mb_win_init_once_begin+0x3ed>
1000065c5: 48 8b 08                    	movq	(%rax), %rcx
1000065c8: 48 89 0b                    	movq	%rcx, (%rbx)
1000065cb: 49 39 44 24 28              	cmpq	%rax, 0x28(%r12)
1000065d0: 75 05                       	jne	0x1000065d7 <_mb_win_init_once_begin+0x2b7>
1000065d2: 4d 89 6c 24 28              	movq	%r13, 0x28(%r12)
1000065d7: 41 83 7c 24 18 03           	cmpl	$0x3, 0x18(%r12)
1000065dd: 75 26                       	jne	0x100006605 <_mb_win_init_once_begin+0x2e5>
1000065df: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
1000065e3: 48 85 c0                    	testq	%rax, %rax
1000065e6: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000065ea: 74 06                       	je	0x1000065f2 <_mb_win_init_once_begin+0x2d2>
1000065ec: c7 00 00 00 00 00           	movl	$0x0, (%rax)
1000065f2: 48 8b 4d b8                 	movq	-0x48(%rbp), %rcx
1000065f6: 48 85 c9                    	testq	%rcx, %rcx
1000065f9: 74 1d                       	je	0x100006618 <_mb_win_init_once_begin+0x2f8>
1000065fb: 49 8b 44 24 40              	movq	0x40(%r12), %rax
100006600: 48 89 01                    	movq	%rax, (%rcx)
100006603: eb 13                       	jmp	0x100006618 <_mb_win_init_once_begin+0x2f8>
100006605: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
100006609: 48 85 c0                    	testq	%rax, %rax
10000660c: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
100006610: 74 06                       	je	0x100006618 <_mb_win_init_once_begin+0x2f8>
100006612: c7 00 01 00 00 00           	movl	$0x1, (%rax)
100006618: 4c 89 f7                    	movq	%r14, %rdi
10000661b: e8 9e 20 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006620: 85 c0                       	testl	%eax, %eax
100006622: 0f 85 e5 00 00 00           	jne	0x10000670d <_mb_win_init_once_begin+0x3ed>
100006628: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
10000662b: 89 03                       	movl	%eax, (%rbx)
10000662d: b8 01 00 00 00              	movl	$0x1, %eax
100006632: 48 83 c4 48                 	addq	$0x48, %rsp
100006636: 5b                          	popq	%rbx
100006637: 41 5c                       	popq	%r12
100006639: 41 5d                       	popq	%r13
10000663b: 41 5e                       	popq	%r14
10000663d: 41 5f                       	popq	%r15
10000663f: 5d                          	popq	%rbp
100006640: c3                          	retq
100006641: 49 89 dd                    	movq	%rbx, %r13
100006644: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
100006650: 4d 8b 6d 00                 	movq	(%r13), %r13
100006654: 49 8b 45 00                 	movq	(%r13), %rax
100006658: 48 85 c0                    	testq	%rax, %rax
10000665b: 74 41                       	je	0x10000669e <_mb_win_init_once_begin+0x37e>
10000665d: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100006661: 4c 89 fe                    	movq	%r15, %rsi
100006664: e8 25 20 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100006669: 4c 89 eb                    	movq	%r13, %rbx
10000666c: 85 c0                       	testl	%eax, %eax
10000666e: 74 e0                       	je	0x100006650 <_mb_win_init_once_begin+0x330>
100006670: e9 44 ff ff ff              	jmp	0x1000065b9 <_mb_win_init_once_begin+0x299>
100006675: 41 c7 44 24 18 01 00 00 00  	movl	$0x1, 0x18(%r12)
10000667e: e8 41 20 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100006683: 49 89 44 24 30              	movq	%rax, 0x30(%r12)
100006688: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
10000668c: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
100006690: 48 85 c0                    	testq	%rax, %rax
100006693: 0f 85 79 ff ff ff           	jne	0x100006612 <_mb_win_init_once_begin+0x2f2>
100006699: e9 7a ff ff ff              	jmp	0x100006618 <_mb_win_init_once_begin+0x2f8>
10000669e: 4c 89 eb                    	movq	%r13, %rbx
1000066a1: e9 13 ff ff ff              	jmp	0x1000065b9 <_mb_win_init_once_begin+0x299>
1000066a6: 4c 89 f7                    	movq	%r14, %rdi
1000066a9: e8 10 20 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000066ae: 85 c0                       	testl	%eax, %eax
1000066b0: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
1000066b4: 0f 84 cb fd ff ff           	je	0x100006485 <_mb_win_init_once_begin+0x165>
1000066ba: eb 51                       	jmp	0x10000670d <_mb_win_init_once_begin+0x3ed>
1000066bc: 4c 89 f7                    	movq	%r14, %rdi
1000066bf: 4c 89 e6                    	movq	%r12, %rsi
1000066c2: e8 49 01 00 00              	callq	0x100006810 <_destroy_active>
1000066c7: 4c 89 f7                    	movq	%r14, %rdi
1000066ca: e8 ef 1f 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000066cf: 85 c0                       	testl	%eax, %eax
1000066d1: 75 3a                       	jne	0x10000670d <_mb_win_init_once_begin+0x3ed>
1000066d3: c7 03 08 00 00 00           	movl	$0x8, (%rbx)
1000066d9: 31 c0                       	xorl	%eax, %eax
1000066db: e9 52 ff ff ff              	jmp	0x100006632 <_mb_win_init_once_begin+0x312>
1000066e0: 49 8d 5d 38                 	leaq	0x38(%r13), %rbx
1000066e4: 48 8b 1b                    	movq	(%rbx), %rbx
1000066e7: 48 85 db                    	testq	%rbx, %rbx
1000066ea: 0f 84 28 fe ff ff           	je	0x100006518 <_mb_win_init_once_begin+0x1f8>
1000066f0: 48 8b 43 08                 	movq	0x8(%rbx), %rax
1000066f4: 48 89 45 b0                 	movq	%rax, -0x50(%rbp)
1000066f8: e8 c7 1f 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
1000066fd: 48 8b 7d b0                 	movq	-0x50(%rbp), %rdi
100006701: 48 89 c6                    	movq	%rax, %rsi
100006704: e8 85 1f 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100006709: 85 c0                       	testl	%eax, %eax
10000670b: 74 d7                       	je	0x1000066e4 <_mb_win_init_once_begin+0x3c4>
10000670d: e8 fe 1e 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100006712: 66 90                       	nop
100006714: c7 fc                       	<unknown>
100006716: ff ff                       	<unknown>
100006718: e7 fd                       	outl	%eax, $0xfd
10000671a: ff ff                       	<unknown>
10000671c: 7e fd                       	jle	0x10000671b <_mb_win_init_once_begin+0x3fb>
10000671e: ff ff                       	<unknown>
100006720: b4 fd                       	movb	$-0x3, %ah
100006722: ff ff                       	<unknown>
100006724: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100006730 <_lookup>:
100006730: 55                          	pushq	%rbp
100006731: 48 89 e5                    	movq	%rsp, %rbp
100006734: 41 57                       	pushq	%r15
100006736: 41 56                       	pushq	%r14
100006738: 53                          	pushq	%rbx
100006739: 50                          	pushq	%rax
10000673a: 48 85 f6                    	testq	%rsi, %rsi
10000673d: 0f 84 c6 00 00 00           	je	0x100006809 <_lookup+0xd9>
100006743: 89 f0                       	movl	%esi, %eax
100006745: 83 e0 07                    	andl	$0x7, %eax
100006748: 0f 85 bb 00 00 00           	jne	0x100006809 <_lookup+0xd9>
10000674e: 48 8d 47 60                 	leaq	0x60(%rdi), %rax
100006752: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100006760: 48 8b 00                    	movq	(%rax), %rax
100006763: 48 85 c0                    	testq	%rax, %rax
100006766: 74 14                       	je	0x10000677c <_lookup+0x4c>
100006768: 48 39 70 08                 	cmpq	%rsi, 0x8(%rax)
10000676c: 75 f2                       	jne	0x100006760 <_lookup+0x30>
10000676e: 48 8b 0e                    	movq	(%rsi), %rcx
100006771: 48 3b 48 10                 	cmpq	0x10(%rax), %rcx
100006775: 74 5d                       	je	0x1000067d4 <_lookup+0xa4>
100006777: e9 8d 00 00 00              	jmp	0x100006809 <_lookup+0xd9>
10000677c: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100006780: 0f 85 83 00 00 00           	jne	0x100006809 <_lookup+0xd9>
100006786: 85 d2                       	testl	%edx, %edx
100006788: 74 48                       	je	0x1000067d2 <_lookup+0xa2>
10000678a: 48 8b 47 68                 	movq	0x68(%rdi), %rax
10000678e: 48 3b 47 70                 	cmpq	0x70(%rdi), %rax
100006792: 74 3e                       	je	0x1000067d2 <_lookup+0xa2>
100006794: 48 83 7f 78 ff              	cmpq	$-0x1, 0x78(%rdi)
100006799: 74 37                       	je	0x1000067d2 <_lookup+0xa2>
10000679b: 49 89 fe                    	movq	%rdi, %r14
10000679e: 49 89 f7                    	movq	%rsi, %r15
1000067a1: bf 01 00 00 00              	movl	$0x1, %edi
1000067a6: be 78 00 00 00              	movl	$0x78, %esi
1000067ab: e8 6c 1e 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
1000067b0: 48 85 c0                    	testq	%rax, %rax
1000067b3: 74 1d                       	je	0x1000067d2 <_lookup+0xa2>
1000067b5: 48 89 c3                    	movq	%rax, %rbx
1000067b8: 48 89 c7                    	movq	%rax, %rdi
1000067bb: 48 83 c7 48                 	addq	$0x48, %rdi
1000067bf: 31 f6                       	xorl	%esi, %esi
1000067c1: e8 9e 1e 00 00              	callq	0x100008664 <_sysconf+0x100008664>
1000067c6: 85 c0                       	testl	%eax, %eax
1000067c8: 74 15                       	je	0x1000067df <_lookup+0xaf>
1000067ca: 48 89 df                    	movq	%rbx, %rdi
1000067cd: e8 68 1e 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
1000067d2: 31 c0                       	xorl	%eax, %eax
1000067d4: 48 83 c4 08                 	addq	$0x8, %rsp
1000067d8: 5b                          	popq	%rbx
1000067d9: 41 5e                       	popq	%r14
1000067db: 41 5f                       	popq	%r15
1000067dd: 5d                          	popq	%rbp
1000067de: c3                          	retq
1000067df: 48 89 d8                    	movq	%rbx, %rax
1000067e2: 4c 89 7b 08                 	movq	%r15, 0x8(%rbx)
1000067e6: 49 8b 4e 78                 	movq	0x78(%r14), %rcx
1000067ea: 48 ff c1                    	incq	%rcx
1000067ed: 49 89 4e 78                 	movq	%rcx, 0x78(%r14)
1000067f1: 48 89 4b 10                 	movq	%rcx, 0x10(%rbx)
1000067f5: 49 8b 56 60                 	movq	0x60(%r14), %rdx
1000067f9: 48 89 13                    	movq	%rdx, (%rbx)
1000067fc: 49 89 5e 60                 	movq	%rbx, 0x60(%r14)
100006800: 49 ff 46 68                 	incq	0x68(%r14)
100006804: 49 89 0f                    	movq	%rcx, (%r15)
100006807: eb cb                       	jmp	0x1000067d4 <_lookup+0xa4>
100006809: e8 02 1e 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000680e: 66 90                       	nop

0000000100006810 <_destroy_active>:
100006810: 55                          	pushq	%rbp
100006811: 48 89 e5                    	movq	%rsp, %rbp
100006814: 41 57                       	pushq	%r15
100006816: 41 56                       	pushq	%r14
100006818: 53                          	pushq	%rbx
100006819: 50                          	pushq	%rax
10000681a: 48 83 7e 20 00              	cmpq	$0x0, 0x20(%rsi)
10000681f: 75 68                       	jne	0x100006889 <_destroy_active+0x79>
100006821: 48 89 f3                    	movq	%rsi, %rbx
100006824: 48 83 7e 38 00              	cmpq	$0x0, 0x38(%rsi)
100006829: 75 5e                       	jne	0x100006889 <_destroy_active+0x79>
10000682b: 49 89 fe                    	movq	%rdi, %r14
10000682e: 48 8d 47 60                 	leaq	0x60(%rdi), %rax
100006832: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100006840: 49 89 c7                    	movq	%rax, %r15
100006843: 48 8b 00                    	movq	(%rax), %rax
100006846: 48 85 c0                    	testq	%rax, %rax
100006849: 74 05                       	je	0x100006850 <_destroy_active+0x40>
10000684b: 48 39 d8                    	cmpq	%rbx, %rax
10000684e: 75 f0                       	jne	0x100006840 <_destroy_active+0x30>
100006850: 48 85 c0                    	testq	%rax, %rax
100006853: 74 34                       	je	0x100006889 <_destroy_active+0x79>
100006855: 48 8d 7b 48                 	leaq	0x48(%rbx), %rdi
100006859: e8 00 1e 00 00              	callq	0x10000865e <_sysconf+0x10000865e>
10000685e: 85 c0                       	testl	%eax, %eax
100006860: 75 27                       	jne	0x100006889 <_destroy_active+0x79>
100006862: 48 8b 43 08                 	movq	0x8(%rbx), %rax
100006866: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
10000686d: 48 8b 03                    	movq	(%rbx), %rax
100006870: 49 89 07                    	movq	%rax, (%r15)
100006873: 49 ff 4e 68                 	decq	0x68(%r14)
100006877: 48 89 df                    	movq	%rbx, %rdi
10000687a: 48 83 c4 08                 	addq	$0x8, %rsp
10000687e: 5b                          	popq	%rbx
10000687f: 41 5e                       	popq	%r14
100006881: 41 5f                       	popq	%r15
100006883: 5d                          	popq	%rbp
100006884: e9 b1 1d 00 00              	jmp	0x10000863a <_sysconf+0x10000863a>
100006889: e8 82 1d 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000688e: 66 90                       	nop

0000000100006890 <_mb_win_init_once_complete>:
100006890: 55                          	pushq	%rbp
100006891: 48 89 e5                    	movq	%rsp, %rbp
100006894: 41 57                       	pushq	%r15
100006896: 41 56                       	pushq	%r14
100006898: 41 55                       	pushq	%r13
10000689a: 41 54                       	pushq	%r12
10000689c: 53                          	pushq	%rbx
10000689d: 48 83 ec 18                 	subq	$0x18, %rsp
1000068a1: 48 85 ff                    	testq	%rdi, %rdi
1000068a4: 0f 84 d1 02 00 00           	je	0x100006b7b <_mb_win_init_once_complete+0x2eb>
1000068aa: 4c 8b 37                    	movq	(%rdi), %r14
1000068ad: 4d 85 f6                    	testq	%r14, %r14
1000068b0: 0f 84 c5 02 00 00           	je	0x100006b7b <_mb_win_init_once_complete+0x2eb>
1000068b6: 49 89 cf                    	movq	%rcx, %r15
1000068b9: 41 89 d5                    	movl	%edx, %r13d
1000068bc: 49 89 f4                    	movq	%rsi, %r12
1000068bf: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000068c3: e8 58 b8 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000068c8: 4c 89 f7                    	movq	%r14, %rdi
1000068cb: e8 e8 1d 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000068d0: 85 c0                       	testl	%eax, %eax
1000068d2: 0f 85 a3 02 00 00           	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
1000068d8: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000068dc: e8 3f b8 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000068e1: 48 89 c3                    	movq	%rax, %rbx
1000068e4: 41 f7 c5 f9 ff ff ff        	testl	$0xfffffff9, %r13d      ## imm = 0xFFFFFFF9
1000068eb: 0f 94 c0                    	sete	%al
1000068ee: 41 83 fd 06                 	cmpl	$0x6, %r13d
1000068f2: 0f 95 c1                    	setne	%cl
1000068f5: 84 c1                       	testb	%al, %cl
1000068f7: 75 1c                       	jne	0x100006915 <_mb_win_init_once_complete+0x85>
1000068f9: 4c 89 f7                    	movq	%r14, %rdi
1000068fc: e8 bd 1d 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006901: 85 c0                       	testl	%eax, %eax
100006903: 0f 85 72 02 00 00           	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006909: 31 c0                       	xorl	%eax, %eax
10000690b: b9 57 00 00 00              	movl	$0x57, %ecx
100006910: e9 48 02 00 00              	jmp	0x100006b5d <_mb_win_init_once_complete+0x2cd>
100006915: 4c 89 7d c0                 	movq	%r15, -0x40(%rbp)
100006919: 4d 85 e4                    	testq	%r12, %r12
10000691c: 0f 84 59 02 00 00           	je	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006922: 44 89 e0                    	movl	%r12d, %eax
100006925: 83 e0 07                    	andl	$0x7, %eax
100006928: 0f 85 4d 02 00 00           	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
10000692e: 8b 0b                       	movl	(%rbx), %ecx
100006930: 4d 8d 7e 60                 	leaq	0x60(%r14), %r15
100006934: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
100006940: 4d 8b 3f                    	movq	(%r15), %r15
100006943: 4d 85 ff                    	testq	%r15, %r15
100006946: 0f 84 93 00 00 00           	je	0x1000069df <_mb_win_init_once_complete+0x14f>
10000694c: 4d 39 67 08                 	cmpq	%r12, 0x8(%r15)
100006950: 75 ee                       	jne	0x100006940 <_mb_win_init_once_complete+0xb0>
100006952: 49 8b 04 24                 	movq	(%r12), %rax
100006956: 49 3b 47 10                 	cmpq	0x10(%r15), %rax
10000695a: 0f 85 1b 02 00 00           	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006960: 4d 85 ff                    	testq	%r15, %r15
100006963: 0f 84 8d 00 00 00           	je	0x1000069f6 <_mb_win_init_once_complete+0x166>
100006969: 41 8b 47 18                 	movl	0x18(%r15), %eax
10000696d: 83 f8 01                    	cmpl	$0x1, %eax
100006970: 48 89 5d c8                 	movq	%rbx, -0x38(%rbp)
100006974: 0f 84 cc 00 00 00           	je	0x100006a46 <_mb_win_init_once_complete+0x1b6>
10000697a: 83 f8 02                    	cmpl	$0x2, %eax
10000697d: 0f 84 8f 00 00 00           	je	0x100006a12 <_mb_win_init_once_complete+0x182>
100006983: 83 f8 03                    	cmpl	$0x3, %eax
100006986: 0f 85 54 01 00 00           	jne	0x100006ae0 <_mb_win_init_once_complete+0x250>
10000698c: 49 8b 4f 38                 	movq	0x38(%r15), %rcx
100006990: 48 85 c9                    	testq	%rcx, %rcx
100006993: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
100006997: 0f 84 ad 01 00 00           	je	0x100006b4a <_mb_win_init_once_complete+0x2ba>
10000699d: 49 83 c7 38                 	addq	$0x38, %r15
1000069a1: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
1000069b0: 4c 8b 61 08                 	movq	0x8(%rcx), %r12
1000069b4: e8 0b 1d 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
1000069b9: 4c 89 e7                    	movq	%r12, %rdi
1000069bc: 48 89 c6                    	movq	%rax, %rsi
1000069bf: e8 ca 1c 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
1000069c4: 49 8b 3f                    	movq	(%r15), %rdi
1000069c7: 48 8b 0f                    	movq	(%rdi), %rcx
1000069ca: 85 c0                       	testl	%eax, %eax
1000069cc: 0f 85 70 01 00 00           	jne	0x100006b42 <_mb_win_init_once_complete+0x2b2>
1000069d2: 49 89 ff                    	movq	%rdi, %r15
1000069d5: 48 85 c9                    	testq	%rcx, %rcx
1000069d8: 75 d6                       	jne	0x1000069b0 <_mb_win_init_once_complete+0x120>
1000069da: e9 6b 01 00 00              	jmp	0x100006b4a <_mb_win_init_once_complete+0x2ba>
1000069df: 49 83 3c 24 00              	cmpq	$0x0, (%r12)
1000069e4: 0f 85 91 01 00 00           	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
1000069ea: 45 31 ff                    	xorl	%r15d, %r15d
1000069ed: 4d 85 ff                    	testq	%r15, %r15
1000069f0: 0f 85 73 ff ff ff           	jne	0x100006969 <_mb_win_init_once_complete+0xd9>
1000069f6: 4c 89 f7                    	movq	%r14, %rdi
1000069f9: e8 c0 1c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000069fe: 85 c0                       	testl	%eax, %eax
100006a00: 0f 85 75 01 00 00           	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006a06: 31 c0                       	xorl	%eax, %eax
100006a08: b9 1f 00 00 00              	movl	$0x1f, %ecx
100006a0d: e9 4b 01 00 00              	jmp	0x100006b5d <_mb_win_init_once_complete+0x2cd>
100006a12: 89 4d d4                    	movl	%ecx, -0x2c(%rbp)
100006a15: 49 8d 5f 38                 	leaq	0x38(%r15), %rbx
100006a19: 0f 1f 80 00 00 00 00        	nopl	(%rax)
100006a20: 48 8b 1b                    	movq	(%rbx), %rbx
100006a23: 48 85 db                    	testq	%rbx, %rbx
100006a26: 0f 84 b4 00 00 00           	je	0x100006ae0 <_mb_win_init_once_complete+0x250>
100006a2c: 4c 8b 63 08                 	movq	0x8(%rbx), %r12
100006a30: e8 8f 1c 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100006a35: 4c 89 e7                    	movq	%r12, %rdi
100006a38: 48 89 c6                    	movq	%rax, %rsi
100006a3b: e8 4e 1c 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100006a40: 85 c0                       	testl	%eax, %eax
100006a42: 74 dc                       	je	0x100006a20 <_mb_win_init_once_complete+0x190>
100006a44: eb 1b                       	jmp	0x100006a61 <_mb_win_init_once_complete+0x1d1>
100006a46: 89 4d d4                    	movl	%ecx, -0x2c(%rbp)
100006a49: 4d 8b 67 30                 	movq	0x30(%r15), %r12
100006a4d: e8 72 1c 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100006a52: 4c 89 e7                    	movq	%r12, %rdi
100006a55: 48 89 c6                    	movq	%rax, %rsi
100006a58: e8 31 1c 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100006a5d: 85 c0                       	testl	%eax, %eax
100006a5f: 74 7f                       	je	0x100006ae0 <_mb_win_init_once_complete+0x250>
100006a61: 41 83 fd 04                 	cmpl	$0x4, %r13d
100006a65: 0f 83 92 00 00 00           	jae	0x100006afd <_mb_win_init_once_complete+0x26d>
100006a6b: 41 83 7f 18 02              	cmpl	$0x2, 0x18(%r15)
100006a70: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
100006a74: 75 58                       	jne	0x100006ace <_mb_win_init_once_complete+0x23e>
100006a76: 4d 8d 6f 38                 	leaq	0x38(%r15), %r13
100006a7a: 49 8b 47 38                 	movq	0x38(%r15), %rax
100006a7e: 48 85 c0                    	testq	%rax, %rax
100006a81: 74 32                       	je	0x100006ab5 <_mb_win_init_once_complete+0x225>
100006a83: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)
100006a90: 4c 8b 60 08                 	movq	0x8(%rax), %r12
100006a94: e8 2b 1c 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100006a99: 4c 89 e7                    	movq	%r12, %rdi
100006a9c: 48 89 c6                    	movq	%rax, %rsi
100006a9f: e8 ea 1b 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100006aa4: 85 c0                       	testl	%eax, %eax
100006aa6: 75 0d                       	jne	0x100006ab5 <_mb_win_init_once_complete+0x225>
100006aa8: 4d 8b 6d 00                 	movq	(%r13), %r13
100006aac: 49 8b 45 00                 	movq	(%r13), %rax
100006ab0: 48 85 c0                    	testq	%rax, %rax
100006ab3: 75 db                       	jne	0x100006a90 <_mb_win_init_once_complete+0x200>
100006ab5: 49 8b 7d 00                 	movq	(%r13), %rdi
100006ab9: 48 85 ff                    	testq	%rdi, %rdi
100006abc: 0f 84 b9 00 00 00           	je	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006ac2: 48 8b 07                    	movq	(%rdi), %rax
100006ac5: 49 89 45 00                 	movq	%rax, (%r13)
100006ac9: e8 6c 1b 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006ace: 41 c7 47 18 03 00 00 00     	movl	$0x3, 0x18(%r15)
100006ad6: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
100006ada: 49 89 47 40                 	movq	%rax, 0x40(%r15)
100006ade: eb 3a                       	jmp	0x100006b1a <_mb_win_init_once_complete+0x28a>
100006ae0: 4c 89 f7                    	movq	%r14, %rdi
100006ae3: e8 d6 1b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006ae8: 85 c0                       	testl	%eax, %eax
100006aea: 0f 85 8b 00 00 00           	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006af0: 31 c0                       	xorl	%eax, %eax
100006af2: b9 1f 00 00 00              	movl	$0x1f, %ecx
100006af7: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
100006afb: eb 60                       	jmp	0x100006b5d <_mb_win_init_once_complete+0x2cd>
100006afd: 49 8b 47 20                 	movq	0x20(%r15), %rax
100006b01: 48 85 c0                    	testq	%rax, %rax
100006b04: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
100006b08: 74 64                       	je	0x100006b6e <_mb_win_init_once_complete+0x2de>
100006b0a: 41 c7 47 18 01 00 00 00     	movl	$0x1, 0x18(%r15)
100006b12: 48 8b 40 08                 	movq	0x8(%rax), %rax
100006b16: 49 89 47 30                 	movq	%rax, 0x30(%r15)
100006b1a: 49 83 c7 48                 	addq	$0x48, %r15
100006b1e: 4c 89 ff                    	movq	%r15, %rdi
100006b21: e8 32 1b 00 00              	callq	0x100008658 <_sysconf+0x100008658>
100006b26: 85 c0                       	testl	%eax, %eax
100006b28: 75 51                       	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006b2a: 4c 89 f7                    	movq	%r14, %rdi
100006b2d: e8 8c 1b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006b32: 89 c1                       	movl	%eax, %ecx
100006b34: b8 01 00 00 00              	movl	$0x1, %eax
100006b39: 85 c9                       	testl	%ecx, %ecx
100006b3b: 8b 4d d4                    	movl	-0x2c(%rbp), %ecx
100006b3e: 74 1d                       	je	0x100006b5d <_mb_win_init_once_complete+0x2cd>
100006b40: eb 39                       	jmp	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006b42: 49 89 0f                    	movq	%rcx, (%r15)
100006b45: e8 f0 1a 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006b4a: 4c 89 f7                    	movq	%r14, %rdi
100006b4d: e8 6c 1b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006b52: 85 c0                       	testl	%eax, %eax
100006b54: 75 25                       	jne	0x100006b7b <_mb_win_init_once_complete+0x2eb>
100006b56: 31 c0                       	xorl	%eax, %eax
100006b58: b9 b7 00 00 00              	movl	$0xb7, %ecx
100006b5d: 89 0b                       	movl	%ecx, (%rbx)
100006b5f: 48 83 c4 18                 	addq	$0x18, %rsp
100006b63: 5b                          	popq	%rbx
100006b64: 41 5c                       	popq	%r12
100006b66: 41 5d                       	popq	%r13
100006b68: 41 5e                       	popq	%r14
100006b6a: 41 5f                       	popq	%r15
100006b6c: 5d                          	popq	%rbp
100006b6d: c3                          	retq
100006b6e: 4c 89 f7                    	movq	%r14, %rdi
100006b71: 4c 89 fe                    	movq	%r15, %rsi
100006b74: e8 97 fc ff ff              	callq	0x100006810 <_destroy_active>
100006b79: eb af                       	jmp	0x100006b2a <_mb_win_init_once_complete+0x29a>
100006b7b: e8 90 1a 00 00              	callq	0x100008610 <_sysconf+0x100008610>

0000000100006b80 <_mb_win_mutex_open>:
100006b80: 55                          	pushq	%rbp
100006b81: 48 89 e5                    	movq	%rsp, %rbp
100006b84: 41 57                       	pushq	%r15
100006b86: 41 56                       	pushq	%r14
100006b88: 41 55                       	pushq	%r13
100006b8a: 41 54                       	pushq	%r12
100006b8c: 53                          	pushq	%rbx
100006b8d: 50                          	pushq	%rax
100006b8e: 48 85 ff                    	testq	%rdi, %rdi
100006b91: 0f 94 c1                    	sete	%cl
100006b94: 48 85 f6                    	testq	%rsi, %rsi
100006b97: 41 0f 94 c0                 	sete	%r8b
100006b9b: b8 01 00 00 00              	movl	$0x1, %eax
100006ba0: 41 08 c8                    	orb	%cl, %r8b
100006ba3: 75 18                       	jne	0x100006bbd <_mb_win_mutex_open+0x3d>
100006ba5: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100006ba9: 74 12                       	je	0x100006bbd <_mb_win_mutex_open+0x3d>
100006bab: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100006bb0: 74 0b                       	je	0x100006bbd <_mb_win_mutex_open+0x3d>
100006bb2: b8 02 00 00 00              	movl	$0x2, %eax
100006bb7: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
100006bbb: 74 0f                       	je	0x100006bcc <_mb_win_mutex_open+0x4c>
100006bbd: 48 83 c4 08                 	addq	$0x8, %rsp
100006bc1: 5b                          	popq	%rbx
100006bc2: 41 5c                       	popq	%r12
100006bc4: 41 5d                       	popq	%r13
100006bc6: 41 5e                       	popq	%r14
100006bc8: 41 5f                       	popq	%r15
100006bca: 5d                          	popq	%rbp
100006bcb: c3                          	retq
100006bcc: 49 89 f7                    	movq	%rsi, %r15
100006bcf: 49 89 fd                    	movq	%rdi, %r13
100006bd2: 49 89 d4                    	movq	%rdx, %r12
100006bd5: bf 01 00 00 00              	movl	$0x1, %edi
100006bda: be 98 00 00 00              	movl	$0x98, %esi
100006bdf: e8 38 1a 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
100006be4: 48 85 c0                    	testq	%rax, %rax
100006be7: 74 16                       	je	0x100006bff <_mb_win_mutex_open+0x7f>
100006be9: 48 89 c3                    	movq	%rax, %rbx
100006bec: 48 89 c7                    	movq	%rax, %rdi
100006bef: 31 f6                       	xorl	%esi, %esi
100006bf1: e8 bc 1a 00 00              	callq	0x1000086b2 <_sysconf+0x1000086b2>
100006bf6: 85 c0                       	testl	%eax, %eax
100006bf8: 74 19                       	je	0x100006c13 <_mb_win_mutex_open+0x93>
100006bfa: 41 89 c6                    	movl	%eax, %r14d
100006bfd: eb 41                       	jmp	0x100006c40 <_mb_win_mutex_open+0xc0>
100006bff: b8 04 00 00 00              	movl	$0x4, %eax
100006c04: 4d 85 e4                    	testq	%r12, %r12
100006c07: 74 b4                       	je	0x100006bbd <_mb_win_mutex_open+0x3d>
100006c09: 41 c7 04 24 0c 00 00 00     	movl	$0xc, (%r12)
100006c11: eb aa                       	jmp	0x100006bbd <_mb_win_mutex_open+0x3d>
100006c13: 4c 8d b3 88 00 00 00        	leaq	0x88(%rbx), %r14
100006c1a: 48 8d 35 bf 00 00 00        	leaq	0xbf(%rip), %rsi        ## 0x100006ce0 <_thread_exited>
100006c21: 4c 89 f7                    	movq	%r14, %rdi
100006c24: e8 77 1a 00 00              	callq	0x1000086a0 <_sysconf+0x1000086a0>
100006c29: 85 c0                       	testl	%eax, %eax
100006c2b: 74 32                       	je	0x100006c5f <_mb_win_mutex_open+0xdf>
100006c2d: 41 89 c6                    	movl	%eax, %r14d
100006c30: 48 89 df                    	movq	%rbx, %rdi
100006c33: e8 74 1a 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100006c38: 85 c0                       	testl	%eax, %eax
100006c3a: 0f 85 91 00 00 00           	jne	0x100006cd1 <_mb_win_mutex_open+0x151>
100006c40: 48 89 df                    	movq	%rbx, %rdi
100006c43: e8 f2 19 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006c48: b8 04 00 00 00              	movl	$0x4, %eax
100006c4d: 4d 85 e4                    	testq	%r12, %r12
100006c50: 0f 84 67 ff ff ff           	je	0x100006bbd <_mb_win_mutex_open+0x3d>
100006c56: 45 89 34 24                 	movl	%r14d, (%r12)
100006c5a: e9 5e ff ff ff              	jmp	0x100006bbd <_mb_win_mutex_open+0x3d>
100006c5f: c7 83 90 00 00 00 01 00 00 00       	movl	$0x1, 0x90(%rbx)
100006c69: 49 8b 3f                    	movq	(%r15), %rdi
100006c6c: 48 89 de                    	movq	%rbx, %rsi
100006c6f: 48 83 c6 48                 	addq	$0x48, %rsi
100006c73: e8 28 b3 ff ff              	callq	0x100001fa0 <_mb_win_threads_acquire>
100006c78: 85 c0                       	testl	%eax, %eax
100006c7a: 74 2a                       	je	0x100006ca6 <_mb_win_mutex_open+0x126>
100006c7c: 49 8b 3e                    	movq	(%r14), %rdi
100006c7f: e8 22 1a 00 00              	callq	0x1000086a6 <_sysconf+0x1000086a6>
100006c84: 85 c0                       	testl	%eax, %eax
100006c86: 75 49                       	jne	0x100006cd1 <_mb_win_mutex_open+0x151>
100006c88: 48 89 df                    	movq	%rbx, %rdi
100006c8b: e8 1c 1a 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100006c90: 85 c0                       	testl	%eax, %eax
100006c92: 75 3d                       	jne	0x100006cd1 <_mb_win_mutex_open+0x151>
100006c94: 48 89 df                    	movq	%rbx, %rdi
100006c97: e8 9e 19 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006c9c: b8 02 00 00 00              	movl	$0x2, %eax
100006ca1: e9 17 ff ff ff              	jmp	0x100006bbd <_mb_win_mutex_open+0x3d>
100006ca6: 49 8b 07                    	movq	(%r15), %rax
100006ca9: 48 89 43 40                 	movq	%rax, 0x40(%rbx)
100006cad: 49 8b 47 08                 	movq	0x8(%r15), %rax
100006cb1: 48 89 43 78                 	movq	%rax, 0x78(%rbx)
100006cb5: 49 89 5d 00                 	movq	%rbx, (%r13)
100006cb9: 31 c0                       	xorl	%eax, %eax
100006cbb: 4d 85 e4                    	testq	%r12, %r12
100006cbe: 0f 84 f9 fe ff ff           	je	0x100006bbd <_mb_win_mutex_open+0x3d>
100006cc4: 41 c7 04 24 00 00 00 00     	movl	$0x0, (%r12)
100006ccc: e9 ec fe ff ff              	jmp	0x100006bbd <_mb_win_mutex_open+0x3d>
100006cd1: e8 3a 19 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100006cd6: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)

0000000100006ce0 <_thread_exited>:
100006ce0: 55                          	pushq	%rbp
100006ce1: 48 89 e5                    	movq	%rsp, %rbp
100006ce4: 41 57                       	pushq	%r15
100006ce6: 41 56                       	pushq	%r14
100006ce8: 41 55                       	pushq	%r13
100006cea: 41 54                       	pushq	%r12
100006cec: 53                          	pushq	%rbx
100006ced: 48 83 ec 18                 	subq	$0x18, %rsp
100006cf1: 48 85 ff                    	testq	%rdi, %rdi
100006cf4: 0f 84 bf 00 00 00           	je	0x100006db9 <_thread_exited+0xd9>
100006cfa: 48 89 fb                    	movq	%rdi, %rbx
100006cfd: 4c 8b 37                    	movq	(%rdi), %r14
100006d00: e8 bf 19 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100006d05: 49 89 c7                    	movq	%rax, %r15
100006d08: 4c 89 75 d0                 	movq	%r14, -0x30(%rbp)
100006d0c: 4c 89 f7                    	movq	%r14, %rdi
100006d0f: e8 a4 19 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100006d14: 85 c0                       	testl	%eax, %eax
100006d16: 0f 85 ac 00 00 00           	jne	0x100006dc8 <_thread_exited+0xe8>
100006d1c: 48 89 5d c8                 	movq	%rbx, -0x38(%rbp)
100006d20: 4c 8b 73 08                 	movq	0x8(%rbx), %r14
100006d24: 4d 85 f6                    	testq	%r14, %r14
100006d27: 74 7a                       	je	0x100006da3 <_thread_exited+0xc3>
100006d29: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
100006d2d: 48 8d 58 60                 	leaq	0x60(%rax), %rbx
100006d31: eb 1a                       	jmp	0x100006d4d <_thread_exited+0x6d>
100006d33: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)
100006d40: 4c 89 e7                    	movq	%r12, %rdi
100006d43: e8 f2 18 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006d48: 4d 85 f6                    	testq	%r14, %r14
100006d4b: 74 56                       	je	0x100006da3 <_thread_exited+0xc3>
100006d4d: 4d 89 f4                    	movq	%r14, %r12
100006d50: 4d 8b 36                    	movq	(%r14), %r14
100006d53: 49 8b 44 24 08              	movq	0x8(%r12), %rax
100006d58: 49 89 dd                    	movq	%rbx, %r13
100006d5b: 0f 1f 44 00 00              	nopl	(%rax,%rax)
100006d60: 4d 8b 6d 00                 	movq	(%r13), %r13
100006d64: 4d 85 ed                    	testq	%r13, %r13
100006d67: 74 d7                       	je	0x100006d40 <_thread_exited+0x60>
100006d69: 49 39 45 08                 	cmpq	%rax, 0x8(%r13)
100006d6d: 75 f1                       	jne	0x100006d60 <_thread_exited+0x80>
100006d6f: 41 83 7d 34 00              	cmpl	$0x0, 0x34(%r13)
100006d74: 74 ca                       	je	0x100006d40 <_thread_exited+0x60>
100006d76: 49 8b 7d 40                 	movq	0x40(%r13), %rdi
100006d7a: 4c 89 fe                    	movq	%r15, %rsi
100006d7d: e8 0c 19 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100006d82: 85 c0                       	testl	%eax, %eax
100006d84: 74 ba                       	je	0x100006d40 <_thread_exited+0x60>
100006d86: 48 b8 00 00 00 00 01 00 00 00       	movabsq	$0x100000000, %rax ## imm = 0x100000000
100006d90: 49 89 45 34                 	movq	%rax, 0x34(%r13)
100006d94: 4c 89 ef                    	movq	%r13, %rdi
100006d97: be 80 00 00 00              	movl	$0x80, %esi
100006d9c: e8 8f 0c 00 00              	callq	0x100007a30 <_grant_first>
100006da1: eb 9d                       	jmp	0x100006d40 <_thread_exited+0x60>
100006da3: 48 8b 7d c8                 	movq	-0x38(%rbp), %rdi
100006da7: e8 8e 18 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006dac: 48 8b 7d d0                 	movq	-0x30(%rbp), %rdi
100006db0: e8 09 19 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006db5: 85 c0                       	testl	%eax, %eax
100006db7: 75 0f                       	jne	0x100006dc8 <_thread_exited+0xe8>
100006db9: 48 83 c4 18                 	addq	$0x18, %rsp
100006dbd: 5b                          	popq	%rbx
100006dbe: 41 5c                       	popq	%r12
100006dc0: 41 5d                       	popq	%r13
100006dc2: 41 5e                       	popq	%r14
100006dc4: 41 5f                       	popq	%r15
100006dc6: 5d                          	popq	%rbp
100006dc7: c3                          	retq
100006dc8: e8 43 18 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100006dcd: 0f 1f 00                    	nopl	(%rax)

0000000100006dd0 <_mb_win_mutex_close>:
100006dd0: 55                          	pushq	%rbp
100006dd1: 48 89 e5                    	movq	%rsp, %rbp
100006dd4: 41 57                       	pushq	%r15
100006dd6: 41 56                       	pushq	%r14
100006dd8: 53                          	pushq	%rbx
100006dd9: 50                          	pushq	%rax
100006dda: 48 85 ff                    	testq	%rdi, %rdi
100006ddd: 74 58                       	je	0x100006e37 <_mb_win_mutex_close+0x67>
100006ddf: 48 8b 1f                    	movq	(%rdi), %rbx
100006de2: 48 85 db                    	testq	%rbx, %rbx
100006de5: 74 57                       	je	0x100006e3e <_mb_win_mutex_close+0x6e>
100006de7: 49 89 ff                    	movq	%rdi, %r15
100006dea: 49 89 f6                    	movq	%rsi, %r14
100006ded: 48 89 df                    	movq	%rbx, %rdi
100006df0: e8 c3 18 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100006df5: 85 c0                       	testl	%eax, %eax
100006df7: 75 39                       	jne	0x100006e32 <_mb_win_mutex_close+0x62>
100006df9: 48 83 7b 58 00              	cmpq	$0x0, 0x58(%rbx)
100006dfe: 75 1f                       	jne	0x100006e1f <_mb_win_mutex_close+0x4f>
100006e00: 48 83 7b 68 00              	cmpq	$0x0, 0x68(%rbx)
100006e05: 75 18                       	jne	0x100006e1f <_mb_win_mutex_close+0x4f>
100006e07: 48 8d 43 60                 	leaq	0x60(%rbx), %rax
100006e0b: 48 89 c1                    	movq	%rax, %rcx
100006e0e: 66 90                       	nop
100006e10: 48 8b 09                    	movq	(%rcx), %rcx
100006e13: 48 85 c9                    	testq	%rcx, %rcx
100006e16: 74 36                       	je	0x100006e4e <_mb_win_mutex_close+0x7e>
100006e18: 48 83 79 18 00              	cmpq	$0x0, 0x18(%rcx)
100006e1d: 74 f1                       	je	0x100006e10 <_mb_win_mutex_close+0x40>
100006e1f: 48 89 df                    	movq	%rbx, %rdi
100006e22: e8 97 18 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006e27: 89 c1                       	movl	%eax, %ecx
100006e29: b8 03 00 00 00              	movl	$0x3, %eax
100006e2e: 85 c9                       	testl	%ecx, %ecx
100006e30: 74 11                       	je	0x100006e43 <_mb_win_mutex_close+0x73>
100006e32: e8 d9 17 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100006e37: b8 01 00 00 00              	movl	$0x1, %eax
100006e3c: eb 05                       	jmp	0x100006e43 <_mb_win_mutex_close+0x73>
100006e3e: b8 02 00 00 00              	movl	$0x2, %eax
100006e43: 48 83 c4 08                 	addq	$0x8, %rsp
100006e47: 5b                          	popq	%rbx
100006e48: 41 5e                       	popq	%r14
100006e4a: 41 5f                       	popq	%r15
100006e4c: 5d                          	popq	%rbp
100006e4d: c3                          	retq
100006e4e: 48 8b 30                    	movq	(%rax), %rsi
100006e51: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
100006e60: 48 89 df                    	movq	%rbx, %rdi
100006e63: 48 85 f6                    	testq	%rsi, %rsi
100006e66: 74 0b                       	je	0x100006e73 <_mb_win_mutex_close+0xa3>
100006e68: e8 83 00 00 00              	callq	0x100006ef0 <_destroy_object>
100006e6d: 48 8b 73 60                 	movq	0x60(%rbx), %rsi
100006e71: eb ed                       	jmp	0x100006e60 <_mb_win_mutex_close+0x90>
100006e73: e8 46 18 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006e78: 85 c0                       	testl	%eax, %eax
100006e7a: 75 b6                       	jne	0x100006e32 <_mb_win_mutex_close+0x62>
100006e7c: 48 8b bb 88 00 00 00        	movq	0x88(%rbx), %rdi
100006e83: e8 1e 18 00 00              	callq	0x1000086a6 <_sysconf+0x1000086a6>
100006e88: 85 c0                       	testl	%eax, %eax
100006e8a: 75 16                       	jne	0x100006ea2 <_mb_win_mutex_close+0xd2>
100006e8c: c7 83 90 00 00 00 00 00 00 00       	movl	$0x0, 0x90(%rbx)
100006e96: 48 89 df                    	movq	%rbx, %rdi
100006e99: e8 0e 18 00 00              	callq	0x1000086ac <_sysconf+0x1000086ac>
100006e9e: 85 c0                       	testl	%eax, %eax
100006ea0: 74 11                       	je	0x100006eb3 <_mb_win_mutex_close+0xe3>
100006ea2: 89 c1                       	movl	%eax, %ecx
100006ea4: b8 04 00 00 00              	movl	$0x4, %eax
100006ea9: 4d 85 f6                    	testq	%r14, %r14
100006eac: 74 95                       	je	0x100006e43 <_mb_win_mutex_close+0x73>
100006eae: 41 89 0e                    	movl	%ecx, (%r14)
100006eb1: eb 90                       	jmp	0x100006e43 <_mb_win_mutex_close+0x73>
100006eb3: 48 8d 7b 48                 	leaq	0x48(%rbx), %rdi
100006eb7: e8 b4 b1 ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100006ebc: 85 c0                       	testl	%eax, %eax
100006ebe: 0f 85 6e ff ff ff           	jne	0x100006e32 <_mb_win_mutex_close+0x62>
100006ec4: 48 89 df                    	movq	%rbx, %rdi
100006ec7: e8 6e 17 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006ecc: 49 c7 07 00 00 00 00        	movq	$0x0, (%r15)
100006ed3: 31 c9                       	xorl	%ecx, %ecx
100006ed5: b8 00 00 00 00              	movl	$0x0, %eax
100006eda: 4d 85 f6                    	testq	%r14, %r14
100006edd: 75 cf                       	jne	0x100006eae <_mb_win_mutex_close+0xde>
100006edf: e9 5f ff ff ff              	jmp	0x100006e43 <_mb_win_mutex_close+0x73>
100006ee4: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100006ef0 <_destroy_object>:
100006ef0: 55                          	pushq	%rbp
100006ef1: 48 89 e5                    	movq	%rsp, %rbp
100006ef4: 53                          	pushq	%rbx
100006ef5: 50                          	pushq	%rax
100006ef6: 48 83 7e 48 00              	cmpq	$0x0, 0x48(%rsi)
100006efb: 75 56                       	jne	0x100006f53 <_destroy_object+0x63>
100006efd: 48 89 f3                    	movq	%rsi, %rbx
100006f00: 48 83 7e 18 00              	cmpq	$0x0, 0x18(%rsi)
100006f05: 75 4c                       	jne	0x100006f53 <_destroy_object+0x63>
100006f07: 48 8d 4f 60                 	leaq	0x60(%rdi), %rcx
100006f0b: 0f 1f 44 00 00              	nopl	(%rax,%rax)
100006f10: 48 89 c8                    	movq	%rcx, %rax
100006f13: 48 8b 09                    	movq	(%rcx), %rcx
100006f16: 48 85 c9                    	testq	%rcx, %rcx
100006f19: 74 05                       	je	0x100006f20 <_destroy_object+0x30>
100006f1b: 48 39 d9                    	cmpq	%rbx, %rcx
100006f1e: 75 f0                       	jne	0x100006f10 <_destroy_object+0x20>
100006f20: 48 85 c9                    	testq	%rcx, %rcx
100006f23: 74 2e                       	je	0x100006f53 <_destroy_object+0x63>
100006f25: 48 8b 0b                    	movq	(%rbx), %rcx
100006f28: 48 89 08                    	movq	%rcx, (%rax)
100006f2b: 48 ff 4f 70                 	decq	0x70(%rdi)
100006f2f: 48 8b 7b 20                 	movq	0x20(%rbx), %rdi
100006f33: e8 02 17 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100006f38: 48 8d 7b 58                 	leaq	0x58(%rbx), %rdi
100006f3c: e8 1d 17 00 00              	callq	0x10000865e <_sysconf+0x10000865e>
100006f41: 85 c0                       	testl	%eax, %eax
100006f43: 75 0e                       	jne	0x100006f53 <_destroy_object+0x63>
100006f45: 48 89 df                    	movq	%rbx, %rdi
100006f48: 48 83 c4 08                 	addq	$0x8, %rsp
100006f4c: 5b                          	popq	%rbx
100006f4d: 5d                          	popq	%rbp
100006f4e: e9 e7 16 00 00              	jmp	0x10000863a <_sysconf+0x10000863a>
100006f53: e8 b8 16 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100006f58: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100006f60 <_mb_win_mutex_acquire_pin>:
100006f60: 55                          	pushq	%rbp
100006f61: 48 89 e5                    	movq	%rsp, %rbp
100006f64: 41 57                       	pushq	%r15
100006f66: 41 56                       	pushq	%r14
100006f68: 53                          	pushq	%rbx
100006f69: 50                          	pushq	%rax
100006f6a: 48 85 ff                    	testq	%rdi, %rdi
100006f6d: 0f 94 c1                    	sete	%cl
100006f70: 48 85 f6                    	testq	%rsi, %rsi
100006f73: 0f 94 c2                    	sete	%dl
100006f76: b8 01 00 00 00              	movl	$0x1, %eax
100006f7b: 08 ca                       	orb	%cl, %dl
100006f7d: 75 6e                       	jne	0x100006fed <_mb_win_mutex_acquire_pin+0x8d>
100006f7f: 48 8b 1f                    	movq	(%rdi), %rbx
100006f82: b8 02 00 00 00              	movl	$0x2, %eax
100006f87: 48 85 db                    	testq	%rbx, %rbx
100006f8a: 74 61                       	je	0x100006fed <_mb_win_mutex_acquire_pin+0x8d>
100006f8c: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100006f90: 75 5b                       	jne	0x100006fed <_mb_win_mutex_acquire_pin+0x8d>
100006f92: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100006f97: 75 54                       	jne	0x100006fed <_mb_win_mutex_acquire_pin+0x8d>
100006f99: 49 89 ff                    	movq	%rdi, %r15
100006f9c: 49 89 f6                    	movq	%rsi, %r14
100006f9f: 48 89 df                    	movq	%rbx, %rdi
100006fa2: e8 11 17 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100006fa7: 85 c0                       	testl	%eax, %eax
100006fa9: 75 5e                       	jne	0x100007009 <_mb_win_mutex_acquire_pin+0xa9>
100006fab: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100006faf: 48 85 c9                    	testq	%rcx, %rcx
100006fb2: 4c 89 f0                    	movq	%r14, %rax
100006fb5: 4c 89 fa                    	movq	%r15, %rdx
100006fb8: 74 14                       	je	0x100006fce <_mb_win_mutex_acquire_pin+0x6e>
100006fba: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)
100006fc0: 48 39 c1                    	cmpq	%rax, %rcx
100006fc3: 74 33                       	je	0x100006ff8 <_mb_win_mutex_acquire_pin+0x98>
100006fc5: 48 8b 49 08                 	movq	0x8(%rcx), %rcx
100006fc9: 48 85 c9                    	testq	%rcx, %rcx
100006fcc: 75 f2                       	jne	0x100006fc0 <_mb_win_mutex_acquire_pin+0x60>
100006fce: 48 89 10                    	movq	%rdx, (%rax)
100006fd1: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100006fd5: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
100006fd9: 48 89 43 58                 	movq	%rax, 0x58(%rbx)
100006fdd: 48 89 df                    	movq	%rbx, %rdi
100006fe0: e8 d9 16 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100006fe5: 89 c1                       	movl	%eax, %ecx
100006fe7: 31 c0                       	xorl	%eax, %eax
100006fe9: 85 c9                       	testl	%ecx, %ecx
100006feb: 75 1c                       	jne	0x100007009 <_mb_win_mutex_acquire_pin+0xa9>
100006fed: 48 83 c4 08                 	addq	$0x8, %rsp
100006ff1: 5b                          	popq	%rbx
100006ff2: 41 5e                       	popq	%r14
100006ff4: 41 5f                       	popq	%r15
100006ff6: 5d                          	popq	%rbp
100006ff7: c3                          	retq
100006ff8: 48 89 df                    	movq	%rbx, %rdi
100006ffb: e8 be 16 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007000: 85 c0                       	testl	%eax, %eax
100007002: b8 02 00 00 00              	movl	$0x2, %eax
100007007: 74 e4                       	je	0x100006fed <_mb_win_mutex_acquire_pin+0x8d>
100007009: e8 02 16 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000700e: 66 90                       	nop

0000000100007010 <_mb_win_mutex_release_pin>:
100007010: 55                          	pushq	%rbp
100007011: 48 89 e5                    	movq	%rsp, %rbp
100007014: 41 56                       	pushq	%r14
100007016: 53                          	pushq	%rbx
100007017: 48 85 ff                    	testq	%rdi, %rdi
10000701a: 74 6c                       	je	0x100007088 <_mb_win_mutex_release_pin+0x78>
10000701c: 48 8b 0f                    	movq	(%rdi), %rcx
10000701f: b8 02 00 00 00              	movl	$0x2, %eax
100007024: 48 85 c9                    	testq	%rcx, %rcx
100007027: 74 64                       	je	0x10000708d <_mb_win_mutex_release_pin+0x7d>
100007029: 48 8b 19                    	movq	(%rcx), %rbx
10000702c: 48 85 db                    	testq	%rbx, %rbx
10000702f: 74 5c                       	je	0x10000708d <_mb_win_mutex_release_pin+0x7d>
100007031: 49 89 fe                    	movq	%rdi, %r14
100007034: 48 89 df                    	movq	%rbx, %rdi
100007037: e8 7c 16 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
10000703c: 85 c0                       	testl	%eax, %eax
10000703e: 75 63                       	jne	0x1000070a3 <_mb_win_mutex_release_pin+0x93>
100007040: 48 8d 73 58                 	leaq	0x58(%rbx), %rsi
100007044: 4c 89 f0                    	movq	%r14, %rax
100007047: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100007050: 48 89 f1                    	movq	%rsi, %rcx
100007053: 48 8b 16                    	movq	(%rsi), %rdx
100007056: 48 85 d2                    	testq	%rdx, %rdx
100007059: 74 09                       	je	0x100007064 <_mb_win_mutex_release_pin+0x54>
10000705b: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
10000705f: 48 39 c2                    	cmpq	%rax, %rdx
100007062: 75 ec                       	jne	0x100007050 <_mb_win_mutex_release_pin+0x40>
100007064: 48 85 d2                    	testq	%rdx, %rdx
100007067: 74 29                       	je	0x100007092 <_mb_win_mutex_release_pin+0x82>
100007069: 48 8b 50 08                 	movq	0x8(%rax), %rdx
10000706d: 48 89 11                    	movq	%rdx, (%rcx)
100007070: 0f 57 c0                    	xorps	%xmm0, %xmm0
100007073: 0f 11 00                    	movups	%xmm0, (%rax)
100007076: 48 89 df                    	movq	%rbx, %rdi
100007079: e8 40 16 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000707e: 89 c1                       	movl	%eax, %ecx
100007080: 31 c0                       	xorl	%eax, %eax
100007082: 85 c9                       	testl	%ecx, %ecx
100007084: 74 07                       	je	0x10000708d <_mb_win_mutex_release_pin+0x7d>
100007086: eb 1b                       	jmp	0x1000070a3 <_mb_win_mutex_release_pin+0x93>
100007088: b8 01 00 00 00              	movl	$0x1, %eax
10000708d: 5b                          	popq	%rbx
10000708e: 41 5e                       	popq	%r14
100007090: 5d                          	popq	%rbp
100007091: c3                          	retq
100007092: 48 89 df                    	movq	%rbx, %rdi
100007095: e8 24 16 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000709a: 85 c0                       	testl	%eax, %eax
10000709c: b8 02 00 00 00              	movl	$0x2, %eax
1000070a1: 74 ea                       	je	0x10000708d <_mb_win_mutex_release_pin+0x7d>
1000070a3: e8 68 15 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000070a8: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

00000001000070b0 <_mb_win_mutex_domain>:
1000070b0: 48 85 ff                    	testq	%rdi, %rdi
1000070b3: 74 12                       	je	0x1000070c7 <_mb_win_mutex_domain+0x17>
1000070b5: 48 8b 07                    	movq	(%rdi), %rax
1000070b8: 48 85 c0                    	testq	%rax, %rax
1000070bb: 74 0a                       	je	0x1000070c7 <_mb_win_mutex_domain+0x17>
1000070bd: 55                          	pushq	%rbp
1000070be: 48 89 e5                    	movq	%rsp, %rbp
1000070c1: 48 8b 40 40                 	movq	0x40(%rax), %rax
1000070c5: 5d                          	popq	%rbp
1000070c6: c3                          	retq
1000070c7: 31 c0                       	xorl	%eax, %eax
1000070c9: c3                          	retq
1000070ca: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

00000001000070d0 <_mb_win_mutex_create>:
1000070d0: 55                          	pushq	%rbp
1000070d1: 48 89 e5                    	movq	%rsp, %rbp
1000070d4: 41 57                       	pushq	%r15
1000070d6: 41 56                       	pushq	%r14
1000070d8: 41 55                       	pushq	%r13
1000070da: 41 54                       	pushq	%r12
1000070dc: 53                          	pushq	%rbx
1000070dd: 48 83 ec 48                 	subq	$0x48, %rsp
1000070e1: 48 8b 05 28 2f 00 00        	movq	0x2f28(%rip), %rax      ## 0x10000a010 <_sysconf+0x10000a010>
1000070e8: 48 8b 00                    	movq	(%rax), %rax
1000070eb: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
1000070ef: 48 85 ff                    	testq	%rdi, %rdi
1000070f2: 0f 84 3a 03 00 00           	je	0x100007432 <_mb_win_mutex_create+0x362>
1000070f8: 4c 8b 37                    	movq	(%rdi), %r14
1000070fb: 4d 85 f6                    	testq	%r14, %r14
1000070fe: 0f 84 2e 03 00 00           	je	0x100007432 <_mb_win_mutex_create+0x362>
100007104: 41 89 d7                    	movl	%edx, %r15d
100007107: 49 89 f4                    	movq	%rsi, %r12
10000710a: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
10000710e: e8 0d b0 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007113: 4c 89 f7                    	movq	%r14, %rdi
100007116: e8 9d 15 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
10000711b: 85 c0                       	testl	%eax, %eax
10000711d: 0f 85 0f 03 00 00           	jne	0x100007432 <_mb_win_mutex_create+0x362>
100007123: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007127: e8 f4 af ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000712c: 48 89 c3                    	movq	%rax, %rbx
10000712f: 41 83 ff 02                 	cmpl	$0x2, %r15d
100007133: 73 4f                       	jae	0x100007184 <_mb_win_mutex_create+0xb4>
100007135: 44 8b 2b                    	movl	(%rbx), %r13d
100007138: 48 c7 45 a8 00 00 00 00     	movq	$0x0, -0x58(%rbp)
100007140: 48 c7 45 98 00 00 00 00     	movq	$0x0, -0x68(%rbp)
100007148: c7 45 a4 00 00 00 00        	movl	$0x0, -0x5c(%rbp)
10000714f: 48 8d 75 a8                 	leaq	-0x58(%rbp), %rsi
100007153: 48 8d 55 98                 	leaq	-0x68(%rbp), %rdx
100007157: 48 8d 4d a4                 	leaq	-0x5c(%rbp), %rcx
10000715b: 4c 89 e7                    	movq	%r12, %rdi
10000715e: e8 2d 03 00 00              	callq	0x100007490 <_parse_name>
100007163: 85 c0                       	testl	%eax, %eax
100007165: 74 3a                       	je	0x1000071a1 <_mb_win_mutex_create+0xd1>
100007167: 41 89 c4                    	movl	%eax, %r12d
10000716a: 4c 89 f7                    	movq	%r14, %rdi
10000716d: e8 4c 15 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007172: 85 c0                       	testl	%eax, %eax
100007174: 0f 85 b8 02 00 00           	jne	0x100007432 <_mb_win_mutex_create+0x362>
10000717a: 31 c0                       	xorl	%eax, %eax
10000717c: 45 89 e5                    	movl	%r12d, %r13d
10000717f: e9 e3 01 00 00              	jmp	0x100007367 <_mb_win_mutex_create+0x297>
100007184: 4c 89 f7                    	movq	%r14, %rdi
100007187: e8 32 15 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000718c: 85 c0                       	testl	%eax, %eax
10000718e: 0f 85 9e 02 00 00           	jne	0x100007432 <_mb_win_mutex_create+0x362>
100007194: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
10000719a: 31 c0                       	xorl	%eax, %eax
10000719c: e9 c9 01 00 00              	jmp	0x10000736a <_mb_win_mutex_create+0x29a>
1000071a1: 44 8b 4d a4                 	movl	-0x5c(%rbp), %r9d
1000071a5: 45 85 c9                    	testl	%r9d, %r9d
1000071a8: 74 5f                       	je	0x100007209 <_mb_win_mutex_create+0x139>
1000071aa: 49 8b 46 70                 	movq	0x70(%r14), %rax
1000071ae: 49 3b 46 78                 	cmpq	0x78(%r14), %rax
1000071b2: 0f 84 8e 01 00 00           	je	0x100007346 <_mb_win_mutex_create+0x276>
1000071b8: 49 83 be 80 00 00 00 ff     	cmpq	$-0x1, 0x80(%r14)
1000071c0: 0f 84 80 01 00 00           	je	0x100007346 <_mb_win_mutex_create+0x276>
1000071c6: 44 89 4d b4                 	movl	%r9d, -0x4c(%rbp)
1000071ca: bf 01 00 00 00              	movl	$0x1, %edi
1000071cf: be 88 00 00 00              	movl	$0x88, %esi
1000071d4: e8 43 14 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
1000071d9: 48 85 c0                    	testq	%rax, %rax
1000071dc: 0f 84 64 01 00 00           	je	0x100007346 <_mb_win_mutex_create+0x276>
1000071e2: 48 89 45 b8                 	movq	%rax, -0x48(%rbp)
1000071e6: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
1000071ea: e8 93 14 00 00              	callq	0x100008682 <_sysconf+0x100008682>
1000071ef: 85 c0                       	testl	%eax, %eax
1000071f1: 0f 84 21 01 00 00           	je	0x100007318 <_mb_win_mutex_create+0x248>
1000071f7: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
1000071fb: e8 7c 14 00 00              	callq	0x10000867c <_sysconf+0x10000867c>
100007200: 4c 8b 65 b8                 	movq	-0x48(%rbp), %r12
100007204: e9 35 01 00 00              	jmp	0x10000733e <_mb_win_mutex_create+0x26e>
100007209: 48 8b 7d a8                 	movq	-0x58(%rbp), %rdi
10000720d: 49 8b 46 60                 	movq	0x60(%r14), %rax
100007211: 48 85 c0                    	testq	%rax, %rax
100007214: 74 53                       	je	0x100007269 <_mb_win_mutex_create+0x199>
100007216: 48 8b 4d 98                 	movq	-0x68(%rbp), %rcx
10000721a: eb 13                       	jmp	0x10000722f <_mb_win_mutex_create+0x15f>
10000721c: 31 d2                       	xorl	%edx, %edx
10000721e: 48 39 ca                    	cmpq	%rcx, %rdx
100007221: 4c 0f 44 e0                 	cmoveq	%rax, %r12
100007225: 74 45                       	je	0x10000726c <_mb_win_mutex_create+0x19c>
100007227: 48 8b 00                    	movq	(%rax), %rax
10000722a: 48 85 c0                    	testq	%rax, %rax
10000722d: 74 3a                       	je	0x100007269 <_mb_win_mutex_create+0x199>
10000722f: 83 78 30 00                 	cmpl	$0x0, 0x30(%rax)
100007233: 74 f2                       	je	0x100007227 <_mb_win_mutex_create+0x157>
100007235: 48 39 48 28                 	cmpq	%rcx, 0x28(%rax)
100007239: 75 ec                       	jne	0x100007227 <_mb_win_mutex_create+0x157>
10000723b: 48 85 c9                    	testq	%rcx, %rcx
10000723e: 74 dc                       	je	0x10000721c <_mb_win_mutex_create+0x14c>
100007240: 48 8b 70 20                 	movq	0x20(%rax), %rsi
100007244: 31 d2                       	xorl	%edx, %edx
100007246: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100007250: 44 0f b7 04 56              	movzwl	(%rsi,%rdx,2), %r8d
100007255: 66 44 3b 04 57              	cmpw	(%rdi,%rdx,2), %r8w
10000725a: 75 c2                       	jne	0x10000721e <_mb_win_mutex_create+0x14e>
10000725c: 48 ff c2                    	incq	%rdx
10000725f: 48 39 d1                    	cmpq	%rdx, %rcx
100007262: 75 ec                       	jne	0x100007250 <_mb_win_mutex_create+0x180>
100007264: 48 89 ca                    	movq	%rcx, %rdx
100007267: eb b5                       	jmp	0x10000721e <_mb_win_mutex_create+0x14e>
100007269: 45 31 e4                    	xorl	%r12d, %r12d
10000726c: 4d 85 e4                    	testq	%r12, %r12
10000726f: 0f 84 82 00 00 00           	je	0x1000072f7 <_mb_win_mutex_create+0x227>
100007275: 44 89 4d b4                 	movl	%r9d, -0x4c(%rbp)
100007279: e8 bc 13 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
10000727e: 49 8b 86 80 00 00 00        	movq	0x80(%r14), %rax
100007285: 48 83 f8 ff                 	cmpq	$-0x1, %rax
100007289: 74 3f                       	je	0x1000072ca <_mb_win_mutex_create+0x1fa>
10000728b: 48 89 45 b8                 	movq	%rax, -0x48(%rbp)
10000728f: bf 18 00 00 00              	movl	$0x18, %edi
100007294: e8 ad 13 00 00              	callq	0x100008646 <_sysconf+0x100008646>
100007299: 48 85 c0                    	testq	%rax, %rax
10000729c: 74 2c                       	je	0x1000072ca <_mb_win_mutex_create+0x1fa>
10000729e: 48 8b 55 b8                 	movq	-0x48(%rbp), %rdx
1000072a2: 48 ff c2                    	incq	%rdx
1000072a5: 49 89 96 80 00 00 00        	movq	%rdx, 0x80(%r14)
1000072ac: 48 89 55 b8                 	movq	%rdx, -0x48(%rbp)
1000072b0: 48 89 50 08                 	movq	%rdx, 0x8(%rax)
1000072b4: 4c 89 60 10                 	movq	%r12, 0x10(%rax)
1000072b8: 49 8b 4e 68                 	movq	0x68(%r14), %rcx
1000072bc: 48 89 08                    	movq	%rcx, (%rax)
1000072bf: 49 89 46 68                 	movq	%rax, 0x68(%r14)
1000072c3: 49 ff 44 24 10              	incq	0x10(%r12)
1000072c8: eb 08                       	jmp	0x1000072d2 <_mb_win_mutex_create+0x202>
1000072ca: 48 c7 45 b8 00 00 00 00     	movq	$0x0, -0x48(%rbp)
1000072d2: 4c 89 f7                    	movq	%r14, %rdi
1000072d5: e8 e4 13 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000072da: 89 c1                       	movl	%eax, %ecx
1000072dc: 48 8b 45 b8                 	movq	-0x48(%rbp), %rax
1000072e0: 48 85 c0                    	testq	%rax, %rax
1000072e3: 74 14                       	je	0x1000072f9 <_mb_win_mutex_create+0x229>
1000072e5: ba b7 00 00 00              	movl	$0xb7, %edx
1000072ea: 85 c9                       	testl	%ecx, %ecx
1000072ec: 44 8b 4d b4                 	movl	-0x4c(%rbp), %r9d
1000072f0: 74 1a                       	je	0x10000730c <_mb_win_mutex_create+0x23c>
1000072f2: e9 3b 01 00 00              	jmp	0x100007432 <_mb_win_mutex_create+0x362>
1000072f7: eb 15                       	jmp	0x10000730e <_mb_win_mutex_create+0x23e>
1000072f9: 85 c9                       	testl	%ecx, %ecx
1000072fb: 44 8b 4d b4                 	movl	-0x4c(%rbp), %r9d
1000072ff: 0f 85 2d 01 00 00           	jne	0x100007432 <_mb_win_mutex_create+0x362>
100007305: ba 08 00 00 00              	movl	$0x8, %edx
10000730a: 31 c0                       	xorl	%eax, %eax
10000730c: 89 13                       	movl	%edx, (%rbx)
10000730e: 4d 85 e4                    	testq	%r12, %r12
100007311: 75 57                       	jne	0x10000736a <_mb_win_mutex_create+0x29a>
100007313: e9 92 fe ff ff              	jmp	0x1000071aa <_mb_win_mutex_create+0xda>
100007318: 4c 8b 65 b8                 	movq	-0x48(%rbp), %r12
10000731c: 4c 89 e7                    	movq	%r12, %rdi
10000731f: 48 83 c7 58                 	addq	$0x58, %rdi
100007323: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
100007327: e8 38 13 00 00              	callq	0x100008664 <_sysconf+0x100008664>
10000732c: 89 45 94                    	movl	%eax, -0x6c(%rbp)
10000732f: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
100007333: e8 44 13 00 00              	callq	0x10000867c <_sysconf+0x10000867c>
100007338: 83 7d 94 00                 	cmpl	$0x0, -0x6c(%rbp)
10000733c: 74 4f                       	je	0x10000738d <_mb_win_mutex_create+0x2bd>
10000733e: 4c 89 e7                    	movq	%r12, %rdi
100007341: e8 f4 12 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100007346: 48 8b 7d a8                 	movq	-0x58(%rbp), %rdi
10000734a: e8 eb 12 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
10000734f: 4c 89 f7                    	movq	%r14, %rdi
100007352: e8 67 13 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007357: 85 c0                       	testl	%eax, %eax
100007359: 0f 85 d3 00 00 00           	jne	0x100007432 <_mb_win_mutex_create+0x362>
10000735f: 41 bd 08 00 00 00           	movl	$0x8, %r13d
100007365: 31 c0                       	xorl	%eax, %eax
100007367: 44 89 2b                    	movl	%r13d, (%rbx)
10000736a: 48 8b 0d 9f 2c 00 00        	movq	0x2c9f(%rip), %rcx      ## 0x10000a010 <_sysconf+0x10000a010>
100007371: 48 8b 09                    	movq	(%rcx), %rcx
100007374: 48 3b 4d d0                 	cmpq	-0x30(%rbp), %rcx
100007378: 0f 85 07 01 00 00           	jne	0x100007485 <_mb_win_mutex_create+0x3b5>
10000737e: 48 83 c4 48                 	addq	$0x48, %rsp
100007382: 5b                          	popq	%rbx
100007383: 41 5c                       	popq	%r12
100007385: 41 5d                       	popq	%r13
100007387: 41 5e                       	popq	%r14
100007389: 41 5f                       	popq	%r15
10000738b: 5d                          	popq	%rbp
10000738c: c3                          	retq
10000738d: 49 8b 86 80 00 00 00        	movq	0x80(%r14), %rax
100007394: 48 ff c0                    	incq	%rax
100007397: 49 89 86 80 00 00 00        	movq	%rax, 0x80(%r14)
10000739e: 49 89 44 24 08              	movq	%rax, 0x8(%r12)
1000073a3: 83 7d b4 00                 	cmpl	$0x0, -0x4c(%rbp)
1000073a7: 74 0f                       	je	0x1000073b8 <_mb_win_mutex_create+0x2e8>
1000073a9: 48 8b 7d a8                 	movq	-0x58(%rbp), %rdi
1000073ad: e8 88 12 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
1000073b2: 4c 8b 65 b8                 	movq	-0x48(%rbp), %r12
1000073b6: eb 1b                       	jmp	0x1000073d3 <_mb_win_mutex_create+0x303>
1000073b8: 41 c7 44 24 30 01 00 00 00  	movl	$0x1, 0x30(%r12)
1000073c1: 48 8b 45 a8                 	movq	-0x58(%rbp), %rax
1000073c5: 49 89 44 24 20              	movq	%rax, 0x20(%r12)
1000073ca: 48 8b 45 98                 	movq	-0x68(%rbp), %rax
1000073ce: 49 89 44 24 28              	movq	%rax, 0x28(%r12)
1000073d3: 49 8b 46 60                 	movq	0x60(%r14), %rax
1000073d7: 49 89 04 24                 	movq	%rax, (%r12)
1000073db: 4d 89 66 60                 	movq	%r12, 0x60(%r14)
1000073df: 49 ff 46 70                 	incq	0x70(%r14)
1000073e3: 4c 89 f7                    	movq	%r14, %rdi
1000073e6: 4c 89 e6                    	movq	%r12, %rsi
1000073e9: e8 52 02 00 00              	callq	0x100007640 <_new_handle>
1000073ee: 48 85 c0                    	testq	%rax, %rax
1000073f1: 74 44                       	je	0x100007437 <_mb_win_mutex_create+0x367>
1000073f3: 49 89 c4                    	movq	%rax, %r12
1000073f6: 45 85 ff                    	testl	%r15d, %r15d
1000073f9: 74 24                       	je	0x10000741f <_mb_win_mutex_create+0x34f>
1000073fb: 4c 8b 7d b8                 	movq	-0x48(%rbp), %r15
1000073ff: 41 c7 47 34 01 00 00 00     	movl	$0x1, 0x34(%r15)
100007407: e8 b8 12 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
10000740c: 49 89 47 40                 	movq	%rax, 0x40(%r15)
100007410: 4c 89 f7                    	movq	%r14, %rdi
100007413: 4c 89 fe                    	movq	%r15, %rsi
100007416: e8 85 02 00 00              	callq	0x1000076a0 <_adopt>
10000741b: 85 c0                       	testl	%eax, %eax
10000741d: 74 28                       	je	0x100007447 <_mb_win_mutex_create+0x377>
10000741f: 4c 89 f7                    	movq	%r14, %rdi
100007422: e8 97 12 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007427: 85 c0                       	testl	%eax, %eax
100007429: 4c 89 e0                    	movq	%r12, %rax
10000742c: 0f 84 35 ff ff ff           	je	0x100007367 <_mb_win_mutex_create+0x297>
100007432: e8 d9 11 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100007437: 4c 89 f7                    	movq	%r14, %rdi
10000743a: 4c 89 e6                    	movq	%r12, %rsi
10000743d: e8 ae fa ff ff              	callq	0x100006ef0 <_destroy_object>
100007442: e9 08 ff ff ff              	jmp	0x10000734f <_mb_win_mutex_create+0x27f>
100007447: 48 8b 45 b8                 	movq	-0x48(%rbp), %rax
10000744b: c7 40 34 00 00 00 00        	movl	$0x0, 0x34(%rax)
100007452: 49 8d 7e 68                 	leaq	0x68(%r14), %rdi
100007456: 48 8b 3f                    	movq	(%rdi), %rdi
100007459: 48 85 ff                    	testq	%rdi, %rdi
10000745c: 74 06                       	je	0x100007464 <_mb_win_mutex_create+0x394>
10000745e: 4c 39 67 08                 	cmpq	%r12, 0x8(%rdi)
100007462: 75 f2                       	jne	0x100007456 <_mb_win_mutex_create+0x386>
100007464: 48 8b 07                    	movq	(%rdi), %rax
100007467: 49 89 46 68                 	movq	%rax, 0x68(%r14)
10000746b: e8 ca 11 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100007470: 48 8b 75 b8                 	movq	-0x48(%rbp), %rsi
100007474: 48 ff 4e 10                 	decq	0x10(%rsi)
100007478: 4c 89 f7                    	movq	%r14, %rdi
10000747b: e8 70 fa ff ff              	callq	0x100006ef0 <_destroy_object>
100007480: e9 ca fe ff ff              	jmp	0x10000734f <_mb_win_mutex_create+0x27f>
100007485: e8 7a 11 00 00              	callq	0x100008604 <_sysconf+0x100008604>
10000748a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100007490 <_parse_name>:
100007490: 48 c7 06 00 00 00 00        	movq	$0x0, (%rsi)
100007497: 48 c7 02 00 00 00 00        	movq	$0x0, (%rdx)
10000749e: 48 85 ff                    	testq	%rdi, %rdi
1000074a1: 0f 84 ee 00 00 00           	je	0x100007595 <_parse_name+0x105>
1000074a7: 66 83 3f 00                 	cmpw	$0x0, (%rdi)
1000074ab: 0f 84 e4 00 00 00           	je	0x100007595 <_parse_name+0x105>
1000074b1: 55                          	pushq	%rbp
1000074b2: 48 89 e5                    	movq	%rsp, %rbp
1000074b5: 41 57                       	pushq	%r15
1000074b7: 41 56                       	pushq	%r14
1000074b9: 41 55                       	pushq	%r13
1000074bb: 41 54                       	pushq	%r12
1000074bd: 53                          	pushq	%rbx
1000074be: 48 83 ec 18                 	subq	$0x18, %rsp
1000074c2: c7 01 00 00 00 00           	movl	$0x0, (%rcx)
1000074c8: 48 c7 c0 ff ff ff ff        	movq	$-0x1, %rax
1000074cf: bb 02 00 00 00              	movl	$0x2, %ebx
1000074d4: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
1000074e0: 4c 8d 78 01                 	leaq	0x1(%rax), %r15
1000074e4: 48 83 c3 fe                 	addq	$-0x2, %rbx
1000074e8: 49 81 ff 04 01 00 00        	cmpq	$0x104, %r15            ## imm = 0x104
1000074ef: 77 0b                       	ja	0x1000074fc <_parse_name+0x6c>
1000074f1: 66 83 7c 47 02 00           	cmpw	$0x0, 0x2(%rdi,%rax,2)
1000074f7: 4c 89 f8                    	movq	%r15, %rax
1000074fa: 75 e4                       	jne	0x1000074e0 <_parse_name+0x50>
1000074fc: b8 ce 00 00 00              	movl	$0xce, %eax
100007501: 49 81 ff 04 01 00 00        	cmpq	$0x104, %r15            ## imm = 0x104
100007508: 0f 87 18 01 00 00           	ja	0x100007626 <_parse_name+0x196>
10000750e: 49 89 fd                    	movq	%rdi, %r13
100007511: 48 89 75 d0                 	movq	%rsi, -0x30(%rbp)
100007515: 49 89 d4                    	movq	%rdx, %r12
100007518: 48 89 4d c8                 	movq	%rcx, -0x38(%rbp)
10000751c: 48 89 df                    	movq	%rbx, %rdi
10000751f: 48 f7 df                    	negq	%rdi
100007522: e8 1f 11 00 00              	callq	0x100008646 <_sysconf+0x100008646>
100007527: 48 85 c0                    	testq	%rax, %rax
10000752a: 74 72                       	je	0x10000759e <_parse_name+0x10e>
10000752c: 49 89 c6                    	movq	%rax, %r14
10000752f: 4d 85 ff                    	testq	%r15, %r15
100007532: 74 2c                       	je	0x100007560 <_parse_name+0xd0>
100007534: 31 c9                       	xorl	%ecx, %ecx
100007536: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100007540: 41 0f b7 54 4d 00           	movzwl	(%r13,%rcx,2), %edx
100007546: 8d 72 bf                    	leal	-0x41(%rdx), %esi
100007549: 8d 7a 20                    	leal	0x20(%rdx), %edi
10000754c: 66 83 fe 1a                 	cmpw	$0x1a, %si
100007550: 0f 43 fa                    	cmovael	%edx, %edi
100007553: 66 41 89 3c 4e              	movw	%di, (%r14,%rcx,2)
100007558: 48 ff c1                    	incq	%rcx
10000755b: 49 39 cf                    	cmpq	%rcx, %r15
10000755e: 75 e0                       	jne	0x100007540 <_parse_name+0xb0>
100007560: 49 83 ff 07                 	cmpq	$0x7, %r15
100007564: 72 42                       	jb	0x1000075a8 <_parse_name+0x118>
100007566: 48 8d 0d 15 1c 00 00        	leaq	0x1c15(%rip), %rcx      ## 0x100009182 <_parse_name.prefix>
10000756d: 31 c0                       	xorl	%eax, %eax
10000756f: 90                          	nop
100007570: 41 0f b7 14 46              	movzwl	(%r14,%rax,2), %edx
100007575: 66 3b 11                    	cmpw	(%rcx), %dx
100007578: 75 12                       	jne	0x10000758c <_parse_name+0xfc>
10000757a: 48 ff c0                    	incq	%rax
10000757d: 48 83 c1 02                 	addq	$0x2, %rcx
100007581: 48 83 f8 06                 	cmpq	$0x6, %rax
100007585: 75 e9                       	jne	0x100007570 <_parse_name+0xe0>
100007587: b8 06 00 00 00              	movl	$0x6, %eax
10000758c: 48 83 f8 06                 	cmpq	$0x6, %rax
100007590: 0f 94 c0                    	sete	%al
100007593: eb 15                       	jmp	0x1000075aa <_parse_name+0x11a>
100007595: c7 01 01 00 00 00           	movl	$0x1, (%rcx)
10000759b: 31 c0                       	xorl	%eax, %eax
10000759d: c3                          	retq
10000759e: b8 08 00 00 00              	movl	$0x8, %eax
1000075a3: e9 7e 00 00 00              	jmp	0x100007626 <_parse_name+0x196>
1000075a8: 31 c0                       	xorl	%eax, %eax
1000075aa: 31 d2                       	xorl	%edx, %edx
1000075ac: 84 c0                       	testb	%al, %al
1000075ae: b9 06 00 00 00              	movl	$0x6, %ecx
1000075b3: 48 0f 44 ca                 	cmoveq	%rdx, %rcx
1000075b7: 4c 39 f9                    	cmpq	%r15, %rcx
1000075ba: 73 14                       	jae	0x1000075d0 <_parse_name+0x140>
1000075bc: 48 89 ca                    	movq	%rcx, %rdx
1000075bf: 90                          	nop
1000075c0: 66 41 83 3c 56 5c           	cmpw	$0x5c, (%r14,%rdx,2)
1000075c6: 74 51                       	je	0x100007619 <_parse_name+0x189>
1000075c8: 48 ff c2                    	incq	%rdx
1000075cb: 4c 39 fa                    	cmpq	%r15, %rdx
1000075ce: 72 f0                       	jb	0x1000075c0 <_parse_name+0x130>
1000075d0: 84 c0                       	testb	%al, %al
1000075d2: 74 1d                       	je	0x1000075f1 <_parse_name+0x161>
1000075d4: 8d 04 09                    	leal	(%rcx,%rcx), %eax
1000075d7: 4c 89 f6                    	movq	%r14, %rsi
1000075da: 48 01 c6                    	addq	%rax, %rsi
1000075dd: 49 29 cf                    	subq	%rcx, %r15
1000075e0: 48 01 c3                    	addq	%rax, %rbx
1000075e3: 48 f7 db                    	negq	%rbx
1000075e6: 4c 89 f7                    	movq	%r14, %rdi
1000075e9: 48 89 da                    	movq	%rbx, %rdx
1000075ec: e8 5b 10 00 00              	callq	0x10000864c <_sysconf+0x10000864c>
1000075f1: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
1000075f5: 4d 85 ff                    	testq	%r15, %r15
1000075f8: 74 09                       	je	0x100007603 <_parse_name+0x173>
1000075fa: 4c 89 30                    	movq	%r14, (%rax)
1000075fd: 4d 89 3c 24                 	movq	%r15, (%r12)
100007601: eb 12                       	jmp	0x100007615 <_parse_name+0x185>
100007603: 4c 89 f7                    	movq	%r14, %rdi
100007606: e8 2f 10 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
10000760b: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
10000760f: c7 00 01 00 00 00           	movl	$0x1, (%rax)
100007615: 31 c0                       	xorl	%eax, %eax
100007617: eb 0d                       	jmp	0x100007626 <_parse_name+0x196>
100007619: 4c 89 f7                    	movq	%r14, %rdi
10000761c: e8 19 10 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100007621: b8 57 00 00 00              	movl	$0x57, %eax
100007626: 48 83 c4 18                 	addq	$0x18, %rsp
10000762a: 5b                          	popq	%rbx
10000762b: 41 5c                       	popq	%r12
10000762d: 41 5d                       	popq	%r13
10000762f: 41 5e                       	popq	%r14
100007631: 41 5f                       	popq	%r15
100007633: 5d                          	popq	%rbp
100007634: c3                          	retq
100007635: 66 66 2e 0f 1f 84 00 00 00 00 00    	nopw	%cs:(%rax,%rax)

0000000100007640 <_new_handle>:
100007640: 55                          	pushq	%rbp
100007641: 48 89 e5                    	movq	%rsp, %rbp
100007644: 41 57                       	pushq	%r15
100007646: 41 56                       	pushq	%r14
100007648: 53                          	pushq	%rbx
100007649: 50                          	pushq	%rax
10000764a: 48 8b 9f 80 00 00 00        	movq	0x80(%rdi), %rbx
100007651: 48 83 fb ff                 	cmpq	$-0x1, %rbx
100007655: 74 38                       	je	0x10000768f <_new_handle+0x4f>
100007657: 49 89 f6                    	movq	%rsi, %r14
10000765a: 49 89 ff                    	movq	%rdi, %r15
10000765d: bf 18 00 00 00              	movl	$0x18, %edi
100007662: e8 df 0f 00 00              	callq	0x100008646 <_sysconf+0x100008646>
100007667: 48 85 c0                    	testq	%rax, %rax
10000766a: 74 23                       	je	0x10000768f <_new_handle+0x4f>
10000766c: 48 ff c3                    	incq	%rbx
10000766f: 49 89 9f 80 00 00 00        	movq	%rbx, 0x80(%r15)
100007676: 48 89 58 08                 	movq	%rbx, 0x8(%rax)
10000767a: 4c 89 70 10                 	movq	%r14, 0x10(%rax)
10000767e: 49 8b 4f 68                 	movq	0x68(%r15), %rcx
100007682: 48 89 08                    	movq	%rcx, (%rax)
100007685: 49 89 47 68                 	movq	%rax, 0x68(%r15)
100007689: 49 ff 46 10                 	incq	0x10(%r14)
10000768d: eb 02                       	jmp	0x100007691 <_new_handle+0x51>
10000768f: 31 db                       	xorl	%ebx, %ebx
100007691: 48 89 d8                    	movq	%rbx, %rax
100007694: 48 83 c4 08                 	addq	$0x8, %rsp
100007698: 5b                          	popq	%rbx
100007699: 41 5e                       	popq	%r14
10000769b: 41 5f                       	popq	%r15
10000769d: 5d                          	popq	%rbp
10000769e: c3                          	retq
10000769f: 90                          	nop

00000001000076a0 <_adopt>:
1000076a0: 55                          	pushq	%rbp
1000076a1: 48 89 e5                    	movq	%rsp, %rbp
1000076a4: 41 57                       	pushq	%r15
1000076a6: 41 56                       	pushq	%r14
1000076a8: 53                          	pushq	%rbx
1000076a9: 50                          	pushq	%rax
1000076aa: 48 89 f3                    	movq	%rsi, %rbx
1000076ad: 49 89 ff                    	movq	%rdi, %r15
1000076b0: 48 8b bf 88 00 00 00        	movq	0x88(%rdi), %rdi
1000076b7: e8 d8 0f 00 00              	callq	0x100008694 <_sysconf+0x100008694>
1000076bc: 49 89 c6                    	movq	%rax, %r14
1000076bf: 48 85 c0                    	testq	%rax, %rax
1000076c2: 74 29                       	je	0x1000076ed <_adopt+0x4d>
1000076c4: bf 10 00 00 00              	movl	$0x10, %edi
1000076c9: e8 78 0f 00 00              	callq	0x100008646 <_sysconf+0x100008646>
1000076ce: 48 85 c0                    	testq	%rax, %rax
1000076d1: 74 4e                       	je	0x100007721 <_adopt+0x81>
1000076d3: 48 8b 4b 08                 	movq	0x8(%rbx), %rcx
1000076d7: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
1000076db: 49 8b 4e 08                 	movq	0x8(%r14), %rcx
1000076df: 48 89 08                    	movq	%rcx, (%rax)
1000076e2: 49 89 46 08                 	movq	%rax, 0x8(%r14)
1000076e6: b8 01 00 00 00              	movl	$0x1, %eax
1000076eb: eb 36                       	jmp	0x100007723 <_adopt+0x83>
1000076ed: bf 01 00 00 00              	movl	$0x1, %edi
1000076f2: be 10 00 00 00              	movl	$0x10, %esi
1000076f7: e8 20 0f 00 00              	callq	0x10000861c <_sysconf+0x10000861c>
1000076fc: 49 89 c6                    	movq	%rax, %r14
1000076ff: 31 c0                       	xorl	%eax, %eax
100007701: 4d 85 f6                    	testq	%r14, %r14
100007704: 74 1d                       	je	0x100007723 <_adopt+0x83>
100007706: 49 8b bf 88 00 00 00        	movq	0x88(%r15), %rdi
10000770d: 4c 89 f6                    	movq	%r14, %rsi
100007710: e8 b5 0f 00 00              	callq	0x1000086ca <_sysconf+0x1000086ca>
100007715: 85 c0                       	testl	%eax, %eax
100007717: 74 15                       	je	0x10000772e <_adopt+0x8e>
100007719: 4c 89 f7                    	movq	%r14, %rdi
10000771c: e8 19 0f 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100007721: 31 c0                       	xorl	%eax, %eax
100007723: 48 83 c4 08                 	addq	$0x8, %rsp
100007727: 5b                          	popq	%rbx
100007728: 41 5e                       	popq	%r14
10000772a: 41 5f                       	popq	%r15
10000772c: 5d                          	popq	%rbp
10000772d: c3                          	retq
10000772e: 4d 89 3e                    	movq	%r15, (%r14)
100007731: eb 91                       	jmp	0x1000076c4 <_adopt+0x24>
100007733: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)

0000000100007740 <_mb_win_mutex_open_named>:
100007740: 55                          	pushq	%rbp
100007741: 48 89 e5                    	movq	%rsp, %rbp
100007744: 41 57                       	pushq	%r15
100007746: 41 56                       	pushq	%r14
100007748: 41 55                       	pushq	%r13
10000774a: 41 54                       	pushq	%r12
10000774c: 53                          	pushq	%rbx
10000774d: 48 83 ec 18                 	subq	$0x18, %rsp
100007751: 48 85 ff                    	testq	%rdi, %rdi
100007754: 0f 84 87 01 00 00           	je	0x1000078e1 <_mb_win_mutex_open_named+0x1a1>
10000775a: 4c 8b 37                    	movq	(%rdi), %r14
10000775d: 4d 85 f6                    	testq	%r14, %r14
100007760: 0f 84 7b 01 00 00           	je	0x1000078e1 <_mb_win_mutex_open_named+0x1a1>
100007766: 49 89 f7                    	movq	%rsi, %r15
100007769: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
10000776d: e8 ae a9 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007772: 4c 89 f7                    	movq	%r14, %rdi
100007775: e8 3e 0f 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
10000777a: 85 c0                       	testl	%eax, %eax
10000777c: 0f 85 5f 01 00 00           	jne	0x1000078e1 <_mb_win_mutex_open_named+0x1a1>
100007782: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007786: e8 95 a9 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000778b: 48 89 c3                    	movq	%rax, %rbx
10000778e: 44 8b 20                    	movl	(%rax), %r12d
100007791: 48 c7 45 c0 00 00 00 00     	movq	$0x0, -0x40(%rbp)
100007799: 48 c7 45 c8 00 00 00 00     	movq	$0x0, -0x38(%rbp)
1000077a1: c7 45 d4 00 00 00 00        	movl	$0x0, -0x2c(%rbp)
1000077a8: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
1000077ac: 48 8d 55 c8                 	leaq	-0x38(%rbp), %rdx
1000077b0: 48 8d 4d d4                 	leaq	-0x2c(%rbp), %rcx
1000077b4: 4c 89 ff                    	movq	%r15, %rdi
1000077b7: e8 d4 fc ff ff              	callq	0x100007490 <_parse_name>
1000077bc: 41 89 c7                    	movl	%eax, %r15d
1000077bf: 48 8b 7d c0                 	movq	-0x40(%rbp), %rdi
1000077c3: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
1000077c6: 44 09 f8                    	orl	%r15d, %eax
1000077c9: 74 27                       	je	0x1000077f2 <_mb_win_mutex_open_named+0xb2>
1000077cb: e8 6a 0e 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
1000077d0: 4c 89 f7                    	movq	%r14, %rdi
1000077d3: e8 e6 0e 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000077d8: 85 c0                       	testl	%eax, %eax
1000077da: 0f 85 01 01 00 00           	jne	0x1000078e1 <_mb_win_mutex_open_named+0x1a1>
1000077e0: 45 85 ff                    	testl	%r15d, %r15d
1000077e3: 41 bc 57 00 00 00           	movl	$0x57, %r12d
1000077e9: 45 0f 45 e7                 	cmovnel	%r15d, %r12d
1000077ed: e9 d7 00 00 00              	jmp	0x1000078c9 <_mb_win_mutex_open_named+0x189>
1000077f2: 49 8b 46 60                 	movq	0x60(%r14), %rax
1000077f6: 48 85 c0                    	testq	%rax, %rax
1000077f9: 74 4e                       	je	0x100007849 <_mb_win_mutex_open_named+0x109>
1000077fb: 48 8b 4d c8                 	movq	-0x38(%rbp), %rcx
1000077ff: eb 13                       	jmp	0x100007814 <_mb_win_mutex_open_named+0xd4>
100007801: 31 d2                       	xorl	%edx, %edx
100007803: 48 39 ca                    	cmpq	%rcx, %rdx
100007806: 4c 0f 44 e8                 	cmoveq	%rax, %r13
10000780a: 74 40                       	je	0x10000784c <_mb_win_mutex_open_named+0x10c>
10000780c: 48 8b 00                    	movq	(%rax), %rax
10000780f: 48 85 c0                    	testq	%rax, %rax
100007812: 74 35                       	je	0x100007849 <_mb_win_mutex_open_named+0x109>
100007814: 83 78 30 00                 	cmpl	$0x0, 0x30(%rax)
100007818: 74 f2                       	je	0x10000780c <_mb_win_mutex_open_named+0xcc>
10000781a: 48 39 48 28                 	cmpq	%rcx, 0x28(%rax)
10000781e: 75 ec                       	jne	0x10000780c <_mb_win_mutex_open_named+0xcc>
100007820: 48 85 c9                    	testq	%rcx, %rcx
100007823: 74 dc                       	je	0x100007801 <_mb_win_mutex_open_named+0xc1>
100007825: 48 8b 70 20                 	movq	0x20(%rax), %rsi
100007829: 31 d2                       	xorl	%edx, %edx
10000782b: 0f 1f 44 00 00              	nopl	(%rax,%rax)
100007830: 44 0f b7 04 56              	movzwl	(%rsi,%rdx,2), %r8d
100007835: 66 44 3b 04 57              	cmpw	(%rdi,%rdx,2), %r8w
10000783a: 75 c7                       	jne	0x100007803 <_mb_win_mutex_open_named+0xc3>
10000783c: 48 ff c2                    	incq	%rdx
10000783f: 48 39 d1                    	cmpq	%rdx, %rcx
100007842: 75 ec                       	jne	0x100007830 <_mb_win_mutex_open_named+0xf0>
100007844: 48 89 ca                    	movq	%rcx, %rdx
100007847: eb ba                       	jmp	0x100007803 <_mb_win_mutex_open_named+0xc3>
100007849: 45 31 ed                    	xorl	%r13d, %r13d
10000784c: e8 e9 0d 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100007851: 4d 85 ed                    	testq	%r13, %r13
100007854: 74 55                       	je	0x1000078ab <_mb_win_mutex_open_named+0x16b>
100007856: 4d 8b be 80 00 00 00        	movq	0x80(%r14), %r15
10000785d: 49 83 ff ff                 	cmpq	$-0x1, %r15
100007861: 74 32                       	je	0x100007895 <_mb_win_mutex_open_named+0x155>
100007863: bf 18 00 00 00              	movl	$0x18, %edi
100007868: e8 d9 0d 00 00              	callq	0x100008646 <_sysconf+0x100008646>
10000786d: 48 85 c0                    	testq	%rax, %rax
100007870: 74 23                       	je	0x100007895 <_mb_win_mutex_open_named+0x155>
100007872: 49 ff c7                    	incq	%r15
100007875: 4d 89 be 80 00 00 00        	movq	%r15, 0x80(%r14)
10000787c: 4c 89 78 08                 	movq	%r15, 0x8(%rax)
100007880: 4c 89 68 10                 	movq	%r13, 0x10(%rax)
100007884: 49 8b 4e 68                 	movq	0x68(%r14), %rcx
100007888: 48 89 08                    	movq	%rcx, (%rax)
10000788b: 49 89 46 68                 	movq	%rax, 0x68(%r14)
10000788f: 49 ff 45 10                 	incq	0x10(%r13)
100007893: eb 03                       	jmp	0x100007898 <_mb_win_mutex_open_named+0x158>
100007895: 45 31 ff                    	xorl	%r15d, %r15d
100007898: 4c 89 f7                    	movq	%r14, %rdi
10000789b: e8 1e 0e 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000078a0: 4d 85 ff                    	testq	%r15, %r15
1000078a3: 74 1a                       	je	0x1000078bf <_mb_win_mutex_open_named+0x17f>
1000078a5: 85 c0                       	testl	%eax, %eax
1000078a7: 74 23                       	je	0x1000078cc <_mb_win_mutex_open_named+0x18c>
1000078a9: eb 36                       	jmp	0x1000078e1 <_mb_win_mutex_open_named+0x1a1>
1000078ab: 4c 89 f7                    	movq	%r14, %rdi
1000078ae: e8 0b 0e 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000078b3: 85 c0                       	testl	%eax, %eax
1000078b5: 75 2a                       	jne	0x1000078e1 <_mb_win_mutex_open_named+0x1a1>
1000078b7: 41 bc 02 00 00 00           	movl	$0x2, %r12d
1000078bd: eb 0a                       	jmp	0x1000078c9 <_mb_win_mutex_open_named+0x189>
1000078bf: 85 c0                       	testl	%eax, %eax
1000078c1: 75 1e                       	jne	0x1000078e1 <_mb_win_mutex_open_named+0x1a1>
1000078c3: 41 bc 08 00 00 00           	movl	$0x8, %r12d
1000078c9: 45 31 ff                    	xorl	%r15d, %r15d
1000078cc: 44 89 23                    	movl	%r12d, (%rbx)
1000078cf: 4c 89 f8                    	movq	%r15, %rax
1000078d2: 48 83 c4 18                 	addq	$0x18, %rsp
1000078d6: 5b                          	popq	%rbx
1000078d7: 41 5c                       	popq	%r12
1000078d9: 41 5d                       	popq	%r13
1000078db: 41 5e                       	popq	%r14
1000078dd: 41 5f                       	popq	%r15
1000078df: 5d                          	popq	%rbp
1000078e0: c3                          	retq
1000078e1: e8 2a 0d 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000078e6: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)

00000001000078f0 <_mb_win_mutex_release>:
1000078f0: 55                          	pushq	%rbp
1000078f1: 48 89 e5                    	movq	%rsp, %rbp
1000078f4: 41 57                       	pushq	%r15
1000078f6: 41 56                       	pushq	%r14
1000078f8: 41 55                       	pushq	%r13
1000078fa: 41 54                       	pushq	%r12
1000078fc: 53                          	pushq	%rbx
1000078fd: 50                          	pushq	%rax
1000078fe: 48 85 ff                    	testq	%rdi, %rdi
100007901: 0f 84 1f 01 00 00           	je	0x100007a26 <_mb_win_mutex_release+0x136>
100007907: 4c 8b 37                    	movq	(%rdi), %r14
10000790a: 4d 85 f6                    	testq	%r14, %r14
10000790d: 0f 84 13 01 00 00           	je	0x100007a26 <_mb_win_mutex_release+0x136>
100007913: 49 89 f7                    	movq	%rsi, %r15
100007916: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
10000791a: e8 01 a8 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000791f: 4c 89 f7                    	movq	%r14, %rdi
100007922: e8 91 0d 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100007927: 85 c0                       	testl	%eax, %eax
100007929: 0f 85 f7 00 00 00           	jne	0x100007a26 <_mb_win_mutex_release+0x136>
10000792f: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007933: e8 e8 a7 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007938: 48 89 c3                    	movq	%rax, %rbx
10000793b: 44 8b 28                    	movl	(%rax), %r13d
10000793e: 49 8d 46 68                 	leaq	0x68(%r14), %rax
100007942: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100007950: 48 8b 00                    	movq	(%rax), %rax
100007953: 48 85 c0                    	testq	%rax, %rax
100007956: 0f 84 8e 00 00 00           	je	0x1000079ea <_mb_win_mutex_release+0xfa>
10000795c: 4c 39 78 08                 	cmpq	%r15, 0x8(%rax)
100007960: 75 ee                       	jne	0x100007950 <_mb_win_mutex_release+0x60>
100007962: 4c 8b 78 10                 	movq	0x10(%rax), %r15
100007966: 41 83 7f 34 00              	cmpl	$0x0, 0x34(%r15)
10000796b: 0f 84 8f 00 00 00           	je	0x100007a00 <_mb_win_mutex_release+0x110>
100007971: 4d 8b 67 40                 	movq	0x40(%r15), %r12
100007975: e8 4a 0d 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
10000797a: 4c 89 e7                    	movq	%r12, %rdi
10000797d: 48 89 c6                    	movq	%rax, %rsi
100007980: e8 09 0d 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100007985: 85 c0                       	testl	%eax, %eax
100007987: 74 77                       	je	0x100007a00 <_mb_win_mutex_release+0x110>
100007989: 49 8b be 88 00 00 00        	movq	0x88(%r14), %rdi
100007990: e8 ff 0c 00 00              	callq	0x100008694 <_sysconf+0x100008694>
100007995: 48 85 c0                    	testq	%rax, %rax
100007998: 74 29                       	je	0x1000079c3 <_mb_win_mutex_release+0xd3>
10000799a: 48 83 c0 08                 	addq	$0x8, %rax
10000799e: 66 90                       	nop
1000079a0: 48 89 c1                    	movq	%rax, %rcx
1000079a3: 48 8b 00                    	movq	(%rax), %rax
1000079a6: 48 85 c0                    	testq	%rax, %rax
1000079a9: 74 18                       	je	0x1000079c3 <_mb_win_mutex_release+0xd3>
1000079ab: 48 8b 50 08                 	movq	0x8(%rax), %rdx
1000079af: 49 3b 57 08                 	cmpq	0x8(%r15), %rdx
1000079b3: 75 eb                       	jne	0x1000079a0 <_mb_win_mutex_release+0xb0>
1000079b5: 48 8b 10                    	movq	(%rax), %rdx
1000079b8: 48 89 11                    	movq	%rdx, (%rcx)
1000079bb: 48 89 c7                    	movq	%rax, %rdi
1000079be: e8 77 0c 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
1000079c3: 41 c7 47 34 00 00 00 00     	movl	$0x0, 0x34(%r15)
1000079cb: 4c 89 ff                    	movq	%r15, %rdi
1000079ce: 31 f6                       	xorl	%esi, %esi
1000079d0: e8 5b 00 00 00              	callq	0x100007a30 <_grant_first>
1000079d5: 4c 89 f7                    	movq	%r14, %rdi
1000079d8: e8 e1 0c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000079dd: 89 c1                       	movl	%eax, %ecx
1000079df: b8 01 00 00 00              	movl	$0x1, %eax
1000079e4: 85 c9                       	testl	%ecx, %ecx
1000079e6: 74 2c                       	je	0x100007a14 <_mb_win_mutex_release+0x124>
1000079e8: eb 3c                       	jmp	0x100007a26 <_mb_win_mutex_release+0x136>
1000079ea: 4c 89 f7                    	movq	%r14, %rdi
1000079ed: e8 cc 0c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000079f2: 85 c0                       	testl	%eax, %eax
1000079f4: 75 30                       	jne	0x100007a26 <_mb_win_mutex_release+0x136>
1000079f6: 31 c0                       	xorl	%eax, %eax
1000079f8: 41 bd 06 00 00 00           	movl	$0x6, %r13d
1000079fe: eb 14                       	jmp	0x100007a14 <_mb_win_mutex_release+0x124>
100007a00: 4c 89 f7                    	movq	%r14, %rdi
100007a03: e8 b6 0c 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007a08: 85 c0                       	testl	%eax, %eax
100007a0a: 75 1a                       	jne	0x100007a26 <_mb_win_mutex_release+0x136>
100007a0c: 31 c0                       	xorl	%eax, %eax
100007a0e: 41 bd 20 01 00 00           	movl	$0x120, %r13d           ## imm = 0x120
100007a14: 44 89 2b                    	movl	%r13d, (%rbx)
100007a17: 48 83 c4 08                 	addq	$0x8, %rsp
100007a1b: 5b                          	popq	%rbx
100007a1c: 41 5c                       	popq	%r12
100007a1e: 41 5d                       	popq	%r13
100007a20: 41 5e                       	popq	%r14
100007a22: 41 5f                       	popq	%r15
100007a24: 5d                          	popq	%rbp
100007a25: c3                          	retq
100007a26: e8 e5 0b 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100007a2b: 0f 1f 44 00 00              	nopl	(%rax,%rax)

0000000100007a30 <_grant_first>:
100007a30: 55                          	pushq	%rbp
100007a31: 48 89 e5                    	movq	%rsp, %rbp
100007a34: 41 57                       	pushq	%r15
100007a36: 41 56                       	pushq	%r14
100007a38: 41 55                       	pushq	%r13
100007a3a: 41 54                       	pushq	%r12
100007a3c: 53                          	pushq	%rbx
100007a3d: 50                          	pushq	%rax
100007a3e: 4c 8b 67 48                 	movq	0x48(%rdi), %r12
100007a42: 4d 85 e4                    	testq	%r12, %r12
100007a45: 0f 84 ab 00 00 00           	je	0x100007af6 <_grant_first+0xc6>
100007a4b: 48 89 fb                    	movq	%rdi, %rbx
100007a4e: 89 75 d4                    	movl	%esi, -0x2c(%rbp)
100007a51: 4c 8d 77 48                 	leaq	0x48(%rdi), %r14
100007a55: 4d 8b 7c 24 08              	movq	0x8(%r12), %r15
100007a5a: 4c 89 ff                    	movq	%r15, %rdi
100007a5d: 4c 89 fe                    	movq	%r15, %rsi
100007a60: e8 29 0c 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100007a65: 85 c0                       	testl	%eax, %eax
100007a67: 74 05                       	je	0x100007a6e <_grant_first+0x3e>
100007a69: 45 31 ed                    	xorl	%r13d, %r13d
100007a6c: eb 37                       	jmp	0x100007aa5 <_grant_first+0x75>
100007a6e: 4d 89 f5                    	movq	%r14, %r13
100007a71: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
100007a80: 4d 8b 6d 00                 	movq	(%r13), %r13
100007a84: 49 8b 45 00                 	movq	(%r13), %rax
100007a88: 48 85 c0                    	testq	%rax, %rax
100007a8b: 74 15                       	je	0x100007aa2 <_grant_first+0x72>
100007a8d: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100007a91: 4c 89 fe                    	movq	%r15, %rsi
100007a94: e8 f5 0b 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100007a99: 4d 89 ee                    	movq	%r13, %r14
100007a9c: 85 c0                       	testl	%eax, %eax
100007a9e: 74 e0                       	je	0x100007a80 <_grant_first+0x50>
100007aa0: eb 03                       	jmp	0x100007aa5 <_grant_first+0x75>
100007aa2: 4d 89 ee                    	movq	%r13, %r14
100007aa5: 49 8b 06                    	movq	(%r14), %rax
100007aa8: 48 85 c0                    	testq	%rax, %rax
100007aab: 74 58                       	je	0x100007b05 <_grant_first+0xd5>
100007aad: 48 8b 08                    	movq	(%rax), %rcx
100007ab0: 49 89 0e                    	movq	%rcx, (%r14)
100007ab3: 48 39 43 50                 	cmpq	%rax, 0x50(%rbx)
100007ab7: 8b 4d d4                    	movl	-0x2c(%rbp), %ecx
100007aba: 75 04                       	jne	0x100007ac0 <_grant_first+0x90>
100007abc: 4c 89 6b 50                 	movq	%r13, 0x50(%rbx)
100007ac0: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
100007ac7: 48 c7 43 34 01 00 00 00     	movq	$0x1, 0x34(%rbx)
100007acf: 49 8b 44 24 08              	movq	0x8(%r12), %rax
100007ad4: 48 89 43 40                 	movq	%rax, 0x40(%rbx)
100007ad8: 41 c7 44 24 10 01 00 00 00  	movl	$0x1, 0x10(%r12)
100007ae1: 41 89 4c 24 14              	movl	%ecx, 0x14(%r12)
100007ae6: 48 83 c3 58                 	addq	$0x58, %rbx
100007aea: 48 89 df                    	movq	%rbx, %rdi
100007aed: e8 66 0b 00 00              	callq	0x100008658 <_sysconf+0x100008658>
100007af2: 85 c0                       	testl	%eax, %eax
100007af4: 75 0f                       	jne	0x100007b05 <_grant_first+0xd5>
100007af6: 48 83 c4 08                 	addq	$0x8, %rsp
100007afa: 5b                          	popq	%rbx
100007afb: 41 5c                       	popq	%r12
100007afd: 41 5d                       	popq	%r13
100007aff: 41 5e                       	popq	%r14
100007b01: 41 5f                       	popq	%r15
100007b03: 5d                          	popq	%rbp
100007b04: c3                          	retq
100007b05: e8 06 0b 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100007b0a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100007b10 <_mb_win_mutex_wait>:
100007b10: 55                          	pushq	%rbp
100007b11: 48 89 e5                    	movq	%rsp, %rbp
100007b14: 31 c9                       	xorl	%ecx, %ecx
100007b16: 5d                          	popq	%rbp
100007b17: e9 04 00 00 00              	jmp	0x100007b20 <_mb_win_mutex_wait_ex>
100007b1c: 0f 1f 40 00                 	nopl	(%rax)

0000000100007b20 <_mb_win_mutex_wait_ex>:
100007b20: 55                          	pushq	%rbp
100007b21: 48 89 e5                    	movq	%rsp, %rbp
100007b24: 41 57                       	pushq	%r15
100007b26: 41 56                       	pushq	%r14
100007b28: 41 55                       	pushq	%r13
100007b2a: 41 54                       	pushq	%r12
100007b2c: 53                          	pushq	%rbx
100007b2d: 48 83 ec 68                 	subq	$0x68, %rsp
100007b31: 48 85 ff                    	testq	%rdi, %rdi
100007b34: 0f 84 5b 06 00 00           	je	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007b3a: 4c 8b 37                    	movq	(%rdi), %r14
100007b3d: 4d 85 f6                    	testq	%r14, %r14
100007b40: 0f 84 4f 06 00 00           	je	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007b46: 41 89 cf                    	movl	%ecx, %r15d
100007b49: 41 89 d4                    	movl	%edx, %r12d
100007b4c: 49 89 f5                    	movq	%rsi, %r13
100007b4f: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007b53: e8 c8 a5 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007b58: 4c 89 f7                    	movq	%r14, %rdi
100007b5b: e8 58 0b 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100007b60: 85 c0                       	testl	%eax, %eax
100007b62: 0f 85 2d 06 00 00           	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007b68: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007b6c: e8 af a5 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007b71: 48 89 c3                    	movq	%rax, %rbx
100007b74: 8b 00                       	movl	(%rax), %eax
100007b76: 89 45 d4                    	movl	%eax, -0x2c(%rbp)
100007b79: 49 8d 46 68                 	leaq	0x68(%r14), %rax
100007b7d: 0f 1f 00                    	nopl	(%rax)
100007b80: 48 8b 00                    	movq	(%rax), %rax
100007b83: 48 85 c0                    	testq	%rax, %rax
100007b86: 74 23                       	je	0x100007bab <_mb_win_mutex_wait_ex+0x8b>
100007b88: 4c 39 68 08                 	cmpq	%r13, 0x8(%rax)
100007b8c: 75 f2                       	jne	0x100007b80 <_mb_win_mutex_wait_ex+0x60>
100007b8e: 45 85 ff                    	testl	%r15d, %r15d
100007b91: 74 46                       	je	0x100007bd9 <_mb_win_mutex_wait_ex+0xb9>
100007b93: 4c 89 f7                    	movq	%r14, %rdi
100007b96: e8 23 0b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007b9b: 85 c0                       	testl	%eax, %eax
100007b9d: 0f 85 f2 05 00 00           	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007ba3: c7 03 32 00 00 00           	movl	$0x32, (%rbx)
100007ba9: eb 16                       	jmp	0x100007bc1 <_mb_win_mutex_wait_ex+0xa1>
100007bab: 4c 89 f7                    	movq	%r14, %rdi
100007bae: e8 0b 0b 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007bb3: 85 c0                       	testl	%eax, %eax
100007bb5: 0f 85 da 05 00 00           	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007bbb: c7 03 06 00 00 00           	movl	$0x6, (%rbx)
100007bc1: 41 bf ff ff ff ff           	movl	$0xffffffff, %r15d      ## imm = 0xFFFFFFFF
100007bc7: 44 89 f8                    	movl	%r15d, %eax
100007bca: 48 83 c4 68                 	addq	$0x68, %rsp
100007bce: 5b                          	popq	%rbx
100007bcf: 41 5c                       	popq	%r12
100007bd1: 41 5d                       	popq	%r13
100007bd3: 41 5e                       	popq	%r14
100007bd5: 41 5f                       	popq	%r15
100007bd7: 5d                          	popq	%rbp
100007bd8: c3                          	retq
100007bd9: 4c 8b 68 10                 	movq	0x10(%rax), %r13
100007bdd: 41 83 7d 34 00              	cmpl	$0x0, 0x34(%r13)
100007be2: 74 5a                       	je	0x100007c3e <_mb_win_mutex_wait_ex+0x11e>
100007be4: 4d 8b 7d 40                 	movq	0x40(%r13), %r15
100007be8: e8 d7 0a 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100007bed: 4c 89 ff                    	movq	%r15, %rdi
100007bf0: 48 89 c6                    	movq	%rax, %rsi
100007bf3: e8 96 0a 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100007bf8: 85 c0                       	testl	%eax, %eax
100007bfa: 74 42                       	je	0x100007c3e <_mb_win_mutex_wait_ex+0x11e>
100007bfc: 41 83 fc ff                 	cmpl	$-0x1, %r12d
100007c00: 0f 84 ca 00 00 00           	je	0x100007cd0 <_mb_win_mutex_wait_ex+0x1b0>
100007c06: 48 8d 75 b8                 	leaq	-0x48(%rbp), %rsi
100007c0a: bf 06 00 00 00              	movl	$0x6, %edi
100007c0f: e8 0e 0a 00 00              	callq	0x100008622 <_sysconf+0x100008622>
100007c14: 85 c0                       	testl	%eax, %eax
100007c16: 75 0c                       	jne	0x100007c24 <_mb_win_mutex_wait_ex+0x104>
100007c18: 48 69 4d b8 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x48(%rbp), %rcx ## imm = 0x3B9ACA00
100007c20: 48 03 4d c0                 	addq	-0x40(%rbp), %rcx
100007c24: 85 c0                       	testl	%eax, %eax
100007c26: 0f 84 cc 00 00 00           	je	0x100007cf8 <_mb_win_mutex_wait_ex+0x1d8>
100007c2c: 45 31 e4                    	xorl	%r12d, %r12d
100007c2f: 31 c0                       	xorl	%eax, %eax
100007c31: 84 c0                       	testb	%al, %al
100007c33: 0f 85 e7 00 00 00           	jne	0x100007d20 <_mb_win_mutex_wait_ex+0x200>
100007c39: e9 fc 04 00 00              	jmp	0x10000813a <_mb_win_mutex_wait_ex+0x61a>
100007c3e: 41 83 7d 34 00              	cmpl	$0x0, 0x34(%r13)
100007c43: 74 47                       	je	0x100007c8c <_mb_win_mutex_wait_ex+0x16c>
100007c45: 41 83 fc ff                 	cmpl	$-0x1, %r12d
100007c49: 0f 84 9c 00 00 00           	je	0x100007ceb <_mb_win_mutex_wait_ex+0x1cb>
100007c4f: 48 8d 75 b8                 	leaq	-0x48(%rbp), %rsi
100007c53: bf 06 00 00 00              	movl	$0x6, %edi
100007c58: e8 c5 09 00 00              	callq	0x100008622 <_sysconf+0x100008622>
100007c5d: 85 c0                       	testl	%eax, %eax
100007c5f: 75 0c                       	jne	0x100007c6d <_mb_win_mutex_wait_ex+0x14d>
100007c61: 48 69 4d b8 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x48(%rbp), %rcx ## imm = 0x3B9ACA00
100007c69: 48 03 4d c0                 	addq	-0x40(%rbp), %rcx
100007c6d: 85 c0                       	testl	%eax, %eax
100007c6f: 0f 84 d4 01 00 00           	je	0x100007e49 <_mb_win_mutex_wait_ex+0x329>
100007c75: 48 c7 45 a0 00 00 00 00     	movq	$0x0, -0x60(%rbp)
100007c7d: 31 c0                       	xorl	%eax, %eax
100007c7f: 84 c0                       	testb	%al, %al
100007c81: 0f 84 b3 04 00 00           	je	0x10000813a <_mb_win_mutex_wait_ex+0x61a>
100007c87: e9 e8 01 00 00              	jmp	0x100007e74 <_mb_win_mutex_wait_ex+0x354>
100007c8c: 45 31 ff                    	xorl	%r15d, %r15d
100007c8f: 41 83 7d 38 00              	cmpl	$0x0, 0x38(%r13)
100007c94: 41 0f 95 c7                 	setne	%r15b
100007c98: 41 c1 e7 07                 	shll	$0x7, %r15d
100007c9c: 49 c7 45 34 01 00 00 00     	movq	$0x1, 0x34(%r13)
100007ca4: e8 1b 0a 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100007ca9: 49 89 45 40                 	movq	%rax, 0x40(%r13)
100007cad: 4c 89 f7                    	movq	%r14, %rdi
100007cb0: 4c 89 ee                    	movq	%r13, %rsi
100007cb3: e8 e8 f9 ff ff              	callq	0x1000076a0 <_adopt>
100007cb8: 85 c0                       	testl	%eax, %eax
100007cba: 0f 85 69 04 00 00           	jne	0x100008129 <_mb_win_mutex_wait_ex+0x609>
100007cc0: 41 c7 45 34 00 00 00 00     	movl	$0x0, 0x34(%r13)
100007cc8: 4c 89 ef                    	movq	%r13, %rdi
100007ccb: e9 9f 04 00 00              	jmp	0x10000816f <_mb_win_mutex_wait_ex+0x64f>
100007cd0: 4c 89 f7                    	movq	%r14, %rdi
100007cd3: e8 e6 09 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007cd8: 85 c0                       	testl	%eax, %eax
100007cda: 0f 85 b5 04 00 00           	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007ce0: c7 03 03 03 00 00           	movl	$0x303, (%rbx)          ## imm = 0x303
100007ce6: e9 d6 fe ff ff              	jmp	0x100007bc1 <_mb_win_mutex_wait_ex+0xa1>
100007ceb: 48 c7 45 a0 00 00 00 00     	movq	$0x0, -0x60(%rbp)
100007cf3: e9 7c 01 00 00              	jmp	0x100007e74 <_mb_win_mutex_wait_ex+0x354>
100007cf8: 44 89 e0                    	movl	%r12d, %eax
100007cfb: 48 69 c0 40 42 0f 00        	imulq	$0xf4240, %rax, %rax    ## imm = 0xF4240
100007d02: 48 89 ca                    	movq	%rcx, %rdx
100007d05: 48 f7 d2                    	notq	%rdx
100007d08: 48 01 c1                    	addq	%rax, %rcx
100007d0b: 45 31 e4                    	xorl	%r12d, %r12d
100007d0e: 48 39 d0                    	cmpq	%rdx, %rax
100007d11: 0f 96 c0                    	setbe	%al
100007d14: 4c 0f 46 e1                 	cmovbeq	%rcx, %r12
100007d18: 84 c0                       	testb	%al, %al
100007d1a: 0f 84 1a 04 00 00           	je	0x10000813a <_mb_win_mutex_wait_ex+0x61a>
100007d20: 49 83 c5 58                 	addq	$0x58, %r13
100007d24: 4c 89 6d b0                 	movq	%r13, -0x50(%rbp)
100007d28: 4c 8d 7d b8                 	leaq	-0x48(%rbp), %r15
100007d2c: eb 0a                       	jmp	0x100007d38 <_mb_win_mutex_wait_ex+0x218>
100007d2e: 66 90                       	nop
100007d30: 85 c9                       	testl	%ecx, %ecx
100007d32: 0f 85 e8 00 00 00           	jne	0x100007e20 <_mb_win_mutex_wait_ex+0x300>
100007d38: bf 06 00 00 00              	movl	$0x6, %edi
100007d3d: 4c 89 fe                    	movq	%r15, %rsi
100007d40: e8 dd 08 00 00              	callq	0x100008622 <_sysconf+0x100008622>
100007d45: 85 c0                       	testl	%eax, %eax
100007d47: 74 0a                       	je	0x100007d53 <_mb_win_mutex_wait_ex+0x233>
100007d49: 85 c0                       	testl	%eax, %eax
100007d4b: 0f 85 af 00 00 00           	jne	0x100007e00 <_mb_win_mutex_wait_ex+0x2e0>
100007d51: eb 14                       	jmp	0x100007d67 <_mb_win_mutex_wait_ex+0x247>
100007d53: 4c 69 6d b8 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x48(%rbp), %r13 ## imm = 0x3B9ACA00
100007d5b: 4c 03 6d c0                 	addq	-0x40(%rbp), %r13
100007d5f: 85 c0                       	testl	%eax, %eax
100007d61: 0f 85 99 00 00 00           	jne	0x100007e00 <_mb_win_mutex_wait_ex+0x2e0>
100007d67: b9 02 00 00 00              	movl	$0x2, %ecx
100007d6c: 4d 39 e5                    	cmpq	%r12, %r13
100007d6f: 73 bf                       	jae	0x100007d30 <_mb_win_mutex_wait_ex+0x210>
100007d71: bf 06 00 00 00              	movl	$0x6, %edi
100007d76: 4c 89 fe                    	movq	%r15, %rsi
100007d79: e8 a4 08 00 00              	callq	0x100008622 <_sysconf+0x100008622>
100007d7e: 89 c1                       	movl	%eax, %ecx
100007d80: 85 c0                       	testl	%eax, %eax
100007d82: 75 0c                       	jne	0x100007d90 <_mb_win_mutex_wait_ex+0x270>
100007d84: 48 69 55 b8 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x48(%rbp), %rdx ## imm = 0x3B9ACA00
100007d8c: 48 03 55 c0                 	addq	-0x40(%rbp), %rdx
100007d90: b8 ff ff ff ff              	movl	$0xffffffff, %eax       ## imm = 0xFFFFFFFF
100007d95: 85 c9                       	testl	%ecx, %ecx
100007d97: 75 46                       	jne	0x100007ddf <_mb_win_mutex_wait_ex+0x2bf>
100007d99: b8 3c 00 00 00              	movl	$0x3c, %eax
100007d9e: 4c 89 e1                    	movq	%r12, %rcx
100007da1: 48 29 d1                    	subq	%rdx, %rcx
100007da4: 76 39                       	jbe	0x100007ddf <_mb_win_mutex_wait_ex+0x2bf>
100007da6: 48 89 c8                    	movq	%rcx, %rax
100007da9: 48 c1 e8 09                 	shrq	$0x9, %rax
100007dad: 48 ba 53 5a 9b a0 2f b8 44 00       	movabsq	$0x44b82fa09b5a53, %rdx ## imm = 0x44B82FA09B5A53
100007db7: 48 f7 e2                    	mulq	%rdx
100007dba: 48 c1 ea 0b                 	shrq	$0xb, %rdx
100007dbe: 48 89 55 b8                 	movq	%rdx, -0x48(%rbp)
100007dc2: 48 69 c2 00 ca 9a 3b        	imulq	$0x3b9aca00, %rdx, %rax ## imm = 0x3B9ACA00
100007dc9: 48 29 c1                    	subq	%rax, %rcx
100007dcc: 48 89 4d c0                 	movq	%rcx, -0x40(%rbp)
100007dd0: 48 8b 7d b0                 	movq	-0x50(%rbp), %rdi
100007dd4: 4c 89 f6                    	movq	%r14, %rsi
100007dd7: 4c 89 fa                    	movq	%r15, %rdx
100007dda: e8 91 08 00 00              	callq	0x100008670 <_sysconf+0x100008670>
100007ddf: 31 c9                       	xorl	%ecx, %ecx
100007de1: 85 c0                       	testl	%eax, %eax
100007de3: 0f 84 47 ff ff ff           	je	0x100007d30 <_mb_win_mutex_wait_ex+0x210>
100007de9: 83 f8 3c                    	cmpl	$0x3c, %eax
100007dec: 0f 84 3e ff ff ff           	je	0x100007d30 <_mb_win_mutex_wait_ex+0x210>
100007df2: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100007e00: 4c 89 f7                    	movq	%r14, %rdi
100007e03: e8 b6 08 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007e08: 85 c0                       	testl	%eax, %eax
100007e0a: 0f 85 85 03 00 00           	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007e10: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
100007e16: b9 01 00 00 00              	movl	$0x1, %ecx
100007e1b: e9 10 ff ff ff              	jmp	0x100007d30 <_mb_win_mutex_wait_ex+0x210>
100007e20: 83 f9 02                    	cmpl	$0x2, %ecx
100007e23: 0f 85 98 fd ff ff           	jne	0x100007bc1 <_mb_win_mutex_wait_ex+0xa1>
100007e29: 4c 89 f7                    	movq	%r14, %rdi
100007e2c: e8 8d 08 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100007e31: 85 c0                       	testl	%eax, %eax
100007e33: 0f 85 5c 03 00 00           	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007e39: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
100007e3c: 89 03                       	movl	%eax, (%rbx)
100007e3e: 41 bf 02 01 00 00           	movl	$0x102, %r15d           ## imm = 0x102
100007e44: e9 7e fd ff ff              	jmp	0x100007bc7 <_mb_win_mutex_wait_ex+0xa7>
100007e49: 44 89 e0                    	movl	%r12d, %eax
100007e4c: 48 69 c0 40 42 0f 00        	imulq	$0xf4240, %rax, %rax    ## imm = 0xF4240
100007e53: 48 89 ca                    	movq	%rcx, %rdx
100007e56: 48 f7 d2                    	notq	%rdx
100007e59: 48 01 c1                    	addq	%rax, %rcx
100007e5c: 31 f6                       	xorl	%esi, %esi
100007e5e: 48 39 d0                    	cmpq	%rdx, %rax
100007e61: 0f 96 c0                    	setbe	%al
100007e64: 48 0f 46 f1                 	cmovbeq	%rcx, %rsi
100007e68: 48 89 75 a0                 	movq	%rsi, -0x60(%rbp)
100007e6c: 84 c0                       	testb	%al, %al
100007e6e: 0f 84 c6 02 00 00           	je	0x10000813a <_mb_win_mutex_wait_ex+0x61a>
100007e74: 48 c7 45 b8 00 00 00 00     	movq	$0x0, -0x48(%rbp)
100007e7c: e8 43 08 00 00              	callq	0x1000086c4 <_sysconf+0x1000086c4>
100007e81: 48 89 45 c0                 	movq	%rax, -0x40(%rbp)
100007e85: 48 c7 45 c8 00 00 00 00     	movq	$0x0, -0x38(%rbp)
100007e8d: 49 8b 45 50                 	movq	0x50(%r13), %rax
100007e91: 49 8d 4d 48                 	leaq	0x48(%r13), %rcx
100007e95: 48 85 c0                    	testq	%rax, %rax
100007e98: 48 89 4d a8                 	movq	%rcx, -0x58(%rbp)
100007e9c: 48 0f 44 c1                 	cmoveq	%rcx, %rax
100007ea0: 48 8d 4d b8                 	leaq	-0x48(%rbp), %rcx
100007ea4: 48 89 08                    	movq	%rcx, (%rax)
100007ea7: 49 89 4d 50                 	movq	%rcx, 0x50(%r13)
100007eab: 49 ff 45 18                 	incq	0x18(%r13)
100007eaf: 4c 89 6d b0                 	movq	%r13, -0x50(%rbp)
100007eb3: 49 83 c5 58                 	addq	$0x58, %r13
100007eb7: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100007ec0: 83 7d c8 00                 	cmpl	$0x0, -0x38(%rbp)
100007ec4: 0f 85 43 02 00 00           	jne	0x10000810d <_mb_win_mutex_wait_ex+0x5ed>
100007eca: 41 83 fc ff                 	cmpl	$-0x1, %r12d
100007ece: 75 14                       	jne	0x100007ee4 <_mb_win_mutex_wait_ex+0x3c4>
100007ed0: 4c 89 ef                    	movq	%r13, %rdi
100007ed3: 4c 89 f6                    	movq	%r14, %rsi
100007ed6: e8 9b 07 00 00              	callq	0x100008676 <_sysconf+0x100008676>
100007edb: 85 c0                       	testl	%eax, %eax
100007edd: 74 e1                       	je	0x100007ec0 <_mb_win_mutex_wait_ex+0x3a0>
100007edf: e9 b1 02 00 00              	jmp	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100007ee4: bf 06 00 00 00              	movl	$0x6, %edi
100007ee9: 48 8d 75 90                 	leaq	-0x70(%rbp), %rsi
100007eed: e8 30 07 00 00              	callq	0x100008622 <_sysconf+0x100008622>
100007ef2: 85 c0                       	testl	%eax, %eax
100007ef4: 48 8b 8d 78 ff ff ff        	movq	-0x88(%rbp), %rcx
100007efb: 75 0c                       	jne	0x100007f09 <_mb_win_mutex_wait_ex+0x3e9>
100007efd: 48 69 4d 90 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x70(%rbp), %rcx ## imm = 0x3B9ACA00
100007f05: 48 03 4d 98                 	addq	-0x68(%rbp), %rcx
100007f09: 85 c0                       	testl	%eax, %eax
100007f0b: 48 89 8d 78 ff ff ff        	movq	%rcx, -0x88(%rbp)
100007f12: 0f 85 f4 00 00 00           	jne	0x10000800c <_mb_win_mutex_wait_ex+0x4ec>
100007f18: 48 3b 4d a0                 	cmpq	-0x60(%rbp), %rcx
100007f1c: 0f 83 ea 00 00 00           	jae	0x10000800c <_mb_win_mutex_wait_ex+0x4ec>
100007f22: bf 06 00 00 00              	movl	$0x6, %edi
100007f27: 48 8d 75 90                 	leaq	-0x70(%rbp), %rsi
100007f2b: e8 f2 06 00 00              	callq	0x100008622 <_sysconf+0x100008622>
100007f30: 89 c1                       	movl	%eax, %ecx
100007f32: 85 c0                       	testl	%eax, %eax
100007f34: 75 0c                       	jne	0x100007f42 <_mb_win_mutex_wait_ex+0x422>
100007f36: 48 69 55 90 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x70(%rbp), %rdx ## imm = 0x3B9ACA00
100007f3e: 48 03 55 98                 	addq	-0x68(%rbp), %rdx
100007f42: b8 ff ff ff ff              	movl	$0xffffffff, %eax       ## imm = 0xFFFFFFFF
100007f47: 85 c9                       	testl	%ecx, %ecx
100007f49: 75 47                       	jne	0x100007f92 <_mb_win_mutex_wait_ex+0x472>
100007f4b: b8 3c 00 00 00              	movl	$0x3c, %eax
100007f50: 48 8b 4d a0                 	movq	-0x60(%rbp), %rcx
100007f54: 48 29 d1                    	subq	%rdx, %rcx
100007f57: 76 39                       	jbe	0x100007f92 <_mb_win_mutex_wait_ex+0x472>
100007f59: 48 89 c8                    	movq	%rcx, %rax
100007f5c: 48 c1 e8 09                 	shrq	$0x9, %rax
100007f60: 48 ba 53 5a 9b a0 2f b8 44 00       	movabsq	$0x44b82fa09b5a53, %rdx ## imm = 0x44B82FA09B5A53
100007f6a: 48 f7 e2                    	mulq	%rdx
100007f6d: 48 c1 ea 0b                 	shrq	$0xb, %rdx
100007f71: 48 89 55 90                 	movq	%rdx, -0x70(%rbp)
100007f75: 48 69 c2 00 ca 9a 3b        	imulq	$0x3b9aca00, %rdx, %rax ## imm = 0x3B9ACA00
100007f7c: 48 29 c1                    	subq	%rax, %rcx
100007f7f: 48 89 4d 98                 	movq	%rcx, -0x68(%rbp)
100007f83: 4c 89 ef                    	movq	%r13, %rdi
100007f86: 4c 89 f6                    	movq	%r14, %rsi
100007f89: 48 8d 55 90                 	leaq	-0x70(%rbp), %rdx
100007f8d: e8 de 06 00 00              	callq	0x100008670 <_sysconf+0x100008670>
100007f92: 85 c0                       	testl	%eax, %eax
100007f94: 0f 94 c1                    	sete	%cl
100007f97: 83 f8 3c                    	cmpl	$0x3c, %eax
100007f9a: 0f 94 c2                    	sete	%dl
100007f9d: 08 ca                       	orb	%cl, %dl
100007f9f: 85 c0                       	testl	%eax, %eax
100007fa1: 0f 84 59 01 00 00           	je	0x100008100 <_mb_win_mutex_wait_ex+0x5e0>
100007fa7: 83 f8 3c                    	cmpl	$0x3c, %eax
100007faa: 0f 84 50 01 00 00           	je	0x100008100 <_mb_win_mutex_wait_ex+0x5e0>
100007fb0: 89 55 84                    	movl	%edx, -0x7c(%rbp)
100007fb3: 4c 8b 7d a8                 	movq	-0x58(%rbp), %r15
100007fb7: 49 8b 07                    	movq	(%r15), %rax
100007fba: 48 85 c0                    	testq	%rax, %rax
100007fbd: 0f 84 9c 00 00 00           	je	0x10000805f <_mb_win_mutex_wait_ex+0x53f>
100007fc3: 48 8b 75 c0                 	movq	-0x40(%rbp), %rsi
100007fc7: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100007fcb: 48 89 75 88                 	movq	%rsi, -0x78(%rbp)
100007fcf: e8 ba 06 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100007fd4: 85 c0                       	testl	%eax, %eax
100007fd6: 74 0c                       	je	0x100007fe4 <_mb_win_mutex_wait_ex+0x4c4>
100007fd8: 45 31 ff                    	xorl	%r15d, %r15d
100007fdb: 48 8b 4d a8                 	movq	-0x58(%rbp), %rcx
100007fdf: e9 d1 00 00 00              	jmp	0x1000080b5 <_mb_win_mutex_wait_ex+0x595>
100007fe4: 4d 8b 3f                    	movq	(%r15), %r15
100007fe7: 49 8b 07                    	movq	(%r15), %rax
100007fea: 48 85 c0                    	testq	%rax, %rax
100007fed: 0f 84 bf 00 00 00           	je	0x1000080b2 <_mb_win_mutex_wait_ex+0x592>
100007ff3: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100007ff7: 48 8b 75 88                 	movq	-0x78(%rbp), %rsi
100007ffb: e8 8e 06 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100008000: 4c 89 f9                    	movq	%r15, %rcx
100008003: 85 c0                       	testl	%eax, %eax
100008005: 74 dd                       	je	0x100007fe4 <_mb_win_mutex_wait_ex+0x4c4>
100008007: e9 a9 00 00 00              	jmp	0x1000080b5 <_mb_win_mutex_wait_ex+0x595>
10000800c: 4c 8b 7d a8                 	movq	-0x58(%rbp), %r15
100008010: 49 8b 07                    	movq	(%r15), %rax
100008013: 48 85 c0                    	testq	%rax, %rax
100008016: 74 3f                       	je	0x100008057 <_mb_win_mutex_wait_ex+0x537>
100008018: 48 8b 75 c0                 	movq	-0x40(%rbp), %rsi
10000801c: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100008020: 48 89 75 88                 	movq	%rsi, -0x78(%rbp)
100008024: e8 65 06 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
100008029: 85 c0                       	testl	%eax, %eax
10000802b: 74 09                       	je	0x100008036 <_mb_win_mutex_wait_ex+0x516>
10000802d: 45 31 ff                    	xorl	%r15d, %r15d
100008030: 48 8b 4d a8                 	movq	-0x58(%rbp), %rcx
100008034: eb 34                       	jmp	0x10000806a <_mb_win_mutex_wait_ex+0x54a>
100008036: 4d 8b 3f                    	movq	(%r15), %r15
100008039: 49 8b 07                    	movq	(%r15), %rax
10000803c: 48 85 c0                    	testq	%rax, %rax
10000803f: 74 26                       	je	0x100008067 <_mb_win_mutex_wait_ex+0x547>
100008041: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100008045: 48 8b 75 88                 	movq	-0x78(%rbp), %rsi
100008049: e8 40 06 00 00              	callq	0x10000868e <_sysconf+0x10000868e>
10000804e: 4c 89 f9                    	movq	%r15, %rcx
100008051: 85 c0                       	testl	%eax, %eax
100008053: 74 e1                       	je	0x100008036 <_mb_win_mutex_wait_ex+0x516>
100008055: eb 13                       	jmp	0x10000806a <_mb_win_mutex_wait_ex+0x54a>
100008057: 4c 89 f9                    	movq	%r15, %rcx
10000805a: 45 31 ff                    	xorl	%r15d, %r15d
10000805d: eb 0b                       	jmp	0x10000806a <_mb_win_mutex_wait_ex+0x54a>
10000805f: 4c 89 f9                    	movq	%r15, %rcx
100008062: 45 31 ff                    	xorl	%r15d, %r15d
100008065: eb 4e                       	jmp	0x1000080b5 <_mb_win_mutex_wait_ex+0x595>
100008067: 4c 89 f9                    	movq	%r15, %rcx
10000806a: 48 8b 01                    	movq	(%rcx), %rax
10000806d: 48 85 c0                    	testq	%rax, %rax
100008070: 0f 84 1f 01 00 00           	je	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100008076: 48 8b 10                    	movq	(%rax), %rdx
100008079: 48 89 11                    	movq	%rdx, (%rcx)
10000807c: 48 8b 4d b0                 	movq	-0x50(%rbp), %rcx
100008080: 48 39 41 50                 	cmpq	%rax, 0x50(%rcx)
100008084: 75 04                       	jne	0x10000808a <_mb_win_mutex_wait_ex+0x56a>
100008086: 4c 89 79 50                 	movq	%r15, 0x50(%rcx)
10000808a: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
100008091: 48 ff 49 18                 	decq	0x18(%rcx)
100008095: 4c 89 f7                    	movq	%r14, %rdi
100008098: e8 21 06 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000809d: 85 c0                       	testl	%eax, %eax
10000809f: 0f 85 f0 00 00 00           	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
1000080a5: 41 bf 02 01 00 00           	movl	$0x102, %r15d           ## imm = 0x102
1000080ab: 31 d2                       	xorl	%edx, %edx
1000080ad: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
1000080b0: eb 4c                       	jmp	0x1000080fe <_mb_win_mutex_wait_ex+0x5de>
1000080b2: 4c 89 f9                    	movq	%r15, %rcx
1000080b5: 48 8b 01                    	movq	(%rcx), %rax
1000080b8: 48 85 c0                    	testq	%rax, %rax
1000080bb: 0f 84 d4 00 00 00           	je	0x100008195 <_mb_win_mutex_wait_ex+0x675>
1000080c1: 48 8b 10                    	movq	(%rax), %rdx
1000080c4: 48 89 11                    	movq	%rdx, (%rcx)
1000080c7: 48 8b 4d b0                 	movq	-0x50(%rbp), %rcx
1000080cb: 48 39 41 50                 	cmpq	%rax, 0x50(%rcx)
1000080cf: 75 04                       	jne	0x1000080d5 <_mb_win_mutex_wait_ex+0x5b5>
1000080d1: 4c 89 79 50                 	movq	%r15, 0x50(%rcx)
1000080d5: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
1000080dc: 48 ff 49 18                 	decq	0x18(%rcx)
1000080e0: 4c 89 f7                    	movq	%r14, %rdi
1000080e3: e8 d6 05 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000080e8: 85 c0                       	testl	%eax, %eax
1000080ea: 0f 85 a5 00 00 00           	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
1000080f0: 41 bf ff ff ff ff           	movl	$0xffffffff, %r15d      ## imm = 0xFFFFFFFF
1000080f6: b8 57 00 00 00              	movl	$0x57, %eax
1000080fb: 8b 55 84                    	movl	-0x7c(%rbp), %edx
1000080fe: 89 03                       	movl	%eax, (%rbx)
100008100: 84 d2                       	testb	%dl, %dl
100008102: 0f 85 b8 fd ff ff           	jne	0x100007ec0 <_mb_win_mutex_wait_ex+0x3a0>
100008108: e9 ba fa ff ff              	jmp	0x100007bc7 <_mb_win_mutex_wait_ex+0xa7>
10000810d: 4c 8b 65 b0                 	movq	-0x50(%rbp), %r12
100008111: 49 ff 4c 24 18              	decq	0x18(%r12)
100008116: 44 8b 7d cc                 	movl	-0x34(%rbp), %r15d
10000811a: 4c 89 f7                    	movq	%r14, %rdi
10000811d: 4c 89 e6                    	movq	%r12, %rsi
100008120: e8 7b f5 ff ff              	callq	0x1000076a0 <_adopt>
100008125: 85 c0                       	testl	%eax, %eax
100008127: 74 28                       	je	0x100008151 <_mb_win_mutex_wait_ex+0x631>
100008129: 4c 89 f7                    	movq	%r14, %rdi
10000812c: e8 8d 05 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100008131: 85 c0                       	testl	%eax, %eax
100008133: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
100008136: 74 56                       	je	0x10000818e <_mb_win_mutex_wait_ex+0x66e>
100008138: eb 5b                       	jmp	0x100008195 <_mb_win_mutex_wait_ex+0x675>
10000813a: 4c 89 f7                    	movq	%r14, %rdi
10000813d: e8 7c 05 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100008142: 85 c0                       	testl	%eax, %eax
100008144: 75 4f                       	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100008146: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
10000814c: e9 70 fa ff ff              	jmp	0x100007bc1 <_mb_win_mutex_wait_ex+0xa1>
100008151: 41 c7 44 24 34 00 00 00 00  	movl	$0x0, 0x34(%r12)
10000815a: 41 81 ff 80 00 00 00        	cmpl	$0x80, %r15d
100008161: 75 09                       	jne	0x10000816c <_mb_win_mutex_wait_ex+0x64c>
100008163: 41 c7 44 24 38 01 00 00 00  	movl	$0x1, 0x38(%r12)
10000816c: 4c 89 e7                    	movq	%r12, %rdi
10000816f: 44 89 fe                    	movl	%r15d, %esi
100008172: e8 b9 f8 ff ff              	callq	0x100007a30 <_grant_first>
100008177: 4c 89 f7                    	movq	%r14, %rdi
10000817a: e8 3f 05 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000817f: 85 c0                       	testl	%eax, %eax
100008181: 75 12                       	jne	0x100008195 <_mb_win_mutex_wait_ex+0x675>
100008183: 41 bf ff ff ff ff           	movl	$0xffffffff, %r15d      ## imm = 0xFFFFFFFF
100008189: b8 08 00 00 00              	movl	$0x8, %eax
10000818e: 89 03                       	movl	%eax, (%rbx)
100008190: e9 32 fa ff ff              	jmp	0x100007bc7 <_mb_win_mutex_wait_ex+0xa7>
100008195: e8 76 04 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000819a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

00000001000081a0 <_mb_win_mutex_close_handle>:
1000081a0: 55                          	pushq	%rbp
1000081a1: 48 89 e5                    	movq	%rsp, %rbp
1000081a4: 41 57                       	pushq	%r15
1000081a6: 41 56                       	pushq	%r14
1000081a8: 41 54                       	pushq	%r12
1000081aa: 53                          	pushq	%rbx
1000081ab: 48 85 ff                    	testq	%rdi, %rdi
1000081ae: 0f 84 ae 00 00 00           	je	0x100008262 <_mb_win_mutex_close_handle+0xc2>
1000081b4: 4c 8b 37                    	movq	(%rdi), %r14
1000081b7: 4d 85 f6                    	testq	%r14, %r14
1000081ba: 0f 84 a2 00 00 00           	je	0x100008262 <_mb_win_mutex_close_handle+0xc2>
1000081c0: 49 89 f7                    	movq	%rsi, %r15
1000081c3: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000081c7: e8 54 9f ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000081cc: 4c 89 f7                    	movq	%r14, %rdi
1000081cf: e8 e4 04 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
1000081d4: 85 c0                       	testl	%eax, %eax
1000081d6: 0f 85 86 00 00 00           	jne	0x100008262 <_mb_win_mutex_close_handle+0xc2>
1000081dc: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000081e0: e8 3b 9f ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000081e5: 48 89 c3                    	movq	%rax, %rbx
1000081e8: 44 8b 20                    	movl	(%rax), %r12d
1000081eb: 49 8d 7e 68                 	leaq	0x68(%r14), %rdi
1000081ef: 90                          	nop
1000081f0: 48 89 f8                    	movq	%rdi, %rax
1000081f3: 48 8b 3f                    	movq	(%rdi), %rdi
1000081f6: 48 85 ff                    	testq	%rdi, %rdi
1000081f9: 74 47                       	je	0x100008242 <_mb_win_mutex_close_handle+0xa2>
1000081fb: 4c 39 7f 08                 	cmpq	%r15, 0x8(%rdi)
1000081ff: 75 ef                       	jne	0x1000081f0 <_mb_win_mutex_close_handle+0x50>
100008201: 48 8b 0f                    	movq	(%rdi), %rcx
100008204: 48 89 08                    	movq	%rcx, (%rax)
100008207: 4c 8b 7f 10                 	movq	0x10(%rdi), %r15
10000820b: 49 ff 4f 10                 	decq	0x10(%r15)
10000820f: e8 26 04 00 00              	callq	0x10000863a <_sysconf+0x10000863a>
100008214: 49 83 7f 10 00              	cmpq	$0x0, 0x10(%r15)
100008219: 75 12                       	jne	0x10000822d <_mb_win_mutex_close_handle+0x8d>
10000821b: 49 83 7f 18 00              	cmpq	$0x0, 0x18(%r15)
100008220: 75 0b                       	jne	0x10000822d <_mb_win_mutex_close_handle+0x8d>
100008222: 4c 89 f7                    	movq	%r14, %rdi
100008225: 4c 89 fe                    	movq	%r15, %rsi
100008228: e8 c3 ec ff ff              	callq	0x100006ef0 <_destroy_object>
10000822d: 4c 89 f7                    	movq	%r14, %rdi
100008230: e8 89 04 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
100008235: 89 c1                       	movl	%eax, %ecx
100008237: b8 01 00 00 00              	movl	$0x1, %eax
10000823c: 85 c9                       	testl	%ecx, %ecx
10000823e: 74 16                       	je	0x100008256 <_mb_win_mutex_close_handle+0xb6>
100008240: eb 20                       	jmp	0x100008262 <_mb_win_mutex_close_handle+0xc2>
100008242: 4c 89 f7                    	movq	%r14, %rdi
100008245: e8 74 04 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
10000824a: 85 c0                       	testl	%eax, %eax
10000824c: 75 14                       	jne	0x100008262 <_mb_win_mutex_close_handle+0xc2>
10000824e: 31 c0                       	xorl	%eax, %eax
100008250: 41 bc 06 00 00 00           	movl	$0x6, %r12d
100008256: 44 89 23                    	movl	%r12d, (%rbx)
100008259: 5b                          	popq	%rbx
10000825a: 41 5c                       	popq	%r12
10000825c: 41 5e                       	popq	%r14
10000825e: 41 5f                       	popq	%r15
100008260: 5d                          	popq	%rbp
100008261: c3                          	retq
100008262: e8 a9 03 00 00              	callq	0x100008610 <_sysconf+0x100008610>
100008267: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)

0000000100008270 <_mb_win_mutex_is_handle>:
100008270: 55                          	pushq	%rbp
100008271: 48 89 e5                    	movq	%rsp, %rbp
100008274: 41 57                       	pushq	%r15
100008276: 41 56                       	pushq	%r14
100008278: 53                          	pushq	%rbx
100008279: 50                          	pushq	%rax
10000827a: 45 31 ff                    	xorl	%r15d, %r15d
10000827d: 48 85 ff                    	testq	%rdi, %rdi
100008280: 74 44                       	je	0x1000082c6 <_mb_win_mutex_is_handle+0x56>
100008282: 4c 8b 37                    	movq	(%rdi), %r14
100008285: 4d 85 f6                    	testq	%r14, %r14
100008288: 74 3c                       	je	0x1000082c6 <_mb_win_mutex_is_handle+0x56>
10000828a: 48 89 f3                    	movq	%rsi, %rbx
10000828d: 4c 89 f7                    	movq	%r14, %rdi
100008290: e8 23 04 00 00              	callq	0x1000086b8 <_sysconf+0x1000086b8>
100008295: 85 c0                       	testl	%eax, %eax
100008297: 75 3b                       	jne	0x1000082d4 <_mb_win_mutex_is_handle+0x64>
100008299: 49 8d 46 68                 	leaq	0x68(%r14), %rax
10000829d: 0f 1f 00                    	nopl	(%rax)
1000082a0: 48 8b 00                    	movq	(%rax), %rax
1000082a3: 48 85 c0                    	testq	%rax, %rax
1000082a6: 74 06                       	je	0x1000082ae <_mb_win_mutex_is_handle+0x3e>
1000082a8: 48 39 58 08                 	cmpq	%rbx, 0x8(%rax)
1000082ac: 75 f2                       	jne	0x1000082a0 <_mb_win_mutex_is_handle+0x30>
1000082ae: 45 31 ff                    	xorl	%r15d, %r15d
1000082b1: 48 85 c0                    	testq	%rax, %rax
1000082b4: 0f 95 c3                    	setne	%bl
1000082b7: 4c 89 f7                    	movq	%r14, %rdi
1000082ba: e8 ff 03 00 00              	callq	0x1000086be <_sysconf+0x1000086be>
1000082bf: 85 c0                       	testl	%eax, %eax
1000082c1: 75 11                       	jne	0x1000082d4 <_mb_win_mutex_is_handle+0x64>
1000082c3: 41 88 df                    	movb	%bl, %r15b
1000082c6: 44 89 f8                    	movl	%r15d, %eax
1000082c9: 48 83 c4 08                 	addq	$0x8, %rsp
1000082cd: 5b                          	popq	%rbx
1000082ce: 41 5e                       	popq	%r14
1000082d0: 41 5f                       	popq	%r15
1000082d2: 5d                          	popq	%rbp
1000082d3: c3                          	retq
1000082d4: e8 37 03 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000082d9: 00 00                       	addb	%al, (%rax)
1000082db: 00 00                       	addb	%al, (%rax)
1000082dd: 00 00                       	addb	%al, (%rax)
1000082df: 00 55 48                    	addb	%dl, 0x48(%rbp)

00000001000082e0 <_mb_win_workers_sleep.cold.1>:
1000082e0: 55                          	pushq	%rbp
1000082e1: 48 89 e5                    	movq	%rsp, %rbp
1000082e4: 0f 10 06                    	movups	(%rsi), %xmm0
1000082e7: 0f 11 07                    	movups	%xmm0, (%rdi)
1000082ea: e8 63 03 00 00              	callq	0x100008652 <_sysconf+0x100008652>
1000082ef: 85 c0                       	testl	%eax, %eax
1000082f1: 5d                          	popq	%rbp
1000082f2: 74 03                       	je	0x1000082f7 <_mb_win_workers_sleep.cold.1+0x17>
1000082f4: 31 c0                       	xorl	%eax, %eax
1000082f6: c3                          	retq
1000082f7: b0 01                       	movb	$0x1, %al
1000082f9: c3                          	retq
1000082fa: 00 00                       	addb	%al, (%rax)
1000082fc: 00 00                       	addb	%al, (%rax)
1000082fe: 00 00                       	addb	%al, (%rax)

0000000100008300 <_mb_win_srw_close.cold.1>:
100008300: 55                          	pushq	%rbp
100008301: 48 89 e5                    	movq	%rsp, %rbp
100008304: 53                          	pushq	%rbx
100008305: 50                          	pushq	%rax
100008306: 89 fa                       	movl	%edi, %edx
100008308: 48 8b 1d 09 1d 00 00        	movq	0x1d09(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000830f: 48 8b 3b                    	movq	(%rbx), %rdi
100008312: 48 8d 35 f6 0c 00 00        	leaq	0xcf6(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008319: 31 c0                       	xorl	%eax, %eax
10000831b: e8 14 03 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008320: 48 8b 3b                    	movq	(%rbx), %rdi
100008323: e8 06 03 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008328: e8 e3 02 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000832d: 0f 1f 00                    	nopl	(%rax)

0000000100008330 <_mb_win_srw_close.cold.2>:
100008330: 55                          	pushq	%rbp
100008331: 48 89 e5                    	movq	%rsp, %rbp
100008334: 53                          	pushq	%rbx
100008335: 50                          	pushq	%rax
100008336: 89 fa                       	movl	%edi, %edx
100008338: 48 8b 1d d9 1c 00 00        	movq	0x1cd9(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000833f: 48 8b 3b                    	movq	(%rbx), %rdi
100008342: 48 8d 35 c6 0c 00 00        	leaq	0xcc6(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008349: 31 c0                       	xorl	%eax, %eax
10000834b: e8 e4 02 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008350: 48 8b 3b                    	movq	(%rbx), %rdi
100008353: e8 d6 02 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008358: e8 b3 02 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000835d: 0f 1f 00                    	nopl	(%rax)

0000000100008360 <_mb_win_srw_acquire_pin.cold.1>:
100008360: 55                          	pushq	%rbp
100008361: 48 89 e5                    	movq	%rsp, %rbp
100008364: 53                          	pushq	%rbx
100008365: 50                          	pushq	%rax
100008366: 89 fa                       	movl	%edi, %edx
100008368: 48 8b 1d a9 1c 00 00        	movq	0x1ca9(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000836f: 48 8b 3b                    	movq	(%rbx), %rdi
100008372: 48 8d 35 96 0c 00 00        	leaq	0xc96(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008379: 31 c0                       	xorl	%eax, %eax
10000837b: e8 b4 02 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008380: 48 8b 3b                    	movq	(%rbx), %rdi
100008383: e8 a6 02 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008388: e8 83 02 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000838d: 0f 1f 00                    	nopl	(%rax)

0000000100008390 <_mb_win_srw_acquire_pin.cold.2>:
100008390: 55                          	pushq	%rbp
100008391: 48 89 e5                    	movq	%rsp, %rbp
100008394: 53                          	pushq	%rbx
100008395: 50                          	pushq	%rax
100008396: 89 fa                       	movl	%edi, %edx
100008398: 48 8b 1d 79 1c 00 00        	movq	0x1c79(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000839f: 48 8b 3b                    	movq	(%rbx), %rdi
1000083a2: 48 8d 35 66 0c 00 00        	leaq	0xc66(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
1000083a9: 31 c0                       	xorl	%eax, %eax
1000083ab: e8 84 02 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000083b0: 48 8b 3b                    	movq	(%rbx), %rdi
1000083b3: e8 76 02 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000083b8: e8 53 02 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000083bd: 0f 1f 00                    	nopl	(%rax)

00000001000083c0 <_mb_win_srw_release_pin.cold.1>:
1000083c0: 55                          	pushq	%rbp
1000083c1: 48 89 e5                    	movq	%rsp, %rbp
1000083c4: 53                          	pushq	%rbx
1000083c5: 50                          	pushq	%rax
1000083c6: 89 fa                       	movl	%edi, %edx
1000083c8: 48 8b 1d 49 1c 00 00        	movq	0x1c49(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
1000083cf: 48 8b 3b                    	movq	(%rbx), %rdi
1000083d2: 48 8d 35 36 0c 00 00        	leaq	0xc36(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
1000083d9: 31 c0                       	xorl	%eax, %eax
1000083db: e8 54 02 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000083e0: 48 8b 3b                    	movq	(%rbx), %rdi
1000083e3: e8 46 02 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000083e8: e8 23 02 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000083ed: 0f 1f 00                    	nopl	(%rax)

00000001000083f0 <_mb_win_srw_release_pin.cold.2>:
1000083f0: 55                          	pushq	%rbp
1000083f1: 48 89 e5                    	movq	%rsp, %rbp
1000083f4: 53                          	pushq	%rbx
1000083f5: 50                          	pushq	%rax
1000083f6: 89 fa                       	movl	%edi, %edx
1000083f8: 48 8b 1d 19 1c 00 00        	movq	0x1c19(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
1000083ff: 48 8b 3b                    	movq	(%rbx), %rdi
100008402: 48 8d 35 06 0c 00 00        	leaq	0xc06(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008409: 31 c0                       	xorl	%eax, %eax
10000840b: e8 24 02 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008410: 48 8b 3b                    	movq	(%rbx), %rdi
100008413: e8 16 02 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008418: e8 f3 01 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000841d: 0f 1f 00                    	nopl	(%rax)

0000000100008420 <_initialize.cold.1>:
100008420: 55                          	pushq	%rbp
100008421: 48 89 e5                    	movq	%rsp, %rbp
100008424: 53                          	pushq	%rbx
100008425: 50                          	pushq	%rax
100008426: 89 fa                       	movl	%edi, %edx
100008428: 48 8b 1d e9 1b 00 00        	movq	0x1be9(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000842f: 48 8b 3b                    	movq	(%rbx), %rdi
100008432: 48 8d 35 d6 0b 00 00        	leaq	0xbd6(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008439: 31 c0                       	xorl	%eax, %eax
10000843b: e8 f4 01 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008440: 48 8b 3b                    	movq	(%rbx), %rdi
100008443: e8 e6 01 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008448: e8 c3 01 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000844d: 0f 1f 00                    	nopl	(%rax)

0000000100008450 <_mb_win_srw_acquire.cold.1>:
100008450: 55                          	pushq	%rbp
100008451: 48 89 e5                    	movq	%rsp, %rbp
100008454: 53                          	pushq	%rbx
100008455: 50                          	pushq	%rax
100008456: 89 fa                       	movl	%edi, %edx
100008458: 48 8b 1d b9 1b 00 00        	movq	0x1bb9(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000845f: 48 8b 3b                    	movq	(%rbx), %rdi
100008462: 48 8d 35 a6 0b 00 00        	leaq	0xba6(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008469: 31 c0                       	xorl	%eax, %eax
10000846b: e8 c4 01 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008470: 48 8b 3b                    	movq	(%rbx), %rdi
100008473: e8 b6 01 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008478: e8 93 01 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000847d: 0f 1f 00                    	nopl	(%rax)

0000000100008480 <_mb_win_srw_try_acquire.cold.1>:
100008480: 55                          	pushq	%rbp
100008481: 48 89 e5                    	movq	%rsp, %rbp
100008484: 53                          	pushq	%rbx
100008485: 50                          	pushq	%rax
100008486: 89 fa                       	movl	%edi, %edx
100008488: 48 8b 1d 89 1b 00 00        	movq	0x1b89(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000848f: 48 8b 3b                    	movq	(%rbx), %rdi
100008492: 48 8d 35 76 0b 00 00        	leaq	0xb76(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008499: 31 c0                       	xorl	%eax, %eax
10000849b: e8 94 01 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000084a0: 48 8b 3b                    	movq	(%rbx), %rdi
1000084a3: e8 86 01 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000084a8: e8 63 01 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000084ad: 0f 1f 00                    	nopl	(%rax)

00000001000084b0 <_mb_win_srw_release.cold.1>:
1000084b0: 55                          	pushq	%rbp
1000084b1: 48 89 e5                    	movq	%rsp, %rbp
1000084b4: 53                          	pushq	%rbx
1000084b5: 50                          	pushq	%rax
1000084b6: 89 fa                       	movl	%edi, %edx
1000084b8: 48 8b 1d 59 1b 00 00        	movq	0x1b59(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
1000084bf: 48 8b 3b                    	movq	(%rbx), %rdi
1000084c2: 48 8d 35 46 0b 00 00        	leaq	0xb46(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
1000084c9: 31 c0                       	xorl	%eax, %eax
1000084cb: e8 64 01 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000084d0: 48 8b 3b                    	movq	(%rbx), %rdi
1000084d3: e8 56 01 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000084d8: e8 33 01 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000084dd: 0f 1f 00                    	nopl	(%rax)

00000001000084e0 <_mb_win_srw_sleep.cold.1>:
1000084e0: 55                          	pushq	%rbp
1000084e1: 48 89 e5                    	movq	%rsp, %rbp
1000084e4: 53                          	pushq	%rbx
1000084e5: 50                          	pushq	%rax
1000084e6: 89 fa                       	movl	%edi, %edx
1000084e8: 48 8b 1d 29 1b 00 00        	movq	0x1b29(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
1000084ef: 48 8b 3b                    	movq	(%rbx), %rdi
1000084f2: 48 8d 35 16 0b 00 00        	leaq	0xb16(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
1000084f9: 31 c0                       	xorl	%eax, %eax
1000084fb: e8 34 01 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008500: 48 8b 3b                    	movq	(%rbx), %rdi
100008503: e8 26 01 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008508: e8 03 01 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000850d: 0f 1f 00                    	nopl	(%rax)

0000000100008510 <_mb_win_srw_sleep.cold.2>:
100008510: 55                          	pushq	%rbp
100008511: 48 89 e5                    	movq	%rsp, %rbp
100008514: 53                          	pushq	%rbx
100008515: 50                          	pushq	%rax
100008516: 89 fa                       	movl	%edi, %edx
100008518: 48 8b 1d f9 1a 00 00        	movq	0x1af9(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000851f: 48 8b 3b                    	movq	(%rbx), %rdi
100008522: 48 8d 35 e6 0a 00 00        	leaq	0xae6(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008529: 31 c0                       	xorl	%eax, %eax
10000852b: e8 04 01 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008530: 48 8b 3b                    	movq	(%rbx), %rdi
100008533: e8 f6 00 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008538: e8 d3 00 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000853d: 0f 1f 00                    	nopl	(%rax)

0000000100008540 <_mb_win_srw_sleep.cold.3>:
100008540: 55                          	pushq	%rbp
100008541: 48 89 e5                    	movq	%rsp, %rbp
100008544: 53                          	pushq	%rbx
100008545: 50                          	pushq	%rax
100008546: 89 fa                       	movl	%edi, %edx
100008548: 48 8b 1d c9 1a 00 00        	movq	0x1ac9(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000854f: 48 8b 3b                    	movq	(%rbx), %rdi
100008552: 48 8d 35 b6 0a 00 00        	leaq	0xab6(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008559: 31 c0                       	xorl	%eax, %eax
10000855b: e8 d4 00 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008560: 48 8b 3b                    	movq	(%rbx), %rdi
100008563: e8 c6 00 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008568: e8 a3 00 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000856d: 0f 1f 00                    	nopl	(%rax)

0000000100008570 <_mb_win_srw_sleep.cold.4>:
100008570: 55                          	pushq	%rbp
100008571: 48 89 e5                    	movq	%rsp, %rbp
100008574: 53                          	pushq	%rbx
100008575: 50                          	pushq	%rax
100008576: 89 fa                       	movl	%edi, %edx
100008578: 48 8b 1d 99 1a 00 00        	movq	0x1a99(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
10000857f: 48 8b 3b                    	movq	(%rbx), %rdi
100008582: 48 8d 35 86 0a 00 00        	leaq	0xa86(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
100008589: 31 c0                       	xorl	%eax, %eax
10000858b: e8 a4 00 00 00              	callq	0x100008634 <_sysconf+0x100008634>
100008590: 48 8b 3b                    	movq	(%rbx), %rdi
100008593: e8 96 00 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
100008598: e8 73 00 00 00              	callq	0x100008610 <_sysconf+0x100008610>
10000859d: 0f 1f 00                    	nopl	(%rax)

00000001000085a0 <_mb_win_srw_sleep.cold.5>:
1000085a0: 55                          	pushq	%rbp
1000085a1: 48 89 e5                    	movq	%rsp, %rbp
1000085a4: 53                          	pushq	%rbx
1000085a5: 50                          	pushq	%rax
1000085a6: 89 fa                       	movl	%edi, %edx
1000085a8: 48 8b 1d 69 1a 00 00        	movq	0x1a69(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
1000085af: 48 8b 3b                    	movq	(%rbx), %rdi
1000085b2: 48 8d 35 56 0a 00 00        	leaq	0xa56(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
1000085b9: 31 c0                       	xorl	%eax, %eax
1000085bb: e8 74 00 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000085c0: 48 8b 3b                    	movq	(%rbx), %rdi
1000085c3: e8 66 00 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000085c8: e8 43 00 00 00              	callq	0x100008610 <_sysconf+0x100008610>
1000085cd: 0f 1f 00                    	nopl	(%rax)

00000001000085d0 <_mb_win_srw_wake.cold.1>:
1000085d0: 55                          	pushq	%rbp
1000085d1: 48 89 e5                    	movq	%rsp, %rbp
1000085d4: 53                          	pushq	%rbx
1000085d5: 50                          	pushq	%rax
1000085d6: 89 fa                       	movl	%edi, %edx
1000085d8: 48 8b 1d 39 1a 00 00        	movq	0x1a39(%rip), %rbx      ## 0x10000a018 <_sysconf+0x10000a018>
1000085df: 48 8b 3b                    	movq	(%rbx), %rdi
1000085e2: 48 8d 35 26 0a 00 00        	leaq	0xa26(%rip), %rsi       ## 0x10000900f <_sysconf+0x10000900f>
1000085e9: 31 c0                       	xorl	%eax, %eax
1000085eb: e8 44 00 00 00              	callq	0x100008634 <_sysconf+0x100008634>
1000085f0: 48 8b 3b                    	movq	(%rbx), %rdi
1000085f3: e8 36 00 00 00              	callq	0x10000862e <_sysconf+0x10000862e>
1000085f8: e8 13 00 00 00              	callq	0x100008610 <_sysconf+0x100008610>

Disassembly of section __TEXT,__stubs:

00000001000085fe <__stubs>:
1000085fe: ff 25 fc 19 00 00           	jmpq	*0x19fc(%rip)           ## 0x10000a000 <_sysconf+0x10000a000>
100008604: ff 25 fe 19 00 00           	jmpq	*0x19fe(%rip)           ## 0x10000a008 <_sysconf+0x10000a008>
10000860a: ff 25 10 1a 00 00           	jmpq	*0x1a10(%rip)           ## 0x10000a020 <_sysconf+0x10000a020>
100008610: ff 25 12 1a 00 00           	jmpq	*0x1a12(%rip)           ## 0x10000a028 <_sysconf+0x10000a028>
100008616: ff 25 14 1a 00 00           	jmpq	*0x1a14(%rip)           ## 0x10000a030 <_sysconf+0x10000a030>
10000861c: ff 25 16 1a 00 00           	jmpq	*0x1a16(%rip)           ## 0x10000a038 <_sysconf+0x10000a038>
100008622: ff 25 18 1a 00 00           	jmpq	*0x1a18(%rip)           ## 0x10000a040 <_sysconf+0x10000a040>
100008628: ff 25 1a 1a 00 00           	jmpq	*0x1a1a(%rip)           ## 0x10000a048 <_sysconf+0x10000a048>
10000862e: ff 25 1c 1a 00 00           	jmpq	*0x1a1c(%rip)           ## 0x10000a050 <_sysconf+0x10000a050>
100008634: ff 25 1e 1a 00 00           	jmpq	*0x1a1e(%rip)           ## 0x10000a058 <_sysconf+0x10000a058>
10000863a: ff 25 20 1a 00 00           	jmpq	*0x1a20(%rip)           ## 0x10000a060 <_sysconf+0x10000a060>
100008640: ff 25 22 1a 00 00           	jmpq	*0x1a22(%rip)           ## 0x10000a068 <_sysconf+0x10000a068>
100008646: ff 25 24 1a 00 00           	jmpq	*0x1a24(%rip)           ## 0x10000a070 <_sysconf+0x10000a070>
10000864c: ff 25 26 1a 00 00           	jmpq	*0x1a26(%rip)           ## 0x10000a078 <_sysconf+0x10000a078>
100008652: ff 25 28 1a 00 00           	jmpq	*0x1a28(%rip)           ## 0x10000a080 <_sysconf+0x10000a080>
100008658: ff 25 2a 1a 00 00           	jmpq	*0x1a2a(%rip)           ## 0x10000a088 <_sysconf+0x10000a088>
10000865e: ff 25 2c 1a 00 00           	jmpq	*0x1a2c(%rip)           ## 0x10000a090 <_sysconf+0x10000a090>
100008664: ff 25 2e 1a 00 00           	jmpq	*0x1a2e(%rip)           ## 0x10000a098 <_sysconf+0x10000a098>
10000866a: ff 25 30 1a 00 00           	jmpq	*0x1a30(%rip)           ## 0x10000a0a0 <_sysconf+0x10000a0a0>
100008670: ff 25 32 1a 00 00           	jmpq	*0x1a32(%rip)           ## 0x10000a0a8 <_sysconf+0x10000a0a8>
100008676: ff 25 34 1a 00 00           	jmpq	*0x1a34(%rip)           ## 0x10000a0b0 <_sysconf+0x10000a0b0>
10000867c: ff 25 36 1a 00 00           	jmpq	*0x1a36(%rip)           ## 0x10000a0b8 <_sysconf+0x10000a0b8>
100008682: ff 25 38 1a 00 00           	jmpq	*0x1a38(%rip)           ## 0x10000a0c0 <_sysconf+0x10000a0c0>
100008688: ff 25 3a 1a 00 00           	jmpq	*0x1a3a(%rip)           ## 0x10000a0c8 <_sysconf+0x10000a0c8>
10000868e: ff 25 3c 1a 00 00           	jmpq	*0x1a3c(%rip)           ## 0x10000a0d0 <_sysconf+0x10000a0d0>
100008694: ff 25 3e 1a 00 00           	jmpq	*0x1a3e(%rip)           ## 0x10000a0d8 <_sysconf+0x10000a0d8>
10000869a: ff 25 40 1a 00 00           	jmpq	*0x1a40(%rip)           ## 0x10000a0e0 <_sysconf+0x10000a0e0>
1000086a0: ff 25 42 1a 00 00           	jmpq	*0x1a42(%rip)           ## 0x10000a0e8 <_sysconf+0x10000a0e8>
1000086a6: ff 25 44 1a 00 00           	jmpq	*0x1a44(%rip)           ## 0x10000a0f0 <_sysconf+0x10000a0f0>
1000086ac: ff 25 46 1a 00 00           	jmpq	*0x1a46(%rip)           ## 0x10000a0f8 <_sysconf+0x10000a0f8>
1000086b2: ff 25 48 1a 00 00           	jmpq	*0x1a48(%rip)           ## 0x10000a100 <_sysconf+0x10000a100>
1000086b8: ff 25 4a 1a 00 00           	jmpq	*0x1a4a(%rip)           ## 0x10000a108 <_sysconf+0x10000a108>
1000086be: ff 25 4c 1a 00 00           	jmpq	*0x1a4c(%rip)           ## 0x10000a110 <_sysconf+0x10000a110>
1000086c4: ff 25 4e 1a 00 00           	jmpq	*0x1a4e(%rip)           ## 0x10000a118 <_sysconf+0x10000a118>
1000086ca: ff 25 50 1a 00 00           	jmpq	*0x1a50(%rip)           ## 0x10000a120 <_sysconf+0x10000a120>
1000086d0: ff 25 52 1a 00 00           	jmpq	*0x1a52(%rip)           ## 0x10000a128 <_sysconf+0x10000a128>
1000086d6: ff 25 54 1a 00 00           	jmpq	*0x1a54(%rip)           ## 0x10000a130 <_sysconf+0x10000a130>
1000086dc: ff 25 56 1a 00 00           	jmpq	*0x1a56(%rip)           ## 0x10000a138 <_sysconf+0x10000a138>
1000086e2: ff 25 58 1a 00 00           	jmpq	*0x1a58(%rip)           ## 0x10000a140 <_sysconf+0x10000a140>
