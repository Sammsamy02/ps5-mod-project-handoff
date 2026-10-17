#pragma once
#include <Zydis/Zydis.h>
namespace RC::ASM
{
    struct Instruction { void* address; ZydisDecodedInstruction raw; ZydisDecodedOperand* operands; };
    auto get_first_instruction_at_address(void* p, ZydisDecodedOperand (&operands)[ZYDIS_MAX_OPERAND_COUNT]) -> Instruction;
    auto resolve_absolute_address(void* p) -> void*;
    auto resolve_jmp(void* p) -> void*;
    auto resolve_call(void* p) -> void*;
    auto resolve_function_address_from_potential_jmp(void* p) -> void*;
}
