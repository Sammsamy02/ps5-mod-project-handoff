/* Owned no-entry consumer DLL; the function call uses the provider's real IAT
 * binding. The optional missing import variant is admitted as bytes but never
 * executed: it exists only to test failed-load dependency rollback. */
typedef unsigned long long u64;
__declspec(dllimport) u64 ProviderCompute(u64);
#if defined(DEPENDENCY_MISSING_IMPORT)
__declspec(dllimport) u64 UnimplementedFixtureApi(u64);
#endif
static u64 adjustment = 3;
static u64 *volatile retained_adjustment = &adjustment; /* DIR64 relocation */
__declspec(dllexport) u64 ConsumerCompute(u64 value)
{
    u64 result = ProviderCompute(value) + *retained_adjustment;
#if defined(DEPENDENCY_MISSING_IMPORT)
    result += UnimplementedFixtureApi(value);
#endif
    return result;
}
