__attribute__((weak)) int weak_target(int x) { return x + 400; }
int host_weak_caller(void) { return weak_target(1); }
