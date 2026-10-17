struct Pair8 { int first; int second; };
extern "C" __attribute__((noinline)) Pair8 make_pair8(int a, int b) { return {a,b}; }
extern "C" __attribute__((noinline)) int consume_pair8(Pair8 p) { return p.first + p.second; }
