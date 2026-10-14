#include "pe_import_scan.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    for (int i = 1; i < argc; ++i)
    {
        std::ifstream file(argv[i], std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        auto result = cap13::pe_scan_imported_dlls(bytes.data(), bytes.size());
        std::printf("FILE %s ok=%d reason=%s imports=%zu\n", argv[i],
                    result.ok, result.reason.c_str(), result.dll_names.size());
        for (const auto& name : result.dll_names)
            std::printf("  %s\n", name.c_str());
        if (!result.ok) return 1;
    }
    return 0;
}
