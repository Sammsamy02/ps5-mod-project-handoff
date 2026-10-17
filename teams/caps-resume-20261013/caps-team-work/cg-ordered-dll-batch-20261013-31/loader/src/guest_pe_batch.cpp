#include "guest_pe_batch.h"
#include "win_api_shim.h"

#include <string>
#include <unordered_set>
#include <utility>

namespace cap13 {
namespace {

std::string module_key(const char* path)
{
    if (!path) return {};
    const char* basename = path;
    for (const char* at = path; *at; ++at)
        if (*at == '/' || *at == '\\') basename = at + 1;
    std::string key = basename;
    for (char& ch : key)
        if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
    return key;
}

bool only_waiting_for_pending(const std::vector<std::string>& missing,
                              const std::vector<std::string>& keys,
                              const std::vector<bool>& pending)
{
    if (missing.empty()) return false;
    for (const auto& entry : missing)
    {
        const size_t separator = entry.find('!');
        if (separator == 0 || separator == std::string::npos) return false;
        const std::string dependency = module_key(entry.substr(0, separator).c_str());
        bool waiting = false;
        for (size_t i = 0; i < keys.size(); ++i)
            if (pending[i] && keys[i] == dependency) { waiting = true; break; }
        if (!waiting) return false;
    }
    return true;
}

} // namespace

bool pe_detach_guest_batch(GuestPeBatchResult& result, GuestTebView& teb)
{
    bool clean = true;
    for (size_t i = result.modules.size(); i > 0; --i)
    {
        if (!result.modules[i - 1]) continue;
        if (result.modules[i - 1]->process_detach(teb) == GuestNotifyStatus::Ok)
            result.modules[i - 1].reset();
        else clean = false;
    }
    std::vector<std::unique_ptr<GuestPeModule>> retained;
    std::vector<size_t> retained_indices;
    retained.reserve(result.modules.size());
    retained_indices.reserve(result.input_indices.size());
    for (size_t i = 0; i < result.modules.size(); ++i)
    {
        if (result.modules[i])
        {
            retained.push_back(std::move(result.modules[i]));
            retained_indices.push_back(result.input_indices[i]);
        }
    }
    result.modules = std::move(retained);
    result.input_indices = std::move(retained_indices);
    return clean;
}

GuestPeBatchResult pe_attach_guest_batch(
    const std::vector<GuestPeBatchInput>& inputs, WinApiShim& shim,
    const LoadOptions& options, StaticTlsBank& bank, GuestTebView& teb)
{
    GuestPeBatchResult result;
    if (inputs.empty() || inputs.size() > 256 ||
        !teb.belongs_to(bank) || !teb.on_owner_thread())
    {
        result.reason = "invalid batch size or guest TEB owner";
        return result;
    }
    std::vector<std::string> keys;
    std::unordered_set<std::string> seen;
    keys.reserve(inputs.size());
    for (const auto& input : inputs)
    {
        const std::string key = module_key(input.source_path);
        if (key.empty() || !input.file_bytes || !input.file_size || !input.segment)
        {
            result.reason = "invalid PE batch input";
            return result;
        }
        if (!seen.insert(key).second)
        {
            result.reason = "duplicate normalized DLL basename: " + key;
            return result;
        }
        keys.push_back(key);
    }

    result.modules.reserve(inputs.size());
    result.input_indices.reserve(inputs.size());
    std::vector<bool> pending(inputs.size(), true);
    std::vector<std::vector<std::string>> deferred(inputs.size());
    size_t remaining = inputs.size();
    const auto fail = [&](std::string reason, std::vector<std::string> missing)
        -> GuestPeBatchResult
    {
        result.reason = std::move(reason);
        result.missing = std::move(missing);
        if (!pe_detach_guest_batch(result, teb))
        {
            result.rollback_failed = true;
            result.reason += "; rollback failed; attached modules retained";
        }
        return std::move(result);
    };

    while (remaining)
    {
        bool progressed = false;
        for (size_t i = 0; i < inputs.size(); ++i)
        {
            if (!pending[i]) continue;
            const auto& input = inputs[i];
            GuestPeLoad attempt = pe_prepare_guest_with_shim(
                input.file_bytes, input.file_size, shim, options, bank,
                *input.segment, input.source_path);
            if (!attempt.ok)
            {
                if (attempt.reason ==
                        "unresolved imports (fail-closed; no null-filled IAT)" &&
                    only_waiting_for_pending(attempt.missing, keys, pending))
                {
                    deferred[i] = std::move(attempt.missing);
                    continue;
                }
                return fail(std::string("PE preparation refused for ") +
                            input.source_path + ": " + attempt.reason,
                            std::move(attempt.missing));
            }
            const GuestNotifyStatus status = attempt.module->process_attach(teb);
            if (status != GuestNotifyStatus::Ok)
                return fail(std::string("process attach refused for ") +
                            input.source_path + " status=" +
                            std::to_string(static_cast<int>(status)), {});
            result.input_indices.push_back(i);
            result.modules.push_back(std::move(attempt.module));
            pending[i] = false;
            --remaining;
            progressed = true;
        }
        if (!progressed)
        {
            std::vector<std::string> missing;
            for (size_t i = 0; i < inputs.size(); ++i)
                if (pending[i])
                    missing.insert(missing.end(), deferred[i].begin(),
                                   deferred[i].end());
            return fail("unresolved intra-batch imports; no DLL can attach",
                        std::move(missing));
        }
    }
    result.ok = true;
    return result;
}

} // namespace cap13
