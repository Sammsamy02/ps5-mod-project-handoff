# Build ledger — overnight 2026-10-12 (Mac-only; variant A only)

| Label | PRX sha256 | Snapshot | Build id | Gates | Package location now |
|---|---|---|---|---|---|
| B1-lookup-nocache | `b9cc52485c51e5ebbaf1944e01f472f65d19136e28d3a0ef965756b1936aa909` | 39e11f3e | l26-20261012T032343Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B1-lookup-nocache/` when the next build starts) |
| B2-error-wrapper-memlog (1st try) | — (no PRX) | c24acd37 | — | REFUSED by the include-graph gate (new `<atomic>` include in lms-luamadesimple); partial output kept in `archive-r6/B2-failed-include-gate/` | — |
| B2b-error-wrapper-memlog | `74fab85a5165c17a4d169f8efdb25e0e647fa7b867ed7843a7b4869a5f8c58b2` | 4fe7345b | l26-20261012T034011Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B2b-error-wrapper-memlog/` when the next build starts) |
| B3-iterators | `40b8972b58e313c61b84e2665d8c166887b49712753b000ff4f0cf919cfb9175` | 73e75292 | l26-20261012T035151Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B3-iterators/` when the next build starts) |
| B4-shim-final-stack | `d044cf0bf736b5f4399081eea764f59673d1d798a530572ab6897f8879c37599` | b63ba460 | l26-20261012T040323Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B4-shim-final-stack/` when the next build starts) |

## Safe (variant A) bisect points — corrected 2026-10-12

Labels inferred (Mac-only) from: each `archive-<X>` holds the build that was in
place when step X started, presence/absence of a variant-B package, and the
build stamp string inside each PRX. zcode's handoff listed two unsafe
variant-B files (2238dcb0, b8da51c7) as bisect points — do not use them.

