#include <DynamicOutput/WriterDevice.hpp>
#include <DynamicOutput/Output.hpp>
#include <Helpers/Utf16Utf8.hpp>
#include <stdexcept>

namespace RC::Output {
void WriterDevice::set_writer(Writer writer, void* context) {
    std::lock_guard<std::recursive_mutex> guard(m_mutex);
    if (!writer || m_receiving) {
        THROW_INTERNAL_FILE_ERROR("Output writer binding is invalid")
    }
    m_writer = writer;
    m_context = context;
    m_is_device_ready = true;
}
bool WriterDevice::has_optional_arg() const { return true; }
void WriterDevice::receive(File::StringViewType content) const {
    receive_with_optional_arg(content, Color::NoColor);
}
void WriterDevice::receive_with_optional_arg(File::StringViewType content, int32_t level) const {
    std::lock_guard<std::recursive_mutex> guard(m_mutex);
    if (m_receiving) {
        THROW_INTERNAL_FILE_ERROR("Output writer callback must not reenter")
    }
    struct Receiving {
        bool& active;
        explicit Receiving(bool& value) : active(value) { active = true; }
        ~Receiving() { active = false; }
    } receiving(m_receiving);
    try {
        if (!m_writer || !m_formatter) throw std::runtime_error("Output writer is not configured");
        (void)Unicode::Utf8Size(content);
        const auto formatted = m_formatter(content);
        const auto utf8 = Unicode::EncodeUtf8(formatted);
        if (!m_writer(m_context, utf8.data(), utf8.size(), level))
            throw std::runtime_error("Output writer refused the message");
    } catch (...) {
        File::Internal::StaticStorage::internal_error = true;
        throw;
    }
}
void WriterDevice::lock() const { m_mutex.lock(); }
void WriterDevice::unlock() const { m_mutex.unlock(); }
}
