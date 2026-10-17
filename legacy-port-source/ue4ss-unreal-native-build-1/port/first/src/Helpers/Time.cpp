#include <chrono>

#include <Helpers/Time.hpp>
#include <fmt/xchar.h>
#include <fmt/chrono.h>

#if !defined(_WIN32)
#include <Helpers/Utf16Utf8.hpp>
#include <iterator>
#include <string>
#endif

#if _WIN32
#define NOMINMAX
#include <Windows.h>
#ifdef TEXT
#undef TEXT
#endif
#endif

#if RC_IS_ANSI == 1
#define RC_STD_MAKE_FORMAT_ARGS fmt::make_format_args
#else
#define RC_STD_MAKE_FORMAT_ARGS fmt::make_format_args<fmt::buffered_context<CharType>>
#endif

namespace RC
{
    auto get_now_as_string(StringViewType format) -> StringType
    {
#if defined(_WIN32)
        bool use_local_time = true;
#ifdef _WIN32
        if (auto module = GetModuleHandleW(L"ntdll.dll"); module && GetProcAddress(module, "wine_get_version"))
        {
            use_local_time = false;
        }
#endif

        if (use_local_time)
        {
            try
            {
                static const auto timezone = std::chrono::current_zone();
                const auto now = std::chrono::time_point_cast<std::chrono::system_clock::duration>(timezone->to_local(std::chrono::system_clock::now()));
                return fmt::vformat(fmt::detail::to_string_view(format), RC_STD_MAKE_FORMAT_ARGS(now));
            }
            catch (std::runtime_error&)
            {
                const auto now = std::chrono::system_clock::now();
                return fmt::vformat(fmt::detail::to_string_view(format), RC_STD_MAKE_FORMAT_ARGS(now));
            }
        }
        else
        {
            const auto now = std::chrono::system_clock::now();
            return fmt::vformat(fmt::detail::to_string_view(format), RC_STD_MAKE_FORMAT_ARGS(now));
        }
#else
        // The common native path accepts bounded ASCII fmt chrono syntax. Message text
        // is still full UTF-16; only the timestamp format has this explicit limitation.
        if (format.size() > 1024) throw std::length_error("Timestamp format is too long");
        std::string ascii;
        ascii.reserve(format.size());
        for (const auto unit : format)
        {
            const auto value = static_cast<std::uint16_t>(unit);
            if (!value || value > 0x7F) throw std::invalid_argument("Timestamp format must be ASCII without NUL");
            ascii.push_back(static_cast<char>(value));
        }

        // SDK libc++ does not provide the timezone database used by the Win/Wine
        // branch. CRT localtime/gmtime still use the real system clock/timezone.
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm calendar{};
        try { calendar = fmt::localtime(now); }
        catch (const fmt::format_error&) { calendar = fmt::gmtime(now); }

        struct BoundedOutput
        {
            using iterator_category = std::output_iterator_tag;
            using value_type = void;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = void;
            std::string* text;
            BoundedOutput& operator*() { return *this; }
            BoundedOutput& operator++() { return *this; }
            BoundedOutput operator++(int) { return *this; }
            BoundedOutput& operator=(char byte)
            {
                if (text->size() == Unicode::MaxBytes) throw std::length_error("Formatted timestamp is too long");
                text->push_back(byte);
                return *this;
            }
        };
        std::string formatted;
        fmt::vformat_to(BoundedOutput{&formatted}, ascii, fmt::make_format_args(calendar));
        return Unicode::DecodeUtf8(formatted);
#endif
    }
} // namespace RC