| Point | PRX sha256 (prefix) | Stamp | Location |
|---|---|---|---|
| round-4 spawn-proven | `59b0528b77ca` | l26-20261011T030618Z | archive-p1/2026-10-08-l26-package-A |
| P1 fault-ledger | `cad3874cf214` | l26-20261011T153345Z | archive-p2/2026-10-08-l26-package-A (its unsafe B twin is b8da51c7) |
| P2 variant-B removal | `38c495281e7a` | l26-20261011T155833Z | archive-wf/2026-10-08-l26-package-A (no B package: post-P2) |
| pre-fixstack (lexical paths) | `d90563f5fe79` | — | archive-r5/pkg.pre-fixstack |
| final-stack (superseded) | `4d3f542245b9` | l26-20261011T190715Z | archive-r5/pkg.final-stack |
| rootfix | `67cafbd2ff5b` | l26-20261011T211306Z | archive-r5/pkg.rootfix |
| **1e49188b known-good** | `1e49188b3824` | l26-20261012T011649Z | archive-r6/pkg-1e49188b-acceptance-pass1 + read-only copy in 2026-10-08-l26-frozen/ |
| B1, B2b, B3, B4 | see the table above | | archive-r6/<label>/ once the next build starts; the newest stays in 2026-10-08-l26-package-A |
| B5-lock-raise | `28d8331f873b39eb2fe7bb806234638182b1f7e3701e42304741b4c8176335a1` | 4a14628b | l26-20261012T150608Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B5-lock-raise/` when the next build starts) |
| B6b-mem-diag | `41728f484b4f05451c4591b2609b7abe9c52fd5308e94568350d6843b5c05ffb` | 3b6b1b5e | l26-20261012T161020Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6b-mem-diag/` when the next build starts) |
| B6d-crash-guards | `fcb7bde40865cef3978bf44476c04786212d7a1685a470628556294e7c7f74d4` | 5071cd81 | l26-20261012T163913Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6d-crash-guards/` when the next build starts) |
| B6e-perf | `e339d9d63832482568807cbeb9f952321c129f7f46829e9f51f7d8232d77893e` | 5b4b8f12 | l26-20261012T170223Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6e-perf/` when the next build starts) |
| B6g-object-index | `0d14ef38b2fdea1d652ea06d9be83b00da49a22cac3ba1ce92346f19b6340996` | d428bd4e | l26-20261012T174140Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6g-object-index/` when the next build starts) |
| B6j-notify | `144a89b2b2f1ae1d0d1e0f4bb5937f07c29310a93ac8303e479e9d43b2f6363f` | 4b2600b6 | l26-20261012T181646Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6j-notify/` when the next build starts) |
| B6k-notify-contain | `57408fd81e86616a95268bf61feaff1c970c59d1d3a7ac82cbb80c69e8c083cc` | c60047c4 | l26-20261012T183553Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6k-notify-contain/` when the next build starts) |
| B6m-perf-notify | `079c30b7101f0ced539de27a02757a0f75afd1fffc09d7ded74ddc82b7f79830` | dec4b02a | l26-20261012T185423Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6m-perf-notify/` when the next build starts) |
| B6n-proven-fields | `348b4e0d032827be98e3e27b8dc989ae995745bf7a47c4a0d7e600d15c2fd5ff` | f93ce4da | l26-20261012T191035Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6n-proven-fields/` when the next build starts) |
| B6o-fast-slots | `22b511bf8ae48cef2de645bfc07b94a38ea2946d38e8de4c75736d44f40b351a` | 3a4b31dd | l26-20261012T195553Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6o-fast-slots/` when the next build starts) |
| B6p-watch-budget | `852d72cef942847b966ef1269e0e2223e217bba238213d2f62b1c7680e59545a` | fbb293b2 | l26-20261012T200810Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/B6p-watch-budget/` when the next build starts) |
| R10-combined | `06913711f6ece468e5db0dbe19d12a343503754e758e3a27666a659088f7dabf` | 4d83d987 | l26-20261012T221034Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R10-combined/` when the next build starts) |
| R10b-fields-fix | `dc2d570cc5f6b551538093b2e4a85367fd8ce9255e446f9c19496a4071bf9d6b` | 4a3f338b | l26-20261012T230106Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R10b-fields-fix/` when the next build starts) |
| R11-watch-world-uenum | `bfd95db6c44a91f4fdc36592dd884a075f1e63d66c98cd50530910ab83b30c3b` | 4513a2c6 | l26-20261013T010043Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R11-watch-world-uenum/` when the next build starts) |
| R13c-early-cap04 | `ab2642f320d88f6d95448128534d74569c11d4c571b951042c47cd5c77e4cf45` | e433ad4b | l26-20261013T052106Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R13c-early-cap04/` when the next build starts) |
| R14-cap02-cap05-b7 | `37e034a47befdab4affcadfe888fe7d1b746fdbd6829f3d0a3d109573040fb1d` | 06c5c750 | l26-20261013T152440Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R14-cap02-cap05-b7/` when the next build starts) |
| R15c-cap09-cap11 | `7e681540c0a681e3848870e31c3c130289900ac03f69b79c71acfc5b433d40ba` | b1169cc8 | l26-20261013T163810Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R15c-cap09-cap11/` when the next build starts) |
| R15d-cap11-namecase | `f54797e0321f86c2d87ab6b52412e61946f30c8d49a2b12b0eed8a0193a9a8ad` | f51a2a73 | l26-20261013T174834Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R15d-cap11-namecase/` when the next build starts) |
| R15e-cap11-layout | `53a645533eb2f627cb70d366939eb63096128ddeb13297072763905592f5b830` | 68ea2781 | l26-20261013T182805Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R15e-cap11-layout/` when the next build starts) |
| R16-cap07s2-pakmount | `15c23396d0aeb12e2a4e3d2bddeefdc8c8cdead7dfcdbc06966c9c4399d51e64` | c4e1fa2c | l26-20261013T231347Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R16-cap07s2-pakmount/` when the next build starts) |
| R16b-cap07s2-trigger | `9d42deba6258e53b940705f0aba0e45ee5780a7b452462fff76d61377e441ee3` | def79f35 | l26-20261013T235036Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R16b-cap07s2-trigger/` when the next build starts) |
| R16d-cap07s2-fsfix | `3b4c202bc6663a6bda46be7eacb02dcf6c11945d7707050d7a3804fbbb515ae0` | b9c06b12 | l26-20261014T002015Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R16d-cap07s2-fsfix/` when the next build starts) |
| R17d-combined-cap10-cap11fu-cap07s3 | `04c9fcc81ae27c59c1fa1703badd7b083581b4f703af5ebe8f08d636b458d905` | 0a3901c2 | l26-20261014T042407Z | fs-throw pre+post, no-detour, build, package P0-P13, verify_build: PASS | `outputs/2026-10-08-l26-package-A/` (moves to `archive-r6/R17d-combined-cap10-cap11fu-cap07s3/` when the next build starts) |
