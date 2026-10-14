0x5d4680  (f 0x1d4680)  55                     push rbp
0x5d4681  (f 0x1d4681)  4889e5                 mov rbp, rsp
0x5d4684  (f 0x1d4684)  488d05f545cf07         lea rax, [rip + 0x7cf45f5]  ; rip->0x82c8c80 (file 0x7ec8c80)
0x5d468b  (f 0x1d468b)  488907                 mov qword ptr [rdi], rax
0x5d468e  (f 0x1d468e)  66c747080001           mov word ptr [rdi + 8], 0x100
0x5d4694  (f 0x1d4694)  5d                     pop rbp
0x5d4695  (f 0x1d4695)  c3                     ret 
--- calls: []
