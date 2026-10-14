// Read-only, bounded PE32+ static-import DLL-name scanner.
#ifndef CAP13_PE_IMPORT_SCAN_H
#define CAP13_PE_IMPORT_SCAN_H

#include <cstddef>
#include <string>
#include <vector>

namespace cap13 {

struct PeImportScanResult
{
    bool ok = false;
    std::string reason;
    std::vector<std::string> dll_names;
};

PeImportScanResult pe_scan_imported_dlls(const void* file_bytes, size_t file_size);

} // namespace cap13
#endif
