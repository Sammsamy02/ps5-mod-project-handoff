# l23 delivery — ready for review, Mac-only

l23 retains the same 93 layout facts and ships no new Map/Set offsets. The complete affected closure was rebuilt twice: 125 fresh units per pass, 249 unchanged retained units, 391 total link inputs. Runtime twins match; both packaged PRXs match. All 20 static gates pass per package; the exact 207 import identities are unchanged.

PRX: **31,324,743 bytes**. SHA256: `0da199eb954529bbe8fdc563a8c58d76853d560feba0230ad2fd4374b3f0d4e1`. SELF magic `4f153d1d`; SDK `0x11000000`.

The full dependency build caught two custom-array assignments incompatible with by-value getters. The final r2 code uses bounded writes only into freshly allocated runtime-owned objects. Review also corrected a Next read that preceded pointer validation. Focused regressions and an independent review cover both corrections. They add no offsets or game-code writes. The first failed build remains preserved and was never packaged.

The PRX includes non-fatal optional container discovery, deduplicated missing-key collection, a narrow core-type gate, validate-on-use pointer getters, and the actual ExecuteInGameThread queue self-test. The original mod remains unchanged. Queue delivery, menu and spawn are still awaiting console evidence. VMT fallback remains design-only.

## Track B results

The authorized generic-platform route now measures **459 of 711** keys. Against all 93 existing facts: **84 match, six mismatch, three remain UHT-dependent**. Five mismatches are FUObjectArray fields/size affected by public Unix lock storage; the sixth is ProcessEvent's conditional IRIS virtual. An IRIS-enabled control matches that slot but is not substituted as proof of the game's macro. The complete generated table is not accepted wholesale.

New values are split into **314 platform-independent candidates** and **55 platform-dependent candidates requiring image coverage**. All four container measurements agree with the saved image; the three new values remain deferred to l24. None entered l23. The earlier full Sony-header blocker is superseded by the user-approved generic route. No Sony platform-extension material was sought in this round.

Start with FINAL-RECEIPT.json, FINAL-DIFF-REVIEW.md, independent-review/R2-INDEPENDENT.md and track-b/REPORT.md. The artifact is in artifact/. All included files are hash-verified in MANIFEST.json. Epic source and raw diagnostics quoting it remain local and are excluded.

**No console contact, upload or launch occurred. Upload awaits the user's OK for this exact PRX.**
