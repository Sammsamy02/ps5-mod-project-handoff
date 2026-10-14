// Included inside namespace RC by the real SinglePassSigScanner translation unit.
// The pinned scalar high/low-nibble and StdFind masked-byte algorithms operate on
// explicit owned spans here instead of VirtualQuery-discovered process regions.
namespace
{
    struct OwnedPattern
    {
        std::vector<int> nibbles;
        std::vector<uint8_t> bytes;
        std::vector<uint8_t> mask;
    };

    auto owned_hex_nibble(char c) -> int
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c == '?') return -1;
        throw std::invalid_argument{"Malformed signature character"};
    }

    auto prepare_owned_pattern(std::string_view signature, SinglePassScanner::ScanMethod method) -> OwnedPattern
    {
        OwnedPattern out;
        if (method == SinglePassScanner::ScanMethod::Scalar)
        {
            // The pinned scalar converter treats question marks as nibbles,
            // disregarding spaces/slashes between them. Do not turn every lone
            // question-mark token into a full byte.
            for (char c : signature)
                if (c != ' ' && c != '/') out.nibbles.push_back(owned_hex_nibble(c));
        }
        else
        {
            // Retain format_aob_string followed by make_mask: discard spaces,
            // turn slashes into byte separators, then consume paired or single
            // question marks as one wildcard byte. Malformed partial literal
            // bytes are rejected rather than converted to zero.
            std::string normalized;
            normalized.reserve(signature.size());
            for (char c : signature)
                if (c != ' ') normalized.push_back(c == '/' ? ' ' : c);
            for (size_t i = 0; i < normalized.size(); ++i)
            {
                if (normalized[i] == ' ') continue;
                if (normalized[i] == '?')
                {
                    out.nibbles.push_back(-1);out.nibbles.push_back(-1);
                    if (i + 1 < normalized.size() && normalized[i + 1] == '?') ++i;
                }
                else
                {
                    if (i + 1 == normalized.size() || normalized[i + 1] == ' ')
                        throw std::invalid_argument{"Signature has an incomplete byte"};
                    out.nibbles.push_back(owned_hex_nibble(normalized[i]));
                    out.nibbles.push_back(owned_hex_nibble(normalized[++i]));
                }
            }
        }
        if (out.nibbles.empty()) throw std::invalid_argument{"Empty signature"};
        if (out.nibbles.size() % 2) throw std::invalid_argument{"Signature has an incomplete byte"};
        for (size_t i = 0; i < out.nibbles.size(); i += 2)
        {
            const int hi = out.nibbles[i], lo = out.nibbles[i + 1];
            out.bytes.push_back(uint8_t((hi < 0 ? 0 : hi << 4) | (lo < 0 ? 0 : lo)));
            out.mask.push_back(uint8_t((hi < 0 ? 0 : 0xf0) | (lo < 0 ? 0 : 0x0f)));
        }
        if (method == SinglePassScanner::ScanMethod::StdFind)
        {
            if (out.mask.front() != 0xff)
                throw std::invalid_argument{"StdFind requires a literal first byte"};
            for (uint8_t mask : out.mask)
                if (mask != 0 && mask != 0xff)
                    throw std::invalid_argument{"StdFind does not support nibble wildcards"};
        }
        return out;
    }

    auto validate_owned_ranges(std::span<const ReadableRange> ranges) -> void
    {
        if (ranges.empty()) throw std::invalid_argument{"Missing readable range input"};
        for (size_t i = 0; i < ranges.size(); ++i)
        {
            const auto& range = ranges[i];
            const uintptr_t start = reinterpret_cast<uintptr_t>(range.data);
            if (!range.data || !range.size || range.size > UINTPTR_MAX - start)
                throw std::invalid_argument{"Invalid readable range extent"};
            const uintptr_t end = start + range.size;
            for (size_t j = 0; j < i; ++j)
            {
                const uintptr_t other = reinterpret_cast<uintptr_t>(ranges[j].data);
                if (start < other + ranges[j].size && other < end)
                    throw std::invalid_argument{"Readable ranges overlap"};
            }
        }
    }

    struct OwnedScanGroup
    {
        std::vector<ReadableRange> ranges;
        std::vector<SignatureContainer>* originals{};
        std::vector<SignatureContainer> copies;
        std::vector<std::vector<OwnedPattern>> patterns;

        auto containers() -> std::vector<SignatureContainer>& { return originals ? *originals : copies; }
    };

    auto same_owned_ranges(const std::vector<ReadableRange>& a, std::span<const ReadableRange> b) -> bool
    {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
            [](const ReadableRange& x, const ReadableRange& y) { return x.data == y.data && x.size == y.size; });
    }
}

