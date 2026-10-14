# Privacy review and rebuilt handoff — 2026-10-14

The initial upload's address scan missed literals joined directly to words. Two
historical control ledgers and a generated package inventory were removed. Actual
computer identifiers were redacted from two historical test notes/receipts. The scan
now detects addresses attached to words, and privately supplied machine identifiers.
Five focused regression checks passed after reproducing three failures in the old scan.

The entire retained `sessions/` tree was read and scanned for conversation, account,
machine and control-record content. An unrelated account/checkpoint record and an
unrelated local-storage maintenance session were excluded. The specifically flagged
`CLAUDE-RESUME-HANDOFF.md` was reviewed: it contained technical status and historical
operator directions. It was omitted in favor of the current handover. Project source,
the 62 #1/#3/DLL stages, the 95 listed scratch scripts and current technical handoffs
remain available.

The rebuilt handoff starts from one new root commit. It does not inherit either
previous upload commit. A fresh GitHub repository is used for the cleaned handoff at
the original URL so the old upload objects are not part of the repository being shared.
The previous upload is kept only in a separate private, archived repository. Do not
make that retired archive public or invite collaborators to it.

Original source worktrees and chat workspaces on the Mac were not changed. Removed
copies and private audit identifiers remain only in gitignored local review storage.
No console access, upload, launch, collaborator invitation or public visibility change
was performed as part of this cleanup.

`tools/verify_export.py` checks exported source hashes and exclusions. Its optional
`.local/privacy-machine-identifiers.json` input stays private and is never uploaded.
The new source manifest records any redacted file with its current exported hash;
historical build receipts still require regeneration before use as release proof.

The repository's original work still has no newly selected project-wide open-source
license. Existing upstream licenses and notices apply to their respective components.
Public visibility alone does not replace those licenses or select a license for the
original project work.
