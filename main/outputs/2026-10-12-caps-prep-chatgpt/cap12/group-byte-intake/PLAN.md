# Implementation plan

Extract existing stable whole-logical-buffer gate in manifest-v1; retain type8 wrapper semantics and add actual TOC owner/container attestation and exact raw type validation. Pin resulting reader in group-plan and new group-bytes adapter. Revalidate type8 bytes and raw09 logical bytes independently, using the same verified overlay bytes/full lookup gate. Explicit actual owner required for type9. Preserve existing reserved-header relaxation and source-scope limits; never open UCAS provenance paths.

Generate bounded metadata only: whole-buffer SHA/IoHash attestation, manifest/group identity, stored hashes unverified, selected sorted member spans/content SHA256 (not shader identity), positive-prefix accounting. Unequal sizes: CODEC_REQUIRED and zero views. Bounds: supplied payload4MiB each/8MiB total, receipt2MiB/TOC32MiB inherited, selections≤16, output8MiB. No payload output.

Initial14-test suite RED (adapter absent). Implement shared gate and adapter; run new suite and affected manifest/planner suites, then one retained synthetic CLI replay. Bind new hashes; historical manifests/receipts remain untouched and labelled historical in new README/receipt.
