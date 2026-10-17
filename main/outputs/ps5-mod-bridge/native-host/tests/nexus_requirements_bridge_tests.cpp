#include "../nexus_bridge.h"
#include "modbridge/nexus.hpp"
#include "../../tests/test.hpp"

#include <array>
#include <string_view>

TEST(nexus_requirements_native_bridge_uses_fixed_url_and_graphql_int_context) {
    std::array<char, 256> output{}; std::size_t written = 0;
    CHECK_EQ(mb_nexus_metadata_url(MB_NEXUS_REQUIREMENTS, "future_game-2", 13, 2147483647,
        output.data(), output.size(), &written), MB_NEXUS_OK);
    CHECK_EQ(std::string_view(output.data(), written), "https://api.nexusmods.com/v2/graphql");
    for (const auto id : {UINT64_C(0), UINT64_C(2147483648), UINT64_MAX}) {
        CHECK_EQ(mb_nexus_metadata_url(MB_NEXUS_REQUIREMENTS, "game", 4, id,
            output.data(), output.size(), &written), MB_NEXUS_INVALID_ARGUMENT);
        CHECK_EQ(output[0], '\0'); CHECK_EQ(written, 0U);
    }
}

TEST(nexus_requirements_native_bridge_reports_object_and_short_output_required_length) {
    constexpr std::string_view body = R"({"data":{"modRequirements":{"nexusRequirements":{"nodes":[],"nodesCount":0,"totalCount":0},"dlcRequirements":[]}}})";
    std::array<char, 4096> output{}; std::size_t written = 0;
    CHECK_EQ(mb_nexus_decode(MB_NEXUS_REQUIREMENTS,
        reinterpret_cast<const unsigned char*>(body.data()), body.size(),
        "future_game-2", 13, 71, output.data(), output.size(), &written), MB_NEXUS_OK);
    const auto decoded = modbridge::Json::parse(output.data());
    CHECK_EQ(decoded.at("status"), "decoded");
    CHECK(decoded.contains("requirements")); CHECK(!decoded.contains("records"));
    const auto required = written;
    output.fill('x');
    CHECK_EQ(mb_nexus_decode(MB_NEXUS_REQUIREMENTS,
        reinterpret_cast<const unsigned char*>(body.data()), body.size(),
        "future_game-2", 13, 71, output.data(), required, &written), MB_NEXUS_BUFFER_TOO_SMALL);
    CHECK_EQ(written, required); CHECK_EQ(output[0], '\0');
}
