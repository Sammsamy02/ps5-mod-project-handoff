/* Behavioral RED stub: deliberately retain the old Bool/Int32 leaf while
 * discarding only the newly introduced reference-provider extension. */
#include "../owned_binding.h"

int lba_install_with_refs(lua_State *L, const lba_provider *provider,
                          const lba_bool_provider *bool_provider,
                          const lba_ref_provider *ref_provider,
                          const char *name, size_t name_length) {
    (void)ref_provider;
    return lba_install_with_bool(L, provider, bool_provider, name, name_length);
}