auto SinglePassScanner::owned_container_ignored(const SignatureContainer& container) -> bool
{
    std::lock_guard<std::mutex> lock(m_scanner_mutex);
    return container.ignore;
}

auto SinglePassScanner::report_owned_match(SignatureContainer& container, size_t signature_index,
                                           const uint8_t* address, size_t signature_size) -> bool
{
    std::lock_guard<std::mutex> lock(m_scanner_mutex);
    if (container.ignore) return true;
    container.index_into_signatures = signature_index;
    container.match_address = const_cast<uint8_t*>(address);
    container.match_signature_size = signature_size;
    container.ignore = container.on_match_found(container);
    if (container.store_results)
        container.result_store.emplace_back(SignatureContainerLight{signature_index, const_cast<uint8_t*>(address)});
    return container.ignore;
}

auto SinglePassScanner::start_scan(SignatureContainerMap& signature_containers,
                                   const ReadableRangeMap& readable_ranges) -> void
{
    const auto method = m_scan_method;
    const uint32_t threads = m_num_threads;
    const uint32_t threshold = m_multithreading_module_size_threshold;
    if (!threads) throw std::invalid_argument{"Scanner worker count is zero"};
    if (method != ScanMethod::Scalar && method != ScanMethod::StdFind)
        throw std::invalid_argument{"Unsupported scanner method"};

    // Complete all target/range/pattern admission before any notification. As in
    // the original monolithic-image path, identical range inputs share copies of
    // their containers; modular inputs retain the original container objects.
    std::vector<OwnedScanGroup> groups;
    groups.reserve(signature_containers.size());
    for (auto& [target, containers] : signature_containers)
    {
        if (static_cast<size_t>(target) >= static_cast<size_t>(ScanTarget::Max))
            throw std::invalid_argument{"Invalid scan target"};
        if (containers.empty()) continue;
        const auto input = readable_ranges.find(target);
        if (input == readable_ranges.end()) throw std::invalid_argument{"Missing requested scan target"};
        validate_owned_ranges(input->second);
        OwnedScanGroup* group{};
        if (!SigScannerStaticData::m_is_modular)
            for (auto& existing : groups)
                if (same_owned_ranges(existing.ranges, input->second)) { group = &existing; break; }
        if (!group)
        {
            group = &groups.emplace_back();
            group->ranges.assign(input->second.begin(), input->second.end());
            if (SigScannerStaticData::m_is_modular) group->originals = &containers;
        }
        if (!group->originals)
            for (const auto& container : containers) group->copies.emplace_back(container);
    }
    for (auto& group : groups)
    {
        for (auto& container : group.containers())
        {
            if (!container.on_match_found || !container.on_scan_finished)
                throw std::invalid_argument{"Missing scanner callback"};
            auto& compiled = group.patterns.emplace_back();
            for (const auto& signature : container.signatures)
                compiled.push_back(prepare_owned_pattern(signature.signature, method));
        }
    }

    for (auto& group : groups)
    {
        auto& containers = group.containers();
        for (const auto range : group.ranges)
        {
            auto segment = [&](size_t first, size_t last) {
                ProfilerSetThreadName("UE4SS-ScannerWorkThread");
                ProfilerScope();
                if (method == ScanMethod::Scalar)
                {
                    // Retain address -> container -> signature traversal and the
                    // actual scalar high/low-nibble matching operation.
                    for (size_t at = first; at < last; ++at)
                        for (size_t c = 0; c < containers.size(); ++c)
                        {
                            if (owned_container_ignored(containers[c])) continue;
                            for (size_t s = 0; s < group.patterns[c].size(); ++s)
                            {
                                const auto& pattern = group.patterns[c][s];
                                if (pattern.bytes.size() > range.size - at) continue;
                                bool matched = true;
                                for (size_t n = 0; n < pattern.nibbles.size(); n += 2)
                                {
                                    const uint8_t byte = range.data[at + n / 2];
                                    if ((pattern.nibbles[n] != -1 && pattern.nibbles[n] != HI_NIBBLE(byte)) ||
                                        (pattern.nibbles[n + 1] != -1 && pattern.nibbles[n + 1] != LO_NIBBLE(byte)))
                                    { matched = false; break; }
                                }
                                if (matched && report_owned_match(containers[c], s, range.data + at, pattern.bytes.size())) break;
                            }
                        }
                }
                else
                {
                    // Retain container -> signature -> std::find traversal and
                    // masked-byte comparison. Each segment owns match starts;
                    // full range bounds allow a match to cross worker partitions.
                    for (size_t c = 0; c < containers.size(); ++c)
                        for (size_t s = 0; s < group.patterns[c].size(); ++s)
                        {
                            if (owned_container_ignored(containers[c])) break;
                            const auto& pattern = group.patterns[c][s];
                            if (pattern.bytes.size() > range.size) continue;
                            const size_t end_offset = std::min(last, range.size - pattern.bytes.size() + 1);
                            if (first >= end_offset) continue;
                            auto it = range.data + first;
                            const auto end = range.data + end_offset;
                            while (end != (it = std::find(it, end, pattern.bytes[0])))
                            {
                                bool matched = true;
                                for (size_t n = 0; n < pattern.bytes.size(); ++n)
                                    if ((it[n] & pattern.mask[n]) != pattern.bytes[n]) { matched = false; break; }
                                if (matched && report_owned_match(containers[c], s, it, pattern.bytes.size())) break;
                                ++it;
                            }
                        }
                }
            };

            const size_t workers = range.size >= threshold ? std::min<size_t>(threads, range.size) : 1;
            if (workers == 1) segment(0, range.size);
            else
            {
                std::vector<std::future<void>> futures;
                futures.reserve(workers);
                const size_t quotient = range.size / workers, remainder = range.size % workers;
                size_t start{};
                for (size_t worker = 0; worker < workers; ++worker)
                {
                    const size_t finish = start + quotient + (worker < remainder ? 1 : 0);
                    futures.emplace_back(std::async(std::launch::async, segment, start, finish));
                    start = finish;
                }
                std::exception_ptr failure;
                for (auto& future : futures)
                    try { future.get(); } catch (...) { if (!failure) failure = std::current_exception(); }
                if (failure) std::rethrow_exception(failure);
            }
        }
    }
    for (auto& group : groups)
        for (auto& container : group.containers()) container.on_scan_finished(container);
}

auto SinglePassScanner::string_scan(std::wstring_view needle,
                                    std::span<const ReadableRange> readable_ranges) -> void*
{
    validate_owned_ranges(readable_ranges);
    if (needle.empty() || needle.size() > SIZE_MAX / sizeof(wchar_t))
        throw std::invalid_argument{"Invalid string scan needle"};
    const size_t bytes = needle.size() * sizeof(wchar_t);
    for (const auto range : readable_ranges)
    {
        if (bytes > range.size) continue;
        for (size_t at = 0; at <= range.size - bytes; ++at)
            if (std::memcmp(range.data + at, needle.data(), bytes) == 0)
                return const_cast<uint8_t*>(range.data + at);
    }
    return nullptr;
}
