#include <machine/sysarch.h>
int main() { void* saved = nullptr; if (sysarch(AMD64_GET_GSBASE, &saved) != 0) return 1; return sysarch(AMD64_SET_GSBASE, &saved); }
