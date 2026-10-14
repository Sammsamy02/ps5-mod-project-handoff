#include "core_bridge.h"
#include <cassert>
#include <cstdlib>
#include <new>
static bool fail_allocation=false;
void* operator new(std::size_t n) { if(fail_allocation) throw std::bad_alloc(); if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc(); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
int main() {
    char out[2048]={'x'};std::size_t n=99; fail_allocation=true;
    assert(mb_core_selftest(out,sizeof(out),&n)==MB_CORE_INTERNAL_ERROR && !n && !out[0]);
    out[0]='x';n=99;
    assert(mb_core_inspect_dds(nullptr,0,0,out,sizeof(out),&n)==MB_CORE_INTERNAL_ERROR && !n && !out[0]);
    fail_allocation=false;
    assert(mb_core_selftest(out,sizeof(out),&n)==MB_CORE_OK && n>0);
}
