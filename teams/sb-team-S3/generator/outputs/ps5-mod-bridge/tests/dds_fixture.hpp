#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace dds_fixture {
inline void u32(std::vector<std::uint8_t>& b,std::size_t p,std::uint32_t v) {
    for(unsigned i=0;i<4;++i)b.at(p+i)=static_cast<std::uint8_t>(v>>(8*i));
}
// Payload lengths are supplied as hand-calculated literals by each test.
inline std::vector<std::uint8_t> legacy(std::uint32_t w=4,std::uint32_t h=4,
    std::string code="DXT1",std::size_t payload=8,std::uint32_t mips=1) {
    std::vector<std::uint8_t> b(128+payload);
    b[0]='D';b[1]='D';b[2]='S';b[3]=' ';
    u32(b,4,124);u32(b,12,h);u32(b,16,w);u32(b,28,mips);
    u32(b,76,32);u32(b,80,4);
    for(unsigned i=0;i<4;++i)b[84+i]=code.at(i);
    for(std::size_t i=128;i<b.size();++i)b[i]=static_cast<std::uint8_t>(i);
    return b;
}
inline std::vector<std::uint8_t> dx10(std::uint32_t format=71,
    std::uint32_t w=4,std::uint32_t h=4,std::size_t payload=8,
    std::uint32_t mips=1,std::uint32_t dimension=3,std::uint32_t array=1,
    std::uint32_t misc=0,std::uint32_t alpha=0) {
    auto b=legacy(w,h,"DX10",payload+20,mips);
    u32(b,128,format);u32(b,132,dimension);u32(b,136,misc);
    u32(b,140,array);u32(b,144,alpha);
    return b;
}
}
