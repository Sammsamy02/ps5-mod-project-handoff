/* Owned no-entry provider DLL: exported code and data used through its import. */
typedef unsigned long long u64;
static u64 bias = 9;
static u64 *volatile retained_bias = &bias; /* DIR64 relocation */
__declspec(dllexport) u64 ProviderCompute(u64 value)
{
    return value * 7 + *retained_bias;
}
