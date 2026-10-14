#!/bin/bash
# session_run.sh — thin wrapper for the session_run.py archive helper.
# Same subcommands: start | note | capture | verdict | close | checklist
# Run `./session_run.sh checklist --flow dll8` or see README.md for the
# archive-law usage of the return session.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
exec python3 "$HERE/session_run.py" "$@"
