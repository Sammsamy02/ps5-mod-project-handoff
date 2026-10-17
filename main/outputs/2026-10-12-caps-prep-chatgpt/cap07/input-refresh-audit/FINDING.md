# Local input refresh — fresh Mac-only metadata check

No newly provenance-complete chunk10 store or required base-export payload was identified in the 11 named retained capture/prep directories. This is a scoped inventory, not a claim that the inputs are absent everywhere. No UCAS, TOC or binary payload content was read.

The scope follows the existing base-identity audit's retained header/TOC sources and the previous local-payload-search receipt: dependency-header captures1–3, original chunk0 header, main retained TOC directory, dependency-owner/read-plan/compatibility prep directories, cap07/evidence and the cap08 identity/intake folders. Exact paths, existence,124 file sizes and32 JSON metadata hashes are in `inventory.json`. No metadata file exceeded the2MiB bound or failed JSON decoding.

The five JSON references to required raw IDs are only the two known partial-header receipts, the existing membership/audit requirements and the placeholder export-intake example. None binds a newly supplied complete payload. There were no .bin files with one of the exact required whole lengths. The retained chunk0 ContainerHeader remains an existing different-owner input; it cannot replace chunk10's store. The partial headers stay quarantined because whole-chunk IoHash and atomic/control-completion provenance are missing. Anonymous files and synthetic fixtures are not admitted.

| Required role | TOC row | Raw chunk ID | Whole decoded bytes | Expected whole IoHash20 |
| --- | --- | --- | --- | --- |
| `owner_store` | 11 | `5b160bf69b8b988d00000006` | 416 | `c5e339d2dab53bb538d4d554d98e2516bf30a5f3` |
| `base_export` | 14 | `25effae0427fb31f00000001` | 122393 | `e138b6e53deb1dc665df2321107fe1bf70f44228` |
| `base_export` | 5 | `8527d3a1cbfa772c00000001` | 1508 | `597884ac2e57af5100097e846814e85c4edfd468` |
| `base_export` | 29293 | `eeb296b2a739309500000001` | 500891 | `4b45da629db4ad60864b4f19b16372b6d3e6c9e9` |
| `base_export` | 7 | `a73a5659c6a46b9700000001` | 1198188 | `b0640818cef6ee7ab498dcd121fb1f7864481b29` |

Full owner TOC SHA256 and container IDs are preserved per requirement in `inventory.json`. Chunk10 rows11/14/5/7 belong to owner TOC892a6327…9f409c0, container8d988b9bf60b165b. Chunk0 row29293 belongs to owner TOC35de2752…0bb75b5, container8f8bb2ccc07563e0. Package5dd733019f47199a still has no captured owner and needs a complete owner catalogue; this inventory cannot invent a selector for it.

Fresh hash check:56 cap07/cap08 source and historical test-log/external parser bindings match current `VERIFICATION.json` schema11. `compose_by_owner.py` also matches the existing real-composition refusal's hash. This preserves the previous test binding; tests were not rerun and no new passing behavior is claimed. The requirements audit remains SHA256898a9444814011d77d73ce740bef118d23f9e19837d4e1af336b78198acfc1ce.

No new intake command was filled because no eligible new input was found. The existing cap08 `export-intake.example.json` remains a placeholder. Before preparing a concrete job, a supplied complete decoded payload needs exact owner TOC/container/row/rawID/full length/whole IoHash and payload-SHA provenance; the intake tool then independently checks its bytes. No extraction, acquisition, source mutation or compatibility promotion occurred.

Reproduce from repository root:

```sh
PYTHONDONTWRITEBYTECODE=1 TMPDIR="$PWD/outputs/2026-10-12-caps-prep-chatgpt/cap07/input-refresh-audit/tmp" python3 outputs/2026-10-12-caps-prep-chatgpt/cap07/input-refresh-audit/inventory.py
```

Fresh result:11 scopes,124 file stats,32 JSON metadata records,56 matching schema11 source/log hashes;0 payload reads;0 inputs admitted. The script fails if its nonempty source binding changes or the authoritative schema differs. The source/capture files remain read-only; only this folder is written.
