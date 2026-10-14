from pathlib import Path
here = r'/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-minimal-path-test/tests'
Path(here+'/loadlist.txt').write_bytes(bytes(range(16)))
