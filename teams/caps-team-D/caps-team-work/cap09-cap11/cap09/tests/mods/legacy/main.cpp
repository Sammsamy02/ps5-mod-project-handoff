// Stand-in for a mod built against the real UE4SS C++ ABI: exports only
// start_mod / uninstall_mod (no SDK entry). The loader must refuse it. Test-only.
extern "C" __attribute__((visibility("default"))) void* start_mod() { return nullptr; }
extern "C" __attribute__((visibility("default"))) void uninstall_mod(void*) {}
