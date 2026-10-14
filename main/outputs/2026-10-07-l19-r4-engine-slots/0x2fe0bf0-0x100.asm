0x2fe0bf0: 55                       push rbp
0x2fe0bf1: 4889e5                   mov rbp, rsp
0x2fe0bf4: e80766b5ff               call 0x2b37200
0x2fe0bf9: 85c0                     test eax, eax
0x2fe0bfb: 0f94c0                   sete al
0x2fe0bfe: 5d                       pop rbp
0x2fe0bff: c3                       ret 
0x2fe0c00: 55                       push rbp
0x2fe0c01: 4889e5                   mov rbp, rsp
0x2fe0c04: 53                       push rbx
0x2fe0c05: 50                       push rax
0x2fe0c06: 4889fb                   mov rbx, rdi
0x2fe0c09: e862e50800               call 0x306f170
0x2fe0c0e: c5fb1005daeaa504         vmovsd xmm0, qword ptr [rip + 0x4a5eada] ; ['쳍㵌馚㸙']
0x2fe0c16: c5fc101dc21ca704         vmovups ymm3, ymmword ptr [rip + 0x4a71cc2]
0x2fe0c1e: c5f8100d3a18a604         vmovups xmm1, xmmword ptr [rip + 0x4a6183a]
0x2fe0c26: 488d05439b2505           lea rax, [rip + 0x5259b43]
0x2fe0c2d: c5fb1015db21a604         vmovsd xmm2, qword ptr [rip + 0x4a621db]
0x2fe0c35: 4883c010                 add rax, 0x10
0x2fe0c39: 488903                   mov qword ptr [rbx], rax
0x2fe0c3c: 48b8c8000000f4010000     movabs rax, 0x1f4000000c8
0x2fe0c46: 48c7834803000000000000   mov qword ptr [rbx + 0x348], 0
0x2fe0c51: c5fb1183a8020000         vmovsd qword ptr [rbx + 0x2a8], xmm0
0x2fe0c59: 488983b0020000           mov qword ptr [rbx + 0x2b0], rax
0x2fe0c60: c5fc119be8020000         vmovups ymmword ptr [rbx + 0x2e8], ymm3
0x2fe0c68: c5f8118b08030000         vmovups xmmword ptr [rbx + 0x308], xmm1
0x2fe0c70: c5f8100dd8dfa504         vmovups xmm1, xmmword ptr [rip + 0x4a5dfd8]
0x2fe0c78: 48b8b9fc873c10270000     movabs rax, 0x27103c87fcb9
0x2fe0c82: 48898318030000           mov qword ptr [rbx + 0x318], rax
0x2fe0c89: c783200300000a000000     mov dword ptr [rbx + 0x320], 0xa
0x2fe0c93: c6833003000000           mov byte ptr [rbx + 0x330], 0
0x2fe0c9a: c5fb119324030000         vmovsd qword ptr [rbx + 0x324], xmm2
0x2fe0ca2: c7832c03000000007a44     mov dword ptr [rbx + 0x32c], 0x447a0000
0x2fe0cac: 66c783310300000000       mov word ptr [rbx + 0x331], 0
0x2fe0cb5: c5f8118b34030000         vmovups xmmword ptr [rbx + 0x334], xmm1
0x2fe0cbd: 66c783440300000001       mov word ptr [rbx + 0x344], 0x100
0x2fe0cc6: 4883c408                 add rsp, 8
0x2fe0cca: 5b                       pop rbx
0x2fe0ccb: 5d                       pop rbp
0x2fe0ccc: c3                       ret 
0x2fe0ccd: cc                       int3 
0x2fe0cce: cc                       int3 
0x2fe0ccf: cc                       int3 
0x2fe0cd0: 55                       push rbp
0x2fe0cd1: 4889e5                   mov rbp, rsp
0x2fe0cd4: 4156                     push r14
0x2fe0cd6: 53                       push rbx
0x2fe0cd7: 4189f6                   mov r14d, esi
0x2fe0cda: 4889fb                   mov rbx, rdi
0x2fe0cdd: e8be95b3ff               call 0x2b1a2a0
0x2fe0ce2: 488b4038                 mov rax, qword ptr [rax + 0x38]
0x2fe0ce6: 4885c0                   test rax, rax
0x2fe0ce9: 746b                     je 0x2fe0d56
