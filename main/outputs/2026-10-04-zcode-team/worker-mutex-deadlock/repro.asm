
repro:	file format mach-o 64-bit x86-64

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
100000689: e8 54 79 00 00              	callq	0x100007fe2 <_sysconf+0x100007fe2>
10000068e: 48 8d 35 7b 0f 00 00        	leaq	0xf7b(%rip), %rsi       ## 0x100001610 <_on_alarm>
100000695: bf 0e 00 00 00              	movl	$0xe, %edi
10000069a: e8 09 7a 00 00              	callq	0x1000080a8 <_sysconf+0x1000080a8>
10000069f: 4c 8b 3d 72 89 00 00        	movq	0x8972(%rip), %r15      ## 0x100009018 <_sysconf+0x100009018>
1000006a6: 49 8b 3f                    	movq	(%r15), %rdi
1000006a9: 31 f6                       	xorl	%esi, %esi
1000006ab: e8 f2 79 00 00              	callq	0x1000080a2 <_sysconf+0x1000080a2>
1000006b0: c7 45 d4 00 00 00 00        	movl	$0x0, -0x2c(%rbp)
1000006b7: 49 8b 3f                    	movq	(%r15), %rdi
1000006ba: 48 8d 35 63 7f 00 00        	leaq	0x7f63(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
1000006c1: 48 8d 15 ec 79 00 00        	leaq	0x79ec(%rip), %rdx      ## 0x1000080b4 <_sysconf+0x1000080b4>
1000006c8: 31 c0                       	xorl	%eax, %eax
1000006ca: e8 31 79 00 00              	callq	0x100008000 <_sysconf+0x100008000>
1000006cf: 49 8b 3f                    	movq	(%r15), %rdi
1000006d2: e8 23 79 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
1000006d7: 48 8d 3d 2a 99 00 00        	leaq	0x992a(%rip), %rdi      ## 0x10000a008 <_domain>
1000006de: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
1000006e2: e8 59 13 00 00              	callq	0x100001a40 <_mb_win_threads_open>
1000006e7: 85 c0                       	testl	%eax, %eax
1000006e9: 0f 85 81 0b 00 00           	jne	0x100001270 <_main+0xc00>
1000006ef: 48 8d 3d 12 99 00 00        	leaq	0x9912(%rip), %rdi      ## 0x10000a008 <_domain>
1000006f6: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
1000006fa: e8 91 14 00 00              	callq	0x100001b90 <_mb_win_threads_enter>
1000006ff: 49 8b 3f                    	movq	(%r15), %rdi
100000702: 85 c0                       	testl	%eax, %eax
100000704: 0f 85 7c 0b 00 00           	jne	0x100001286 <_main+0xc16>
10000070a: 48 8d 35 13 7f 00 00        	leaq	0x7f13(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000711: 48 8d 15 ca 79 00 00        	leaq	0x79ca(%rip), %rdx      ## 0x1000080e2 <_sysconf+0x1000080e2>
100000718: 31 c0                       	xorl	%eax, %eax
10000071a: e8 e1 78 00 00              	callq	0x100008000 <_sysconf+0x100008000>
10000071f: 49 8b 3f                    	movq	(%r15), %rdi
100000722: e8 d3 78 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000727: 48 8b 05 3a 8a 00 00        	movq	0x8a3a(%rip), %rax      ## 0x100009168 <_sysconf+0x100009168>
10000072e: 48 89 85 70 ff ff ff        	movq	%rax, -0x90(%rbp)
100000735: 0f 10 05 1c 8a 00 00        	movups	0x8a1c(%rip), %xmm0     ## 0x100009158 <_sysconf+0x100009158>
10000073c: 0f 29 85 60 ff ff ff        	movaps	%xmm0, -0xa0(%rbp)
100000743: 0f 10 05 fe 89 00 00        	movups	0x89fe(%rip), %xmm0     ## 0x100009148 <_sysconf+0x100009148>
10000074a: 0f 29 85 50 ff ff ff        	movaps	%xmm0, -0xb0(%rbp)
100000751: 48 8d 3d b8 98 00 00        	leaq	0x98b8(%rip), %rdi      ## 0x10000a010 <_workers>
100000758: 48 8d b5 50 ff ff ff        	leaq	-0xb0(%rbp), %rsi
10000075f: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
100000763: e8 d8 1d 00 00              	callq	0x100002540 <_mb_win_workers_open>
100000768: 85 c0                       	testl	%eax, %eax
10000076a: 0f 85 29 0b 00 00           	jne	0x100001299 <_main+0xc29>
100000770: 48 8d 3d a1 98 00 00        	leaq	0x98a1(%rip), %rdi      ## 0x10000a018 <_critical>
100000777: 48 8d 35 8a 98 00 00        	leaq	0x988a(%rip), %rsi      ## 0x10000a008 <_domain>
10000077e: 48 8d 4d d4                 	leaq	-0x2c(%rbp), %rcx
100000782: ba 10 00 00 00              	movl	$0x10, %edx
100000787: e8 24 30 00 00              	callq	0x1000037b0 <_mb_win_critical_open>
10000078c: 85 c0                       	testl	%eax, %eax
10000078e: 0f 85 1b 0b 00 00           	jne	0x1000012af <_main+0xc3f>
100000794: 48 8d 3d 85 98 00 00        	leaq	0x9885(%rip), %rdi      ## 0x10000a020 <_address_wait>
10000079b: 48 8d 35 66 98 00 00        	leaq	0x9866(%rip), %rsi      ## 0x10000a008 <_domain>
1000007a2: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
1000007a6: e8 15 3b 00 00              	callq	0x1000042c0 <_mb_win_address_wait_open>
1000007ab: 85 c0                       	testl	%eax, %eax
1000007ad: 0f 85 12 0b 00 00           	jne	0x1000012c5 <_main+0xc55>
1000007b3: 0f 10 05 b6 89 00 00        	movups	0x89b6(%rip), %xmm0     ## 0x100009170 <_sysconf+0x100009170>
1000007ba: 0f 29 45 80                 	movaps	%xmm0, -0x80(%rbp)
1000007be: 48 8d 3d 63 98 00 00        	leaq	0x9863(%rip), %rdi      ## 0x10000a028 <_srw>
1000007c5: 48 8d 75 80                 	leaq	-0x80(%rbp), %rsi
1000007c9: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
1000007cd: e8 6e 44 00 00              	callq	0x100004c40 <_mb_win_srw_open>
1000007d2: 85 c0                       	testl	%eax, %eax
1000007d4: 0f 85 01 0b 00 00           	jne	0x1000012db <_main+0xc6b>
1000007da: 0f 10 05 9f 89 00 00        	movups	0x899f(%rip), %xmm0     ## 0x100009180 <_sysconf+0x100009180>
1000007e1: 0f 29 45 90                 	movaps	%xmm0, -0x70(%rbp)
1000007e5: 48 8d 3d 44 98 00 00        	leaq	0x9844(%rip), %rdi      ## 0x10000a030 <_init_once>
1000007ec: 48 8d 75 90                 	leaq	-0x70(%rbp), %rsi
1000007f0: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
1000007f4: e8 27 54 00 00              	callq	0x100005c20 <_mb_win_init_once_open>
1000007f9: 85 c0                       	testl	%eax, %eax
1000007fb: 0f 85 f0 0a 00 00           	jne	0x1000012f1 <_main+0xc81>
100000801: 0f 10 05 88 89 00 00        	movups	0x8988(%rip), %xmm0     ## 0x100009190 <_sysconf+0x100009190>
100000808: 0f 29 45 a0                 	movaps	%xmm0, -0x60(%rbp)
10000080c: 48 8d 3d 25 98 00 00        	leaq	0x9825(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000813: 48 8d 75 a0                 	leaq	-0x60(%rbp), %rsi
100000817: 48 8d 55 d4                 	leaq	-0x2c(%rbp), %rdx
10000081b: e8 30 60 00 00              	callq	0x100006850 <_mb_win_mutex_open>
100000820: 49 8b 3f                    	movq	(%r15), %rdi
100000823: 85 c0                       	testl	%eax, %eax
100000825: 0f 85 dc 0a 00 00           	jne	0x100001307 <_main+0xc97>
10000082b: 48 8d 35 f2 7d 00 00        	leaq	0x7df2(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000832: 48 8d 15 3d 79 00 00        	leaq	0x793d(%rip), %rdx      ## 0x100008176 <_sysconf+0x100008176>
100000839: 31 c0                       	xorl	%eax, %eax
10000083b: e8 c0 77 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000840: 49 8b 3f                    	movq	(%r15), %rdi
100000843: e8 b2 77 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000848: 0f 57 c0                    	xorps	%xmm0, %xmm0
10000084b: 0f 11 05 ee 97 00 00        	movups	%xmm0, 0x97ee(%rip)     ## 0x10000a040 <_S>
100000852: 0f 11 05 f7 97 00 00        	movups	%xmm0, 0x97f7(%rip)     ## 0x10000a050 <_S+0x10>
100000859: 0f 11 05 00 98 00 00        	movups	%xmm0, 0x9800(%rip)     ## 0x10000a060 <_S+0x20>
100000860: 0f 11 05 09 98 00 00        	movups	%xmm0, 0x9809(%rip)     ## 0x10000a070 <_S+0x30>
100000867: 0f 11 05 12 98 00 00        	movups	%xmm0, 0x9812(%rip)     ## 0x10000a080 <_S+0x40>
10000086e: 0f 11 05 1b 98 00 00        	movups	%xmm0, 0x981b(%rip)     ## 0x10000a090 <_S+0x50>
100000875: 48 8d 35 c4 97 00 00        	leaq	0x97c4(%rip), %rsi      ## 0x10000a040 <_S>
10000087c: 48 8d 3d 95 97 00 00        	leaq	0x9795(%rip), %rdi      ## 0x10000a018 <_critical>
100000883: ba 0b 00 00 00              	movl	$0xb, %edx
100000888: e8 d3 33 00 00              	callq	0x100003c60 <_mb_win_critical_initialize>
10000088d: 85 c0                       	testl	%eax, %eax
10000088f: 0f 85 85 0a 00 00           	jne	0x10000131a <_main+0xcaa>
100000895: 48 8d 3d 7c 97 00 00        	leaq	0x977c(%rip), %rdi      ## 0x10000a018 <_critical>
10000089c: 48 8d 35 9d 97 00 00        	leaq	0x979d(%rip), %rsi      ## 0x10000a040 <_S>
1000008a3: e8 a8 35 00 00              	callq	0x100003e50 <_mb_win_critical_enter>
1000008a8: 85 c0                       	testl	%eax, %eax
1000008aa: 0f 85 80 0a 00 00           	jne	0x100001330 <_main+0xcc0>
1000008b0: 48 8d 1d 71 97 00 00        	leaq	0x9771(%rip), %rbx      ## 0x10000a028 <_srw>
1000008b7: 48 8d 35 aa 97 00 00        	leaq	0x97aa(%rip), %rsi      ## 0x10000a068 <_S+0x28>
1000008be: 48 89 df                    	movq	%rbx, %rdi
1000008c1: e8 ca 46 00 00              	callq	0x100004f90 <_mb_win_srw_initialize>
1000008c6: 48 8d 35 a3 97 00 00        	leaq	0x97a3(%rip), %rsi      ## 0x10000a070 <_S+0x30>
1000008cd: 48 89 df                    	movq	%rbx, %rdi
1000008d0: e8 3b 47 00 00              	callq	0x100005010 <_mb_win_srw_condition_initialize>
1000008d5: 49 8b 3f                    	movq	(%r15), %rdi
1000008d8: 48 8d 35 45 7d 00 00        	leaq	0x7d45(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
1000008df: 48 8d 15 df 78 00 00        	leaq	0x78df(%rip), %rdx      ## 0x1000081c5 <_sysconf+0x1000081c5>
1000008e6: 31 c0                       	xorl	%eax, %eax
1000008e8: e8 13 77 00 00              	callq	0x100008000 <_sysconf+0x100008000>
1000008ed: 49 8b 3f                    	movq	(%r15), %rdi
1000008f0: e8 05 77 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
1000008f5: c7 45 d0 00 00 00 00        	movl	$0x0, -0x30(%rbp)
1000008fc: 48 c7 45 c0 00 00 00 00     	movq	$0x0, -0x40(%rbp)
100000904: 48 8d 3d 25 97 00 00        	leaq	0x9725(%rip), %rdi      ## 0x10000a030 <_init_once>
10000090b: 48 8d 35 66 97 00 00        	leaq	0x9766(%rip), %rsi      ## 0x10000a078 <_S+0x38>
100000912: 48 8d 4d d0                 	leaq	-0x30(%rbp), %rcx
100000916: 4c 8d 45 c0                 	leaq	-0x40(%rbp), %r8
10000091a: 31 d2                       	xorl	%edx, %edx
10000091c: e8 cf 56 00 00              	callq	0x100005ff0 <_mb_win_init_once_begin>
100000921: 85 c0                       	testl	%eax, %eax
100000923: 0f 84 1d 0a 00 00           	je	0x100001346 <_main+0xcd6>
100000929: 83 7d d0 01                 	cmpl	$0x1, -0x30(%rbp)
10000092d: 0f 85 13 0a 00 00           	jne	0x100001346 <_main+0xcd6>
100000933: 48 bb 01 00 5a 5a ef be d1 51       	movabsq	$0x51d1beef5a5a0001, %rbx ## imm = 0x51D1BEEF5A5A0001
10000093d: 48 8d 3d ec 96 00 00        	leaq	0x96ec(%rip), %rdi      ## 0x10000a030 <_init_once>
100000944: 48 8d 35 2d 97 00 00        	leaq	0x972d(%rip), %rsi      ## 0x10000a078 <_S+0x38>
10000094b: 31 d2                       	xorl	%edx, %edx
10000094d: 48 89 d9                    	movq	%rbx, %rcx
100000950: e8 0b 5c 00 00              	callq	0x100006560 <_mb_win_init_once_complete>
100000955: 85 c0                       	testl	%eax, %eax
100000957: 0f 84 ff 09 00 00           	je	0x10000135c <_main+0xcec>
10000095d: 48 8d 3d cc 96 00 00        	leaq	0x96cc(%rip), %rdi      ## 0x10000a030 <_init_once>
100000964: 48 8d 35 0d 97 00 00        	leaq	0x970d(%rip), %rsi      ## 0x10000a078 <_S+0x38>
10000096b: 48 8d 4d d0                 	leaq	-0x30(%rbp), %rcx
10000096f: 4c 8d 45 c0                 	leaq	-0x40(%rbp), %r8
100000973: ba 01 00 00 00              	movl	$0x1, %edx
100000978: e8 73 56 00 00              	callq	0x100005ff0 <_mb_win_init_once_begin>
10000097d: 85 c0                       	testl	%eax, %eax
10000097f: 0f 84 ed 09 00 00           	je	0x100001372 <_main+0xd02>
100000985: 83 7d d0 00                 	cmpl	$0x0, -0x30(%rbp)
100000989: 0f 85 e3 09 00 00           	jne	0x100001372 <_main+0xd02>
10000098f: 48 39 5d c0                 	cmpq	%rbx, -0x40(%rbp)
100000993: 0f 85 d9 09 00 00           	jne	0x100001372 <_main+0xd02>
100000999: 48 8d 3d 90 96 00 00        	leaq	0x9690(%rip), %rdi      ## 0x10000a030 <_init_once>
1000009a0: 48 8d 35 d1 96 00 00        	leaq	0x96d1(%rip), %rsi      ## 0x10000a078 <_S+0x38>
1000009a7: 31 d2                       	xorl	%edx, %edx
1000009a9: 48 89 d9                    	movq	%rbx, %rcx
1000009ac: e8 af 5b 00 00              	callq	0x100006560 <_mb_win_init_once_complete>
1000009b1: 85 c0                       	testl	%eax, %eax
1000009b3: 0f 85 cf 09 00 00           	jne	0x100001388 <_main+0xd18>
1000009b9: 48 8d 3d 70 96 00 00        	leaq	0x9670(%rip), %rdi      ## 0x10000a030 <_init_once>
1000009c0: 48 8d 35 b9 96 00 00        	leaq	0x96b9(%rip), %rsi      ## 0x10000a080 <_S+0x40>
1000009c7: 48 8d 5d d0                 	leaq	-0x30(%rbp), %rbx
1000009cb: 4c 8d 45 c0                 	leaq	-0x40(%rbp), %r8
1000009cf: ba 01 00 00 00              	movl	$0x1, %edx
1000009d4: 48 89 d9                    	movq	%rbx, %rcx
1000009d7: e8 14 56 00 00              	callq	0x100005ff0 <_mb_win_init_once_begin>
1000009dc: 85 c0                       	testl	%eax, %eax
1000009de: 0f 85 1e 08 00 00           	jne	0x100001202 <_main+0xb92>
1000009e4: 48 8d 3d 1d 96 00 00        	leaq	0x961d(%rip), %rdi      ## 0x10000a008 <_domain>
1000009eb: e8 30 17 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
1000009f0: 81 38 e5 03 00 00           	cmpl	$0x3e5, (%rax)          ## imm = 0x3E5
1000009f6: 0f 85 06 08 00 00           	jne	0x100001202 <_main+0xb92>
1000009fc: 48 c7 05 71 96 00 00 00 00 00 00    	movq	$0x0, 0x9671(%rip) ## 0x10000a078 <_S+0x38>
100000a07: 49 8b 3f                    	movq	(%r15), %rdi
100000a0a: 48 8d 35 13 7c 00 00        	leaq	0x7c13(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000a11: 48 8d 15 02 78 00 00        	leaq	0x7802(%rip), %rdx      ## 0x10000821a <_sysconf+0x10000821a>
100000a18: 31 c0                       	xorl	%eax, %eax
100000a1a: e8 e1 75 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000a1f: 49 8b 3f                    	movq	(%r15), %rdi
100000a22: e8 d3 75 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000a27: 48 8d 3d da 95 00 00        	leaq	0x95da(%rip), %rdi      ## 0x10000a008 <_domain>
100000a2e: e8 ed 16 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000a33: c7 00 ce 8a 46 c2           	movl	$0xc2468ace, (%rax)     ## imm = 0xC2468ACE
100000a39: 48 8d 3d f8 95 00 00        	leaq	0x95f8(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000a40: 31 f6                       	xorl	%esi, %esi
100000a42: 31 d2                       	xorl	%edx, %edx
100000a44: e8 57 63 00 00              	callq	0x100006da0 <_mb_win_mutex_create>
100000a49: 48 85 c0                    	testq	%rax, %rax
100000a4c: 0f 84 4c 09 00 00           	je	0x10000139e <_main+0xd2e>
100000a52: 49 89 c6                    	movq	%rax, %r14
100000a55: 48 8d 3d dc 95 00 00        	leaq	0x95dc(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000a5c: 48 89 c6                    	movq	%rax, %rsi
100000a5f: ba 01 00 00 00              	movl	$0x1, %edx
100000a64: 31 c9                       	xorl	%ecx, %ecx
100000a66: e8 85 6d 00 00              	callq	0x1000077f0 <_mb_win_mutex_wait_ex>
100000a6b: 85 c0                       	testl	%eax, %eax
100000a6d: 0f 85 41 09 00 00           	jne	0x1000013b4 <_main+0xd44>
100000a73: 48 8d 3d be 95 00 00        	leaq	0x95be(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000a7a: 4c 89 f6                    	movq	%r14, %rsi
100000a7d: e8 3e 6b 00 00              	callq	0x1000075c0 <_mb_win_mutex_release>
100000a82: 83 f8 01                    	cmpl	$0x1, %eax
100000a85: 0f 85 3f 09 00 00           	jne	0x1000013ca <_main+0xd5a>
100000a8b: 48 8d 3d a6 95 00 00        	leaq	0x95a6(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000a92: 4c 89 f6                    	movq	%r14, %rsi
100000a95: 31 d2                       	xorl	%edx, %edx
100000a97: b9 01 00 00 00              	movl	$0x1, %ecx
100000a9c: e8 4f 6d 00 00              	callq	0x1000077f0 <_mb_win_mutex_wait_ex>
100000aa1: 83 f8 ff                    	cmpl	$-0x1, %eax
100000aa4: 0f 85 6e 07 00 00           	jne	0x100001218 <_main+0xba8>
100000aaa: 48 8d 3d 57 95 00 00        	leaq	0x9557(%rip), %rdi      ## 0x10000a008 <_domain>
100000ab1: e8 6a 16 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000ab6: 83 38 32                    	cmpl	$0x32, (%rax)
100000ab9: 0f 85 59 07 00 00           	jne	0x100001218 <_main+0xba8>
100000abf: 48 8d 3d 72 95 00 00        	leaq	0x9572(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000ac6: 48 8d 35 73 7d 00 00        	leaq	0x7d73(%rip), %rsi      ## 0x100008840 <_named>
100000acd: 31 d2                       	xorl	%edx, %edx
100000acf: e8 cc 62 00 00              	callq	0x100006da0 <_mb_win_mutex_create>
100000ad4: 48 85 c0                    	testq	%rax, %rax
100000ad7: 0f 84 03 09 00 00           	je	0x1000013e0 <_main+0xd70>
100000add: 48 8d 3d 54 95 00 00        	leaq	0x9554(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000ae4: 48 8d 35 75 7d 00 00        	leaq	0x7d75(%rip), %rsi      ## 0x100008860 <_named_folded>
100000aeb: e8 20 69 00 00              	callq	0x100007410 <_mb_win_mutex_open_named>
100000af0: 48 85 c0                    	testq	%rax, %rax
100000af3: 0f 84 fd 08 00 00           	je	0x1000013f6 <_main+0xd86>
100000af9: 48 8d 3d 38 95 00 00        	leaq	0x9538(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000b00: 48 8d 35 39 7d 00 00        	leaq	0x7d39(%rip), %rsi      ## 0x100008840 <_named>
100000b07: ba 01 00 00 00              	movl	$0x1, %edx
100000b0c: e8 8f 62 00 00              	callq	0x100006da0 <_mb_win_mutex_create>
100000b11: 48 85 c0                    	testq	%rax, %rax
100000b14: 0f 84 14 07 00 00           	je	0x10000122e <_main+0xbbe>
100000b1a: 49 89 c6                    	movq	%rax, %r14
100000b1d: 48 8d 3d e4 94 00 00        	leaq	0x94e4(%rip), %rdi      ## 0x10000a008 <_domain>
100000b24: e8 f7 15 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000b29: 81 38 b7 00 00 00           	cmpl	$0xb7, (%rax)
100000b2f: 0f 85 f9 06 00 00           	jne	0x10000122e <_main+0xbbe>
100000b35: 48 8d 3d fc 94 00 00        	leaq	0x94fc(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000b3c: 4c 89 f6                    	movq	%r14, %rsi
100000b3f: e8 7c 6a 00 00              	callq	0x1000075c0 <_mb_win_mutex_release>
100000b44: 85 c0                       	testl	%eax, %eax
100000b46: 0f 85 f8 06 00 00           	jne	0x100001244 <_main+0xbd4>
100000b4c: 48 8d 3d b5 94 00 00        	leaq	0x94b5(%rip), %rdi      ## 0x10000a008 <_domain>
100000b53: e8 c8 15 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000b58: 81 38 20 01 00 00           	cmpl	$0x120, (%rax)          ## imm = 0x120
100000b5e: 0f 85 e0 06 00 00           	jne	0x100001244 <_main+0xbd4>
100000b64: 4c 8d 35 cd 94 00 00        	leaq	0x94cd(%rip), %r14      ## 0x10000a038 <_mutex_owner>
100000b6b: 48 8d 35 ee 7c 00 00        	leaq	0x7cee(%rip), %rsi      ## 0x100008860 <_named_folded>
100000b72: 4c 89 f7                    	movq	%r14, %rdi
100000b75: e8 96 68 00 00              	callq	0x100007410 <_mb_win_mutex_open_named>
100000b7a: 4c 89 f7                    	movq	%r14, %rdi
100000b7d: 48 89 c6                    	movq	%rax, %rsi
100000b80: 31 d2                       	xorl	%edx, %edx
100000b82: 31 c9                       	xorl	%ecx, %ecx
100000b84: e8 67 6c 00 00              	callq	0x1000077f0 <_mb_win_mutex_wait_ex>
100000b89: 85 c0                       	testl	%eax, %eax
100000b8b: 0f 85 7b 08 00 00           	jne	0x10000140c <_main+0xd9c>
100000b91: 48 8d 3d a0 94 00 00        	leaq	0x94a0(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000b98: 48 8d 35 e1 7c 00 00        	leaq	0x7ce1(%rip), %rsi      ## 0x100008880 <_backslash>
100000b9f: 31 d2                       	xorl	%edx, %edx
100000ba1: e8 fa 61 00 00              	callq	0x100006da0 <_mb_win_mutex_create>
100000ba6: 48 85 c0                    	testq	%rax, %rax
100000ba9: 0f 85 ab 06 00 00           	jne	0x10000125a <_main+0xbea>
100000baf: 48 8d 3d 52 94 00 00        	leaq	0x9452(%rip), %rdi      ## 0x10000a008 <_domain>
100000bb6: e8 65 15 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000bbb: 83 38 57                    	cmpl	$0x57, (%rax)
100000bbe: 0f 85 96 06 00 00           	jne	0x10000125a <_main+0xbea>
100000bc4: 49 8b 3f                    	movq	(%r15), %rdi
100000bc7: 48 8d 35 56 7a 00 00        	leaq	0x7a56(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000bce: 48 8d 15 79 76 00 00        	leaq	0x7679(%rip), %rdx      ## 0x10000824e <_sysconf+0x10000824e>
100000bd5: 31 c0                       	xorl	%eax, %eax
100000bd7: e8 24 74 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000bdc: 49 8b 3f                    	movq	(%r15), %rdi
100000bdf: e8 16 74 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000be4: 48 8d 3d 25 94 00 00        	leaq	0x9425(%rip), %rdi      ## 0x10000a010 <_workers>
100000beb: e8 d0 1d 00 00              	callq	0x1000029c0 <_mb_win_workers_enter_call>
100000bf0: 85 c0                       	testl	%eax, %eax
100000bf2: 0f 85 2a 08 00 00           	jne	0x100001422 <_main+0xdb2>
100000bf8: c7 45 d0 00 00 00 00        	movl	$0x0, -0x30(%rbp)
100000bff: 48 89 1c 24                 	movq	%rbx, (%rsp)
100000c03: 48 8d 3d 06 94 00 00        	leaq	0x9406(%rip), %rdi      ## 0x10000a010 <_workers>
100000c0a: 48 8d 0d 6f 0a 00 00        	leaq	0xa6f(%rip), %rcx       ## 0x100001680 <_worker_start>
100000c11: 4c 8d 05 28 94 00 00        	leaq	0x9428(%rip), %r8       ## 0x10000a040 <_S>
100000c18: 31 db                       	xorl	%ebx, %ebx
100000c1a: 31 f6                       	xorl	%esi, %esi
100000c1c: 31 d2                       	xorl	%edx, %edx
100000c1e: 45 31 c9                    	xorl	%r9d, %r9d
100000c21: e8 7a 21 00 00              	callq	0x100002da0 <_mb_win_workers_create>
100000c26: 48 89 05 73 94 00 00        	movq	%rax, 0x9473(%rip)      ## 0x10000a0a0 <_thread_handle>
100000c2d: 48 85 c0                    	testq	%rax, %rax
100000c30: 0f 84 02 08 00 00           	je	0x100001438 <_main+0xdc8>
100000c36: 48 8d 3d eb 93 00 00        	leaq	0x93eb(%rip), %rdi      ## 0x10000a028 <_srw>
100000c3d: 48 8d 35 24 94 00 00        	leaq	0x9424(%rip), %rsi      ## 0x10000a068 <_S+0x28>
100000c44: ba 01 00 00 00              	movl	$0x1, %edx
100000c49: e8 d2 43 00 00              	callq	0x100005020 <_mb_win_srw_acquire>
100000c4e: 8b 05 38 94 00 00           	movl	0x9438(%rip), %eax      ## 0x10000a08c <_S+0x4c>
100000c54: 85 c0                       	testl	%eax, %eax
100000c56: 0f 84 02 05 00 00           	je	0x10000115e <_main+0xaee>
100000c5c: 48 8d 3d c5 93 00 00        	leaq	0x93c5(%rip), %rdi      ## 0x10000a028 <_srw>
100000c63: 48 8d 35 fe 93 00 00        	leaq	0x93fe(%rip), %rsi      ## 0x10000a068 <_S+0x28>
100000c6a: ba 01 00 00 00              	movl	$0x1, %edx
100000c6f: e8 bc 47 00 00              	callq	0x100005430 <_mb_win_srw_release>
100000c74: 48 8d 35 c5 93 00 00        	leaq	0x93c5(%rip), %rsi      ## 0x10000a040 <_S>
100000c7b: 48 8d 3d 96 93 00 00        	leaq	0x9396(%rip), %rdi      ## 0x10000a018 <_critical>
100000c82: e8 b9 34 00 00              	callq	0x100004140 <_mb_win_critical_leave>
100000c87: 4c 8b 2d 8a 83 00 00        	movq	0x838a(%rip), %r13      ## 0x100009018 <_sysconf+0x100009018>
100000c8e: 49 8b 7d 00                 	movq	(%r13), %rdi
100000c92: 48 8d 35 11 76 00 00        	leaq	0x7611(%rip), %rsi      ## 0x1000082aa <_sysconf+0x1000082aa>
100000c99: 89 da                       	movl	%ebx, %edx
100000c9b: 31 c0                       	xorl	%eax, %eax
100000c9d: e8 5e 73 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000ca2: 48 8b 35 f7 93 00 00        	movq	0x93f7(%rip), %rsi      ## 0x10000a0a0 <_thread_handle>
100000ca9: 48 8d 3d 60 93 00 00        	leaq	0x9360(%rip), %rdi      ## 0x10000a010 <_workers>
100000cb0: 31 d2                       	xorl	%edx, %edx
100000cb2: e8 f9 24 00 00              	callq	0x1000031b0 <_mb_win_workers_wait>
100000cb7: 3d 02 01 00 00              	cmpl	$0x102, %eax            ## imm = 0x102
100000cbc: 0f 85 93 07 00 00           	jne	0x100001455 <_main+0xde5>
100000cc2: c7 45 cc 00 00 00 00        	movl	$0x0, -0x34(%rbp)
100000cc9: 48 8b 35 d0 93 00 00        	movq	0x93d0(%rip), %rsi      ## 0x10000a0a0 <_thread_handle>
100000cd0: 48 8d 3d 39 93 00 00        	leaq	0x9339(%rip), %rdi      ## 0x10000a010 <_workers>
100000cd7: 48 8d 55 cc                 	leaq	-0x34(%rbp), %rdx
100000cdb: e8 60 28 00 00              	callq	0x100003540 <_mb_win_workers_exit_code>
100000ce0: 85 c0                       	testl	%eax, %eax
100000ce2: 0f 84 84 07 00 00           	je	0x10000146c <_main+0xdfc>
100000ce8: 81 7d cc 03 01 00 00        	cmpl	$0x103, -0x34(%rbp)     ## imm = 0x103
100000cef: 0f 85 77 07 00 00           	jne	0x10000146c <_main+0xdfc>
100000cf5: 48 8d 1d 14 93 00 00        	leaq	0x9314(%rip), %rbx      ## 0x10000a010 <_workers>
100000cfc: 48 89 df                    	movq	%rbx, %rdi
100000cff: 31 f6                       	xorl	%esi, %esi
100000d01: e8 aa 29 00 00              	callq	0x1000036b0 <_mb_win_workers_sleep>
100000d06: 49 8b 7d 00                 	movq	(%r13), %rdi
100000d0a: 4c 8d 35 13 79 00 00        	leaq	0x7913(%rip), %r14      ## 0x100008624 <_sysconf+0x100008624>
100000d11: 48 8d 15 d0 75 00 00        	leaq	0x75d0(%rip), %rdx      ## 0x1000082e8 <_sysconf+0x1000082e8>
100000d18: 4c 89 f6                    	movq	%r14, %rsi
100000d1b: 31 c0                       	xorl	%eax, %eax
100000d1d: e8 de 72 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000d22: 49 8b 7d 00                 	movq	(%r13), %rdi
100000d26: e8 cf 72 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000d2b: b8 01 00 00 00              	movl	$0x1, %eax
100000d30: 87 05 52 93 00 00           	xchgl	%eax, 0x9352(%rip)      ## 0x10000a088 <_S+0x48>
100000d36: 4c 8d 3d 4b 93 00 00        	leaq	0x934b(%rip), %r15      ## 0x10000a088 <_S+0x48>
100000d3d: 4c 8d 25 dc 92 00 00        	leaq	0x92dc(%rip), %r12      ## 0x10000a020 <_address_wait>
100000d44: 4c 89 e7                    	movq	%r12, %rdi
100000d47: 4c 89 fe                    	movq	%r15, %rsi
100000d4a: e8 01 3e 00 00              	callq	0x100004b50 <_mb_win_address_wait_wake_single>
100000d4f: 4c 89 e7                    	movq	%r12, %rdi
100000d52: 4c 89 fe                    	movq	%r15, %rsi
100000d55: e8 d6 3e 00 00              	callq	0x100004c30 <_mb_win_address_wait_wake_all>
100000d5a: 48 8d 3d a7 92 00 00        	leaq	0x92a7(%rip), %rdi      ## 0x10000a008 <_domain>
100000d61: e8 ba 13 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100000d66: c7 00 ce 8a 46 c2           	movl	$0xc2468ace, (%rax)     ## imm = 0xC2468ACE
100000d6c: 49 8b 7d 00                 	movq	(%r13), %rdi
100000d70: 48 8d 15 8d 75 00 00        	leaq	0x758d(%rip), %rdx      ## 0x100008304 <_sysconf+0x100008304>
100000d77: 4c 89 f6                    	movq	%r14, %rsi
100000d7a: 31 c0                       	xorl	%eax, %eax
100000d7c: e8 7f 72 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000d81: 49 8b 7d 00                 	movq	(%r13), %rdi
100000d85: e8 70 72 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000d8a: 48 8b 35 0f 93 00 00        	movq	0x930f(%rip), %rsi      ## 0x10000a0a0 <_thread_handle>
100000d91: 48 89 df                    	movq	%rbx, %rdi
100000d94: ba ff ff ff ff              	movl	$0xffffffff, %edx       ## imm = 0xFFFFFFFF
100000d99: e8 12 24 00 00              	callq	0x1000031b0 <_mb_win_workers_wait>
100000d9e: 49 8b 7d 00                 	movq	(%r13), %rdi
100000da2: 85 c0                       	testl	%eax, %eax
100000da4: 0f 85 d9 06 00 00           	jne	0x100001483 <_main+0xe13>
100000daa: 48 8d 35 73 78 00 00        	leaq	0x7873(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000db1: 48 8d 15 80 75 00 00        	leaq	0x7580(%rip), %rdx      ## 0x100008338 <_sysconf+0x100008338>
100000db8: 31 c0                       	xorl	%eax, %eax
100000dba: e8 41 72 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000dbf: 49 8b 7d 00                 	movq	(%r13), %rdi
100000dc3: e8 32 72 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000dc8: 48 8b 35 d1 92 00 00        	movq	0x92d1(%rip), %rsi      ## 0x10000a0a0 <_thread_handle>
100000dcf: 48 8d 3d 3a 92 00 00        	leaq	0x923a(%rip), %rdi      ## 0x10000a010 <_workers>
100000dd6: 48 8d 55 cc                 	leaq	-0x34(%rbp), %rdx
100000dda: e8 61 27 00 00              	callq	0x100003540 <_mb_win_workers_exit_code>
100000ddf: 49 8b 5d 00                 	movq	(%r13), %rbx
100000de3: 85 c0                       	testl	%eax, %eax
100000de5: 0f 84 ab 06 00 00           	je	0x100001496 <_main+0xe26>
100000deb: 44 8b 75 cc                 	movl	-0x34(%rbp), %r14d
100000def: 41 81 fe 67 45 23 81        	cmpl	$0x81234567, %r14d      ## imm = 0x81234567
100000df6: 0f 85 9a 06 00 00           	jne	0x100001496 <_main+0xe26>
100000dfc: 44 8b 3d 99 92 00 00        	movl	0x9299(%rip), %r15d     ## 0x10000a09c <_S+0x5c>
100000e03: 48 8b 35 96 92 00 00        	movq	0x9296(%rip), %rsi      ## 0x10000a0a0 <_thread_handle>
100000e0a: 4c 8d 25 ff 91 00 00        	leaq	0x91ff(%rip), %r12      ## 0x10000a010 <_workers>
100000e11: 4c 89 e7                    	movq	%r12, %rdi
100000e14: 31 d2                       	xorl	%edx, %edx
100000e16: e8 95 23 00 00              	callq	0x1000031b0 <_mb_win_workers_wait>
100000e1b: 48 8d 35 49 75 00 00        	leaq	0x7549(%rip), %rsi      ## 0x10000836b <_sysconf+0x10000836b>
100000e22: 48 89 df                    	movq	%rbx, %rdi
100000e25: 44 89 f2                    	movl	%r14d, %edx
100000e28: 44 89 f9                    	movl	%r15d, %ecx
100000e2b: 41 89 c0                    	movl	%eax, %r8d
100000e2e: 31 c0                       	xorl	%eax, %eax
100000e30: e8 cb 71 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000e35: 48 8b 35 64 92 00 00        	movq	0x9264(%rip), %rsi      ## 0x10000a0a0 <_thread_handle>
100000e3c: 4c 89 e7                    	movq	%r12, %rdi
100000e3f: e8 bc 27 00 00              	callq	0x100003600 <_mb_win_workers_close_handle>
100000e44: 49 8b 7d 00                 	movq	(%r13), %rdi
100000e48: 85 c0                       	testl	%eax, %eax
100000e4a: 0f 84 5c 06 00 00           	je	0x1000014ac <_main+0xe3c>
100000e50: 48 8d 35 cd 77 00 00        	leaq	0x77cd(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000e57: 48 8d 15 47 75 00 00        	leaq	0x7547(%rip), %rdx      ## 0x1000083a5 <_sysconf+0x1000083a5>
100000e5e: 31 c0                       	xorl	%eax, %eax
100000e60: e8 9b 71 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000e65: 49 8b 7d 00                 	movq	(%r13), %rdi
100000e69: e8 8c 71 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000e6e: 48 8d 3d a3 91 00 00        	leaq	0x91a3(%rip), %rdi      ## 0x10000a018 <_critical>
100000e75: 48 8d 35 c4 91 00 00        	leaq	0x91c4(%rip), %rsi      ## 0x10000a040 <_S>
100000e7c: e8 4f 33 00 00              	callq	0x1000041d0 <_mb_win_critical_delete>
100000e81: c7 45 bc 00 00 00 00        	movl	$0x0, -0x44(%rbp)
100000e88: 48 c7 45 c0 00 00 00 00     	movq	$0x0, -0x40(%rbp)
100000e90: 48 8d 3d 99 91 00 00        	leaq	0x9199(%rip), %rdi      ## 0x10000a030 <_init_once>
100000e97: 48 8d 35 e2 91 00 00        	leaq	0x91e2(%rip), %rsi      ## 0x10000a080 <_S+0x40>
100000e9e: 48 8d 4d bc                 	leaq	-0x44(%rbp), %rcx
100000ea2: 4c 8d 45 c0                 	leaq	-0x40(%rbp), %r8
100000ea6: ba 01 00 00 00              	movl	$0x1, %edx
100000eab: e8 40 51 00 00              	callq	0x100005ff0 <_mb_win_init_once_begin>
100000eb0: 85 c0                       	testl	%eax, %eax
100000eb2: 0f 84 07 06 00 00           	je	0x1000014bf <_main+0xe4f>
100000eb8: 83 7d bc 00                 	cmpl	$0x0, -0x44(%rbp)
100000ebc: 0f 85 fd 05 00 00           	jne	0x1000014bf <_main+0xe4f>
100000ec2: 48 b8 02 00 5a 5a ef be d2 51       	movabsq	$0x51d2beef5a5a0002, %rax ## imm = 0x51D2BEEF5A5A0002
100000ecc: 48 39 45 c0                 	cmpq	%rax, -0x40(%rbp)
100000ed0: 0f 85 e9 05 00 00           	jne	0x1000014bf <_main+0xe4f>
100000ed6: 49 8b 7d 00                 	movq	(%r13), %rdi
100000eda: 48 8d 35 43 77 00 00        	leaq	0x7743(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000ee1: 48 8d 15 eb 74 00 00        	leaq	0x74eb(%rip), %rdx      ## 0x1000083d3 <_sysconf+0x1000083d3>
100000ee8: 31 c0                       	xorl	%eax, %eax
100000eea: e8 11 71 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000eef: 49 8b 7d 00                 	movq	(%r13), %rdi
100000ef3: e8 02 71 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000ef8: 48 8d 3d 39 91 00 00        	leaq	0x9139(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000eff: 48 8d 35 0a 79 00 00        	leaq	0x790a(%rip), %rsi      ## 0x100008810 <_abandon_folded>
100000f06: e8 05 65 00 00              	callq	0x100007410 <_mb_win_mutex_open_named>
100000f0b: 48 85 c0                    	testq	%rax, %rax
100000f0e: 0f 84 c2 05 00 00           	je	0x1000014d6 <_main+0xe66>
100000f14: 48 89 c3                    	movq	%rax, %rbx
100000f17: 48 8d 3d 1a 91 00 00        	leaq	0x911a(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000f1e: 48 89 c6                    	movq	%rax, %rsi
100000f21: ba 88 13 00 00              	movl	$0x1388, %edx           ## imm = 0x1388
100000f26: 31 c9                       	xorl	%ecx, %ecx
100000f28: e8 c3 68 00 00              	callq	0x1000077f0 <_mb_win_mutex_wait_ex>
100000f2d: 41 89 c6                    	movl	%eax, %r14d
100000f30: 49 8b 7d 00                 	movq	(%r13), %rdi
100000f34: 48 8d 35 e3 74 00 00        	leaq	0x74e3(%rip), %rsi      ## 0x10000841e <_sysconf+0x10000841e>
100000f3b: 89 c2                       	movl	%eax, %edx
100000f3d: 31 c0                       	xorl	%eax, %eax
100000f3f: e8 bc 70 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000f44: 41 81 fe 80 00 00 00        	cmpl	$0x80, %r14d
100000f4b: 0f 85 9c 05 00 00           	jne	0x1000014ed <_main+0xe7d>
100000f51: 48 8d 3d e0 90 00 00        	leaq	0x90e0(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000f58: 48 89 de                    	movq	%rbx, %rsi
100000f5b: e8 60 66 00 00              	callq	0x1000075c0 <_mb_win_mutex_release>
100000f60: 83 f8 01                    	cmpl	$0x1, %eax
100000f63: 0f 85 9b 05 00 00           	jne	0x100001504 <_main+0xe94>
100000f69: 48 8d 3d c8 90 00 00        	leaq	0x90c8(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000f70: 48 89 de                    	movq	%rbx, %rsi
100000f73: 31 d2                       	xorl	%edx, %edx
100000f75: 31 c9                       	xorl	%ecx, %ecx
100000f77: e8 74 68 00 00              	callq	0x1000077f0 <_mb_win_mutex_wait_ex>
100000f7c: 85 c0                       	testl	%eax, %eax
100000f7e: 0f 85 97 05 00 00           	jne	0x10000151b <_main+0xeab>
100000f84: 48 8d 3d ad 90 00 00        	leaq	0x90ad(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
100000f8b: 48 89 de                    	movq	%rbx, %rsi
100000f8e: e8 2d 66 00 00              	callq	0x1000075c0 <_mb_win_mutex_release>
100000f93: 49 8b 7d 00                 	movq	(%r13), %rdi
100000f97: 83 f8 01                    	cmpl	$0x1, %eax
100000f9a: 0f 85 92 05 00 00           	jne	0x100001532 <_main+0xec2>
100000fa0: 48 8d 35 7d 76 00 00        	leaq	0x767d(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000fa7: 48 8d 15 d0 74 00 00        	leaq	0x74d0(%rip), %rdx      ## 0x10000847e <_sysconf+0x10000847e>
100000fae: 31 c0                       	xorl	%eax, %eax
100000fb0: e8 4b 70 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100000fb5: 49 8b 7d 00                 	movq	(%r13), %rdi
100000fb9: e8 3c 70 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
100000fbe: 48 8d 3d 4b 90 00 00        	leaq	0x904b(%rip), %rdi      ## 0x10000a010 <_workers>
100000fc5: e8 b6 1a 00 00              	callq	0x100002a80 <_mb_win_workers_leave_call>
100000fca: 85 c0                       	testl	%eax, %eax
100000fcc: 0f 85 73 05 00 00           	jne	0x100001545 <_main+0xed5>
100000fd2: 48 8d 3d 37 90 00 00        	leaq	0x9037(%rip), %rdi      ## 0x10000a010 <_workers>
100000fd9: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100000fdd: e8 be 1b 00 00              	callq	0x100002ba0 <_mb_win_workers_collect>
100000fe2: 49 8b 7d 00                 	movq	(%r13), %rdi
100000fe6: 85 c0                       	testl	%eax, %eax
100000fe8: 0f 85 6e 05 00 00           	jne	0x10000155c <_main+0xeec>
100000fee: 48 8d 35 2f 76 00 00        	leaq	0x762f(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
100000ff5: 48 8d 15 c9 74 00 00        	leaq	0x74c9(%rip), %rdx      ## 0x1000084c5 <_sysconf+0x1000084c5>
100000ffc: 31 c0                       	xorl	%eax, %eax
100000ffe: e8 fd 6f 00 00              	callq	0x100008000 <_sysconf+0x100008000>
100001003: 49 8b 7d 00                 	movq	(%r13), %rdi
100001007: e8 ee 6f 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
10000100c: 48 8d 3d fd 8f 00 00        	leaq	0x8ffd(%rip), %rdi      ## 0x10000a010 <_workers>
100001013: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100001017: e8 a4 16 00 00              	callq	0x1000026c0 <_mb_win_workers_close>
10000101c: 85 c0                       	testl	%eax, %eax
10000101e: 0f 85 4b 05 00 00           	jne	0x10000156f <_main+0xeff>
100001024: 48 8d 3d fd 8f 00 00        	leaq	0x8ffd(%rip), %rdi      ## 0x10000a028 <_srw>
10000102b: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
10000102f: e8 1c 3d 00 00              	callq	0x100004d50 <_mb_win_srw_close>
100001034: 85 c0                       	testl	%eax, %eax
100001036: 0f 85 47 05 00 00           	jne	0x100001583 <_main+0xf13>
10000103c: 48 8d 3d dd 8f 00 00        	leaq	0x8fdd(%rip), %rdi      ## 0x10000a020 <_address_wait>
100001043: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100001047: e8 c4 33 00 00              	callq	0x100004410 <_mb_win_address_wait_close>
10000104c: 85 c0                       	testl	%eax, %eax
10000104e: 0f 85 43 05 00 00           	jne	0x100001597 <_main+0xf27>
100001054: 48 8d 3d bd 8f 00 00        	leaq	0x8fbd(%rip), %rdi      ## 0x10000a018 <_critical>
10000105b: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
10000105f: e8 ec 28 00 00              	callq	0x100003950 <_mb_win_critical_close>
100001064: 85 c0                       	testl	%eax, %eax
100001066: 0f 85 3f 05 00 00           	jne	0x1000015ab <_main+0xf3b>
10000106c: 48 8d 3d bd 8f 00 00        	leaq	0x8fbd(%rip), %rdi      ## 0x10000a030 <_init_once>
100001073: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100001077: e8 b4 4c 00 00              	callq	0x100005d30 <_mb_win_init_once_close>
10000107c: 85 c0                       	testl	%eax, %eax
10000107e: 0f 85 3b 05 00 00           	jne	0x1000015bf <_main+0xf4f>
100001084: 48 8d 3d ad 8f 00 00        	leaq	0x8fad(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
10000108b: 4c 8d 75 d4                 	leaq	-0x2c(%rbp), %r14
10000108f: 4c 89 f6                    	movq	%r14, %rsi
100001092: e8 09 5a 00 00              	callq	0x100006aa0 <_mb_win_mutex_close>
100001097: 89 c3                       	movl	%eax, %ebx
100001099: 49 8b 7d 00                 	movq	(%r13), %rdi
10000109d: 85 c0                       	testl	%eax, %eax
10000109f: 48 8d 05 da 74 00 00        	leaq	0x74da(%rip), %rax      ## 0x100008580 <_sysconf+0x100008580>
1000010a6: 48 8d 0d ce 74 00 00        	leaq	0x74ce(%rip), %rcx      ## 0x10000857b <_sysconf+0x10000857b>
1000010ad: 48 0f 44 c8                 	cmoveq	%rax, %rcx
1000010b1: 48 8d 05 d7 74 00 00        	leaq	0x74d7(%rip), %rax      ## 0x10000858f <_sysconf+0x10000858f>
1000010b8: 4c 8d 05 c4 74 00 00        	leaq	0x74c4(%rip), %r8       ## 0x100008583 <_sysconf+0x100008583>
1000010bf: 4c 0f 44 c0                 	cmoveq	%rax, %r8
1000010c3: 48 8d 35 69 74 00 00        	leaq	0x7469(%rip), %rsi      ## 0x100008533 <_sysconf+0x100008533>
1000010ca: 89 da                       	movl	%ebx, %edx
1000010cc: 31 c0                       	xorl	%eax, %eax
1000010ce: e8 2d 6f 00 00              	callq	0x100008000 <_sysconf+0x100008000>
1000010d3: 49 8b 7d 00                 	movq	(%r13), %rdi
1000010d7: 48 8d 35 46 75 00 00        	leaq	0x7546(%rip), %rsi      ## 0x100008624 <_sysconf+0x100008624>
1000010de: 48 8d 15 b2 74 00 00        	leaq	0x74b2(%rip), %rdx      ## 0x100008597 <_sysconf+0x100008597>
1000010e5: 31 c0                       	xorl	%eax, %eax
1000010e7: e8 14 6f 00 00              	callq	0x100008000 <_sysconf+0x100008000>
1000010ec: 49 8b 7d 00                 	movq	(%r13), %rdi
1000010f0: e8 05 6f 00 00              	callq	0x100007ffa <_sysconf+0x100007ffa>
1000010f5: 48 8d 3d 0c 8f 00 00        	leaq	0x8f0c(%rip), %rdi      ## 0x10000a008 <_domain>
1000010fc: 4c 89 f6                    	movq	%r14, %rsi
1000010ff: e8 bc 0b 00 00              	callq	0x100001cc0 <_mb_win_threads_leave>
100001104: 85 c0                       	testl	%eax, %eax
100001106: 0f 85 c7 04 00 00           	jne	0x1000015d3 <_main+0xf63>
10000110c: 48 8d 3d f5 8e 00 00        	leaq	0x8ef5(%rip), %rdi      ## 0x10000a008 <_domain>
100001113: 48 8d 75 d4                 	leaq	-0x2c(%rbp), %rsi
100001117: e8 c4 0c 00 00              	callq	0x100001de0 <_mb_win_threads_close>
10000111c: 49 8b 7d 00                 	movq	(%r13), %rdi
100001120: 85 c0                       	testl	%eax, %eax
100001122: 0f 85 bf 04 00 00           	jne	0x1000015e7 <_main+0xf77>
100001128: 85 db                       	testl	%ebx, %ebx
10000112a: 48 8d 05 d1 74 00 00        	leaq	0x74d1(%rip), %rax      ## 0x100008602 <_sysconf+0x100008602>
100001131: 48 8d 15 af 74 00 00        	leaq	0x74af(%rip), %rdx      ## 0x1000085e7 <_sysconf+0x1000085e7>
100001138: 48 0f 44 d0                 	cmoveq	%rax, %rdx
10000113c: 48 8d 35 84 74 00 00        	leaq	0x7484(%rip), %rsi      ## 0x1000085c7 <_sysconf+0x1000085c7>
100001143: 31 c0                       	xorl	%eax, %eax
100001145: e8 b6 6e 00 00              	callq	0x100008000 <_sysconf+0x100008000>
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
100001160: 4c 8d 35 c1 8e 00 00        	leaq	0x8ec1(%rip), %r14      ## 0x10000a028 <_srw>
100001167: 4c 8d 3d 02 8f 00 00        	leaq	0x8f02(%rip), %r15      ## 0x10000a070 <_S+0x30>
10000116e: 4c 8d 25 f3 8e 00 00        	leaq	0x8ef3(%rip), %r12      ## 0x10000a068 <_S+0x28>
100001175: 4c 8d 2d 94 8e 00 00        	leaq	0x8e94(%rip), %r13      ## 0x10000a010 <_workers>
10000117c: eb 10                       	jmp	0x10000118e <_main+0xb1e>
10000117e: 66 90                       	nop
100001180: 8b 05 06 8f 00 00           	movl	0x8f06(%rip), %eax      ## 0x10000a08c <_S+0x4c>
100001186: 85 c0                       	testl	%eax, %eax
100001188: 0f 85 ce fa ff ff           	jne	0x100000c5c <_main+0x5ec>
10000118e: 4c 89 f7                    	movq	%r14, %rdi
100001191: 4c 89 fe                    	movq	%r15, %rsi
100001194: 4c 89 e2                    	movq	%r12, %rdx
100001197: b9 64 00 00 00              	movl	$0x64, %ecx
10000119c: 45 31 c0                    	xorl	%r8d, %r8d
10000119f: e8 cc 43 00 00              	callq	0x100005570 <_mb_win_srw_sleep>
1000011a4: 8b 05 e2 8e 00 00           	movl	0x8ee2(%rip), %eax      ## 0x10000a08c <_S+0x4c>
1000011aa: 85 c0                       	testl	%eax, %eax
1000011ac: 75 d2                       	jne	0x100001180 <_main+0xb10>
1000011ae: 48 8b 35 eb 8e 00 00        	movq	0x8eeb(%rip), %rsi      ## 0x10000a0a0 <_thread_handle>
1000011b5: 4c 89 ef                    	movq	%r13, %rdi
1000011b8: 31 d2                       	xorl	%edx, %edx
1000011ba: e8 f1 1f 00 00              	callq	0x1000031b0 <_mb_win_workers_wait>
1000011bf: 3d 02 01 00 00              	cmpl	$0x102, %eax            ## imm = 0x102
1000011c4: 75 07                       	jne	0x1000011cd <_main+0xb5d>
1000011c6: ff c3                       	incl	%ebx
1000011c8: 83 fb 14                    	cmpl	$0x14, %ebx
1000011cb: 75 b3                       	jne	0x100001180 <_main+0xb10>
1000011cd: 48 8d 3d 54 8e 00 00        	leaq	0x8e54(%rip), %rdi      ## 0x10000a028 <_srw>
1000011d4: 48 8d 35 8d 8e 00 00        	leaq	0x8e8d(%rip), %rsi      ## 0x10000a068 <_S+0x28>
1000011db: ba 01 00 00 00              	movl	$0x1, %edx
1000011e0: e8 4b 42 00 00              	callq	0x100005430 <_mb_win_srw_release>
1000011e5: 48 8b 05 2c 7e 00 00        	movq	0x7e2c(%rip), %rax      ## 0x100009018 <_sysconf+0x100009018>
1000011ec: 48 8b 38                    	movq	(%rax), %rdi
1000011ef: 48 8d 35 39 74 00 00        	leaq	0x7439(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000011f6: 48 8d 15 9d 70 00 00        	leaq	0x709d(%rip), %rdx      ## 0x10000829a <_sysconf+0x10000829a>
1000011fd: e9 f3 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001202: 49 8b 3f                    	movq	(%r15), %rdi
100001205: 48 8d 35 23 74 00 00        	leaq	0x7423(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000120c: 48 8d 15 f6 6f 00 00        	leaq	0x6ff6(%rip), %rdx      ## 0x100008209 <_sysconf+0x100008209>
100001213: e9 dd 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001218: 49 8b 3f                    	movq	(%r15), %rdi
10000121b: 48 8d 35 0d 74 00 00        	leaq	0x740d(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001222: 48 8d 15 57 74 00 00        	leaq	0x7457(%rip), %rdx      ## 0x100008680 <_sysconf+0x100008680>
100001229: e9 c7 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000122e: 49 8b 3f                    	movq	(%r15), %rdi
100001231: 48 8d 35 f7 73 00 00        	leaq	0x73f7(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001238: 48 8d 15 75 74 00 00        	leaq	0x7475(%rip), %rdx      ## 0x1000086b4 <_sysconf+0x1000086b4>
10000123f: e9 b1 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001244: 49 8b 3f                    	movq	(%r15), %rdi
100001247: 48 8d 35 e1 73 00 00        	leaq	0x73e1(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000124e: 48 8d 15 76 74 00 00        	leaq	0x7476(%rip), %rdx      ## 0x1000086cb <_sysconf+0x1000086cb>
100001255: e9 9b 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000125a: 49 8b 3f                    	movq	(%r15), %rdi
10000125d: 48 8d 35 cb 73 00 00        	leaq	0x73cb(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001264: 48 8d 15 84 74 00 00        	leaq	0x7484(%rip), %rdx      ## 0x1000086ef <_sysconf+0x1000086ef>
10000126b: e9 85 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001270: 49 8b 3f                    	movq	(%r15), %rdi
100001273: 48 8d 35 b5 73 00 00        	leaq	0x73b5(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000127a: 48 8d 15 46 6e 00 00        	leaq	0x6e46(%rip), %rdx      ## 0x1000080c7 <_sysconf+0x1000080c7>
100001281: e9 6f 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001286: 48 8d 35 a2 73 00 00        	leaq	0x73a2(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000128d: 48 8d 15 40 6e 00 00        	leaq	0x6e40(%rip), %rdx      ## 0x1000080d4 <_sysconf+0x1000080d4>
100001294: e9 5c 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001299: 49 8b 3f                    	movq	(%r15), %rdi
10000129c: 48 8d 35 8c 73 00 00        	leaq	0x738c(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000012a3: 48 8d 15 7c 6e 00 00        	leaq	0x6e7c(%rip), %rdx      ## 0x100008126 <_sysconf+0x100008126>
1000012aa: e9 46 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000012af: 49 8b 3f                    	movq	(%r15), %rdi
1000012b2: 48 8d 35 76 73 00 00        	leaq	0x7376(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000012b9: 48 8d 15 73 6e 00 00        	leaq	0x6e73(%rip), %rdx      ## 0x100008133 <_sysconf+0x100008133>
1000012c0: e9 30 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000012c5: 49 8b 3f                    	movq	(%r15), %rdi
1000012c8: 48 8d 35 60 73 00 00        	leaq	0x7360(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000012cf: 48 8d 15 6b 6e 00 00        	leaq	0x6e6b(%rip), %rdx      ## 0x100008141 <_sysconf+0x100008141>
1000012d6: e9 1a 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000012db: 49 8b 3f                    	movq	(%r15), %rdi
1000012de: 48 8d 35 4a 73 00 00        	leaq	0x734a(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000012e5: 48 8d 15 67 6e 00 00        	leaq	0x6e67(%rip), %rdx      ## 0x100008153 <_sysconf+0x100008153>
1000012ec: e9 04 03 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000012f1: 49 8b 3f                    	movq	(%r15), %rdi
1000012f4: 48 8d 35 34 73 00 00        	leaq	0x7334(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000012fb: 48 8d 15 5a 6e 00 00        	leaq	0x6e5a(%rip), %rdx      ## 0x10000815c <_sysconf+0x10000815c>
100001302: e9 ee 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001307: 48 8d 35 21 73 00 00        	leaq	0x7321(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000130e: 48 8d 15 56 6e 00 00        	leaq	0x6e56(%rip), %rdx      ## 0x10000816b <_sysconf+0x10000816b>
100001315: e9 db 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000131a: 49 8b 3f                    	movq	(%r15), %rdi
10000131d: 48 8d 35 0b 73 00 00        	leaq	0x730b(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001324: 48 8d 15 77 6e 00 00        	leaq	0x6e77(%rip), %rdx      ## 0x1000081a2 <_sysconf+0x1000081a2>
10000132b: e9 c5 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001330: 49 8b 3f                    	movq	(%r15), %rdi
100001333: 48 8d 35 f5 72 00 00        	leaq	0x72f5(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000133a: 48 8d 15 75 6e 00 00        	leaq	0x6e75(%rip), %rdx      ## 0x1000081b6 <_sysconf+0x1000081b6>
100001341: e9 af 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001346: 49 8b 3f                    	movq	(%r15), %rdi
100001349: 48 8d 35 df 72 00 00        	leaq	0x72df(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001350: 48 8d 15 7f 6e 00 00        	leaq	0x6e7f(%rip), %rdx      ## 0x1000081d6 <_sysconf+0x1000081d6>
100001357: e9 99 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000135c: 49 8b 3f                    	movq	(%r15), %rdi
10000135f: 48 8d 35 c9 72 00 00        	leaq	0x72c9(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001366: 48 8d 15 74 6e 00 00        	leaq	0x6e74(%rip), %rdx      ## 0x1000081e1 <_sysconf+0x1000081e1>
10000136d: e9 83 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001372: 49 8b 3f                    	movq	(%r15), %rdi
100001375: 48 8d 35 b3 72 00 00        	leaq	0x72b3(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000137c: 48 8d 15 6c 6e 00 00        	leaq	0x6e6c(%rip), %rdx      ## 0x1000081ef <_sysconf+0x1000081ef>
100001383: e9 6d 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001388: 49 8b 3f                    	movq	(%r15), %rdi
10000138b: 48 8d 35 9d 72 00 00        	leaq	0x729d(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001392: 48 8d 15 61 6e 00 00        	leaq	0x6e61(%rip), %rdx      ## 0x1000081fa <_sysconf+0x1000081fa>
100001399: e9 57 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000139e: 49 8b 3f                    	movq	(%r15), %rdi
1000013a1: 48 8d 35 87 72 00 00        	leaq	0x7287(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000013a8: 48 8d 15 93 72 00 00        	leaq	0x7293(%rip), %rdx      ## 0x100008642 <_sysconf+0x100008642>
1000013af: e9 41 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000013b4: 49 8b 3f                    	movq	(%r15), %rdi
1000013b7: 48 8d 35 71 72 00 00        	leaq	0x7271(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000013be: 48 8d 15 94 72 00 00        	leaq	0x7294(%rip), %rdx      ## 0x100008659 <_sysconf+0x100008659>
1000013c5: e9 2b 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000013ca: 49 8b 3f                    	movq	(%r15), %rdi
1000013cd: 48 8d 35 5b 72 00 00        	leaq	0x725b(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000013d4: 48 8d 15 95 72 00 00        	leaq	0x7295(%rip), %rdx      ## 0x100008670 <_sysconf+0x100008670>
1000013db: e9 15 02 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000013e0: 49 8b 3f                    	movq	(%r15), %rdi
1000013e3: 48 8d 35 45 72 00 00        	leaq	0x7245(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000013ea: 48 8d 15 a1 72 00 00        	leaq	0x72a1(%rip), %rdx      ## 0x100008692 <_sysconf+0x100008692>
1000013f1: e9 ff 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000013f6: 49 8b 3f                    	movq	(%r15), %rdi
1000013f9: 48 8d 35 2f 72 00 00        	leaq	0x722f(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001400: 48 8d 15 99 72 00 00        	leaq	0x7299(%rip), %rdx      ## 0x1000086a0 <_sysconf+0x1000086a0>
100001407: e9 e9 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000140c: 49 8b 3f                    	movq	(%r15), %rdi
10000140f: 48 8d 35 19 72 00 00        	leaq	0x7219(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001416: 48 8d 15 c0 72 00 00        	leaq	0x72c0(%rip), %rdx      ## 0x1000086dd <_sysconf+0x1000086dd>
10000141d: e9 d3 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001422: 49 8b 3f                    	movq	(%r15), %rdi
100001425: 48 8d 35 03 72 00 00        	leaq	0x7203(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000142c: 48 8d 15 4e 6e 00 00        	leaq	0x6e4e(%rip), %rdx      ## 0x100008281 <_sysconf+0x100008281>
100001433: e9 bd 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001438: 48 8b 05 d9 7b 00 00        	movq	0x7bd9(%rip), %rax      ## 0x100009018 <_sysconf+0x100009018>
10000143f: 48 8b 38                    	movq	(%rax), %rdi
100001442: 48 8d 35 e6 71 00 00        	leaq	0x71e6(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001449: 48 8d 15 3c 6e 00 00        	leaq	0x6e3c(%rip), %rdx      ## 0x10000828c <_sysconf+0x10000828c>
100001450: e9 a0 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001455: 49 8b 7d 00                 	movq	(%r13), %rdi
100001459: 48 8d 35 cf 71 00 00        	leaq	0x71cf(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001460: 48 8d 15 6e 6e 00 00        	leaq	0x6e6e(%rip), %rdx      ## 0x1000082d5 <_sysconf+0x1000082d5>
100001467: e9 89 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000146c: 49 8b 7d 00                 	movq	(%r13), %rdi
100001470: 48 8d 35 b8 71 00 00        	leaq	0x71b8(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001477: 48 8d 15 61 6e 00 00        	leaq	0x6e61(%rip), %rdx      ## 0x1000082df <_sysconf+0x1000082df>
10000147e: e9 72 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001483: 48 8d 35 a5 71 00 00        	leaq	0x71a5(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000148a: 48 8d 15 99 6e 00 00        	leaq	0x6e99(%rip), %rdx      ## 0x10000832a <_sysconf+0x10000832a>
100001491: e9 5f 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001496: 48 8d 35 92 71 00 00        	leaq	0x7192(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000149d: 48 8d 15 bd 6e 00 00        	leaq	0x6ebd(%rip), %rdx      ## 0x100008361 <_sysconf+0x100008361>
1000014a4: 48 89 df                    	movq	%rbx, %rdi
1000014a7: e9 49 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000014ac: 48 8d 35 7c 71 00 00        	leaq	0x717c(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000014b3: 48 8d 15 de 6e 00 00        	leaq	0x6ede(%rip), %rdx      ## 0x100008398 <_sysconf+0x100008398>
1000014ba: e9 36 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000014bf: 49 8b 7d 00                 	movq	(%r13), %rdi
1000014c3: 48 8d 35 65 71 00 00        	leaq	0x7165(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000014ca: 48 8d 15 f2 6e 00 00        	leaq	0x6ef2(%rip), %rdx      ## 0x1000083c3 <_sysconf+0x1000083c3>
1000014d1: e9 1f 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000014d6: 49 8b 7d 00                 	movq	(%r13), %rdi
1000014da: 48 8d 35 4e 71 00 00        	leaq	0x714e(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000014e1: 48 8d 15 29 6f 00 00        	leaq	0x6f29(%rip), %rdx      ## 0x100008411 <_sysconf+0x100008411>
1000014e8: e9 08 01 00 00              	jmp	0x1000015f5 <_main+0xf85>
1000014ed: 49 8b 7d 00                 	movq	(%r13), %rdi
1000014f1: 48 8d 35 37 71 00 00        	leaq	0x7137(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000014f8: 48 8d 15 48 6f 00 00        	leaq	0x6f48(%rip), %rdx      ## 0x100008447 <_sysconf+0x100008447>
1000014ff: e9 f1 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001504: 49 8b 7d 00                 	movq	(%r13), %rdi
100001508: 48 8d 35 20 71 00 00        	leaq	0x7120(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000150f: 48 8d 15 3b 6f 00 00        	leaq	0x6f3b(%rip), %rdx      ## 0x100008451 <_sysconf+0x100008451>
100001516: e9 da 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000151b: 49 8b 7d 00                 	movq	(%r13), %rdi
10000151f: 48 8d 35 09 71 00 00        	leaq	0x7109(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001526: 48 8d 15 35 6f 00 00        	leaq	0x6f35(%rip), %rdx      ## 0x100008462 <_sysconf+0x100008462>
10000152d: e9 c3 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001532: 48 8d 35 f6 70 00 00        	leaq	0x70f6(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001539: 48 8d 15 2c 6f 00 00        	leaq	0x6f2c(%rip), %rdx      ## 0x10000846c <_sysconf+0x10000846c>
100001540: e9 b0 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
100001545: 49 8b 7d 00                 	movq	(%r13), %rdi
100001549: 48 8d 35 df 70 00 00        	leaq	0x70df(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001550: 48 8d 15 53 6f 00 00        	leaq	0x6f53(%rip), %rdx      ## 0x1000084aa <_sysconf+0x1000084aa>
100001557: e9 99 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000155c: 48 8d 35 cc 70 00 00        	leaq	0x70cc(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
100001563: 48 8d 15 4b 6f 00 00        	leaq	0x6f4b(%rip), %rdx      ## 0x1000084b5 <_sysconf+0x1000084b5>
10000156a: e9 86 00 00 00              	jmp	0x1000015f5 <_main+0xf85>
10000156f: 49 8b 7d 00                 	movq	(%r13), %rdi
100001573: 48 8d 35 b5 70 00 00        	leaq	0x70b5(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000157a: 48 8d 15 68 6f 00 00        	leaq	0x6f68(%rip), %rdx      ## 0x1000084e9 <_sysconf+0x1000084e9>
100001581: eb 72                       	jmp	0x1000015f5 <_main+0xf85>
100001583: 49 8b 7d 00                 	movq	(%r13), %rdi
100001587: 48 8d 35 a1 70 00 00        	leaq	0x70a1(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
10000158e: 48 8d 15 62 6f 00 00        	leaq	0x6f62(%rip), %rdx      ## 0x1000084f7 <_sysconf+0x1000084f7>
100001595: eb 5e                       	jmp	0x1000015f5 <_main+0xf85>
100001597: 49 8b 7d 00                 	movq	(%r13), %rdi
10000159b: 48 8d 35 8d 70 00 00        	leaq	0x708d(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000015a2: 48 8d 15 58 6f 00 00        	leaq	0x6f58(%rip), %rdx      ## 0x100008501 <_sysconf+0x100008501>
1000015a9: eb 4a                       	jmp	0x1000015f5 <_main+0xf85>
1000015ab: 49 8b 7d 00                 	movq	(%r13), %rdi
1000015af: 48 8d 35 79 70 00 00        	leaq	0x7079(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000015b6: 48 8d 15 57 6f 00 00        	leaq	0x6f57(%rip), %rdx      ## 0x100008514 <_sysconf+0x100008514>
1000015bd: eb 36                       	jmp	0x1000015f5 <_main+0xf85>
1000015bf: 49 8b 7d 00                 	movq	(%r13), %rdi
1000015c3: 48 8d 35 65 70 00 00        	leaq	0x7065(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000015ca: 48 8d 15 52 6f 00 00        	leaq	0x6f52(%rip), %rdx      ## 0x100008523 <_sysconf+0x100008523>
1000015d1: eb 22                       	jmp	0x1000015f5 <_main+0xf85>
1000015d3: 49 8b 7d 00                 	movq	(%r13), %rdi
1000015d7: 48 8d 35 51 70 00 00        	leaq	0x7051(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000015de: 48 8d 15 c6 6f 00 00        	leaq	0x6fc6(%rip), %rdx      ## 0x1000085ab <_sysconf+0x1000085ab>
1000015e5: eb 0e                       	jmp	0x1000015f5 <_main+0xf85>
1000015e7: 48 8d 35 41 70 00 00        	leaq	0x7041(%rip), %rsi      ## 0x10000862f <_sysconf+0x10000862f>
1000015ee: 48 8d 15 c4 6f 00 00        	leaq	0x6fc4(%rip), %rdx      ## 0x1000085b9 <_sysconf+0x1000085b9>
1000015f5: 31 c0                       	xorl	%eax, %eax
1000015f7: e8 04 6a 00 00              	callq	0x100008000 <_sysconf+0x100008000>
1000015fc: bf 02 00 00 00              	movl	$0x2, %edi
100001601: e8 ee 69 00 00              	callq	0x100007ff4 <_sysconf+0x100007ff4>
100001606: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)

0000000100001610 <_on_alarm>:
100001610: 55                          	pushq	%rbp
100001611: 48 89 e5                    	movq	%rsp, %rbp
100001614: 48 8b 05 fd 79 00 00        	movq	0x79fd(%rip), %rax      ## 0x100009018 <_sysconf+0x100009018>
10000161b: 48 8b 08                    	movq	(%rax), %rcx
10000161e: 48 8d 3d de 6f 00 00        	leaq	0x6fde(%rip), %rdi      ## 0x100008603 <_sysconf+0x100008603>
100001625: be 20 00 00 00              	movl	$0x20, %esi
10000162a: ba 01 00 00 00              	movl	$0x1, %edx
10000162f: e8 d8 69 00 00              	callq	0x10000800c <_sysconf+0x10000800c>
100001634: bf 63 00 00 00              	movl	$0x63, %edi
100001639: e8 98 69 00 00              	callq	0x100007fd6 <_sysconf+0x100007fd6>
10000163e: 66 90                       	nop

0000000100001640 <_code_acquire>:
100001640: 55                          	pushq	%rbp
100001641: 48 89 e5                    	movq	%rsp, %rbp
100001644: 53                          	pushq	%rbx
100001645: 50                          	pushq	%rax
100001646: 48 89 d3                    	movq	%rdx, %rbx
100001649: bf 08 00 00 00              	movl	$0x8, %edi
10000164e: e8 bf 69 00 00              	callq	0x100008012 <_sysconf+0x100008012>
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
100001677: e8 8a 69 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
1000016db: 4c 8b 2d 36 79 00 00        	movq	0x7936(%rip), %r13      ## 0x100009018 <_sysconf+0x100009018>
1000016e2: 49 8b 4d 00                 	movq	(%r13), %rcx
1000016e6: 48 8d 3d 14 70 00 00        	leaq	0x7014(%rip), %rdi      ## 0x100008701 <_sysconf+0x100008701>
1000016ed: be 0f 00 00 00              	movl	$0xf, %esi
1000016f2: ba 01 00 00 00              	movl	$0x1, %edx
1000016f7: e8 10 69 00 00              	callq	0x10000800c <_sysconf+0x10000800c>
1000016fc: 48 8d 3d 05 89 00 00        	leaq	0x8905(%rip), %rdi      ## 0x10000a008 <_domain>
100001703: e8 18 0a 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100001708: c7 00 ab 95 73 e1           	movl	$0xe17395ab, (%rax)     ## imm = 0xE17395AB
10000170e: c7 85 14 ff ff ff 00 00 00 00       	movl	$0x0, -0xec(%rbp)
100001718: 48 c7 85 00 ff ff ff 00 00 00 00    	movq	$0x0, -0x100(%rbp)
100001723: 4c 8d 73 40                 	leaq	0x40(%rbx), %r14
100001727: 48 8d 3d 02 89 00 00        	leaq	0x8902(%rip), %rdi      ## 0x10000a030 <_init_once>
10000172e: 48 8d 8d 14 ff ff ff        	leaq	-0xec(%rbp), %rcx
100001735: 4c 8d 85 00 ff ff ff        	leaq	-0x100(%rbp), %r8
10000173c: 4c 89 f6                    	movq	%r14, %rsi
10000173f: 31 d2                       	xorl	%edx, %edx
100001741: e8 aa 48 00 00              	callq	0x100005ff0 <_mb_win_init_once_begin>
100001746: 85 c0                       	testl	%eax, %eax
100001748: 74 09                       	je	0x100001753 <_worker_start+0xd3>
10000174a: 83 bd 14 ff ff ff 01        	cmpl	$0x1, -0xec(%rbp)
100001751: 74 08                       	je	0x10000175b <_worker_start+0xdb>
100001753: b8 01 00 00 00              	movl	$0x1, %eax
100001758: 87 43 58                    	xchgl	%eax, 0x58(%rbx)
10000175b: 49 8b 4d 00                 	movq	(%r13), %rcx
10000175f: 48 8d 3d ab 6f 00 00        	leaq	0x6fab(%rip), %rdi      ## 0x100008711 <_sysconf+0x100008711>
100001766: be 1d 00 00 00              	movl	$0x1d, %esi
10000176b: ba 01 00 00 00              	movl	$0x1, %edx
100001770: e8 97 68 00 00              	callq	0x10000800c <_sysconf+0x10000800c>
100001775: 48 8d 3d bc 88 00 00        	leaq	0x88bc(%rip), %rdi      ## 0x10000a038 <_mutex_owner>
10000177c: 48 8d 35 1d 71 00 00        	leaq	0x711d(%rip), %rsi      ## 0x1000088a0 <_abandon>
100001783: 41 bf 01 00 00 00           	movl	$0x1, %r15d
100001789: ba 01 00 00 00              	movl	$0x1, %edx
10000178e: e8 0d 56 00 00              	callq	0x100006da0 <_mb_win_mutex_create>
100001793: 48 85 c0                    	testq	%rax, %rax
100001796: 75 08                       	jne	0x1000017a0 <_worker_start+0x120>
100001798: b9 01 00 00 00              	movl	$0x1, %ecx
10000179d: 87 4b 58                    	xchgl	%ecx, 0x58(%rbx)
1000017a0: 49 8b 7d 00                 	movq	(%r13), %rdi
1000017a4: 48 8d 35 84 6f 00 00        	leaq	0x6f84(%rip), %rsi      ## 0x10000872f <_sysconf+0x10000872f>
1000017ab: 48 89 c2                    	movq	%rax, %rdx
1000017ae: 31 c0                       	xorl	%eax, %eax
1000017b0: e8 4b 68 00 00              	callq	0x100008000 <_sysconf+0x100008000>
1000017b5: 44 87 7b 50                 	xchgl	%r15d, 0x50(%rbx)
1000017b9: 4c 8d 3d 50 88 00 00        	leaq	0x8850(%rip), %r15      ## 0x10000a010 <_workers>
1000017c0: 4c 89 ff                    	movq	%r15, %rdi
1000017c3: 31 f6                       	xorl	%esi, %esi
1000017c5: e8 e6 1e 00 00              	callq	0x1000036b0 <_mb_win_workers_sleep>
1000017ca: 4c 89 ff                    	movq	%r15, %rdi
1000017cd: be 01 00 00 00              	movl	$0x1, %esi
1000017d2: e8 d9 1e 00 00              	callq	0x1000036b0 <_mb_win_workers_sleep>
1000017d7: c7 85 1c ff ff ff 00 00 00 00       	movl	$0x0, -0xe4(%rbp)
1000017e1: 48 8d 3d 30 88 00 00        	leaq	0x8830(%rip), %rdi      ## 0x10000a018 <_critical>
1000017e8: 48 8d 95 1c ff ff ff        	leaq	-0xe4(%rbp), %rdx
1000017ef: 48 89 de                    	movq	%rbx, %rsi
1000017f2: e8 89 28 00 00              	callq	0x100004080 <_mb_win_critical_try_enter>
1000017f7: 83 bd 1c ff ff ff 00        	cmpl	$0x0, -0xe4(%rbp)
1000017fe: 74 0f                       	je	0x10000180f <_worker_start+0x18f>
100001800: 48 8d 3d 11 88 00 00        	leaq	0x8811(%rip), %rdi      ## 0x10000a018 <_critical>
100001807: 48 89 de                    	movq	%rbx, %rsi
10000180a: e8 31 29 00 00              	callq	0x100004140 <_mb_win_critical_leave>
10000180f: 4c 8d 7b 28                 	leaq	0x28(%rbx), %r15
100001813: 4c 8d 25 0e 88 00 00        	leaq	0x880e(%rip), %r12      ## 0x10000a028 <_srw>
10000181a: 4c 89 e7                    	movq	%r12, %rdi
10000181d: 4c 89 fe                    	movq	%r15, %rsi
100001820: 31 d2                       	xorl	%edx, %edx
100001822: e8 f9 37 00 00              	callq	0x100005020 <_mb_win_srw_acquire>
100001827: 83 bd 1c ff ff ff 01        	cmpl	$0x1, -0xe4(%rbp)
10000182e: b8 02 00 00 00              	movl	$0x2, %eax
100001833: 83 d8 00                    	sbbl	$0x0, %eax
100001836: 87 43 4c                    	xchgl	%eax, 0x4c(%rbx)
100001839: 48 8d 73 30                 	leaq	0x30(%rbx), %rsi
10000183d: 4c 89 e7                    	movq	%r12, %rdi
100001840: ba 01 00 00 00              	movl	$0x1, %edx
100001845: e8 e6 42 00 00              	callq	0x100005b30 <_mb_win_srw_wake>
10000184a: 4c 89 e7                    	movq	%r12, %rdi
10000184d: 4c 89 fe                    	movq	%r15, %rsi
100001850: 31 d2                       	xorl	%edx, %edx
100001852: e8 d9 3b 00 00              	callq	0x100005430 <_mb_win_srw_release>
100001857: 49 8b 7d 00                 	movq	(%r13), %rdi
10000185b: 8b 53 4c                    	movl	0x4c(%rbx), %edx
10000185e: 48 8d 35 fb 6e 00 00        	leaq	0x6efb(%rip), %rsi      ## 0x100008760 <_sysconf+0x100008760>
100001865: 31 c0                       	xorl	%eax, %eax
100001867: e8 94 67 00 00              	callq	0x100008000 <_sysconf+0x100008000>
10000186c: c7 85 10 ff ff ff 00 00 00 00       	movl	$0x0, -0xf0(%rbp)
100001876: 4c 8d 7b 48                 	leaq	0x48(%rbx), %r15
10000187a: 4c 8d 25 9f 87 00 00        	leaq	0x879f(%rip), %r12      ## 0x10000a020 <_address_wait>
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
1000018c6: 4c 8b 2d 4b 77 00 00        	movq	0x774b(%rip), %r13      ## 0x100009018 <_sysconf+0x100009018>
1000018cd: 49 8b 4d 00                 	movq	(%r13), %rcx
1000018d1: 48 8d 3d ac 6e 00 00        	leaq	0x6eac(%rip), %rdi      ## 0x100008784 <_sysconf+0x100008784>
1000018d8: be 23 00 00 00              	movl	$0x23, %esi
1000018dd: ba 01 00 00 00              	movl	$0x1, %edx
1000018e2: e8 25 67 00 00              	callq	0x10000800c <_sysconf+0x10000800c>
1000018e7: 4c 8d 25 2a 87 00 00        	leaq	0x872a(%rip), %r12      ## 0x10000a018 <_critical>
1000018ee: 4c 89 e7                    	movq	%r12, %rdi
1000018f1: 48 89 de                    	movq	%rbx, %rsi
1000018f4: e8 57 25 00 00              	callq	0x100003e50 <_mb_win_critical_enter>
1000018f9: 48 8d 3d 08 87 00 00        	leaq	0x8708(%rip), %rdi      ## 0x10000a008 <_domain>
100001900: e8 1b 08 00 00              	callq	0x100002120 <_mb_win_threads_last_error>
100001905: 8b 00                       	movl	(%rax), %eax
100001907: 89 43 5c                    	movl	%eax, 0x5c(%rbx)
10000190a: 4c 89 e7                    	movq	%r12, %rdi
10000190d: 48 89 de                    	movq	%rbx, %rsi
100001910: e8 2b 28 00 00              	callq	0x100004140 <_mb_win_critical_leave>
100001915: 49 8b 7d 00                 	movq	(%r13), %rdi
100001919: 8b 53 5c                    	movl	0x5c(%rbx), %edx
10000191c: 48 8d 35 85 6e 00 00        	leaq	0x6e85(%rip), %rsi      ## 0x1000087a8 <_sysconf+0x1000087a8>
100001923: 31 c0                       	xorl	%eax, %eax
100001925: e8 d6 66 00 00              	callq	0x100008000 <_sysconf+0x100008000>
10000192a: 48 8d 3d ff 86 00 00        	leaq	0x86ff(%rip), %rdi      ## 0x10000a030 <_init_once>
100001931: 4c 89 f6                    	movq	%r14, %rsi
100001934: 31 d2                       	xorl	%edx, %edx
100001936: 4c 89 f9                    	movq	%r15, %rcx
100001939: e8 22 4c 00 00              	callq	0x100006560 <_mb_win_init_once_complete>
10000193e: 85 c0                       	testl	%eax, %eax
100001940: 75 08                       	jne	0x10000194a <_worker_start+0x2ca>
100001942: b8 01 00 00 00              	movl	$0x1, %eax
100001947: 87 43 58                    	xchgl	%eax, 0x58(%rbx)
10000194a: c7 85 18 ff ff ff 00 00 00 00       	movl	$0x0, -0xe8(%rbp)
100001954: 48 c7 85 08 ff ff ff 00 00 00 00    	movq	$0x0, -0xf8(%rbp)
10000195f: 48 8d 3d ca 86 00 00        	leaq	0x86ca(%rip), %rdi      ## 0x10000a030 <_init_once>
100001966: 41 bc 01 00 00 00           	movl	$0x1, %r12d
10000196c: 48 8d 8d 18 ff ff ff        	leaq	-0xe8(%rbp), %rcx
100001973: 4c 8d 85 08 ff ff ff        	leaq	-0xf8(%rbp), %r8
10000197a: 4c 89 f6                    	movq	%r14, %rsi
10000197d: ba 01 00 00 00              	movl	$0x1, %edx
100001982: e8 69 46 00 00              	callq	0x100005ff0 <_mb_win_init_once_begin>
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
1000019c7: 48 8d 35 f6 6d 00 00        	leaq	0x6df6(%rip), %rsi      ## 0x1000087c4 <_sysconf+0x1000087c4>
1000019ce: 31 c0                       	xorl	%eax, %eax
1000019d0: e8 2b 66 00 00              	callq	0x100008000 <_sysconf+0x100008000>
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
100001a72: e8 71 65 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
100001a77: 48 85 c0                    	testq	%rax, %rax
100001a7a: 74 1d                       	je	0x100001a99 <_mb_win_threads_open+0x59>
100001a7c: 49 89 c6                    	movq	%rax, %r14
100001a7f: 48 89 c7                    	movq	%rax, %rdi
100001a82: 31 f6                       	xorl	%esi, %esi
100001a84: e8 f5 65 00 00              	callq	0x10000807e <_sysconf+0x10000807e>
100001a89: 85 c0                       	testl	%eax, %eax
100001a8b: 74 27                       	je	0x100001ab4 <_mb_win_threads_open+0x74>
100001a8d: 89 c3                       	movl	%eax, %ebx
100001a8f: 4c 89 f7                    	movq	%r14, %rdi
100001a92: e8 6f 65 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
100001ac2: e8 a5 65 00 00              	callq	0x10000806c <_sysconf+0x10000806c>
100001ac7: 85 c0                       	testl	%eax, %eax
100001ac9: 74 1f                       	je	0x100001aea <_mb_win_threads_open+0xaa>
100001acb: 89 c3                       	movl	%eax, %ebx
100001acd: 4c 89 f7                    	movq	%r14, %rdi
100001ad0: e8 a3 65 00 00              	callq	0x100008078 <_sysconf+0x100008078>
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
100001b25: e8 5a 65 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100001b70: e8 15 65 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001b75: 85 c0                       	testl	%eax, %eax
100001b77: 75 0c                       	jne	0x100001b85 <_thread_exited+0x75>
100001b79: 48 89 df                    	movq	%rbx, %rdi
100001b7c: 5b                          	popq	%rbx
100001b7d: 41 5e                       	popq	%r14
100001b7f: 5d                          	popq	%rbp
100001b80: e9 81 64 00 00              	jmp	0x100008006 <_sysconf+0x100008006>
100001b85: e8 52 64 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100001bb3: e8 cc 64 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100001bb8: 85 c0                       	testl	%eax, %eax
100001bba: 75 1a                       	jne	0x100001bd6 <_mb_win_threads_enter+0x46>
100001bbc: 83 bb f8 00 00 00 00        	cmpl	$0x0, 0xf8(%rbx)
100001bc3: 74 24                       	je	0x100001be9 <_mb_win_threads_enter+0x59>
100001bc5: 48 89 df                    	movq	%rbx, %rdi
100001bc8: e8 bd 64 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001bcd: 85 c0                       	testl	%eax, %eax
100001bcf: b8 02 00 00 00              	movl	$0x2, %eax
100001bd4: 74 0a                       	je	0x100001be0 <_mb_win_threads_enter+0x50>
100001bd6: e8 01 64 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100001bf6: e8 65 64 00 00              	callq	0x100008060 <_sysconf+0x100008060>
100001bfb: 48 85 c0                    	testq	%rax, %rax
100001bfe: 74 1b                       	je	0x100001c1b <_mb_win_threads_enter+0x8b>
100001c00: 48 89 df                    	movq	%rbx, %rdi
100001c03: e8 82 64 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100001c2c: e8 b7 63 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
100001c31: 48 85 c0                    	testq	%rax, %rax
100001c34: 74 32                       	je	0x100001c68 <_mb_win_threads_enter+0xd8>
100001c36: 49 89 c7                    	movq	%rax, %r15
100001c39: 48 89 18                    	movq	%rbx, (%rax)
100001c3c: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100001c40: 48 89 c6                    	movq	%rax, %rsi
100001c43: e8 4e 64 00 00              	callq	0x100008096 <_sysconf+0x100008096>
100001c48: 85 c0                       	testl	%eax, %eax
100001c4a: 74 49                       	je	0x100001c95 <_mb_win_threads_enter+0x105>
100001c4c: 41 89 c6                    	movl	%eax, %r14d
100001c4f: 4c 89 ff                    	movq	%r15, %rdi
100001c52: e8 af 63 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100001c57: 48 89 df                    	movq	%rbx, %rdi
100001c5a: e8 2b 64 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001c5f: 85 c0                       	testl	%eax, %eax
100001c61: 74 1b                       	je	0x100001c7e <_mb_win_threads_enter+0xee>
100001c63: e9 6e ff ff ff              	jmp	0x100001bd6 <_mb_win_threads_enter+0x46>
100001c68: 48 89 df                    	movq	%rbx, %rdi
100001c6b: e8 1a 64 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100001ce3: e8 9c 63 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100001ce8: 85 c0                       	testl	%eax, %eax
100001cea: 75 1a                       	jne	0x100001d06 <_mb_win_threads_leave+0x46>
100001cec: 83 bb f8 00 00 00 00        	cmpl	$0x0, 0xf8(%rbx)
100001cf3: 74 24                       	je	0x100001d19 <_mb_win_threads_leave+0x59>
100001cf5: 48 89 df                    	movq	%rbx, %rdi
100001cf8: e8 8d 63 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001cfd: 85 c0                       	testl	%eax, %eax
100001cff: b8 02 00 00 00              	movl	$0x2, %eax
100001d04: 74 0a                       	je	0x100001d10 <_mb_win_threads_leave+0x50>
100001d06: e8 d1 62 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100001d26: e8 35 63 00 00              	callq	0x100008060 <_sysconf+0x100008060>
100001d2b: 48 85 c0                    	testq	%rax, %rax
100001d2e: 74 28                       	je	0x100001d58 <_mb_win_threads_leave+0x98>
100001d30: 49 89 c6                    	movq	%rax, %r14
100001d33: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100001d37: 31 f6                       	xorl	%esi, %esi
100001d39: e8 58 63 00 00              	callq	0x100008096 <_sysconf+0x100008096>
100001d3e: 85 c0                       	testl	%eax, %eax
100001d40: 74 24                       	je	0x100001d66 <_mb_win_threads_leave+0xa6>
100001d42: 41 89 c7                    	movl	%eax, %r15d
100001d45: 48 89 df                    	movq	%rbx, %rdi
100001d48: e8 3d 63 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001d4d: 85 c0                       	testl	%eax, %eax
100001d4f: 75 b5                       	jne	0x100001d06 <_mb_win_threads_leave+0x46>
100001d51: b8 04 00 00 00              	movl	$0x4, %eax
100001d56: eb 6d                       	jmp	0x100001dc5 <_mb_win_threads_leave+0x105>
100001d58: 48 89 df                    	movq	%rbx, %rdi
100001d5b: e8 2a 63 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100001da8: e8 dd 62 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001dad: 85 c0                       	testl	%eax, %eax
100001daf: 0f 85 51 ff ff ff           	jne	0x100001d06 <_mb_win_threads_leave+0x46>
100001db5: 4c 89 f7                    	movq	%r14, %rdi
100001db8: e8 49 62 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
100001e00: e8 7f 62 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100001e05: 85 c0                       	testl	%eax, %eax
100001e07: 75 27                       	jne	0x100001e30 <_mb_win_threads_close+0x50>
100001e09: 48 83 7b 48 00              	cmpq	$0x0, 0x48(%rbx)
100001e0e: 75 0d                       	jne	0x100001e1d <_mb_win_threads_close+0x3d>
100001e10: 48 83 7b 50 00              	cmpq	$0x0, 0x50(%rbx)
100001e15: 75 06                       	jne	0x100001e1d <_mb_win_threads_close+0x3d>
100001e17: 83 7b 58 00                 	cmpl	$0x0, 0x58(%rbx)
100001e1b: 74 38                       	je	0x100001e55 <_mb_win_threads_close+0x75>
100001e1d: 48 89 df                    	movq	%rbx, %rdi
100001e20: e8 65 62 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001e25: 89 c1                       	movl	%eax, %ecx
100001e27: b8 03 00 00 00              	movl	$0x3, %eax
100001e2c: 85 c9                       	testl	%ecx, %ecx
100001e2e: 74 1a                       	je	0x100001e4a <_mb_win_threads_close+0x6a>
100001e30: e8 a7 61 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100001e62: e8 23 62 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001e67: 85 c0                       	testl	%eax, %eax
100001e69: 75 c5                       	jne	0x100001e30 <_mb_win_threads_close+0x50>
100001e6b: 83 bb fc 00 00 00 00        	cmpl	$0x0, 0xfc(%rbx)
100001e72: 74 2c                       	je	0x100001ea0 <_mb_win_threads_close+0xc0>
100001e74: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
100001e78: e8 f5 61 00 00              	callq	0x100008072 <_sysconf+0x100008072>
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
100001ea3: e8 d0 61 00 00              	callq	0x100008078 <_sysconf+0x100008078>
100001ea8: 85 c0                       	testl	%eax, %eax
100001eaa: 74 09                       	je	0x100001eb5 <_mb_win_threads_close+0xd5>
100001eac: 89 c1                       	movl	%eax, %ecx
100001eae: b8 04 00 00 00              	movl	$0x4, %eax
100001eb3: eb 16                       	jmp	0x100001ecb <_mb_win_threads_close+0xeb>
100001eb5: 49 c7 07 00 00 00 00        	movq	$0x0, (%r15)
100001ebc: 48 89 df                    	movq	%rbx, %rdi
100001ebf: e8 42 61 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
100001f0a: e8 51 61 00 00              	callq	0x100008060 <_sysconf+0x100008060>
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
100001f31: e8 4e 61 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100001f36: 85 c0                       	testl	%eax, %eax
100001f38: 75 02                       	jne	0x100001f3c <_mb_win_threads_lock+0x1c>
100001f3a: 5d                          	popq	%rbp
100001f3b: c3                          	retq
100001f3c: e8 9b 60 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100001f41: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)

0000000100001f50 <_mb_win_threads_unlock>:
100001f50: 55                          	pushq	%rbp
100001f51: 48 89 e5                    	movq	%rsp, %rbp
100001f54: 48 85 ff                    	testq	%rdi, %rdi
100001f57: 74 13                       	je	0x100001f6c <_mb_win_threads_unlock+0x1c>
100001f59: 48 8b 3f                    	movq	(%rdi), %rdi
100001f5c: 48 85 ff                    	testq	%rdi, %rdi
100001f5f: 74 0b                       	je	0x100001f6c <_mb_win_threads_unlock+0x1c>
100001f61: e8 24 61 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100001f66: 85 c0                       	testl	%eax, %eax
100001f68: 75 02                       	jne	0x100001f6c <_mb_win_threads_unlock+0x1c>
100001f6a: 5d                          	popq	%rbp
100001f6b: c3                          	retq
100001f6c: e8 6b 60 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100001f71: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)

0000000100001f80 <_mb_win_threads_allocate>:
100001f80: 55                          	pushq	%rbp
100001f81: 48 89 e5                    	movq	%rsp, %rbp
100001f84: 48 89 f7                    	movq	%rsi, %rdi
100001f87: 5d                          	popq	%rbp
100001f88: e9 85 60 00 00              	jmp	0x100008012 <_sysconf+0x100008012>
100001f8d: 0f 1f 00                    	nopl	(%rax)

0000000100001f90 <_mb_win_threads_release>:
100001f90: 55                          	pushq	%rbp
100001f91: 48 89 e5                    	movq	%rsp, %rbp
100001f94: 48 89 f7                    	movq	%rsi, %rdi
100001f97: 5d                          	popq	%rbp
100001f98: e9 69 60 00 00              	jmp	0x100008006 <_sysconf+0x100008006>
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
100001fd5: e8 aa 60 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100001fda: 85 c0                       	testl	%eax, %eax
100001fdc: 0f 85 85 00 00 00           	jne	0x100002067 <_mb_win_threads_acquire+0xc7>
100001fe2: 83 bb f8 00 00 00 00        	cmpl	$0x0, 0xf8(%rbx)
100001fe9: 74 1c                       	je	0x100002007 <_mb_win_threads_acquire+0x67>
100001feb: 48 89 df                    	movq	%rbx, %rdi
100001fee: e8 97 60 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002049: e8 3c 60 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000204e: 89 c1                       	movl	%eax, %ecx
100002050: 31 c0                       	xorl	%eax, %eax
100002052: eb 0f                       	jmp	0x100002063 <_mb_win_threads_acquire+0xc3>
100002054: 48 89 df                    	movq	%rbx, %rdi
100002057: e8 2e 60 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000205c: 89 c1                       	movl	%eax, %ecx
10000205e: b8 04 00 00 00              	movl	$0x4, %eax
100002063: 85 c9                       	testl	%ecx, %ecx
100002065: 74 95                       	je	0x100001ffc <_mb_win_threads_acquire+0x5c>
100002067: e8 70 5f 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
10000209b: e8 e4 5f 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
1000020e9: e8 9c 5f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000020ee: 89 c1                       	movl	%eax, %ecx
1000020f0: 31 c0                       	xorl	%eax, %eax
1000020f2: 85 c9                       	testl	%ecx, %ecx
1000020f4: 74 1a                       	je	0x100002110 <_mb_win_threads_release_pin+0xa0>
1000020f6: eb 1d                       	jmp	0x100002115 <_mb_win_threads_release_pin+0xa5>
1000020f8: b8 01 00 00 00              	movl	$0x1, %eax
1000020fd: eb 11                       	jmp	0x100002110 <_mb_win_threads_release_pin+0xa0>
1000020ff: 48 89 df                    	movq	%rbx, %rdi
100002102: e8 83 5f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100002107: 85 c0                       	testl	%eax, %eax
100002109: b8 02 00 00 00              	movl	$0x2, %eax
10000210e: 75 05                       	jne	0x100002115 <_mb_win_threads_release_pin+0xa5>
100002110: 5b                          	popq	%rbx
100002111: 41 5e                       	popq	%r14
100002113: 5d                          	popq	%rbp
100002114: c3                          	retq
100002115: e8 c2 5e 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100002149: e8 12 5f 00 00              	callq	0x100008060 <_sysconf+0x100008060>
10000214e: 48 85 c0                    	testq	%rax, %rax
100002151: 74 10                       	je	0x100002163 <_mb_win_threads_last_error+0x43>
100002153: 48 39 18                    	cmpq	%rbx, (%rax)
100002156: 75 0b                       	jne	0x100002163 <_mb_win_threads_last_error+0x43>
100002158: 48 83 c0 1c                 	addq	$0x1c, %rax
10000215c: 48 83 c4 08                 	addq	$0x8, %rsp
100002160: 5b                          	popq	%rbx
100002161: 5d                          	popq	%rbp
100002162: c3                          	retq
100002163: e8 74 5e 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
1000021af: e8 ac 5e 00 00              	callq	0x100008060 <_sysconf+0x100008060>
1000021b4: 48 85 c0                    	testq	%rax, %rax
1000021b7: 0f 84 bf 00 00 00           	je	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
1000021bd: 49 89 c7                    	movq	%rax, %r15
1000021c0: 48 8b 18                    	movq	(%rax), %rbx
1000021c3: 4c 39 f3                    	cmpq	%r14, %rbx
1000021c6: 0f 85 b0 00 00 00           	jne	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
1000021cc: 48 85 db                    	testq	%rbx, %rbx
1000021cf: 0f 84 a7 00 00 00           	je	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
1000021d5: 48 89 df                    	movq	%rbx, %rdi
1000021d8: e8 a7 5e 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100002223: e8 62 5e 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002265: e8 20 5e 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000226a: 85 c0                       	testl	%eax, %eax
10000226c: 75 0e                       	jne	0x10000227c <_mb_win_threads_tls_alloc+0x10c>
10000226e: 44 89 f0                    	movl	%r14d, %eax
100002271: 48 83 c4 08                 	addq	$0x8, %rsp
100002275: 5b                          	popq	%rbx
100002276: 41 5e                       	popq	%r14
100002278: 41 5f                       	popq	%r15
10000227a: 5d                          	popq	%rbp
10000227b: c3                          	retq
10000227c: e8 5b 5d 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
1000022d5: e8 86 5d 00 00              	callq	0x100008060 <_sysconf+0x100008060>
1000022da: 48 85 c0                    	testq	%rax, %rax
1000022dd: 74 7f                       	je	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022df: 49 89 c7                    	movq	%rax, %r15
1000022e2: 4c 8b 30                    	movq	(%rax), %r14
1000022e5: 4d 39 e6                    	cmpq	%r12, %r14
1000022e8: 75 74                       	jne	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022ea: 4d 85 f6                    	testq	%r14, %r14
1000022ed: 74 6f                       	je	0x10000235e <_mb_win_threads_tls_free+0xce>
1000022ef: 4c 89 f7                    	movq	%r14, %rdi
1000022f2: e8 8d 5d 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
10000232f: e8 56 5d 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100002334: 89 c1                       	movl	%eax, %ecx
100002336: b8 01 00 00 00              	movl	$0x1, %eax
10000233b: eb 14                       	jmp	0x100002351 <_mb_win_threads_tls_free+0xc1>
10000233d: 41 c7 47 1c 57 00 00 00     	movl	$0x57, 0x1c(%r15)
100002345: 4c 89 f7                    	movq	%r14, %rdi
100002348: e8 3d 5d 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
10000235e: e8 79 5c 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
1000023b5: e8 a6 5c 00 00              	callq	0x100008060 <_sysconf+0x100008060>
1000023ba: 48 85 c0                    	testq	%rax, %rax
1000023bd: 74 78                       	je	0x100002437 <_mb_win_threads_tls_get+0xc7>
1000023bf: 49 89 c6                    	movq	%rax, %r14
1000023c2: 4c 8b 38                    	movq	(%rax), %r15
1000023c5: 4d 39 e7                    	cmpq	%r12, %r15
1000023c8: 75 6d                       	jne	0x100002437 <_mb_win_threads_tls_get+0xc7>
1000023ca: 4d 85 ff                    	testq	%r15, %r15
1000023cd: 74 68                       	je	0x100002437 <_mb_win_threads_tls_get+0xc7>
1000023cf: 4c 89 ff                    	movq	%r15, %rdi
1000023d2: e8 ad 5c 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
10000240a: e8 7b 5c 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000240f: 85 c0                       	testl	%eax, %eax
100002411: 74 18                       	je	0x10000242b <_mb_win_threads_tls_get+0xbb>
100002413: eb 22                       	jmp	0x100002437 <_mb_win_threads_tls_get+0xc7>
100002415: 41 c7 46 1c 57 00 00 00     	movl	$0x57, 0x1c(%r14)
10000241d: 4c 89 ff                    	movq	%r15, %rdi
100002420: e8 65 5c 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002437: e8 a0 5b 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100002489: e8 d2 5b 00 00              	callq	0x100008060 <_sysconf+0x100008060>
10000248e: 48 85 c0                    	testq	%rax, %rax
100002491: 74 7e                       	je	0x100002511 <_mb_win_threads_tls_set+0xd1>
100002493: 49 89 c7                    	movq	%rax, %r15
100002496: 4c 8b 20                    	movq	(%rax), %r12
100002499: 4d 39 ec                    	cmpq	%r13, %r12
10000249c: 75 73                       	jne	0x100002511 <_mb_win_threads_tls_set+0xd1>
10000249e: 4d 85 e4                    	testq	%r12, %r12
1000024a1: 74 6e                       	je	0x100002511 <_mb_win_threads_tls_set+0xd1>
1000024a3: 4c 89 e7                    	movq	%r12, %rdi
1000024a6: e8 d9 5b 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
1000024da: e8 ab 5b 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002504: e8 81 5b 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100002509: 89 c1                       	movl	%eax, %ecx
10000250b: 31 c0                       	xorl	%eax, %eax
10000250d: 85 c9                       	testl	%ecx, %ecx
10000250f: 74 d9                       	je	0x1000024ea <_mb_win_threads_tls_set+0xaa>
100002511: e8 c6 5a 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
1000025af: e8 34 5a 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
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
1000025f9: e8 08 5a 00 00              	callq	0x100008006 <_sysconf+0x100008006>
1000025fe: b8 02 00 00 00              	movl	$0x2, %eax
100002603: eb 88                       	jmp	0x10000258d <_mb_win_workers_open+0x4d>
100002605: b8 04 00 00 00              	movl	$0x4, %eax
10000260a: 4d 85 ff                    	testq	%r15, %r15
10000260d: 0f 84 7a ff ff ff           	je	0x10000258d <_mb_win_workers_open+0x4d>
100002613: 41 c7 07 0c 00 00 00        	movl	$0xc, (%r15)
10000261a: e9 6e ff ff ff              	jmp	0x10000258d <_mb_win_workers_open+0x4d>
10000261f: 48 89 df                    	movq	%rbx, %rdi
100002622: 31 f6                       	xorl	%esi, %esi
100002624: e8 55 5a 00 00              	callq	0x10000807e <_sysconf+0x10000807e>
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
100002676: e8 b5 59 00 00              	callq	0x100008030 <_sysconf+0x100008030>
10000267b: 85 c0                       	testl	%eax, %eax
10000267d: 75 b2                       	jne	0x100002631 <_mb_win_workers_open+0xf1>
10000267f: c7 83 e8 00 00 00 01 00 00 00       	movl	$0x1, 0xe8(%rbx)
100002689: 48 89 df                    	movq	%rbx, %rdi
10000268c: 48 83 c7 70                 	addq	$0x70, %rdi
100002690: 31 f6                       	xorl	%esi, %esi
100002692: e8 d5 59 00 00              	callq	0x10000806c <_sysconf+0x10000806c>
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
1000026ee: e8 91 59 00 00              	callq	0x100008084 <_sysconf+0x100008084>
1000026f3: 85 c0                       	testl	%eax, %eax
1000026f5: 75 35                       	jne	0x10000272c <_mb_win_workers_close+0x6c>
1000026f7: 49 83 be c8 00 00 00 00     	cmpq	$0x0, 0xc8(%r14)
1000026ff: 75 14                       	jne	0x100002715 <_mb_win_workers_close+0x55>
100002701: 49 83 be d0 00 00 00 00     	cmpq	$0x0, 0xd0(%r14)
100002709: 75 0a                       	jne	0x100002715 <_mb_win_workers_close+0x55>
10000270b: 49 83 be d8 00 00 00 00     	cmpq	$0x0, 0xd8(%r14)
100002713: 74 26                       	je	0x10000273b <_mb_win_workers_close+0x7b>
100002715: 4c 89 f7                    	movq	%r14, %rdi
100002718: e8 6d 59 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000271d: 89 c1                       	movl	%eax, %ecx
10000271f: b8 03 00 00 00              	movl	$0x3, %eax
100002724: 85 c9                       	testl	%ecx, %ecx
100002726: 0f 84 fe 00 00 00           	je	0x10000282a <_mb_win_workers_close+0x16a>
10000272c: e8 ab 58 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100002731: b8 01 00 00 00              	movl	$0x1, %eax
100002736: e9 ef 00 00 00              	jmp	0x10000282a <_mb_win_workers_close+0x16a>
10000273b: 41 c7 86 e0 00 00 00 01 00 00 00    	movl	$0x1, 0xe0(%r14)
100002746: 4c 89 f7                    	movq	%r14, %rdi
100002749: e8 3c 59 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000274e: 85 c0                       	testl	%eax, %eax
100002750: 75 da                       	jne	0x10000272c <_mb_win_workers_close+0x6c>
100002752: 41 83 be ec 00 00 00 00     	cmpl	$0x0, 0xec(%r14)
10000275a: 74 30                       	je	0x10000278c <_mb_win_workers_close+0xcc>
10000275c: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002760: e8 0d 59 00 00              	callq	0x100008072 <_sysconf+0x100008072>
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
10000279a: e8 8b 58 00 00              	callq	0x10000802a <_sysconf+0x10000802a>
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
1000027cf: e8 a4 58 00 00              	callq	0x100008078 <_sysconf+0x100008078>
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
100002811: e8 f0 57 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
100002875: e8 0a 58 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
10000289a: e8 eb 57 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
1000028e2: e8 a3 57 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000028e7: 89 c1                       	movl	%eax, %ecx
1000028e9: 31 c0                       	xorl	%eax, %eax
1000028eb: 85 c9                       	testl	%ecx, %ecx
1000028ed: 74 b9                       	je	0x1000028a8 <_mb_win_workers_acquire+0x68>
1000028ef: e8 e8 56 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100002933: e8 4c 57 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
10000298f: e8 f6 56 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100002994: 89 c1                       	movl	%eax, %ecx
100002996: 31 c0                       	xorl	%eax, %eax
100002998: 85 c9                       	testl	%ecx, %ecx
10000299a: 74 1a                       	je	0x1000029b6 <_mb_win_workers_release_pin+0xb6>
10000299c: eb 1d                       	jmp	0x1000029bb <_mb_win_workers_release_pin+0xbb>
10000299e: b8 01 00 00 00              	movl	$0x1, %eax
1000029a3: eb 11                       	jmp	0x1000029b6 <_mb_win_workers_release_pin+0xb6>
1000029a5: 48 89 df                    	movq	%rbx, %rdi
1000029a8: e8 dd 56 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000029ad: 85 c0                       	testl	%eax, %eax
1000029af: b8 02 00 00 00              	movl	$0x2, %eax
1000029b4: 75 05                       	jne	0x1000029bb <_mb_win_workers_release_pin+0xbb>
1000029b6: 5b                          	popq	%rbx
1000029b7: 41 5e                       	popq	%r14
1000029b9: 5d                          	popq	%rbp
1000029ba: c3                          	retq
1000029bb: e8 1c 56 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>

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
1000029f0: e8 6b 56 00 00              	callq	0x100008060 <_sysconf+0x100008060>
1000029f5: 49 89 c7                    	movq	%rax, %r15
1000029f8: 41 be 02 00 00 00           	movl	$0x2, %r14d
1000029fe: 41 83 ff ff                 	cmpl	$-0x1, %r15d
100002a02: 74 69                       	je	0x100002a6d <_mb_win_workers_enter_call+0xad>
100002a04: 48 89 df                    	movq	%rbx, %rdi
100002a07: e8 78 56 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100002a0c: 85 c0                       	testl	%eax, %eax
100002a0e: 75 69                       	jne	0x100002a79 <_mb_win_workers_enter_call+0xb9>
100002a10: 83 bb e0 00 00 00 00        	cmpl	$0x0, 0xe0(%rbx)
100002a17: 74 0e                       	je	0x100002a27 <_mb_win_workers_enter_call+0x67>
100002a19: 48 89 df                    	movq	%rbx, %rdi
100002a1c: e8 69 56 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002a3f: e8 52 56 00 00              	callq	0x100008096 <_sysconf+0x100008096>
100002a44: 41 89 c4                    	movl	%eax, %r12d
100002a47: 44 09 f8                    	orl	%r15d, %eax
100002a4a: 75 07                       	jne	0x100002a53 <_mb_win_workers_enter_call+0x93>
100002a4c: 48 ff 83 d8 00 00 00        	incq	0xd8(%rbx)
100002a53: 48 89 df                    	movq	%rbx, %rdi
100002a56: e8 2f 56 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002a79: e8 5e 55 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100002a9f: e8 bc 55 00 00              	callq	0x100008060 <_sysconf+0x100008060>
100002aa4: 49 89 c7                    	movq	%rax, %r15
100002aa7: 45 85 ff                    	testl	%r15d, %r15d
100002aaa: 74 57                       	je	0x100002b03 <_mb_win_workers_leave_call+0x83>
100002aac: 48 89 df                    	movq	%rbx, %rdi
100002aaf: e8 d0 55 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100002ab4: 85 c0                       	testl	%eax, %eax
100002ab6: 75 5b                       	jne	0x100002b13 <_mb_win_workers_leave_call+0x93>
100002ab8: 48 8b 7b 70                 	movq	0x70(%rbx), %rdi
100002abc: 44 89 fe                    	movl	%r15d, %esi
100002abf: ff ce                       	decl	%esi
100002ac1: e8 d0 55 00 00              	callq	0x100008096 <_sysconf+0x100008096>
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
100002aed: e8 98 55 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002b13: e8 c4 54 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100002b38: e8 23 55 00 00              	callq	0x100008060 <_sysconf+0x100008060>
100002b3d: 48 89 c3                    	movq	%rax, %rbx
100002b40: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002b44: 31 f6                       	xorl	%esi, %esi
100002b46: e8 4b 55 00 00              	callq	0x100008096 <_sysconf+0x100008096>
100002b4b: 85 c0                       	testl	%eax, %eax
100002b4d: 75 07                       	jne	0x100002b56 <_mb_win_workers_suspend_calls+0x36>
100002b4f: 89 d8                       	movl	%ebx, %eax
100002b51: 5b                          	popq	%rbx
100002b52: 41 5e                       	popq	%r14
100002b54: 5d                          	popq	%rbp
100002b55: c3                          	retq
100002b56: e8 81 54 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100002b7a: e8 e1 54 00 00              	callq	0x100008060 <_sysconf+0x100008060>
100002b7f: 85 c0                       	testl	%eax, %eax
100002b81: 75 14                       	jne	0x100002b97 <_mb_win_workers_restore_calls+0x37>
100002b83: 49 8b 7e 70                 	movq	0x70(%r14), %rdi
100002b87: 89 de                       	movl	%ebx, %esi
100002b89: e8 08 55 00 00              	callq	0x100008096 <_sysconf+0x100008096>
100002b8e: 85 c0                       	testl	%eax, %eax
100002b90: 75 05                       	jne	0x100002b97 <_mb_win_workers_restore_calls+0x37>
100002b92: 5b                          	popq	%rbx
100002b93: 41 5e                       	popq	%r14
100002b95: 5d                          	popq	%rbp
100002b96: c3                          	retq
100002b97: e8 40 54 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100002bcc: e8 b3 54 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100002c39: e8 4c 54 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002c6a: e8 f7 53 00 00              	callq	0x100008066 <_sysconf+0x100008066>
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
100002c97: e8 e8 53 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100002d56: e8 ab 52 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100002d5b: 4d 89 f4                    	movq	%r14, %r12
100002d5e: e9 b8 fe ff ff              	jmp	0x100002c1b <_mb_win_workers_collect+0x7b>
100002d63: 41 bf 01 00 00 00           	movl	$0x1, %r15d
100002d69: eb 17                       	jmp	0x100002d82 <_mb_win_workers_collect+0x1e2>
100002d6b: 41 bf 02 00 00 00           	movl	$0x2, %r15d
100002d71: eb 0f                       	jmp	0x100002d82 <_mb_win_workers_collect+0x1e2>
100002d73: 45 31 ff                    	xorl	%r15d, %r15d
100002d76: 48 89 df                    	movq	%rbx, %rdi
100002d79: e8 0c 53 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002d94: e8 43 52 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100002e15: e8 46 52 00 00              	callq	0x100008060 <_sysconf+0x100008060>
100002e1a: 85 c0                       	testl	%eax, %eax
100002e1c: 0f 84 90 00 00 00           	je	0x100002eb2 <_mb_win_workers_create+0x112>
100002e22: bf 01 00 00 00              	movl	$0x1, %edi
100002e27: be 70 00 00 00              	movl	$0x70, %esi
100002e2c: e8 b7 51 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
100002e31: 48 85 c0                    	testq	%rax, %rax
100002e34: 0f 84 42 01 00 00           	je	0x100002f7c <_mb_win_workers_create+0x1dc>
100002e3a: 49 89 c7                    	movq	%rax, %r15
100002e3d: 4c 89 30                    	movq	%r14, (%rax)
100002e40: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
100002e44: 49 89 47 18                 	movq	%rax, 0x18(%r15)
100002e48: 4d 89 6f 20                 	movq	%r13, 0x20(%r15)
100002e4c: 41 c7 47 48 01 00 00 00     	movl	$0x1, 0x48(%r15)
100002e54: 4c 89 f7                    	movq	%r14, %rdi
100002e57: e8 28 52 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100002e5c: 85 c0                       	testl	%eax, %eax
100002e5e: 0f 85 8a 01 00 00           	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002e64: 41 83 be e0 00 00 00 00     	cmpl	$0x0, 0xe0(%r14)
100002e6c: 0f 85 f6 00 00 00           	jne	0x100002f68 <_mb_win_workers_create+0x1c8>
100002e72: 49 8b 86 c8 00 00 00        	movq	0xc8(%r14), %rax
100002e79: 49 3b 86 a0 00 00 00        	cmpq	0xa0(%r14), %rax
100002e80: 0f 83 e2 00 00 00           	jae	0x100002f68 <_mb_win_workers_create+0x1c8>
100002e86: 8b 05 74 71 00 00           	movl	0x7174(%rip), %eax      ## 0x10000a000 <_next_identity>
100002e8c: 0f 1f 40 00                 	nopl	(%rax)
100002e90: 8d 48 ff                    	leal	-0x1(%rax), %ecx
100002e93: 83 f9 fd                    	cmpl	$-0x3, %ecx
100002e96: 77 27                       	ja	0x100002ebf <_mb_win_workers_create+0x11f>
100002e98: 8d 48 01                    	leal	0x1(%rax), %ecx
100002e9b: f0                          	lock
100002e9c: 0f b1 0d 5d 71 00 00        	cmpxchgl	%ecx, 0x715d(%rip)      ## 0x10000a000 <_next_identity>
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
100002ef7: e8 8e 51 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100002f24: e8 5b 51 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100002f29: 85 c0                       	testl	%eax, %eax
100002f2b: 0f 85 bd 00 00 00           	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002f31: 41 c7 47 5c 01 00 00 00     	movl	$0x1, 0x5c(%r15)
100002f39: 41 c7 47 48 00 00 00 00     	movl	$0x0, 0x48(%r15)
100002f41: 4c 89 f7                    	movq	%r14, %rdi
100002f44: e8 41 51 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100002f49: 85 c0                       	testl	%eax, %eax
100002f4b: 0f 85 9d 00 00 00           	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002f51: 48 8b 7d c8                 	movq	-0x38(%rbp), %rdi
100002f55: 31 f6                       	xorl	%esi, %esi
100002f57: e8 44 fc ff ff              	callq	0x100002ba0 <_mb_win_workers_collect>
100002f5c: 31 c0                       	xorl	%eax, %eax
100002f5e: 41 c7 04 24 08 00 00 00     	movl	$0x8, (%r12)
100002f66: eb 1e                       	jmp	0x100002f86 <_mb_win_workers_create+0x1e6>
100002f68: 4c 89 f7                    	movq	%r14, %rdi
100002f6b: e8 1a 51 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100002f70: 85 c0                       	testl	%eax, %eax
100002f72: 75 7a                       	jne	0x100002fee <_mb_win_workers_create+0x24e>
100002f74: 4c 89 ff                    	movq	%r15, %rdi
100002f77: e8 8a 50 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
100002faf: e8 a0 50 00 00              	callq	0x100008054 <_sysconf+0x100008054>
100002fb4: 41 89 c5                    	movl	%eax, %r13d
100002fb7: 4c 89 f7                    	movq	%r14, %rdi
100002fba: e8 c5 50 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100002fe5: e8 58 50 00 00              	callq	0x100008042 <_sysconf+0x100008042>
100002fea: 85 c0                       	testl	%eax, %eax
100002fec: 74 ea                       	je	0x100002fd8 <_mb_win_workers_create+0x238>
100002fee: e8 e9 4f 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100002ff3: 41 83 7f 54 00              	cmpl	$0x0, 0x54(%r15)
100002ff8: 0f 85 3b ff ff ff           	jne	0x100002f39 <_mb_win_workers_create+0x199>
100002ffe: 41 c7 47 64 01 00 00 00     	movl	$0x1, 0x64(%r15)
100003006: 41 c7 47 58 01 00 00 00     	movl	$0x1, 0x58(%r15)
10000300e: 41 c7 47 48 00 00 00 00     	movl	$0x0, 0x48(%r15)
100003016: 49 8b 47 30                 	movq	0x30(%r15), %rax
10000301a: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
10000301e: 45 8b 7f 38                 	movl	0x38(%r15), %r15d
100003022: 4c 89 ef                    	movq	%r13, %rdi
100003025: e8 fa 4f 00 00              	callq	0x100008024 <_sysconf+0x100008024>
10000302a: 85 c0                       	testl	%eax, %eax
10000302c: 75 c0                       	jne	0x100002fee <_mb_win_workers_create+0x24e>
10000302e: 4c 89 f7                    	movq	%r14, %rdi
100003031: e8 54 50 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
1000030a9: e8 d6 4f 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
1000030da: e8 45 4f 00 00              	callq	0x100008024 <_sysconf+0x100008024>
1000030df: 85 c0                       	testl	%eax, %eax
1000030e1: 75 23                       	jne	0x100003106 <_run_worker+0xa6>
1000030e3: 45 84 ed                    	testb	%r13b, %r13b
1000030e6: 74 23                       	je	0x10000310b <_run_worker+0xab>
1000030e8: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)
1000030f0: 41 83 7e 58 00              	cmpl	$0x0, 0x58(%r14)
1000030f5: 75 22                       	jne	0x100003119 <_run_worker+0xb9>
1000030f7: 4c 89 e7                    	movq	%r12, %rdi
1000030fa: 48 89 de                    	movq	%rbx, %rsi
1000030fd: e8 40 4f 00 00              	callq	0x100008042 <_sysconf+0x100008042>
100003102: 85 c0                       	testl	%eax, %eax
100003104: 74 ea                       	je	0x1000030f0 <_run_worker+0x90>
100003106: e8 d1 4e 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
10000310b: 48 89 df                    	movq	%rbx, %rdi
10000310e: e8 77 4f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003113: 85 c0                       	testl	%eax, %eax
100003115: 74 76                       	je	0x10000318d <_run_worker+0x12d>
100003117: eb ed                       	jmp	0x100003106 <_run_worker+0xa6>
100003119: 48 89 df                    	movq	%rbx, %rdi
10000311c: e8 69 4f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
10000314f: e8 30 4f 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100003154: 85 c0                       	testl	%eax, %eax
100003156: 75 ae                       	jne	0x100003106 <_run_worker+0xa6>
100003158: 45 89 6e 3c                 	movl	%r13d, 0x3c(%r14)
10000315c: 41 c7 46 5c 01 00 00 00     	movl	$0x1, 0x5c(%r14)
100003164: 4c 89 e7                    	movq	%r12, %rdi
100003167: e8 b8 4e 00 00              	callq	0x100008024 <_sysconf+0x100008024>
10000316c: 85 c0                       	testl	%eax, %eax
10000316e: 75 96                       	jne	0x100003106 <_run_worker+0xa6>
100003170: 48 89 df                    	movq	%rbx, %rdi
100003173: e8 12 4f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
1000031f3: e8 8c 4e 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100003266: e8 83 4d 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
10000326b: 85 c0                       	testl	%eax, %eax
10000326d: 74 44                       	je	0x1000032b3 <_mb_win_workers_wait+0x103>
10000326f: e8 56 4d 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100003274: 83 38 00                    	cmpl	$0x0, (%rax)
100003277: 74 57                       	je	0x1000032d0 <_mb_win_workers_wait+0x120>
100003279: e8 4c 4d 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
10000327e: 83 38 00                    	cmpl	$0x0, (%rax)
100003281: 0f 95 c0                    	setne	%al
100003284: 48 c7 45 d0 00 00 00 00     	movq	$0x0, -0x30(%rbp)
10000328c: eb 65                       	jmp	0x1000032f3 <_mb_win_workers_wait+0x143>
10000328e: 4c 89 f7                    	movq	%r14, %rdi
100003291: e8 f4 4d 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
10000332f: e8 0e 4d 00 00              	callq	0x100008042 <_sysconf+0x100008042>
100003334: 45 31 ed                    	xorl	%r13d, %r13d
100003337: 85 c0                       	testl	%eax, %eax
100003339: 74 d5                       	je	0x100003310 <_mb_win_workers_wait+0x160>
10000333b: e9 06 01 00 00              	jmp	0x100003446 <_mb_win_workers_wait+0x296>
100003340: 41 bd 02 01 00 00           	movl	$0x102, %r13d           ## imm = 0x102
100003346: 45 85 ff                    	testl	%r15d, %r15d
100003349: 0f 84 fd 00 00 00           	je	0x10000344c <_mb_win_workers_wait+0x29c>
10000334f: bf 06 00 00 00              	movl	$0x6, %edi
100003354: 48 8d 75 b0                 	leaq	-0x50(%rbp), %rsi
100003358: e8 91 4c 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
10000335d: 85 c0                       	testl	%eax, %eax
10000335f: 74 17                       	je	0x100003378 <_mb_win_workers_wait+0x1c8>
100003361: e8 64 4c 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100003366: 83 38 00                    	cmpl	$0x0, (%rax)
100003369: 74 51                       	je	0x1000033bc <_mb_win_workers_wait+0x20c>
10000336b: e8 5a 4c 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
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
1000033e0: e8 a5 4c 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000033e5: 85 c0                       	testl	%eax, %eax
1000033e7: 0f 85 96 00 00 00           	jne	0x100003483 <_mb_win_workers_wait+0x2d3>
1000033ed: 49 81 fd 40 42 0f 00        	cmpq	$0xf4240, %r13          ## imm = 0xF4240
1000033f4: b8 40 42 0f 00              	movl	$0xf4240, %eax          ## imm = 0xF4240
1000033f9: 4c 0f 43 e8                 	cmovaeq	%rax, %r13
1000033fd: 4c 89 ef                    	movq	%r13, %rdi
100003400: e8 8b 00 00 00              	callq	0x100003490 <_pause_ns>
100003405: 89 45 cc                    	movl	%eax, -0x34(%rbp)
100003408: 4c 89 f7                    	movq	%r14, %rdi
10000340b: e8 74 4c 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100003453: e8 32 4c 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100003483: e8 54 4b 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
1000034cf: e8 4a 4b 00 00              	callq	0x10000801e <_sysconf+0x10000801e>
1000034d4: 31 db                       	xorl	%ebx, %ebx
1000034d6: 85 c0                       	testl	%eax, %eax
1000034d8: 74 51                       	je	0x10000352b <_pause_ns+0x9b>
1000034da: 4c 8d 75 d0                 	leaq	-0x30(%rbp), %r14
1000034de: 4c 8d 7d c0                 	leaq	-0x40(%rbp), %r15
1000034e2: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
1000034f0: e8 d5 4a 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
1000034f5: 83 38 04                    	cmpl	$0x4, (%rax)
1000034f8: 75 19                       	jne	0x100003513 <_pause_ns+0x83>
1000034fa: 0f 10 45 c0                 	movups	-0x40(%rbp), %xmm0
1000034fe: 0f 29 45 d0                 	movaps	%xmm0, -0x30(%rbp)
100003502: 4c 89 f7                    	movq	%r14, %rdi
100003505: 4c 89 fe                    	movq	%r15, %rsi
100003508: e8 11 4b 00 00              	callq	0x10000801e <_sysconf+0x10000801e>
10000350d: 85 c0                       	testl	%eax, %eax
10000350f: 75 df                       	jne	0x1000034f0 <_pause_ns+0x60>
100003511: eb 18                       	jmp	0x10000352b <_pause_ns+0x9b>
100003513: e8 b2 4a 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100003518: 83 38 00                    	cmpl	$0x0, (%rax)
10000351b: 74 09                       	je	0x100003526 <_pause_ns+0x96>
10000351d: e8 a8 4a 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
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
10000357d: e8 02 4b 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
1000035c4: e8 c1 4a 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000035c9: 85 c0                       	testl	%eax, %eax
1000035cb: 75 2e                       	jne	0x1000035fb <_mb_win_workers_exit_code+0xbb>
1000035cd: 41 c7 07 06 00 00 00        	movl	$0x6, (%r15)
1000035d4: 31 c0                       	xorl	%eax, %eax
1000035d6: eb 1a                       	jmp	0x1000035f2 <_mb_win_workers_exit_code+0xb2>
1000035d8: 41 bf 03 01 00 00           	movl	$0x103, %r15d           ## imm = 0x103
1000035de: 4c 89 f7                    	movq	%r14, %rdi
1000035e1: e8 a4 4a 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
1000035fb: e8 dc 49 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>

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
100003634: e8 4b 4a 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100003674: e8 11 4a 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003679: 89 c1                       	movl	%eax, %ecx
10000367b: b8 01 00 00 00              	movl	$0x1, %eax
100003680: 85 c9                       	testl	%ecx, %ecx
100003682: 74 17                       	je	0x10000369b <_mb_win_workers_close_handle+0x9b>
100003684: eb 20                       	jmp	0x1000036a6 <_mb_win_workers_close_handle+0xa6>
100003686: 48 89 df                    	movq	%rbx, %rdi
100003689: e8 fc 49 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
1000036a6: e8 31 49 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100003703: e8 94 49 00 00              	callq	0x10000809c <_sysconf+0x10000809c>
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
100003746: e8 d3 48 00 00              	callq	0x10000801e <_sysconf+0x10000801e>
10000374b: 85 c0                       	testl	%eax, %eax
10000374d: 74 e1                       	je	0x100003730 <_mb_win_workers_sleep+0x80>
10000374f: e8 76 48 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100003754: 83 38 04                    	cmpl	$0x4, (%rax)
100003757: 75 17                       	jne	0x100003770 <_mb_win_workers_sleep+0xc0>
100003759: 48 89 df                    	movq	%rbx, %rdi
10000375c: 4c 89 f6                    	movq	%r14, %rsi
10000375f: e8 4c 48 00 00              	callq	0x100007fb0 <_mb_win_workers_sleep.cold.1>
100003764: a8 01                       	testb	$0x1, %al
100003766: 74 e7                       	je	0x10000374f <_mb_win_workers_sleep+0x9f>
100003768: eb c6                       	jmp	0x100003730 <_mb_win_workers_sleep+0x80>
10000376a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)
100003770: e8 55 48 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100003775: 83 38 00                    	cmpl	$0x0, (%rax)
100003778: 74 0a                       	je	0x100003784 <_mb_win_workers_sleep+0xd4>
10000377a: e8 4b 48 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
10000377f: 83 38 00                    	cmpl	$0x0, (%rax)
100003782: 74 ac                       	je	0x100003730 <_mb_win_workers_sleep+0x80>
100003784: e8 53 48 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
1000037fc: e8 c9 47 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100003801: c7 00 00 00 00 00           	movl	$0x0, (%rax)
100003807: bf 3a 00 00 00              	movl	$0x3a, %edi
10000380c: e8 9d 48 00 00              	callq	0x1000080ae <_sysconf+0x1000080ae>
100003811: 48 85 c0                    	testq	%rax, %rax
100003814: 7e 5d                       	jle	0x100003873 <_mb_win_critical_open+0xc3>
100003816: 49 89 c6                    	movq	%rax, %r14
100003819: 4b 8d 04 a4                 	leaq	(%r12,%r12,4), %rax
10000381d: 48 8d 34 c5 b8 00 00 00     	leaq	0xb8(,%rax,8), %rsi
100003825: bf 01 00 00 00              	movl	$0x1, %edi
10000382a: e8 b9 47 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
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
100003864: e8 9d 47 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100003869: b8 02 00 00 00              	movl	$0x2, %eax
10000386e: e9 cc 00 00 00              	jmp	0x10000393f <_mb_win_critical_open+0x18f>
100003873: e8 52 47 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100003878: 83 38 00                    	cmpl	$0x0, (%rax)
10000387b: 0f 84 8c 00 00 00           	je	0x10000390d <_mb_win_critical_open+0x15d>
100003881: e8 44 47 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
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
1000038be: e8 bb 47 00 00              	callq	0x10000807e <_sysconf+0x10000807e>
1000038c3: 85 c0                       	testl	%eax, %eax
1000038c5: 75 18                       	jne	0x1000038df <_mb_win_critical_open+0x12f>
1000038c7: c7 83 b0 00 00 00 01 00 00 00       	movl	$0x1, 0xb0(%rbx)
1000038d1: 48 89 df                    	movq	%rbx, %rdi
1000038d4: 48 83 c7 40                 	addq	$0x40, %rdi
1000038d8: 31 f6                       	xorl	%esi, %esi
1000038da: e8 51 47 00 00              	callq	0x100008030 <_sysconf+0x100008030>
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
100003983: e8 fc 46 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100003988: 85 c0                       	testl	%eax, %eax
10000398a: 75 2b                       	jne	0x1000039b7 <_mb_win_critical_close+0x67>
10000398c: 49 83 bf a0 00 00 00 00     	cmpq	$0x0, 0xa0(%r15)
100003994: 75 0a                       	jne	0x1000039a0 <_mb_win_critical_close+0x50>
100003996: 49 83 bf 98 00 00 00 00     	cmpq	$0x0, 0x98(%r15)
10000399e: 74 26                       	je	0x1000039c6 <_mb_win_critical_close+0x76>
1000039a0: 4c 89 ff                    	movq	%r15, %rdi
1000039a3: e8 e2 46 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000039a8: 89 c1                       	movl	%eax, %ecx
1000039aa: b8 03 00 00 00              	movl	$0x3, %eax
1000039af: 85 c9                       	testl	%ecx, %ecx
1000039b1: 0f 84 14 01 00 00           	je	0x100003acb <_mb_win_critical_close+0x17b>
1000039b7: e8 20 46 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100003a03: e8 82 46 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003a08: 85 c0                       	testl	%eax, %eax
100003a0a: 75 ab                       	jne	0x1000039b7 <_mb_win_critical_close+0x67>
100003a0c: 41 83 bf b4 00 00 00 00     	cmpl	$0x0, 0xb4(%r15)
100003a14: 74 2f                       	je	0x100003a45 <_mb_win_critical_close+0xf5>
100003a16: 49 8d 7f 40                 	leaq	0x40(%r15), %rdi
100003a1a: e8 0b 46 00 00              	callq	0x10000802a <_sysconf+0x10000802a>
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
100003a5a: e8 19 46 00 00              	callq	0x100008078 <_sysconf+0x100008078>
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
100003ab2: e8 4f 45 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
100003b1e: e8 61 45 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100003b23: 85 c0                       	testl	%eax, %eax
100003b25: 75 68                       	jne	0x100003b8f <_mb_win_critical_acquire+0xaf>
100003b27: 49 83 3f 00                 	cmpq	$0x0, (%r15)
100003b2b: 75 0a                       	jne	0x100003b37 <_mb_win_critical_acquire+0x57>
100003b2d: 4c 89 f8                    	movq	%r15, %rax
100003b30: 49 83 7f 08 00              	cmpq	$0x0, 0x8(%r15)
100003b35: 74 1c                       	je	0x100003b53 <_mb_win_critical_acquire+0x73>
100003b37: 48 89 df                    	movq	%rbx, %rdi
100003b3a: e8 4b 45 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100003b82: e8 03 45 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003b87: 89 c1                       	movl	%eax, %ecx
100003b89: 31 c0                       	xorl	%eax, %eax
100003b8b: 85 c9                       	testl	%ecx, %ecx
100003b8d: 74 b9                       	je	0x100003b48 <_mb_win_critical_acquire+0x68>
100003b8f: e8 48 44 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100003bd3: e8 ac 44 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100003c2f: e8 56 44 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003c34: 89 c1                       	movl	%eax, %ecx
100003c36: 31 c0                       	xorl	%eax, %eax
100003c38: 85 c9                       	testl	%ecx, %ecx
100003c3a: 74 1a                       	je	0x100003c56 <_mb_win_critical_release_pin+0xb6>
100003c3c: eb 1d                       	jmp	0x100003c5b <_mb_win_critical_release_pin+0xbb>
100003c3e: b8 01 00 00 00              	movl	$0x1, %eax
100003c43: eb 11                       	jmp	0x100003c56 <_mb_win_critical_release_pin+0xb6>
100003c45: 48 89 df                    	movq	%rbx, %rdi
100003c48: e8 3d 44 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003c4d: 85 c0                       	testl	%eax, %eax
100003c4f: b8 02 00 00 00              	movl	$0x2, %eax
100003c54: 75 05                       	jne	0x100003c5b <_mb_win_critical_release_pin+0xbb>
100003c56: 5b                          	popq	%rbx
100003c57: 41 5e                       	popq	%r14
100003c59: 5d                          	popq	%rbp
100003c5a: c3                          	retq
100003c5b: e8 7c 43 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>

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
100003cc5: e8 ba 43 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100003d33: e8 52 43 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100003d8a: e8 fb 42 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100003e12: e8 73 42 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003e17: 89 c1                       	movl	%eax, %ecx
100003e19: 31 c0                       	xorl	%eax, %eax
100003e1b: eb 0f                       	jmp	0x100003e2c <_mb_win_critical_initialize+0x1cc>
100003e1d: 4c 89 ff                    	movq	%r15, %rdi
100003e20: e8 65 42 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100003e3f: e8 98 41 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100003eab: e8 e0 41 00 00              	callq	0x100008090 <_sysconf+0x100008090>
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
100003ee2: e8 73 41 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100003ee7: 85 c0                       	testl	%eax, %eax
100003ee9: 0f 85 89 00 00 00           	jne	0x100003f78 <_mb_win_critical_enter+0x128>
100003eef: 41 ff cc                    	decl	%r12d
100003ef2: 74 23                       	je	0x100003f17 <_mb_win_critical_enter+0xc7>
100003ef4: 48 89 df                    	movq	%rbx, %rdi
100003ef7: e8 8e 41 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003efc: 85 c0                       	testl	%eax, %eax
100003efe: 0f 85 b2 00 00 00           	jne	0x100003fb6 <_mb_win_critical_enter+0x166>
100003f04: f3 90                       	pause
100003f06: 48 89 df                    	movq	%rbx, %rdi
100003f09: e8 76 41 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100003f2f: e8 0e 41 00 00              	callq	0x100008042 <_sysconf+0x100008042>
100003f34: 49 ff 4d 20                 	decq	0x20(%r13)
100003f38: 45 31 e4                    	xorl	%r12d, %r12d
100003f3b: 85 c0                       	testl	%eax, %eax
100003f3d: b8 05 00 00 00              	movl	$0x5, %eax
100003f42: 44 0f 45 f0                 	cmovnel	%eax, %r14d
100003f46: 0f 84 76 ff ff ff           	je	0x100003ec2 <_mb_win_critical_enter+0x72>
100003f4c: eb 2a                       	jmp	0x100003f78 <_mb_win_critical_enter+0x128>
100003f4e: 48 89 df                    	movq	%rbx, %rdi
100003f51: e8 34 41 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100003fad: e8 d8 40 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100003fb2: 85 c0                       	testl	%eax, %eax
100003fb4: 74 aa                       	je	0x100003f60 <_mb_win_critical_enter+0x110>
100003fb6: e8 21 40 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100004017: e8 68 40 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100004062: e8 23 40 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004067: 85 c0                       	testl	%eax, %eax
100004069: b8 02 00 00 00              	movl	$0x2, %eax
10000406e: 75 09                       	jne	0x100004079 <_acquire_state+0xb9>
100004070: 5b                          	popq	%rbx
100004071: 41 5c                       	popq	%r12
100004073: 41 5e                       	popq	%r14
100004075: 41 5f                       	popq	%r15
100004077: 5d                          	popq	%rbp
100004078: c3                          	retq
100004079: e8 5e 3f 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
1000040a8: e8 e3 3f 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000040ad: 49 89 c6                    	movq	%rax, %r14
1000040b0: 4c 8b 7d d8                 	movq	-0x28(%rbp), %r15
1000040b4: 41 83 7f 10 00              	cmpl	$0x0, 0x10(%r15)
1000040b9: 74 10                       	je	0x1000040cb <_mb_win_critical_try_enter+0x4b>
1000040bb: 49 8b 7f 08                 	movq	0x8(%r15), %rdi
1000040bf: 4c 89 f6                    	movq	%r14, %rsi
1000040c2: e8 93 3f 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
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
10000410f: e8 76 3f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004130: e8 a7 3e 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
10000416b: e8 20 3f 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100004170: 48 89 df                    	movq	%rbx, %rdi
100004173: 48 89 c6                    	movq	%rax, %rsi
100004176: e8 df 3e 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
10000417b: 85 c0                       	testl	%eax, %eax
10000417d: 74 26                       	je	0x1000041a5 <_mb_win_critical_leave+0x65>
10000417f: 41 ff 4e 10                 	decl	0x10(%r14)
100004183: 75 11                       	jne	0x100004196 <_mb_win_critical_leave+0x56>
100004185: 48 8b 7d e8                 	movq	-0x18(%rbp), %rdi
100004189: 48 83 c7 40                 	addq	$0x40, %rdi
10000418d: e8 92 3e 00 00              	callq	0x100008024 <_sysconf+0x100008024>
100004192: 85 c0                       	testl	%eax, %eax
100004194: 75 2c                       	jne	0x1000041c2 <_mb_win_critical_leave+0x82>
100004196: 48 8b 7d e8                 	movq	-0x18(%rbp), %rdi
10000419a: e8 eb 3e 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000419f: 89 c1                       	movl	%eax, %ecx
1000041a1: 31 c0                       	xorl	%eax, %eax
1000041a3: eb 10                       	jmp	0x1000041b5 <_mb_win_critical_leave+0x75>
1000041a5: 48 8b 7d e8                 	movq	-0x18(%rbp), %rdi
1000041a9: e8 dc 3e 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000041ae: 89 c1                       	movl	%eax, %ecx
1000041b0: b8 02 00 00 00              	movl	$0x2, %eax
1000041b5: 85 c9                       	testl	%ecx, %ecx
1000041b7: 75 09                       	jne	0x1000041c2 <_mb_win_critical_leave+0x82>
1000041b9: 48 83 c4 10                 	addq	$0x10, %rsp
1000041bd: 5b                          	popq	%rbx
1000041be: 41 5e                       	popq	%r14
1000041c0: 5d                          	popq	%rbp
1000041c1: c3                          	retq
1000041c2: e8 15 3e 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100004205: e8 80 3e 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004231: e8 54 3e 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004236: 89 c1                       	movl	%eax, %ecx
100004238: 31 c0                       	xorl	%eax, %eax
10000423a: 85 c9                       	testl	%ecx, %ecx
10000423c: 74 d7                       	je	0x100004215 <_mb_win_critical_delete+0x45>
10000423e: e8 99 3d 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100004295: e8 f0 3d 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
1000042b5: e8 22 3d 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100004301: e8 e2 3c 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
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
10000432b: e8 d6 3c 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100004330: b8 05 00 00 00              	movl	$0x5, %eax
100004335: e9 c5 00 00 00              	jmp	0x1000043ff <_mb_win_address_wait_open+0x13f>
10000433a: b8 04 00 00 00              	movl	$0x4, %eax
10000433f: 4d 85 f6                    	testq	%r14, %r14
100004342: 0f 84 b7 00 00 00           	je	0x1000043ff <_mb_win_address_wait_open+0x13f>
100004348: 41 c7 06 0c 00 00 00        	movl	$0xc, (%r14)
10000434f: e9 ab 00 00 00              	jmp	0x1000043ff <_mb_win_address_wait_open+0x13f>
100004354: 48 89 df                    	movq	%rbx, %rdi
100004357: 31 f6                       	xorl	%esi, %esi
100004359: e8 20 3d 00 00              	callq	0x10000807e <_sysconf+0x10000807e>
10000435e: 85 c0                       	testl	%eax, %eax
100004360: 74 0a                       	je	0x10000436c <_mb_win_address_wait_open+0xac>
100004362: 85 c0                       	testl	%eax, %eax
100004364: 74 1d                       	je	0x100004383 <_mb_win_address_wait_open+0xc3>
100004366: 85 c0                       	testl	%eax, %eax
100004368: 75 5a                       	jne	0x1000043c4 <_mb_win_address_wait_open+0x104>
10000436a: eb 32                       	jmp	0x10000439e <_mb_win_address_wait_open+0xde>
10000436c: c7 83 c0 00 00 00 01 00 00 00       	movl	$0x1, 0xc0(%rbx)
100004376: 48 8d 7b 70                 	leaq	0x70(%rbx), %rdi
10000437a: e8 cf 3c 00 00              	callq	0x10000804e <_sysconf+0x10000804e>
10000437f: 85 c0                       	testl	%eax, %eax
100004381: 75 e3                       	jne	0x100004366 <_mb_win_address_wait_open+0xa6>
100004383: c7 83 c8 00 00 00 01 00 00 00       	movl	$0x1, 0xc8(%rbx)
10000438d: 48 8d 7b 40                 	leaq	0x40(%rbx), %rdi
100004391: 48 8d 73 70                 	leaq	0x70(%rbx), %rsi
100004395: e8 96 3c 00 00              	callq	0x100008030 <_sysconf+0x100008030>
10000439a: 85 c0                       	testl	%eax, %eax
10000439c: 75 26                       	jne	0x1000043c4 <_mb_win_address_wait_open+0x104>
10000439e: c7 83 c4 00 00 00 01 00 00 00       	movl	$0x1, 0xc4(%rbx)
1000043a8: 48 89 df                    	movq	%rbx, %rdi
1000043ab: 48 83 c7 70                 	addq	$0x70, %rdi
1000043af: e8 94 3c 00 00              	callq	0x100008048 <_sysconf+0x100008048>
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
10000443e: e8 41 3c 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100004443: 85 c0                       	testl	%eax, %eax
100004445: 75 2b                       	jne	0x100004472 <_mb_win_address_wait_close+0x62>
100004447: 49 83 be b0 00 00 00 00     	cmpq	$0x0, 0xb0(%r14)
10000444f: 75 0a                       	jne	0x10000445b <_mb_win_address_wait_close+0x4b>
100004451: 49 83 be b8 00 00 00 00     	cmpq	$0x0, 0xb8(%r14)
100004459: 74 26                       	je	0x100004481 <_mb_win_address_wait_close+0x71>
10000445b: 4c 89 f7                    	movq	%r14, %rdi
10000445e: e8 27 3c 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004463: 89 c1                       	movl	%eax, %ecx
100004465: b8 03 00 00 00              	movl	$0x3, %eax
10000446a: 85 c9                       	testl	%ecx, %ecx
10000446c: 0f 84 fe 00 00 00           	je	0x100004570 <_mb_win_address_wait_close+0x160>
100004472: e8 65 3b 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100004477: b8 01 00 00 00              	movl	$0x1, %eax
10000447c: e9 ef 00 00 00              	jmp	0x100004570 <_mb_win_address_wait_close+0x160>
100004481: 41 c7 86 cc 00 00 00 01 00 00 00    	movl	$0x1, 0xcc(%r14)
10000448c: 4c 89 f7                    	movq	%r14, %rdi
10000448f: e8 f6 3b 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004494: 85 c0                       	testl	%eax, %eax
100004496: 75 da                       	jne	0x100004472 <_mb_win_address_wait_close+0x62>
100004498: 41 83 be c4 00 00 00 00     	cmpl	$0x0, 0xc4(%r14)
1000044a0: 74 30                       	je	0x1000044d2 <_mb_win_address_wait_close+0xc2>
1000044a2: 49 8d 7e 40                 	leaq	0x40(%r14), %rdi
1000044a6: e8 7f 3b 00 00              	callq	0x10000802a <_sysconf+0x10000802a>
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
1000044e0: e8 63 3b 00 00              	callq	0x100008048 <_sysconf+0x100008048>
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
100004515: e8 5e 3b 00 00              	callq	0x100008078 <_sysconf+0x100008078>
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
100004557: e8 aa 3a 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
1000045be: e8 c1 3a 00 00              	callq	0x100008084 <_sysconf+0x100008084>
1000045c3: 85 c0                       	testl	%eax, %eax
1000045c5: 75 68                       	jne	0x10000462f <_mb_win_address_wait_acquire+0xaf>
1000045c7: 49 83 3f 00                 	cmpq	$0x0, (%r15)
1000045cb: 75 0a                       	jne	0x1000045d7 <_mb_win_address_wait_acquire+0x57>
1000045cd: 4c 89 f8                    	movq	%r15, %rax
1000045d0: 49 83 7f 08 00              	cmpq	$0x0, 0x8(%r15)
1000045d5: 74 1c                       	je	0x1000045f3 <_mb_win_address_wait_acquire+0x73>
1000045d7: 48 89 df                    	movq	%rbx, %rdi
1000045da: e8 ab 3a 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004622: e8 63 3a 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004627: 89 c1                       	movl	%eax, %ecx
100004629: 31 c0                       	xorl	%eax, %eax
10000462b: 85 c9                       	testl	%ecx, %ecx
10000462d: 74 b9                       	je	0x1000045e8 <_mb_win_address_wait_acquire+0x68>
10000462f: e8 a8 39 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100004673: e8 0c 3a 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
1000046cf: e8 b6 39 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000046d4: 89 c1                       	movl	%eax, %ecx
1000046d6: 31 c0                       	xorl	%eax, %eax
1000046d8: 85 c9                       	testl	%ecx, %ecx
1000046da: 74 1a                       	je	0x1000046f6 <_mb_win_address_wait_release_pin+0xb6>
1000046dc: eb 1d                       	jmp	0x1000046fb <_mb_win_address_wait_release_pin+0xbb>
1000046de: b8 01 00 00 00              	movl	$0x1, %eax
1000046e3: eb 11                       	jmp	0x1000046f6 <_mb_win_address_wait_release_pin+0xb6>
1000046e5: 48 89 df                    	movq	%rbx, %rdi
1000046e8: e8 9d 39 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000046ed: 85 c0                       	testl	%eax, %eax
1000046ef: b8 02 00 00 00              	movl	$0x2, %eax
1000046f4: 75 05                       	jne	0x1000046fb <_mb_win_address_wait_release_pin+0xbb>
1000046f6: 5b                          	popq	%rbx
1000046f7: 41 5e                       	popq	%r14
1000046f9: 5d                          	popq	%rbp
1000046fa: c3                          	retq
1000046fb: e8 dc 38 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>

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
1000047cf: e8 b0 38 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100004846: e8 a3 37 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
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
100004896: e8 ef 37 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004947: e8 a2 36 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
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
1000049bf: e8 78 36 00 00              	callq	0x10000803c <_sysconf+0x10000803c>
1000049c4: 83 f8 3c                    	cmpl	$0x3c, %eax
1000049c7: 75 1c                       	jne	0x1000049e5 <_mb_win_address_wait_wait+0x2e5>
1000049c9: e9 52 ff ff ff              	jmp	0x100004920 <_mb_win_address_wait_wait+0x220>
1000049ce: 66 90                       	nop
1000049d0: 48 8b 7d d0                 	movq	-0x30(%rbp), %rdi
1000049d4: 4c 89 f6                    	movq	%r14, %rsi
1000049d7: e8 66 36 00 00              	callq	0x100008042 <_sysconf+0x100008042>
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
100004a73: e8 12 36 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004aa7: e8 de 35 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004ae1: e8 a4 35 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004b17: e8 6e 35 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004b48: e8 8f 34 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100004bab: e8 d4 34 00 00              	callq	0x100008084 <_sysconf+0x100008084>
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
100004bff: e8 20 34 00 00              	callq	0x100008024 <_sysconf+0x100008024>
100004c04: 85 c0                       	testl	%eax, %eax
100004c06: 75 1e                       	jne	0x100004c26 <_wake+0xc6>
100004c08: 48 89 df                    	movq	%rbx, %rdi
100004c0b: e8 7a 34 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
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
100004c26: e8 b1 33 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
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
100004c9f: e8 44 33 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
100004ca4: 48 85 c0                    	testq	%rax, %rax
100004ca7: 74 2b                       	je	0x100004cd4 <_mb_win_srw_open+0x94>
100004ca9: 48 89 c3                    	movq	%rax, %rbx
100004cac: 48 89 c7                    	movq	%rax, %rdi
100004caf: 31 f6                       	xorl	%esi, %esi
100004cb1: e8 c8 33 00 00              	callq	0x10000807e <_sysconf+0x10000807e>
100004cb6: 85 c0                       	testl	%eax, %eax
100004cb8: 74 2d                       	je	0x100004ce7 <_mb_win_srw_open+0xa7>
100004cba: 41 89 c6                    	movl	%eax, %r14d
100004cbd: 48 89 df                    	movq	%rbx, %rdi
100004cc0: e8 41 33 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
100004cfe: e8 75 33 00 00              	callq	0x100008078 <_sysconf+0x100008078>
100004d03: 85 c0                       	testl	%eax, %eax
100004d05: 75 3d                       	jne	0x100004d44 <_mb_win_srw_open+0x104>
100004d07: 48 89 df                    	movq	%rbx, %rdi
100004d0a: e8 f7 32 00 00              	callq	0x100008006 <_sysconf+0x100008006>
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
100004d44: e8 93 32 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100004d49: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100004d50 <_mb_win_srw_close>:
100004d50: 55                          	pushq	%rbp
100004d51: 48 89 e5                    	movq	%rsp, %rbp
100004d54: 41 57                       	pushq	%r15
100004d56: 41 56                       	pushq	%r14
100004d58: 41 54                       	pushq	%r12
100004d5a: 53                          	pushq	%rbx
100004d5b: 48 85 ff                    	testq	%rdi, %rdi
100004d5e: 74 3a                       	je	0x100004d9a <_mb_win_srw_close+0x4a>
100004d60: 48 8b 1f                    	movq	(%rdi), %rbx
100004d63: 48 85 db                    	testq	%rbx, %rbx
100004d66: 74 39                       	je	0x100004da1 <_mb_win_srw_close+0x51>
100004d68: 49 89 ff                    	movq	%rdi, %r15
100004d6b: 49 89 f6                    	movq	%rsi, %r14
100004d6e: 48 89 df                    	movq	%rbx, %rdi
100004d71: e8 0e 33 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100004d76: 85 c0                       	testl	%eax, %eax
100004d78: 0f 85 92 00 00 00           	jne	0x100004e10 <_mb_win_srw_close+0xc0>
100004d7e: 48 83 7b 58 00              	cmpq	$0x0, 0x58(%rbx)
100004d83: 74 23                       	je	0x100004da8 <_mb_win_srw_close+0x58>
100004d85: 48 89 df                    	movq	%rbx, %rdi
100004d88: e8 fd 32 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004d8d: 89 c1                       	movl	%eax, %ecx
100004d8f: b8 03 00 00 00              	movl	$0x3, %eax
100004d94: 85 c9                       	testl	%ecx, %ecx
100004d96: 74 45                       	je	0x100004ddd <_mb_win_srw_close+0x8d>
100004d98: eb 76                       	jmp	0x100004e10 <_mb_win_srw_close+0xc0>
100004d9a: b8 01 00 00 00              	movl	$0x1, %eax
100004d9f: eb 3c                       	jmp	0x100004ddd <_mb_win_srw_close+0x8d>
100004da1: b8 02 00 00 00              	movl	$0x2, %eax
100004da6: eb 35                       	jmp	0x100004ddd <_mb_win_srw_close+0x8d>
100004da8: 4c 8b 63 60                 	movq	0x60(%rbx), %r12
100004dac: 48 89 df                    	movq	%rbx, %rdi
100004daf: e8 d6 32 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004db4: 85 c0                       	testl	%eax, %eax
100004db6: 75 58                       	jne	0x100004e10 <_mb_win_srw_close+0xc0>
100004db8: b8 03 00 00 00              	movl	$0x3, %eax
100004dbd: 4d 85 e4                    	testq	%r12, %r12
100004dc0: 75 1b                       	jne	0x100004ddd <_mb_win_srw_close+0x8d>
100004dc2: 48 89 df                    	movq	%rbx, %rdi
100004dc5: e8 ae 32 00 00              	callq	0x100008078 <_sysconf+0x100008078>
100004dca: 85 c0                       	testl	%eax, %eax
100004dcc: 74 18                       	je	0x100004de6 <_mb_win_srw_close+0x96>
100004dce: 89 c1                       	movl	%eax, %ecx
100004dd0: b8 04 00 00 00              	movl	$0x4, %eax
100004dd5: 4d 85 f6                    	testq	%r14, %r14
100004dd8: 74 03                       	je	0x100004ddd <_mb_win_srw_close+0x8d>
100004dda: 41 89 0e                    	movl	%ecx, (%r14)
100004ddd: 5b                          	popq	%rbx
100004dde: 41 5c                       	popq	%r12
100004de0: 41 5e                       	popq	%r14
100004de2: 41 5f                       	popq	%r15
100004de4: 5d                          	popq	%rbp
100004de5: c3                          	retq
100004de6: 48 8d 7b 48                 	leaq	0x48(%rbx), %rdi
100004dea: e8 81 d2 ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100004def: 85 c0                       	testl	%eax, %eax
100004df1: 75 1d                       	jne	0x100004e10 <_mb_win_srw_close+0xc0>
100004df3: 48 89 df                    	movq	%rbx, %rdi
100004df6: e8 0b 32 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100004dfb: 49 c7 07 00 00 00 00        	movq	$0x0, (%r15)
100004e02: 31 c9                       	xorl	%ecx, %ecx
100004e04: b8 00 00 00 00              	movl	$0x0, %eax
100004e09: 4d 85 f6                    	testq	%r14, %r14
100004e0c: 75 cc                       	jne	0x100004dda <_mb_win_srw_close+0x8a>
100004e0e: eb cd                       	jmp	0x100004ddd <_mb_win_srw_close+0x8d>
100004e10: e8 c7 31 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100004e15: 66 66 2e 0f 1f 84 00 00 00 00 00    	nopw	%cs:(%rax,%rax)

0000000100004e20 <_mb_win_srw_acquire_pin>:
100004e20: 55                          	pushq	%rbp
100004e21: 48 89 e5                    	movq	%rsp, %rbp
100004e24: 41 57                       	pushq	%r15
100004e26: 41 56                       	pushq	%r14
100004e28: 53                          	pushq	%rbx
100004e29: 50                          	pushq	%rax
100004e2a: 48 85 ff                    	testq	%rdi, %rdi
100004e2d: 0f 94 c1                    	sete	%cl
100004e30: 48 85 f6                    	testq	%rsi, %rsi
100004e33: 0f 94 c2                    	sete	%dl
100004e36: b8 01 00 00 00              	movl	$0x1, %eax
100004e3b: 08 ca                       	orb	%cl, %dl
100004e3d: 75 6e                       	jne	0x100004ead <_mb_win_srw_acquire_pin+0x8d>
100004e3f: 48 8b 1f                    	movq	(%rdi), %rbx
100004e42: b8 02 00 00 00              	movl	$0x2, %eax
100004e47: 48 85 db                    	testq	%rbx, %rbx
100004e4a: 74 61                       	je	0x100004ead <_mb_win_srw_acquire_pin+0x8d>
100004e4c: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100004e50: 75 5b                       	jne	0x100004ead <_mb_win_srw_acquire_pin+0x8d>
100004e52: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100004e57: 75 54                       	jne	0x100004ead <_mb_win_srw_acquire_pin+0x8d>
100004e59: 49 89 ff                    	movq	%rdi, %r15
100004e5c: 49 89 f6                    	movq	%rsi, %r14
100004e5f: 48 89 df                    	movq	%rbx, %rdi
100004e62: e8 1d 32 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100004e67: 85 c0                       	testl	%eax, %eax
100004e69: 75 5e                       	jne	0x100004ec9 <_mb_win_srw_acquire_pin+0xa9>
100004e6b: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100004e6f: 48 85 c9                    	testq	%rcx, %rcx
100004e72: 4c 89 f0                    	movq	%r14, %rax
100004e75: 4c 89 fa                    	movq	%r15, %rdx
100004e78: 74 14                       	je	0x100004e8e <_mb_win_srw_acquire_pin+0x6e>
100004e7a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)
100004e80: 48 39 c1                    	cmpq	%rax, %rcx
100004e83: 74 33                       	je	0x100004eb8 <_mb_win_srw_acquire_pin+0x98>
100004e85: 48 8b 49 08                 	movq	0x8(%rcx), %rcx
100004e89: 48 85 c9                    	testq	%rcx, %rcx
100004e8c: 75 f2                       	jne	0x100004e80 <_mb_win_srw_acquire_pin+0x60>
100004e8e: 48 89 10                    	movq	%rdx, (%rax)
100004e91: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100004e95: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
100004e99: 48 89 43 58                 	movq	%rax, 0x58(%rbx)
100004e9d: 48 89 df                    	movq	%rbx, %rdi
100004ea0: e8 e5 31 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004ea5: 89 c1                       	movl	%eax, %ecx
100004ea7: 31 c0                       	xorl	%eax, %eax
100004ea9: 85 c9                       	testl	%ecx, %ecx
100004eab: 75 1c                       	jne	0x100004ec9 <_mb_win_srw_acquire_pin+0xa9>
100004ead: 48 83 c4 08                 	addq	$0x8, %rsp
100004eb1: 5b                          	popq	%rbx
100004eb2: 41 5e                       	popq	%r14
100004eb4: 41 5f                       	popq	%r15
100004eb6: 5d                          	popq	%rbp
100004eb7: c3                          	retq
100004eb8: 48 89 df                    	movq	%rbx, %rdi
100004ebb: e8 ca 31 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004ec0: 85 c0                       	testl	%eax, %eax
100004ec2: b8 02 00 00 00              	movl	$0x2, %eax
100004ec7: 74 e4                       	je	0x100004ead <_mb_win_srw_acquire_pin+0x8d>
100004ec9: e8 0e 31 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100004ece: 66 90                       	nop

0000000100004ed0 <_mb_win_srw_release_pin>:
100004ed0: 55                          	pushq	%rbp
100004ed1: 48 89 e5                    	movq	%rsp, %rbp
100004ed4: 41 56                       	pushq	%r14
100004ed6: 53                          	pushq	%rbx
100004ed7: 48 85 ff                    	testq	%rdi, %rdi
100004eda: 74 6c                       	je	0x100004f48 <_mb_win_srw_release_pin+0x78>
100004edc: 48 8b 0f                    	movq	(%rdi), %rcx
100004edf: b8 02 00 00 00              	movl	$0x2, %eax
100004ee4: 48 85 c9                    	testq	%rcx, %rcx
100004ee7: 74 64                       	je	0x100004f4d <_mb_win_srw_release_pin+0x7d>
100004ee9: 48 8b 19                    	movq	(%rcx), %rbx
100004eec: 48 85 db                    	testq	%rbx, %rbx
100004eef: 74 5c                       	je	0x100004f4d <_mb_win_srw_release_pin+0x7d>
100004ef1: 49 89 fe                    	movq	%rdi, %r14
100004ef4: 48 89 df                    	movq	%rbx, %rdi
100004ef7: e8 88 31 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100004efc: 85 c0                       	testl	%eax, %eax
100004efe: 75 63                       	jne	0x100004f63 <_mb_win_srw_release_pin+0x93>
100004f00: 48 8d 73 58                 	leaq	0x58(%rbx), %rsi
100004f04: 4c 89 f0                    	movq	%r14, %rax
100004f07: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100004f10: 48 89 f1                    	movq	%rsi, %rcx
100004f13: 48 8b 16                    	movq	(%rsi), %rdx
100004f16: 48 85 d2                    	testq	%rdx, %rdx
100004f19: 74 09                       	je	0x100004f24 <_mb_win_srw_release_pin+0x54>
100004f1b: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
100004f1f: 48 39 c2                    	cmpq	%rax, %rdx
100004f22: 75 ec                       	jne	0x100004f10 <_mb_win_srw_release_pin+0x40>
100004f24: 48 85 d2                    	testq	%rdx, %rdx
100004f27: 74 29                       	je	0x100004f52 <_mb_win_srw_release_pin+0x82>
100004f29: 48 8b 50 08                 	movq	0x8(%rax), %rdx
100004f2d: 48 89 11                    	movq	%rdx, (%rcx)
100004f30: 0f 57 c0                    	xorps	%xmm0, %xmm0
100004f33: 0f 11 00                    	movups	%xmm0, (%rax)
100004f36: 48 89 df                    	movq	%rbx, %rdi
100004f39: e8 4c 31 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004f3e: 89 c1                       	movl	%eax, %ecx
100004f40: 31 c0                       	xorl	%eax, %eax
100004f42: 85 c9                       	testl	%ecx, %ecx
100004f44: 74 07                       	je	0x100004f4d <_mb_win_srw_release_pin+0x7d>
100004f46: eb 1b                       	jmp	0x100004f63 <_mb_win_srw_release_pin+0x93>
100004f48: b8 01 00 00 00              	movl	$0x1, %eax
100004f4d: 5b                          	popq	%rbx
100004f4e: 41 5e                       	popq	%r14
100004f50: 5d                          	popq	%rbp
100004f51: c3                          	retq
100004f52: 48 89 df                    	movq	%rbx, %rdi
100004f55: e8 30 31 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100004f5a: 85 c0                       	testl	%eax, %eax
100004f5c: b8 02 00 00 00              	movl	$0x2, %eax
100004f61: 74 ea                       	je	0x100004f4d <_mb_win_srw_release_pin+0x7d>
100004f63: e8 74 30 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100004f68: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100004f70 <_mb_win_srw_domain>:
100004f70: 48 85 ff                    	testq	%rdi, %rdi
100004f73: 74 12                       	je	0x100004f87 <_mb_win_srw_domain+0x17>
100004f75: 48 8b 07                    	movq	(%rdi), %rax
100004f78: 48 85 c0                    	testq	%rax, %rax
100004f7b: 74 0a                       	je	0x100004f87 <_mb_win_srw_domain+0x17>
100004f7d: 55                          	pushq	%rbp
100004f7e: 48 89 e5                    	movq	%rsp, %rbp
100004f81: 48 8b 40 40                 	movq	0x40(%rax), %rax
100004f85: 5d                          	popq	%rbp
100004f86: c3                          	retq
100004f87: 31 c0                       	xorl	%eax, %eax
100004f89: c3                          	retq
100004f8a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100004f90 <_mb_win_srw_initialize>:
100004f90: 55                          	pushq	%rbp
100004f91: 48 89 e5                    	movq	%rsp, %rbp
100004f94: 5d                          	popq	%rbp
100004f95: e9 06 00 00 00              	jmp	0x100004fa0 <_initialize>
100004f9a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100004fa0 <_initialize>:
100004fa0: 55                          	pushq	%rbp
100004fa1: 48 89 e5                    	movq	%rsp, %rbp
100004fa4: 41 56                       	pushq	%r14
100004fa6: 53                          	pushq	%rbx
100004fa7: 48 85 ff                    	testq	%rdi, %rdi
100004faa: 74 42                       	je	0x100004fee <_initialize+0x4e>
100004fac: 4c 8b 37                    	movq	(%rdi), %r14
100004faf: 4d 85 f6                    	testq	%r14, %r14
100004fb2: 74 3a                       	je	0x100004fee <_initialize+0x4e>
100004fb4: 48 89 f3                    	movq	%rsi, %rbx
100004fb7: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100004fbb: e8 60 d1 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100004fc0: 4c 89 f7                    	movq	%r14, %rdi
100004fc3: e8 bc 30 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100004fc8: 85 c0                       	testl	%eax, %eax
100004fca: 75 22                       	jne	0x100004fee <_initialize+0x4e>
100004fcc: 48 85 db                    	testq	%rbx, %rbx
100004fcf: 74 1d                       	je	0x100004fee <_initialize+0x4e>
100004fd1: 89 d8                       	movl	%ebx, %eax
100004fd3: 83 e0 07                    	andl	$0x7, %eax
100004fd6: 75 16                       	jne	0x100004fee <_initialize+0x4e>
100004fd8: 49 8d 46 60                 	leaq	0x60(%r14), %rax
100004fdc: 0f 1f 40 00                 	nopl	(%rax)
100004fe0: 48 8b 00                    	movq	(%rax), %rax
100004fe3: 48 85 c0                    	testq	%rax, %rax
100004fe6: 74 0b                       	je	0x100004ff3 <_initialize+0x53>
100004fe8: 48 39 58 08                 	cmpq	%rbx, 0x8(%rax)
100004fec: 75 f2                       	jne	0x100004fe0 <_initialize+0x40>
100004fee: e8 e9 2f 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100004ff3: 48 c7 03 00 00 00 00        	movq	$0x0, (%rbx)
100004ffa: 4c 89 f7                    	movq	%r14, %rdi
100004ffd: e8 88 30 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005002: 85 c0                       	testl	%eax, %eax
100005004: 75 e8                       	jne	0x100004fee <_initialize+0x4e>
100005006: 5b                          	popq	%rbx
100005007: 41 5e                       	popq	%r14
100005009: 5d                          	popq	%rbp
10000500a: c3                          	retq
10000500b: 0f 1f 44 00 00              	nopl	(%rax,%rax)

0000000100005010 <_mb_win_srw_condition_initialize>:
100005010: 55                          	pushq	%rbp
100005011: 48 89 e5                    	movq	%rsp, %rbp
100005014: 5d                          	popq	%rbp
100005015: e9 86 ff ff ff              	jmp	0x100004fa0 <_initialize>
10000501a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100005020 <_mb_win_srw_acquire>:
100005020: 55                          	pushq	%rbp
100005021: 48 89 e5                    	movq	%rsp, %rbp
100005024: 41 57                       	pushq	%r15
100005026: 41 56                       	pushq	%r14
100005028: 53                          	pushq	%rbx
100005029: 50                          	pushq	%rax
10000502a: 48 85 ff                    	testq	%rdi, %rdi
10000502d: 74 68                       	je	0x100005097 <_mb_win_srw_acquire+0x77>
10000502f: 4c 8b 37                    	movq	(%rdi), %r14
100005032: 4d 85 f6                    	testq	%r14, %r14
100005035: 74 60                       	je	0x100005097 <_mb_win_srw_acquire+0x77>
100005037: 89 d3                       	movl	%edx, %ebx
100005039: 49 89 f7                    	movq	%rsi, %r15
10000503c: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100005040: e8 db d0 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100005045: 4c 89 f7                    	movq	%r14, %rdi
100005048: e8 37 30 00 00              	callq	0x100008084 <_sysconf+0x100008084>
10000504d: 85 c0                       	testl	%eax, %eax
10000504f: 75 46                       	jne	0x100005097 <_mb_win_srw_acquire+0x77>
100005051: 83 fb 02                    	cmpl	$0x2, %ebx
100005054: 73 41                       	jae	0x100005097 <_mb_win_srw_acquire+0x77>
100005056: 4c 89 f7                    	movq	%r14, %rdi
100005059: 4c 89 fe                    	movq	%r15, %rsi
10000505c: ba 01 00 00 00              	movl	$0x1, %edx
100005061: b9 01 00 00 00              	movl	$0x1, %ecx
100005066: 45 31 c0                    	xorl	%r8d, %r8d
100005069: e8 32 00 00 00              	callq	0x1000050a0 <_lookup>
10000506e: 48 85 c0                    	testq	%rax, %rax
100005071: 74 24                       	je	0x100005097 <_mb_win_srw_acquire+0x77>
100005073: 4c 89 f7                    	movq	%r14, %rdi
100005076: 48 89 c6                    	movq	%rax, %rsi
100005079: 89 da                       	movl	%ebx, %edx
10000507b: e8 70 01 00 00              	callq	0x1000051f0 <_take>
100005080: 4c 89 f7                    	movq	%r14, %rdi
100005083: e8 02 30 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005088: 85 c0                       	testl	%eax, %eax
10000508a: 75 0b                       	jne	0x100005097 <_mb_win_srw_acquire+0x77>
10000508c: 48 83 c4 08                 	addq	$0x8, %rsp
100005090: 5b                          	popq	%rbx
100005091: 41 5e                       	popq	%r14
100005093: 41 5f                       	popq	%r15
100005095: 5d                          	popq	%rbp
100005096: c3                          	retq
100005097: e8 40 2f 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
10000509c: 0f 1f 40 00                 	nopl	(%rax)

00000001000050a0 <_lookup>:
1000050a0: 55                          	pushq	%rbp
1000050a1: 48 89 e5                    	movq	%rsp, %rbp
1000050a4: 41 57                       	pushq	%r15
1000050a6: 41 56                       	pushq	%r14
1000050a8: 41 55                       	pushq	%r13
1000050aa: 41 54                       	pushq	%r12
1000050ac: 53                          	pushq	%rbx
1000050ad: 48 83 ec 28                 	subq	$0x28, %rsp
1000050b1: 48 8b 05 58 3f 00 00        	movq	0x3f58(%rip), %rax      ## 0x100009010 <_sysconf+0x100009010>
1000050b8: 48 8b 00                    	movq	(%rax), %rax
1000050bb: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
1000050bf: 48 85 f6                    	testq	%rsi, %rsi
1000050c2: 0f 84 19 01 00 00           	je	0x1000051e1 <_lookup+0x141>
1000050c8: 89 f0                       	movl	%esi, %eax
1000050ca: 83 e0 07                    	andl	$0x7, %eax
1000050cd: 0f 85 0e 01 00 00           	jne	0x1000051e1 <_lookup+0x141>
1000050d3: 48 8d 47 60                 	leaq	0x60(%rdi), %rax
1000050d7: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
1000050e0: 48 8b 00                    	movq	(%rax), %rax
1000050e3: 48 85 c0                    	testq	%rax, %rax
1000050e6: 74 1d                       	je	0x100005105 <_lookup+0x65>
1000050e8: 48 39 70 08                 	cmpq	%rsi, 0x8(%rax)
1000050ec: 75 f2                       	jne	0x1000050e0 <_lookup+0x40>
1000050ee: 39 50 18                    	cmpl	%edx, 0x18(%rax)
1000050f1: 0f 85 ea 00 00 00           	jne	0x1000051e1 <_lookup+0x141>
1000050f7: 48 8b 0e                    	movq	(%rsi), %rcx
1000050fa: 48 3b 48 10                 	cmpq	0x10(%rax), %rcx
1000050fe: 74 5a                       	je	0x10000515a <_lookup+0xba>
100005100: e9 dc 00 00 00              	jmp	0x1000051e1 <_lookup+0x141>
100005105: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100005109: 0f 85 d2 00 00 00           	jne	0x1000051e1 <_lookup+0x141>
10000510f: 85 c9                       	testl	%ecx, %ecx
100005111: 74 45                       	je	0x100005158 <_lookup+0xb8>
100005113: 48 8b 47 68                 	movq	0x68(%rdi), %rax
100005117: 48 3b 47 70                 	cmpq	0x70(%rdi), %rax
10000511b: 74 3b                       	je	0x100005158 <_lookup+0xb8>
10000511d: 48 83 7f 78 ff              	cmpq	$-0x1, 0x78(%rdi)
100005122: 74 34                       	je	0x100005158 <_lookup+0xb8>
100005124: 89 55 bc                    	movl	%edx, -0x44(%rbp)
100005127: 4d 89 c4                    	movq	%r8, %r12
10000512a: 49 89 fd                    	movq	%rdi, %r13
10000512d: 49 89 f6                    	movq	%rsi, %r14
100005130: bf 01 00 00 00              	movl	$0x1, %edi
100005135: be 80 00 00 00              	movl	$0x80, %esi
10000513a: e8 a9 2e 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
10000513f: 48 85 c0                    	testq	%rax, %rax
100005142: 74 14                       	je	0x100005158 <_lookup+0xb8>
100005144: 49 89 c7                    	movq	%rax, %r15
100005147: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
10000514b: e8 fe 2e 00 00              	callq	0x10000804e <_sysconf+0x10000804e>
100005150: 85 c0                       	testl	%eax, %eax
100005152: 74 25                       	je	0x100005179 <_lookup+0xd9>
100005154: 89 c3                       	movl	%eax, %ebx
100005156: eb 3d                       	jmp	0x100005195 <_lookup+0xf5>
100005158: 31 c0                       	xorl	%eax, %eax
10000515a: 48 8b 0d af 3e 00 00        	movq	0x3eaf(%rip), %rcx      ## 0x100009010 <_sysconf+0x100009010>
100005161: 48 8b 09                    	movq	(%rcx), %rcx
100005164: 48 3b 4d d0                 	cmpq	-0x30(%rbp), %rcx
100005168: 75 7c                       	jne	0x1000051e6 <_lookup+0x146>
10000516a: 48 83 c4 28                 	addq	$0x28, %rsp
10000516e: 5b                          	popq	%rbx
10000516f: 41 5c                       	popq	%r12
100005171: 41 5d                       	popq	%r13
100005173: 41 5e                       	popq	%r14
100005175: 41 5f                       	popq	%r15
100005177: 5d                          	popq	%rbp
100005178: c3                          	retq
100005179: 49 8d 7f 40                 	leaq	0x40(%r15), %rdi
10000517d: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
100005181: e8 aa 2e 00 00              	callq	0x100008030 <_sysconf+0x100008030>
100005186: 89 c3                       	movl	%eax, %ebx
100005188: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
10000518c: e8 b7 2e 00 00              	callq	0x100008048 <_sysconf+0x100008048>
100005191: 85 c0                       	testl	%eax, %eax
100005193: 75 4c                       	jne	0x1000051e1 <_lookup+0x141>
100005195: 4c 89 f8                    	movq	%r15, %rax
100005198: 8b 4d bc                    	movl	-0x44(%rbp), %ecx
10000519b: 85 db                       	testl	%ebx, %ebx
10000519d: 74 15                       	je	0x1000051b4 <_lookup+0x114>
10000519f: 4d 85 e4                    	testq	%r12, %r12
1000051a2: 74 04                       	je	0x1000051a8 <_lookup+0x108>
1000051a4: 41 89 1c 24                 	movl	%ebx, (%r12)
1000051a8: 48 89 c7                    	movq	%rax, %rdi
1000051ab: e8 56 2e 00 00              	callq	0x100008006 <_sysconf+0x100008006>
1000051b0: 31 c0                       	xorl	%eax, %eax
1000051b2: eb a6                       	jmp	0x10000515a <_lookup+0xba>
1000051b4: 4c 89 70 08                 	movq	%r14, 0x8(%rax)
1000051b8: 89 48 18                    	movl	%ecx, 0x18(%rax)
1000051bb: 49 8b 4d 78                 	movq	0x78(%r13), %rcx
1000051bf: 48 ff c1                    	incq	%rcx
1000051c2: 49 89 4d 78                 	movq	%rcx, 0x78(%r13)
1000051c6: 48 89 48 10                 	movq	%rcx, 0x10(%rax)
1000051ca: 49 8b 55 60                 	movq	0x60(%r13), %rdx
1000051ce: 48 89 10                    	movq	%rdx, (%rax)
1000051d1: 49 89 45 60                 	movq	%rax, 0x60(%r13)
1000051d5: 49 ff 45 68                 	incq	0x68(%r13)
1000051d9: 49 89 0e                    	movq	%rcx, (%r14)
1000051dc: e9 79 ff ff ff              	jmp	0x10000515a <_lookup+0xba>
1000051e1: e8 f6 2d 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
1000051e6: e8 e5 2d 00 00              	callq	0x100007fd0 <_sysconf+0x100007fd0>
1000051eb: 0f 1f 44 00 00              	nopl	(%rax,%rax)

00000001000051f0 <_take>:
1000051f0: 55                          	pushq	%rbp
1000051f1: 48 89 e5                    	movq	%rsp, %rbp
1000051f4: 41 57                       	pushq	%r15
1000051f6: 41 56                       	pushq	%r14
1000051f8: 41 54                       	pushq	%r12
1000051fa: 53                          	pushq	%rbx
1000051fb: 48 8b 46 28                 	movq	0x28(%rsi), %rax
1000051ff: 48 83 f8 ff                 	cmpq	$-0x1, %rax
100005203: 0f 84 8f 00 00 00           	je	0x100005298 <_take+0xa8>
100005209: 48 89 f3                    	movq	%rsi, %rbx
10000520c: 48 83 7e 20 ff              	cmpq	$-0x1, 0x20(%rsi)
100005211: 0f 84 81 00 00 00           	je	0x100005298 <_take+0xa8>
100005217: 48 8b 4b 30                 	movq	0x30(%rbx), %rcx
10000521b: 48 83 f9 ff                 	cmpq	$-0x1, %rcx
10000521f: 74 77                       	je	0x100005298 <_take+0xa8>
100005221: 41 89 d6                    	movl	%edx, %r14d
100005224: 49 89 ff                    	movq	%rdi, %r15
100005227: 48 ff c0                    	incq	%rax
10000522a: 48 89 43 28                 	movq	%rax, 0x28(%rbx)
10000522e: 85 d2                       	testl	%edx, %edx
100005230: 75 07                       	jne	0x100005239 <_take+0x49>
100005232: 48 ff c1                    	incq	%rcx
100005235: 48 89 4b 30                 	movq	%rcx, 0x30(%rbx)
100005239: 4c 8d 63 40                 	leaq	0x40(%rbx), %r12
10000523d: eb 20                       	jmp	0x10000525f <_take+0x6f>
10000523f: 48 83 7b 20 00              	cmpq	$0x0, 0x20(%rbx)
100005244: 74 31                       	je	0x100005277 <_take+0x87>
100005246: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100005250: 4c 89 e7                    	movq	%r12, %rdi
100005253: 4c 89 fe                    	movq	%r15, %rsi
100005256: e8 e7 2d 00 00              	callq	0x100008042 <_sysconf+0x100008042>
10000525b: 85 c0                       	testl	%eax, %eax
10000525d: 75 39                       	jne	0x100005298 <_take+0xa8>
10000525f: 83 7b 1c 00                 	cmpl	$0x0, 0x1c(%rbx)
100005263: 75 eb                       	jne	0x100005250 <_take+0x60>
100005265: 45 85 f6                    	testl	%r14d, %r14d
100005268: 74 d5                       	je	0x10000523f <_take+0x4f>
10000526a: 48 83 7b 30 00              	cmpq	$0x0, 0x30(%rbx)
10000526f: 75 df                       	jne	0x100005250 <_take+0x60>
100005271: 48 ff 43 20                 	incq	0x20(%rbx)
100005275: eb 14                       	jmp	0x10000528b <_take+0x9b>
100005277: 48 ff 4b 30                 	decq	0x30(%rbx)
10000527b: c7 43 1c 01 00 00 00        	movl	$0x1, 0x1c(%rbx)
100005282: e8 09 2e 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100005287: 48 89 43 38                 	movq	%rax, 0x38(%rbx)
10000528b: 48 ff 4b 28                 	decq	0x28(%rbx)
10000528f: 5b                          	popq	%rbx
100005290: 41 5c                       	popq	%r12
100005292: 41 5e                       	popq	%r14
100005294: 41 5f                       	popq	%r15
100005296: 5d                          	popq	%rbp
100005297: c3                          	retq
100005298: e8 3f 2d 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
10000529d: 0f 1f 00                    	nopl	(%rax)

00000001000052a0 <_mb_win_srw_try_acquire>:
1000052a0: 55                          	pushq	%rbp
1000052a1: 48 89 e5                    	movq	%rsp, %rbp
1000052a4: 41 57                       	pushq	%r15
1000052a6: 41 56                       	pushq	%r14
1000052a8: 53                          	pushq	%rbx
1000052a9: 50                          	pushq	%rax
1000052aa: 48 85 ff                    	testq	%rdi, %rdi
1000052ad: 0f 84 d7 00 00 00           	je	0x10000538a <_mb_win_srw_try_acquire+0xea>
1000052b3: 48 8b 1f                    	movq	(%rdi), %rbx
1000052b6: 48 85 db                    	testq	%rbx, %rbx
1000052b9: 0f 84 cb 00 00 00           	je	0x10000538a <_mb_win_srw_try_acquire+0xea>
1000052bf: 41 89 d6                    	movl	%edx, %r14d
1000052c2: 49 89 f7                    	movq	%rsi, %r15
1000052c5: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
1000052c9: e8 52 ce ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000052ce: 48 89 df                    	movq	%rbx, %rdi
1000052d1: e8 ae 2d 00 00              	callq	0x100008084 <_sysconf+0x100008084>
1000052d6: 85 c0                       	testl	%eax, %eax
1000052d8: 0f 85 ac 00 00 00           	jne	0x10000538a <_mb_win_srw_try_acquire+0xea>
1000052de: 41 83 fe 02                 	cmpl	$0x2, %r14d
1000052e2: 0f 83 a2 00 00 00           	jae	0x10000538a <_mb_win_srw_try_acquire+0xea>
1000052e8: 48 89 df                    	movq	%rbx, %rdi
1000052eb: 4c 89 fe                    	movq	%r15, %rsi
1000052ee: ba 01 00 00 00              	movl	$0x1, %edx
1000052f3: b9 01 00 00 00              	movl	$0x1, %ecx
1000052f8: 45 31 c0                    	xorl	%r8d, %r8d
1000052fb: e8 a0 fd ff ff              	callq	0x1000050a0 <_lookup>
100005300: 48 85 c0                    	testq	%rax, %rax
100005303: 0f 84 81 00 00 00           	je	0x10000538a <_mb_win_srw_try_acquire+0xea>
100005309: 83 78 1c 00                 	cmpl	$0x0, 0x1c(%rax)
10000530d: 74 0a                       	je	0x100005319 <_mb_win_srw_try_acquire+0x79>
10000530f: 45 31 ff                    	xorl	%r15d, %r15d
100005312: 45 84 ff                    	testb	%r15b, %r15b
100005315: 75 1c                       	jne	0x100005333 <_mb_win_srw_try_acquire+0x93>
100005317: eb 4b                       	jmp	0x100005364 <_mb_win_srw_try_acquire+0xc4>
100005319: 31 c9                       	xorl	%ecx, %ecx
10000531b: 45 85 f6                    	testl	%r14d, %r14d
10000531e: 0f 95 c1                    	setne	%cl
100005321: c1 e1 04                    	shll	$0x4, %ecx
100005324: 48 83 7c 08 20 00           	cmpq	$0x0, 0x20(%rax,%rcx)
10000532a: 41 0f 94 c7                 	sete	%r15b
10000532e: 45 84 ff                    	testb	%r15b, %r15b
100005331: 74 31                       	je	0x100005364 <_mb_win_srw_try_acquire+0xc4>
100005333: 48 8b 48 20                 	movq	0x20(%rax), %rcx
100005337: 48 83 f9 ff                 	cmpq	$-0x1, %rcx
10000533b: 74 4d                       	je	0x10000538a <_mb_win_srw_try_acquire+0xea>
10000533d: 45 85 f6                    	testl	%r14d, %r14d
100005340: 74 09                       	je	0x10000534b <_mb_win_srw_try_acquire+0xab>
100005342: 48 ff c1                    	incq	%rcx
100005345: 48 89 48 20                 	movq	%rcx, 0x20(%rax)
100005349: eb 19                       	jmp	0x100005364 <_mb_win_srw_try_acquire+0xc4>
10000534b: c7 40 1c 01 00 00 00        	movl	$0x1, 0x1c(%rax)
100005352: 49 89 c6                    	movq	%rax, %r14
100005355: e8 36 2d 00 00              	callq	0x100008090 <_sysconf+0x100008090>
10000535a: 48 89 c1                    	movq	%rax, %rcx
10000535d: 4c 89 f0                    	movq	%r14, %rax
100005360: 49 89 4e 38                 	movq	%rcx, 0x38(%r14)
100005364: 48 89 df                    	movq	%rbx, %rdi
100005367: 48 89 c6                    	movq	%rax, %rsi
10000536a: e8 21 00 00 00              	callq	0x100005390 <_retire>
10000536f: 48 89 df                    	movq	%rbx, %rdi
100005372: e8 13 2d 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005377: 85 c0                       	testl	%eax, %eax
100005379: 75 0f                       	jne	0x10000538a <_mb_win_srw_try_acquire+0xea>
10000537b: 41 0f b6 c7                 	movzbl	%r15b, %eax
10000537f: 48 83 c4 08                 	addq	$0x8, %rsp
100005383: 5b                          	popq	%rbx
100005384: 41 5e                       	popq	%r14
100005386: 41 5f                       	popq	%r15
100005388: 5d                          	popq	%rbp
100005389: c3                          	retq
10000538a: e8 4d 2c 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
10000538f: 90                          	nop

0000000100005390 <_retire>:
100005390: 55                          	pushq	%rbp
100005391: 48 89 e5                    	movq	%rsp, %rbp
100005394: 41 57                       	pushq	%r15
100005396: 41 56                       	pushq	%r14
100005398: 53                          	pushq	%rbx
100005399: 50                          	pushq	%rax
10000539a: 48 83 7e 28 00              	cmpq	$0x0, 0x28(%rsi)
10000539f: 75 1b                       	jne	0x1000053bc <_retire+0x2c>
1000053a1: 48 83 7e 20 00              	cmpq	$0x0, 0x20(%rsi)
1000053a6: 75 14                       	jne	0x1000053bc <_retire+0x2c>
1000053a8: 83 7e 1c 00                 	cmpl	$0x0, 0x1c(%rsi)
1000053ac: 75 0e                       	jne	0x1000053bc <_retire+0x2c>
1000053ae: 48 83 7e 70 00              	cmpq	$0x0, 0x70(%rsi)
1000053b3: 75 07                       	jne	0x1000053bc <_retire+0x2c>
1000053b5: 48 83 7e 30 00              	cmpq	$0x0, 0x30(%rsi)
1000053ba: 74 0b                       	je	0x1000053c7 <_retire+0x37>
1000053bc: 48 83 c4 08                 	addq	$0x8, %rsp
1000053c0: 5b                          	popq	%rbx
1000053c1: 41 5e                       	popq	%r14
1000053c3: 41 5f                       	popq	%r15
1000053c5: 5d                          	popq	%rbp
1000053c6: c3                          	retq
1000053c7: 48 8d 47 60                 	leaq	0x60(%rdi), %rax
1000053cb: 0f 1f 44 00 00              	nopl	(%rax,%rax)
1000053d0: 48 89 c3                    	movq	%rax, %rbx
1000053d3: 48 8b 00                    	movq	(%rax), %rax
1000053d6: 48 85 c0                    	testq	%rax, %rax
1000053d9: 74 05                       	je	0x1000053e0 <_retire+0x50>
1000053db: 48 39 f0                    	cmpq	%rsi, %rax
1000053de: 75 f0                       	jne	0x1000053d0 <_retire+0x40>
1000053e0: 48 85 c0                    	testq	%rax, %rax
1000053e3: 74 3a                       	je	0x10000541f <_retire+0x8f>
1000053e5: 49 89 fe                    	movq	%rdi, %r14
1000053e8: 49 89 f7                    	movq	%rsi, %r15
1000053eb: 48 8d 7e 40                 	leaq	0x40(%rsi), %rdi
1000053ef: e8 36 2c 00 00              	callq	0x10000802a <_sysconf+0x10000802a>
1000053f4: 85 c0                       	testl	%eax, %eax
1000053f6: 75 27                       	jne	0x10000541f <_retire+0x8f>
1000053f8: 4c 89 ff                    	movq	%r15, %rdi
1000053fb: 49 8b 47 08                 	movq	0x8(%r15), %rax
1000053ff: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
100005406: 49 8b 07                    	movq	(%r15), %rax
100005409: 48 89 03                    	movq	%rax, (%rbx)
10000540c: 49 ff 4e 68                 	decq	0x68(%r14)
100005410: 48 83 c4 08                 	addq	$0x8, %rsp
100005414: 5b                          	popq	%rbx
100005415: 41 5e                       	popq	%r14
100005417: 41 5f                       	popq	%r15
100005419: 5d                          	popq	%rbp
10000541a: e9 e7 2b 00 00              	jmp	0x100008006 <_sysconf+0x100008006>
10000541f: e8 b8 2b 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100005424: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100005430 <_mb_win_srw_release>:
100005430: 55                          	pushq	%rbp
100005431: 48 89 e5                    	movq	%rsp, %rbp
100005434: 41 57                       	pushq	%r15
100005436: 41 56                       	pushq	%r14
100005438: 41 54                       	pushq	%r12
10000543a: 53                          	pushq	%rbx
10000543b: 48 85 ff                    	testq	%rdi, %rdi
10000543e: 0f 84 a9 00 00 00           	je	0x1000054ed <_mb_win_srw_release+0xbd>
100005444: 48 8b 1f                    	movq	(%rdi), %rbx
100005447: 48 85 db                    	testq	%rbx, %rbx
10000544a: 0f 84 9d 00 00 00           	je	0x1000054ed <_mb_win_srw_release+0xbd>
100005450: 41 89 d6                    	movl	%edx, %r14d
100005453: 49 89 f7                    	movq	%rsi, %r15
100005456: 48 8b 7b 40                 	movq	0x40(%rbx), %rdi
10000545a: e8 c1 cc ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000545f: 48 89 df                    	movq	%rbx, %rdi
100005462: e8 1d 2c 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100005467: 85 c0                       	testl	%eax, %eax
100005469: 0f 85 7e 00 00 00           	jne	0x1000054ed <_mb_win_srw_release+0xbd>
10000546f: 41 83 fe 02                 	cmpl	$0x2, %r14d
100005473: 73 78                       	jae	0x1000054ed <_mb_win_srw_release+0xbd>
100005475: 4d 85 ff                    	testq	%r15, %r15
100005478: 74 73                       	je	0x1000054ed <_mb_win_srw_release+0xbd>
10000547a: 44 89 f8                    	movl	%r15d, %eax
10000547d: 83 e0 07                    	andl	$0x7, %eax
100005480: 75 6b                       	jne	0x1000054ed <_mb_win_srw_release+0xbd>
100005482: 4c 8d 63 60                 	leaq	0x60(%rbx), %r12
100005486: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100005490: 4d 8b 24 24                 	movq	(%r12), %r12
100005494: 4d 85 e4                    	testq	%r12, %r12
100005497: 74 1b                       	je	0x1000054b4 <_mb_win_srw_release+0x84>
100005499: 4d 39 7c 24 08              	cmpq	%r15, 0x8(%r12)
10000549e: 75 f0                       	jne	0x100005490 <_mb_win_srw_release+0x60>
1000054a0: 41 83 7c 24 18 01           	cmpl	$0x1, 0x18(%r12)
1000054a6: 75 45                       	jne	0x1000054ed <_mb_win_srw_release+0xbd>
1000054a8: 49 8b 07                    	movq	(%r15), %rax
1000054ab: 49 3b 44 24 10              	cmpq	0x10(%r12), %rax
1000054b0: 74 0b                       	je	0x1000054bd <_mb_win_srw_release+0x8d>
1000054b2: eb 39                       	jmp	0x1000054ed <_mb_win_srw_release+0xbd>
1000054b4: 49 83 3f 00                 	cmpq	$0x0, (%r15)
1000054b8: 75 33                       	jne	0x1000054ed <_mb_win_srw_release+0xbd>
1000054ba: 45 31 e4                    	xorl	%r12d, %r12d
1000054bd: 4d 85 e4                    	testq	%r12, %r12
1000054c0: 74 2b                       	je	0x1000054ed <_mb_win_srw_release+0xbd>
1000054c2: 4c 89 e7                    	movq	%r12, %rdi
1000054c5: 44 89 f6                    	movl	%r14d, %esi
1000054c8: e8 33 00 00 00              	callq	0x100005500 <_drop>
1000054cd: 48 89 df                    	movq	%rbx, %rdi
1000054d0: 4c 89 e6                    	movq	%r12, %rsi
1000054d3: e8 b8 fe ff ff              	callq	0x100005390 <_retire>
1000054d8: 48 89 df                    	movq	%rbx, %rdi
1000054db: e8 aa 2b 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000054e0: 85 c0                       	testl	%eax, %eax
1000054e2: 75 09                       	jne	0x1000054ed <_mb_win_srw_release+0xbd>
1000054e4: 5b                          	popq	%rbx
1000054e5: 41 5c                       	popq	%r12
1000054e7: 41 5e                       	popq	%r14
1000054e9: 41 5f                       	popq	%r15
1000054eb: 5d                          	popq	%rbp
1000054ec: c3                          	retq
1000054ed: e8 ea 2a 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
1000054f2: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)

0000000100005500 <_drop>:
100005500: 55                          	pushq	%rbp
100005501: 48 89 e5                    	movq	%rsp, %rbp
100005504: 41 56                       	pushq	%r14
100005506: 53                          	pushq	%rbx
100005507: 85 f6                       	testl	%esi, %esi
100005509: 74 18                       	je	0x100005523 <_drop+0x23>
10000550b: 48 8b 47 20                 	movq	0x20(%rdi), %rax
10000550f: 48 85 c0                    	testq	%rax, %rax
100005512: 74 4d                       	je	0x100005561 <_drop+0x61>
100005514: 83 7f 1c 00                 	cmpl	$0x0, 0x1c(%rdi)
100005518: 75 47                       	jne	0x100005561 <_drop+0x61>
10000551a: 48 ff c8                    	decq	%rax
10000551d: 48 89 47 20                 	movq	%rax, 0x20(%rdi)
100005521: eb 2c                       	jmp	0x10000554f <_drop+0x4f>
100005523: 83 7f 1c 00                 	cmpl	$0x0, 0x1c(%rdi)
100005527: 74 38                       	je	0x100005561 <_drop+0x61>
100005529: 48 8b 5f 38                 	movq	0x38(%rdi), %rbx
10000552d: 49 89 fe                    	movq	%rdi, %r14
100005530: e8 5b 2b 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100005535: 48 89 df                    	movq	%rbx, %rdi
100005538: 48 89 c6                    	movq	%rax, %rsi
10000553b: e8 1a 2b 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100005540: 85 c0                       	testl	%eax, %eax
100005542: 74 1d                       	je	0x100005561 <_drop+0x61>
100005544: 4c 89 f7                    	movq	%r14, %rdi
100005547: 41 c7 46 1c 00 00 00 00     	movl	$0x0, 0x1c(%r14)
10000554f: 48 83 c7 40                 	addq	$0x40, %rdi
100005553: e8 cc 2a 00 00              	callq	0x100008024 <_sysconf+0x100008024>
100005558: 85 c0                       	testl	%eax, %eax
10000555a: 75 05                       	jne	0x100005561 <_drop+0x61>
10000555c: 5b                          	popq	%rbx
10000555d: 41 5e                       	popq	%r14
10000555f: 5d                          	popq	%rbp
100005560: c3                          	retq
100005561: e8 76 2a 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100005566: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)

0000000100005570 <_mb_win_srw_sleep>:
100005570: 55                          	pushq	%rbp
100005571: 48 89 e5                    	movq	%rsp, %rbp
100005574: 41 57                       	pushq	%r15
100005576: 41 56                       	pushq	%r14
100005578: 41 55                       	pushq	%r13
10000557a: 41 54                       	pushq	%r12
10000557c: 53                          	pushq	%rbx
10000557d: 48 81 ec 98 00 00 00        	subq	$0x98, %rsp
100005584: 44 89 85 78 ff ff ff        	movl	%r8d, -0x88(%rbp)
10000558b: 48 8b 05 7e 3a 00 00        	movq	0x3a7e(%rip), %rax      ## 0x100009010 <_sysconf+0x100009010>
100005592: 48 8b 00                    	movq	(%rax), %rax
100005595: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
100005599: 48 85 ff                    	testq	%rdi, %rdi
10000559c: 0f 84 84 05 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000055a2: 4c 8b 37                    	movq	(%rdi), %r14
1000055a5: 4d 85 f6                    	testq	%r14, %r14
1000055a8: 0f 84 78 05 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000055ae: 41 89 cf                    	movl	%ecx, %r15d
1000055b1: 49 89 d5                    	movq	%rdx, %r13
1000055b4: 48 89 f3                    	movq	%rsi, %rbx
1000055b7: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000055bb: e8 60 cb ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000055c0: 4c 89 f7                    	movq	%r14, %rdi
1000055c3: e8 bc 2a 00 00              	callq	0x100008084 <_sysconf+0x100008084>
1000055c8: 85 c0                       	testl	%eax, %eax
1000055ca: 0f 85 56 05 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000055d0: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000055d4: e8 47 cb ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000055d9: 49 89 c4                    	movq	%rax, %r12
1000055dc: 83 bd 78 ff ff ff 02        	cmpl	$0x2, -0x88(%rbp)
1000055e3: 72 1f                       	jb	0x100005604 <_mb_win_srw_sleep+0x94>
1000055e5: 4c 89 f7                    	movq	%r14, %rdi
1000055e8: e8 9d 2a 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000055ed: 85 c0                       	testl	%eax, %eax
1000055ef: 0f 85 31 05 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000055f5: 41 c7 04 24 57 00 00 00     	movl	$0x57, (%r12)
1000055fd: 31 c0                       	xorl	%eax, %eax
1000055ff: e9 00 05 00 00              	jmp	0x100005b04 <_mb_win_srw_sleep+0x594>
100005604: 4d 85 ed                    	testq	%r13, %r13
100005607: 0f 84 19 05 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
10000560d: 44 89 e8                    	movl	%r13d, %eax
100005610: 83 e0 07                    	andl	$0x7, %eax
100005613: 0f 85 0d 05 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005619: 4c 89 a5 58 ff ff ff        	movq	%r12, -0xa8(%rbp)
100005620: 41 8b 0c 24                 	movl	(%r12), %ecx
100005624: 4d 8d 66 60                 	leaq	0x60(%r14), %r12
100005628: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)
100005630: 4d 8b 24 24                 	movq	(%r12), %r12
100005634: 4d 85 e4                    	testq	%r12, %r12
100005637: 74 23                       	je	0x10000565c <_mb_win_srw_sleep+0xec>
100005639: 4d 39 6c 24 08              	cmpq	%r13, 0x8(%r12)
10000563e: 75 f0                       	jne	0x100005630 <_mb_win_srw_sleep+0xc0>
100005640: 41 83 7c 24 18 01           	cmpl	$0x1, 0x18(%r12)
100005646: 0f 85 da 04 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
10000564c: 49 8b 45 00                 	movq	(%r13), %rax
100005650: 49 3b 44 24 10              	cmpq	0x10(%r12), %rax
100005655: 74 13                       	je	0x10000566a <_mb_win_srw_sleep+0xfa>
100005657: e9 ca 04 00 00              	jmp	0x100005b26 <_mb_win_srw_sleep+0x5b6>
10000565c: 49 83 7d 00 00              	cmpq	$0x0, (%r13)
100005661: 0f 85 bf 04 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005667: 45 31 e4                    	xorl	%r12d, %r12d
10000566a: 4d 85 e4                    	testq	%r12, %r12
10000566d: 0f 84 b3 04 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005673: 89 8d 50 ff ff ff           	movl	%ecx, -0xb0(%rbp)
100005679: 83 bd 78 ff ff ff 00        	cmpl	$0x0, -0x88(%rbp)
100005680: 74 1c                       	je	0x10000569e <_mb_win_srw_sleep+0x12e>
100005682: 49 83 7c 24 20 00           	cmpq	$0x0, 0x20(%r12)
100005688: 0f 84 98 04 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
10000568e: 45 89 fd                    	movl	%r15d, %r13d
100005691: 41 83 7c 24 1c 00           	cmpl	$0x0, 0x1c(%r12)
100005697: 74 31                       	je	0x1000056ca <_mb_win_srw_sleep+0x15a>
100005699: e9 88 04 00 00              	jmp	0x100005b26 <_mb_win_srw_sleep+0x5b6>
10000569e: 41 83 7c 24 1c 00           	cmpl	$0x0, 0x1c(%r12)
1000056a4: 0f 84 7c 04 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000056aa: 4d 8b 6c 24 38              	movq	0x38(%r12), %r13
1000056af: e8 dc 29 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000056b4: 4c 89 ef                    	movq	%r13, %rdi
1000056b7: 45 89 fd                    	movl	%r15d, %r13d
1000056ba: 48 89 c6                    	movq	%rax, %rsi
1000056bd: e8 98 29 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
1000056c2: 85 c0                       	testl	%eax, %eax
1000056c4: 0f 84 5c 04 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000056ca: 0f 57 c0                    	xorps	%xmm0, %xmm0
1000056cd: 0f 29 85 60 ff ff ff        	movaps	%xmm0, -0xa0(%rbp)
1000056d4: c7 85 7c ff ff ff 00 00 00 00       	movl	$0x0, -0x84(%rbp)
1000056de: 41 83 fd ff                 	cmpl	$-0x1, %r13d
1000056e2: 0f 84 bc 00 00 00           	je	0x1000057a4 <_mb_win_srw_sleep+0x234>
1000056e8: 48 8d b5 60 ff ff ff        	leaq	-0xa0(%rbp), %rsi
1000056ef: bf 06 00 00 00              	movl	$0x6, %edi
1000056f4: e8 f5 28 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
1000056f9: 85 c0                       	testl	%eax, %eax
1000056fb: 74 09                       	je	0x100005706 <_mb_win_srw_sleep+0x196>
1000056fd: e8 c8 28 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100005702: 8b 00                       	movl	(%rax), %eax
100005704: eb 02                       	jmp	0x100005708 <_mb_win_srw_sleep+0x198>
100005706: 31 c0                       	xorl	%eax, %eax
100005708: 89 85 7c ff ff ff           	movl	%eax, -0x84(%rbp)
10000570e: 85 c0                       	testl	%eax, %eax
100005710: 75 2a                       	jne	0x10000573c <_mb_win_srw_sleep+0x1cc>
100005712: 48 b8 c7 76 be ff ff ff ff 7f       	movabsq	$0x7fffffffffbe76c7, %rax ## imm = 0x7FFFFFFFFFBE76C7
10000571c: 48 39 85 60 ff ff ff        	cmpq	%rax, -0xa0(%rbp)
100005723: 77 0d                       	ja	0x100005732 <_mb_win_srw_sleep+0x1c2>
100005725: 48 81 bd 68 ff ff ff 00 ca 9a 3b    	cmpq	$0x3b9aca00, -0x98(%rbp) ## imm = 0x3B9ACA00
100005730: 72 0a                       	jb	0x10000573c <_mb_win_srw_sleep+0x1cc>
100005732: c7 85 7c ff ff ff 54 00 00 00       	movl	$0x54, -0x84(%rbp)
10000573c: 83 bd 7c ff ff ff 00        	cmpl	$0x0, -0x84(%rbp)
100005743: 75 5f                       	jne	0x1000057a4 <_mb_win_srw_sleep+0x234>
100005745: 44 89 e8                    	movl	%r13d, %eax
100005748: 48 69 c8 d3 4d 62 10        	imulq	$0x10624dd3, %rax, %rcx ## imm = 0x10624DD3
10000574f: 48 c1 e9 26                 	shrq	$0x26, %rcx
100005753: 48 8b 85 60 ff ff ff        	movq	-0xa0(%rbp), %rax
10000575a: 48 01 c8                    	addq	%rcx, %rax
10000575d: 48 89 85 60 ff ff ff        	movq	%rax, -0xa0(%rbp)
100005764: 69 c9 e8 03 00 00           	imull	$0x3e8, %ecx, %ecx      ## imm = 0x3E8
10000576a: 44 89 ea                    	movl	%r13d, %edx
10000576d: 29 ca                       	subl	%ecx, %edx
10000576f: 69 ca 40 42 0f 00           	imull	$0xf4240, %edx, %ecx    ## imm = 0xF4240
100005775: 48 03 8d 68 ff ff ff        	addq	-0x98(%rbp), %rcx
10000577c: 48 89 8d 68 ff ff ff        	movq	%rcx, -0x98(%rbp)
100005783: 48 81 f9 00 ca 9a 3b        	cmpq	$0x3b9aca00, %rcx       ## imm = 0x3B9ACA00
10000578a: 7c 18                       	jl	0x1000057a4 <_mb_win_srw_sleep+0x234>
10000578c: 48 ff c0                    	incq	%rax
10000578f: 48 89 85 60 ff ff ff        	movq	%rax, -0xa0(%rbp)
100005796: 48 81 c1 00 36 65 c4        	addq	$-0x3b9aca00, %rcx      ## imm = 0xC4653600
10000579d: 48 89 8d 68 ff ff ff        	movq	%rcx, -0x98(%rbp)
1000057a4: 83 bd 7c ff ff ff 00        	cmpl	$0x0, -0x84(%rbp)
1000057ab: 74 24                       	je	0x1000057d1 <_mb_win_srw_sleep+0x261>
1000057ad: 4c 89 f7                    	movq	%r14, %rdi
1000057b0: e8 d5 28 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000057b5: 85 c0                       	testl	%eax, %eax
1000057b7: 0f 85 69 03 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000057bd: 48 8b 85 58 ff ff ff        	movq	-0xa8(%rbp), %rax
1000057c4: c7 00 1f 00 00 00           	movl	$0x1f, (%rax)
1000057ca: 31 c0                       	xorl	%eax, %eax
1000057cc: e9 33 03 00 00              	jmp	0x100005b04 <_mb_win_srw_sleep+0x594>
1000057d1: 4c 8d 85 7c ff ff ff        	leaq	-0x84(%rbp), %r8
1000057d8: 4c 89 f7                    	movq	%r14, %rdi
1000057db: 48 89 de                    	movq	%rbx, %rsi
1000057de: ba 02 00 00 00              	movl	$0x2, %edx
1000057e3: b9 01 00 00 00              	movl	$0x1, %ecx
1000057e8: e8 b3 f8 ff ff              	callq	0x1000050a0 <_lookup>
1000057ed: 48 85 c0                    	testq	%rax, %rax
1000057f0: 74 7f                       	je	0x100005871 <_mb_win_srw_sleep+0x301>
1000057f2: 48 89 c3                    	movq	%rax, %rbx
1000057f5: 0f 57 c0                    	xorps	%xmm0, %xmm0
1000057f8: 0f 29 45 b0                 	movaps	%xmm0, -0x50(%rbp)
1000057fc: 0f 29 45 a0                 	movaps	%xmm0, -0x60(%rbp)
100005800: 0f 29 45 90                 	movaps	%xmm0, -0x70(%rbp)
100005804: 0f 29 45 80                 	movaps	%xmm0, -0x80(%rbp)
100005808: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
10000580c: e8 3d 28 00 00              	callq	0x10000804e <_sysconf+0x10000804e>
100005811: 85 c0                       	testl	%eax, %eax
100005813: 75 27                       	jne	0x10000583c <_mb_win_srw_sleep+0x2cc>
100005815: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
100005819: 48 8d 7d 88                 	leaq	-0x78(%rbp), %rdi
10000581d: e8 0e 28 00 00              	callq	0x100008030 <_sysconf+0x100008030>
100005822: 41 89 c5                    	movl	%eax, %r13d
100005825: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
100005829: e8 1a 28 00 00              	callq	0x100008048 <_sysconf+0x100008048>
10000582e: 85 c0                       	testl	%eax, %eax
100005830: 0f 85 f0 02 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005836: 44 89 e8                    	movl	%r13d, %eax
100005839: 45 89 fd                    	movl	%r15d, %r13d
10000583c: 89 85 7c ff ff ff           	movl	%eax, -0x84(%rbp)
100005842: 85 c0                       	testl	%eax, %eax
100005844: 49 89 df                    	movq	%rbx, %r15
100005847: 74 5c                       	je	0x1000058a5 <_mb_win_srw_sleep+0x335>
100005849: 4c 89 f7                    	movq	%r14, %rdi
10000584c: 4c 89 fe                    	movq	%r15, %rsi
10000584f: e8 3c fb ff ff              	callq	0x100005390 <_retire>
100005854: 4c 89 f7                    	movq	%r14, %rdi
100005857: e8 2e 28 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000585c: 85 c0                       	testl	%eax, %eax
10000585e: 0f 85 c2 02 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005864: 31 c0                       	xorl	%eax, %eax
100005866: 41 bd 1f 00 00 00           	movl	$0x1f, %r13d
10000586c: e9 89 02 00 00              	jmp	0x100005afa <_mb_win_srw_sleep+0x58a>
100005871: 4c 89 f7                    	movq	%r14, %rdi
100005874: e8 11 28 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005879: 85 c0                       	testl	%eax, %eax
10000587b: 0f 85 a5 02 00 00           	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005881: 83 bd 7c ff ff ff 00        	cmpl	$0x0, -0x84(%rbp)
100005888: b8 08 00 00 00              	movl	$0x8, %eax
10000588d: b9 1f 00 00 00              	movl	$0x1f, %ecx
100005892: 0f 44 c8                    	cmovel	%eax, %ecx
100005895: 48 8b 85 58 ff ff ff        	movq	-0xa8(%rbp), %rax
10000589c: 89 08                       	movl	%ecx, (%rax)
10000589e: 31 c0                       	xorl	%eax, %eax
1000058a0: e9 5f 02 00 00              	jmp	0x100005b04 <_mb_win_srw_sleep+0x594>
1000058a5: 49 8b 47 78                 	movq	0x78(%r15), %rax
1000058a9: 4c 89 fb                    	movq	%r15, %rbx
1000058ac: 48 83 c3 70                 	addq	$0x70, %rbx
1000058b0: 48 85 c0                    	testq	%rax, %rax
1000058b3: 48 0f 44 c3                 	cmoveq	%rbx, %rax
1000058b7: 48 8d 4d 80                 	leaq	-0x80(%rbp), %rcx
1000058bb: 48 89 08                    	movq	%rcx, (%rax)
1000058be: 49 89 4f 78                 	movq	%rcx, 0x78(%r15)
1000058c2: 49 8b 47 28                 	movq	0x28(%r15), %rax
1000058c6: 48 83 f8 ff                 	cmpq	$-0x1, %rax
1000058ca: 0f 84 56 02 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000058d0: 49 83 7c 24 28 ff           	cmpq	$-0x1, 0x28(%r12)
1000058d6: 0f 84 4a 02 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
1000058dc: 48 ff c0                    	incq	%rax
1000058df: 49 89 47 28                 	movq	%rax, 0x28(%r15)
1000058e3: 49 ff 44 24 28              	incq	0x28(%r12)
1000058e8: 4c 89 e7                    	movq	%r12, %rdi
1000058eb: 8b b5 78 ff ff ff           	movl	-0x88(%rbp), %esi
1000058f1: e8 0a fc ff ff              	callq	0x100005500 <_drop>
1000058f6: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100005900: 83 7d b8 00                 	cmpl	$0x0, -0x48(%rbp)
100005904: 0f 85 2a 01 00 00           	jne	0x100005a34 <_mb_win_srw_sleep+0x4c4>
10000590a: 41 83 fd ff                 	cmpl	$-0x1, %r13d
10000590e: 74 23                       	je	0x100005933 <_mb_win_srw_sleep+0x3c3>
100005910: bf 06 00 00 00              	movl	$0x6, %edi
100005915: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
100005919: e8 d0 26 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
10000591e: 85 c0                       	testl	%eax, %eax
100005920: 74 1f                       	je	0x100005941 <_mb_win_srw_sleep+0x3d1>
100005922: e8 a3 26 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
100005927: 8b 00                       	movl	(%rax), %eax
100005929: 85 c0                       	testl	%eax, %eax
10000592b: 0f 85 7f 00 00 00           	jne	0x1000059b0 <_mb_win_srw_sleep+0x440>
100005931: eb 14                       	jmp	0x100005947 <_mb_win_srw_sleep+0x3d7>
100005933: 48 8d 7d 88                 	leaq	-0x78(%rbp), %rdi
100005937: 4c 89 f6                    	movq	%r14, %rsi
10000593a: e8 03 27 00 00              	callq	0x100008042 <_sysconf+0x100008042>
10000593f: eb 6f                       	jmp	0x1000059b0 <_mb_win_srw_sleep+0x440>
100005941: 31 c0                       	xorl	%eax, %eax
100005943: 85 c0                       	testl	%eax, %eax
100005945: 75 69                       	jne	0x1000059b0 <_mb_win_srw_sleep+0x440>
100005947: 48 8b 8d 60 ff ff ff        	movq	-0xa0(%rbp), %rcx
10000594e: 48 2b 4d c0                 	subq	-0x40(%rbp), %rcx
100005952: 7e 47                       	jle	0x10000599b <_mb_win_srw_sleep+0x42b>
100005954: 48 89 8d 40 ff ff ff        	movq	%rcx, -0xc0(%rbp)
10000595b: 48 8b 85 68 ff ff ff        	movq	-0x98(%rbp), %rax
100005962: 48 2b 45 c8                 	subq	-0x38(%rbp), %rax
100005966: 48 89 85 48 ff ff ff        	movq	%rax, -0xb8(%rbp)
10000596d: 79 17                       	jns	0x100005986 <_mb_win_srw_sleep+0x416>
10000596f: 48 ff c9                    	decq	%rcx
100005972: 48 89 8d 40 ff ff ff        	movq	%rcx, -0xc0(%rbp)
100005979: 48 05 00 ca 9a 3b           	addq	$0x3b9aca00, %rax       ## imm = 0x3B9ACA00
10000597f: 48 89 85 48 ff ff ff        	movq	%rax, -0xb8(%rbp)
100005986: 48 8d 7d 88                 	leaq	-0x78(%rbp), %rdi
10000598a: 4c 89 f6                    	movq	%r14, %rsi
10000598d: 48 8d 95 40 ff ff ff        	leaq	-0xc0(%rbp), %rdx
100005994: e8 a3 26 00 00              	callq	0x10000803c <_sysconf+0x10000803c>
100005999: eb 15                       	jmp	0x1000059b0 <_mb_win_srw_sleep+0x440>
10000599b: b8 3c 00 00 00              	movl	$0x3c, %eax
1000059a0: 75 0e                       	jne	0x1000059b0 <_mb_win_srw_sleep+0x440>
1000059a2: 48 8b 55 c8                 	movq	-0x38(%rbp), %rdx
1000059a6: 48 3b 95 68 ff ff ff        	cmpq	-0x98(%rbp), %rdx
1000059ad: 7c a5                       	jl	0x100005954 <_mb_win_srw_sleep+0x3e4>
1000059af: 90                          	nop
1000059b0: 89 85 7c ff ff ff           	movl	%eax, -0x84(%rbp)
1000059b6: 85 c0                       	testl	%eax, %eax
1000059b8: 0f 84 42 ff ff ff           	je	0x100005900 <_mb_win_srw_sleep+0x390>
1000059be: 83 f8 3c                    	cmpl	$0x3c, %eax
1000059c1: 75 71                       	jne	0x100005a34 <_mb_win_srw_sleep+0x4c4>
1000059c3: bf 06 00 00 00              	movl	$0x6, %edi
1000059c8: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
1000059cc: e8 1d 26 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
1000059d1: 85 c0                       	testl	%eax, %eax
1000059d3: 74 19                       	je	0x1000059ee <_mb_win_srw_sleep+0x47e>
1000059d5: e8 f0 25 00 00              	callq	0x100007fca <_sysconf+0x100007fca>
1000059da: 8b 00                       	movl	(%rax), %eax
1000059dc: 85 c0                       	testl	%eax, %eax
1000059de: 74 14                       	je	0x1000059f4 <_mb_win_srw_sleep+0x484>
1000059e0: 89 85 7c ff ff ff           	movl	%eax, -0x84(%rbp)
1000059e6: 31 c0                       	xorl	%eax, %eax
1000059e8: 84 c0                       	testb	%al, %al
1000059ea: 75 28                       	jne	0x100005a14 <_mb_win_srw_sleep+0x4a4>
1000059ec: eb 46                       	jmp	0x100005a34 <_mb_win_srw_sleep+0x4c4>
1000059ee: 31 c0                       	xorl	%eax, %eax
1000059f0: 85 c0                       	testl	%eax, %eax
1000059f2: 75 ec                       	jne	0x1000059e0 <_mb_win_srw_sleep+0x470>
1000059f4: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
1000059f8: 48 8b 8d 60 ff ff ff        	movq	-0xa0(%rbp), %rcx
1000059ff: 48 39 c8                    	cmpq	%rcx, %rax
100005a02: 7d 1f                       	jge	0x100005a23 <_mb_win_srw_sleep+0x4b3>
100005a04: c7 85 7c ff ff ff 00 00 00 00       	movl	$0x0, -0x84(%rbp)
100005a0e: b0 01                       	movb	$0x1, %al
100005a10: 84 c0                       	testb	%al, %al
100005a12: 74 20                       	je	0x100005a34 <_mb_win_srw_sleep+0x4c4>
100005a14: 83 bd 7c ff ff ff 00        	cmpl	$0x0, -0x84(%rbp)
100005a1b: 0f 84 df fe ff ff           	je	0x100005900 <_mb_win_srw_sleep+0x390>
100005a21: eb 11                       	jmp	0x100005a34 <_mb_win_srw_sleep+0x4c4>
100005a23: 75 c1                       	jne	0x1000059e6 <_mb_win_srw_sleep+0x476>
100005a25: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
100005a29: 48 3b 85 68 ff ff ff        	cmpq	-0x98(%rbp), %rax
100005a30: 7c d2                       	jl	0x100005a04 <_mb_win_srw_sleep+0x494>
100005a32: eb b2                       	jmp	0x1000059e6 <_mb_win_srw_sleep+0x476>
100005a34: 44 8b 4d b8                 	movl	-0x48(%rbp), %r9d
100005a38: 8b 95 7c ff ff ff           	movl	-0x84(%rbp), %edx
100005a3e: 31 f6                       	xorl	%esi, %esi
100005a40: 4c 8d 45 80                 	leaq	-0x80(%rbp), %r8
100005a44: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
100005a50: 48 89 f0                    	movq	%rsi, %rax
100005a53: 48 89 d9                    	movq	%rbx, %rcx
100005a56: 48 8b 1b                    	movq	(%rbx), %rbx
100005a59: 4c 39 c3                    	cmpq	%r8, %rbx
100005a5c: 40 0f 94 c7                 	sete	%dil
100005a60: 48 85 db                    	testq	%rbx, %rbx
100005a63: 74 08                       	je	0x100005a6d <_mb_win_srw_sleep+0x4fd>
100005a65: 48 89 de                    	movq	%rbx, %rsi
100005a68: 40 84 ff                    	testb	%dil, %dil
100005a6b: 74 e3                       	je	0x100005a50 <_mb_win_srw_sleep+0x4e0>
100005a6d: 45 85 c9                    	testl	%r9d, %r9d
100005a70: 40 0f 95 c6                 	setne	%sil
100005a74: 83 fa 3c                    	cmpl	$0x3c, %edx
100005a77: ba b4 05 00 00              	movl	$0x5b4, %edx            ## imm = 0x5B4
100005a7c: 41 bd 1f 00 00 00           	movl	$0x1f, %r13d
100005a82: 44 0f 44 ea                 	cmovel	%edx, %r13d
100005a86: 48 85 db                    	testq	%rbx, %rbx
100005a89: 0f 84 97 00 00 00           	je	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005a8f: 89 f3                       	movl	%esi, %ebx
100005a91: 44 89 8d 54 ff ff ff        	movl	%r9d, -0xac(%rbp)
100005a98: 48 8b 55 80                 	movq	-0x80(%rbp), %rdx
100005a9c: 48 89 11                    	movq	%rdx, (%rcx)
100005a9f: 4d 39 47 78                 	cmpq	%r8, 0x78(%r15)
100005aa3: 48 8d 7d 88                 	leaq	-0x78(%rbp), %rdi
100005aa7: 75 04                       	jne	0x100005aad <_mb_win_srw_sleep+0x53d>
100005aa9: 49 89 47 78                 	movq	%rax, 0x78(%r15)
100005aad: 49 ff 4f 28                 	decq	0x28(%r15)
100005ab1: e8 74 25 00 00              	callq	0x10000802a <_sysconf+0x10000802a>
100005ab6: 85 c0                       	testl	%eax, %eax
100005ab8: 75 6c                       	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005aba: 4c 89 f7                    	movq	%r14, %rdi
100005abd: 4c 89 fe                    	movq	%r15, %rsi
100005ac0: e8 cb f8 ff ff              	callq	0x100005390 <_retire>
100005ac5: 4c 89 f7                    	movq	%r14, %rdi
100005ac8: 4c 89 e6                    	movq	%r12, %rsi
100005acb: 8b 95 78 ff ff ff           	movl	-0x88(%rbp), %edx
100005ad1: e8 1a f7 ff ff              	callq	0x1000051f0 <_take>
100005ad6: 49 ff 4c 24 28              	decq	0x28(%r12)
100005adb: 4c 89 f7                    	movq	%r14, %rdi
100005ade: e8 a7 25 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005ae3: 85 c0                       	testl	%eax, %eax
100005ae5: 75 3f                       	jne	0x100005b26 <_mb_win_srw_sleep+0x5b6>
100005ae7: 31 c0                       	xorl	%eax, %eax
100005ae9: 88 d8                       	movb	%bl, %al
100005aeb: 83 bd 54 ff ff ff 00        	cmpl	$0x0, -0xac(%rbp)
100005af2: 44 0f 45 ad 50 ff ff ff     	cmovnel	-0xb0(%rbp), %r13d
100005afa: 48 8b 8d 58 ff ff ff        	movq	-0xa8(%rbp), %rcx
100005b01: 44 89 29                    	movl	%r13d, (%rcx)
100005b04: 48 8b 0d 05 35 00 00        	movq	0x3505(%rip), %rcx      ## 0x100009010 <_sysconf+0x100009010>
100005b0b: 48 8b 09                    	movq	(%rcx), %rcx
100005b0e: 48 3b 4d d0                 	cmpq	-0x30(%rbp), %rcx
100005b12: 75 17                       	jne	0x100005b2b <_mb_win_srw_sleep+0x5bb>
100005b14: 48 81 c4 98 00 00 00        	addq	$0x98, %rsp
100005b1b: 5b                          	popq	%rbx
100005b1c: 41 5c                       	popq	%r12
100005b1e: 41 5d                       	popq	%r13
100005b20: 41 5e                       	popq	%r14
100005b22: 41 5f                       	popq	%r15
100005b24: 5d                          	popq	%rbp
100005b25: c3                          	retq
100005b26: e8 b1 24 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100005b2b: e8 a0 24 00 00              	callq	0x100007fd0 <_sysconf+0x100007fd0>

0000000100005b30 <_mb_win_srw_wake>:
100005b30: 55                          	pushq	%rbp
100005b31: 48 89 e5                    	movq	%rsp, %rbp
100005b34: 41 57                       	pushq	%r15
100005b36: 41 56                       	pushq	%r14
100005b38: 53                          	pushq	%rbx
100005b39: 50                          	pushq	%rax
100005b3a: 48 85 ff                    	testq	%rdi, %rdi
100005b3d: 0f 84 cc 00 00 00           	je	0x100005c0f <_mb_win_srw_wake+0xdf>
100005b43: 4c 8b 37                    	movq	(%rdi), %r14
100005b46: 4d 85 f6                    	testq	%r14, %r14
100005b49: 0f 84 c0 00 00 00           	je	0x100005c0f <_mb_win_srw_wake+0xdf>
100005b4f: 89 d3                       	movl	%edx, %ebx
100005b51: 49 89 f7                    	movq	%rsi, %r15
100005b54: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100005b58: e8 c3 c5 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100005b5d: 4c 89 f7                    	movq	%r14, %rdi
100005b60: e8 1f 25 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100005b65: 85 c0                       	testl	%eax, %eax
100005b67: 0f 85 a2 00 00 00           	jne	0x100005c0f <_mb_win_srw_wake+0xdf>
100005b6d: 83 fb 02                    	cmpl	$0x2, %ebx
100005b70: 0f 83 99 00 00 00           	jae	0x100005c0f <_mb_win_srw_wake+0xdf>
100005b76: 4d 85 ff                    	testq	%r15, %r15
100005b79: 0f 84 90 00 00 00           	je	0x100005c0f <_mb_win_srw_wake+0xdf>
100005b7f: 44 89 f8                    	movl	%r15d, %eax
100005b82: 83 e0 07                    	andl	$0x7, %eax
100005b85: 0f 85 84 00 00 00           	jne	0x100005c0f <_mb_win_srw_wake+0xdf>
100005b8b: 49 8d 46 60                 	leaq	0x60(%r14), %rax
100005b8f: 90                          	nop
100005b90: 48 8b 00                    	movq	(%rax), %rax
100005b93: 48 85 c0                    	testq	%rax, %rax
100005b96: 74 1c                       	je	0x100005bb4 <_mb_win_srw_wake+0x84>
100005b98: 4c 39 78 08                 	cmpq	%r15, 0x8(%rax)
100005b9c: 75 f2                       	jne	0x100005b90 <_mb_win_srw_wake+0x60>
100005b9e: 83 78 18 02                 	cmpl	$0x2, 0x18(%rax)
100005ba2: 75 6b                       	jne	0x100005c0f <_mb_win_srw_wake+0xdf>
100005ba4: 49 8b 0f                    	movq	(%r15), %rcx
100005ba7: 48 3b 48 10                 	cmpq	0x10(%rax), %rcx
100005bab: 75 62                       	jne	0x100005c0f <_mb_win_srw_wake+0xdf>
100005bad: 48 85 c0                    	testq	%rax, %rax
100005bb0: 75 0f                       	jne	0x100005bc1 <_mb_win_srw_wake+0x91>
100005bb2: eb 44                       	jmp	0x100005bf8 <_mb_win_srw_wake+0xc8>
100005bb4: 49 83 3f 00                 	cmpq	$0x0, (%r15)
100005bb8: 75 55                       	jne	0x100005c0f <_mb_win_srw_wake+0xdf>
100005bba: 31 c0                       	xorl	%eax, %eax
100005bbc: 48 85 c0                    	testq	%rax, %rax
100005bbf: 74 37                       	je	0x100005bf8 <_mb_win_srw_wake+0xc8>
100005bc1: 4c 8b 78 70                 	movq	0x70(%rax), %r15
100005bc5: eb 0c                       	jmp	0x100005bd3 <_mb_win_srw_wake+0xa3>
100005bc7: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100005bd0: 4d 8b 3f                    	movq	(%r15), %r15
100005bd3: 4d 85 ff                    	testq	%r15, %r15
100005bd6: 74 20                       	je	0x100005bf8 <_mb_win_srw_wake+0xc8>
100005bd8: 41 83 7f 38 00              	cmpl	$0x0, 0x38(%r15)
100005bdd: 75 f1                       	jne	0x100005bd0 <_mb_win_srw_wake+0xa0>
100005bdf: 41 c7 47 38 01 00 00 00     	movl	$0x1, 0x38(%r15)
100005be7: 49 8d 7f 08                 	leaq	0x8(%r15), %rdi
100005beb: e8 46 24 00 00              	callq	0x100008036 <_sysconf+0x100008036>
100005bf0: 85 c0                       	testl	%eax, %eax
100005bf2: 75 1b                       	jne	0x100005c0f <_mb_win_srw_wake+0xdf>
100005bf4: 85 db                       	testl	%ebx, %ebx
100005bf6: 75 d8                       	jne	0x100005bd0 <_mb_win_srw_wake+0xa0>
100005bf8: 4c 89 f7                    	movq	%r14, %rdi
100005bfb: e8 8a 24 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005c00: 85 c0                       	testl	%eax, %eax
100005c02: 75 0b                       	jne	0x100005c0f <_mb_win_srw_wake+0xdf>
100005c04: 48 83 c4 08                 	addq	$0x8, %rsp
100005c08: 5b                          	popq	%rbx
100005c09: 41 5e                       	popq	%r14
100005c0b: 41 5f                       	popq	%r15
100005c0d: 5d                          	popq	%rbp
100005c0e: c3                          	retq
100005c0f: e8 c8 23 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
		...

0000000100005c20 <_mb_win_init_once_open>:
100005c20: 55                          	pushq	%rbp
100005c21: 48 89 e5                    	movq	%rsp, %rbp
100005c24: 41 57                       	pushq	%r15
100005c26: 41 56                       	pushq	%r14
100005c28: 41 55                       	pushq	%r13
100005c2a: 41 54                       	pushq	%r12
100005c2c: 53                          	pushq	%rbx
100005c2d: 50                          	pushq	%rax
100005c2e: 48 85 ff                    	testq	%rdi, %rdi
100005c31: 0f 94 c1                    	sete	%cl
100005c34: 48 85 f6                    	testq	%rsi, %rsi
100005c37: 41 0f 94 c0                 	sete	%r8b
100005c3b: b8 01 00 00 00              	movl	$0x1, %eax
100005c40: 41 08 c8                    	orb	%cl, %r8b
100005c43: 75 18                       	jne	0x100005c5d <_mb_win_init_once_open+0x3d>
100005c45: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100005c49: 74 12                       	je	0x100005c5d <_mb_win_init_once_open+0x3d>
100005c4b: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100005c50: 74 0b                       	je	0x100005c5d <_mb_win_init_once_open+0x3d>
100005c52: b8 02 00 00 00              	movl	$0x2, %eax
100005c57: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
100005c5b: 74 0f                       	je	0x100005c6c <_mb_win_init_once_open+0x4c>
100005c5d: 48 83 c4 08                 	addq	$0x8, %rsp
100005c61: 5b                          	popq	%rbx
100005c62: 41 5c                       	popq	%r12
100005c64: 41 5d                       	popq	%r13
100005c66: 41 5e                       	popq	%r14
100005c68: 41 5f                       	popq	%r15
100005c6a: 5d                          	popq	%rbp
100005c6b: c3                          	retq
100005c6c: 49 89 f5                    	movq	%rsi, %r13
100005c6f: 49 89 fc                    	movq	%rdi, %r12
100005c72: 49 89 d7                    	movq	%rdx, %r15
100005c75: bf 01 00 00 00              	movl	$0x1, %edi
100005c7a: be 80 00 00 00              	movl	$0x80, %esi
100005c7f: e8 64 23 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
100005c84: 48 85 c0                    	testq	%rax, %rax
100005c87: 74 2b                       	je	0x100005cb4 <_mb_win_init_once_open+0x94>
100005c89: 48 89 c3                    	movq	%rax, %rbx
100005c8c: 48 89 c7                    	movq	%rax, %rdi
100005c8f: 31 f6                       	xorl	%esi, %esi
100005c91: e8 e8 23 00 00              	callq	0x10000807e <_sysconf+0x10000807e>
100005c96: 85 c0                       	testl	%eax, %eax
100005c98: 74 2d                       	je	0x100005cc7 <_mb_win_init_once_open+0xa7>
100005c9a: 41 89 c6                    	movl	%eax, %r14d
100005c9d: 48 89 df                    	movq	%rbx, %rdi
100005ca0: e8 61 23 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100005ca5: b8 04 00 00 00              	movl	$0x4, %eax
100005caa: 4d 85 ff                    	testq	%r15, %r15
100005cad: 74 ae                       	je	0x100005c5d <_mb_win_init_once_open+0x3d>
100005caf: 45 89 37                    	movl	%r14d, (%r15)
100005cb2: eb a9                       	jmp	0x100005c5d <_mb_win_init_once_open+0x3d>
100005cb4: b8 04 00 00 00              	movl	$0x4, %eax
100005cb9: 4d 85 ff                    	testq	%r15, %r15
100005cbc: 74 9f                       	je	0x100005c5d <_mb_win_init_once_open+0x3d>
100005cbe: 41 c7 07 0c 00 00 00        	movl	$0xc, (%r15)
100005cc5: eb 96                       	jmp	0x100005c5d <_mb_win_init_once_open+0x3d>
100005cc7: 49 8b 7d 00                 	movq	(%r13), %rdi
100005ccb: 48 89 de                    	movq	%rbx, %rsi
100005cce: 48 83 c6 48                 	addq	$0x48, %rsi
100005cd2: e8 c9 c2 ff ff              	callq	0x100001fa0 <_mb_win_threads_acquire>
100005cd7: 85 c0                       	testl	%eax, %eax
100005cd9: 74 1e                       	je	0x100005cf9 <_mb_win_init_once_open+0xd9>
100005cdb: 48 89 df                    	movq	%rbx, %rdi
100005cde: e8 95 23 00 00              	callq	0x100008078 <_sysconf+0x100008078>
100005ce3: 85 c0                       	testl	%eax, %eax
100005ce5: 75 3d                       	jne	0x100005d24 <_mb_win_init_once_open+0x104>
100005ce7: 48 89 df                    	movq	%rbx, %rdi
100005cea: e8 17 23 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100005cef: b8 02 00 00 00              	movl	$0x2, %eax
100005cf4: e9 64 ff ff ff              	jmp	0x100005c5d <_mb_win_init_once_open+0x3d>
100005cf9: 49 8b 45 00                 	movq	(%r13), %rax
100005cfd: 48 89 43 40                 	movq	%rax, 0x40(%rbx)
100005d01: 49 8b 45 08                 	movq	0x8(%r13), %rax
100005d05: 48 89 43 70                 	movq	%rax, 0x70(%rbx)
100005d09: 49 89 1c 24                 	movq	%rbx, (%r12)
100005d0d: 31 c0                       	xorl	%eax, %eax
100005d0f: 4d 85 ff                    	testq	%r15, %r15
100005d12: 0f 84 45 ff ff ff           	je	0x100005c5d <_mb_win_init_once_open+0x3d>
100005d18: 41 c7 07 00 00 00 00        	movl	$0x0, (%r15)
100005d1f: e9 39 ff ff ff              	jmp	0x100005c5d <_mb_win_init_once_open+0x3d>
100005d24: e8 b3 22 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100005d29: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100005d30 <_mb_win_init_once_close>:
100005d30: 55                          	pushq	%rbp
100005d31: 48 89 e5                    	movq	%rsp, %rbp
100005d34: 41 57                       	pushq	%r15
100005d36: 41 56                       	pushq	%r14
100005d38: 41 55                       	pushq	%r13
100005d3a: 41 54                       	pushq	%r12
100005d3c: 53                          	pushq	%rbx
100005d3d: 50                          	pushq	%rax
100005d3e: 48 85 ff                    	testq	%rdi, %rdi
100005d41: 74 39                       	je	0x100005d7c <_mb_win_init_once_close+0x4c>
100005d43: 48 8b 1f                    	movq	(%rdi), %rbx
100005d46: 48 85 db                    	testq	%rbx, %rbx
100005d49: 74 38                       	je	0x100005d83 <_mb_win_init_once_close+0x53>
100005d4b: 49 89 fc                    	movq	%rdi, %r12
100005d4e: 49 89 f7                    	movq	%rsi, %r15
100005d51: 48 89 df                    	movq	%rbx, %rdi
100005d54: e8 2b 23 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100005d59: 85 c0                       	testl	%eax, %eax
100005d5b: 75 1a                       	jne	0x100005d77 <_mb_win_init_once_close+0x47>
100005d5d: 48 83 7b 58 00              	cmpq	$0x0, 0x58(%rbx)
100005d62: 74 33                       	je	0x100005d97 <_mb_win_init_once_close+0x67>
100005d64: 48 89 df                    	movq	%rbx, %rdi
100005d67: e8 1e 23 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005d6c: 89 c1                       	movl	%eax, %ecx
100005d6e: b8 03 00 00 00              	movl	$0x3, %eax
100005d73: 85 c9                       	testl	%ecx, %ecx
100005d75: 74 11                       	je	0x100005d88 <_mb_win_init_once_close+0x58>
100005d77: e8 60 22 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100005d7c: b8 01 00 00 00              	movl	$0x1, %eax
100005d81: eb 05                       	jmp	0x100005d88 <_mb_win_init_once_close+0x58>
100005d83: b8 02 00 00 00              	movl	$0x2, %eax
100005d88: 48 83 c4 08                 	addq	$0x8, %rsp
100005d8c: 5b                          	popq	%rbx
100005d8d: 41 5c                       	popq	%r12
100005d8f: 41 5d                       	popq	%r13
100005d91: 41 5e                       	popq	%r14
100005d93: 41 5f                       	popq	%r15
100005d95: 5d                          	popq	%rbp
100005d96: c3                          	retq
100005d97: 4c 8d 6b 60                 	leaq	0x60(%rbx), %r13
100005d9b: 4c 89 e8                    	movq	%r13, %rax
100005d9e: 66 90                       	nop
100005da0: 48 8b 00                    	movq	(%rax), %rax
100005da3: 48 85 c0                    	testq	%rax, %rax
100005da6: 74 16                       	je	0x100005dbe <_mb_win_init_once_close+0x8e>
100005da8: 83 78 18 03                 	cmpl	$0x3, 0x18(%rax)
100005dac: 75 b6                       	jne	0x100005d64 <_mb_win_init_once_close+0x34>
100005dae: 48 83 78 20 00              	cmpq	$0x0, 0x20(%rax)
100005db3: 75 af                       	jne	0x100005d64 <_mb_win_init_once_close+0x34>
100005db5: 48 83 78 38 00              	cmpq	$0x0, 0x38(%rax)
100005dba: 74 e4                       	je	0x100005da0 <_mb_win_init_once_close+0x70>
100005dbc: eb a6                       	jmp	0x100005d64 <_mb_win_init_once_close+0x34>
100005dbe: 4d 8b 75 00                 	movq	(%r13), %r14
100005dc2: 4d 85 f6                    	testq	%r14, %r14
100005dc5: 74 54                       	je	0x100005e1b <_mb_win_init_once_close+0xeb>
100005dc7: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100005dd0: 49 8b 06                    	movq	(%r14), %rax
100005dd3: 49 89 45 00                 	movq	%rax, (%r13)
100005dd7: 49 83 7e 20 00              	cmpq	$0x0, 0x20(%r14)
100005ddc: 74 0e                       	je	0x100005dec <_mb_win_init_once_close+0xbc>
100005dde: eb 97                       	jmp	0x100005d77 <_mb_win_init_once_close+0x47>
100005de0: 48 8b 07                    	movq	(%rdi), %rax
100005de3: 49 89 46 38                 	movq	%rax, 0x38(%r14)
100005de7: e8 1a 22 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100005dec: 49 8b 7e 38                 	movq	0x38(%r14), %rdi
100005df0: 48 85 ff                    	testq	%rdi, %rdi
100005df3: 75 eb                       	jne	0x100005de0 <_mb_win_init_once_close+0xb0>
100005df5: 49 8d 7e 48                 	leaq	0x48(%r14), %rdi
100005df9: e8 2c 22 00 00              	callq	0x10000802a <_sysconf+0x10000802a>
100005dfe: 85 c0                       	testl	%eax, %eax
100005e00: 0f 85 71 ff ff ff           	jne	0x100005d77 <_mb_win_init_once_close+0x47>
100005e06: 48 ff 4b 68                 	decq	0x68(%rbx)
100005e0a: 4c 89 f7                    	movq	%r14, %rdi
100005e0d: e8 f4 21 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100005e12: 4c 8b 73 60                 	movq	0x60(%rbx), %r14
100005e16: 4d 85 f6                    	testq	%r14, %r14
100005e19: 75 b5                       	jne	0x100005dd0 <_mb_win_init_once_close+0xa0>
100005e1b: 48 89 df                    	movq	%rbx, %rdi
100005e1e: e8 67 22 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005e23: 85 c0                       	testl	%eax, %eax
100005e25: 0f 85 4c ff ff ff           	jne	0x100005d77 <_mb_win_init_once_close+0x47>
100005e2b: 48 89 df                    	movq	%rbx, %rdi
100005e2e: e8 45 22 00 00              	callq	0x100008078 <_sysconf+0x100008078>
100005e33: 85 c0                       	testl	%eax, %eax
100005e35: 74 09                       	je	0x100005e40 <_mb_win_init_once_close+0x110>
100005e37: 89 c1                       	movl	%eax, %ecx
100005e39: b8 04 00 00 00              	movl	$0x4, %eax
100005e3e: eb 28                       	jmp	0x100005e68 <_mb_win_init_once_close+0x138>
100005e40: 48 8d 7b 48                 	leaq	0x48(%rbx), %rdi
100005e44: e8 27 c2 ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100005e49: 85 c0                       	testl	%eax, %eax
100005e4b: 0f 85 26 ff ff ff           	jne	0x100005d77 <_mb_win_init_once_close+0x47>
100005e51: 48 89 df                    	movq	%rbx, %rdi
100005e54: e8 ad 21 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100005e59: 49 c7 04 24 00 00 00 00     	movq	$0x0, (%r12)
100005e61: 31 c9                       	xorl	%ecx, %ecx
100005e63: b8 00 00 00 00              	movl	$0x0, %eax
100005e68: 4d 85 ff                    	testq	%r15, %r15
100005e6b: 0f 84 17 ff ff ff           	je	0x100005d88 <_mb_win_init_once_close+0x58>
100005e71: 41 89 0f                    	movl	%ecx, (%r15)
100005e74: e9 0f ff ff ff              	jmp	0x100005d88 <_mb_win_init_once_close+0x58>
100005e79: 0f 1f 80 00 00 00 00        	nopl	(%rax)

0000000100005e80 <_mb_win_init_once_acquire_pin>:
100005e80: 55                          	pushq	%rbp
100005e81: 48 89 e5                    	movq	%rsp, %rbp
100005e84: 41 57                       	pushq	%r15
100005e86: 41 56                       	pushq	%r14
100005e88: 53                          	pushq	%rbx
100005e89: 50                          	pushq	%rax
100005e8a: 48 85 ff                    	testq	%rdi, %rdi
100005e8d: 0f 94 c1                    	sete	%cl
100005e90: 48 85 f6                    	testq	%rsi, %rsi
100005e93: 0f 94 c2                    	sete	%dl
100005e96: b8 01 00 00 00              	movl	$0x1, %eax
100005e9b: 08 ca                       	orb	%cl, %dl
100005e9d: 75 6e                       	jne	0x100005f0d <_mb_win_init_once_acquire_pin+0x8d>
100005e9f: 48 8b 1f                    	movq	(%rdi), %rbx
100005ea2: b8 02 00 00 00              	movl	$0x2, %eax
100005ea7: 48 85 db                    	testq	%rbx, %rbx
100005eaa: 74 61                       	je	0x100005f0d <_mb_win_init_once_acquire_pin+0x8d>
100005eac: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100005eb0: 75 5b                       	jne	0x100005f0d <_mb_win_init_once_acquire_pin+0x8d>
100005eb2: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100005eb7: 75 54                       	jne	0x100005f0d <_mb_win_init_once_acquire_pin+0x8d>
100005eb9: 49 89 ff                    	movq	%rdi, %r15
100005ebc: 49 89 f6                    	movq	%rsi, %r14
100005ebf: 48 89 df                    	movq	%rbx, %rdi
100005ec2: e8 bd 21 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100005ec7: 85 c0                       	testl	%eax, %eax
100005ec9: 75 5e                       	jne	0x100005f29 <_mb_win_init_once_acquire_pin+0xa9>
100005ecb: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100005ecf: 48 85 c9                    	testq	%rcx, %rcx
100005ed2: 4c 89 f0                    	movq	%r14, %rax
100005ed5: 4c 89 fa                    	movq	%r15, %rdx
100005ed8: 74 14                       	je	0x100005eee <_mb_win_init_once_acquire_pin+0x6e>
100005eda: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)
100005ee0: 48 39 c1                    	cmpq	%rax, %rcx
100005ee3: 74 33                       	je	0x100005f18 <_mb_win_init_once_acquire_pin+0x98>
100005ee5: 48 8b 49 08                 	movq	0x8(%rcx), %rcx
100005ee9: 48 85 c9                    	testq	%rcx, %rcx
100005eec: 75 f2                       	jne	0x100005ee0 <_mb_win_init_once_acquire_pin+0x60>
100005eee: 48 89 10                    	movq	%rdx, (%rax)
100005ef1: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100005ef5: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
100005ef9: 48 89 43 58                 	movq	%rax, 0x58(%rbx)
100005efd: 48 89 df                    	movq	%rbx, %rdi
100005f00: e8 85 21 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005f05: 89 c1                       	movl	%eax, %ecx
100005f07: 31 c0                       	xorl	%eax, %eax
100005f09: 85 c9                       	testl	%ecx, %ecx
100005f0b: 75 1c                       	jne	0x100005f29 <_mb_win_init_once_acquire_pin+0xa9>
100005f0d: 48 83 c4 08                 	addq	$0x8, %rsp
100005f11: 5b                          	popq	%rbx
100005f12: 41 5e                       	popq	%r14
100005f14: 41 5f                       	popq	%r15
100005f16: 5d                          	popq	%rbp
100005f17: c3                          	retq
100005f18: 48 89 df                    	movq	%rbx, %rdi
100005f1b: e8 6a 21 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005f20: 85 c0                       	testl	%eax, %eax
100005f22: b8 02 00 00 00              	movl	$0x2, %eax
100005f27: 74 e4                       	je	0x100005f0d <_mb_win_init_once_acquire_pin+0x8d>
100005f29: e8 ae 20 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100005f2e: 66 90                       	nop

0000000100005f30 <_mb_win_init_once_release_pin>:
100005f30: 55                          	pushq	%rbp
100005f31: 48 89 e5                    	movq	%rsp, %rbp
100005f34: 41 56                       	pushq	%r14
100005f36: 53                          	pushq	%rbx
100005f37: 48 85 ff                    	testq	%rdi, %rdi
100005f3a: 74 6c                       	je	0x100005fa8 <_mb_win_init_once_release_pin+0x78>
100005f3c: 48 8b 0f                    	movq	(%rdi), %rcx
100005f3f: b8 02 00 00 00              	movl	$0x2, %eax
100005f44: 48 85 c9                    	testq	%rcx, %rcx
100005f47: 74 64                       	je	0x100005fad <_mb_win_init_once_release_pin+0x7d>
100005f49: 48 8b 19                    	movq	(%rcx), %rbx
100005f4c: 48 85 db                    	testq	%rbx, %rbx
100005f4f: 74 5c                       	je	0x100005fad <_mb_win_init_once_release_pin+0x7d>
100005f51: 49 89 fe                    	movq	%rdi, %r14
100005f54: 48 89 df                    	movq	%rbx, %rdi
100005f57: e8 28 21 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100005f5c: 85 c0                       	testl	%eax, %eax
100005f5e: 75 63                       	jne	0x100005fc3 <_mb_win_init_once_release_pin+0x93>
100005f60: 48 8d 73 58                 	leaq	0x58(%rbx), %rsi
100005f64: 4c 89 f0                    	movq	%r14, %rax
100005f67: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100005f70: 48 89 f1                    	movq	%rsi, %rcx
100005f73: 48 8b 16                    	movq	(%rsi), %rdx
100005f76: 48 85 d2                    	testq	%rdx, %rdx
100005f79: 74 09                       	je	0x100005f84 <_mb_win_init_once_release_pin+0x54>
100005f7b: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
100005f7f: 48 39 c2                    	cmpq	%rax, %rdx
100005f82: 75 ec                       	jne	0x100005f70 <_mb_win_init_once_release_pin+0x40>
100005f84: 48 85 d2                    	testq	%rdx, %rdx
100005f87: 74 29                       	je	0x100005fb2 <_mb_win_init_once_release_pin+0x82>
100005f89: 48 8b 50 08                 	movq	0x8(%rax), %rdx
100005f8d: 48 89 11                    	movq	%rdx, (%rcx)
100005f90: 0f 57 c0                    	xorps	%xmm0, %xmm0
100005f93: 0f 11 00                    	movups	%xmm0, (%rax)
100005f96: 48 89 df                    	movq	%rbx, %rdi
100005f99: e8 ec 20 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005f9e: 89 c1                       	movl	%eax, %ecx
100005fa0: 31 c0                       	xorl	%eax, %eax
100005fa2: 85 c9                       	testl	%ecx, %ecx
100005fa4: 74 07                       	je	0x100005fad <_mb_win_init_once_release_pin+0x7d>
100005fa6: eb 1b                       	jmp	0x100005fc3 <_mb_win_init_once_release_pin+0x93>
100005fa8: b8 01 00 00 00              	movl	$0x1, %eax
100005fad: 5b                          	popq	%rbx
100005fae: 41 5e                       	popq	%r14
100005fb0: 5d                          	popq	%rbp
100005fb1: c3                          	retq
100005fb2: 48 89 df                    	movq	%rbx, %rdi
100005fb5: e8 d0 20 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100005fba: 85 c0                       	testl	%eax, %eax
100005fbc: b8 02 00 00 00              	movl	$0x2, %eax
100005fc1: 74 ea                       	je	0x100005fad <_mb_win_init_once_release_pin+0x7d>
100005fc3: e8 14 20 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100005fc8: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100005fd0 <_mb_win_init_once_domain>:
100005fd0: 48 85 ff                    	testq	%rdi, %rdi
100005fd3: 74 12                       	je	0x100005fe7 <_mb_win_init_once_domain+0x17>
100005fd5: 48 8b 07                    	movq	(%rdi), %rax
100005fd8: 48 85 c0                    	testq	%rax, %rax
100005fdb: 74 0a                       	je	0x100005fe7 <_mb_win_init_once_domain+0x17>
100005fdd: 55                          	pushq	%rbp
100005fde: 48 89 e5                    	movq	%rsp, %rbp
100005fe1: 48 8b 40 40                 	movq	0x40(%rax), %rax
100005fe5: 5d                          	popq	%rbp
100005fe6: c3                          	retq
100005fe7: 31 c0                       	xorl	%eax, %eax
100005fe9: c3                          	retq
100005fea: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100005ff0 <_mb_win_init_once_begin>:
100005ff0: 55                          	pushq	%rbp
100005ff1: 48 89 e5                    	movq	%rsp, %rbp
100005ff4: 41 57                       	pushq	%r15
100005ff6: 41 56                       	pushq	%r14
100005ff8: 41 55                       	pushq	%r13
100005ffa: 41 54                       	pushq	%r12
100005ffc: 53                          	pushq	%rbx
100005ffd: 48 83 ec 48                 	subq	$0x48, %rsp
100006001: 4c 89 45 b8                 	movq	%r8, -0x48(%rbp)
100006005: 48 85 ff                    	testq	%rdi, %rdi
100006008: 0f 84 cf 03 00 00           	je	0x1000063dd <_mb_win_init_once_begin+0x3ed>
10000600e: 4c 8b 37                    	movq	(%rdi), %r14
100006011: 4d 85 f6                    	testq	%r14, %r14
100006014: 0f 84 c3 03 00 00           	je	0x1000063dd <_mb_win_init_once_begin+0x3ed>
10000601a: 49 89 cd                    	movq	%rcx, %r13
10000601d: 41 89 d7                    	movl	%edx, %r15d
100006020: 49 89 f4                    	movq	%rsi, %r12
100006023: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100006027: e8 f4 c0 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000602c: 4c 89 f7                    	movq	%r14, %rdi
10000602f: e8 50 20 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100006034: 85 c0                       	testl	%eax, %eax
100006036: 0f 85 a1 03 00 00           	jne	0x1000063dd <_mb_win_init_once_begin+0x3ed>
10000603c: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100006040: e8 db c0 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100006045: 48 89 c3                    	movq	%rax, %rbx
100006048: 41 83 ff 04                 	cmpl	$0x4, %r15d
10000604c: 0f 83 a0 00 00 00           	jae	0x1000060f2 <_mb_win_init_once_begin+0x102>
100006052: 4c 89 6d c0                 	movq	%r13, -0x40(%rbp)
100006056: 48 89 5d c8                 	movq	%rbx, -0x38(%rbp)
10000605a: 8b 03                       	movl	(%rbx), %eax
10000605c: 89 45 d4                    	movl	%eax, -0x2c(%rbp)
10000605f: 44 89 fb                    	movl	%r15d, %ebx
100006062: 83 e3 01                    	andl	$0x1, %ebx
100006065: 89 da                       	movl	%ebx, %edx
100006067: 83 f2 01                    	xorl	$0x1, %edx
10000606a: 4c 89 f7                    	movq	%r14, %rdi
10000606d: 4c 89 e6                    	movq	%r12, %rsi
100006070: e8 8b 03 00 00              	callq	0x100006400 <_lookup>
100006075: 49 89 c5                    	movq	%rax, %r13
100006078: 49 89 c4                    	movq	%rax, %r12
10000607b: 85 db                       	testl	%ebx, %ebx
10000607d: 0f 85 8c 00 00 00           	jne	0x10000610f <_mb_win_init_once_begin+0x11f>
100006083: 4d 85 ed                    	testq	%r13, %r13
100006086: 0f 84 b5 00 00 00           	je	0x100006141 <_mb_win_init_once_begin+0x151>
10000608c: 41 8b 44 24 18              	movl	0x18(%r12), %eax
100006091: 48 83 f8 03                 	cmpq	$0x3, %rax
100006095: 0f 87 4d 01 00 00           	ja	0x1000061e8 <_mb_win_init_once_begin+0x1f8>
10000609b: 48 8d 0d 42 03 00 00        	leaq	0x342(%rip), %rcx       ## 0x1000063e4 <_mb_win_init_once_begin+0x3f4>
1000060a2: 48 63 14 81                 	movslq	(%rcx,%rax,4), %rdx
1000060a6: 48 01 ca                    	addq	%rcx, %rdx
1000060a9: ff e2                       	jmpq	*%rdx
1000060ab: 45 85 ff                    	testl	%r15d, %r15d
1000060ae: 0f 84 91 02 00 00           	je	0x100006345 <_mb_win_init_once_begin+0x355>
1000060b4: 41 c7 44 24 18 02 00 00 00  	movl	$0x2, 0x18(%r12)
1000060bd: bf 10 00 00 00              	movl	$0x10, %edi
1000060c2: e8 4b 1f 00 00              	callq	0x100008012 <_sysconf+0x100008012>
1000060c7: 48 85 c0                    	testq	%rax, %rax
1000060ca: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000060ce: 0f 84 b8 02 00 00           	je	0x10000638c <_mb_win_init_once_begin+0x39c>
1000060d4: 49 89 c7                    	movq	%rax, %r15
1000060d7: e8 b4 1f 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000060dc: 49 89 47 08                 	movq	%rax, 0x8(%r15)
1000060e0: 49 8b 44 24 38              	movq	0x38(%r12), %rax
1000060e5: 49 89 07                    	movq	%rax, (%r15)
1000060e8: 4d 89 7c 24 38              	movq	%r15, 0x38(%r12)
1000060ed: e9 6a 02 00 00              	jmp	0x10000635c <_mb_win_init_once_begin+0x36c>
1000060f2: 4c 89 f7                    	movq	%r14, %rdi
1000060f5: e8 90 1f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000060fa: 85 c0                       	testl	%eax, %eax
1000060fc: 0f 85 db 02 00 00           	jne	0x1000063dd <_mb_win_init_once_begin+0x3ed>
100006102: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
100006108: 31 c0                       	xorl	%eax, %eax
10000610a: e9 f3 01 00 00              	jmp	0x100006302 <_mb_win_init_once_begin+0x312>
10000610f: 4d 85 ed                    	testq	%r13, %r13
100006112: 74 0c                       	je	0x100006120 <_mb_win_init_once_begin+0x130>
100006114: 41 83 7c 24 18 03           	cmpl	$0x3, 0x18(%r12)
10000611a: 0f 84 8f 01 00 00           	je	0x1000062af <_mb_win_init_once_begin+0x2bf>
100006120: 4c 89 f7                    	movq	%r14, %rdi
100006123: e8 62 1f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006128: 85 c0                       	testl	%eax, %eax
10000612a: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
10000612e: 0f 85 a9 02 00 00           	jne	0x1000063dd <_mb_win_init_once_begin+0x3ed>
100006134: c7 00 e5 03 00 00           	movl	$0x3e5, (%rax)          ## imm = 0x3E5
10000613a: 31 c0                       	xorl	%eax, %eax
10000613c: e9 c1 01 00 00              	jmp	0x100006302 <_mb_win_init_once_begin+0x312>
100006141: 4c 89 f7                    	movq	%r14, %rdi
100006144: e8 41 1f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006149: 85 c0                       	testl	%eax, %eax
10000614b: 0f 85 8c 02 00 00           	jne	0x1000063dd <_mb_win_init_once_begin+0x3ed>
100006151: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
100006155: c7 00 08 00 00 00           	movl	$0x8, (%rax)
10000615b: 31 c0                       	xorl	%eax, %eax
10000615d: e9 a0 01 00 00              	jmp	0x100006302 <_mb_win_init_once_begin+0x312>
100006162: 45 85 ff                    	testl	%r15d, %r15d
100006165: 74 56                       	je	0x1000061bd <_mb_win_init_once_begin+0x1cd>
100006167: bf 10 00 00 00              	movl	$0x10, %edi
10000616c: e8 a1 1e 00 00              	callq	0x100008012 <_sysconf+0x100008012>
100006171: 48 85 c0                    	testq	%rax, %rax
100006174: 0f 84 fc 01 00 00           	je	0x100006376 <_mb_win_init_once_begin+0x386>
10000617a: 49 89 c7                    	movq	%rax, %r15
10000617d: e8 0e 1f 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100006182: 49 89 47 08                 	movq	%rax, 0x8(%r15)
100006186: 49 8b 44 24 38              	movq	0x38(%r12), %rax
10000618b: 49 89 07                    	movq	%rax, (%r15)
10000618e: 4d 89 7c 24 38              	movq	%r15, 0x38(%r12)
100006193: e9 3d 01 00 00              	jmp	0x1000062d5 <_mb_win_init_once_begin+0x2e5>
100006198: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
10000619c: 48 85 c0                    	testq	%rax, %rax
10000619f: 74 06                       	je	0x1000061a7 <_mb_win_init_once_begin+0x1b7>
1000061a1: c7 00 00 00 00 00           	movl	$0x0, (%rax)
1000061a7: 48 8b 4d b8                 	movq	-0x48(%rbp), %rcx
1000061ab: 48 85 c9                    	testq	%rcx, %rcx
1000061ae: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000061b2: 0f 85 13 01 00 00           	jne	0x1000062cb <_mb_win_init_once_begin+0x2db>
1000061b8: e9 2b 01 00 00              	jmp	0x1000062e8 <_mb_win_init_once_begin+0x2f8>
1000061bd: 83 f8 02                    	cmpl	$0x2, %eax
1000061c0: 0f 84 ea 01 00 00           	je	0x1000063b0 <_mb_win_init_once_begin+0x3c0>
1000061c6: 83 f8 01                    	cmpl	$0x1, %eax
1000061c9: 75 1d                       	jne	0x1000061e8 <_mb_win_init_once_begin+0x1f8>
1000061cb: 49 8b 5c 24 30              	movq	0x30(%r12), %rbx
1000061d0: e8 bb 1e 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000061d5: 48 89 df                    	movq	%rbx, %rdi
1000061d8: 48 89 c6                    	movq	%rax, %rsi
1000061db: e8 7a 1e 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
1000061e0: 85 c0                       	testl	%eax, %eax
1000061e2: 0f 85 f5 01 00 00           	jne	0x1000063dd <_mb_win_init_once_begin+0x3ed>
1000061e8: 48 c7 45 98 00 00 00 00     	movq	$0x0, -0x68(%rbp)
1000061f0: e8 9b 1e 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000061f5: 48 89 45 a0                 	movq	%rax, -0x60(%rbp)
1000061f9: 41 d1 ef                    	shrl	%r15d
1000061fc: 44 89 7d a8                 	movl	%r15d, -0x58(%rbp)
100006200: 49 8b 44 24 28              	movq	0x28(%r12), %rax
100006205: 49 8d 5d 20                 	leaq	0x20(%r13), %rbx
100006209: 48 85 c0                    	testq	%rax, %rax
10000620c: 48 0f 44 c3                 	cmoveq	%rbx, %rax
100006210: 48 8d 4d 98                 	leaq	-0x68(%rbp), %rcx
100006214: 48 89 08                    	movq	%rcx, (%rax)
100006217: 49 89 4c 24 28              	movq	%rcx, 0x28(%r12)
10000621c: 49 83 c5 48                 	addq	$0x48, %r13
100006220: eb 33                       	jmp	0x100006255 <_mb_win_init_once_begin+0x265>
100006222: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100006230: 49 8b 7c 24 30              	movq	0x30(%r12), %rdi
100006235: 48 8b 75 a0                 	movq	-0x60(%rbp), %rsi
100006239: e8 1c 1e 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
10000623e: 85 c0                       	testl	%eax, %eax
100006240: 75 22                       	jne	0x100006264 <_mb_win_init_once_begin+0x274>
100006242: 4c 89 ef                    	movq	%r13, %rdi
100006245: 4c 89 f6                    	movq	%r14, %rsi
100006248: e8 f5 1d 00 00              	callq	0x100008042 <_sysconf+0x100008042>
10000624d: 85 c0                       	testl	%eax, %eax
10000624f: 0f 85 88 01 00 00           	jne	0x1000063dd <_mb_win_init_once_begin+0x3ed>
100006255: 41 8b 44 24 18              	movl	0x18(%r12), %eax
10000625a: 83 f8 01                    	cmpl	$0x1, %eax
10000625d: 74 d1                       	je	0x100006230 <_mb_win_init_once_begin+0x240>
10000625f: 83 f8 03                    	cmpl	$0x3, %eax
100006262: 75 de                       	jne	0x100006242 <_mb_win_init_once_begin+0x252>
100006264: 49 8b 44 24 20              	movq	0x20(%r12), %rax
100006269: 48 85 c0                    	testq	%rax, %rax
10000626c: 74 18                       	je	0x100006286 <_mb_win_init_once_begin+0x296>
10000626e: 4c 8b 7d a0                 	movq	-0x60(%rbp), %r15
100006272: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100006276: 4c 89 fe                    	movq	%r15, %rsi
100006279: e8 dc 1d 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
10000627e: 85 c0                       	testl	%eax, %eax
100006280: 0f 84 8b 00 00 00           	je	0x100006311 <_mb_win_init_once_begin+0x321>
100006286: 45 31 ed                    	xorl	%r13d, %r13d
100006289: 48 8b 03                    	movq	(%rbx), %rax
10000628c: 48 85 c0                    	testq	%rax, %rax
10000628f: 0f 84 48 01 00 00           	je	0x1000063dd <_mb_win_init_once_begin+0x3ed>
100006295: 48 8b 08                    	movq	(%rax), %rcx
100006298: 48 89 0b                    	movq	%rcx, (%rbx)
10000629b: 49 39 44 24 28              	cmpq	%rax, 0x28(%r12)
1000062a0: 75 05                       	jne	0x1000062a7 <_mb_win_init_once_begin+0x2b7>
1000062a2: 4d 89 6c 24 28              	movq	%r13, 0x28(%r12)
1000062a7: 41 83 7c 24 18 03           	cmpl	$0x3, 0x18(%r12)
1000062ad: 75 26                       	jne	0x1000062d5 <_mb_win_init_once_begin+0x2e5>
1000062af: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
1000062b3: 48 85 c0                    	testq	%rax, %rax
1000062b6: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000062ba: 74 06                       	je	0x1000062c2 <_mb_win_init_once_begin+0x2d2>
1000062bc: c7 00 00 00 00 00           	movl	$0x0, (%rax)
1000062c2: 48 8b 4d b8                 	movq	-0x48(%rbp), %rcx
1000062c6: 48 85 c9                    	testq	%rcx, %rcx
1000062c9: 74 1d                       	je	0x1000062e8 <_mb_win_init_once_begin+0x2f8>
1000062cb: 49 8b 44 24 40              	movq	0x40(%r12), %rax
1000062d0: 48 89 01                    	movq	%rax, (%rcx)
1000062d3: eb 13                       	jmp	0x1000062e8 <_mb_win_init_once_begin+0x2f8>
1000062d5: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
1000062d9: 48 85 c0                    	testq	%rax, %rax
1000062dc: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000062e0: 74 06                       	je	0x1000062e8 <_mb_win_init_once_begin+0x2f8>
1000062e2: c7 00 01 00 00 00           	movl	$0x1, (%rax)
1000062e8: 4c 89 f7                    	movq	%r14, %rdi
1000062eb: e8 9a 1d 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000062f0: 85 c0                       	testl	%eax, %eax
1000062f2: 0f 85 e5 00 00 00           	jne	0x1000063dd <_mb_win_init_once_begin+0x3ed>
1000062f8: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
1000062fb: 89 03                       	movl	%eax, (%rbx)
1000062fd: b8 01 00 00 00              	movl	$0x1, %eax
100006302: 48 83 c4 48                 	addq	$0x48, %rsp
100006306: 5b                          	popq	%rbx
100006307: 41 5c                       	popq	%r12
100006309: 41 5d                       	popq	%r13
10000630b: 41 5e                       	popq	%r14
10000630d: 41 5f                       	popq	%r15
10000630f: 5d                          	popq	%rbp
100006310: c3                          	retq
100006311: 49 89 dd                    	movq	%rbx, %r13
100006314: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
100006320: 4d 8b 6d 00                 	movq	(%r13), %r13
100006324: 49 8b 45 00                 	movq	(%r13), %rax
100006328: 48 85 c0                    	testq	%rax, %rax
10000632b: 74 41                       	je	0x10000636e <_mb_win_init_once_begin+0x37e>
10000632d: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100006331: 4c 89 fe                    	movq	%r15, %rsi
100006334: e8 21 1d 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100006339: 4c 89 eb                    	movq	%r13, %rbx
10000633c: 85 c0                       	testl	%eax, %eax
10000633e: 74 e0                       	je	0x100006320 <_mb_win_init_once_begin+0x330>
100006340: e9 44 ff ff ff              	jmp	0x100006289 <_mb_win_init_once_begin+0x299>
100006345: 41 c7 44 24 18 01 00 00 00  	movl	$0x1, 0x18(%r12)
10000634e: e8 3d 1d 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100006353: 49 89 44 24 30              	movq	%rax, 0x30(%r12)
100006358: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
10000635c: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
100006360: 48 85 c0                    	testq	%rax, %rax
100006363: 0f 85 79 ff ff ff           	jne	0x1000062e2 <_mb_win_init_once_begin+0x2f2>
100006369: e9 7a ff ff ff              	jmp	0x1000062e8 <_mb_win_init_once_begin+0x2f8>
10000636e: 4c 89 eb                    	movq	%r13, %rbx
100006371: e9 13 ff ff ff              	jmp	0x100006289 <_mb_win_init_once_begin+0x299>
100006376: 4c 89 f7                    	movq	%r14, %rdi
100006379: e8 0c 1d 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000637e: 85 c0                       	testl	%eax, %eax
100006380: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
100006384: 0f 84 cb fd ff ff           	je	0x100006155 <_mb_win_init_once_begin+0x165>
10000638a: eb 51                       	jmp	0x1000063dd <_mb_win_init_once_begin+0x3ed>
10000638c: 4c 89 f7                    	movq	%r14, %rdi
10000638f: 4c 89 e6                    	movq	%r12, %rsi
100006392: e8 49 01 00 00              	callq	0x1000064e0 <_destroy_active>
100006397: 4c 89 f7                    	movq	%r14, %rdi
10000639a: e8 eb 1c 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000639f: 85 c0                       	testl	%eax, %eax
1000063a1: 75 3a                       	jne	0x1000063dd <_mb_win_init_once_begin+0x3ed>
1000063a3: c7 03 08 00 00 00           	movl	$0x8, (%rbx)
1000063a9: 31 c0                       	xorl	%eax, %eax
1000063ab: e9 52 ff ff ff              	jmp	0x100006302 <_mb_win_init_once_begin+0x312>
1000063b0: 49 8d 5d 38                 	leaq	0x38(%r13), %rbx
1000063b4: 48 8b 1b                    	movq	(%rbx), %rbx
1000063b7: 48 85 db                    	testq	%rbx, %rbx
1000063ba: 0f 84 28 fe ff ff           	je	0x1000061e8 <_mb_win_init_once_begin+0x1f8>
1000063c0: 48 8b 43 08                 	movq	0x8(%rbx), %rax
1000063c4: 48 89 45 b0                 	movq	%rax, -0x50(%rbp)
1000063c8: e8 c3 1c 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000063cd: 48 8b 7d b0                 	movq	-0x50(%rbp), %rdi
1000063d1: 48 89 c6                    	movq	%rax, %rsi
1000063d4: e8 81 1c 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
1000063d9: 85 c0                       	testl	%eax, %eax
1000063db: 74 d7                       	je	0x1000063b4 <_mb_win_init_once_begin+0x3c4>
1000063dd: e8 fa 1b 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
1000063e2: 66 90                       	nop
1000063e4: c7 fc                       	<unknown>
1000063e6: ff ff                       	<unknown>
1000063e8: e7 fd                       	outl	%eax, $0xfd
1000063ea: ff ff                       	<unknown>
1000063ec: 7e fd                       	jle	0x1000063eb <_mb_win_init_once_begin+0x3fb>
1000063ee: ff ff                       	<unknown>
1000063f0: b4 fd                       	movb	$-0x3, %ah
1000063f2: ff ff                       	<unknown>
1000063f4: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100006400 <_lookup>:
100006400: 55                          	pushq	%rbp
100006401: 48 89 e5                    	movq	%rsp, %rbp
100006404: 41 57                       	pushq	%r15
100006406: 41 56                       	pushq	%r14
100006408: 53                          	pushq	%rbx
100006409: 50                          	pushq	%rax
10000640a: 48 85 f6                    	testq	%rsi, %rsi
10000640d: 0f 84 c6 00 00 00           	je	0x1000064d9 <_lookup+0xd9>
100006413: 89 f0                       	movl	%esi, %eax
100006415: 83 e0 07                    	andl	$0x7, %eax
100006418: 0f 85 bb 00 00 00           	jne	0x1000064d9 <_lookup+0xd9>
10000641e: 48 8d 47 60                 	leaq	0x60(%rdi), %rax
100006422: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100006430: 48 8b 00                    	movq	(%rax), %rax
100006433: 48 85 c0                    	testq	%rax, %rax
100006436: 74 14                       	je	0x10000644c <_lookup+0x4c>
100006438: 48 39 70 08                 	cmpq	%rsi, 0x8(%rax)
10000643c: 75 f2                       	jne	0x100006430 <_lookup+0x30>
10000643e: 48 8b 0e                    	movq	(%rsi), %rcx
100006441: 48 3b 48 10                 	cmpq	0x10(%rax), %rcx
100006445: 74 5d                       	je	0x1000064a4 <_lookup+0xa4>
100006447: e9 8d 00 00 00              	jmp	0x1000064d9 <_lookup+0xd9>
10000644c: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100006450: 0f 85 83 00 00 00           	jne	0x1000064d9 <_lookup+0xd9>
100006456: 85 d2                       	testl	%edx, %edx
100006458: 74 48                       	je	0x1000064a2 <_lookup+0xa2>
10000645a: 48 8b 47 68                 	movq	0x68(%rdi), %rax
10000645e: 48 3b 47 70                 	cmpq	0x70(%rdi), %rax
100006462: 74 3e                       	je	0x1000064a2 <_lookup+0xa2>
100006464: 48 83 7f 78 ff              	cmpq	$-0x1, 0x78(%rdi)
100006469: 74 37                       	je	0x1000064a2 <_lookup+0xa2>
10000646b: 49 89 fe                    	movq	%rdi, %r14
10000646e: 49 89 f7                    	movq	%rsi, %r15
100006471: bf 01 00 00 00              	movl	$0x1, %edi
100006476: be 78 00 00 00              	movl	$0x78, %esi
10000647b: e8 68 1b 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
100006480: 48 85 c0                    	testq	%rax, %rax
100006483: 74 1d                       	je	0x1000064a2 <_lookup+0xa2>
100006485: 48 89 c3                    	movq	%rax, %rbx
100006488: 48 89 c7                    	movq	%rax, %rdi
10000648b: 48 83 c7 48                 	addq	$0x48, %rdi
10000648f: 31 f6                       	xorl	%esi, %esi
100006491: e8 9a 1b 00 00              	callq	0x100008030 <_sysconf+0x100008030>
100006496: 85 c0                       	testl	%eax, %eax
100006498: 74 15                       	je	0x1000064af <_lookup+0xaf>
10000649a: 48 89 df                    	movq	%rbx, %rdi
10000649d: e8 64 1b 00 00              	callq	0x100008006 <_sysconf+0x100008006>
1000064a2: 31 c0                       	xorl	%eax, %eax
1000064a4: 48 83 c4 08                 	addq	$0x8, %rsp
1000064a8: 5b                          	popq	%rbx
1000064a9: 41 5e                       	popq	%r14
1000064ab: 41 5f                       	popq	%r15
1000064ad: 5d                          	popq	%rbp
1000064ae: c3                          	retq
1000064af: 48 89 d8                    	movq	%rbx, %rax
1000064b2: 4c 89 7b 08                 	movq	%r15, 0x8(%rbx)
1000064b6: 49 8b 4e 78                 	movq	0x78(%r14), %rcx
1000064ba: 48 ff c1                    	incq	%rcx
1000064bd: 49 89 4e 78                 	movq	%rcx, 0x78(%r14)
1000064c1: 48 89 4b 10                 	movq	%rcx, 0x10(%rbx)
1000064c5: 49 8b 56 60                 	movq	0x60(%r14), %rdx
1000064c9: 48 89 13                    	movq	%rdx, (%rbx)
1000064cc: 49 89 5e 60                 	movq	%rbx, 0x60(%r14)
1000064d0: 49 ff 46 68                 	incq	0x68(%r14)
1000064d4: 49 89 0f                    	movq	%rcx, (%r15)
1000064d7: eb cb                       	jmp	0x1000064a4 <_lookup+0xa4>
1000064d9: e8 fe 1a 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
1000064de: 66 90                       	nop

00000001000064e0 <_destroy_active>:
1000064e0: 55                          	pushq	%rbp
1000064e1: 48 89 e5                    	movq	%rsp, %rbp
1000064e4: 41 57                       	pushq	%r15
1000064e6: 41 56                       	pushq	%r14
1000064e8: 53                          	pushq	%rbx
1000064e9: 50                          	pushq	%rax
1000064ea: 48 83 7e 20 00              	cmpq	$0x0, 0x20(%rsi)
1000064ef: 75 68                       	jne	0x100006559 <_destroy_active+0x79>
1000064f1: 48 89 f3                    	movq	%rsi, %rbx
1000064f4: 48 83 7e 38 00              	cmpq	$0x0, 0x38(%rsi)
1000064f9: 75 5e                       	jne	0x100006559 <_destroy_active+0x79>
1000064fb: 49 89 fe                    	movq	%rdi, %r14
1000064fe: 48 8d 47 60                 	leaq	0x60(%rdi), %rax
100006502: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100006510: 49 89 c7                    	movq	%rax, %r15
100006513: 48 8b 00                    	movq	(%rax), %rax
100006516: 48 85 c0                    	testq	%rax, %rax
100006519: 74 05                       	je	0x100006520 <_destroy_active+0x40>
10000651b: 48 39 d8                    	cmpq	%rbx, %rax
10000651e: 75 f0                       	jne	0x100006510 <_destroy_active+0x30>
100006520: 48 85 c0                    	testq	%rax, %rax
100006523: 74 34                       	je	0x100006559 <_destroy_active+0x79>
100006525: 48 8d 7b 48                 	leaq	0x48(%rbx), %rdi
100006529: e8 fc 1a 00 00              	callq	0x10000802a <_sysconf+0x10000802a>
10000652e: 85 c0                       	testl	%eax, %eax
100006530: 75 27                       	jne	0x100006559 <_destroy_active+0x79>
100006532: 48 8b 43 08                 	movq	0x8(%rbx), %rax
100006536: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
10000653d: 48 8b 03                    	movq	(%rbx), %rax
100006540: 49 89 07                    	movq	%rax, (%r15)
100006543: 49 ff 4e 68                 	decq	0x68(%r14)
100006547: 48 89 df                    	movq	%rbx, %rdi
10000654a: 48 83 c4 08                 	addq	$0x8, %rsp
10000654e: 5b                          	popq	%rbx
10000654f: 41 5e                       	popq	%r14
100006551: 41 5f                       	popq	%r15
100006553: 5d                          	popq	%rbp
100006554: e9 ad 1a 00 00              	jmp	0x100008006 <_sysconf+0x100008006>
100006559: e8 7e 1a 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
10000655e: 66 90                       	nop

0000000100006560 <_mb_win_init_once_complete>:
100006560: 55                          	pushq	%rbp
100006561: 48 89 e5                    	movq	%rsp, %rbp
100006564: 41 57                       	pushq	%r15
100006566: 41 56                       	pushq	%r14
100006568: 41 55                       	pushq	%r13
10000656a: 41 54                       	pushq	%r12
10000656c: 53                          	pushq	%rbx
10000656d: 48 83 ec 18                 	subq	$0x18, %rsp
100006571: 48 85 ff                    	testq	%rdi, %rdi
100006574: 0f 84 d1 02 00 00           	je	0x10000684b <_mb_win_init_once_complete+0x2eb>
10000657a: 4c 8b 37                    	movq	(%rdi), %r14
10000657d: 4d 85 f6                    	testq	%r14, %r14
100006580: 0f 84 c5 02 00 00           	je	0x10000684b <_mb_win_init_once_complete+0x2eb>
100006586: 49 89 cf                    	movq	%rcx, %r15
100006589: 41 89 d5                    	movl	%edx, %r13d
10000658c: 49 89 f4                    	movq	%rsi, %r12
10000658f: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100006593: e8 88 bb ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100006598: 4c 89 f7                    	movq	%r14, %rdi
10000659b: e8 e4 1a 00 00              	callq	0x100008084 <_sysconf+0x100008084>
1000065a0: 85 c0                       	testl	%eax, %eax
1000065a2: 0f 85 a3 02 00 00           	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
1000065a8: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000065ac: e8 6f bb ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000065b1: 48 89 c3                    	movq	%rax, %rbx
1000065b4: 41 f7 c5 f9 ff ff ff        	testl	$0xfffffff9, %r13d      ## imm = 0xFFFFFFF9
1000065bb: 0f 94 c0                    	sete	%al
1000065be: 41 83 fd 06                 	cmpl	$0x6, %r13d
1000065c2: 0f 95 c1                    	setne	%cl
1000065c5: 84 c1                       	testb	%al, %cl
1000065c7: 75 1c                       	jne	0x1000065e5 <_mb_win_init_once_complete+0x85>
1000065c9: 4c 89 f7                    	movq	%r14, %rdi
1000065cc: e8 b9 1a 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000065d1: 85 c0                       	testl	%eax, %eax
1000065d3: 0f 85 72 02 00 00           	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
1000065d9: 31 c0                       	xorl	%eax, %eax
1000065db: b9 57 00 00 00              	movl	$0x57, %ecx
1000065e0: e9 48 02 00 00              	jmp	0x10000682d <_mb_win_init_once_complete+0x2cd>
1000065e5: 4c 89 7d c0                 	movq	%r15, -0x40(%rbp)
1000065e9: 4d 85 e4                    	testq	%r12, %r12
1000065ec: 0f 84 59 02 00 00           	je	0x10000684b <_mb_win_init_once_complete+0x2eb>
1000065f2: 44 89 e0                    	movl	%r12d, %eax
1000065f5: 83 e0 07                    	andl	$0x7, %eax
1000065f8: 0f 85 4d 02 00 00           	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
1000065fe: 8b 0b                       	movl	(%rbx), %ecx
100006600: 4d 8d 7e 60                 	leaq	0x60(%r14), %r15
100006604: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
100006610: 4d 8b 3f                    	movq	(%r15), %r15
100006613: 4d 85 ff                    	testq	%r15, %r15
100006616: 0f 84 93 00 00 00           	je	0x1000066af <_mb_win_init_once_complete+0x14f>
10000661c: 4d 39 67 08                 	cmpq	%r12, 0x8(%r15)
100006620: 75 ee                       	jne	0x100006610 <_mb_win_init_once_complete+0xb0>
100006622: 49 8b 04 24                 	movq	(%r12), %rax
100006626: 49 3b 47 10                 	cmpq	0x10(%r15), %rax
10000662a: 0f 85 1b 02 00 00           	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
100006630: 4d 85 ff                    	testq	%r15, %r15
100006633: 0f 84 8d 00 00 00           	je	0x1000066c6 <_mb_win_init_once_complete+0x166>
100006639: 41 8b 47 18                 	movl	0x18(%r15), %eax
10000663d: 83 f8 01                    	cmpl	$0x1, %eax
100006640: 48 89 5d c8                 	movq	%rbx, -0x38(%rbp)
100006644: 0f 84 cc 00 00 00           	je	0x100006716 <_mb_win_init_once_complete+0x1b6>
10000664a: 83 f8 02                    	cmpl	$0x2, %eax
10000664d: 0f 84 8f 00 00 00           	je	0x1000066e2 <_mb_win_init_once_complete+0x182>
100006653: 83 f8 03                    	cmpl	$0x3, %eax
100006656: 0f 85 54 01 00 00           	jne	0x1000067b0 <_mb_win_init_once_complete+0x250>
10000665c: 49 8b 4f 38                 	movq	0x38(%r15), %rcx
100006660: 48 85 c9                    	testq	%rcx, %rcx
100006663: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
100006667: 0f 84 ad 01 00 00           	je	0x10000681a <_mb_win_init_once_complete+0x2ba>
10000666d: 49 83 c7 38                 	addq	$0x38, %r15
100006671: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
100006680: 4c 8b 61 08                 	movq	0x8(%rcx), %r12
100006684: e8 07 1a 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100006689: 4c 89 e7                    	movq	%r12, %rdi
10000668c: 48 89 c6                    	movq	%rax, %rsi
10000668f: e8 c6 19 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100006694: 49 8b 3f                    	movq	(%r15), %rdi
100006697: 48 8b 0f                    	movq	(%rdi), %rcx
10000669a: 85 c0                       	testl	%eax, %eax
10000669c: 0f 85 70 01 00 00           	jne	0x100006812 <_mb_win_init_once_complete+0x2b2>
1000066a2: 49 89 ff                    	movq	%rdi, %r15
1000066a5: 48 85 c9                    	testq	%rcx, %rcx
1000066a8: 75 d6                       	jne	0x100006680 <_mb_win_init_once_complete+0x120>
1000066aa: e9 6b 01 00 00              	jmp	0x10000681a <_mb_win_init_once_complete+0x2ba>
1000066af: 49 83 3c 24 00              	cmpq	$0x0, (%r12)
1000066b4: 0f 85 91 01 00 00           	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
1000066ba: 45 31 ff                    	xorl	%r15d, %r15d
1000066bd: 4d 85 ff                    	testq	%r15, %r15
1000066c0: 0f 85 73 ff ff ff           	jne	0x100006639 <_mb_win_init_once_complete+0xd9>
1000066c6: 4c 89 f7                    	movq	%r14, %rdi
1000066c9: e8 bc 19 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000066ce: 85 c0                       	testl	%eax, %eax
1000066d0: 0f 85 75 01 00 00           	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
1000066d6: 31 c0                       	xorl	%eax, %eax
1000066d8: b9 1f 00 00 00              	movl	$0x1f, %ecx
1000066dd: e9 4b 01 00 00              	jmp	0x10000682d <_mb_win_init_once_complete+0x2cd>
1000066e2: 89 4d d4                    	movl	%ecx, -0x2c(%rbp)
1000066e5: 49 8d 5f 38                 	leaq	0x38(%r15), %rbx
1000066e9: 0f 1f 80 00 00 00 00        	nopl	(%rax)
1000066f0: 48 8b 1b                    	movq	(%rbx), %rbx
1000066f3: 48 85 db                    	testq	%rbx, %rbx
1000066f6: 0f 84 b4 00 00 00           	je	0x1000067b0 <_mb_win_init_once_complete+0x250>
1000066fc: 4c 8b 63 08                 	movq	0x8(%rbx), %r12
100006700: e8 8b 19 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100006705: 4c 89 e7                    	movq	%r12, %rdi
100006708: 48 89 c6                    	movq	%rax, %rsi
10000670b: e8 4a 19 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100006710: 85 c0                       	testl	%eax, %eax
100006712: 74 dc                       	je	0x1000066f0 <_mb_win_init_once_complete+0x190>
100006714: eb 1b                       	jmp	0x100006731 <_mb_win_init_once_complete+0x1d1>
100006716: 89 4d d4                    	movl	%ecx, -0x2c(%rbp)
100006719: 4d 8b 67 30                 	movq	0x30(%r15), %r12
10000671d: e8 6e 19 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100006722: 4c 89 e7                    	movq	%r12, %rdi
100006725: 48 89 c6                    	movq	%rax, %rsi
100006728: e8 2d 19 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
10000672d: 85 c0                       	testl	%eax, %eax
10000672f: 74 7f                       	je	0x1000067b0 <_mb_win_init_once_complete+0x250>
100006731: 41 83 fd 04                 	cmpl	$0x4, %r13d
100006735: 0f 83 92 00 00 00           	jae	0x1000067cd <_mb_win_init_once_complete+0x26d>
10000673b: 41 83 7f 18 02              	cmpl	$0x2, 0x18(%r15)
100006740: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
100006744: 75 58                       	jne	0x10000679e <_mb_win_init_once_complete+0x23e>
100006746: 4d 8d 6f 38                 	leaq	0x38(%r15), %r13
10000674a: 49 8b 47 38                 	movq	0x38(%r15), %rax
10000674e: 48 85 c0                    	testq	%rax, %rax
100006751: 74 32                       	je	0x100006785 <_mb_win_init_once_complete+0x225>
100006753: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)
100006760: 4c 8b 60 08                 	movq	0x8(%rax), %r12
100006764: e8 27 19 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100006769: 4c 89 e7                    	movq	%r12, %rdi
10000676c: 48 89 c6                    	movq	%rax, %rsi
10000676f: e8 e6 18 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100006774: 85 c0                       	testl	%eax, %eax
100006776: 75 0d                       	jne	0x100006785 <_mb_win_init_once_complete+0x225>
100006778: 4d 8b 6d 00                 	movq	(%r13), %r13
10000677c: 49 8b 45 00                 	movq	(%r13), %rax
100006780: 48 85 c0                    	testq	%rax, %rax
100006783: 75 db                       	jne	0x100006760 <_mb_win_init_once_complete+0x200>
100006785: 49 8b 7d 00                 	movq	(%r13), %rdi
100006789: 48 85 ff                    	testq	%rdi, %rdi
10000678c: 0f 84 b9 00 00 00           	je	0x10000684b <_mb_win_init_once_complete+0x2eb>
100006792: 48 8b 07                    	movq	(%rdi), %rax
100006795: 49 89 45 00                 	movq	%rax, (%r13)
100006799: e8 68 18 00 00              	callq	0x100008006 <_sysconf+0x100008006>
10000679e: 41 c7 47 18 03 00 00 00     	movl	$0x3, 0x18(%r15)
1000067a6: 48 8b 45 c0                 	movq	-0x40(%rbp), %rax
1000067aa: 49 89 47 40                 	movq	%rax, 0x40(%r15)
1000067ae: eb 3a                       	jmp	0x1000067ea <_mb_win_init_once_complete+0x28a>
1000067b0: 4c 89 f7                    	movq	%r14, %rdi
1000067b3: e8 d2 18 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000067b8: 85 c0                       	testl	%eax, %eax
1000067ba: 0f 85 8b 00 00 00           	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
1000067c0: 31 c0                       	xorl	%eax, %eax
1000067c2: b9 1f 00 00 00              	movl	$0x1f, %ecx
1000067c7: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000067cb: eb 60                       	jmp	0x10000682d <_mb_win_init_once_complete+0x2cd>
1000067cd: 49 8b 47 20                 	movq	0x20(%r15), %rax
1000067d1: 48 85 c0                    	testq	%rax, %rax
1000067d4: 48 8b 5d c8                 	movq	-0x38(%rbp), %rbx
1000067d8: 74 64                       	je	0x10000683e <_mb_win_init_once_complete+0x2de>
1000067da: 41 c7 47 18 01 00 00 00     	movl	$0x1, 0x18(%r15)
1000067e2: 48 8b 40 08                 	movq	0x8(%rax), %rax
1000067e6: 49 89 47 30                 	movq	%rax, 0x30(%r15)
1000067ea: 49 83 c7 48                 	addq	$0x48, %r15
1000067ee: 4c 89 ff                    	movq	%r15, %rdi
1000067f1: e8 2e 18 00 00              	callq	0x100008024 <_sysconf+0x100008024>
1000067f6: 85 c0                       	testl	%eax, %eax
1000067f8: 75 51                       	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
1000067fa: 4c 89 f7                    	movq	%r14, %rdi
1000067fd: e8 88 18 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006802: 89 c1                       	movl	%eax, %ecx
100006804: b8 01 00 00 00              	movl	$0x1, %eax
100006809: 85 c9                       	testl	%ecx, %ecx
10000680b: 8b 4d d4                    	movl	-0x2c(%rbp), %ecx
10000680e: 74 1d                       	je	0x10000682d <_mb_win_init_once_complete+0x2cd>
100006810: eb 39                       	jmp	0x10000684b <_mb_win_init_once_complete+0x2eb>
100006812: 49 89 0f                    	movq	%rcx, (%r15)
100006815: e8 ec 17 00 00              	callq	0x100008006 <_sysconf+0x100008006>
10000681a: 4c 89 f7                    	movq	%r14, %rdi
10000681d: e8 68 18 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006822: 85 c0                       	testl	%eax, %eax
100006824: 75 25                       	jne	0x10000684b <_mb_win_init_once_complete+0x2eb>
100006826: 31 c0                       	xorl	%eax, %eax
100006828: b9 b7 00 00 00              	movl	$0xb7, %ecx
10000682d: 89 0b                       	movl	%ecx, (%rbx)
10000682f: 48 83 c4 18                 	addq	$0x18, %rsp
100006833: 5b                          	popq	%rbx
100006834: 41 5c                       	popq	%r12
100006836: 41 5d                       	popq	%r13
100006838: 41 5e                       	popq	%r14
10000683a: 41 5f                       	popq	%r15
10000683c: 5d                          	popq	%rbp
10000683d: c3                          	retq
10000683e: 4c 89 f7                    	movq	%r14, %rdi
100006841: 4c 89 fe                    	movq	%r15, %rsi
100006844: e8 97 fc ff ff              	callq	0x1000064e0 <_destroy_active>
100006849: eb af                       	jmp	0x1000067fa <_mb_win_init_once_complete+0x29a>
10000684b: e8 8c 17 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>

0000000100006850 <_mb_win_mutex_open>:
100006850: 55                          	pushq	%rbp
100006851: 48 89 e5                    	movq	%rsp, %rbp
100006854: 41 57                       	pushq	%r15
100006856: 41 56                       	pushq	%r14
100006858: 41 55                       	pushq	%r13
10000685a: 41 54                       	pushq	%r12
10000685c: 53                          	pushq	%rbx
10000685d: 50                          	pushq	%rax
10000685e: 48 85 ff                    	testq	%rdi, %rdi
100006861: 0f 94 c1                    	sete	%cl
100006864: 48 85 f6                    	testq	%rsi, %rsi
100006867: 41 0f 94 c0                 	sete	%r8b
10000686b: b8 01 00 00 00              	movl	$0x1, %eax
100006870: 41 08 c8                    	orb	%cl, %r8b
100006873: 75 18                       	jne	0x10000688d <_mb_win_mutex_open+0x3d>
100006875: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100006879: 74 12                       	je	0x10000688d <_mb_win_mutex_open+0x3d>
10000687b: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100006880: 74 0b                       	je	0x10000688d <_mb_win_mutex_open+0x3d>
100006882: b8 02 00 00 00              	movl	$0x2, %eax
100006887: 48 83 3f 00                 	cmpq	$0x0, (%rdi)
10000688b: 74 0f                       	je	0x10000689c <_mb_win_mutex_open+0x4c>
10000688d: 48 83 c4 08                 	addq	$0x8, %rsp
100006891: 5b                          	popq	%rbx
100006892: 41 5c                       	popq	%r12
100006894: 41 5d                       	popq	%r13
100006896: 41 5e                       	popq	%r14
100006898: 41 5f                       	popq	%r15
10000689a: 5d                          	popq	%rbp
10000689b: c3                          	retq
10000689c: 49 89 f7                    	movq	%rsi, %r15
10000689f: 49 89 fd                    	movq	%rdi, %r13
1000068a2: 49 89 d4                    	movq	%rdx, %r12
1000068a5: bf 01 00 00 00              	movl	$0x1, %edi
1000068aa: be 98 00 00 00              	movl	$0x98, %esi
1000068af: e8 34 17 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
1000068b4: 48 85 c0                    	testq	%rax, %rax
1000068b7: 74 16                       	je	0x1000068cf <_mb_win_mutex_open+0x7f>
1000068b9: 48 89 c3                    	movq	%rax, %rbx
1000068bc: 48 89 c7                    	movq	%rax, %rdi
1000068bf: 31 f6                       	xorl	%esi, %esi
1000068c1: e8 b8 17 00 00              	callq	0x10000807e <_sysconf+0x10000807e>
1000068c6: 85 c0                       	testl	%eax, %eax
1000068c8: 74 19                       	je	0x1000068e3 <_mb_win_mutex_open+0x93>
1000068ca: 41 89 c6                    	movl	%eax, %r14d
1000068cd: eb 41                       	jmp	0x100006910 <_mb_win_mutex_open+0xc0>
1000068cf: b8 04 00 00 00              	movl	$0x4, %eax
1000068d4: 4d 85 e4                    	testq	%r12, %r12
1000068d7: 74 b4                       	je	0x10000688d <_mb_win_mutex_open+0x3d>
1000068d9: 41 c7 04 24 0c 00 00 00     	movl	$0xc, (%r12)
1000068e1: eb aa                       	jmp	0x10000688d <_mb_win_mutex_open+0x3d>
1000068e3: 4c 8d b3 88 00 00 00        	leaq	0x88(%rbx), %r14
1000068ea: 48 8d 35 bf 00 00 00        	leaq	0xbf(%rip), %rsi        ## 0x1000069b0 <_thread_exited>
1000068f1: 4c 89 f7                    	movq	%r14, %rdi
1000068f4: e8 73 17 00 00              	callq	0x10000806c <_sysconf+0x10000806c>
1000068f9: 85 c0                       	testl	%eax, %eax
1000068fb: 74 32                       	je	0x10000692f <_mb_win_mutex_open+0xdf>
1000068fd: 41 89 c6                    	movl	%eax, %r14d
100006900: 48 89 df                    	movq	%rbx, %rdi
100006903: e8 70 17 00 00              	callq	0x100008078 <_sysconf+0x100008078>
100006908: 85 c0                       	testl	%eax, %eax
10000690a: 0f 85 91 00 00 00           	jne	0x1000069a1 <_mb_win_mutex_open+0x151>
100006910: 48 89 df                    	movq	%rbx, %rdi
100006913: e8 ee 16 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100006918: b8 04 00 00 00              	movl	$0x4, %eax
10000691d: 4d 85 e4                    	testq	%r12, %r12
100006920: 0f 84 67 ff ff ff           	je	0x10000688d <_mb_win_mutex_open+0x3d>
100006926: 45 89 34 24                 	movl	%r14d, (%r12)
10000692a: e9 5e ff ff ff              	jmp	0x10000688d <_mb_win_mutex_open+0x3d>
10000692f: c7 83 90 00 00 00 01 00 00 00       	movl	$0x1, 0x90(%rbx)
100006939: 49 8b 3f                    	movq	(%r15), %rdi
10000693c: 48 89 de                    	movq	%rbx, %rsi
10000693f: 48 83 c6 48                 	addq	$0x48, %rsi
100006943: e8 58 b6 ff ff              	callq	0x100001fa0 <_mb_win_threads_acquire>
100006948: 85 c0                       	testl	%eax, %eax
10000694a: 74 2a                       	je	0x100006976 <_mb_win_mutex_open+0x126>
10000694c: 49 8b 3e                    	movq	(%r14), %rdi
10000694f: e8 1e 17 00 00              	callq	0x100008072 <_sysconf+0x100008072>
100006954: 85 c0                       	testl	%eax, %eax
100006956: 75 49                       	jne	0x1000069a1 <_mb_win_mutex_open+0x151>
100006958: 48 89 df                    	movq	%rbx, %rdi
10000695b: e8 18 17 00 00              	callq	0x100008078 <_sysconf+0x100008078>
100006960: 85 c0                       	testl	%eax, %eax
100006962: 75 3d                       	jne	0x1000069a1 <_mb_win_mutex_open+0x151>
100006964: 48 89 df                    	movq	%rbx, %rdi
100006967: e8 9a 16 00 00              	callq	0x100008006 <_sysconf+0x100008006>
10000696c: b8 02 00 00 00              	movl	$0x2, %eax
100006971: e9 17 ff ff ff              	jmp	0x10000688d <_mb_win_mutex_open+0x3d>
100006976: 49 8b 07                    	movq	(%r15), %rax
100006979: 48 89 43 40                 	movq	%rax, 0x40(%rbx)
10000697d: 49 8b 47 08                 	movq	0x8(%r15), %rax
100006981: 48 89 43 78                 	movq	%rax, 0x78(%rbx)
100006985: 49 89 5d 00                 	movq	%rbx, (%r13)
100006989: 31 c0                       	xorl	%eax, %eax
10000698b: 4d 85 e4                    	testq	%r12, %r12
10000698e: 0f 84 f9 fe ff ff           	je	0x10000688d <_mb_win_mutex_open+0x3d>
100006994: 41 c7 04 24 00 00 00 00     	movl	$0x0, (%r12)
10000699c: e9 ec fe ff ff              	jmp	0x10000688d <_mb_win_mutex_open+0x3d>
1000069a1: e8 36 16 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
1000069a6: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)

00000001000069b0 <_thread_exited>:
1000069b0: 55                          	pushq	%rbp
1000069b1: 48 89 e5                    	movq	%rsp, %rbp
1000069b4: 41 57                       	pushq	%r15
1000069b6: 41 56                       	pushq	%r14
1000069b8: 41 55                       	pushq	%r13
1000069ba: 41 54                       	pushq	%r12
1000069bc: 53                          	pushq	%rbx
1000069bd: 48 83 ec 18                 	subq	$0x18, %rsp
1000069c1: 48 85 ff                    	testq	%rdi, %rdi
1000069c4: 0f 84 bf 00 00 00           	je	0x100006a89 <_thread_exited+0xd9>
1000069ca: 48 89 fb                    	movq	%rdi, %rbx
1000069cd: 4c 8b 37                    	movq	(%rdi), %r14
1000069d0: e8 bb 16 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000069d5: 49 89 c7                    	movq	%rax, %r15
1000069d8: 4c 89 75 d0                 	movq	%r14, -0x30(%rbp)
1000069dc: 4c 89 f7                    	movq	%r14, %rdi
1000069df: e8 a0 16 00 00              	callq	0x100008084 <_sysconf+0x100008084>
1000069e4: 85 c0                       	testl	%eax, %eax
1000069e6: 0f 85 ac 00 00 00           	jne	0x100006a98 <_thread_exited+0xe8>
1000069ec: 48 89 5d c8                 	movq	%rbx, -0x38(%rbp)
1000069f0: 4c 8b 73 08                 	movq	0x8(%rbx), %r14
1000069f4: 4d 85 f6                    	testq	%r14, %r14
1000069f7: 74 7a                       	je	0x100006a73 <_thread_exited+0xc3>
1000069f9: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
1000069fd: 48 8d 58 60                 	leaq	0x60(%rax), %rbx
100006a01: eb 1a                       	jmp	0x100006a1d <_thread_exited+0x6d>
100006a03: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)
100006a10: 4c 89 e7                    	movq	%r12, %rdi
100006a13: e8 ee 15 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100006a18: 4d 85 f6                    	testq	%r14, %r14
100006a1b: 74 56                       	je	0x100006a73 <_thread_exited+0xc3>
100006a1d: 4d 89 f4                    	movq	%r14, %r12
100006a20: 4d 8b 36                    	movq	(%r14), %r14
100006a23: 49 8b 44 24 08              	movq	0x8(%r12), %rax
100006a28: 49 89 dd                    	movq	%rbx, %r13
100006a2b: 0f 1f 44 00 00              	nopl	(%rax,%rax)
100006a30: 4d 8b 6d 00                 	movq	(%r13), %r13
100006a34: 4d 85 ed                    	testq	%r13, %r13
100006a37: 74 d7                       	je	0x100006a10 <_thread_exited+0x60>
100006a39: 49 39 45 08                 	cmpq	%rax, 0x8(%r13)
100006a3d: 75 f1                       	jne	0x100006a30 <_thread_exited+0x80>
100006a3f: 41 83 7d 34 00              	cmpl	$0x0, 0x34(%r13)
100006a44: 74 ca                       	je	0x100006a10 <_thread_exited+0x60>
100006a46: 49 8b 7d 40                 	movq	0x40(%r13), %rdi
100006a4a: 4c 89 fe                    	movq	%r15, %rsi
100006a4d: e8 08 16 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100006a52: 85 c0                       	testl	%eax, %eax
100006a54: 74 ba                       	je	0x100006a10 <_thread_exited+0x60>
100006a56: 48 b8 00 00 00 00 01 00 00 00       	movabsq	$0x100000000, %rax ## imm = 0x100000000
100006a60: 49 89 45 34                 	movq	%rax, 0x34(%r13)
100006a64: 4c 89 ef                    	movq	%r13, %rdi
100006a67: be 80 00 00 00              	movl	$0x80, %esi
100006a6c: e8 8f 0c 00 00              	callq	0x100007700 <_grant_first>
100006a71: eb 9d                       	jmp	0x100006a10 <_thread_exited+0x60>
100006a73: 48 8b 7d c8                 	movq	-0x38(%rbp), %rdi
100006a77: e8 8a 15 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100006a7c: 48 8b 7d d0                 	movq	-0x30(%rbp), %rdi
100006a80: e8 05 16 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006a85: 85 c0                       	testl	%eax, %eax
100006a87: 75 0f                       	jne	0x100006a98 <_thread_exited+0xe8>
100006a89: 48 83 c4 18                 	addq	$0x18, %rsp
100006a8d: 5b                          	popq	%rbx
100006a8e: 41 5c                       	popq	%r12
100006a90: 41 5d                       	popq	%r13
100006a92: 41 5e                       	popq	%r14
100006a94: 41 5f                       	popq	%r15
100006a96: 5d                          	popq	%rbp
100006a97: c3                          	retq
100006a98: e8 3f 15 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100006a9d: 0f 1f 00                    	nopl	(%rax)

0000000100006aa0 <_mb_win_mutex_close>:
100006aa0: 55                          	pushq	%rbp
100006aa1: 48 89 e5                    	movq	%rsp, %rbp
100006aa4: 41 57                       	pushq	%r15
100006aa6: 41 56                       	pushq	%r14
100006aa8: 53                          	pushq	%rbx
100006aa9: 50                          	pushq	%rax
100006aaa: 48 85 ff                    	testq	%rdi, %rdi
100006aad: 74 58                       	je	0x100006b07 <_mb_win_mutex_close+0x67>
100006aaf: 48 8b 1f                    	movq	(%rdi), %rbx
100006ab2: 48 85 db                    	testq	%rbx, %rbx
100006ab5: 74 57                       	je	0x100006b0e <_mb_win_mutex_close+0x6e>
100006ab7: 49 89 ff                    	movq	%rdi, %r15
100006aba: 49 89 f6                    	movq	%rsi, %r14
100006abd: 48 89 df                    	movq	%rbx, %rdi
100006ac0: e8 bf 15 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100006ac5: 85 c0                       	testl	%eax, %eax
100006ac7: 75 39                       	jne	0x100006b02 <_mb_win_mutex_close+0x62>
100006ac9: 48 83 7b 58 00              	cmpq	$0x0, 0x58(%rbx)
100006ace: 75 1f                       	jne	0x100006aef <_mb_win_mutex_close+0x4f>
100006ad0: 48 83 7b 68 00              	cmpq	$0x0, 0x68(%rbx)
100006ad5: 75 18                       	jne	0x100006aef <_mb_win_mutex_close+0x4f>
100006ad7: 48 8d 43 60                 	leaq	0x60(%rbx), %rax
100006adb: 48 89 c1                    	movq	%rax, %rcx
100006ade: 66 90                       	nop
100006ae0: 48 8b 09                    	movq	(%rcx), %rcx
100006ae3: 48 85 c9                    	testq	%rcx, %rcx
100006ae6: 74 36                       	je	0x100006b1e <_mb_win_mutex_close+0x7e>
100006ae8: 48 83 79 18 00              	cmpq	$0x0, 0x18(%rcx)
100006aed: 74 f1                       	je	0x100006ae0 <_mb_win_mutex_close+0x40>
100006aef: 48 89 df                    	movq	%rbx, %rdi
100006af2: e8 93 15 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006af7: 89 c1                       	movl	%eax, %ecx
100006af9: b8 03 00 00 00              	movl	$0x3, %eax
100006afe: 85 c9                       	testl	%ecx, %ecx
100006b00: 74 11                       	je	0x100006b13 <_mb_win_mutex_close+0x73>
100006b02: e8 d5 14 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100006b07: b8 01 00 00 00              	movl	$0x1, %eax
100006b0c: eb 05                       	jmp	0x100006b13 <_mb_win_mutex_close+0x73>
100006b0e: b8 02 00 00 00              	movl	$0x2, %eax
100006b13: 48 83 c4 08                 	addq	$0x8, %rsp
100006b17: 5b                          	popq	%rbx
100006b18: 41 5e                       	popq	%r14
100006b1a: 41 5f                       	popq	%r15
100006b1c: 5d                          	popq	%rbp
100006b1d: c3                          	retq
100006b1e: 48 8b 30                    	movq	(%rax), %rsi
100006b21: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
100006b30: 48 89 df                    	movq	%rbx, %rdi
100006b33: 48 85 f6                    	testq	%rsi, %rsi
100006b36: 74 0b                       	je	0x100006b43 <_mb_win_mutex_close+0xa3>
100006b38: e8 83 00 00 00              	callq	0x100006bc0 <_destroy_object>
100006b3d: 48 8b 73 60                 	movq	0x60(%rbx), %rsi
100006b41: eb ed                       	jmp	0x100006b30 <_mb_win_mutex_close+0x90>
100006b43: e8 42 15 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006b48: 85 c0                       	testl	%eax, %eax
100006b4a: 75 b6                       	jne	0x100006b02 <_mb_win_mutex_close+0x62>
100006b4c: 48 8b bb 88 00 00 00        	movq	0x88(%rbx), %rdi
100006b53: e8 1a 15 00 00              	callq	0x100008072 <_sysconf+0x100008072>
100006b58: 85 c0                       	testl	%eax, %eax
100006b5a: 75 16                       	jne	0x100006b72 <_mb_win_mutex_close+0xd2>
100006b5c: c7 83 90 00 00 00 00 00 00 00       	movl	$0x0, 0x90(%rbx)
100006b66: 48 89 df                    	movq	%rbx, %rdi
100006b69: e8 0a 15 00 00              	callq	0x100008078 <_sysconf+0x100008078>
100006b6e: 85 c0                       	testl	%eax, %eax
100006b70: 74 11                       	je	0x100006b83 <_mb_win_mutex_close+0xe3>
100006b72: 89 c1                       	movl	%eax, %ecx
100006b74: b8 04 00 00 00              	movl	$0x4, %eax
100006b79: 4d 85 f6                    	testq	%r14, %r14
100006b7c: 74 95                       	je	0x100006b13 <_mb_win_mutex_close+0x73>
100006b7e: 41 89 0e                    	movl	%ecx, (%r14)
100006b81: eb 90                       	jmp	0x100006b13 <_mb_win_mutex_close+0x73>
100006b83: 48 8d 7b 48                 	leaq	0x48(%rbx), %rdi
100006b87: e8 e4 b4 ff ff              	callq	0x100002070 <_mb_win_threads_release_pin>
100006b8c: 85 c0                       	testl	%eax, %eax
100006b8e: 0f 85 6e ff ff ff           	jne	0x100006b02 <_mb_win_mutex_close+0x62>
100006b94: 48 89 df                    	movq	%rbx, %rdi
100006b97: e8 6a 14 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100006b9c: 49 c7 07 00 00 00 00        	movq	$0x0, (%r15)
100006ba3: 31 c9                       	xorl	%ecx, %ecx
100006ba5: b8 00 00 00 00              	movl	$0x0, %eax
100006baa: 4d 85 f6                    	testq	%r14, %r14
100006bad: 75 cf                       	jne	0x100006b7e <_mb_win_mutex_close+0xde>
100006baf: e9 5f ff ff ff              	jmp	0x100006b13 <_mb_win_mutex_close+0x73>
100006bb4: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)

0000000100006bc0 <_destroy_object>:
100006bc0: 55                          	pushq	%rbp
100006bc1: 48 89 e5                    	movq	%rsp, %rbp
100006bc4: 53                          	pushq	%rbx
100006bc5: 50                          	pushq	%rax
100006bc6: 48 83 7e 48 00              	cmpq	$0x0, 0x48(%rsi)
100006bcb: 75 56                       	jne	0x100006c23 <_destroy_object+0x63>
100006bcd: 48 89 f3                    	movq	%rsi, %rbx
100006bd0: 48 83 7e 18 00              	cmpq	$0x0, 0x18(%rsi)
100006bd5: 75 4c                       	jne	0x100006c23 <_destroy_object+0x63>
100006bd7: 48 8d 4f 60                 	leaq	0x60(%rdi), %rcx
100006bdb: 0f 1f 44 00 00              	nopl	(%rax,%rax)
100006be0: 48 89 c8                    	movq	%rcx, %rax
100006be3: 48 8b 09                    	movq	(%rcx), %rcx
100006be6: 48 85 c9                    	testq	%rcx, %rcx
100006be9: 74 05                       	je	0x100006bf0 <_destroy_object+0x30>
100006beb: 48 39 d9                    	cmpq	%rbx, %rcx
100006bee: 75 f0                       	jne	0x100006be0 <_destroy_object+0x20>
100006bf0: 48 85 c9                    	testq	%rcx, %rcx
100006bf3: 74 2e                       	je	0x100006c23 <_destroy_object+0x63>
100006bf5: 48 8b 0b                    	movq	(%rbx), %rcx
100006bf8: 48 89 08                    	movq	%rcx, (%rax)
100006bfb: 48 ff 4f 70                 	decq	0x70(%rdi)
100006bff: 48 8b 7b 20                 	movq	0x20(%rbx), %rdi
100006c03: e8 fe 13 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100006c08: 48 8d 7b 58                 	leaq	0x58(%rbx), %rdi
100006c0c: e8 19 14 00 00              	callq	0x10000802a <_sysconf+0x10000802a>
100006c11: 85 c0                       	testl	%eax, %eax
100006c13: 75 0e                       	jne	0x100006c23 <_destroy_object+0x63>
100006c15: 48 89 df                    	movq	%rbx, %rdi
100006c18: 48 83 c4 08                 	addq	$0x8, %rsp
100006c1c: 5b                          	popq	%rbx
100006c1d: 5d                          	popq	%rbp
100006c1e: e9 e3 13 00 00              	jmp	0x100008006 <_sysconf+0x100008006>
100006c23: e8 b4 13 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100006c28: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100006c30 <_mb_win_mutex_acquire_pin>:
100006c30: 55                          	pushq	%rbp
100006c31: 48 89 e5                    	movq	%rsp, %rbp
100006c34: 41 57                       	pushq	%r15
100006c36: 41 56                       	pushq	%r14
100006c38: 53                          	pushq	%rbx
100006c39: 50                          	pushq	%rax
100006c3a: 48 85 ff                    	testq	%rdi, %rdi
100006c3d: 0f 94 c1                    	sete	%cl
100006c40: 48 85 f6                    	testq	%rsi, %rsi
100006c43: 0f 94 c2                    	sete	%dl
100006c46: b8 01 00 00 00              	movl	$0x1, %eax
100006c4b: 08 ca                       	orb	%cl, %dl
100006c4d: 75 6e                       	jne	0x100006cbd <_mb_win_mutex_acquire_pin+0x8d>
100006c4f: 48 8b 1f                    	movq	(%rdi), %rbx
100006c52: b8 02 00 00 00              	movl	$0x2, %eax
100006c57: 48 85 db                    	testq	%rbx, %rbx
100006c5a: 74 61                       	je	0x100006cbd <_mb_win_mutex_acquire_pin+0x8d>
100006c5c: 48 83 3e 00                 	cmpq	$0x0, (%rsi)
100006c60: 75 5b                       	jne	0x100006cbd <_mb_win_mutex_acquire_pin+0x8d>
100006c62: 48 83 7e 08 00              	cmpq	$0x0, 0x8(%rsi)
100006c67: 75 54                       	jne	0x100006cbd <_mb_win_mutex_acquire_pin+0x8d>
100006c69: 49 89 ff                    	movq	%rdi, %r15
100006c6c: 49 89 f6                    	movq	%rsi, %r14
100006c6f: 48 89 df                    	movq	%rbx, %rdi
100006c72: e8 0d 14 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100006c77: 85 c0                       	testl	%eax, %eax
100006c79: 75 5e                       	jne	0x100006cd9 <_mb_win_mutex_acquire_pin+0xa9>
100006c7b: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100006c7f: 48 85 c9                    	testq	%rcx, %rcx
100006c82: 4c 89 f0                    	movq	%r14, %rax
100006c85: 4c 89 fa                    	movq	%r15, %rdx
100006c88: 74 14                       	je	0x100006c9e <_mb_win_mutex_acquire_pin+0x6e>
100006c8a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)
100006c90: 48 39 c1                    	cmpq	%rax, %rcx
100006c93: 74 33                       	je	0x100006cc8 <_mb_win_mutex_acquire_pin+0x98>
100006c95: 48 8b 49 08                 	movq	0x8(%rcx), %rcx
100006c99: 48 85 c9                    	testq	%rcx, %rcx
100006c9c: 75 f2                       	jne	0x100006c90 <_mb_win_mutex_acquire_pin+0x60>
100006c9e: 48 89 10                    	movq	%rdx, (%rax)
100006ca1: 48 8b 4b 58                 	movq	0x58(%rbx), %rcx
100006ca5: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
100006ca9: 48 89 43 58                 	movq	%rax, 0x58(%rbx)
100006cad: 48 89 df                    	movq	%rbx, %rdi
100006cb0: e8 d5 13 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006cb5: 89 c1                       	movl	%eax, %ecx
100006cb7: 31 c0                       	xorl	%eax, %eax
100006cb9: 85 c9                       	testl	%ecx, %ecx
100006cbb: 75 1c                       	jne	0x100006cd9 <_mb_win_mutex_acquire_pin+0xa9>
100006cbd: 48 83 c4 08                 	addq	$0x8, %rsp
100006cc1: 5b                          	popq	%rbx
100006cc2: 41 5e                       	popq	%r14
100006cc4: 41 5f                       	popq	%r15
100006cc6: 5d                          	popq	%rbp
100006cc7: c3                          	retq
100006cc8: 48 89 df                    	movq	%rbx, %rdi
100006ccb: e8 ba 13 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006cd0: 85 c0                       	testl	%eax, %eax
100006cd2: b8 02 00 00 00              	movl	$0x2, %eax
100006cd7: 74 e4                       	je	0x100006cbd <_mb_win_mutex_acquire_pin+0x8d>
100006cd9: e8 fe 12 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100006cde: 66 90                       	nop

0000000100006ce0 <_mb_win_mutex_release_pin>:
100006ce0: 55                          	pushq	%rbp
100006ce1: 48 89 e5                    	movq	%rsp, %rbp
100006ce4: 41 56                       	pushq	%r14
100006ce6: 53                          	pushq	%rbx
100006ce7: 48 85 ff                    	testq	%rdi, %rdi
100006cea: 74 6c                       	je	0x100006d58 <_mb_win_mutex_release_pin+0x78>
100006cec: 48 8b 0f                    	movq	(%rdi), %rcx
100006cef: b8 02 00 00 00              	movl	$0x2, %eax
100006cf4: 48 85 c9                    	testq	%rcx, %rcx
100006cf7: 74 64                       	je	0x100006d5d <_mb_win_mutex_release_pin+0x7d>
100006cf9: 48 8b 19                    	movq	(%rcx), %rbx
100006cfc: 48 85 db                    	testq	%rbx, %rbx
100006cff: 74 5c                       	je	0x100006d5d <_mb_win_mutex_release_pin+0x7d>
100006d01: 49 89 fe                    	movq	%rdi, %r14
100006d04: 48 89 df                    	movq	%rbx, %rdi
100006d07: e8 78 13 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100006d0c: 85 c0                       	testl	%eax, %eax
100006d0e: 75 63                       	jne	0x100006d73 <_mb_win_mutex_release_pin+0x93>
100006d10: 48 8d 73 58                 	leaq	0x58(%rbx), %rsi
100006d14: 4c 89 f0                    	movq	%r14, %rax
100006d17: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100006d20: 48 89 f1                    	movq	%rsi, %rcx
100006d23: 48 8b 16                    	movq	(%rsi), %rdx
100006d26: 48 85 d2                    	testq	%rdx, %rdx
100006d29: 74 09                       	je	0x100006d34 <_mb_win_mutex_release_pin+0x54>
100006d2b: 48 8d 72 08                 	leaq	0x8(%rdx), %rsi
100006d2f: 48 39 c2                    	cmpq	%rax, %rdx
100006d32: 75 ec                       	jne	0x100006d20 <_mb_win_mutex_release_pin+0x40>
100006d34: 48 85 d2                    	testq	%rdx, %rdx
100006d37: 74 29                       	je	0x100006d62 <_mb_win_mutex_release_pin+0x82>
100006d39: 48 8b 50 08                 	movq	0x8(%rax), %rdx
100006d3d: 48 89 11                    	movq	%rdx, (%rcx)
100006d40: 0f 57 c0                    	xorps	%xmm0, %xmm0
100006d43: 0f 11 00                    	movups	%xmm0, (%rax)
100006d46: 48 89 df                    	movq	%rbx, %rdi
100006d49: e8 3c 13 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006d4e: 89 c1                       	movl	%eax, %ecx
100006d50: 31 c0                       	xorl	%eax, %eax
100006d52: 85 c9                       	testl	%ecx, %ecx
100006d54: 74 07                       	je	0x100006d5d <_mb_win_mutex_release_pin+0x7d>
100006d56: eb 1b                       	jmp	0x100006d73 <_mb_win_mutex_release_pin+0x93>
100006d58: b8 01 00 00 00              	movl	$0x1, %eax
100006d5d: 5b                          	popq	%rbx
100006d5e: 41 5e                       	popq	%r14
100006d60: 5d                          	popq	%rbp
100006d61: c3                          	retq
100006d62: 48 89 df                    	movq	%rbx, %rdi
100006d65: e8 20 13 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006d6a: 85 c0                       	testl	%eax, %eax
100006d6c: b8 02 00 00 00              	movl	$0x2, %eax
100006d71: 74 ea                       	je	0x100006d5d <_mb_win_mutex_release_pin+0x7d>
100006d73: e8 64 12 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100006d78: 0f 1f 84 00 00 00 00 00     	nopl	(%rax,%rax)

0000000100006d80 <_mb_win_mutex_domain>:
100006d80: 48 85 ff                    	testq	%rdi, %rdi
100006d83: 74 12                       	je	0x100006d97 <_mb_win_mutex_domain+0x17>
100006d85: 48 8b 07                    	movq	(%rdi), %rax
100006d88: 48 85 c0                    	testq	%rax, %rax
100006d8b: 74 0a                       	je	0x100006d97 <_mb_win_mutex_domain+0x17>
100006d8d: 55                          	pushq	%rbp
100006d8e: 48 89 e5                    	movq	%rsp, %rbp
100006d91: 48 8b 40 40                 	movq	0x40(%rax), %rax
100006d95: 5d                          	popq	%rbp
100006d96: c3                          	retq
100006d97: 31 c0                       	xorl	%eax, %eax
100006d99: c3                          	retq
100006d9a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100006da0 <_mb_win_mutex_create>:
100006da0: 55                          	pushq	%rbp
100006da1: 48 89 e5                    	movq	%rsp, %rbp
100006da4: 41 57                       	pushq	%r15
100006da6: 41 56                       	pushq	%r14
100006da8: 41 55                       	pushq	%r13
100006daa: 41 54                       	pushq	%r12
100006dac: 53                          	pushq	%rbx
100006dad: 48 83 ec 48                 	subq	$0x48, %rsp
100006db1: 48 8b 05 58 22 00 00        	movq	0x2258(%rip), %rax      ## 0x100009010 <_sysconf+0x100009010>
100006db8: 48 8b 00                    	movq	(%rax), %rax
100006dbb: 48 89 45 d0                 	movq	%rax, -0x30(%rbp)
100006dbf: 48 85 ff                    	testq	%rdi, %rdi
100006dc2: 0f 84 3a 03 00 00           	je	0x100007102 <_mb_win_mutex_create+0x362>
100006dc8: 4c 8b 37                    	movq	(%rdi), %r14
100006dcb: 4d 85 f6                    	testq	%r14, %r14
100006dce: 0f 84 2e 03 00 00           	je	0x100007102 <_mb_win_mutex_create+0x362>
100006dd4: 41 89 d7                    	movl	%edx, %r15d
100006dd7: 49 89 f4                    	movq	%rsi, %r12
100006dda: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100006dde: e8 3d b3 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100006de3: 4c 89 f7                    	movq	%r14, %rdi
100006de6: e8 99 12 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100006deb: 85 c0                       	testl	%eax, %eax
100006ded: 0f 85 0f 03 00 00           	jne	0x100007102 <_mb_win_mutex_create+0x362>
100006df3: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100006df7: e8 24 b3 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100006dfc: 48 89 c3                    	movq	%rax, %rbx
100006dff: 41 83 ff 02                 	cmpl	$0x2, %r15d
100006e03: 73 4f                       	jae	0x100006e54 <_mb_win_mutex_create+0xb4>
100006e05: 44 8b 2b                    	movl	(%rbx), %r13d
100006e08: 48 c7 45 a8 00 00 00 00     	movq	$0x0, -0x58(%rbp)
100006e10: 48 c7 45 98 00 00 00 00     	movq	$0x0, -0x68(%rbp)
100006e18: c7 45 a4 00 00 00 00        	movl	$0x0, -0x5c(%rbp)
100006e1f: 48 8d 75 a8                 	leaq	-0x58(%rbp), %rsi
100006e23: 48 8d 55 98                 	leaq	-0x68(%rbp), %rdx
100006e27: 48 8d 4d a4                 	leaq	-0x5c(%rbp), %rcx
100006e2b: 4c 89 e7                    	movq	%r12, %rdi
100006e2e: e8 2d 03 00 00              	callq	0x100007160 <_parse_name>
100006e33: 85 c0                       	testl	%eax, %eax
100006e35: 74 3a                       	je	0x100006e71 <_mb_win_mutex_create+0xd1>
100006e37: 41 89 c4                    	movl	%eax, %r12d
100006e3a: 4c 89 f7                    	movq	%r14, %rdi
100006e3d: e8 48 12 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006e42: 85 c0                       	testl	%eax, %eax
100006e44: 0f 85 b8 02 00 00           	jne	0x100007102 <_mb_win_mutex_create+0x362>
100006e4a: 31 c0                       	xorl	%eax, %eax
100006e4c: 45 89 e5                    	movl	%r12d, %r13d
100006e4f: e9 e3 01 00 00              	jmp	0x100007037 <_mb_win_mutex_create+0x297>
100006e54: 4c 89 f7                    	movq	%r14, %rdi
100006e57: e8 2e 12 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006e5c: 85 c0                       	testl	%eax, %eax
100006e5e: 0f 85 9e 02 00 00           	jne	0x100007102 <_mb_win_mutex_create+0x362>
100006e64: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
100006e6a: 31 c0                       	xorl	%eax, %eax
100006e6c: e9 c9 01 00 00              	jmp	0x10000703a <_mb_win_mutex_create+0x29a>
100006e71: 44 8b 4d a4                 	movl	-0x5c(%rbp), %r9d
100006e75: 45 85 c9                    	testl	%r9d, %r9d
100006e78: 74 5f                       	je	0x100006ed9 <_mb_win_mutex_create+0x139>
100006e7a: 49 8b 46 70                 	movq	0x70(%r14), %rax
100006e7e: 49 3b 46 78                 	cmpq	0x78(%r14), %rax
100006e82: 0f 84 8e 01 00 00           	je	0x100007016 <_mb_win_mutex_create+0x276>
100006e88: 49 83 be 80 00 00 00 ff     	cmpq	$-0x1, 0x80(%r14)
100006e90: 0f 84 80 01 00 00           	je	0x100007016 <_mb_win_mutex_create+0x276>
100006e96: 44 89 4d b4                 	movl	%r9d, -0x4c(%rbp)
100006e9a: bf 01 00 00 00              	movl	$0x1, %edi
100006e9f: be 88 00 00 00              	movl	$0x88, %esi
100006ea4: e8 3f 11 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
100006ea9: 48 85 c0                    	testq	%rax, %rax
100006eac: 0f 84 64 01 00 00           	je	0x100007016 <_mb_win_mutex_create+0x276>
100006eb2: 48 89 45 b8                 	movq	%rax, -0x48(%rbp)
100006eb6: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
100006eba: e8 8f 11 00 00              	callq	0x10000804e <_sysconf+0x10000804e>
100006ebf: 85 c0                       	testl	%eax, %eax
100006ec1: 0f 84 21 01 00 00           	je	0x100006fe8 <_mb_win_mutex_create+0x248>
100006ec7: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
100006ecb: e8 78 11 00 00              	callq	0x100008048 <_sysconf+0x100008048>
100006ed0: 4c 8b 65 b8                 	movq	-0x48(%rbp), %r12
100006ed4: e9 35 01 00 00              	jmp	0x10000700e <_mb_win_mutex_create+0x26e>
100006ed9: 48 8b 7d a8                 	movq	-0x58(%rbp), %rdi
100006edd: 49 8b 46 60                 	movq	0x60(%r14), %rax
100006ee1: 48 85 c0                    	testq	%rax, %rax
100006ee4: 74 53                       	je	0x100006f39 <_mb_win_mutex_create+0x199>
100006ee6: 48 8b 4d 98                 	movq	-0x68(%rbp), %rcx
100006eea: eb 13                       	jmp	0x100006eff <_mb_win_mutex_create+0x15f>
100006eec: 31 d2                       	xorl	%edx, %edx
100006eee: 48 39 ca                    	cmpq	%rcx, %rdx
100006ef1: 4c 0f 44 e0                 	cmoveq	%rax, %r12
100006ef5: 74 45                       	je	0x100006f3c <_mb_win_mutex_create+0x19c>
100006ef7: 48 8b 00                    	movq	(%rax), %rax
100006efa: 48 85 c0                    	testq	%rax, %rax
100006efd: 74 3a                       	je	0x100006f39 <_mb_win_mutex_create+0x199>
100006eff: 83 78 30 00                 	cmpl	$0x0, 0x30(%rax)
100006f03: 74 f2                       	je	0x100006ef7 <_mb_win_mutex_create+0x157>
100006f05: 48 39 48 28                 	cmpq	%rcx, 0x28(%rax)
100006f09: 75 ec                       	jne	0x100006ef7 <_mb_win_mutex_create+0x157>
100006f0b: 48 85 c9                    	testq	%rcx, %rcx
100006f0e: 74 dc                       	je	0x100006eec <_mb_win_mutex_create+0x14c>
100006f10: 48 8b 70 20                 	movq	0x20(%rax), %rsi
100006f14: 31 d2                       	xorl	%edx, %edx
100006f16: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100006f20: 44 0f b7 04 56              	movzwl	(%rsi,%rdx,2), %r8d
100006f25: 66 44 3b 04 57              	cmpw	(%rdi,%rdx,2), %r8w
100006f2a: 75 c2                       	jne	0x100006eee <_mb_win_mutex_create+0x14e>
100006f2c: 48 ff c2                    	incq	%rdx
100006f2f: 48 39 d1                    	cmpq	%rdx, %rcx
100006f32: 75 ec                       	jne	0x100006f20 <_mb_win_mutex_create+0x180>
100006f34: 48 89 ca                    	movq	%rcx, %rdx
100006f37: eb b5                       	jmp	0x100006eee <_mb_win_mutex_create+0x14e>
100006f39: 45 31 e4                    	xorl	%r12d, %r12d
100006f3c: 4d 85 e4                    	testq	%r12, %r12
100006f3f: 0f 84 82 00 00 00           	je	0x100006fc7 <_mb_win_mutex_create+0x227>
100006f45: 44 89 4d b4                 	movl	%r9d, -0x4c(%rbp)
100006f49: e8 b8 10 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100006f4e: 49 8b 86 80 00 00 00        	movq	0x80(%r14), %rax
100006f55: 48 83 f8 ff                 	cmpq	$-0x1, %rax
100006f59: 74 3f                       	je	0x100006f9a <_mb_win_mutex_create+0x1fa>
100006f5b: 48 89 45 b8                 	movq	%rax, -0x48(%rbp)
100006f5f: bf 18 00 00 00              	movl	$0x18, %edi
100006f64: e8 a9 10 00 00              	callq	0x100008012 <_sysconf+0x100008012>
100006f69: 48 85 c0                    	testq	%rax, %rax
100006f6c: 74 2c                       	je	0x100006f9a <_mb_win_mutex_create+0x1fa>
100006f6e: 48 8b 55 b8                 	movq	-0x48(%rbp), %rdx
100006f72: 48 ff c2                    	incq	%rdx
100006f75: 49 89 96 80 00 00 00        	movq	%rdx, 0x80(%r14)
100006f7c: 48 89 55 b8                 	movq	%rdx, -0x48(%rbp)
100006f80: 48 89 50 08                 	movq	%rdx, 0x8(%rax)
100006f84: 4c 89 60 10                 	movq	%r12, 0x10(%rax)
100006f88: 49 8b 4e 68                 	movq	0x68(%r14), %rcx
100006f8c: 48 89 08                    	movq	%rcx, (%rax)
100006f8f: 49 89 46 68                 	movq	%rax, 0x68(%r14)
100006f93: 49 ff 44 24 10              	incq	0x10(%r12)
100006f98: eb 08                       	jmp	0x100006fa2 <_mb_win_mutex_create+0x202>
100006f9a: 48 c7 45 b8 00 00 00 00     	movq	$0x0, -0x48(%rbp)
100006fa2: 4c 89 f7                    	movq	%r14, %rdi
100006fa5: e8 e0 10 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100006faa: 89 c1                       	movl	%eax, %ecx
100006fac: 48 8b 45 b8                 	movq	-0x48(%rbp), %rax
100006fb0: 48 85 c0                    	testq	%rax, %rax
100006fb3: 74 14                       	je	0x100006fc9 <_mb_win_mutex_create+0x229>
100006fb5: ba b7 00 00 00              	movl	$0xb7, %edx
100006fba: 85 c9                       	testl	%ecx, %ecx
100006fbc: 44 8b 4d b4                 	movl	-0x4c(%rbp), %r9d
100006fc0: 74 1a                       	je	0x100006fdc <_mb_win_mutex_create+0x23c>
100006fc2: e9 3b 01 00 00              	jmp	0x100007102 <_mb_win_mutex_create+0x362>
100006fc7: eb 15                       	jmp	0x100006fde <_mb_win_mutex_create+0x23e>
100006fc9: 85 c9                       	testl	%ecx, %ecx
100006fcb: 44 8b 4d b4                 	movl	-0x4c(%rbp), %r9d
100006fcf: 0f 85 2d 01 00 00           	jne	0x100007102 <_mb_win_mutex_create+0x362>
100006fd5: ba 08 00 00 00              	movl	$0x8, %edx
100006fda: 31 c0                       	xorl	%eax, %eax
100006fdc: 89 13                       	movl	%edx, (%rbx)
100006fde: 4d 85 e4                    	testq	%r12, %r12
100006fe1: 75 57                       	jne	0x10000703a <_mb_win_mutex_create+0x29a>
100006fe3: e9 92 fe ff ff              	jmp	0x100006e7a <_mb_win_mutex_create+0xda>
100006fe8: 4c 8b 65 b8                 	movq	-0x48(%rbp), %r12
100006fec: 4c 89 e7                    	movq	%r12, %rdi
100006fef: 48 83 c7 58                 	addq	$0x58, %rdi
100006ff3: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
100006ff7: e8 34 10 00 00              	callq	0x100008030 <_sysconf+0x100008030>
100006ffc: 89 45 94                    	movl	%eax, -0x6c(%rbp)
100006fff: 48 8d 7d c0                 	leaq	-0x40(%rbp), %rdi
100007003: e8 40 10 00 00              	callq	0x100008048 <_sysconf+0x100008048>
100007008: 83 7d 94 00                 	cmpl	$0x0, -0x6c(%rbp)
10000700c: 74 4f                       	je	0x10000705d <_mb_win_mutex_create+0x2bd>
10000700e: 4c 89 e7                    	movq	%r12, %rdi
100007011: e8 f0 0f 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100007016: 48 8b 7d a8                 	movq	-0x58(%rbp), %rdi
10000701a: e8 e7 0f 00 00              	callq	0x100008006 <_sysconf+0x100008006>
10000701f: 4c 89 f7                    	movq	%r14, %rdi
100007022: e8 63 10 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007027: 85 c0                       	testl	%eax, %eax
100007029: 0f 85 d3 00 00 00           	jne	0x100007102 <_mb_win_mutex_create+0x362>
10000702f: 41 bd 08 00 00 00           	movl	$0x8, %r13d
100007035: 31 c0                       	xorl	%eax, %eax
100007037: 44 89 2b                    	movl	%r13d, (%rbx)
10000703a: 48 8b 0d cf 1f 00 00        	movq	0x1fcf(%rip), %rcx      ## 0x100009010 <_sysconf+0x100009010>
100007041: 48 8b 09                    	movq	(%rcx), %rcx
100007044: 48 3b 4d d0                 	cmpq	-0x30(%rbp), %rcx
100007048: 0f 85 07 01 00 00           	jne	0x100007155 <_mb_win_mutex_create+0x3b5>
10000704e: 48 83 c4 48                 	addq	$0x48, %rsp
100007052: 5b                          	popq	%rbx
100007053: 41 5c                       	popq	%r12
100007055: 41 5d                       	popq	%r13
100007057: 41 5e                       	popq	%r14
100007059: 41 5f                       	popq	%r15
10000705b: 5d                          	popq	%rbp
10000705c: c3                          	retq
10000705d: 49 8b 86 80 00 00 00        	movq	0x80(%r14), %rax
100007064: 48 ff c0                    	incq	%rax
100007067: 49 89 86 80 00 00 00        	movq	%rax, 0x80(%r14)
10000706e: 49 89 44 24 08              	movq	%rax, 0x8(%r12)
100007073: 83 7d b4 00                 	cmpl	$0x0, -0x4c(%rbp)
100007077: 74 0f                       	je	0x100007088 <_mb_win_mutex_create+0x2e8>
100007079: 48 8b 7d a8                 	movq	-0x58(%rbp), %rdi
10000707d: e8 84 0f 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100007082: 4c 8b 65 b8                 	movq	-0x48(%rbp), %r12
100007086: eb 1b                       	jmp	0x1000070a3 <_mb_win_mutex_create+0x303>
100007088: 41 c7 44 24 30 01 00 00 00  	movl	$0x1, 0x30(%r12)
100007091: 48 8b 45 a8                 	movq	-0x58(%rbp), %rax
100007095: 49 89 44 24 20              	movq	%rax, 0x20(%r12)
10000709a: 48 8b 45 98                 	movq	-0x68(%rbp), %rax
10000709e: 49 89 44 24 28              	movq	%rax, 0x28(%r12)
1000070a3: 49 8b 46 60                 	movq	0x60(%r14), %rax
1000070a7: 49 89 04 24                 	movq	%rax, (%r12)
1000070ab: 4d 89 66 60                 	movq	%r12, 0x60(%r14)
1000070af: 49 ff 46 70                 	incq	0x70(%r14)
1000070b3: 4c 89 f7                    	movq	%r14, %rdi
1000070b6: 4c 89 e6                    	movq	%r12, %rsi
1000070b9: e8 52 02 00 00              	callq	0x100007310 <_new_handle>
1000070be: 48 85 c0                    	testq	%rax, %rax
1000070c1: 74 44                       	je	0x100007107 <_mb_win_mutex_create+0x367>
1000070c3: 49 89 c4                    	movq	%rax, %r12
1000070c6: 45 85 ff                    	testl	%r15d, %r15d
1000070c9: 74 24                       	je	0x1000070ef <_mb_win_mutex_create+0x34f>
1000070cb: 4c 8b 7d b8                 	movq	-0x48(%rbp), %r15
1000070cf: 41 c7 47 34 01 00 00 00     	movl	$0x1, 0x34(%r15)
1000070d7: e8 b4 0f 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000070dc: 49 89 47 40                 	movq	%rax, 0x40(%r15)
1000070e0: 4c 89 f7                    	movq	%r14, %rdi
1000070e3: 4c 89 fe                    	movq	%r15, %rsi
1000070e6: e8 85 02 00 00              	callq	0x100007370 <_adopt>
1000070eb: 85 c0                       	testl	%eax, %eax
1000070ed: 74 28                       	je	0x100007117 <_mb_win_mutex_create+0x377>
1000070ef: 4c 89 f7                    	movq	%r14, %rdi
1000070f2: e8 93 0f 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000070f7: 85 c0                       	testl	%eax, %eax
1000070f9: 4c 89 e0                    	movq	%r12, %rax
1000070fc: 0f 84 35 ff ff ff           	je	0x100007037 <_mb_win_mutex_create+0x297>
100007102: e8 d5 0e 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100007107: 4c 89 f7                    	movq	%r14, %rdi
10000710a: 4c 89 e6                    	movq	%r12, %rsi
10000710d: e8 ae fa ff ff              	callq	0x100006bc0 <_destroy_object>
100007112: e9 08 ff ff ff              	jmp	0x10000701f <_mb_win_mutex_create+0x27f>
100007117: 48 8b 45 b8                 	movq	-0x48(%rbp), %rax
10000711b: c7 40 34 00 00 00 00        	movl	$0x0, 0x34(%rax)
100007122: 49 8d 7e 68                 	leaq	0x68(%r14), %rdi
100007126: 48 8b 3f                    	movq	(%rdi), %rdi
100007129: 48 85 ff                    	testq	%rdi, %rdi
10000712c: 74 06                       	je	0x100007134 <_mb_win_mutex_create+0x394>
10000712e: 4c 39 67 08                 	cmpq	%r12, 0x8(%rdi)
100007132: 75 f2                       	jne	0x100007126 <_mb_win_mutex_create+0x386>
100007134: 48 8b 07                    	movq	(%rdi), %rax
100007137: 49 89 46 68                 	movq	%rax, 0x68(%r14)
10000713b: e8 c6 0e 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100007140: 48 8b 75 b8                 	movq	-0x48(%rbp), %rsi
100007144: 48 ff 4e 10                 	decq	0x10(%rsi)
100007148: 4c 89 f7                    	movq	%r14, %rdi
10000714b: e8 70 fa ff ff              	callq	0x100006bc0 <_destroy_object>
100007150: e9 ca fe ff ff              	jmp	0x10000701f <_mb_win_mutex_create+0x27f>
100007155: e8 76 0e 00 00              	callq	0x100007fd0 <_sysconf+0x100007fd0>
10000715a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100007160 <_parse_name>:
100007160: 48 c7 06 00 00 00 00        	movq	$0x0, (%rsi)
100007167: 48 c7 02 00 00 00 00        	movq	$0x0, (%rdx)
10000716e: 48 85 ff                    	testq	%rdi, %rdi
100007171: 0f 84 ee 00 00 00           	je	0x100007265 <_parse_name+0x105>
100007177: 66 83 3f 00                 	cmpw	$0x0, (%rdi)
10000717b: 0f 84 e4 00 00 00           	je	0x100007265 <_parse_name+0x105>
100007181: 55                          	pushq	%rbp
100007182: 48 89 e5                    	movq	%rsp, %rbp
100007185: 41 57                       	pushq	%r15
100007187: 41 56                       	pushq	%r14
100007189: 41 55                       	pushq	%r13
10000718b: 41 54                       	pushq	%r12
10000718d: 53                          	pushq	%rbx
10000718e: 48 83 ec 18                 	subq	$0x18, %rsp
100007192: c7 01 00 00 00 00           	movl	$0x0, (%rcx)
100007198: 48 c7 c0 ff ff ff ff        	movq	$-0x1, %rax
10000719f: bb 02 00 00 00              	movl	$0x2, %ebx
1000071a4: 66 66 66 2e 0f 1f 84 00 00 00 00 00 	nopw	%cs:(%rax,%rax)
1000071b0: 4c 8d 78 01                 	leaq	0x1(%rax), %r15
1000071b4: 48 83 c3 fe                 	addq	$-0x2, %rbx
1000071b8: 49 81 ff 04 01 00 00        	cmpq	$0x104, %r15            ## imm = 0x104
1000071bf: 77 0b                       	ja	0x1000071cc <_parse_name+0x6c>
1000071c1: 66 83 7c 47 02 00           	cmpw	$0x0, 0x2(%rdi,%rax,2)
1000071c7: 4c 89 f8                    	movq	%r15, %rax
1000071ca: 75 e4                       	jne	0x1000071b0 <_parse_name+0x50>
1000071cc: b8 ce 00 00 00              	movl	$0xce, %eax
1000071d1: 49 81 ff 04 01 00 00        	cmpq	$0x104, %r15            ## imm = 0x104
1000071d8: 0f 87 18 01 00 00           	ja	0x1000072f6 <_parse_name+0x196>
1000071de: 49 89 fd                    	movq	%rdi, %r13
1000071e1: 48 89 75 d0                 	movq	%rsi, -0x30(%rbp)
1000071e5: 49 89 d4                    	movq	%rdx, %r12
1000071e8: 48 89 4d c8                 	movq	%rcx, -0x38(%rbp)
1000071ec: 48 89 df                    	movq	%rbx, %rdi
1000071ef: 48 f7 df                    	negq	%rdi
1000071f2: e8 1b 0e 00 00              	callq	0x100008012 <_sysconf+0x100008012>
1000071f7: 48 85 c0                    	testq	%rax, %rax
1000071fa: 74 72                       	je	0x10000726e <_parse_name+0x10e>
1000071fc: 49 89 c6                    	movq	%rax, %r14
1000071ff: 4d 85 ff                    	testq	%r15, %r15
100007202: 74 2c                       	je	0x100007230 <_parse_name+0xd0>
100007204: 31 c9                       	xorl	%ecx, %ecx
100007206: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)
100007210: 41 0f b7 54 4d 00           	movzwl	(%r13,%rcx,2), %edx
100007216: 8d 72 bf                    	leal	-0x41(%rdx), %esi
100007219: 8d 7a 20                    	leal	0x20(%rdx), %edi
10000721c: 66 83 fe 1a                 	cmpw	$0x1a, %si
100007220: 0f 43 fa                    	cmovael	%edx, %edi
100007223: 66 41 89 3c 4e              	movw	%di, (%r14,%rcx,2)
100007228: 48 ff c1                    	incq	%rcx
10000722b: 49 39 cf                    	cmpq	%rcx, %r15
10000722e: 75 e0                       	jne	0x100007210 <_parse_name+0xb0>
100007230: 49 83 ff 07                 	cmpq	$0x7, %r15
100007234: 72 42                       	jb	0x100007278 <_parse_name+0x118>
100007236: 48 8d 0d 85 16 00 00        	leaq	0x1685(%rip), %rcx      ## 0x1000088c2 <_parse_name.prefix>
10000723d: 31 c0                       	xorl	%eax, %eax
10000723f: 90                          	nop
100007240: 41 0f b7 14 46              	movzwl	(%r14,%rax,2), %edx
100007245: 66 3b 11                    	cmpw	(%rcx), %dx
100007248: 75 12                       	jne	0x10000725c <_parse_name+0xfc>
10000724a: 48 ff c0                    	incq	%rax
10000724d: 48 83 c1 02                 	addq	$0x2, %rcx
100007251: 48 83 f8 06                 	cmpq	$0x6, %rax
100007255: 75 e9                       	jne	0x100007240 <_parse_name+0xe0>
100007257: b8 06 00 00 00              	movl	$0x6, %eax
10000725c: 48 83 f8 06                 	cmpq	$0x6, %rax
100007260: 0f 94 c0                    	sete	%al
100007263: eb 15                       	jmp	0x10000727a <_parse_name+0x11a>
100007265: c7 01 01 00 00 00           	movl	$0x1, (%rcx)
10000726b: 31 c0                       	xorl	%eax, %eax
10000726d: c3                          	retq
10000726e: b8 08 00 00 00              	movl	$0x8, %eax
100007273: e9 7e 00 00 00              	jmp	0x1000072f6 <_parse_name+0x196>
100007278: 31 c0                       	xorl	%eax, %eax
10000727a: 31 d2                       	xorl	%edx, %edx
10000727c: 84 c0                       	testb	%al, %al
10000727e: b9 06 00 00 00              	movl	$0x6, %ecx
100007283: 48 0f 44 ca                 	cmoveq	%rdx, %rcx
100007287: 4c 39 f9                    	cmpq	%r15, %rcx
10000728a: 73 14                       	jae	0x1000072a0 <_parse_name+0x140>
10000728c: 48 89 ca                    	movq	%rcx, %rdx
10000728f: 90                          	nop
100007290: 66 41 83 3c 56 5c           	cmpw	$0x5c, (%r14,%rdx,2)
100007296: 74 51                       	je	0x1000072e9 <_parse_name+0x189>
100007298: 48 ff c2                    	incq	%rdx
10000729b: 4c 39 fa                    	cmpq	%r15, %rdx
10000729e: 72 f0                       	jb	0x100007290 <_parse_name+0x130>
1000072a0: 84 c0                       	testb	%al, %al
1000072a2: 74 1d                       	je	0x1000072c1 <_parse_name+0x161>
1000072a4: 8d 04 09                    	leal	(%rcx,%rcx), %eax
1000072a7: 4c 89 f6                    	movq	%r14, %rsi
1000072aa: 48 01 c6                    	addq	%rax, %rsi
1000072ad: 49 29 cf                    	subq	%rcx, %r15
1000072b0: 48 01 c3                    	addq	%rax, %rbx
1000072b3: 48 f7 db                    	negq	%rbx
1000072b6: 4c 89 f7                    	movq	%r14, %rdi
1000072b9: 48 89 da                    	movq	%rbx, %rdx
1000072bc: e8 57 0d 00 00              	callq	0x100008018 <_sysconf+0x100008018>
1000072c1: 48 8b 45 d0                 	movq	-0x30(%rbp), %rax
1000072c5: 4d 85 ff                    	testq	%r15, %r15
1000072c8: 74 09                       	je	0x1000072d3 <_parse_name+0x173>
1000072ca: 4c 89 30                    	movq	%r14, (%rax)
1000072cd: 4d 89 3c 24                 	movq	%r15, (%r12)
1000072d1: eb 12                       	jmp	0x1000072e5 <_parse_name+0x185>
1000072d3: 4c 89 f7                    	movq	%r14, %rdi
1000072d6: e8 2b 0d 00 00              	callq	0x100008006 <_sysconf+0x100008006>
1000072db: 48 8b 45 c8                 	movq	-0x38(%rbp), %rax
1000072df: c7 00 01 00 00 00           	movl	$0x1, (%rax)
1000072e5: 31 c0                       	xorl	%eax, %eax
1000072e7: eb 0d                       	jmp	0x1000072f6 <_parse_name+0x196>
1000072e9: 4c 89 f7                    	movq	%r14, %rdi
1000072ec: e8 15 0d 00 00              	callq	0x100008006 <_sysconf+0x100008006>
1000072f1: b8 57 00 00 00              	movl	$0x57, %eax
1000072f6: 48 83 c4 18                 	addq	$0x18, %rsp
1000072fa: 5b                          	popq	%rbx
1000072fb: 41 5c                       	popq	%r12
1000072fd: 41 5d                       	popq	%r13
1000072ff: 41 5e                       	popq	%r14
100007301: 41 5f                       	popq	%r15
100007303: 5d                          	popq	%rbp
100007304: c3                          	retq
100007305: 66 66 2e 0f 1f 84 00 00 00 00 00    	nopw	%cs:(%rax,%rax)

0000000100007310 <_new_handle>:
100007310: 55                          	pushq	%rbp
100007311: 48 89 e5                    	movq	%rsp, %rbp
100007314: 41 57                       	pushq	%r15
100007316: 41 56                       	pushq	%r14
100007318: 53                          	pushq	%rbx
100007319: 50                          	pushq	%rax
10000731a: 48 8b 9f 80 00 00 00        	movq	0x80(%rdi), %rbx
100007321: 48 83 fb ff                 	cmpq	$-0x1, %rbx
100007325: 74 38                       	je	0x10000735f <_new_handle+0x4f>
100007327: 49 89 f6                    	movq	%rsi, %r14
10000732a: 49 89 ff                    	movq	%rdi, %r15
10000732d: bf 18 00 00 00              	movl	$0x18, %edi
100007332: e8 db 0c 00 00              	callq	0x100008012 <_sysconf+0x100008012>
100007337: 48 85 c0                    	testq	%rax, %rax
10000733a: 74 23                       	je	0x10000735f <_new_handle+0x4f>
10000733c: 48 ff c3                    	incq	%rbx
10000733f: 49 89 9f 80 00 00 00        	movq	%rbx, 0x80(%r15)
100007346: 48 89 58 08                 	movq	%rbx, 0x8(%rax)
10000734a: 4c 89 70 10                 	movq	%r14, 0x10(%rax)
10000734e: 49 8b 4f 68                 	movq	0x68(%r15), %rcx
100007352: 48 89 08                    	movq	%rcx, (%rax)
100007355: 49 89 47 68                 	movq	%rax, 0x68(%r15)
100007359: 49 ff 46 10                 	incq	0x10(%r14)
10000735d: eb 02                       	jmp	0x100007361 <_new_handle+0x51>
10000735f: 31 db                       	xorl	%ebx, %ebx
100007361: 48 89 d8                    	movq	%rbx, %rax
100007364: 48 83 c4 08                 	addq	$0x8, %rsp
100007368: 5b                          	popq	%rbx
100007369: 41 5e                       	popq	%r14
10000736b: 41 5f                       	popq	%r15
10000736d: 5d                          	popq	%rbp
10000736e: c3                          	retq
10000736f: 90                          	nop

0000000100007370 <_adopt>:
100007370: 55                          	pushq	%rbp
100007371: 48 89 e5                    	movq	%rsp, %rbp
100007374: 41 57                       	pushq	%r15
100007376: 41 56                       	pushq	%r14
100007378: 53                          	pushq	%rbx
100007379: 50                          	pushq	%rax
10000737a: 48 89 f3                    	movq	%rsi, %rbx
10000737d: 49 89 ff                    	movq	%rdi, %r15
100007380: 48 8b bf 88 00 00 00        	movq	0x88(%rdi), %rdi
100007387: e8 d4 0c 00 00              	callq	0x100008060 <_sysconf+0x100008060>
10000738c: 49 89 c6                    	movq	%rax, %r14
10000738f: 48 85 c0                    	testq	%rax, %rax
100007392: 74 29                       	je	0x1000073bd <_adopt+0x4d>
100007394: bf 10 00 00 00              	movl	$0x10, %edi
100007399: e8 74 0c 00 00              	callq	0x100008012 <_sysconf+0x100008012>
10000739e: 48 85 c0                    	testq	%rax, %rax
1000073a1: 74 4e                       	je	0x1000073f1 <_adopt+0x81>
1000073a3: 48 8b 4b 08                 	movq	0x8(%rbx), %rcx
1000073a7: 48 89 48 08                 	movq	%rcx, 0x8(%rax)
1000073ab: 49 8b 4e 08                 	movq	0x8(%r14), %rcx
1000073af: 48 89 08                    	movq	%rcx, (%rax)
1000073b2: 49 89 46 08                 	movq	%rax, 0x8(%r14)
1000073b6: b8 01 00 00 00              	movl	$0x1, %eax
1000073bb: eb 36                       	jmp	0x1000073f3 <_adopt+0x83>
1000073bd: bf 01 00 00 00              	movl	$0x1, %edi
1000073c2: be 10 00 00 00              	movl	$0x10, %esi
1000073c7: e8 1c 0c 00 00              	callq	0x100007fe8 <_sysconf+0x100007fe8>
1000073cc: 49 89 c6                    	movq	%rax, %r14
1000073cf: 31 c0                       	xorl	%eax, %eax
1000073d1: 4d 85 f6                    	testq	%r14, %r14
1000073d4: 74 1d                       	je	0x1000073f3 <_adopt+0x83>
1000073d6: 49 8b bf 88 00 00 00        	movq	0x88(%r15), %rdi
1000073dd: 4c 89 f6                    	movq	%r14, %rsi
1000073e0: e8 b1 0c 00 00              	callq	0x100008096 <_sysconf+0x100008096>
1000073e5: 85 c0                       	testl	%eax, %eax
1000073e7: 74 15                       	je	0x1000073fe <_adopt+0x8e>
1000073e9: 4c 89 f7                    	movq	%r14, %rdi
1000073ec: e8 15 0c 00 00              	callq	0x100008006 <_sysconf+0x100008006>
1000073f1: 31 c0                       	xorl	%eax, %eax
1000073f3: 48 83 c4 08                 	addq	$0x8, %rsp
1000073f7: 5b                          	popq	%rbx
1000073f8: 41 5e                       	popq	%r14
1000073fa: 41 5f                       	popq	%r15
1000073fc: 5d                          	popq	%rbp
1000073fd: c3                          	retq
1000073fe: 4d 89 3e                    	movq	%r15, (%r14)
100007401: eb 91                       	jmp	0x100007394 <_adopt+0x24>
100007403: 66 66 66 66 2e 0f 1f 84 00 00 00 00 00      	nopw	%cs:(%rax,%rax)

0000000100007410 <_mb_win_mutex_open_named>:
100007410: 55                          	pushq	%rbp
100007411: 48 89 e5                    	movq	%rsp, %rbp
100007414: 41 57                       	pushq	%r15
100007416: 41 56                       	pushq	%r14
100007418: 41 55                       	pushq	%r13
10000741a: 41 54                       	pushq	%r12
10000741c: 53                          	pushq	%rbx
10000741d: 48 83 ec 18                 	subq	$0x18, %rsp
100007421: 48 85 ff                    	testq	%rdi, %rdi
100007424: 0f 84 87 01 00 00           	je	0x1000075b1 <_mb_win_mutex_open_named+0x1a1>
10000742a: 4c 8b 37                    	movq	(%rdi), %r14
10000742d: 4d 85 f6                    	testq	%r14, %r14
100007430: 0f 84 7b 01 00 00           	je	0x1000075b1 <_mb_win_mutex_open_named+0x1a1>
100007436: 49 89 f7                    	movq	%rsi, %r15
100007439: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
10000743d: e8 de ac ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007442: 4c 89 f7                    	movq	%r14, %rdi
100007445: e8 3a 0c 00 00              	callq	0x100008084 <_sysconf+0x100008084>
10000744a: 85 c0                       	testl	%eax, %eax
10000744c: 0f 85 5f 01 00 00           	jne	0x1000075b1 <_mb_win_mutex_open_named+0x1a1>
100007452: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007456: e8 c5 ac ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
10000745b: 48 89 c3                    	movq	%rax, %rbx
10000745e: 44 8b 20                    	movl	(%rax), %r12d
100007461: 48 c7 45 c0 00 00 00 00     	movq	$0x0, -0x40(%rbp)
100007469: 48 c7 45 c8 00 00 00 00     	movq	$0x0, -0x38(%rbp)
100007471: c7 45 d4 00 00 00 00        	movl	$0x0, -0x2c(%rbp)
100007478: 48 8d 75 c0                 	leaq	-0x40(%rbp), %rsi
10000747c: 48 8d 55 c8                 	leaq	-0x38(%rbp), %rdx
100007480: 48 8d 4d d4                 	leaq	-0x2c(%rbp), %rcx
100007484: 4c 89 ff                    	movq	%r15, %rdi
100007487: e8 d4 fc ff ff              	callq	0x100007160 <_parse_name>
10000748c: 41 89 c7                    	movl	%eax, %r15d
10000748f: 48 8b 7d c0                 	movq	-0x40(%rbp), %rdi
100007493: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
100007496: 44 09 f8                    	orl	%r15d, %eax
100007499: 74 27                       	je	0x1000074c2 <_mb_win_mutex_open_named+0xb2>
10000749b: e8 66 0b 00 00              	callq	0x100008006 <_sysconf+0x100008006>
1000074a0: 4c 89 f7                    	movq	%r14, %rdi
1000074a3: e8 e2 0b 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000074a8: 85 c0                       	testl	%eax, %eax
1000074aa: 0f 85 01 01 00 00           	jne	0x1000075b1 <_mb_win_mutex_open_named+0x1a1>
1000074b0: 45 85 ff                    	testl	%r15d, %r15d
1000074b3: 41 bc 57 00 00 00           	movl	$0x57, %r12d
1000074b9: 45 0f 45 e7                 	cmovnel	%r15d, %r12d
1000074bd: e9 d7 00 00 00              	jmp	0x100007599 <_mb_win_mutex_open_named+0x189>
1000074c2: 49 8b 46 60                 	movq	0x60(%r14), %rax
1000074c6: 48 85 c0                    	testq	%rax, %rax
1000074c9: 74 4e                       	je	0x100007519 <_mb_win_mutex_open_named+0x109>
1000074cb: 48 8b 4d c8                 	movq	-0x38(%rbp), %rcx
1000074cf: eb 13                       	jmp	0x1000074e4 <_mb_win_mutex_open_named+0xd4>
1000074d1: 31 d2                       	xorl	%edx, %edx
1000074d3: 48 39 ca                    	cmpq	%rcx, %rdx
1000074d6: 4c 0f 44 e8                 	cmoveq	%rax, %r13
1000074da: 74 40                       	je	0x10000751c <_mb_win_mutex_open_named+0x10c>
1000074dc: 48 8b 00                    	movq	(%rax), %rax
1000074df: 48 85 c0                    	testq	%rax, %rax
1000074e2: 74 35                       	je	0x100007519 <_mb_win_mutex_open_named+0x109>
1000074e4: 83 78 30 00                 	cmpl	$0x0, 0x30(%rax)
1000074e8: 74 f2                       	je	0x1000074dc <_mb_win_mutex_open_named+0xcc>
1000074ea: 48 39 48 28                 	cmpq	%rcx, 0x28(%rax)
1000074ee: 75 ec                       	jne	0x1000074dc <_mb_win_mutex_open_named+0xcc>
1000074f0: 48 85 c9                    	testq	%rcx, %rcx
1000074f3: 74 dc                       	je	0x1000074d1 <_mb_win_mutex_open_named+0xc1>
1000074f5: 48 8b 70 20                 	movq	0x20(%rax), %rsi
1000074f9: 31 d2                       	xorl	%edx, %edx
1000074fb: 0f 1f 44 00 00              	nopl	(%rax,%rax)
100007500: 44 0f b7 04 56              	movzwl	(%rsi,%rdx,2), %r8d
100007505: 66 44 3b 04 57              	cmpw	(%rdi,%rdx,2), %r8w
10000750a: 75 c7                       	jne	0x1000074d3 <_mb_win_mutex_open_named+0xc3>
10000750c: 48 ff c2                    	incq	%rdx
10000750f: 48 39 d1                    	cmpq	%rdx, %rcx
100007512: 75 ec                       	jne	0x100007500 <_mb_win_mutex_open_named+0xf0>
100007514: 48 89 ca                    	movq	%rcx, %rdx
100007517: eb ba                       	jmp	0x1000074d3 <_mb_win_mutex_open_named+0xc3>
100007519: 45 31 ed                    	xorl	%r13d, %r13d
10000751c: e8 e5 0a 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100007521: 4d 85 ed                    	testq	%r13, %r13
100007524: 74 55                       	je	0x10000757b <_mb_win_mutex_open_named+0x16b>
100007526: 4d 8b be 80 00 00 00        	movq	0x80(%r14), %r15
10000752d: 49 83 ff ff                 	cmpq	$-0x1, %r15
100007531: 74 32                       	je	0x100007565 <_mb_win_mutex_open_named+0x155>
100007533: bf 18 00 00 00              	movl	$0x18, %edi
100007538: e8 d5 0a 00 00              	callq	0x100008012 <_sysconf+0x100008012>
10000753d: 48 85 c0                    	testq	%rax, %rax
100007540: 74 23                       	je	0x100007565 <_mb_win_mutex_open_named+0x155>
100007542: 49 ff c7                    	incq	%r15
100007545: 4d 89 be 80 00 00 00        	movq	%r15, 0x80(%r14)
10000754c: 4c 89 78 08                 	movq	%r15, 0x8(%rax)
100007550: 4c 89 68 10                 	movq	%r13, 0x10(%rax)
100007554: 49 8b 4e 68                 	movq	0x68(%r14), %rcx
100007558: 48 89 08                    	movq	%rcx, (%rax)
10000755b: 49 89 46 68                 	movq	%rax, 0x68(%r14)
10000755f: 49 ff 45 10                 	incq	0x10(%r13)
100007563: eb 03                       	jmp	0x100007568 <_mb_win_mutex_open_named+0x158>
100007565: 45 31 ff                    	xorl	%r15d, %r15d
100007568: 4c 89 f7                    	movq	%r14, %rdi
10000756b: e8 1a 0b 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007570: 4d 85 ff                    	testq	%r15, %r15
100007573: 74 1a                       	je	0x10000758f <_mb_win_mutex_open_named+0x17f>
100007575: 85 c0                       	testl	%eax, %eax
100007577: 74 23                       	je	0x10000759c <_mb_win_mutex_open_named+0x18c>
100007579: eb 36                       	jmp	0x1000075b1 <_mb_win_mutex_open_named+0x1a1>
10000757b: 4c 89 f7                    	movq	%r14, %rdi
10000757e: e8 07 0b 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007583: 85 c0                       	testl	%eax, %eax
100007585: 75 2a                       	jne	0x1000075b1 <_mb_win_mutex_open_named+0x1a1>
100007587: 41 bc 02 00 00 00           	movl	$0x2, %r12d
10000758d: eb 0a                       	jmp	0x100007599 <_mb_win_mutex_open_named+0x189>
10000758f: 85 c0                       	testl	%eax, %eax
100007591: 75 1e                       	jne	0x1000075b1 <_mb_win_mutex_open_named+0x1a1>
100007593: 41 bc 08 00 00 00           	movl	$0x8, %r12d
100007599: 45 31 ff                    	xorl	%r15d, %r15d
10000759c: 44 89 23                    	movl	%r12d, (%rbx)
10000759f: 4c 89 f8                    	movq	%r15, %rax
1000075a2: 48 83 c4 18                 	addq	$0x18, %rsp
1000075a6: 5b                          	popq	%rbx
1000075a7: 41 5c                       	popq	%r12
1000075a9: 41 5d                       	popq	%r13
1000075ab: 41 5e                       	popq	%r14
1000075ad: 41 5f                       	popq	%r15
1000075af: 5d                          	popq	%rbp
1000075b0: c3                          	retq
1000075b1: e8 26 0a 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
1000075b6: 66 2e 0f 1f 84 00 00 00 00 00       	nopw	%cs:(%rax,%rax)

00000001000075c0 <_mb_win_mutex_release>:
1000075c0: 55                          	pushq	%rbp
1000075c1: 48 89 e5                    	movq	%rsp, %rbp
1000075c4: 41 57                       	pushq	%r15
1000075c6: 41 56                       	pushq	%r14
1000075c8: 41 55                       	pushq	%r13
1000075ca: 41 54                       	pushq	%r12
1000075cc: 53                          	pushq	%rbx
1000075cd: 50                          	pushq	%rax
1000075ce: 48 85 ff                    	testq	%rdi, %rdi
1000075d1: 0f 84 1f 01 00 00           	je	0x1000076f6 <_mb_win_mutex_release+0x136>
1000075d7: 4c 8b 37                    	movq	(%rdi), %r14
1000075da: 4d 85 f6                    	testq	%r14, %r14
1000075dd: 0f 84 13 01 00 00           	je	0x1000076f6 <_mb_win_mutex_release+0x136>
1000075e3: 49 89 f7                    	movq	%rsi, %r15
1000075e6: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
1000075ea: e8 31 ab ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
1000075ef: 4c 89 f7                    	movq	%r14, %rdi
1000075f2: e8 8d 0a 00 00              	callq	0x100008084 <_sysconf+0x100008084>
1000075f7: 85 c0                       	testl	%eax, %eax
1000075f9: 0f 85 f7 00 00 00           	jne	0x1000076f6 <_mb_win_mutex_release+0x136>
1000075ff: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007603: e8 18 ab ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007608: 48 89 c3                    	movq	%rax, %rbx
10000760b: 44 8b 28                    	movl	(%rax), %r13d
10000760e: 49 8d 46 68                 	leaq	0x68(%r14), %rax
100007612: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100007620: 48 8b 00                    	movq	(%rax), %rax
100007623: 48 85 c0                    	testq	%rax, %rax
100007626: 0f 84 8e 00 00 00           	je	0x1000076ba <_mb_win_mutex_release+0xfa>
10000762c: 4c 39 78 08                 	cmpq	%r15, 0x8(%rax)
100007630: 75 ee                       	jne	0x100007620 <_mb_win_mutex_release+0x60>
100007632: 4c 8b 78 10                 	movq	0x10(%rax), %r15
100007636: 41 83 7f 34 00              	cmpl	$0x0, 0x34(%r15)
10000763b: 0f 84 8f 00 00 00           	je	0x1000076d0 <_mb_win_mutex_release+0x110>
100007641: 4d 8b 67 40                 	movq	0x40(%r15), %r12
100007645: e8 46 0a 00 00              	callq	0x100008090 <_sysconf+0x100008090>
10000764a: 4c 89 e7                    	movq	%r12, %rdi
10000764d: 48 89 c6                    	movq	%rax, %rsi
100007650: e8 05 0a 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100007655: 85 c0                       	testl	%eax, %eax
100007657: 74 77                       	je	0x1000076d0 <_mb_win_mutex_release+0x110>
100007659: 49 8b be 88 00 00 00        	movq	0x88(%r14), %rdi
100007660: e8 fb 09 00 00              	callq	0x100008060 <_sysconf+0x100008060>
100007665: 48 85 c0                    	testq	%rax, %rax
100007668: 74 29                       	je	0x100007693 <_mb_win_mutex_release+0xd3>
10000766a: 48 83 c0 08                 	addq	$0x8, %rax
10000766e: 66 90                       	nop
100007670: 48 89 c1                    	movq	%rax, %rcx
100007673: 48 8b 00                    	movq	(%rax), %rax
100007676: 48 85 c0                    	testq	%rax, %rax
100007679: 74 18                       	je	0x100007693 <_mb_win_mutex_release+0xd3>
10000767b: 48 8b 50 08                 	movq	0x8(%rax), %rdx
10000767f: 49 3b 57 08                 	cmpq	0x8(%r15), %rdx
100007683: 75 eb                       	jne	0x100007670 <_mb_win_mutex_release+0xb0>
100007685: 48 8b 10                    	movq	(%rax), %rdx
100007688: 48 89 11                    	movq	%rdx, (%rcx)
10000768b: 48 89 c7                    	movq	%rax, %rdi
10000768e: e8 73 09 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100007693: 41 c7 47 34 00 00 00 00     	movl	$0x0, 0x34(%r15)
10000769b: 4c 89 ff                    	movq	%r15, %rdi
10000769e: 31 f6                       	xorl	%esi, %esi
1000076a0: e8 5b 00 00 00              	callq	0x100007700 <_grant_first>
1000076a5: 4c 89 f7                    	movq	%r14, %rdi
1000076a8: e8 dd 09 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000076ad: 89 c1                       	movl	%eax, %ecx
1000076af: b8 01 00 00 00              	movl	$0x1, %eax
1000076b4: 85 c9                       	testl	%ecx, %ecx
1000076b6: 74 2c                       	je	0x1000076e4 <_mb_win_mutex_release+0x124>
1000076b8: eb 3c                       	jmp	0x1000076f6 <_mb_win_mutex_release+0x136>
1000076ba: 4c 89 f7                    	movq	%r14, %rdi
1000076bd: e8 c8 09 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000076c2: 85 c0                       	testl	%eax, %eax
1000076c4: 75 30                       	jne	0x1000076f6 <_mb_win_mutex_release+0x136>
1000076c6: 31 c0                       	xorl	%eax, %eax
1000076c8: 41 bd 06 00 00 00           	movl	$0x6, %r13d
1000076ce: eb 14                       	jmp	0x1000076e4 <_mb_win_mutex_release+0x124>
1000076d0: 4c 89 f7                    	movq	%r14, %rdi
1000076d3: e8 b2 09 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000076d8: 85 c0                       	testl	%eax, %eax
1000076da: 75 1a                       	jne	0x1000076f6 <_mb_win_mutex_release+0x136>
1000076dc: 31 c0                       	xorl	%eax, %eax
1000076de: 41 bd 20 01 00 00           	movl	$0x120, %r13d           ## imm = 0x120
1000076e4: 44 89 2b                    	movl	%r13d, (%rbx)
1000076e7: 48 83 c4 08                 	addq	$0x8, %rsp
1000076eb: 5b                          	popq	%rbx
1000076ec: 41 5c                       	popq	%r12
1000076ee: 41 5d                       	popq	%r13
1000076f0: 41 5e                       	popq	%r14
1000076f2: 41 5f                       	popq	%r15
1000076f4: 5d                          	popq	%rbp
1000076f5: c3                          	retq
1000076f6: e8 e1 08 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
1000076fb: 0f 1f 44 00 00              	nopl	(%rax,%rax)

0000000100007700 <_grant_first>:
100007700: 55                          	pushq	%rbp
100007701: 48 89 e5                    	movq	%rsp, %rbp
100007704: 41 57                       	pushq	%r15
100007706: 41 56                       	pushq	%r14
100007708: 41 55                       	pushq	%r13
10000770a: 41 54                       	pushq	%r12
10000770c: 53                          	pushq	%rbx
10000770d: 50                          	pushq	%rax
10000770e: 4c 8b 67 48                 	movq	0x48(%rdi), %r12
100007712: 4d 85 e4                    	testq	%r12, %r12
100007715: 0f 84 ab 00 00 00           	je	0x1000077c6 <_grant_first+0xc6>
10000771b: 48 89 fb                    	movq	%rdi, %rbx
10000771e: 89 75 d4                    	movl	%esi, -0x2c(%rbp)
100007721: 4c 8d 77 48                 	leaq	0x48(%rdi), %r14
100007725: 4d 8b 7c 24 08              	movq	0x8(%r12), %r15
10000772a: 4c 89 ff                    	movq	%r15, %rdi
10000772d: 4c 89 fe                    	movq	%r15, %rsi
100007730: e8 25 09 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100007735: 85 c0                       	testl	%eax, %eax
100007737: 74 05                       	je	0x10000773e <_grant_first+0x3e>
100007739: 45 31 ed                    	xorl	%r13d, %r13d
10000773c: eb 37                       	jmp	0x100007775 <_grant_first+0x75>
10000773e: 4d 89 f5                    	movq	%r14, %r13
100007741: 66 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00	nopw	%cs:(%rax,%rax)
100007750: 4d 8b 6d 00                 	movq	(%r13), %r13
100007754: 49 8b 45 00                 	movq	(%r13), %rax
100007758: 48 85 c0                    	testq	%rax, %rax
10000775b: 74 15                       	je	0x100007772 <_grant_first+0x72>
10000775d: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100007761: 4c 89 fe                    	movq	%r15, %rsi
100007764: e8 f1 08 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100007769: 4d 89 ee                    	movq	%r13, %r14
10000776c: 85 c0                       	testl	%eax, %eax
10000776e: 74 e0                       	je	0x100007750 <_grant_first+0x50>
100007770: eb 03                       	jmp	0x100007775 <_grant_first+0x75>
100007772: 4d 89 ee                    	movq	%r13, %r14
100007775: 49 8b 06                    	movq	(%r14), %rax
100007778: 48 85 c0                    	testq	%rax, %rax
10000777b: 74 58                       	je	0x1000077d5 <_grant_first+0xd5>
10000777d: 48 8b 08                    	movq	(%rax), %rcx
100007780: 49 89 0e                    	movq	%rcx, (%r14)
100007783: 48 39 43 50                 	cmpq	%rax, 0x50(%rbx)
100007787: 8b 4d d4                    	movl	-0x2c(%rbp), %ecx
10000778a: 75 04                       	jne	0x100007790 <_grant_first+0x90>
10000778c: 4c 89 6b 50                 	movq	%r13, 0x50(%rbx)
100007790: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
100007797: 48 c7 43 34 01 00 00 00     	movq	$0x1, 0x34(%rbx)
10000779f: 49 8b 44 24 08              	movq	0x8(%r12), %rax
1000077a4: 48 89 43 40                 	movq	%rax, 0x40(%rbx)
1000077a8: 41 c7 44 24 10 01 00 00 00  	movl	$0x1, 0x10(%r12)
1000077b1: 41 89 4c 24 14              	movl	%ecx, 0x14(%r12)
1000077b6: 48 83 c3 58                 	addq	$0x58, %rbx
1000077ba: 48 89 df                    	movq	%rbx, %rdi
1000077bd: e8 62 08 00 00              	callq	0x100008024 <_sysconf+0x100008024>
1000077c2: 85 c0                       	testl	%eax, %eax
1000077c4: 75 0f                       	jne	0x1000077d5 <_grant_first+0xd5>
1000077c6: 48 83 c4 08                 	addq	$0x8, %rsp
1000077ca: 5b                          	popq	%rbx
1000077cb: 41 5c                       	popq	%r12
1000077cd: 41 5d                       	popq	%r13
1000077cf: 41 5e                       	popq	%r14
1000077d1: 41 5f                       	popq	%r15
1000077d3: 5d                          	popq	%rbp
1000077d4: c3                          	retq
1000077d5: e8 02 08 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
1000077da: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

00000001000077e0 <_mb_win_mutex_wait>:
1000077e0: 55                          	pushq	%rbp
1000077e1: 48 89 e5                    	movq	%rsp, %rbp
1000077e4: 31 c9                       	xorl	%ecx, %ecx
1000077e6: 5d                          	popq	%rbp
1000077e7: e9 04 00 00 00              	jmp	0x1000077f0 <_mb_win_mutex_wait_ex>
1000077ec: 0f 1f 40 00                 	nopl	(%rax)

00000001000077f0 <_mb_win_mutex_wait_ex>:
1000077f0: 55                          	pushq	%rbp
1000077f1: 48 89 e5                    	movq	%rsp, %rbp
1000077f4: 41 57                       	pushq	%r15
1000077f6: 41 56                       	pushq	%r14
1000077f8: 41 55                       	pushq	%r13
1000077fa: 41 54                       	pushq	%r12
1000077fc: 53                          	pushq	%rbx
1000077fd: 48 83 ec 68                 	subq	$0x68, %rsp
100007801: 48 85 ff                    	testq	%rdi, %rdi
100007804: 0f 84 5b 06 00 00           	je	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
10000780a: 4c 8b 37                    	movq	(%rdi), %r14
10000780d: 4d 85 f6                    	testq	%r14, %r14
100007810: 0f 84 4f 06 00 00           	je	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007816: 41 89 cf                    	movl	%ecx, %r15d
100007819: 41 89 d4                    	movl	%edx, %r12d
10000781c: 49 89 f5                    	movq	%rsi, %r13
10000781f: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007823: e8 f8 a8 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007828: 4c 89 f7                    	movq	%r14, %rdi
10000782b: e8 54 08 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100007830: 85 c0                       	testl	%eax, %eax
100007832: 0f 85 2d 06 00 00           	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007838: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
10000783c: e8 df a8 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007841: 48 89 c3                    	movq	%rax, %rbx
100007844: 8b 00                       	movl	(%rax), %eax
100007846: 89 45 d4                    	movl	%eax, -0x2c(%rbp)
100007849: 49 8d 46 68                 	leaq	0x68(%r14), %rax
10000784d: 0f 1f 00                    	nopl	(%rax)
100007850: 48 8b 00                    	movq	(%rax), %rax
100007853: 48 85 c0                    	testq	%rax, %rax
100007856: 74 23                       	je	0x10000787b <_mb_win_mutex_wait_ex+0x8b>
100007858: 4c 39 68 08                 	cmpq	%r13, 0x8(%rax)
10000785c: 75 f2                       	jne	0x100007850 <_mb_win_mutex_wait_ex+0x60>
10000785e: 45 85 ff                    	testl	%r15d, %r15d
100007861: 74 46                       	je	0x1000078a9 <_mb_win_mutex_wait_ex+0xb9>
100007863: 4c 89 f7                    	movq	%r14, %rdi
100007866: e8 1f 08 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
10000786b: 85 c0                       	testl	%eax, %eax
10000786d: 0f 85 f2 05 00 00           	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007873: c7 03 32 00 00 00           	movl	$0x32, (%rbx)
100007879: eb 16                       	jmp	0x100007891 <_mb_win_mutex_wait_ex+0xa1>
10000787b: 4c 89 f7                    	movq	%r14, %rdi
10000787e: e8 07 08 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007883: 85 c0                       	testl	%eax, %eax
100007885: 0f 85 da 05 00 00           	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
10000788b: c7 03 06 00 00 00           	movl	$0x6, (%rbx)
100007891: 41 bf ff ff ff ff           	movl	$0xffffffff, %r15d      ## imm = 0xFFFFFFFF
100007897: 44 89 f8                    	movl	%r15d, %eax
10000789a: 48 83 c4 68                 	addq	$0x68, %rsp
10000789e: 5b                          	popq	%rbx
10000789f: 41 5c                       	popq	%r12
1000078a1: 41 5d                       	popq	%r13
1000078a3: 41 5e                       	popq	%r14
1000078a5: 41 5f                       	popq	%r15
1000078a7: 5d                          	popq	%rbp
1000078a8: c3                          	retq
1000078a9: 4c 8b 68 10                 	movq	0x10(%rax), %r13
1000078ad: 41 83 7d 34 00              	cmpl	$0x0, 0x34(%r13)
1000078b2: 74 5a                       	je	0x10000790e <_mb_win_mutex_wait_ex+0x11e>
1000078b4: 4d 8b 7d 40                 	movq	0x40(%r13), %r15
1000078b8: e8 d3 07 00 00              	callq	0x100008090 <_sysconf+0x100008090>
1000078bd: 4c 89 ff                    	movq	%r15, %rdi
1000078c0: 48 89 c6                    	movq	%rax, %rsi
1000078c3: e8 92 07 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
1000078c8: 85 c0                       	testl	%eax, %eax
1000078ca: 74 42                       	je	0x10000790e <_mb_win_mutex_wait_ex+0x11e>
1000078cc: 41 83 fc ff                 	cmpl	$-0x1, %r12d
1000078d0: 0f 84 ca 00 00 00           	je	0x1000079a0 <_mb_win_mutex_wait_ex+0x1b0>
1000078d6: 48 8d 75 b8                 	leaq	-0x48(%rbp), %rsi
1000078da: bf 06 00 00 00              	movl	$0x6, %edi
1000078df: e8 0a 07 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
1000078e4: 85 c0                       	testl	%eax, %eax
1000078e6: 75 0c                       	jne	0x1000078f4 <_mb_win_mutex_wait_ex+0x104>
1000078e8: 48 69 4d b8 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x48(%rbp), %rcx ## imm = 0x3B9ACA00
1000078f0: 48 03 4d c0                 	addq	-0x40(%rbp), %rcx
1000078f4: 85 c0                       	testl	%eax, %eax
1000078f6: 0f 84 cc 00 00 00           	je	0x1000079c8 <_mb_win_mutex_wait_ex+0x1d8>
1000078fc: 45 31 e4                    	xorl	%r12d, %r12d
1000078ff: 31 c0                       	xorl	%eax, %eax
100007901: 84 c0                       	testb	%al, %al
100007903: 0f 85 e7 00 00 00           	jne	0x1000079f0 <_mb_win_mutex_wait_ex+0x200>
100007909: e9 fc 04 00 00              	jmp	0x100007e0a <_mb_win_mutex_wait_ex+0x61a>
10000790e: 41 83 7d 34 00              	cmpl	$0x0, 0x34(%r13)
100007913: 74 47                       	je	0x10000795c <_mb_win_mutex_wait_ex+0x16c>
100007915: 41 83 fc ff                 	cmpl	$-0x1, %r12d
100007919: 0f 84 9c 00 00 00           	je	0x1000079bb <_mb_win_mutex_wait_ex+0x1cb>
10000791f: 48 8d 75 b8                 	leaq	-0x48(%rbp), %rsi
100007923: bf 06 00 00 00              	movl	$0x6, %edi
100007928: e8 c1 06 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
10000792d: 85 c0                       	testl	%eax, %eax
10000792f: 75 0c                       	jne	0x10000793d <_mb_win_mutex_wait_ex+0x14d>
100007931: 48 69 4d b8 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x48(%rbp), %rcx ## imm = 0x3B9ACA00
100007939: 48 03 4d c0                 	addq	-0x40(%rbp), %rcx
10000793d: 85 c0                       	testl	%eax, %eax
10000793f: 0f 84 d4 01 00 00           	je	0x100007b19 <_mb_win_mutex_wait_ex+0x329>
100007945: 48 c7 45 a0 00 00 00 00     	movq	$0x0, -0x60(%rbp)
10000794d: 31 c0                       	xorl	%eax, %eax
10000794f: 84 c0                       	testb	%al, %al
100007951: 0f 84 b3 04 00 00           	je	0x100007e0a <_mb_win_mutex_wait_ex+0x61a>
100007957: e9 e8 01 00 00              	jmp	0x100007b44 <_mb_win_mutex_wait_ex+0x354>
10000795c: 45 31 ff                    	xorl	%r15d, %r15d
10000795f: 41 83 7d 38 00              	cmpl	$0x0, 0x38(%r13)
100007964: 41 0f 95 c7                 	setne	%r15b
100007968: 41 c1 e7 07                 	shll	$0x7, %r15d
10000796c: 49 c7 45 34 01 00 00 00     	movq	$0x1, 0x34(%r13)
100007974: e8 17 07 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100007979: 49 89 45 40                 	movq	%rax, 0x40(%r13)
10000797d: 4c 89 f7                    	movq	%r14, %rdi
100007980: 4c 89 ee                    	movq	%r13, %rsi
100007983: e8 e8 f9 ff ff              	callq	0x100007370 <_adopt>
100007988: 85 c0                       	testl	%eax, %eax
10000798a: 0f 85 69 04 00 00           	jne	0x100007df9 <_mb_win_mutex_wait_ex+0x609>
100007990: 41 c7 45 34 00 00 00 00     	movl	$0x0, 0x34(%r13)
100007998: 4c 89 ef                    	movq	%r13, %rdi
10000799b: e9 9f 04 00 00              	jmp	0x100007e3f <_mb_win_mutex_wait_ex+0x64f>
1000079a0: 4c 89 f7                    	movq	%r14, %rdi
1000079a3: e8 e2 06 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
1000079a8: 85 c0                       	testl	%eax, %eax
1000079aa: 0f 85 b5 04 00 00           	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
1000079b0: c7 03 03 03 00 00           	movl	$0x303, (%rbx)          ## imm = 0x303
1000079b6: e9 d6 fe ff ff              	jmp	0x100007891 <_mb_win_mutex_wait_ex+0xa1>
1000079bb: 48 c7 45 a0 00 00 00 00     	movq	$0x0, -0x60(%rbp)
1000079c3: e9 7c 01 00 00              	jmp	0x100007b44 <_mb_win_mutex_wait_ex+0x354>
1000079c8: 44 89 e0                    	movl	%r12d, %eax
1000079cb: 48 69 c0 40 42 0f 00        	imulq	$0xf4240, %rax, %rax    ## imm = 0xF4240
1000079d2: 48 89 ca                    	movq	%rcx, %rdx
1000079d5: 48 f7 d2                    	notq	%rdx
1000079d8: 48 01 c1                    	addq	%rax, %rcx
1000079db: 45 31 e4                    	xorl	%r12d, %r12d
1000079de: 48 39 d0                    	cmpq	%rdx, %rax
1000079e1: 0f 96 c0                    	setbe	%al
1000079e4: 4c 0f 46 e1                 	cmovbeq	%rcx, %r12
1000079e8: 84 c0                       	testb	%al, %al
1000079ea: 0f 84 1a 04 00 00           	je	0x100007e0a <_mb_win_mutex_wait_ex+0x61a>
1000079f0: 49 83 c5 58                 	addq	$0x58, %r13
1000079f4: 4c 89 6d b0                 	movq	%r13, -0x50(%rbp)
1000079f8: 4c 8d 7d b8                 	leaq	-0x48(%rbp), %r15
1000079fc: eb 0a                       	jmp	0x100007a08 <_mb_win_mutex_wait_ex+0x218>
1000079fe: 66 90                       	nop
100007a00: 85 c9                       	testl	%ecx, %ecx
100007a02: 0f 85 e8 00 00 00           	jne	0x100007af0 <_mb_win_mutex_wait_ex+0x300>
100007a08: bf 06 00 00 00              	movl	$0x6, %edi
100007a0d: 4c 89 fe                    	movq	%r15, %rsi
100007a10: e8 d9 05 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
100007a15: 85 c0                       	testl	%eax, %eax
100007a17: 74 0a                       	je	0x100007a23 <_mb_win_mutex_wait_ex+0x233>
100007a19: 85 c0                       	testl	%eax, %eax
100007a1b: 0f 85 af 00 00 00           	jne	0x100007ad0 <_mb_win_mutex_wait_ex+0x2e0>
100007a21: eb 14                       	jmp	0x100007a37 <_mb_win_mutex_wait_ex+0x247>
100007a23: 4c 69 6d b8 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x48(%rbp), %r13 ## imm = 0x3B9ACA00
100007a2b: 4c 03 6d c0                 	addq	-0x40(%rbp), %r13
100007a2f: 85 c0                       	testl	%eax, %eax
100007a31: 0f 85 99 00 00 00           	jne	0x100007ad0 <_mb_win_mutex_wait_ex+0x2e0>
100007a37: b9 02 00 00 00              	movl	$0x2, %ecx
100007a3c: 4d 39 e5                    	cmpq	%r12, %r13
100007a3f: 73 bf                       	jae	0x100007a00 <_mb_win_mutex_wait_ex+0x210>
100007a41: bf 06 00 00 00              	movl	$0x6, %edi
100007a46: 4c 89 fe                    	movq	%r15, %rsi
100007a49: e8 a0 05 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
100007a4e: 89 c1                       	movl	%eax, %ecx
100007a50: 85 c0                       	testl	%eax, %eax
100007a52: 75 0c                       	jne	0x100007a60 <_mb_win_mutex_wait_ex+0x270>
100007a54: 48 69 55 b8 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x48(%rbp), %rdx ## imm = 0x3B9ACA00
100007a5c: 48 03 55 c0                 	addq	-0x40(%rbp), %rdx
100007a60: b8 ff ff ff ff              	movl	$0xffffffff, %eax       ## imm = 0xFFFFFFFF
100007a65: 85 c9                       	testl	%ecx, %ecx
100007a67: 75 46                       	jne	0x100007aaf <_mb_win_mutex_wait_ex+0x2bf>
100007a69: b8 3c 00 00 00              	movl	$0x3c, %eax
100007a6e: 4c 89 e1                    	movq	%r12, %rcx
100007a71: 48 29 d1                    	subq	%rdx, %rcx
100007a74: 76 39                       	jbe	0x100007aaf <_mb_win_mutex_wait_ex+0x2bf>
100007a76: 48 89 c8                    	movq	%rcx, %rax
100007a79: 48 c1 e8 09                 	shrq	$0x9, %rax
100007a7d: 48 ba 53 5a 9b a0 2f b8 44 00       	movabsq	$0x44b82fa09b5a53, %rdx ## imm = 0x44B82FA09B5A53
100007a87: 48 f7 e2                    	mulq	%rdx
100007a8a: 48 c1 ea 0b                 	shrq	$0xb, %rdx
100007a8e: 48 89 55 b8                 	movq	%rdx, -0x48(%rbp)
100007a92: 48 69 c2 00 ca 9a 3b        	imulq	$0x3b9aca00, %rdx, %rax ## imm = 0x3B9ACA00
100007a99: 48 29 c1                    	subq	%rax, %rcx
100007a9c: 48 89 4d c0                 	movq	%rcx, -0x40(%rbp)
100007aa0: 48 8b 7d b0                 	movq	-0x50(%rbp), %rdi
100007aa4: 4c 89 f6                    	movq	%r14, %rsi
100007aa7: 4c 89 fa                    	movq	%r15, %rdx
100007aaa: e8 8d 05 00 00              	callq	0x10000803c <_sysconf+0x10000803c>
100007aaf: 31 c9                       	xorl	%ecx, %ecx
100007ab1: 85 c0                       	testl	%eax, %eax
100007ab3: 0f 84 47 ff ff ff           	je	0x100007a00 <_mb_win_mutex_wait_ex+0x210>
100007ab9: 83 f8 3c                    	cmpl	$0x3c, %eax
100007abc: 0f 84 3e ff ff ff           	je	0x100007a00 <_mb_win_mutex_wait_ex+0x210>
100007ac2: 66 66 66 66 66 2e 0f 1f 84 00 00 00 00 00   	nopw	%cs:(%rax,%rax)
100007ad0: 4c 89 f7                    	movq	%r14, %rdi
100007ad3: e8 b2 05 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007ad8: 85 c0                       	testl	%eax, %eax
100007ada: 0f 85 85 03 00 00           	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007ae0: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
100007ae6: b9 01 00 00 00              	movl	$0x1, %ecx
100007aeb: e9 10 ff ff ff              	jmp	0x100007a00 <_mb_win_mutex_wait_ex+0x210>
100007af0: 83 f9 02                    	cmpl	$0x2, %ecx
100007af3: 0f 85 98 fd ff ff           	jne	0x100007891 <_mb_win_mutex_wait_ex+0xa1>
100007af9: 4c 89 f7                    	movq	%r14, %rdi
100007afc: e8 89 05 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007b01: 85 c0                       	testl	%eax, %eax
100007b03: 0f 85 5c 03 00 00           	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007b09: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
100007b0c: 89 03                       	movl	%eax, (%rbx)
100007b0e: 41 bf 02 01 00 00           	movl	$0x102, %r15d           ## imm = 0x102
100007b14: e9 7e fd ff ff              	jmp	0x100007897 <_mb_win_mutex_wait_ex+0xa7>
100007b19: 44 89 e0                    	movl	%r12d, %eax
100007b1c: 48 69 c0 40 42 0f 00        	imulq	$0xf4240, %rax, %rax    ## imm = 0xF4240
100007b23: 48 89 ca                    	movq	%rcx, %rdx
100007b26: 48 f7 d2                    	notq	%rdx
100007b29: 48 01 c1                    	addq	%rax, %rcx
100007b2c: 31 f6                       	xorl	%esi, %esi
100007b2e: 48 39 d0                    	cmpq	%rdx, %rax
100007b31: 0f 96 c0                    	setbe	%al
100007b34: 48 0f 46 f1                 	cmovbeq	%rcx, %rsi
100007b38: 48 89 75 a0                 	movq	%rsi, -0x60(%rbp)
100007b3c: 84 c0                       	testb	%al, %al
100007b3e: 0f 84 c6 02 00 00           	je	0x100007e0a <_mb_win_mutex_wait_ex+0x61a>
100007b44: 48 c7 45 b8 00 00 00 00     	movq	$0x0, -0x48(%rbp)
100007b4c: e8 3f 05 00 00              	callq	0x100008090 <_sysconf+0x100008090>
100007b51: 48 89 45 c0                 	movq	%rax, -0x40(%rbp)
100007b55: 48 c7 45 c8 00 00 00 00     	movq	$0x0, -0x38(%rbp)
100007b5d: 49 8b 45 50                 	movq	0x50(%r13), %rax
100007b61: 49 8d 4d 48                 	leaq	0x48(%r13), %rcx
100007b65: 48 85 c0                    	testq	%rax, %rax
100007b68: 48 89 4d a8                 	movq	%rcx, -0x58(%rbp)
100007b6c: 48 0f 44 c1                 	cmoveq	%rcx, %rax
100007b70: 48 8d 4d b8                 	leaq	-0x48(%rbp), %rcx
100007b74: 48 89 08                    	movq	%rcx, (%rax)
100007b77: 49 89 4d 50                 	movq	%rcx, 0x50(%r13)
100007b7b: 49 ff 45 18                 	incq	0x18(%r13)
100007b7f: 4c 89 6d b0                 	movq	%r13, -0x50(%rbp)
100007b83: 49 83 c5 58                 	addq	$0x58, %r13
100007b87: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)
100007b90: 83 7d c8 00                 	cmpl	$0x0, -0x38(%rbp)
100007b94: 0f 85 43 02 00 00           	jne	0x100007ddd <_mb_win_mutex_wait_ex+0x5ed>
100007b9a: 41 83 fc ff                 	cmpl	$-0x1, %r12d
100007b9e: 75 14                       	jne	0x100007bb4 <_mb_win_mutex_wait_ex+0x3c4>
100007ba0: 4c 89 ef                    	movq	%r13, %rdi
100007ba3: 4c 89 f6                    	movq	%r14, %rsi
100007ba6: e8 97 04 00 00              	callq	0x100008042 <_sysconf+0x100008042>
100007bab: 85 c0                       	testl	%eax, %eax
100007bad: 74 e1                       	je	0x100007b90 <_mb_win_mutex_wait_ex+0x3a0>
100007baf: e9 b1 02 00 00              	jmp	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007bb4: bf 06 00 00 00              	movl	$0x6, %edi
100007bb9: 48 8d 75 90                 	leaq	-0x70(%rbp), %rsi
100007bbd: e8 2c 04 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
100007bc2: 85 c0                       	testl	%eax, %eax
100007bc4: 48 8b 8d 78 ff ff ff        	movq	-0x88(%rbp), %rcx
100007bcb: 75 0c                       	jne	0x100007bd9 <_mb_win_mutex_wait_ex+0x3e9>
100007bcd: 48 69 4d 90 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x70(%rbp), %rcx ## imm = 0x3B9ACA00
100007bd5: 48 03 4d 98                 	addq	-0x68(%rbp), %rcx
100007bd9: 85 c0                       	testl	%eax, %eax
100007bdb: 48 89 8d 78 ff ff ff        	movq	%rcx, -0x88(%rbp)
100007be2: 0f 85 f4 00 00 00           	jne	0x100007cdc <_mb_win_mutex_wait_ex+0x4ec>
100007be8: 48 3b 4d a0                 	cmpq	-0x60(%rbp), %rcx
100007bec: 0f 83 ea 00 00 00           	jae	0x100007cdc <_mb_win_mutex_wait_ex+0x4ec>
100007bf2: bf 06 00 00 00              	movl	$0x6, %edi
100007bf7: 48 8d 75 90                 	leaq	-0x70(%rbp), %rsi
100007bfb: e8 ee 03 00 00              	callq	0x100007fee <_sysconf+0x100007fee>
100007c00: 89 c1                       	movl	%eax, %ecx
100007c02: 85 c0                       	testl	%eax, %eax
100007c04: 75 0c                       	jne	0x100007c12 <_mb_win_mutex_wait_ex+0x422>
100007c06: 48 69 55 90 00 ca 9a 3b     	imulq	$0x3b9aca00, -0x70(%rbp), %rdx ## imm = 0x3B9ACA00
100007c0e: 48 03 55 98                 	addq	-0x68(%rbp), %rdx
100007c12: b8 ff ff ff ff              	movl	$0xffffffff, %eax       ## imm = 0xFFFFFFFF
100007c17: 85 c9                       	testl	%ecx, %ecx
100007c19: 75 47                       	jne	0x100007c62 <_mb_win_mutex_wait_ex+0x472>
100007c1b: b8 3c 00 00 00              	movl	$0x3c, %eax
100007c20: 48 8b 4d a0                 	movq	-0x60(%rbp), %rcx
100007c24: 48 29 d1                    	subq	%rdx, %rcx
100007c27: 76 39                       	jbe	0x100007c62 <_mb_win_mutex_wait_ex+0x472>
100007c29: 48 89 c8                    	movq	%rcx, %rax
100007c2c: 48 c1 e8 09                 	shrq	$0x9, %rax
100007c30: 48 ba 53 5a 9b a0 2f b8 44 00       	movabsq	$0x44b82fa09b5a53, %rdx ## imm = 0x44B82FA09B5A53
100007c3a: 48 f7 e2                    	mulq	%rdx
100007c3d: 48 c1 ea 0b                 	shrq	$0xb, %rdx
100007c41: 48 89 55 90                 	movq	%rdx, -0x70(%rbp)
100007c45: 48 69 c2 00 ca 9a 3b        	imulq	$0x3b9aca00, %rdx, %rax ## imm = 0x3B9ACA00
100007c4c: 48 29 c1                    	subq	%rax, %rcx
100007c4f: 48 89 4d 98                 	movq	%rcx, -0x68(%rbp)
100007c53: 4c 89 ef                    	movq	%r13, %rdi
100007c56: 4c 89 f6                    	movq	%r14, %rsi
100007c59: 48 8d 55 90                 	leaq	-0x70(%rbp), %rdx
100007c5d: e8 da 03 00 00              	callq	0x10000803c <_sysconf+0x10000803c>
100007c62: 85 c0                       	testl	%eax, %eax
100007c64: 0f 94 c1                    	sete	%cl
100007c67: 83 f8 3c                    	cmpl	$0x3c, %eax
100007c6a: 0f 94 c2                    	sete	%dl
100007c6d: 08 ca                       	orb	%cl, %dl
100007c6f: 85 c0                       	testl	%eax, %eax
100007c71: 0f 84 59 01 00 00           	je	0x100007dd0 <_mb_win_mutex_wait_ex+0x5e0>
100007c77: 83 f8 3c                    	cmpl	$0x3c, %eax
100007c7a: 0f 84 50 01 00 00           	je	0x100007dd0 <_mb_win_mutex_wait_ex+0x5e0>
100007c80: 89 55 84                    	movl	%edx, -0x7c(%rbp)
100007c83: 4c 8b 7d a8                 	movq	-0x58(%rbp), %r15
100007c87: 49 8b 07                    	movq	(%r15), %rax
100007c8a: 48 85 c0                    	testq	%rax, %rax
100007c8d: 0f 84 9c 00 00 00           	je	0x100007d2f <_mb_win_mutex_wait_ex+0x53f>
100007c93: 48 8b 75 c0                 	movq	-0x40(%rbp), %rsi
100007c97: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100007c9b: 48 89 75 88                 	movq	%rsi, -0x78(%rbp)
100007c9f: e8 b6 03 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100007ca4: 85 c0                       	testl	%eax, %eax
100007ca6: 74 0c                       	je	0x100007cb4 <_mb_win_mutex_wait_ex+0x4c4>
100007ca8: 45 31 ff                    	xorl	%r15d, %r15d
100007cab: 48 8b 4d a8                 	movq	-0x58(%rbp), %rcx
100007caf: e9 d1 00 00 00              	jmp	0x100007d85 <_mb_win_mutex_wait_ex+0x595>
100007cb4: 4d 8b 3f                    	movq	(%r15), %r15
100007cb7: 49 8b 07                    	movq	(%r15), %rax
100007cba: 48 85 c0                    	testq	%rax, %rax
100007cbd: 0f 84 bf 00 00 00           	je	0x100007d82 <_mb_win_mutex_wait_ex+0x592>
100007cc3: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100007cc7: 48 8b 75 88                 	movq	-0x78(%rbp), %rsi
100007ccb: e8 8a 03 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100007cd0: 4c 89 f9                    	movq	%r15, %rcx
100007cd3: 85 c0                       	testl	%eax, %eax
100007cd5: 74 dd                       	je	0x100007cb4 <_mb_win_mutex_wait_ex+0x4c4>
100007cd7: e9 a9 00 00 00              	jmp	0x100007d85 <_mb_win_mutex_wait_ex+0x595>
100007cdc: 4c 8b 7d a8                 	movq	-0x58(%rbp), %r15
100007ce0: 49 8b 07                    	movq	(%r15), %rax
100007ce3: 48 85 c0                    	testq	%rax, %rax
100007ce6: 74 3f                       	je	0x100007d27 <_mb_win_mutex_wait_ex+0x537>
100007ce8: 48 8b 75 c0                 	movq	-0x40(%rbp), %rsi
100007cec: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100007cf0: 48 89 75 88                 	movq	%rsi, -0x78(%rbp)
100007cf4: e8 61 03 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100007cf9: 85 c0                       	testl	%eax, %eax
100007cfb: 74 09                       	je	0x100007d06 <_mb_win_mutex_wait_ex+0x516>
100007cfd: 45 31 ff                    	xorl	%r15d, %r15d
100007d00: 48 8b 4d a8                 	movq	-0x58(%rbp), %rcx
100007d04: eb 34                       	jmp	0x100007d3a <_mb_win_mutex_wait_ex+0x54a>
100007d06: 4d 8b 3f                    	movq	(%r15), %r15
100007d09: 49 8b 07                    	movq	(%r15), %rax
100007d0c: 48 85 c0                    	testq	%rax, %rax
100007d0f: 74 26                       	je	0x100007d37 <_mb_win_mutex_wait_ex+0x547>
100007d11: 48 8b 78 08                 	movq	0x8(%rax), %rdi
100007d15: 48 8b 75 88                 	movq	-0x78(%rbp), %rsi
100007d19: e8 3c 03 00 00              	callq	0x10000805a <_sysconf+0x10000805a>
100007d1e: 4c 89 f9                    	movq	%r15, %rcx
100007d21: 85 c0                       	testl	%eax, %eax
100007d23: 74 e1                       	je	0x100007d06 <_mb_win_mutex_wait_ex+0x516>
100007d25: eb 13                       	jmp	0x100007d3a <_mb_win_mutex_wait_ex+0x54a>
100007d27: 4c 89 f9                    	movq	%r15, %rcx
100007d2a: 45 31 ff                    	xorl	%r15d, %r15d
100007d2d: eb 0b                       	jmp	0x100007d3a <_mb_win_mutex_wait_ex+0x54a>
100007d2f: 4c 89 f9                    	movq	%r15, %rcx
100007d32: 45 31 ff                    	xorl	%r15d, %r15d
100007d35: eb 4e                       	jmp	0x100007d85 <_mb_win_mutex_wait_ex+0x595>
100007d37: 4c 89 f9                    	movq	%r15, %rcx
100007d3a: 48 8b 01                    	movq	(%rcx), %rax
100007d3d: 48 85 c0                    	testq	%rax, %rax
100007d40: 0f 84 1f 01 00 00           	je	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007d46: 48 8b 10                    	movq	(%rax), %rdx
100007d49: 48 89 11                    	movq	%rdx, (%rcx)
100007d4c: 48 8b 4d b0                 	movq	-0x50(%rbp), %rcx
100007d50: 48 39 41 50                 	cmpq	%rax, 0x50(%rcx)
100007d54: 75 04                       	jne	0x100007d5a <_mb_win_mutex_wait_ex+0x56a>
100007d56: 4c 89 79 50                 	movq	%r15, 0x50(%rcx)
100007d5a: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
100007d61: 48 ff 49 18                 	decq	0x18(%rcx)
100007d65: 4c 89 f7                    	movq	%r14, %rdi
100007d68: e8 1d 03 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007d6d: 85 c0                       	testl	%eax, %eax
100007d6f: 0f 85 f0 00 00 00           	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007d75: 41 bf 02 01 00 00           	movl	$0x102, %r15d           ## imm = 0x102
100007d7b: 31 d2                       	xorl	%edx, %edx
100007d7d: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
100007d80: eb 4c                       	jmp	0x100007dce <_mb_win_mutex_wait_ex+0x5de>
100007d82: 4c 89 f9                    	movq	%r15, %rcx
100007d85: 48 8b 01                    	movq	(%rcx), %rax
100007d88: 48 85 c0                    	testq	%rax, %rax
100007d8b: 0f 84 d4 00 00 00           	je	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007d91: 48 8b 10                    	movq	(%rax), %rdx
100007d94: 48 89 11                    	movq	%rdx, (%rcx)
100007d97: 48 8b 4d b0                 	movq	-0x50(%rbp), %rcx
100007d9b: 48 39 41 50                 	cmpq	%rax, 0x50(%rcx)
100007d9f: 75 04                       	jne	0x100007da5 <_mb_win_mutex_wait_ex+0x5b5>
100007da1: 4c 89 79 50                 	movq	%r15, 0x50(%rcx)
100007da5: 48 c7 00 00 00 00 00        	movq	$0x0, (%rax)
100007dac: 48 ff 49 18                 	decq	0x18(%rcx)
100007db0: 4c 89 f7                    	movq	%r14, %rdi
100007db3: e8 d2 02 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007db8: 85 c0                       	testl	%eax, %eax
100007dba: 0f 85 a5 00 00 00           	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007dc0: 41 bf ff ff ff ff           	movl	$0xffffffff, %r15d      ## imm = 0xFFFFFFFF
100007dc6: b8 57 00 00 00              	movl	$0x57, %eax
100007dcb: 8b 55 84                    	movl	-0x7c(%rbp), %edx
100007dce: 89 03                       	movl	%eax, (%rbx)
100007dd0: 84 d2                       	testb	%dl, %dl
100007dd2: 0f 85 b8 fd ff ff           	jne	0x100007b90 <_mb_win_mutex_wait_ex+0x3a0>
100007dd8: e9 ba fa ff ff              	jmp	0x100007897 <_mb_win_mutex_wait_ex+0xa7>
100007ddd: 4c 8b 65 b0                 	movq	-0x50(%rbp), %r12
100007de1: 49 ff 4c 24 18              	decq	0x18(%r12)
100007de6: 44 8b 7d cc                 	movl	-0x34(%rbp), %r15d
100007dea: 4c 89 f7                    	movq	%r14, %rdi
100007ded: 4c 89 e6                    	movq	%r12, %rsi
100007df0: e8 7b f5 ff ff              	callq	0x100007370 <_adopt>
100007df5: 85 c0                       	testl	%eax, %eax
100007df7: 74 28                       	je	0x100007e21 <_mb_win_mutex_wait_ex+0x631>
100007df9: 4c 89 f7                    	movq	%r14, %rdi
100007dfc: e8 89 02 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007e01: 85 c0                       	testl	%eax, %eax
100007e03: 8b 45 d4                    	movl	-0x2c(%rbp), %eax
100007e06: 74 56                       	je	0x100007e5e <_mb_win_mutex_wait_ex+0x66e>
100007e08: eb 5b                       	jmp	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007e0a: 4c 89 f7                    	movq	%r14, %rdi
100007e0d: e8 78 02 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007e12: 85 c0                       	testl	%eax, %eax
100007e14: 75 4f                       	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007e16: c7 03 57 00 00 00           	movl	$0x57, (%rbx)
100007e1c: e9 70 fa ff ff              	jmp	0x100007891 <_mb_win_mutex_wait_ex+0xa1>
100007e21: 41 c7 44 24 34 00 00 00 00  	movl	$0x0, 0x34(%r12)
100007e2a: 41 81 ff 80 00 00 00        	cmpl	$0x80, %r15d
100007e31: 75 09                       	jne	0x100007e3c <_mb_win_mutex_wait_ex+0x64c>
100007e33: 41 c7 44 24 38 01 00 00 00  	movl	$0x1, 0x38(%r12)
100007e3c: 4c 89 e7                    	movq	%r12, %rdi
100007e3f: 44 89 fe                    	movl	%r15d, %esi
100007e42: e8 b9 f8 ff ff              	callq	0x100007700 <_grant_first>
100007e47: 4c 89 f7                    	movq	%r14, %rdi
100007e4a: e8 3b 02 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007e4f: 85 c0                       	testl	%eax, %eax
100007e51: 75 12                       	jne	0x100007e65 <_mb_win_mutex_wait_ex+0x675>
100007e53: 41 bf ff ff ff ff           	movl	$0xffffffff, %r15d      ## imm = 0xFFFFFFFF
100007e59: b8 08 00 00 00              	movl	$0x8, %eax
100007e5e: 89 03                       	movl	%eax, (%rbx)
100007e60: e9 32 fa ff ff              	jmp	0x100007897 <_mb_win_mutex_wait_ex+0xa7>
100007e65: e8 72 01 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100007e6a: 66 0f 1f 44 00 00           	nopw	(%rax,%rax)

0000000100007e70 <_mb_win_mutex_close_handle>:
100007e70: 55                          	pushq	%rbp
100007e71: 48 89 e5                    	movq	%rsp, %rbp
100007e74: 41 57                       	pushq	%r15
100007e76: 41 56                       	pushq	%r14
100007e78: 41 54                       	pushq	%r12
100007e7a: 53                          	pushq	%rbx
100007e7b: 48 85 ff                    	testq	%rdi, %rdi
100007e7e: 0f 84 ae 00 00 00           	je	0x100007f32 <_mb_win_mutex_close_handle+0xc2>
100007e84: 4c 8b 37                    	movq	(%rdi), %r14
100007e87: 4d 85 f6                    	testq	%r14, %r14
100007e8a: 0f 84 a2 00 00 00           	je	0x100007f32 <_mb_win_mutex_close_handle+0xc2>
100007e90: 49 89 f7                    	movq	%rsi, %r15
100007e93: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007e97: e8 84 a2 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007e9c: 4c 89 f7                    	movq	%r14, %rdi
100007e9f: e8 e0 01 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100007ea4: 85 c0                       	testl	%eax, %eax
100007ea6: 0f 85 86 00 00 00           	jne	0x100007f32 <_mb_win_mutex_close_handle+0xc2>
100007eac: 49 8b 7e 40                 	movq	0x40(%r14), %rdi
100007eb0: e8 6b a2 ff ff              	callq	0x100002120 <_mb_win_threads_last_error>
100007eb5: 48 89 c3                    	movq	%rax, %rbx
100007eb8: 44 8b 20                    	movl	(%rax), %r12d
100007ebb: 49 8d 7e 68                 	leaq	0x68(%r14), %rdi
100007ebf: 90                          	nop
100007ec0: 48 89 f8                    	movq	%rdi, %rax
100007ec3: 48 8b 3f                    	movq	(%rdi), %rdi
100007ec6: 48 85 ff                    	testq	%rdi, %rdi
100007ec9: 74 47                       	je	0x100007f12 <_mb_win_mutex_close_handle+0xa2>
100007ecb: 4c 39 7f 08                 	cmpq	%r15, 0x8(%rdi)
100007ecf: 75 ef                       	jne	0x100007ec0 <_mb_win_mutex_close_handle+0x50>
100007ed1: 48 8b 0f                    	movq	(%rdi), %rcx
100007ed4: 48 89 08                    	movq	%rcx, (%rax)
100007ed7: 4c 8b 7f 10                 	movq	0x10(%rdi), %r15
100007edb: 49 ff 4f 10                 	decq	0x10(%r15)
100007edf: e8 22 01 00 00              	callq	0x100008006 <_sysconf+0x100008006>
100007ee4: 49 83 7f 10 00              	cmpq	$0x0, 0x10(%r15)
100007ee9: 75 12                       	jne	0x100007efd <_mb_win_mutex_close_handle+0x8d>
100007eeb: 49 83 7f 18 00              	cmpq	$0x0, 0x18(%r15)
100007ef0: 75 0b                       	jne	0x100007efd <_mb_win_mutex_close_handle+0x8d>
100007ef2: 4c 89 f7                    	movq	%r14, %rdi
100007ef5: 4c 89 fe                    	movq	%r15, %rsi
100007ef8: e8 c3 ec ff ff              	callq	0x100006bc0 <_destroy_object>
100007efd: 4c 89 f7                    	movq	%r14, %rdi
100007f00: e8 85 01 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007f05: 89 c1                       	movl	%eax, %ecx
100007f07: b8 01 00 00 00              	movl	$0x1, %eax
100007f0c: 85 c9                       	testl	%ecx, %ecx
100007f0e: 74 16                       	je	0x100007f26 <_mb_win_mutex_close_handle+0xb6>
100007f10: eb 20                       	jmp	0x100007f32 <_mb_win_mutex_close_handle+0xc2>
100007f12: 4c 89 f7                    	movq	%r14, %rdi
100007f15: e8 70 01 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007f1a: 85 c0                       	testl	%eax, %eax
100007f1c: 75 14                       	jne	0x100007f32 <_mb_win_mutex_close_handle+0xc2>
100007f1e: 31 c0                       	xorl	%eax, %eax
100007f20: 41 bc 06 00 00 00           	movl	$0x6, %r12d
100007f26: 44 89 23                    	movl	%r12d, (%rbx)
100007f29: 5b                          	popq	%rbx
100007f2a: 41 5c                       	popq	%r12
100007f2c: 41 5e                       	popq	%r14
100007f2e: 41 5f                       	popq	%r15
100007f30: 5d                          	popq	%rbp
100007f31: c3                          	retq
100007f32: e8 a5 00 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100007f37: 66 0f 1f 84 00 00 00 00 00  	nopw	(%rax,%rax)

0000000100007f40 <_mb_win_mutex_is_handle>:
100007f40: 55                          	pushq	%rbp
100007f41: 48 89 e5                    	movq	%rsp, %rbp
100007f44: 41 57                       	pushq	%r15
100007f46: 41 56                       	pushq	%r14
100007f48: 53                          	pushq	%rbx
100007f49: 50                          	pushq	%rax
100007f4a: 45 31 ff                    	xorl	%r15d, %r15d
100007f4d: 48 85 ff                    	testq	%rdi, %rdi
100007f50: 74 44                       	je	0x100007f96 <_mb_win_mutex_is_handle+0x56>
100007f52: 4c 8b 37                    	movq	(%rdi), %r14
100007f55: 4d 85 f6                    	testq	%r14, %r14
100007f58: 74 3c                       	je	0x100007f96 <_mb_win_mutex_is_handle+0x56>
100007f5a: 48 89 f3                    	movq	%rsi, %rbx
100007f5d: 4c 89 f7                    	movq	%r14, %rdi
100007f60: e8 1f 01 00 00              	callq	0x100008084 <_sysconf+0x100008084>
100007f65: 85 c0                       	testl	%eax, %eax
100007f67: 75 3b                       	jne	0x100007fa4 <_mb_win_mutex_is_handle+0x64>
100007f69: 49 8d 46 68                 	leaq	0x68(%r14), %rax
100007f6d: 0f 1f 00                    	nopl	(%rax)
100007f70: 48 8b 00                    	movq	(%rax), %rax
100007f73: 48 85 c0                    	testq	%rax, %rax
100007f76: 74 06                       	je	0x100007f7e <_mb_win_mutex_is_handle+0x3e>
100007f78: 48 39 58 08                 	cmpq	%rbx, 0x8(%rax)
100007f7c: 75 f2                       	jne	0x100007f70 <_mb_win_mutex_is_handle+0x30>
100007f7e: 45 31 ff                    	xorl	%r15d, %r15d
100007f81: 48 85 c0                    	testq	%rax, %rax
100007f84: 0f 95 c3                    	setne	%bl
100007f87: 4c 89 f7                    	movq	%r14, %rdi
100007f8a: e8 fb 00 00 00              	callq	0x10000808a <_sysconf+0x10000808a>
100007f8f: 85 c0                       	testl	%eax, %eax
100007f91: 75 11                       	jne	0x100007fa4 <_mb_win_mutex_is_handle+0x64>
100007f93: 41 88 df                    	movb	%bl, %r15b
100007f96: 44 89 f8                    	movl	%r15d, %eax
100007f99: 48 83 c4 08                 	addq	$0x8, %rsp
100007f9d: 5b                          	popq	%rbx
100007f9e: 41 5e                       	popq	%r14
100007fa0: 41 5f                       	popq	%r15
100007fa2: 5d                          	popq	%rbp
100007fa3: c3                          	retq
100007fa4: e8 33 00 00 00              	callq	0x100007fdc <_sysconf+0x100007fdc>
100007fa9: 00 00                       	addb	%al, (%rax)
100007fab: 00 00                       	addb	%al, (%rax)
100007fad: 00 00                       	addb	%al, (%rax)
100007faf: 00 55 48                    	addb	%dl, 0x48(%rbp)

0000000100007fb0 <_mb_win_workers_sleep.cold.1>:
100007fb0: 55                          	pushq	%rbp
100007fb1: 48 89 e5                    	movq	%rsp, %rbp
100007fb4: 0f 10 06                    	movups	(%rsi), %xmm0
100007fb7: 0f 11 07                    	movups	%xmm0, (%rdi)
100007fba: e8 5f 00 00 00              	callq	0x10000801e <_sysconf+0x10000801e>
100007fbf: 85 c0                       	testl	%eax, %eax
100007fc1: 5d                          	popq	%rbp
100007fc2: 74 03                       	je	0x100007fc7 <_mb_win_workers_sleep.cold.1+0x17>
100007fc4: 31 c0                       	xorl	%eax, %eax
100007fc6: c3                          	retq
100007fc7: b0 01                       	movb	$0x1, %al
100007fc9: c3                          	retq

Disassembly of section __TEXT,__stubs:

0000000100007fca <__stubs>:
100007fca: ff 25 30 10 00 00           	jmpq	*0x1030(%rip)           ## 0x100009000 <_sysconf+0x100009000>
100007fd0: ff 25 32 10 00 00           	jmpq	*0x1032(%rip)           ## 0x100009008 <_sysconf+0x100009008>
100007fd6: ff 25 44 10 00 00           	jmpq	*0x1044(%rip)           ## 0x100009020 <_sysconf+0x100009020>
100007fdc: ff 25 46 10 00 00           	jmpq	*0x1046(%rip)           ## 0x100009028 <_sysconf+0x100009028>
100007fe2: ff 25 48 10 00 00           	jmpq	*0x1048(%rip)           ## 0x100009030 <_sysconf+0x100009030>
100007fe8: ff 25 4a 10 00 00           	jmpq	*0x104a(%rip)           ## 0x100009038 <_sysconf+0x100009038>
100007fee: ff 25 4c 10 00 00           	jmpq	*0x104c(%rip)           ## 0x100009040 <_sysconf+0x100009040>
100007ff4: ff 25 4e 10 00 00           	jmpq	*0x104e(%rip)           ## 0x100009048 <_sysconf+0x100009048>
100007ffa: ff 25 50 10 00 00           	jmpq	*0x1050(%rip)           ## 0x100009050 <_sysconf+0x100009050>
100008000: ff 25 52 10 00 00           	jmpq	*0x1052(%rip)           ## 0x100009058 <_sysconf+0x100009058>
100008006: ff 25 54 10 00 00           	jmpq	*0x1054(%rip)           ## 0x100009060 <_sysconf+0x100009060>
10000800c: ff 25 56 10 00 00           	jmpq	*0x1056(%rip)           ## 0x100009068 <_sysconf+0x100009068>
100008012: ff 25 58 10 00 00           	jmpq	*0x1058(%rip)           ## 0x100009070 <_sysconf+0x100009070>
100008018: ff 25 5a 10 00 00           	jmpq	*0x105a(%rip)           ## 0x100009078 <_sysconf+0x100009078>
10000801e: ff 25 5c 10 00 00           	jmpq	*0x105c(%rip)           ## 0x100009080 <_sysconf+0x100009080>
100008024: ff 25 5e 10 00 00           	jmpq	*0x105e(%rip)           ## 0x100009088 <_sysconf+0x100009088>
10000802a: ff 25 60 10 00 00           	jmpq	*0x1060(%rip)           ## 0x100009090 <_sysconf+0x100009090>
100008030: ff 25 62 10 00 00           	jmpq	*0x1062(%rip)           ## 0x100009098 <_sysconf+0x100009098>
100008036: ff 25 64 10 00 00           	jmpq	*0x1064(%rip)           ## 0x1000090a0 <_sysconf+0x1000090a0>
10000803c: ff 25 66 10 00 00           	jmpq	*0x1066(%rip)           ## 0x1000090a8 <_sysconf+0x1000090a8>
100008042: ff 25 68 10 00 00           	jmpq	*0x1068(%rip)           ## 0x1000090b0 <_sysconf+0x1000090b0>
100008048: ff 25 6a 10 00 00           	jmpq	*0x106a(%rip)           ## 0x1000090b8 <_sysconf+0x1000090b8>
10000804e: ff 25 6c 10 00 00           	jmpq	*0x106c(%rip)           ## 0x1000090c0 <_sysconf+0x1000090c0>
100008054: ff 25 6e 10 00 00           	jmpq	*0x106e(%rip)           ## 0x1000090c8 <_sysconf+0x1000090c8>
10000805a: ff 25 70 10 00 00           	jmpq	*0x1070(%rip)           ## 0x1000090d0 <_sysconf+0x1000090d0>
100008060: ff 25 72 10 00 00           	jmpq	*0x1072(%rip)           ## 0x1000090d8 <_sysconf+0x1000090d8>
100008066: ff 25 74 10 00 00           	jmpq	*0x1074(%rip)           ## 0x1000090e0 <_sysconf+0x1000090e0>
10000806c: ff 25 76 10 00 00           	jmpq	*0x1076(%rip)           ## 0x1000090e8 <_sysconf+0x1000090e8>
100008072: ff 25 78 10 00 00           	jmpq	*0x1078(%rip)           ## 0x1000090f0 <_sysconf+0x1000090f0>
100008078: ff 25 7a 10 00 00           	jmpq	*0x107a(%rip)           ## 0x1000090f8 <_sysconf+0x1000090f8>
10000807e: ff 25 7c 10 00 00           	jmpq	*0x107c(%rip)           ## 0x100009100 <_sysconf+0x100009100>
100008084: ff 25 7e 10 00 00           	jmpq	*0x107e(%rip)           ## 0x100009108 <_sysconf+0x100009108>
10000808a: ff 25 80 10 00 00           	jmpq	*0x1080(%rip)           ## 0x100009110 <_sysconf+0x100009110>
100008090: ff 25 82 10 00 00           	jmpq	*0x1082(%rip)           ## 0x100009118 <_sysconf+0x100009118>
100008096: ff 25 84 10 00 00           	jmpq	*0x1084(%rip)           ## 0x100009120 <_sysconf+0x100009120>
10000809c: ff 25 86 10 00 00           	jmpq	*0x1086(%rip)           ## 0x100009128 <_sysconf+0x100009128>
1000080a2: ff 25 88 10 00 00           	jmpq	*0x1088(%rip)           ## 0x100009130 <_sysconf+0x100009130>
1000080a8: ff 25 8a 10 00 00           	jmpq	*0x108a(%rip)           ## 0x100009138 <_sysconf+0x100009138>
1000080ae: ff 25 8c 10 00 00           	jmpq	*0x108c(%rip)           ## 0x100009140 <_sysconf+0x100009140>
