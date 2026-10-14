#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace RC::Platform::NativeLayouts {
struct Identity {
    uint32_t format{},pointer_size{},engine_major{},engine_minor{};
    bool non_shipping{},stats{};
    std::string target,engine_revision,build_id;
    bool operator==(const Identity&) const = default;
};
struct Bitfield {
    uint8_t position{},length{},storage_bytes{};
    bool operator==(const Bitfield&) const = default;
};
struct Entry {
    std::string section,key;
    uint32_t offset{};
    std::optional<Bitfield> bitfield;
    bool operator==(const Entry&) const = default;
};
struct Bundle {
    Identity identity;
    std::vector<Entry> members,vtables;
};
// Strict staged data parser: no external state, schema binding or engine access.
// Identity equality is declared consistency only, never target/ABI proof.
// Names are ASCII identifiers; sections also accept C++ nested-name :: segments.
// Revision/BuildId are untrusted nonempty printable ASCII metadata. Whole-line
// ;/# comments, optional initial UTF-8 BOM and CRLF/LF are accepted.
// Throws std::invalid_argument with filename/line detail for refused input.
// Limits:8MiB/file,16384 layout entries/file,512 bytes/name/value token.
// At most 16385 sections/file (including NativeLayout) prevents empty-section
// inputs from bypassing the bounded entry storage.
auto ParsePair(std::string_view members,std::string_view vtables) -> Bundle;
}
