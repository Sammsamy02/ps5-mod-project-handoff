# Owned DLL synchronization runtime

The actual Windows-format PE consumer can now import and use seven critical-section APIs, eleven SRW/condition-variable APIs and three address-wait APIs. Its internal Windows callback runs on a native worker, with PE code retained through join. This closes concrete service gaps in the retained DLL import census; it does not establish general downloaded DLL mod compatibility.

The opt-in `--entry --clock --workers --sync` composition completed 73 commands, including actual host PE execution, a genuine PS5 SDK payload link, unchanged Win64 calling conventions and all SDK provider declarations. Both normal and release-retry runs returned 86/93, completed the two worker exports with DWORD 0x81234567, preserved its parameter cookie and thread-local error, and retired all owners. The DLL consumer binds 44 imports and its helper binds 10.

An invalid callback parameter is an explicit host-only control. A finite, status-aware condition wait detects that finished callback instead of hanging indefinitely. Its 18 commands demonstrate the exact failure and cleanup retirement, including a subsequent release retry. Separate admission-leave failure tests preserve the initial error without repeating worker exports.

The three service owners have independent pthread/Win64 controls and PS5 object checks. Address-wait and SRW controls also pass TSan; all three pass ASan/UBSan. The no-sync worker regression completed 19 commands and retains byte-identical DLL fixtures from the earlier verified worker build. The exact receipts and current checkpoint are beside this document.

Address-wait admission requires naturally aligned unsigned 1/2/4/8-byte scalar objects, matching-width atomic stores and a predicate recheck after wake. The PE worker's scheduling can bypass its blocking wait; deterministic lost-wake and wake-selection evidence comes from the separate service tests. TryAcquireSRW uses an 8-bit BOOLEAN; critical-section TryEnter uses a 32-bit BOOL. Native synchronization state lives in owner sidecars rather than Windows object bytes.

The new 240,960-byte payload is build 6d295f566d084f578836b245a7748391, SHA-256 0dc08b3766c6d8d690745562462ced538030c9be11883f9318b2e3f53473b724. It has not been uploaded or run on the PS5. It is a standalone SDK ELF diagnostic, not a game PRX or a UE4SS mod test. The previous f72 startup/clock run now has exact console evidence; it does not prove this newer worker/synchronization profile.

Only owned fixture code executes during local verification. Build outputs, SDK bodies, game captures, credentials and unrelated dirty app work are excluded from source publication.
