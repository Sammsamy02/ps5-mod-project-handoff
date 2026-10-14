# Follow-up Mac storage inspection and cleanup

## Verified result

Recovered another 7.42GiB in allocated-file accounting: 18 old trial copies removed (3.30GiB), and 32 old l10–l12 aside linker maps compressed in place (4.11GiB saved). Original fixtures rehashed after removal; each compressed map verified SHA256-identical before and after replacement, with path, logical length, mode/owner and modification time retained. Sources, PRXs, source/receipt evidence, current builds and all history preserved. No PS5 contact/actions. Mac free space measured 31.44GiB.

Earlier cleanup removed21.56GiB of duplicate generated artifacts and old cache data. Combined allocated-size reduction across these cleanups: 28.97GiB. Physical free-space change can differ because of APFS and concurrent processes.

## What “worktrees” means here

There is one large managed checkout: /WORKSPACE/.codex/worktrees/translation-core/what. Before this follow-up it used124.8GiB. About66.5GiB was dependency/test experiments and55.3GiB was outputs; remaining data includes original observations. Before this follow-up, major file types were object files41.38GiB, link maps19.08GiB, PRXs14.80GiB, PAK test inputs7.21GiB, binary files4.42GiB, UCAS inputs4.36GiB, Rust libraries4.27GiB, and Rust metadata3.03GiB. These categories include both dependencies/needed artifacts and old experiments, so deleting an entire category would break reproducibility or current work. Full size inventory: PROJECT-SIZES.json.

## Conversation history — retained

323 transcript files total52.485GiB; every file has a distinct length, so none is an exact whole-file duplicate. July149 files account for50.212GiB. A single Open WebUI/plugin project family has146 transcripts totaling50.209GiB, including agent forks. These are not archived; root activity was October5,2026 despite July filenames. Current PS5 what history is about2.04GiB.

The largest sampled July file was2.27GiB:98.02% embedded image data,1,936 occurrences representing226 images; repeated copies are93.57% of bytes. Compaction history retains the repeated images. This measurement is one full sample, not proof all history has exactly the same composition.

Deleting the50.2GiB family would discard that project's user/agent history, not just cache. No such deletion was performed. Native transparent compression was tested separately on synthetic files: it worked for smaller examples but silently skipped compression on1GiB+ examples. Replacing live transcript files also risks concurrent appends. Therefore do not delete/edit/replace transcript contents blindly; any future removal needs explicit selection of the conversation family and a verified preservation decision.

## Audit files

FIXTURE-DUPLICATE-PLAN.json and FIXTURE-DELETED.jsonl map deleted copies to preserved originals. MAP-COMPRESSION-RECEIPTS.jsonl records unchanged hashes and before/after allocation. No active worktree source or shared project checkpoint changed by cleanup.
