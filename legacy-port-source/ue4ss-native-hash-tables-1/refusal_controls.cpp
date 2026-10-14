#include "source/NativeHashTablesUnavailablePolicy.hpp"
#include <cassert>
#include <string_view>
namespace Policy = RC::Unreal::HashTablesNativePolicy;
template<typename Func> void MustRefuse(Func&& Function, std::string_view Service)
{
    bool Refused = false;
    try { Function(); }
    catch (const std::runtime_error& Error) { Refused = true; assert(std::string_view(Error.what()).find(Service) != std::string_view::npos); }
    assert(Refused);
}
int main()
{
    assert(!Policy::IsConfigured());
    assert(!Policy::IsAvailable());
    assert(!Policy::RunSelfTest());
    Policy::SetupGetAddress(nullptr);
    int OwnedOpaqueCandidate = 42;
    Policy::SetupGetAddress(&OwnedOpaqueCandidate);
    assert(OwnedOpaqueCandidate == 42);
    assert(!Policy::IsConfigured());
    assert(!Policy::IsAvailable());
    assert(!Policy::RunSelfTest());
    MustRefuse([] { Policy::RefuseSingleton(); }, "singleton");
    MustRefuse([] { Policy::RefuseLock(); }, "lock");
    MustRefuse([] { Policy::RefuseUnlock(); }, "unlock");
    std::puts("Native hash policy refusal controls passed.");
}
