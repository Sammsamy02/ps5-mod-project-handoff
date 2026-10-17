# B5 host test: lock-then-raise leak (Mac-only, no console contact)

Run: `./build.sh` (checks free memory >= 2 GB, extracts the real B2 wrapper code, compiles the real
Lua 5.4.7 C sources as C one file at a time, builds and runs the test). Output of the last run: RESULT.txt.
`RESULT-mutant-NEW-uses-throw_error.txt` = same test with the NEW variant sabotaged to use throw_error
under the guard: must FAIL (it does: 12 failures), proving the checks can fail.

Real code used (extract_real.py, byte-for-byte from the candidate, one documented fmt->concat adaptation):
LuaMadeSimple.cpp:22-29, 63-90, 1534-1607 and LuaMadeSimple.hpp:842-858 (throw_error_impl, handle_error,
throw_error_from_buffer, capture_cxx_exception_text, the free TRY). Only process_lua_function is minimal
(lookup replaced by a vector); it ends in the same `TRY(lua_state, [&]{...})`.
Lua sources: .superpowers/native-dependency-spike/ue4ss-lua-vm-experiment-1/upstream/re-ue4ss/deps/first/LuaRaw
(32 files, compiled with `clang -x c`; ldo.o imports _setjmp/_longjmp, no C++ personality).
