# Stage 32: automatic static PE dependency closure

Base: isolated stage 31. Work only in `cg-dependency-closure-20261013-32`.

Add a bounded read-only PE import-directory scanner. Given one root DLL and a generic dependency-source callback, discover imported DLLs recursively by basename. Exclude already registered or built-in shim modules, reject mismatched source identities and malformed import metadata, then attach the discovered closure through stage 31's ordered batch. No per-game names or addresses.

Host evidence: root consumer discovers and calls its provider; a transitive chain is collected; missing and cyclic dependencies refuse; malformed import data refuses before guest entry; five unchanged DLLs scan read-only with hashes unchanged. Re-run full regressions, patch replay, and update handoff. No console execution or claim of universal compatibility.
