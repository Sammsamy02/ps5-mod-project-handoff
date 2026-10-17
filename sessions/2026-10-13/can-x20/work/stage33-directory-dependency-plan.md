# Stage 33: directory-backed static DLL source

Base: isolated stage 32. Add a generic POSIX directory source for immediate neighboring DLLs, with case-insensitive exact-basename lookup, duplicate ambiguity refusal, symlink and non-regular-file refusal, bounded reads, and a shared guest segment backend. No game names or per-mod addresses. Use with stage 32's closure API. Test real file discovery, ambiguous names, symlink refusal, and missing names; rerun regressions, original synthetic E2E, exact patch replay. Do not claim transitive game execution or PS5 behavior.
