# Current PS5 test

Select and run **TEST_THIS_DLL.elf** once in Payload Manager. Leave FTP on afterward. This is the current standalone DLL diagnostic; no Townfall launch is needed.

This is now the f72faea5 entry + clock diagnostic, uploaded after the previous db1c40e6 DLL call test returned 86/93 and retired cleanly on PS5. It checks DllMain attach/detach and Windows QueryPerformanceCounter/QueryPerformanceFrequency through the native clock service. Its result path is `/data/modbridge-owned-dll-candidate.jsonl`; records are selected by the full build identity `f72faea594794fd28bc7571177411817`. Uploading does not execute the new test.

SHA-256: `588d6e6ddf476fe764153df9a3235a3558f0e98e2a3b4b6070a6fb645c47bff4`.
