# example-emission — SYNTHETIC HOST FIXTURE ONLY

The files under `synthetic-fixture/` were produced by
`validation/test_assembler.py`'s synthetic image (hypothesis A, 16 items) run
through `min_predicates.py` -> `assemble_staged_pair.py`. The identity values
are explicitly named `SYNTHETIC-FIXTURE*`; they are NOT scan facts, NOT target
evidence, and MUST NOT be staged into any runtime. The pair exists solely to
show the exact output grammar and the assembly-manifest provenance shape.

Nothing here validates any Townfall value. ACCEPTED.md remains the only place
accepted game data may appear.
