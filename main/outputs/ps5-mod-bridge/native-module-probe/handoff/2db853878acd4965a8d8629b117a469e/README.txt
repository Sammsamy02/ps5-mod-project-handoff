Townfall log-only module test artifacts

Build: 2db853878acd4965a8d8629b117a469e
This bundle contains the existing compiled log-only module, unchanged, and a
single-title configuration template. It contains no UE4SS runtime or Ploader.

Module path in template: /data/plugins/modbridge_log_probe.prx
Configuration path expected by Ploader: /data/plugins/ploader.ini
Title: PPSA33286. Mode: 2.
If a configuration already exists, preserve it and merge this title entry;
do not replace unrelated entries with this single-title file.

The probe appends to /data/townfall_mod.log. The companion expected-log-lines.txt
shows the exact build marker and separate init_array/module_start entry tags.
An OK notification alone does not prove the log write or game-process entry.
Associate fresh markers with this module and the actual loading context.
This proves startup only, not UE4SS or mod compatibility.

File: SDK plaintext fake SELF, embedded ET_SCE_DYNAMIC ELF, 5757 bytes.
SHA256: ccb9085c9eb69e2d2bd7294e2666d6b2acbb6aa422e0e7572762f708053c20ce
Imports: sceKernelOpen, sceKernelWrite, sceKernelClose from libkernel.
No game-memory operations, hooks, network client or game-file modification.

OnionHEN's upstream README documents bundled kstuff for fSELF/fPKG and a
Toolbox switch. Check the installed build's release/source and its Toolbox
kstuff state. The exact installed 13.60 fork/build is not identified here;
the general documentation is not proof of native Townfall module acceptance.
Sources:
https://github.com/aydencharles/onionHEN#features
https://github.com/yourphonelinging0x0/ps5-plugin-loader#configuration

These local handoff files do not install anything or launch a loader.
