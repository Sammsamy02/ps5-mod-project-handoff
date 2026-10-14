struct DataWindowAnchor {
    uint64_t vaddr;      // eboot p_vaddr of the window
    uint32_t length;
    const unsigned char *bytes;
};
static const unsigned char kWindowE[] = { 0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15, 0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75 };   // AES S-box head, p_vaddr 0x7dd2a40
static const unsigned char kWindowA[] = { 0x04, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x47, 0x4e, 0x55, 0x00, 0x04, 0xbb, 0x43, 0x30, 0xa7, 0xd7, 0x1d, 0xce, 0xfe, 0x1c, 0xc2, 0xc4, 0xd1, 0x97, 0x69, 0xd5, 0x00, 0x00, 0x00, 0x00 };   // GNU build-id note, p_vaddr 0xbc5f250
static const DataWindowAnchor kDataWindowAnchors[2] = {
    { 0x7dd2a40, 64, kWindowE },
    { 0xbc5f250, 36, kWindowA },
};
static constexpr uint64_t kCandidateBase = 0x400000;       // validated bias
static constexpr uint64_t kImageExtentVaddr = 0xbc602c8;  // max(p_vaddr+p_memsz)

// VMMAP protection bits (kinfo_vmentry kve_protection, observed in-game:
// 1=read, 4=exec, 3=rw). Matches lmf base-scan readable-bit semantics.
static constexpr uint32_t kVmProtRead = 1u;

static bool window_anchor_verify(const lmf::LiveModuleOsOps &base_ops,
                                 void *ctx) {
    // Fetch fresh mappings via the SAME proven vmentry op the core uses.
    lmf::OsVmMapping mappings[256];
    size_t total = 0;
    if (base_ops.query_vmentry == nullptr ||
        base_ops.query_vmentry(ctx, mappings, 256, &total) != 0 || total == 0)
        return false;
    const size_t count = total < 256 ? total : 256;
    for (const DataWindowAnchor &a : kDataWindowAnchors) {
        const uint64_t rt = kCandidateBase + a.vaddr;
        bool covered = false;
        for (size_t i = 0; i < count; ++i) {
            if ((mappings[i].protection & kVmProtRead) != 0 &&
                mappings[i].start <= rt && rt + a.length <= mappings[i].end) {
                covered = true;
                break;
            }
        }
        if (!covered) return false;
        const unsigned char *p = reinterpret_cast<const unsigned char *>(rt);
        for (uint32_t i = 0; i < a.length; ++i)
            if (p[i] != a.bytes[i]) return false;  // span-covered read; safe
    }
    return true;
}
