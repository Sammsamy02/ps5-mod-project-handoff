#pragma once

#if defined(__PROSPERO__) || defined(RC_FILE_USE_POSIX_BACKEND)

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <File/Common.hpp>
#include <File/FileType/FileBase.hpp>

namespace RC::File
{
    // Actual POSIX descriptor/file mapping backend. Host selection is explicit;
    // it does not define or emulate a Prospero/Windows/Linux OS macro.
    class PosixFile : public FileInterface<PosixFile>
    {
        static_assert(sizeof(CharType) == 2, "The native File contract uses 16-bit character units");

        struct Identity
        {
            uint64_t device{}, inode{}, bytes{};
            int64_t modification_seconds{}, change_seconds{};
            uint32_t modification_nanoseconds{}, change_nanoseconds{};
            auto operator==(const Identity&) const -> bool = default;
        };

        int m_fd{-1};
        uint8_t* m_mapping{};
        size_t m_mapping_bytes{};
        bool m_mapping_active{};
        OpenProperties m_properties{};
        std::filesystem::path m_path, m_serialization_path;
        bool m_serialization_config_failed{}, m_cache_loaded{}, m_identity_loaded{};
        Identity m_cached_identity{};
        std::vector<uint8_t> m_cache;
        size_t m_next_item{};
        alignas(uint64_t) std::array<uint8_t, sizeof(uint64_t)> m_item{};

        auto close_noexcept() noexcept -> void;
        auto require_open() const -> void;
        auto require_serialization_path() const -> void;
        auto live_identity() const -> Identity;
        auto load_cache() -> void;
        auto persist_cache(const std::vector<uint8_t>&) -> void;
        auto reset_cache() noexcept -> void;
        auto build_identity_cache() const -> std::vector<uint8_t>;

      public:
        PosixFile() = default;
        PosixFile(const PosixFile&) = delete;
        auto operator=(const PosixFile&) -> PosixFile& = delete;
        RC_FILE_API PosixFile(PosixFile&&) noexcept;
        RC_FILE_API auto operator=(PosixFile&&) noexcept -> PosixFile&;
        RC_FILE_API ~PosixFile() noexcept override;

        // Borrowed descriptor value; ownership always remains with this object.
        RC_FILE_API auto get_file_descriptor() const noexcept -> int;
        RC_FILE_API auto is_file_open() const noexcept -> bool;
        RC_FILE_API auto serialization_file_exists() -> bool;
        RC_FILE_API auto static open_file(const std::filesystem::path&, const OpenProperties&) -> PosixFile;
        RC_FILE_API auto static delete_file(const std::filesystem::path&) -> void;

        RC_FILE_API auto is_valid() noexcept -> bool override;
        RC_FILE_API auto invalidate_file() noexcept -> void override;
        RC_FILE_API auto delete_file() -> void override;
        // Legacy void* interface: pointer to our owned int, not an encoded HANDLE.
        // Borrowed until move/close/destruction; do not mutate or close through it.
        RC_FILE_API auto get_raw_handle() noexcept -> void* override;
        RC_FILE_API auto get_file_path() const -> const std::filesystem::path& override;
        RC_FILE_API auto set_serialization_output_file(const std::filesystem::path&) noexcept -> void override;
        RC_FILE_API auto serialize_identifying_properties() -> void override;
        RC_FILE_API auto deserialize_identifying_properties() -> void override;
        RC_FILE_API auto is_deserialized_and_live_equal() -> bool override;
        RC_FILE_API auto invalidate_serialization() -> void override;
        RC_FILE_API auto serialize_item(const GenericItemData&, bool is_internal_item = false) -> void override;
        RC_FILE_API auto get_serialized_item(size_t, bool is_internal_item = false) -> void* override;
        RC_FILE_API auto close_current_file() -> void override;
        RC_FILE_API auto write_string_to_file(StringViewType) -> void override;
        RC_FILE_API auto is_same_as(PosixFile&) -> bool override;
        RC_FILE_API auto read_all() const -> StringType override;
        RC_FILE_API auto memory_map() -> std::span<uint8_t> override;

        // Backend helper used by the native HandleTemplate to retain actual int,
        // long and long-long widths without changing the existing generic enum.
        RC_FILE_API auto serialize_integral_value(uint64_t bits, uint32_t bytes) -> void;
    };

    template <ImplementsFileInterface UnderlyingAbstraction> class HandleTemplate;
    using Handle = HandleTemplate<PosixFile>;
} // namespace RC::File
#endif
