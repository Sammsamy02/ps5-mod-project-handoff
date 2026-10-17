# l17 local lifecycle plan

Approved scope: descriptor lifetime and initializer ownership only. Preserve l16 source/captures/artifacts and all admission, fault, signature, ABI, mod and pad behavior.

1. Copy frozen inputs and write behavioral tests for unlink, acquisition failures, FD reuse, owner-thread refusal, initializer success/refusal/exception cleanup, retry exhaustion and actual owner join ordering. Record red against the copied l16 code.
2. Publish the actual initializer ID through a synchronized gate in the selected runtime owner; foreign probe/capture/fault queries must refuse before unsynchronized state.
3. Unlink each uniquely created startup sink immediately after successful open; log denial and retain the file. Log every acquired descriptor and ordinal.
4. Add once-only initializer check-scope cleanup at seed consumption entry, explicitly complete it immediately after FName signature, native call and roundtrip checks. Keep ledger/captures for the remaining initializer. Close-error descriptors are consumed before close and never retried.
5. Run retained tests and new extracted initializer/real-owner cases in normal, fatal ASan/UBSan and supported TSan builds, plus focused native cross-compiles and hook/import relocation checks. Report exact receipts, limits and source diff. No console proof is claimed.
