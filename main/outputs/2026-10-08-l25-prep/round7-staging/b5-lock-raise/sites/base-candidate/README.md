Pristine B4 candidate `src/LuaMod.cpp` (sha256 must be `4d6bc63b7a90a8026a9c918ee6bee36c2d4eaa450220be82e8071f5427bfde9a`),
reconstructed in round 8 by reverse-applying `../final-b5.diff` to the candidate file, because at 10:04 another workflow
applied final-b5 to the live candidate tree (candidate LuaMod.cpp is now sha afaf5caa...). The reverse result matches the
documented base sha, so the reconstruction is exact. Used by `gate/make_lists.py` (compiled-files-b4.txt) and
`b6-diagnostics/tests/verify_patch_chain.sh`, so the checks keep proving "base -> final-b5 -> b6" whatever state the live tree is in.
