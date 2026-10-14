#include <ASMHelper/ASMHelper.hpp>
#include <DynamicOutput/Output.hpp>
#include <cstdio>
#include <cstring>
#include <stdexcept>
// buffer: [0] jmp rel32 -> target at +16; [16] plain 'push rbp' body
alignas(64) static unsigned char code[64];
int main()
{
    std::memset(code, 0x90, sizeof code);
    code[0] = 0xE9; int32_t rel = 16 - 5; std::memcpy(code + 1, &rel, 4);
    code[16] = 0x55;
    void* stub = code; void* body = code + 16;
    try
    {
        void* r1 = RC::ASM::resolve_function_address_from_potential_jmp(stub);
        void* r2 = RC::ASM::resolve_function_address_from_potential_jmp(body);
        void* r3 = RC::ASM::resolve_function_address_from_potential_jmp(nullptr);
        std::printf("RESULT stub->%s body->%s null->%s\n",
                    r1 == body ? "body(followed)" : r1 == stub ? "stub(as-is)" : "other",
                    r2 == body ? "body" : "other", r3 == nullptr ? "null" : "other");
    }
    catch (const std::exception& e)
    {
        std::printf("RESULT threw: %s\n", e.what());
    }
    try
    {
        RC::ASM::resolve_jmp(stub);
        std::printf("RESULT resolve_jmp: no throw\n");
    }
    catch (const std::exception& e)
    {
        std::printf("RESULT resolve_jmp threw: %.60s\n", e.what());
    }
}
