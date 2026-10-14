#pragma once

#include <algorithm> // Actual Helpers/String.hpp prerequisite reached through OutputDevice.
#include <DynamicOutput/OutputDevice.hpp>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace RC::Output {
class RC_DYNOUT_API WriterDevice final : public OutputDevice {
public:
    // Synchronous whole-span UTF-8 writer. Borrowed context outlives the device and every call.
    // The callback consumes/copies bytes before return; it must not retain pointers, reenter,
    // or mutate the output-device container. A false return or exception is an output failure.
    // Raw/formatted text is limited to 65536 UTF-16 units and 196608 UTF-8 bytes.
    // Invalid/oversized messages throw before the callback; valid messages are never truncated.
    using Writer = bool (*)(void*, const char*, std::size_t, int32_t);
    WriterDevice() = default;
    ~WriterDevice() override = default;
    void set_writer(Writer writer, void* context);
    // Configure inherited formatter and device bindings before dispatch begins.
    bool has_optional_arg() const override;
    void receive(File::StringViewType content) const override;
    void receive_with_optional_arg(File::StringViewType content, int32_t level = 0) const override;
    void lock() const override;
    void unlock() const override;
private:
    mutable std::recursive_mutex m_mutex;
    Writer m_writer{};
    void* m_context{};
    mutable bool m_receiving{};
};
}
