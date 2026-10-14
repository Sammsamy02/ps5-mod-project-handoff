#include "nexus_download.h"
int main() { return mb_nexus_resolve_download_v1(nullptr, nullptr) == MB_NEXUS_DOWNLOAD_BAD_ARGUMENT ? 0 : 1; }
