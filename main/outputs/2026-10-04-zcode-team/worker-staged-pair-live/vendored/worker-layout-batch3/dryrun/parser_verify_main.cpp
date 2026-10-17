// parser_verify_main.cpp — host harness that feeds the EXACT emitted staged-
// pair bytes through the VENDORED binding parser
// (.superpowers/native-dependency-spike/ue4ss-native-layout-parser-1,
// included read-only) and dumps the parsed bundle as JSON on stdout.
//
// The vendored parser is never modified; this file only consumes its public
// API (RC::Platform::NativeLayouts::ParsePair). Usage:
//   parser_verify <MemberVariableLayout.ini> <VTableLayout.ini>
// On refusal (std::invalid_argument) it prints {"ok":false,...} and exits 3.

#include <NativeLayoutFiles.hpp>

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string read_file(const char* path) {
    std::ifstream fh(path, std::ios::binary);
    if (!fh) throw std::runtime_error(std::string("cannot open ") + path);
    std::ostringstream ss;
    ss << fh.rdbuf();
    return ss.str();
}

std::string json_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", c);
            out += buf;
        } else {
            out += c;
        }
    }
    return out;
}

void dump_entries(const std::vector<RC::Platform::NativeLayouts::Entry>& es,
                  const char* name) {
    std::cout << ",\"" << name << "\":[";
    bool first = true;
    for (const auto& e : es) {
        if (!first) std::cout << ",";
        first = false;
        std::cout << "{\"section\":\"" << json_escape(e.section)
                  << "\",\"key\":\"" << json_escape(e.key)
                  << "\",\"offset\":" << e.offset;
        if (e.bitfield.has_value()) {
            std::cout << ",\"bitfield\":{\"position\":"
                      << int(e.bitfield->position) << ",\"length\":"
                      << int(e.bitfield->length) << ",\"storage\":"
                      << int(e.bitfield->storage_bytes) << "}";
        }
        std::cout << "}";
    }
    std::cout << "]";
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: parser_verify <members.ini> <vtables.ini>\n";
        return 2;
    }
    try {
        const std::string members = read_file(argv[1]);
        const std::string vtables = read_file(argv[2]);
        const auto bundle = RC::Platform::NativeLayouts::ParsePair(members,
                                                                   vtables);
        std::cout << "{\"ok\":true";
        const auto& id = bundle.identity;
        std::cout << ",\"identity\":{\"format\":" << id.format
                  << ",\"pointer_size\":" << id.pointer_size
                  << ",\"engine_major\":" << id.engine_major
                  << ",\"engine_minor\":" << id.engine_minor
                  << ",\"non_shipping\":" << (id.non_shipping ? 1 : 0)
                  << ",\"stats\":" << (id.stats ? 1 : 0)
                  << ",\"target\":\"" << json_escape(id.target)
                  << "\",\"engine_revision\":\""
                  << json_escape(id.engine_revision)
                  << "\",\"build_id\":\"" << json_escape(id.build_id)
                  << "\"}";
        dump_entries(bundle.members, "members");
        dump_entries(bundle.vtables, "vtables");
        std::cout << "}\n";
        return 0;
    } catch (const std::invalid_argument& exc) {
        std::cout << "{\"ok\":false,\"refused\":\""
                  << json_escape(exc.what()) << "\"}\n";
        return 3;
    } catch (const std::exception& exc) {
        std::cout << "{\"ok\":false,\"error\":\"" << json_escape(exc.what())
                  << "\"}\n";
        return 4;
    }
}
