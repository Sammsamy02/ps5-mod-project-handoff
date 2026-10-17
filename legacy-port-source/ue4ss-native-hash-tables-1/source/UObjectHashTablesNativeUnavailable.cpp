// Explicit unavailable native optimization; the actual upstream array fallback remains.
#if defined(__PROSPERO__)
#include <Unreal/UObjectHashTables.hpp>
#include "NativeHashTablesUnavailablePolicy.hpp"
namespace RC::Unreal
{
    bool FUObjectHashTables::IsConfigured() { return HashTablesNativePolicy::IsConfigured(); }
    bool FUObjectHashTables::IsAvailable() { return HashTablesNativePolicy::IsAvailable(); }
    bool FUObjectHashTables::RunSelfTest() { return HashTablesNativePolicy::RunSelfTest(); }
    // The opaque candidate is neither retained, classified, called nor read.
    void FUObjectHashTables::SetupGetAddress(void* Address) { HashTablesNativePolicy::SetupGetAddress(Address); }
    FUObjectHashTables& FUObjectHashTables::Get() { HashTablesNativePolicy::RefuseSingleton(); }
    void FUObjectHashTables::Lock() const { HashTablesNativePolicy::RefuseLock(); }
    void FUObjectHashTables::Unlock() const { HashTablesNativePolicy::RefuseUnlock(); }
}
#endif // defined(__PROSPERO__)
