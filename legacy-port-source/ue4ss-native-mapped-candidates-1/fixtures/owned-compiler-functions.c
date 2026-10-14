__attribute__((noinline,aligned(64))) int owned_a(int n) { volatile int a[4]={n,n+1,n+2,n+3}; return a[n&3]; }
__attribute__((noinline,aligned(64))) int owned_b(int n) { return owned_a(n)+1; }
int main(void){return owned_b(2);}
