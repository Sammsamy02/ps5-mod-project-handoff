0x885400  (f 0x485400)  55                     push rbp
0x885401  (f 0x485401)  4889e5                 mov rbp, rsp
0x885404  (f 0x485404)  488d0515a2a707         lea rax, [rip + 0x7a7a215]  ; rip->0x82ff620 (file 0x7eff620)
0x88540b  (f 0x48540b)  c5f857c0               vxorps xmm0, xmm0, xmm0
0x88540f  (f 0x48540f)  488907                 mov qword ptr [rdi], rax
0x885412  (f 0x485412)  897708                 mov dword ptr [rdi + 8], esi
0x885415  (f 0x485415)  c7470cffffffff         mov dword ptr [rdi + 0xc], 0xffffffff
0x88541c  (f 0x48541c)  c5f8114710             vmovups xmmword ptr [rdi + 0x10], xmm0
0x885421  (f 0x485421)  48c7472000000000       mov qword ptr [rdi + 0x20], 0
0x885429  (f 0x485429)  5d                     pop rbp
0x88542a  (f 0x48542a)  c3                     ret 
--- calls: []
