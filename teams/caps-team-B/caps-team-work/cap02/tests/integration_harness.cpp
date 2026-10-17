#include <cstdio>
#include <optional>
#include <string_view>
#include "NativeLayoutDeclarations.hpp"
#include "RuntimeBuildProfile.hpp"
#include "CompiledNativeLayoutDeclarations.hpp"
#include "AutoProfileGenerator.hpp"
int main() {
  const auto& d = RC::Platform::NativeLayouts::kCompiledNativeLayoutDeclarations;
  auto sh = rc::autoprofile::shadow_check(d.members, d.vtables);
  std::printf("parsed=%d verdict=%s pack=%s member_rows=%zu vtable_rows=%zu reason=%s\n",
              sh.parsed, sh.matched?"MATCH":"DIVERGE", sh.pack_id, sh.member_rows, sh.vtable_rows, sh.reason);
  return sh.matched ? 0 : 1;
}
