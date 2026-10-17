# Stage 27: mapped PE ordinal exports

Base: `cg-mapped-module-registry-20261013-26`. Work only in the isolated `cg-ordinal-module-exports-20261013-27` copy.

1. Add a failing host test for fixture ordinal `#1`, missing ordinals, and an ordinal-importing dependent DLL. Keep the source fixture unchanged.
2. Validate the PE32+ ordinal thunk encoding. Resolve mapped PE ordinals through the existing attachment gate and executable export check. Reject forwarders and invalid entries.
3. Run the focused and full offline regressions. Package an exact incremental patch, manifest, and evidence from stage 26. Update reviewer handoff and status.

No console execution or claim of universal compatibility.
