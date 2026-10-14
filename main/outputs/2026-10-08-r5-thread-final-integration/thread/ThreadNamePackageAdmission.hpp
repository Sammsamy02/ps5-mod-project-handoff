#pragma once
extern "C" int bridge_native_thread_import_ready(void);
namespace NativeNameThread {
// Final packaging independently verifies NID and module/library identities.
// This checks only the owned typed relocation row at startup. No claim that
// local SDK declarations alone prove resolution or native engine execution.
inline bool exact_import_admitted() noexcept { return bridge_native_thread_import_ready()==1; }
}
