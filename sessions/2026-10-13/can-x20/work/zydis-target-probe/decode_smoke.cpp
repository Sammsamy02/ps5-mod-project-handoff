#include <Zydis.h>
#include <cstdio>

int main()
{
    const unsigned char code[] = {0x48, 0x8b, 0x05, 0x12, 0x34, 0x00, 0x00};
    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64))) return 2;
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
            &decoder, code, sizeof(code), &instruction, operands))) return 3;
    std::printf("length=%u category=%u rip-base=%d\n", instruction.length,
                instruction.meta.category, operands[1].mem.base == ZYDIS_REGISTER_RIP);
    return instruction.length == sizeof(code) &&
                   operands[1].mem.base == ZYDIS_REGISTER_RIP ? 0 : 1;
}
