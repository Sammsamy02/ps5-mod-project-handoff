0x2fde490: 55                       push rbp
0x2fde491: 4889e5                   mov rbp, rsp
0x2fde494: 4157                     push r15
0x2fde496: 4156                     push r14
0x2fde498: 4155                     push r13
0x2fde49a: 4154                     push r12
0x2fde49c: 53                       push rbx
0x2fde49d: 4883e4e0                 and rsp, 0xffffffffffffffe0
0x2fde4a1: 4881ec80000000           sub rsp, 0x80
0x2fde4a8: 4c8b2d593a1506           mov r13, qword ptr [rip + 0x6153a59]
0x2fde4af: 4989fc                   mov r12, rdi
0x2fde4b2: 4889cf                   mov rdi, rcx
0x2fde4b5: 4989f6                   mov r14, rsi
0x2fde4b8: 31f6                     xor esi, esi
0x2fde4ba: 4989cf                   mov r15, rcx
0x2fde4bd: 4889d3                   mov rbx, rdx
0x2fde4c0: 498b4500                 mov rax, qword ptr [r13]
0x2fde4c4: 4889442460               mov qword ptr [rsp + 0x60], rax
0x2fde4c9: e8526308fd               call 0x64820
0x2fde4ce: 4c89e7                   mov rdi, r12
0x2fde4d1: 4c89f6                   mov rsi, r14
0x2fde4d4: 4889da                   mov rdx, rbx
0x2fde4d7: e8f4adffff               call 0x2fd92d0
0x2fde4dc: 4889c3                   mov rbx, rax
0x2fde4df: 4885c0                   test rax, rax
0x2fde4e2: 0f84bc000000             je 0x2fde5a4
0x2fde4e8: 4c8ba3b8010000           mov r12, qword ptr [rbx + 0x1b8]
0x2fde4ef: 4d85e4                   test r12, r12
0x2fde4f2: 0f84ee000000             je 0x2fde5e6
0x2fde4f8: c4c17d108424e0010000     vmovupd ymm0, ymmword ptr [r12 + 0x1e0]
0x2fde502: c5fd11442420             vmovupd ymmword ptr [rsp + 0x20], ymm0
0x2fde508: 4d8bbc24d0010000         mov r15, qword ptr [r12 + 0x1d0]
0x2fde510: 4d85ff                   test r15, r15
0x2fde513: 0f847f010000             je 0x2fde698
0x2fde519: c4c17dc20704             vcmpneqpd ymm0, ymm0, ymmword ptr [r15]
0x2fde51f: c4e27d0fc0               vtestpd ymm0, ymm0
0x2fde524: 742f                     je 0x2fde555
0x2fde526: c5fc10442420             vmovups ymm0, ymmword ptr [rsp + 0x20]
0x2fde52c: 488d7c2440               lea rdi, [rsp + 0x40]
0x2fde531: 488d742420               lea rsi, [rsp + 0x20]
0x2fde536: c4c17c1107               vmovups ymmword ptr [r15], ymm0
0x2fde53b: e8c03a17fd               call 0x152000
0x2fde540: 488b442450               mov rax, qword ptr [rsp + 0x50]
0x2fde545: 49894730                 mov qword ptr [r15 + 0x30], rax
0x2fde549: c5f910442440             vmovupd xmm0, xmmword ptr [rsp + 0x40]
0x2fde54f: c4c179114720             vmovupd xmmword ptr [r15 + 0x20], xmm0
0x2fde555: c4c17b104728             vmovsd xmm0, qword ptr [r15 + 0x28]
0x2fde55b: 488b83b8010000           mov rax, qword ptr [rbx + 0x1b8]
0x2fde562: 48c744242000000000       mov qword ptr [rsp + 0x20], 0
0x2fde56b: c5fb11442428             vmovsd qword ptr [rsp + 0x28], xmm0
0x2fde571: 48c744243000000000       mov qword ptr [rsp + 0x30], 0
0x2fde57a: 4885c0                   test rax, rax
0x2fde57d: 0f8487000000             je 0x2fde60a
0x2fde583: c5f8108000020000         vmovups xmm0, xmmword ptr [rax + 0x200]
0x2fde58b: c5fb108810020000         vmovsd xmm1, qword ptr [rax + 0x210]
0x2fde593: c5f811442440             vmovups xmmword ptr [rsp + 0x40], xmm0
0x2fde599: c5fb114c2450             vmovsd qword ptr [rsp + 0x50], xmm1
0x2fde59f: e980000000               jmp 0x2fde624
0x2fde5a4: 4c8d742420               lea r14, [rsp + 0x20]
0x2fde5a9: 488d35cc758d04           lea rsi, [rip + 0x48d75cc] ; ['Could not find a starting spot']
0x2fde5b0: 31c0                     xor eax, eax
0x2fde5b2: 4c89f7                   mov rdi, r14
0x2fde5b5: e8c6d408fd               call 0x6ba80
0x2fde5ba: 4d39fe                   cmp r14, r15
0x2fde5bd: 0f84c4000000             je 0x2fde687
0x2fde5c3: 498b3f                   mov rdi, qword ptr [r15]
0x2fde5c6: 4885ff                   test rdi, rdi
0x2fde5c9: 7405                     je 0x2fde5d0
0x2fde5cb: e8c0940dfd               call 0xb7a90
0x2fde5d0: 488b442420               mov rax, qword ptr [rsp + 0x20]
0x2fde5d5: 498907                   mov qword ptr [r15], rax
0x2fde5d8: 488b442428               mov rax, qword ptr [rsp + 0x28]
0x2fde5dd: 49894708                 mov qword ptr [r15 + 8], rax
0x2fde5e1: e981000000               jmp 0x2fde667
0x2fde5e6: 488d057bf93406           lea rax, [rip + 0x634f97b]
0x2fde5ed: 48c744242000000000       mov qword ptr [rsp + 0x20], 0
0x2fde5f6: c5fb104008               vmovsd xmm0, qword ptr [rax + 8]
0x2fde5fb: c5fb11442428             vmovsd qword ptr [rsp + 0x28], xmm0
0x2fde601: 48c744243000000000       mov qword ptr [rsp + 0x30], 0
