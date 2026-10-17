#include "guest_pe_dynamic_source.h"

#include <cstring>
#include <utility>

namespace cap13 {
namespace {

bool same_basename(const char* requested, const char* path)
{
    if (!requested || !*requested || !path || !*path) return false;
    for (const char* at = requested; *at; ++at)
        if (*at == '/' || *at == '\\' || *at == ':' ||
            static_cast<unsigned char>(*at) < 0x20) return false;
    const char* basename = path;
    for (const char* at = path; *at; ++at)
        if (*at == '/' || *at == '\\') basename = at + 1;
    for (; *requested && *basename; ++requested, ++basename)
    {
        const char left = *requested >= 'A' && *requested <= 'Z' ?
            char(*requested - 'A' + 'a') : *requested;
        const char right = *basename >= 'A' && *basename <= 'Z' ?
            char(*basename - 'A' + 'a') : *basename;
        if (left != right) return false;
    }
    return !*requested && !*basename;
}

} // namespace

WinModule pe_load_guest_from_source(void* user, const char* dll_basename)
{
    if (!user) return nullptr;
    auto& context = *static_cast<GuestPeDynamicLoadContext*>(user);
    context.last_error.clear();
    if (!context.fetch || !context.shim || !context.bank || !context.teb ||
        !context.retained || !dll_basename || !*dll_basename)
    {
        context.last_error = "invalid dynamic PE load context";
        return nullptr;
    }
    if (context.retained->size() >= context.retained->capacity())
    {
        context.last_error = "dynamic PE retention capacity exhausted";
        return nullptr;
    }
    GuestPeFetchedDependency root;
    if (!context.fetch(context.fetch_user, dll_basename, &root))
    {
        context.last_error = "dynamic PE basename unavailable from source";
        return nullptr;
    }
    if (!same_basename(dll_basename, root.source_path.c_str()) ||
        root.bytes.empty() || !root.segment)
    {
        context.last_error = "dynamic PE source returned a mismatched basename or invalid image";
        return nullptr;
    }
    GuestPeBatchResult result = pe_attach_guest_closure(
        {root.source_path.c_str(), root.bytes.data(), root.bytes.size(),
         root.segment}, context.fetch, context.fetch_user, *context.shim,
        context.options, *context.bank, *context.teb);
    if (!result.ok)
    {
        context.last_error = result.reason;
        if (result.rollback_failed)
            context.retained->push_back(std::move(result));
        return nullptr;
    }
    WinModule root_handle = nullptr;
    for (size_t i = 0; i < result.input_indices.size(); ++i)
        if (result.input_indices[i] == 0 && result.modules[i])
        {
            root_handle = result.modules[i]->mapped_handle();
            break;
        }
    if (!root_handle)
    {
        context.last_error = "attached dynamic PE root has no mapped handle";
        if (!pe_detach_guest_batch(result, *context.teb))
            context.retained->push_back(std::move(result));
        return nullptr;
    }
    context.retained->push_back(std::move(result));
    return root_handle;
}

} // namespace cap13
