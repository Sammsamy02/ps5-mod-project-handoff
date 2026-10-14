// Vendored copy (verbatim) of outputs/2026-10-04-zcode-team/worker-real-input/
// vendored/keydef_modifier_ops.cpp — original provenance preserved below.
// Vendored for rung 5 by worker-real-input, 2026-10-04.
// The retained KeyDef.cpp pulls fmt/core.h + Helpers/String.hpp; only the
// ModifierKeys member operators declared in the retained KeyDef.hpp
// (.../first/include/Input/KeyDef.hpp:264-273) are needed on this host-test path.
// Definitions below are verbatim from .../first/src/Input/KeyDef.cpp:217-268.
// Nothing else from KeyDef.cpp is referenced by Handler.cpp on this path.
#include <Input/KeyDef.hpp>
namespace RC::Input
{
auto ModifierKeys::operator|(const ModifierKeys& rkeys) -> ModifierKeys&
{
    keys |= rkeys.keys;
    return *this;
}
auto ModifierKeys::operator|(const ModifierKey& key) -> ModifierKeys&
{
    if (is_modify_key_valid(key)) keys |= (1 << key);
    return *this;
}
auto ModifierKeys::operator|=(const ModifierKeys& rkeys) -> ModifierKeys&
{
    return *this = *this | rkeys;
}
auto ModifierKeys::operator|=(const ModifierKey& key) -> ModifierKeys&
{
    return *this = *this | key;
}
auto ModifierKeys::operator==(const ModifierKeys& key) const -> bool { return keys == key.keys; }
auto ModifierKeys::operator<(const ModifierKeys& rkeys) const -> bool { return keys < rkeys.keys; }
auto ModifierKeys::operator>(const ModifierKeys& rkeys) const -> bool { return keys > rkeys.keys; }
auto ModifierKeys::operator!=(const ModifierKeys& key) const -> bool { return keys != key.keys; }
ModifierKeys::ModifierKeys(std::initializer_list<ModifierKey> keys) : keys{0}
{
    for (auto key : keys)
        if (is_modify_key_valid(key)) this->keys |= (1 << key);
}
}
