#pragma once
#include <cstddef>
#include <cstdint>
namespace RC::Unreal { class FField; class FFieldClass; class FProperty;
namespace NativePropertyGuard {
using ReadBytes = bool (*)(uintptr_t, size_t, void*);
using ImageClassRange = bool (*)(uintptr_t, size_t);
// Install only after image identity and layout publication. Callbacks have
// process lifetime; no owner captured. Revoke before teardown. No re-install.
void Install(ReadBytes, ImageClassRange);
void Revoke() noexcept;
FProperty* ReadPointer(const void* owner,int offset,const char* key);
FFieldClass* ReadClass(const FField* field);
}
}
