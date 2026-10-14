#include "modbridge/core.hpp"
#include <picosha2.h>
#include <array>
#include <fstream>

namespace modbridge {

std::string sha256_bytes(std::span<const std::uint8_t> bytes) {
    picosha2::hash256_one_by_one hash;
    if (!bytes.empty()) hash.process(bytes.begin(), bytes.end());
    hash.finish();
    return picosha2::get_hash_hex_string(hash);
}

std::string sha256_file(const std::filesystem::path& path) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error) {
        throw std::ios_base::failure("Not a readable regular file: " + path.string());
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::ios_base::failure("Cannot open file for hashing: " + path.string());
    picosha2::hash256_one_by_one hash;
    std::array<std::uint8_t, 64 * 1024> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const auto count = input.gcount();
        hash.process(buffer.begin(), buffer.begin() + count);
    }
    if (input.bad() || !input.eof()) {
        throw std::ios_base::failure("Cannot read file for hashing: " + path.string());
    }
    hash.finish();
    return picosha2::get_hash_hex_string(hash);
}

std::string canonical_json(const Json& value) {
    // Json's default object container is std::map, recursively sorting keys;
    // dump preserves array order and rejects invalid UTF-8 rather than losing it.
    return value.dump();
}

} // namespace modbridge
