# cap12-resourceindex (Mac-only) - file index

| File | What it is |
|---|---|
| `gsc_codec_m1a_copy.py` | verbatim copy of `cap12-m1a-20261012/gsc_codec.py` (sha256 9d1e9de4...656917); imported, never edited |
| `gsc_codec_ri.py` | extended codec: M1a layer + full per-shader object decode/encode (`roundtrip`, `decode`, `encode`); its proofs include de-dup accounting, resource-list partition vs shuffled baseline, slot order vs sort_key bytes |
| `ri_negative_controls.py` | one negative control per newly decoded field (first AND every differing byte must lie in that field) + checker spill self-test + 4 fail-closed controls; exits non-zero on 0 checked or any fail |
| `ri_crosscheck.py` | ResourceIndex/sort_key vs `.stinfo` sidecar cross-check; writes the two JSON files below |
| `roundtrip-receipt.json` | round-trip sha256 receipt + all decode proofs (and the M1a layer's own proofs, re-run) |
| `negative-controls-ri.json` | results of `ri_negative_controls.py` |
| `stinfo-crosscheck.json` | results of `ri_crosscheck.py` |
| `m1b-expectations.json` | per-map predictions plus the numbered M1b checks 1-6 (`m1b_checks`, incl. stop rule) for the later M1b library fetch (check-only, never ship) |
| `shader-objects-summary.json` | compact per-shader table (class, type, ResourceIndex, sort_key, binding counts) |
| `run-log-final.txt` | output of the final roundtrip + controls + cross-check run, with script sha256s |
| `MANIFEST.sha256` | sha256 of every file here + the M1a dir before/after fingerprint |

REPORT.md: writing it from the worker subagent is blocked by the harness ("subagents return
findings as text"); the full report text is in the worker's returned result for the parent to save.

Re-run (about 4 min in total, the controls take ~3.7 min; Mac-only, no network):
`python3 -B gsc_codec_ri.py roundtrip <bin> roundtrip-receipt.json --summary shader-objects-summary.json`;
`python3 -B ri_negative_controls.py <bin> negative-controls-ri.json`; `python3 -B ri_crosscheck.py <bin> <stinfo> .`
