# Actual-owner cleanup regression

Run `python3 run_owner_stop.py` from this directory, or pass the script's absolute path from any directory. It runs exactly four cases in plain and fatal ASan/UBSan builds. It extracts the current production sink/bridge-stop/supplier-stop bodies, compiles the actual selected `RuntimeOwner` source with real host worker threads, and records whether any descriptor closes before worker completion.

Cases: normal stop; exception injected before supplier stop, recovered by actual owner destruction; negative result after actual owner destruction; empty-owner cleanup. Every case requires all retained descriptors to be closed. The supplier body stays unchanged apart from its test-only symbol rename. A null acknowledgement sink is used, so unused acknowledgement link shims do not establish acknowledgement I/O or target unload safety.

Each run retains its generated extracts, binaries, exact compile/run commands, output and input SHA-256 hashes in `runs/<run-id>/RECEIPT.json`. `LATEST-RECEIPT.json` points to the latest evidence by retaining the same JSON contents. Probe fixture files are created below the run directory and removed after the cases. Production files and the main test fixture are read-only inputs. No console contact occurs.
