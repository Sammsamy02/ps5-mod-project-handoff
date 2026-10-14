#include "static_tls_bank.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

static int failures = 0;

static void check(bool condition, const char* name)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
}

int main()
{
    cap13::StaticTlsBank bank;
    const uint32_t first_index = bank.register_module({0x11, 0x22, 0x33}, 3);
    check(first_index == 0, "first static module receives index zero");
    check(bank.current_block(first_index) == nullptr,
          "guest thread must attach before reading a static TLS block");

    auto main_view = bank.attach_current_thread();
    auto* main_block = bank.current_block(first_index);
    check(main_view.slots.size() == 1 && main_view.slots[0] == main_block && main_block,
          "attach exposes module block through the thread pointer array");
    check(main_block && main_block[0] == 0x11 && main_block[1] == 0x22 &&
          main_block[2] == 0x33 && main_block[3] == 0 && main_block[4] == 0 &&
          main_block[5] == 0, "template bytes copy and zero fill are correct");
    if (main_block) main_block[0] = 0x77;

    std::atomic<bool> worker_saw_template{false};
    std::atomic<bool> worker_pointer_distinct{false};
    std::thread worker([&] {
        auto worker_view = bank.attach_current_thread();
        auto* worker_block = bank.current_block(first_index);
        worker_saw_template = worker_view.slots.size() == 1 && worker_block &&
                              worker_block[0] == 0x11 && worker_block[3] == 0;
        worker_pointer_distinct = worker_block && worker_block != main_block;
        if (worker_block) worker_block[0] = 0x99;
    });
    worker.join();
    check(worker_saw_template && worker_pointer_distinct && main_block && main_block[0] == 0x77,
          "different threads receive independent initialized blocks");

    const uint32_t second_index = bank.register_module({0x44}, 1);
    check(second_index == 1, "late registration receives monotonic index");
    check(bank.current_block(second_index) == nullptr,
          "late module is unavailable until the thread refreshes its array");
    auto late_view = bank.attach_current_thread();
    auto* second_block = bank.current_block(second_index);
    check(late_view.epoch > main_view.epoch && late_view.slots.size() == 2 &&
          late_view.slots[0] == main_block && late_view.slots[1] == second_block &&
          second_block && second_block[0] == 0x44 && second_block[1] == 0 &&
          main_block && main_block[0] == 0x77,
          "late attach adds new block without replacing existing state");

    cap13::StaticTlsBank other;
    const uint32_t other_index = other.register_module({}, 0);
    auto other_view = other.attach_current_thread();
    check(other_index == 0 && other_view.slots.size() == 1 &&
          other_view.slots[0] && other_view.slots[0] != main_block,
          "separate banks have independent index spaces and non-null empty blocks");

    bool overflow_refused = false;
    try { bank.register_module({0xAA}, std::numeric_limits<size_t>::max()); }
    catch (const std::overflow_error&) { overflow_refused = true; }
    check(overflow_refused, "template-size overflow refuses before allocation");

    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
