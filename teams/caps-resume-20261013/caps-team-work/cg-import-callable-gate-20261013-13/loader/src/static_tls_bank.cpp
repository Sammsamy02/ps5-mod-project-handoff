#include "static_tls_bank.h"

#include <atomic>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace cap13 {

namespace {

struct ThreadState
{
    // Each allocation stays at a stable address even when this vector grows.
    std::vector<std::unique_ptr<uint8_t[]>> blocks;
};

std::atomic<uint64_t> next_bank_id{1};
thread_local std::unordered_map<uint64_t, ThreadState> thread_states;

} // namespace

struct StaticTlsBank::Impl
{
    struct Module
    {
        std::vector<uint8_t> initial;
        size_t zero_fill;
    };

    const uint64_t id = next_bank_id.fetch_add(1, std::memory_order_relaxed);
    std::mutex mu;
    std::vector<Module> modules;
    uint64_t epoch = 0;
};

StaticTlsBank::StaticTlsBank() : m(new Impl()) {}
StaticTlsBank::~StaticTlsBank() = default;

uint32_t StaticTlsBank::register_module(const std::vector<uint8_t>& initial,
                                        size_t zero_fill)
{
    if (initial.size() > std::numeric_limits<size_t>::max() - zero_fill)
        throw std::overflow_error("static TLS template plus zero fill overflows size_t");
    std::lock_guard<std::mutex> guard(m->mu);
    if (m->modules.size() >= std::numeric_limits<uint32_t>::max())
        throw std::overflow_error("static TLS module index space exhausted");
    const uint32_t index = static_cast<uint32_t>(m->modules.size());
    m->modules.push_back(Impl::Module{initial, zero_fill});
    ++m->epoch;
    return index;
}

StaticTlsBank::ThreadArray StaticTlsBank::attach_current_thread()
{
    std::lock_guard<std::mutex> guard(m->mu);
    ThreadState& state = thread_states[m->id];
    while (state.blocks.size() < m->modules.size())
    {
        const Impl::Module& module = m->modules[state.blocks.size()];
        const size_t size = module.initial.size() + module.zero_fill;
        auto bytes = std::make_unique<uint8_t[]>(size ? size : 1);
        std::memset(bytes.get(), 0, size ? size : 1);
        if (!module.initial.empty())
            std::memcpy(bytes.get(), module.initial.data(), module.initial.size());
        state.blocks.push_back(std::move(bytes));
    }
    ThreadArray view;
    view.epoch = m->epoch;
    view.slots.reserve(state.blocks.size());
    for (const auto& block : state.blocks)
        view.slots.push_back(block.get());
    return view;
}

uint8_t* StaticTlsBank::current_block(uint32_t index) const
{
    auto it = thread_states.find(m->id);
    if (it == thread_states.end() || index >= it->second.blocks.size()) return nullptr;
    return it->second.blocks[index].get();
}

// This class does not set GS, invoke TLS callbacks, or write AddressOfIndex.
// The loader must keep refusing real TLS until those steps are integrated.

}
