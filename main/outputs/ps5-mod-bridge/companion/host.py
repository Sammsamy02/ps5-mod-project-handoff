#!/usr/bin/env python3
"""Run the local preparation UI. Provider traffic starts with user actions."""
import argparse
import json
from pathlib import Path
import sys

import modbridge_nexus as nexus
from modbridge_host import BridgeApplication, create_server


def main():
    parser = argparse.ArgumentParser(description="ModBridge local preparation interface")
    parser.add_argument("--store", required=True, type=Path)
    parser.add_argument("--quarantine", required=True, type=Path)
    parser.add_argument("--port", type=int, default=18123)
    parser.add_argument("--client-id")
    parser.add_argument("--redirect-uri")
    parser.add_argument("--scope", default="")
    args = parser.parse_args()
    app = server = None
    try:
        if not 0 <= args.port <= 65535 or bool(args.client_id) != bool(args.redirect_uri):
            raise ValueError()
        config = nexus.OAuthConfig(args.client_id, args.redirect_uri, args.scope) if args.client_id else None
        app = BridgeApplication(args.store, args.quarantine, config)
        server = create_server(app, port=args.port)
        print(json.dumps({"url": app.origin, "mode": "desktop_preparation", "console_execution": "unverified"}), flush=True)
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    except Exception:
        print(json.dumps({"error": "Local interface could not start; check its private paths and configuration."}), file=sys.stderr)
        return 2
    finally:
        if server:
            server.server_close()
        if app:
            app.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
