#include <Zydis/Zydis.h>
#include <polyhook2/Instruction.hpp>
#include <polyhook2/OwnedMemory.hpp>
#include <cassert>
#include <stdexcept>
#include <cstdio>
#include <limits>

static auto instruction(uint64_t address, PLH::Mode mode, bool indirect = true) {
    PLH::Instruction::Displacement displacement{};
    displacement.Absolute = address;
    return PLH::Instruction(0, displacement, 0, false, indirect,
        std::vector<uint8_t>{0xff, 0x25, 0, 0, 0, 0}, "jmp", "owned holder", mode);
}
static bool refused(const PLH::Instruction& i) {
    try { (void)i.getDestination(); return false; }
    catch(const std::runtime_error&) { return true; }
}
int main() {
    uint64_t holder = UINT64_C(0x12345678aabbccdd);
    const auto addr = reinterpret_cast<uint64_t>(&holder);
    {
        PLH::OwnedMemory::ScopedRegion region(&holder, sizeof(holder), PLH::R);
        assert(instruction(addr, PLH::Mode::x64).getDestination() == holder);
        PLH::Instruction::Displacement rip{}; rip.Relative = 10;
        PLH::Instruction indirect_relative(addr - 16, rip, 2, true, true,
            std::vector<uint8_t>{0xff, 0x25, 10, 0, 0, 0}, "jmp", "[rip+10]", PLH::Mode::x64);
        assert(indirect_relative.getDestination() == holder);
        assert(instruction(addr, PLH::Mode::x86).getDestination() == UINT32_C(0xaabbccdd));
    }
    assert(refused(instruction(addr, PLH::Mode::x64)));
    assert(refused(instruction(0, PLH::Mode::x64)));
    assert(refused(instruction(std::numeric_limits<uint64_t>::max() - 2, PLH::Mode::x64)));
    {
        PLH::OwnedMemory::ScopedRegion region(&holder, 3, PLH::R);
        assert(refused(instruction(addr, PLH::Mode::x86)));
    }
    {
        PLH::OwnedMemory::ScopedRegion region(&holder, sizeof(holder), PLH::W);
        assert(refused(instruction(addr, PLH::Mode::x64)));
    }
    holder = 0;
    {
        PLH::OwnedMemory::ScopedRegion region(&holder, sizeof(holder), PLH::R);
        assert(instruction(addr, PLH::Mode::x64).getDestination() == 0);
    }
    assert(instruction(73, PLH::Mode::x64, false).getDestination() == 73);
    PLH::Instruction::Displacement d{}; d.Relative = -16;
    PLH::Instruction relative(100, d, 0, true, false, std::vector<uint8_t>(6), "jmp", "direct", PLH::Mode::x64);
    assert(relative.getDestination() == 90);
    std::puts("11 genuine Instruction destination controls passed");
}
