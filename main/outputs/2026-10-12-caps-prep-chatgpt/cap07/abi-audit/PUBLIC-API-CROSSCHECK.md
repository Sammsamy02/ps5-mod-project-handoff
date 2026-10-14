# Independent public API comparison — Mac-only

Checked official Epic API documentation on 2026-10-12. The accessible current pages identify Unreal Engine5.8; explicit5.6 page requests failed. These are semantic comparisons, not evidence of the captured5.6 vtable layout or target platform ABI.

[Official OpenWrite documentation](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/IPlatformFile/OpenWrite?lang=en-US) describes a handle-returning overload with a filename and two boolean controls, for append and read sharing. That shape agrees with the observed +0x108 path and its returned object's destination role. The same page also documents a newer flags/result overload: version-specific layout cannot be inferred from the current method list.

[Official IPlatformFile reference](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Core/IPlatformFile) distinguishes directory existence (a directory argument and boolean result), visitor-based iteration, and asynchronous opening (a filename plus a boolean or flags overload). This agrees with the +0x110 predicate followed by a separate visitor dispatch; it does not establish a numeric slot or confirm the old async labels.

Conclusion: source/API agreement supports the proposed write-open and directory-predicate meanings. Exact5.6 declaration order, compiler/platform ABI, actual read route, live receiver/lifetime and two-source pin admission are still missing. No callable thunk or runtime value is admitted by this comparison. See FINDING.md for independently captured binary observations; no Epic source is copied here.
