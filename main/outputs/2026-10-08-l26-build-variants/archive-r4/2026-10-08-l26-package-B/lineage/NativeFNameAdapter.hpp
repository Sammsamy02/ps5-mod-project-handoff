#pragma once
#include <cstddef>
#include <cstdint>

namespace rc::fnameabi {
// Wire object, not a compiler-generated aggregate-return signature.
struct alignas(8) Name { uint32_t index=0, number=0; };
static_assert(sizeof(Name)==8 && alignof(Name)==8);
static_assert(sizeof(char16_t)==2);
using Ctor = void (*)(Name* out, const char16_t* name, uint32_t mode);
constexpr size_t kMaxUnits=1024;
struct Result { bool ok=false; Name value{}; const char* reason="unavailable constructor"; };

// Internal UE4SS strings retain their existing ABI. Only explicit UTF16
// buffers cross this game boundary, even on a host with 4-byte wchar_t.
template<class Char> bool utf16(const Char* source,char16_t* out,size_t capacity) {
    static_assert(sizeof(Char)==2 || sizeof(Char)==4,"wide input required");
    if(!out || !capacity)return false;
    out[0]=0; if(!source)return true;
    size_t used=0;
    for(size_t i=0;i<kMaxUnits;++i) {
        uint32_t cp=static_cast<uint32_t>(source[i]);
        if(!cp){out[used]=0;return true;}
        if constexpr(sizeof(Char)==2) {
            if(cp>=0xd800 && cp<=0xdbff) {
                if(i+1>=kMaxUnits)return false;
                const uint32_t lo=static_cast<uint32_t>(source[++i]);
                if(lo<0xdc00 || lo>0xdfff)return false;
                cp=0x10000+((cp-0xd800)<<10)+(lo-0xdc00);
            } else if(cp>=0xdc00 && cp<=0xdfff)return false;
        } else if(cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return false;
        const size_t need=cp>=0x10000?2:1;
        if(used+need>=capacity)return false;
        if(need==2){cp-=0x10000;out[used++]=static_cast<char16_t>(0xd800+(cp>>10));out[used++]=static_cast<char16_t>(0xdc00+(cp&0x3ff));}
        else out[used++]=static_cast<char16_t>(cp);
    }
    return false; // bounded unterminated input, never a truncated game name
}
template<class Char> Result invoke(Ctor ctor,const Char* input,uint32_t mode) {
    Result r;
    if(!ctor)return r;
    if(mode>1){r.reason="invalid EFindName";return r;}
    char16_t text[kMaxUnits+1]{};
    if(!utf16(input,text,kMaxUnits+1)){r.reason="invalid or oversized UTF16 input";return r;}
    // Native wide/no-length constructor: RDI=out, RSI=UTF16, EDX=mode.
    // RAX is ignored, regardless of incidental/non-void callee behavior.
    ctor(&r.value,text,mode);
    r.ok=true;r.reason="native output constructor returned";return r;
}
} // namespace rc::fnameabi
