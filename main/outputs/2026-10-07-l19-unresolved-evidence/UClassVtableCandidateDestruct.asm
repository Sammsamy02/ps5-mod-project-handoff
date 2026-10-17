0x722800  (f 0x322800)  55                     push rbp
0x722801  (f 0x322801)  4889e5                 mov rbp, rsp
0x722804  (f 0x322804)  53                     push rbx
0x722805  (f 0x322805)  50                     push rax
0x722806  (f 0x322806)  4889fb                 mov rbx, rdi
0x722809  (f 0x322809)  e8222d1600             call 0x885530
0x72280e  (f 0x32280e)  488b4340               mov rax, qword ptr [rbx + 0x40]
0x722812  (f 0x322812)  4885c0                 test rax, rax
0x722815  (f 0x322815)  741f                   je 0x722836
0x722817  (f 0x322817)  b95e87a34a             mov ecx, 0x4aa3875e
0x72281c  (f 0x32281c)  2388d4000000           and ecx, dword ptr [rax + 0xd4]
0x722822  (f 0x322822)  098bd4000000           or dword ptr [rbx + 0xd4], ecx
0x722828  (f 0x322828)  488b80d8000000         mov rax, qword ptr [rax + 0xd8]
0x72282f  (f 0x32282f)  480983d8000000         or qword ptr [rbx + 0xd8], rax
0x722836  (f 0x322836)  4883c408               add rsp, 8
0x72283a  (f 0x32283a)  5b                     pop rbx
0x72283b  (f 0x32283b)  5d                     pop rbp
0x72283c  (f 0x32283c)  c3                     ret 
--- calls: [('0xb22809', '0x885530')]
