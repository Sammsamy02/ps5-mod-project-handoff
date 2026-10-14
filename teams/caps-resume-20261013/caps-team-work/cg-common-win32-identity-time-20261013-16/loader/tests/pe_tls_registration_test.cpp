#include "pe_tls_registration.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

static int failures = 0;
static void check(bool condition, const char* name)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
}

static cap13::PeTlsDirectory64 make_directory(std::vector<uint8_t>& image)
{
    const uint64_t base = reinterpret_cast<uint64_t>(image.data());
    image[0x100] = 0x21;
    image[0x101] = 0x43;
    const uint32_t marker = 0xDEADBEEF;
    std::memcpy(image.data() + 0x200, &marker, sizeof(marker));
    const uint64_t callback = base + 0x400;
    std::memcpy(image.data() + 0x300, &callback, sizeof(callback));
    return {base + 0x100, base + 0x102, base + 0x200,
            base + 0x300, 2, 0};
}

int main()
{
    std::vector<uint8_t> image(0x1000, 0);
    auto dir = make_directory(image);
    cap13::StaticTlsBank bank;
    auto result = cap13::register_mapped_static_tls(bank, image.data(), image.size(), dir);
    uint32_t written = 0xFFFFFFFF;
    std::memcpy(&written, image.data() + 0x200, sizeof(written));
    check(result.ok && result.index == 0 && written == 0,
          "mapped image receives its assigned TLS module index");
    check(result.callbacks.size() == 1 && result.callbacks[0] == dir.AddressOfCallBacks + 0x100,
          "callback table is bounded and its target is returned in order");
    auto view = bank.attach_current_thread();
    auto* block = bank.current_block(result.index);
    check(view.slots.size() == 1 && view.slots[0] == block && block &&
          block[0] == 0x21 && block[1] == 0x43 && block[2] == 0 && block[3] == 0,
          "registered mapped template initializes the current thread");
    image[0x100] = 0x99;
    bool worker_kept_original = false;
    std::thread worker([&] {
        bank.attach_current_thread();
        auto* worker_block = bank.current_block(result.index);
        worker_kept_original = worker_block && worker_block[0] == 0x21 &&
                               worker_block != block;
    });
    worker.join();
    check(worker_kept_original, "template is copied before mapped image can change");

    std::vector<uint8_t> bad_image(0x1000, 0);
    auto bad_dir = make_directory(bad_image);
    cap13::StaticTlsBank bad_bank;
    bad_dir.AddressOfIndex = reinterpret_cast<uint64_t>(bad_image.data()) + bad_image.size() - 3;
    auto bad = cap13::register_mapped_static_tls(bad_bank, bad_image.data(), bad_image.size(), bad_dir);
    uint32_t untouched = 0;
    std::memcpy(&untouched, bad_image.data() + 0x200, sizeof(untouched));
    check(!bad.ok && untouched == 0xDEADBEEF && bad_bank.attach_current_thread().slots.empty(),
          "out-of-bounds index target refuses before any registration or write");

    bad_dir = make_directory(bad_image);
    const uint64_t outside = reinterpret_cast<uint64_t>(bad_image.data()) + bad_image.size();
    std::memcpy(bad_image.data() + 0x300, &outside, sizeof(outside));
    bad = cap13::register_mapped_static_tls(bad_bank, bad_image.data(), bad_image.size(), bad_dir);
    check(!bad.ok && bad_bank.attach_current_thread().slots.empty(),
          "callback target outside mapped image refuses before registration");

    bad_dir = make_directory(bad_image);
    const uint64_t first_valid = reinterpret_cast<uint64_t>(bad_image.data()) + 0x400;
    std::memcpy(bad_image.data() + 0x308, &outside, sizeof(outside));
    bad = cap13::register_mapped_static_tls(bad_bank, bad_image.data(), bad_image.size(), bad_dir);
    check(!bad.ok && bad.callbacks.empty() && bad.index == UINT32_MAX &&
          bad_bank.attach_current_thread().slots.empty(),
          "partial callback scan leaves no callable work or assigned index");

    bad_dir = make_directory(bad_image);
    std::memcpy(bad_image.data() + bad_image.size() - 8, &first_valid, sizeof(first_valid));
    bad_dir.AddressOfCallBacks = reinterpret_cast<uint64_t>(bad_image.data()) + bad_image.size() - 8;
    bad = cap13::register_mapped_static_tls(bad_bank, bad_image.data(), bad_image.size(), bad_dir);
    check(!bad.ok && bad_bank.attach_current_thread().slots.empty(),
          "unterminated callback table refuses at mapped image boundary");

    bad_dir = make_directory(bad_image);
    bad_dir.EndAddressOfRawData = reinterpret_cast<uint64_t>(bad_image.data()) + bad_image.size() + 1;
    bad = cap13::register_mapped_static_tls(bad_bank, bad_image.data(), bad_image.size(), bad_dir);
    check(!bad.ok && bad_bank.attach_current_thread().slots.empty(),
          "template outside mapped image refuses before registration");

    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
