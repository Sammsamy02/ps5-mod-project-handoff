# Native Lua owned int32 read

Preserve actual LuaLibrary::deref_to_int32 namespace/signature and exactly one integer argument. The argument is only a label; it never grants read authority. Negative/null/unrepresentable labels, unknown/unreadable/short source ranges, and registration/read failures return Lua nil. A successful full 4-byte int32 read returns its actual signed integer including zero.

Coordinate stable genuine PLH owned-region API. Register only the function's stack destination for its synchronous read lifetime; revoke registration before any Lua return-value push can longjmp. Source permission is externally supplied by an explicitly owned registration. No unchecked dereference, process memory provider, engine memory, mapbase payload, console operations or game reads.

Compile the additive TU under the actual PS5 SDK profile. Build owned controls through real LuaMadeSimple and LuaRaw, actual owned-region service, and real MemAccessor; cover successful positive/zero/negative/extreme values, wrong types/arity, unknown/expired/unreadable/short/boundary labels and destination cleanup. Native caller compile/link only; host may execute owned registered C++ values. Preserve exact source changes, inputs, commands and symbols. Root performs composition.
