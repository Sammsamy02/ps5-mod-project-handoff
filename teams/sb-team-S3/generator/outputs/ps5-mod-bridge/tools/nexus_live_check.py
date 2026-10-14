#!/usr/bin/env python3
"""Read-only live check of the Nexus calls ModBridge makes, with your own saved sign-in.

    python3 -I tools/nexus_live_check.py --store PATH [--game DOMAIN] [--mod ID]

PATH is the private credential file the companion host uses (`host.py --store`).
The script makes about eight read-only requests: account, latest mods, one
mod's files (with its update links), its requirements (GraphQL), a name
search and the all-mods view (GraphQL). It downloads nothing, writes nothing except a refreshed
OAuth token into the same store (as the host does), and prints no secret.

Several of these calls were written from public docs and tested only against
fakes (docs/INSTALL-FLOW-UX.md 3.8 and 3.9). Each line says what the result
confirms. Exit status 0 means every check passed.
"""
import argparse
from pathlib import Path
import re
import sys

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
import modbridge_nexus as nexus  # noqa: E402
from modbridge_updates import resolve_file_update  # noqa: E402


def search_term(name):
    """A word from a real mod name to search for: the longest plain word of 4+ letters."""
    words = sorted(re.findall(r"[A-Za-z]{4,}", name or ""), key=len, reverse=True)
    return words[0] if words else None


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--store", required=True, type=Path, help="the companion's private credential file")
    parser.add_argument("--game", default="silenthilltownfall", help="Nexus game domain (default: Townfall)")
    parser.add_argument("--mod", type=int, help="mod id for the files/requirements checks (default: newest listed)")
    args = parser.parse_args(argv)
    client = nexus.AccountManager(nexus.CredentialStore(args.store)).client()
    return 0 if run(client, args.game, args.mod) else 1


def run(client, game, mod=None, out=print):
    """Run every check against `client`; True when all six pass."""
    results = []

    def check(name, confirms, action):
        try:
            detail = action()
            results.append(True)
            out(f"PASS {name}: {detail}  [{confirms}]")
            return True
        except nexus.NexusError as error:
            results.append(False)
            out(f"FAIL {name}: Nexus error '{error.code}'  [{confirms}]")
        except Exception as error:  # a decoder or contract problem: report the type only
            results.append(False)
            out(f"FAIL {name}: {type(error).__name__}  [{confirms}]")
        return False

    state = {}

    def account():
        user = client.user()
        return "premium" if user.is_premium else "free"

    def latest():
        mods = client.latest_mods(game)
        if not mods:
            raise RuntimeError("no mods listed")
        state["mod"] = mod or mods[0]["mod_id"]
        state["name"] = next((m["name"] for m in mods if m["mod_id"] == state["mod"]), mods[0]["name"])
        return f"{len(mods)} listed; using mod {state['mod']}"

    def files():
        data = client.files_and_updates(game, state["mod"])
        if not data["files"]:
            return "no files listed"
        verdict = resolve_file_update(data["files"][0]["file_id"], data["files"], data["file_updates"])
        return f"{len(data['files'])} files, {len(data['file_updates'])} update links; first file: {verdict['status']}"

    def requirements():
        record = client.requirements(game, state["mod"])
        return f"{len(record['nexus_requirements'])} Nexus and {len(record['dlc_requirements'])} DLC requirements listed"

    def search():
        term = search_term(state.get("name"))
        if term is None:
            raise RuntimeError("no searchable word in the mod name")
        result = client.search_mods(game, term, 1)
        if result["total"] == 0:
            raise RuntimeError(f"Nexus matched nothing for '{term}', a word from a listed mod")
        if not result["items"]:
            raise RuntimeError(f"{result['total']} matched for '{term}' but every item was filtered out")
        return f"'{term}': {result['total']} matched, {len(result['items'])} listed on page 1 of {result['pages']}"

    def browse():
        result = client.browse_mods(game, "downloads", 1)
        if result["total"] == 0 or not result["items"]:
            raise RuntimeError("Nexus listed no mods for the game in the all-mods view")
        return f"{result['total']} mods, {len(result['items'])} on page 1 of {result['pages']}"

    check("account", "credentials work", account)
    if check("latest mods", "REST catalog", latest):
        check("files + update links", "file_updates parser accepts live data", files)
        check("requirements", "GraphQL requirements query and decoder (3.8)", requirements)
        check("search", "search query, published filter, relevance sort and counts (3.9)", search)
        check("browse all", "no-name query, downloads sort and paging (3.10)", browse)
    passed = sum(results)
    out(f"\n{passed} of {len(results)} checks passed.")
    return passed == len(results) == 6


if __name__ == "__main__":
    sys.exit(main())
