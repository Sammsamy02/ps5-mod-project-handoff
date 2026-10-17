#include "test.hpp"
#include "modbridge/core.hpp"
#include <array>
#include <fstream>

using namespace modbridge;

// Catches incorrect initialization, padding, hex output, or dropped input bytes.
TEST(hash_known_vectors) {
    CHECK_EQ(sha256_bytes({}), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::array<std::uint8_t, 3> abc{'a', 'b', 'c'};
    CHECK_EQ(sha256_bytes(abc), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

// Catches truncated streams, repeated/skipped chunks, and text-mode processing.
// Independent oracle: Python hashlib, 17 * (bytes(range(256)) * 4096) + b'tail\x00\xff'.
TEST(stream_hash_large_file) {
    test::TempDir dir;
    const auto file = dir.path / "large.bin";
    std::array<char, 256> block{};
    for (std::size_t i = 0; i < block.size(); ++i) block[i] = static_cast<char>(i);
    {
        std::ofstream out(file, std::ios::binary);
        for (unsigned i = 0; i < 17 * 4096; ++i) out.write(block.data(), block.size());
        const std::array<char, 6> tail{'t', 'a', 'i', 'l', '\0', static_cast<char>(255)};
        out.write(tail.data(), tail.size());
        CHECK(out.good());
    }
    CHECK_EQ(sha256_file(file), "f98768f9ff21dec6714aad16141b794ca80f430edabd273ce1960e44c593e40b");
}

// Catches insertion-dependent objects and incorrectly reordered arrays.
TEST(canonical_object_key_order) {
    Json first = Json::object();
    first["z"] = {{"b", 2}, {"a", 1}};
    first["a"] = Json::array({3, 1, 2});
    Json second = Json::object();
    second["a"] = Json::array({3, 1, 2});
    second["z"] = {{"a", 1}, {"b", 2}};
    CHECK_EQ(canonical_json(first), "{\"a\":[3,1,2],\"z\":{\"a\":1,\"b\":2}}");
    CHECK_EQ(canonical_json(first), canonical_json(second));
    second["a"] = Json::array({1, 2, 3});
    CHECK(canonical_json(first) != canonical_json(second));
}

// Catches returning an empty or partial digest when opening/reading fails.
TEST(file_hash_reports_io_errors) {
    test::TempDir dir;
    CHECK_THROWS_AS(sha256_file(dir.path / "missing.bin"), std::ios_base::failure);
    CHECK_THROWS_AS(sha256_file(dir.path), std::ios_base::failure);
}

TEST(empty_file_hash) {
    test::TempDir dir;
    const auto file = dir.path / "empty.bin";
    { std::ofstream out(file, std::ios::binary); CHECK(out.good()); }
    CHECK_EQ(sha256_file(file), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}
