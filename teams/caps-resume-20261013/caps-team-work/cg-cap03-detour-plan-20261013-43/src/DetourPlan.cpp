#include "DetourPlan.hpp"

#include <Zydis.h>

#include <algorithm>
#include <climits>
#include <cstring>

namespace cap03 {
namespace {

bool add_address(uint64_t base, uint64_t increment, uint64_t* result)
{
    if (base > UINT64_MAX - increment) return false;
    *result = base + increment;
    return true;
}

bool spans_overlap(uint64_t left_begin, uint64_t left_end,
                   uint64_t right_begin, uint64_t right_end)
{
    return left_begin < right_end && right_begin < left_end;
}

void absolute_jump(uint8_t* destination, uint64_t target)
{
    destination[0] = 0xff;
    destination[1] = 0x25;
    destination[2] = destination[3] = destination[4] = destination[5] = 0;
    for (unsigned i = 0; i < 8; ++i)
        destination[6 + i] = static_cast<uint8_t>(target >> (8 * i));
}

bool signed_delta(uint64_t destination, uint64_t next, unsigned bits,
                  int64_t* result)
{
    if (bits != 8 && bits != 16 && bits != 32) return false;
    const __int128 delta = static_cast<__int128>(destination) -
                           static_cast<__int128>(next);
    const __int128 lower = -(static_cast<__int128>(1) << (bits - 1));
    const __int128 upper = (static_cast<__int128>(1) << (bits - 1)) - 1;
    if (delta < lower || delta > upper) return false;
    *result = static_cast<int64_t>(delta);
    return true;
}

bool write_relative(uint8_t* instruction, unsigned length, unsigned offset,
                    unsigned bits, uint64_t destination, uint64_t next)
{
    if ((bits != 8 && bits != 16 && bits != 32) ||
        offset > length || bits / 8 > length - offset)
        return false;
    int64_t delta = 0;
    if (!signed_delta(destination, next, bits, &delta)) return false;
    for (unsigned i = 0; i < bits / 8; ++i)
        instruction[offset + i] = static_cast<uint8_t>(
            static_cast<uint64_t>(delta) >> (8 * i));
    return true;
}

bool is_instruction_start(size_t offset, const size_t* starts, size_t count,
                          size_t stolen_length)
{
    if (offset == stolen_length) return true;
    for (size_t i = 0; i < count; ++i)
        if (starts[i] == offset) return true;
    return false;
}

} // namespace

DetourPlanResult plan_detour(const uint8_t* original, size_t available,
                             uint64_t target_address,
                             uint64_t trampoline_address,
                             uint64_t handler_address,
                             DetourPlan* out)
{
    if (!out) return DetourPlanResult::BadArgs;
    *out = DetourPlan{};
    if (!original || !target_address || !trampoline_address || !handler_address)
        return DetourPlanResult::BadArgs;
    if (available < kAbsoluteJumpBytes) return DetourPlanResult::DecodeFailed;

    ZydisDecoder decoder;
    if (!ZYAN_SUCCESS(ZydisDecoderInit(
            &decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)))
        return DetourPlanResult::DecodeFailed;

    size_t starts[kMaxPatchBytes]{};
    size_t instruction_count = 0;
    size_t stolen = 0;
    while (stolen < kAbsoluteJumpBytes)
    {
        if (stolen >= kMaxPatchBytes || stolen >= available)
            return DetourPlanResult::DecodeFailed;
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        const size_t remaining = std::min(available - stolen,
                                          kMaxPatchBytes - stolen);
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &decoder, original + stolen, remaining,
                &instruction, operands)) || !instruction.length ||
            instruction.length > remaining)
            return DetourPlanResult::DecodeFailed;
        // A terminator before the overwrite is covered may be followed by
        // inline data or another function, which must not be stolen blindly.
        if ((instruction.meta.category == ZYDIS_CATEGORY_RET ||
             instruction.meta.category == ZYDIS_CATEGORY_UNCOND_BR) &&
            stolen + instruction.length < kAbsoluteJumpBytes)
            return DetourPlanResult::RelocationUnsupported;
        starts[instruction_count++] = stolen;
        stolen += instruction.length;
    }

    uint64_t target_end = 0;
    uint64_t trampoline_end = 0;
    const size_t trampoline_length = stolen + kAbsoluteJumpBytes;
    if (!add_address(target_address, stolen, &target_end) ||
        !add_address(trampoline_address, trampoline_length, &trampoline_end))
        return DetourPlanResult::AddressOverflow;
    if (spans_overlap(target_address, target_end,
                      trampoline_address, trampoline_end))
        return DetourPlanResult::Overlap;
    if (handler_address >= target_address && handler_address < target_end)
        return DetourPlanResult::Overlap;
    if (handler_address >= trampoline_address &&
        handler_address < trampoline_end)
        return DetourPlanResult::Overlap;

    DetourPlan draft{};
    draft.stolen_length = stolen;
    draft.trampoline_length = trampoline_length;
    std::memcpy(draft.trampoline.data(), original, stolen);
    draft.target_patch.fill(0x90);
    absolute_jump(draft.target_patch.data(), handler_address);
    absolute_jump(draft.trampoline.data() + stolen, target_end);

    for (size_t index = 0; index < instruction_count; ++index)
    {
        const size_t offset = starts[index];
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &decoder, original + offset, stolen - offset,
                &instruction, operands)))
            return DetourPlanResult::DecodeFailed;
        uint64_t original_ip = 0, relocated_ip = 0, relocated_next = 0;
        if (!add_address(target_address, offset, &original_ip) ||
            !add_address(trampoline_address, offset, &relocated_ip) ||
            !add_address(relocated_ip, instruction.length, &relocated_next))
            return DetourPlanResult::AddressOverflow;

        bool patched_memory = false;
        bool patched_immediates[2]{};
        for (unsigned operand_index = 0;
             operand_index < instruction.operand_count; ++operand_index)
        {
            const ZydisDecodedOperand& operand = operands[operand_index];
            const bool rip_memory = operand.type == ZYDIS_OPERAND_TYPE_MEMORY &&
                (operand.mem.base == ZYDIS_REGISTER_RIP ||
                 operand.mem.base == ZYDIS_REGISTER_EIP);
            const bool relative_imm = operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE &&
                                      operand.imm.is_relative;
            if (!rip_memory && !relative_imm) continue;
            // EIP-relative addressing wraps the effective base to 32 bits.
            // The 64-bit signed-delta writer below only models RIP.
            if (rip_memory && operand.mem.base == ZYDIS_REGISTER_EIP)
                return DetourPlanResult::RelocationUnsupported;
            uint64_t destination = 0;
            if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(
                    &instruction, &operand, original_ip, &destination)))
                return DetourPlanResult::RelocationUnsupported;
            // A branch to the first byte after the stolen prologue can land
            // on the trampoline's jump-back. A memory access at that byte
            // must still read the unchanged original location.
            const bool in_stolen_span = destination >= target_address &&
                (destination < target_end ||
                 (relative_imm && destination == target_end));
            if (rip_memory && in_stolen_span &&
                (operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE))
                return DetourPlanResult::RelocationUnsupported;
            if (relative_imm && in_stolen_span &&
                instruction.meta.category == ZYDIS_CATEGORY_CALL)
                return DetourPlanResult::RelocationUnsupported;
            // LEA computes an address; it must keep returning the original
            // address. A true memory access into stolen bytes instead reads
            // the copied bytes in the trampoline.
            const bool address_generation = rip_memory &&
                operand.mem.type == ZYDIS_MEMOP_TYPE_AGEN;
            if (in_stolen_span && !address_generation)
            {
                const size_t inside = static_cast<size_t>(destination - target_address);
                if (relative_imm &&
                    !is_instruction_start(inside, starts, instruction_count, stolen))
                    return DetourPlanResult::RelocationUnsupported;
                if (!add_address(trampoline_address, inside, &destination))
                    return DetourPlanResult::AddressOverflow;
            }

            unsigned field_offset = 0;
            unsigned field_bits = 0;
            if (rip_memory)
            {
                if (patched_memory) return DetourPlanResult::RelocationUnsupported;
                patched_memory = true;
                field_offset = instruction.raw.disp.offset;
                field_bits = instruction.raw.disp.size;
            }
            else
            {
                int raw_index = -1;
                for (int i = 0; i < 2; ++i)
                    if (instruction.raw.imm[i].is_relative && !patched_immediates[i])
                    { raw_index = i; break; }
                if (raw_index < 0) return DetourPlanResult::RelocationUnsupported;
                patched_immediates[raw_index] = true;
                field_offset = instruction.raw.imm[raw_index].offset;
                field_bits = instruction.raw.imm[raw_index].size;
            }
            if (!write_relative(draft.trampoline.data() + offset,
                                instruction.length, field_offset, field_bits,
                                destination, relocated_next))
                return DetourPlanResult::RelativeOutOfRange;
        }
    }
    *out = draft;
    return DetourPlanResult::Ok;
}

} // namespace cap03
