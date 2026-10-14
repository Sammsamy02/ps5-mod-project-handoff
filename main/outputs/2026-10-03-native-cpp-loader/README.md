# Native C++ loader continuation

This source continuation replaces the production PS5 CppModModule's generic dlopen path with direct native load, export lookup and unload APIs. It preserves non-PS5 behavior, handles valid handle zero, and retains failed-unload state without an implicit retry. Claude's activation, archive and installer work is retained. The remote Claude branch still matches the imported 43a96969 source.

The two changed production header consumers were rebuilt and integrated with the full existing 359-unit runtime, keeping 357 units, bootstrap, controller input, Claude activation and all fixed inputs. The new relocatable object is 116576216 bytes / SHA-256 0ea73d04e95dbf23eb573ef0bdff4b9d114c9b2ad7331b4349e84e80838738bc. Three native API imports replace five unresolved generic loader callbacks. Production source and composition reviews found no material issues. Component checks passed 14 native-stand-in groups, real host POSIX regression and native compilation.

Apply the earlier source handoffs and reconstruct the old coherent source snapshot/build before applying these two canonical CppModModule files. Their old bytes are also preserved in native-cpp-module-direct-1/baseline. See native-coherent-cpp-loader-1/README.md for the exact rebuild order and --base-receipt recipe. No old composed runtime object is needed, but a freshly rebuilt pad base's per-unit outputs are.

The archive also preserves two completed, bounded offline investigations: the FNameToString SkySphereMesh anchor is absent from Townfall; a genuine entry/FDE/direct-call reachability pass did not reach the Tick anchor, with no budget exhaustion. No candidate was guessed and no production rule was promoted from those results. Their experiments use source/tool prerequisites from the earlier handoff and never execute saved game bytes.

This is source and text evidence, not a deployable module or a successful C++ mod/game test. UE4SS engine binding, Item Spawner, child-module packaging/lifecycle integration and arbitrary Windows DLL compatibility remain unfinished.
