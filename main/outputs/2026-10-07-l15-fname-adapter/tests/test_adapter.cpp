#include "NativeFNameAdapter.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <bit>
#include <sys/wait.h>
#include <unistd.h>
namespace a = rc::fnameabi;
static unsigned calls;
static uint32_t expected_mode;
static void ctor(a::Name* out, const char16_t* text, uint32_t mode) {
    assert(reinterpret_cast<uintptr_t>(out)%8==0);
    assert(out->index==0 && out->number==0);
    assert(text[0]==u'B' && text[1]==u'y' && text[11]==u'y' && text[12]==0);
    assert(mode==expected_mode); ++calls; out->index=3;
}
#if !defined(SKIP_LEGACY_NEGATIVE)
static void legacy_detector(a::Name*,const char16_t* text,uint32_t) {
    // Wrong ABI is exercised in an isolated child. Do not dereference bad args.
    if (!text) _Exit(77);
    _Exit(0);
}
#endif
int main() {
    static_assert(sizeof(a::Name)==8 && alignof(a::Name)==8);
    static_assert(sizeof(char16_t)==2);
    auto n=a::invoke(ctor,u"ByteProperty",0);
    assert(n.ok && n.value.index==3 && n.value.number==0 && calls==1);
    auto w=a::invoke(ctor,L"ByteProperty",0);
    assert(w.ok && w.value.index==3 && calls==2);
    expected_mode=1;auto add=a::invoke(ctor,u"ByteProperty",1);
    assert(add.ok && add.value.index==3 && calls==3);expected_mode=0;
    assert(!a::invoke(ctor,u"ByteProperty",2).ok && calls==3);
    char16_t buf[16]{};
    assert(a::utf16(U"A\U0001F600",buf,16) && buf[0]==u'A' && buf[1]==0xd83d && buf[2]==0xde00 && buf[3]==0);
    assert(a::utf16(u"A\U0001F600",buf,16) && buf[1]==0xd83d && buf[2]==0xde00);
    const char32_t invalid[]={0x110000,0}; assert(!a::utf16(invalid,buf,16));
    const char16_t surrogate[]={0xd800,0}; assert(!a::utf16(surrogate,buf,16));
    assert(!a::utf16(u"long",buf,3));
    assert(!a::invoke(nullptr,u"ByteProperty",0).ok && calls==3);
#if !defined(SKIP_LEGACY_NEGATIVE)
    pid_t p=fork();assert(p>=0);
    if(!p) { using Old=a::Name(*)(const char16_t*,uint32_t);
        auto wrong=std::bit_cast<Old>(&legacy_detector);(void)wrong(u"ByteProperty",0);_Exit(0); }
    int status=0;assert(waitpid(p,&status,0)==p);
    assert((WIFEXITED(status)&&WEXITSTATUS(status)==77)||WIFSIGNALED(status));
#endif
    std::puts("PASS: native output ABI; zero/alignment; mode; UTF16 and UTF32; invalid/overflow; legacy rejected");
}
