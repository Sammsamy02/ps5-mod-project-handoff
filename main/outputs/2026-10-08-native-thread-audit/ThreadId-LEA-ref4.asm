0xa0a1e0 file0xa0e1e0 55 push rbp
0xa0a1e1 file0xa0e1e1 4889e5 mov rbp, rsp
0xa0a1e4 file0xa0e1e4 4157 push r15
0xa0a1e6 file0xa0e1e6 4156 push r14
0xa0a1e8 file0xa0e1e8 4155 push r13
0xa0a1ea file0xa0e1ea 4154 push r12
0xa0a1ec file0xa0e1ec 53 push rbx
0xa0a1ed file0xa0e1ed 4881eca8000000 sub rsp, 0xa8
0xa0a1f4 file0xa0e1f4 488b1d0d7d7208 mov rbx, qword ptr [rip + 0x8727d0d] rip->0x9131f08
0xa0a1fb file0xa0e1fb 89b574ffffff mov dword ptr [rbp - 0x8c], esi
0xa0a201 file0xa0e201 4989fe mov r14, rdi
0xa0a204 file0xa0e204 488b03 mov rax, qword ptr [rbx]
0xa0a207 file0xa0e207 488945d0 mov qword ptr [rbp - 0x30], rax
0xa0a20b file0xa0e20b e8f0feffff call 0xa0a100
0xa0a210 file0xa0e210 84c0 test al, al
0xa0a212 file0xa0e212 0f8533070000 jne 0xa0a94b
0xa0a218 file0xa0e218 e8a3eaffff call 0xa08cc0
0xa0a21d file0xa0e21d 498b06 mov rax, qword ptr [r14]
0xa0a220 file0xa0e220 4885c0 test rax, rax
0xa0a223 file0xa0e223 0f8422070000 je 0xa0a94b
0xa0a229 file0xa0e229 0fb680a0000000 movzx eax, byte ptr [rax + 0xa0]
0xa0a230 file0xa0e230 84c0 test al, al
0xa0a232 file0xa0e232 0f85f8060000 jne 0xa0a930
0xa0a238 file0xa0e238 8b057a889f08 mov eax, dword ptr [rip + 0x89f887a] rip->0x9402ab8
0xa0a23e file0xa0e23e 4c89b550ffffff mov qword ptr [rbp - 0xb0], r14
0xa0a245 file0xa0e245 8d4801 lea ecx, [rax + 1]
0xa0a248 file0xa0e248 890d6a889f08 mov dword ptr [rip + 0x89f886a], ecx rip->0x9402ab8
0xa0a24e file0xa0e24e 85c0 test eax, eax
0xa0a250 file0xa0e250 7f17 jg 0xa0a269
0xa0a252 file0xa0e252 e8696d63ff call 0x40fc0
0xa0a257 file0xa0e257 488b08 mov rcx, qword ptr [rax]
0xa0a25a file0xa0e25a 4889c7 mov rdi, rax
0xa0a25d file0xa0e25d be01000000 mov esi, 1
0xa0a262 file0xa0e262 ff5140 call qword ptr [rcx + 0x40]
0xa0a265 file0xa0e265 84c0 test al, al
0xa0a267 file0xa0e267 740a je 0xa0a273
0xa0a269 file0xa0e269 c78574ffffff00000000 mov dword ptr [rbp - 0x8c], 0
0xa0a273 file0xa0e273 8b059b7d8d08 mov eax, dword ptr [rip + 0x88d7d9b] rip->0x92e2014
0xa0a279 file0xa0e279 b921000000 mov ecx, 0x21
0xa0a27e file0xa0e27e 488d5d98 lea rbx, [rbp - 0x68]
0xa0a282 file0xa0e282 4889df mov rdi, rbx
0xa0a285 file0xa0e285 83f821 cmp eax, 0x21
0xa0a288 file0xa0e288 0f42c8 cmovb ecx, eax
0xa0a28b file0xa0e28b 8d048d00000000 lea eax, [rcx*4]
0xa0a292 file0xa0e292 48898d60ffffff mov qword ptr [rbp - 0xa0], rcx
0xa0a299 file0xa0e299 c5fb2ac0 vcvtsi2sd xmm0, xmm0, eax
0xa0a29d file0xa0e29d c5fb5e05abe00307 vdivsd xmm0, xmm0, qword ptr [rip + 0x703e0ab] rip->0x7a48350
0xa0a2a5 file0xa0e2a5 e876f26aff call 0xb9520
0xa0a2aa file0xa0e2aa 4889df mov rdi, rbx
0xa0a2ad file0xa0e2ad e8eef26aff call 0xb95a0
0xa0a2b2 file0xa0e2b2 c5fb118558ffffff vmovsd qword ptr [rbp - 0xa8], xmm0
0xa0a2ba file0xa0e2ba c5f32a05567d8d08 vcvtsi2sd xmm0, xmm1, dword ptr [rip + 0x88d7d56] rip->0x92e2018
0xa0a2c2 file0xa0e2c2 c5fb118578ffffff vmovsd qword ptr [rbp - 0x88], xmm0
0xa0a2ca file0xa0e2ca e8115e5fff call 0xe0
0xa0a2cf file0xa0e2cf 0fb60dea879f08 movzx ecx, byte ptr [rip + 0x89f87ea] rip->0x9402ac0
0xa0a2d6 file0xa0e2d6 4189c7 mov r15d, eax
0xa0a2d9 file0xa0e2d9 84c9 test cl, cl
0xa0a2db file0xa0e2db 0f84a4060000 je 0xa0a985
0xa0a2e1 file0xa0e2e1 488d7d80 lea rdi, [rbp - 0x80]
0xa0a2e5 file0xa0e2e5 31f6 xor esi, esi
0xa0a2e7 file0xa0e2e7 e8b43b6bff call 0xbdea0
0xa0a2ec file0xa0e2ec c5f8104580 vmovups xmm0, xmmword ptr [rbp - 0x80]
0xa0a2f1 file0xa0e2f1 488b4588 mov rax, qword ptr [rbp - 0x78]
0xa0a2f5 file0xa0e2f5 c5f8298540ffffff vmovaps xmmword ptr [rbp - 0xc0], xmm0
0xa0a2fd file0xa0e2fd 4885c0 test rax, rax
0xa0a300 file0xa0e300 7404 je 0xa0a306
0xa0a302 file0xa0e302 f0ff4008 lock inc dword ptr [rax + 8]
0xa0a306 file0xa0e306 4c8d2d73b69508 lea r13, [rip + 0x895b673] rip->0x9365980
0xa0a30d file0xa0e30d 418b7d00 mov edi, dword ptr [r13]
0xa0a311 file0xa0e311 e81a82a606 call 0x7472530
0xa0a316 file0xa0e316 4889c3 mov rbx, rax
0xa0a319 file0xa0e319 4885c0 test rax, rax
0xa0a31c file0xa0e31c 7528 jne 0xa0a346
0xa0a31e file0xa0e31e bf18000000 mov edi, 0x18
0xa0a323 file0xa0e323 e858c9e803 call 0x4896c80
0xa0a328 file0xa0e328 418b7d00 mov edi, dword ptr [r13]
0xa0a32c file0xa0e32c 4889c6 mov rsi, rax
0xa0a32f file0xa0e32f c5f857c0 vxorps xmm0, xmm0, xmm0
0xa0a333 file0xa0e333 4889c3 mov rbx, rax
0xa0a336 file0xa0e336 c5f81100 vmovups xmmword ptr [rax], xmm0
0xa0a33a file0xa0e33a c7401000000000 mov dword ptr [rax + 0x10], 0
0xa0a341 file0xa0e341 e84a82a606 call 0x7472590
0xa0a346 file0xa0e346 4c8b6308 mov r12, qword ptr [rbx + 8]
0xa0a34a file0xa0e34a 4d85e4 test r12, r12
0xa0a34d file0xa0e34d 740a je 0xa0a359
0xa0a34f file0xa0e34f 8b4310 mov eax, dword ptr [rbx + 0x10]
0xa0a352 file0xa0e352 ffc8 dec eax
0xa0a354 file0xa0e354 e913010000 jmp 0xa0a46c
0xa0a359 file0xa0e359 4c8b23 mov r12, qword ptr [rbx]
0xa0a35c file0xa0e35c 4d85e4 test r12, r12
0xa0a35f file0xa0e35f 740c je 0xa0a36d
0xa0a361 file0xa0e361 4c896308 mov qword ptr [rbx + 8], r12
0xa0a365 file0xa0e365 4889d8 mov rax, rbx
0xa0a368 file0xa0e368 e9f3000000 jmp 0xa0a460
0xa0a36d file0xa0e36d 4c8d254cfd9008 lea r12, [rip + 0x890fd4c] rip->0x931a0c0
0xa0a374 file0xa0e374 6666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0xa0a380 file0xa0e380 498b4540 mov rax, qword ptr [r13 + 0x40]
0xa0a384 file0xa0e384 89c7 mov edi, eax
0xa0a386 file0xa0e386 81e7ffffff03 and edi, 0x3ffffff
0xa0a38c file0xa0e38c 746a je 0xa0a3f8
0xa0a38e file0xa0e38e 4c8db000000004 lea r14, [rax + 0x4000000]
0xa0a395 file0xa0e395 4889c1 mov rcx, rax
0xa0a398 file0xa0e398 4c89f2 mov rdx, r14
0xa0a39b file0xa0e39b 48c1e91a shr rcx, 0x1a
0xa0a39f file0xa0e39f 48c1ea1a shr rdx, 0x1a
0xa0a3a3 file0xa0e3a3 4839ca cmp rdx, rcx
0xa0a3a6 file0xa0e3a6 722f jb 0xa0a3d7
0xa0a3a8 file0xa0e3a8 89f9 mov ecx, edi
0xa0a3aa file0xa0e3aa 89c2 mov edx, eax
0xa0a3ac file0xa0e3ac 4981e6000000fc and r14, 0xfffffffffc000000
0xa0a3b3 file0xa0e3b3 c1e90e shr ecx, 0xe
0xa0a3b6 file0xa0e3b6 81e2ff3f0000 and edx, 0x3fff
0xa0a3bc file0xa0e3bc 498b74cc40 mov rsi, qword ptr [r12 + rcx*8 + 0x40]
0xa0a3c1 file0xa0e3c1 488d1452 lea rdx, [rdx + rdx*2]
0xa0a3c5 file0xa0e3c5 448b44d610 mov r8d, dword ptr [rsi + rdx*8 + 0x10]
0xa0a3ca file0xa0e3ca 4d09c6 or r14, r8
0xa0a3cd file0xa0e3cd f04d0fb17540 lock cmpxchg qword ptr [r13 + 0x40], r14
0xa0a3d3 file0xa0e3d3 75ab jne 0xa0a380
0xa0a3d5 file0xa0e3d5 eb2b jmp 0xa0a402
0xa0a3d7 file0xa0e3d7 89bd68ffffff mov dword ptr [rbp - 0x98], edi
0xa0a3dd file0xa0e3dd 48898538ffffff mov qword ptr [rbp - 0xc8], rax
0xa0a3e4 file0xa0e3e4 e8e78f65ff call 0x633d0
0xa0a3e9 file0xa0e3e9 488b8538ffffff mov rax, qword ptr [rbp - 0xc8]
0xa0a3f0 file0xa0e3f0 8bbd68ffffff mov edi, dword ptr [rbp - 0x98]
0xa0a3f6 file0xa0e3f6 ebb0 jmp 0xa0a3a8
0xa0a3f8 file0xa0e3f8 48c7430800000000 mov qword ptr [rbx + 8], 0
0xa0a400 file0xa0e400 eb28 jmp 0xa0a42a
0xa0a402 file0xa0e402 488d44d610 lea rax, [rsi + rdx*8 + 0x10]
0xa0a407 file0xa0e407 c70000000000 mov dword ptr [rax], 0
0xa0a40d file0xa0e40d 498b44cc40 mov rax, qword ptr [r12 + rcx*8 + 0x40]
0xa0a412 file0xa0e412 4c8b64d008 mov r12, qword ptr [rax + rdx*8 + 8]
0xa0a417 file0xa0e417 e8d48f65ff call 0x633f0
0xa0a41c file0xa0e41c b8ff000000 mov eax, 0xff
0xa0a421 file0xa0e421 4c896308 mov qword ptr [rbx + 8], r12
0xa0a425 file0xa0e425 4d85e4 test r12, r12
0xa0a428 file0xa0e428 7542 jne 0xa0a46c
0xa0a42a file0xa0e42a bf00000100 mov edi, 0x10000
0xa0a42f file0xa0e42f 31f6 xor esi, esi
0xa0a431 file0xa0e431 e8ead56aff call 0xb7a20
0xa0a436 file0xa0e436 4989c4 mov r12, rax
0xa0a439 file0xa0e439 b9ff000000 mov ecx, 0xff
0xa0a43e file0xa0e43e 48894308 mov qword ptr [rbx + 8], rax
0xa0a442 file0xa0e442 66666666662e0f1f840000000000 nop word ptr cs:[rax + rax]
0xa0a450 file0xa0e450 4889c2 mov rdx, rax
0xa0a453 file0xa0e453 480500010000 add rax, 0x100
0xa0a459 file0xa0e459 ffc9 dec ecx
0xa0a45b file0xa0e45b 488902 mov qword ptr [rdx], rax
0xa0a45e file0xa0e45e 75f0 jne 0xa0a450
0xa0a460 file0xa0e460 48c70000000000 mov qword ptr [rax], 0
0xa0a467 file0xa0e467 b8ff000000 mov eax, 0xff
0xa0a46c file0xa0e46c 498b0c24 mov rcx, qword ptr [r12]
0xa0a470 file0xa0e470 488d1529364b07 lea rdx, [rip + 0x74b3629] rip->0x7ebdaa0
0xa0a477 file0xa0e477 498d7c2470 lea rdi, [r12 + 0x70]
0xa0a47c file0xa0e47c 48894b08 mov qword ptr [rbx + 8], rcx
0xa0a480 file0xa0e480 48b90200000001000000 movabs rcx, 0x100000002
0xa0a48a file0xa0e48a 894310 mov dword ptr [rbx + 0x10], eax
0xa0a48d file0xa0e48d 49891424 mov qword ptr [r12], rdx
0xa0a491 file0xa0e491 48ba0000000001000000 movabs rdx, 0x100000000
0xa0a49b file0xa0e49b 49894c2408 mov qword ptr [r12 + 8], rcx
0xa0a4a0 file0xa0e4a0 488d0dc12c4b07 lea rcx, [rip + 0x74b2cc1] rip->0x7ebd168
0xa0a4a7 file0xa0e4a7 49c744241000000000 mov qword ptr [r12 + 0x10], 0
0xa0a4b0 file0xa0e4b0 41c744241800000000 mov dword ptr [r12 + 0x18], 0
0xa0a4b9 file0xa0e4b9 41c7442420ffffffff mov dword ptr [r12 + 0x20], 0xffffffff
0xa0a4c2 file0xa0e4c2 49c744243000000000 mov qword ptr [r12 + 0x30], 0
0xa0a4cb file0xa0e4cb 4989542438 mov qword ptr [r12 + 0x38], rdx
0xa0a4d0 file0xa0e4d0 41c644244001 mov byte ptr [r12 + 0x40], 1
0xa0a4d6 file0xa0e4d6 49894c2470 mov qword ptr [r12 + 0x70], rcx
0xa0a4db file0xa0e4db 48b920000000000000e8 movabs rcx, 0xe800000000000020
0xa0a4e5 file0xa0e4e5 49c744247800000000 mov qword ptr [r12 + 0x78], 0
0xa0a4ee file0xa0e4ee 49898c2480000000 mov qword ptr [r12 + 0x80], rcx
0xa0a4f6 file0xa0e4f6 49c784249000000000000000 mov qword ptr [r12 + 0x90], 0
0xa0a502 file0xa0e502 4989942498000000 mov qword ptr [r12 + 0x98], rdx
0xa0a50a file0xa0e50a 6641c78424a00000000000 mov word ptr [r12 + 0xa0], 0
0xa0a515 file0xa0e515 b1ff mov cl, 0xff
0xa0a517 file0xa0e517 f0410fc04c2440 lock xadd byte ptr [r12 + 0x40], cl
0xa0a51e file0xa0e51e f6c102 test cl, 2
0xa0a521 file0xa0e521 0f853f040000 jne 0xa0a966
0xa0a527 file0xa0e527 488d05f2855407 lea rax, [rip + 0x75485f2] rip->0x7f52b20
0xa0a52e file0xa0e52e 498d742448 lea rsi, [r12 + 0x48]
0xa0a533 file0xa0e533 49890424 mov qword ptr [r12], rax
0xa0a537 file0xa0e537 498b442470 mov rax, qword ptr [r12 + 0x70]
0xa0a53c file0xa0e53c ff5018 call qword ptr [rax + 0x18]
0xa0a53f file0xa0e53f 48833de179720800 cmp qword ptr [rip + 0x87279e1], 0 rip->0x9131f28
0xa0a547 file0xa0e547 488d057a354b07 lea rax, [rip + 0x74b357a] rip->0x7ebdac8
0xa0a54e file0xa0e54e 4989442470 mov qword ptr [r12 + 0x70], rax
0xa0a553 file0xa0e553 4d89642448 mov qword ptr [r12 + 0x48], r12
0xa0a558 file0xa0e558 4d89642450 mov qword ptr [r12 + 0x50], r12
0xa0a55d file0xa0e55d 7405 je 0xa0a564
0xa0a55f file0xa0e55f e89c5a5fff call 0
0xa0a564 file0xa0e564 64488b042500000000 mov rax, qword ptr fs:[0]
0xa0a56d file0xa0e56d 488d8068eeffff lea rax, [rax - 0x1198]
0xa0a574 file0xa0e574 488b00 mov rax, qword ptr [rax]
0xa0a577 file0xa0e577 4885c0 test rax, rax
0xa0a57a file0xa0e57a 7406 je 0xa0a582
0xa0a57c file0xa0e57c 488b4030 mov rax, qword ptr [rax + 0x30]
0xa0a580 file0xa0e580 eb02 jmp 0xa0a584
0xa0a582 file0xa0e582 31c0 xor eax, eax
0xa0a584 file0xa0e584 488d35d1fecc06 lea rsi, [rip + 0x6ccfed1] rip->0x76da45c
0xa0a58b file0xa0e58b 48b980ffffffffffff07 movabs rcx, 0x7ffffffffffff80
0xa0a595 file0xa0e595 48ba00000000000000c8 movabs rdx, 0xc800000000000000
0xa0a59f file0xa0e59f 4989442478 mov qword ptr [r12 + 0x78], rax
0xa0a5a4 file0xa0e5a4 488d054d855407 lea rax, [rip + 0x754854d] rip->0x7f52af8
0xa0a5ab file0xa0e5ab 48c1e606 shl rsi, 6
0xa0a5af file0xa0e5af 4821f1 and rcx, rsi
0xa0a5b2 file0xa0e5b2 4809ca or rdx, rcx
0xa0a5b5 file0xa0e5b5 4989942480000000 mov qword ptr [r12 + 0x80], rdx
0xa0a5bd file0xa0e5bd 41c644241c01 mov byte ptr [r12 + 0x1c], 1
0xa0a5c3 file0xa0e5c3 c5f8288540ffffff vmovaps xmm0, xmmword ptr [rbp - 0xc0]
0xa0a5cb file0xa0e5cb 488b8d50ffffff mov rcx, qword ptr [rbp - 0xb0]
0xa0a5d2 file0xa0e5d2 c4c178118424a8000000 vmovups xmmword ptr [r12 + 0xa8], xmm0
0xa0a5dc file0xa0e5dc 49890424 mov qword ptr [r12], rax
0xa0a5e0 file0xa0e5e0 488b31 mov rsi, qword ptr [rcx]
0xa0a5e3 file0xa0e5e3 4885f6 test rsi, rsi
0xa0a5e6 file0xa0e5e6 7408 je 0xa0a5f0
0xa0a5e8 file0xa0e5e8 4c89e7 mov rdi, r12
0xa0a5eb file0xa0e5eb e8e0f1bbff call 0x5c97d0
0xa0a5f0 file0xa0e5f0 c5fb108578ffffff vmovsd xmm0, qword ptr [rbp - 0x88]
0xa0a5f8 file0xa0e5f8 c5fb5e0550dd0307 vdivsd xmm0, xmm0, qword ptr [rip + 0x703dd50] rip->0x7a48350
0xa0a600 file0xa0e600 488d75c0 lea rsi, [rbp - 0x40]
0xa0a604 file0xa0e604 4c89e7 mov rdi, r12
0xa0a607 file0xa0e607 c645c001 mov byte ptr [rbp - 0x40], 1
0xa0a60b file0xa0e60b c5fb118578ffffff vmovsd qword ptr [rbp - 0x88], xmm0
0xa0a613 file0xa0e613 e818ec62ff call 0x39230
0xa0a618 file0xa0e618 4d85e4 test r12, r12
0xa0a61b file0xa0e61b 7414 je 0xa0a631
0xa0a61d file0xa0e61d 498d442408 lea rax, [r12 + 8]
0xa0a622 file0xa0e622 f0ff08 lock dec dword ptr [rax]
0xa0a625 file0xa0e625 750a jne 0xa0a631
0xa0a627 file0xa0e627 498b0424 mov rax, qword ptr [r12]
0xa0a62b file0xa0e62b 4c89e7 mov rdi, r12
0xa0a62e file0xa0e62e ff5008 call qword ptr [rax + 8]
0xa0a631 file0xa0e631 698560ffffff10270000 imul eax, dword ptr [rbp - 0xa0], 0x2710
0xa0a63b file0xa0e63b c5fb108d78ffffff vmovsd xmm1, qword ptr [rbp - 0x88]
0xa0a643 file0xa0e643 c5f3588d58ffffff vaddsd xmm1, xmm1, qword ptr [rbp - 0xa8]
0xa0a64b file0xa0e64b 4c8d6d98 lea r13, [rbp - 0x68]
0xa0a64f file0xa0e64f c5e857d2 vxorps xmm2, xmm2, xmm2
0xa0a653 file0xa0e653 c5eb2ac0 vcvtsi2sd xmm0, xmm2, eax
0xa0a657 file0xa0e657 c5fb580561cb0307 vaddsd xmm0, xmm0, qword ptr [rip + 0x703cb61] rip->0x7a471c0
0xa0a65f file0xa0e65f c5fb118d60ffffff vmovsd qword ptr [rbp - 0xa0], xmm1
0xa0a667 file0xa0e667 c5fa7ec0 vmovq xmm0, xmm0
0xa0a66b file0xa0e66b c4e37909c001 vroundpd xmm0, xmm0, 1
0xa0a671 file0xa0e671 c461fb2ce0 vcvttsd2si r12, xmm0
0xa0a676 file0xa0e676 eb10 jmp 0xa0a688
0xa0a680 file0xa0e680 84db test bl, bl
0xa0a682 file0xa0e682 0f856e020000 jne 0xa0a8f6
0xa0a688 file0xa0e688 803d75798d0800 cmp byte ptr [rip + 0x88d7975], 0 rip->0x92e2004
0xa0a68f file0xa0e68f 0f8436010000 je 0xa0a7cb
0xa0a695 file0xa0e695 e8f6e96aff call 0xb9090
0xa0a69a file0xa0e69a 84c0 test al, al
0xa0a69c file0xa0e69c 741e je 0xa0a6bc
0xa0a69e file0xa0e69e 488d0567e39408 lea rax, [rip + 0x894e367] rip->0x9358a0c
0xa0a6a5 file0xa0e6a5 803800 cmp byte ptr [rax], 0
0xa0a6a8 file0xa0e6a8 7512 jne 0xa0a6bc
0xa0a6aa file0xa0e6aa e8d1a67cff call 0x1d4d80
0xa0a6af file0xa0e6af 4889c7 mov rdi, rax
0xa0a6b2 file0xa0e6b2 be01000000 mov esi, 1
0xa0a6b7 file0xa0e6b7 e8c4ad7dff call 0x1e5480
0xa0a6bc file0xa0e6bc 80bd74ffffff00 cmp byte ptr [rbp - 0x8c], 0
0xa0a6c3 file0xa0e6c3 7413 je 0xa0a6d8
0xa0a6c5 file0xa0e6c5 e8f66863ff call 0x40fc0
0xa0a6ca file0xa0e6ca 488b08 mov rcx, qword ptr [rax]
0xa0a6cd file0xa0e6cd 4889c7 mov rdi, rax
0xa0a6d0 file0xa0e6d0 be01000000 mov esi, 1
0xa0a6d5 file0xa0e6d5 ff5150 call qword ptr [rcx + 0x50]
0xa0a6d8 file0xa0e6d8 4c89e0 mov rax, r12
0xa0a6db file0xa0e6db 48b94b598638d6c56d34 movabs rcx, 0x346dc5d63886594b
0xa0a6e5 file0xa0e6e5 488b7d80 mov rdi, qword ptr [rbp - 0x80]
0xa0a6e9 file0xa0e6e9 48f7e9 imul rcx
0xa0a6ec file0xa0e6ec 4889d0 mov rax, rdx
0xa0a6ef file0xa0e6ef 48c1fa0b sar rdx, 0xb
0xa0a6f3 file0xa0e6f3 48c1e83f shr rax, 0x3f
0xa0a6f7 file0xa0e6f7 4801c2 add rdx, rax
0xa0a6fa file0xa0e6fa b8ffffffff mov eax, 0xffffffff
0xa0a6ff file0xa0e6ff 4839c2 cmp rdx, rax
0xa0a702 file0xa0e702 480f4dd0 cmovge rdx, rax
0xa0a706 file0xa0e706 4531f6 xor r14d, r14d
0xa0a709 file0xa0e709 4889d0 mov rax, rdx
0xa0a70c file0xa0e70c 48c1f83f sar rax, 0x3f
0xa0a710 file0xa0e710 c4e278f2f2 andn esi, eax, edx
0xa0a715 file0xa0e715 488b07 mov rax, qword ptr [rdi]
0xa0a718 file0xa0e718 31d2 xor edx, edx
0xa0a71a file0xa0e71a ff5020 call qword ptr [rax + 0x20]
0xa0a71d file0xa0e71d 4c89ef mov rdi, r13
0xa0a720 file0xa0e720 89c3 mov ebx, eax
0xa0a722 file0xa0e722 e849ee6aff call 0xb9570
0xa0a727 file0xa0e727 4c89ef mov rdi, r13
0xa0a72a file0xa0e72a e871ee6aff call 0xb95a0
0xa0a72f file0xa0e72f c5f92e8560ffffff vucomisd xmm0, qword ptr [rbp - 0xa0]
0xa0a737 file0xa0e737 7210 jb 0xa0a749
0xa0a739 file0xa0e739 e832f96aff call 0xba070
0xa0a73e file0xa0e73e 4889c7 mov rdi, rax
0xa0a741 file0xa0e741 e8ba186bff call 0xbc000
0xa0a746 file0xa0e746 4189c6 mov r14d, eax
0xa0a749 file0xa0e749 e892595fff call 0xe0
0xa0a74e file0xa0e74e 4108c7 or r15b, al
0xa0a751 file0xa0e751 89d9 mov ecx, ebx
0xa0a753 file0xa0e753 4408f9 or cl, r15b
0xa0a756 file0xa0e756 f6c101 test cl, 1
0xa0a759 file0xa0e759 0f8521ffffff jne 0xa0a680
0xa0a75f file0xa0e75f 4584f6 test r14b, r14b
0xa0a762 file0xa0e762 0f8418ffffff je 0xa0a680
0xa0a768 file0xa0e768 803d4d839f0800 cmp byte ptr [rip + 0x89f834d], 0 rip->0x9402abc
0xa0a76f file0xa0e76f 0f850bffffff jne 0xa0a680
0xa0a775 file0xa0e775 8b057d5f9f08 mov eax, dword ptr [rip + 0x89f5f7d] rip->0x94006f8
0xa0a77b file0xa0e77b 85c0 test eax, eax
0xa0a77d file0xa0e77d 0f8ffdfeffff jg 0xa0a680
0xa0a783 file0xa0e783 4c89ef mov rdi, r13
0xa0a786 file0xa0e786 e815ee6aff call 0xb95a0
0xa0a78b file0xa0e78b 488d050ae39408 lea rax, [rip + 0x894e30a] rip->0x9358a9c
0xa0a792 file0xa0e792 c5fb118578ffffff vmovsd qword ptr [rbp - 0x88], xmm0
0xa0a79a file0xa0e79a 8b38 mov edi, dword ptr [rax]
0xa0a79c file0xa0e79c 89bd40ffffff mov dword ptr [rbp - 0xc0], edi
0xa0a7a2 file0xa0e7a2 e839bb0100 call 0xa262e0
0xa0a7a7 file0xa0e7a7 48c745c000000000 mov qword ptr [rbp - 0x40], 0
0xa0a7af file0xa0e7af 488b08 mov rcx, qword ptr [rax]
0xa0a7b2 file0xa0e7b2 4c637008 movsxd r14, dword ptr [rax + 8]
0xa0a7b6 file0xa0e7b6 448975c8 mov dword ptr [rbp - 0x38], r14d
0xa0a7ba file0xa0e7ba 4d85f6 test r14, r14
0xa0a7bd file0xa0e7bd 7565 jne 0xa0a824
0xa0a7bf file0xa0e7bf c745cc00000000 mov dword ptr [rbp - 0x34], 0
0xa0a7c6 file0xa0e7c6 e99f000000 jmp 0xa0a86a
0xa0a7cb file0xa0e7cb 488d0d3ae29408 lea rcx, [rip + 0x894e23a] rip->0x9358a0c
0xa0a7d2 file0xa0e7d2 833d0f5e9f0800 cmp dword ptr [rip + 0x89f5e0f], 0 rip->0x94005e8
0xa0a7d9 file0xa0e7d9 488d0580619408 lea rax, [rip + 0x8946180] rip->0x9350960
0xa0a7e0 file0xa0e7e0 c60100 mov byte ptr [rcx], 0
0xa0a7e3 file0xa0e7e3 488d0db64dc206 lea rcx, [rip + 0x6c24db6] rip->0x762f5a0
0xa0a7ea file0xa0e7ea 66c7000000 mov word ptr [rax], 0
0xa0a7ef file0xa0e7ef 7407 je 0xa0a7f8
0xa0a7f1 file0xa0e7f1 488b0de85d9f08 mov rcx, qword ptr [rip + 0x89f5de8] rip->0x94005e0
0xa0a7f8 file0xa0e7f8 488d3dd7480a07 lea rdi, [rip + 0x70a48d7] rip->0x7aaf0d6
0xa0a7ff file0xa0e7ff be71030000 mov esi, 0x371
0xa0a804 file0xa0e804 488d1519c0b506 lea rdx, [rip + 0x6b5c019] rip->0x7566824
0xa0a80b file0xa0e80b 31c0 xor eax, eax
0xa0a80d file0xa0e80d e89ea876ff call 0x1750b0
0xa0a812 file0xa0e812 e879e86aff call 0xb9090
0xa0a817 file0xa0e817 84c0 test al, al
0xa0a819 file0xa0e819 0f857ffeffff jne 0xa0a69e
0xa0a81f file0xa0e81f e998feffff jmp 0xa0a6bc
0xa0a824 file0xa0e824 488d45cc lea rax, [rbp - 0x34]
0xa0a828 file0xa0e828 bf02000000 mov edi, 2
0xa0a82d file0xa0e82d be02000000 mov esi, 2
0xa0a832 file0xa0e832 4489f2 mov edx, r14d
0xa0a835 file0xa0e835 4c8d45c0 lea r8, [rbp - 0x40]
0xa0a839 file0xa0e839 4589f1 mov r9d, r14d
0xa0a83c file0xa0e83c 48898d68ffffff mov qword ptr [rbp - 0x98], rcx
0xa0a843 file0xa0e843 31c9 xor ecx, ecx
0xa0a845 file0xa0e845 48890424 mov qword ptr [rsp], rax
0xa0a849 file0xa0e849 e882cd61ff call 0x275d0
0xa0a84e file0xa0e84e 488b7dc0 mov rdi, qword ptr [rbp - 0x40]
0xa0a852 file0xa0e852 488bb568ffffff mov rsi, qword ptr [rbp - 0x98]
0xa0a859 file0xa0e859 4d01f6 add r14, r14
0xa0a85c file0xa0e85c 4c89f2 mov rdx, r14
0xa0a85f file0xa0e85f e8ec75a606 call 0x7471e50
0xa0a864 file0xa0e864 837dc802 cmp dword ptr [rbp - 0x38], 2
0xa0a868 file0xa0e868 7d47 jge 0xa0a8b1
0xa0a86a file0xa0e86a 8b9540ffffff mov edx, dword ptr [rbp - 0xc0]
0xa0a870 file0xa0e870 488d7db0 lea rdi, [rbp - 0x50]
0xa0a874 file0xa0e874 488d3539ace906 lea rsi, [rip + 0x6e9ac39] rip->0x78a54b4
0xa0a87b file0xa0e87b 31c0 xor eax, eax
0xa0a87d file0xa0e87d e8fe1166ff call 0x6ba80
0xa0a882 file0xa0e882 488b7dc0 mov rdi, qword ptr [rbp - 0x40]
0xa0a886 file0xa0e886 4885ff test rdi, rdi
0xa0a889 file0xa0e889 7405 je 0xa0a890
