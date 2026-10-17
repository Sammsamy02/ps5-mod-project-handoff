#!/usr/bin/env python3
"""run_ui_flow_tests.py — Level-1 UI-flow host tests (worker-app-ui-flows).

Drives the REAL rebuilt app server (download-runtime build with fixture OAuth
config, modmgr env) over real TCP with a real session: cookie jar + CSRF token
+ Origin, exactly the admission the browser UI satisfies. Verifies the
install / enable / disable / uninstall flows the Installed tab now exposes,
plus the negative gates. Console staging is counted but never dispatched.
No console, no provider contact.
"""
import http.client
import json
import os
import pathlib
import re
import sys

PORT = 62283
ORIGIN = "http://127.0.0.1:62283"
HERE = pathlib.Path(__file__).resolve().parent
WORKER = HERE.parent
FIX = pathlib.Path("/tmp/mb-ui-flows-fix")
TITLE = "PPSA33286"
UI_ID = "ui-flow-mod"

PASS = FAIL = 0
RECEIPT = []


def check(name, ok, detail=""):
    global PASS, FAIL
    (PASS if ok else FAIL).__class__  # noqa - clarity only
    if ok:
        PASS += 1
        print("  ok   %s" % name)
    else:
        FAIL += 1
        print("  FAIL %s %s" % (name, detail))
    RECEIPT.append({"check": name, "ok": bool(ok), "detail": str(detail)[:400]})


class Client:
    def __init__(self):
        self.cookie = None
        self.csrf = None

    def req(self, method, path, body=None, csrf=False, origin=False, send_cookie=True):
        conn = http.client.HTTPConnection("127.0.0.1", PORT, timeout=30)
        headers = {"Host": "127.0.0.1:%d" % PORT}
        if send_cookie and self.cookie:
            headers["Cookie"] = "mb_session=" + self.cookie
        if origin:
            headers["Origin"] = ORIGIN
        if csrf:
            headers["X-CSRF-Token"] = self.csrf or ""
        payload = body.encode() if isinstance(body, str) else body
        if payload is not None:
            headers["Content-Type"] = "application/json"
        conn.request(method, path, body=payload, headers=headers)
        resp = conn.getresponse()
        data = resp.read()
        setc = resp.getheader("Set-Cookie")
        if setc:
            m = re.search(r"mb_session=([A-Za-z0-9_-]{43})", setc)
            if m:
                self.cookie = m.group(1)
        conn.close()
        try:
            doc = json.loads(data.decode())
        except Exception:
            doc = None
        return resp.status, doc, data


def queue_rows():
    manifest = FIX / "state" / "modmgr" / "console-queue" / "manifest.jsonl"
    if not manifest.exists():
        return 0
    return sum(1 for line in manifest.read_text().splitlines() if line.strip())


def main():
    c = Client()
    print("== UI flow tests (Level 1, real server + real session) ==")

    # 1. capabilities + session issue
    status, doc, _ = c.req("GET", "/api/capabilities")
    check("capabilities 200 with session issue", status == 200 and doc is not None, status)
    check("installed_records capability true", doc and doc.get("capabilities", {}).get("installed_records") is True, doc)
    check("mod_service_records capability false (records route, not service)", doc and doc.get("capabilities", {}).get("mod_service_records") is False, doc)
    check("csrf_token issued", isinstance(doc and doc.get("csrf_token"), str) and len(doc["csrf_token"]) == 43, doc and doc.get("csrf_token"))
    c.csrf = doc["csrf_token"] if doc else None
    check("Set-Cookie mb_session issued", c.cookie is not None and len(c.cookie) == 43, c.cookie)

    # 2. served UI bytes are MY fixed assets
    status, _, raw = c.req("GET", "/app.js")
    check("GET /app.js 200", status == 200, status)
    check("served app.js embeds toggle control", b"data-mod-toggle" in raw, len(raw))
    check("served app.js embeds confirm uninstall", b"Confirm uninstall" in raw)
    check("served app.js embeds install form", b"install-form" in raw)
    status, _, raw = c.req("GET", "/app")
    check("GET /app serves index.html", status == 200 and b"installed-content" in raw, status)
    status, _, raw = c.req("GET", "/style.css")
    check("GET /style.css 200 with install-form rules", status == 200 and b"#install-form" in raw, status)

    # 3. installed list renders the real seeded row (UI contract)
    status, doc, _ = c.req("GET", "/api/installed")
    check("GET /api/installed 200", status == 200, status)
    items = (doc or {}).get("items", [])
    check("installed list has seeded zb-lua row", any(i.get("id") == "zb-lua" for i in items), [i.get("id") for i in items])
    row = next((i for i in items if i.get("id") == "zb-lua"), {})
    check("row carries transaction_confirmed true", row.get("transaction_confirmed") is True, row)
    check("row carries 64-hex sha256 (UI filter passes)", bool(re.fullmatch(r"[0-9a-f]{64}", row.get("sha256") or "")), row.get("sha256"))
    check("row carries enabled/files/bytes projection", row.get("enabled") is True and isinstance(row.get("files"), int) and isinstance(row.get("bytes"), int), row)

    # 4. admission negatives (what the browser never sends)
    anon = Client()
    status, doc, _ = anon.req("GET", "/api/installed", send_cookie=False)
    check("installed without session refused (%s)" % status, status in (401, 403, 404), status)
    status, doc, _ = c.req("POST", "/api/install", body=json.dumps({"title": TITLE, "id": UI_ID, "path": str(FIX / "intake" / "ui-flow-mod.zip")}), origin=True, csrf=False)
    check("install without CSRF refused 403", status == 403, (status, doc))
    status, doc, _ = c.req("POST", "/api/install", body=json.dumps({"title": TITLE, "id": UI_ID, "path": str(FIX / "intake" / "ui-flow-mod.zip")}), csrf=True, origin=False)
    check("install without Origin refused 403", status == 403, (status, doc))

    # 5. install flow with containment negative
    status, doc, _ = c.req("POST", "/api/install", body=json.dumps({"title": TITLE, "id": UI_ID, "path": "/etc/hosts"}), csrf=True, origin=True)
    check("install outside intake refused 400", status == 400, (status, doc))
    q0 = queue_rows()
    status, doc, _ = c.req("POST", "/api/install", body=json.dumps({"title": TITLE, "id": UI_ID, "path": str(FIX / "intake" / "ui-flow-mod.zip")}), csrf=True, origin=True)
    check("install from intake 200 ok envelope", status == 200 and doc and doc.get("ok") is True and doc.get("op") == "install", (status, doc))
    staged = (doc or {}).get("queue") or []
    check("install staged console commands (register/prepare/install)", len(staged) >= 3 and all(r.get("console_executed") is False for r in staged), [r.get("name") for r in staged])
    check("queue manifest grew by staged rows", queue_rows() == q0 + len(staged), (q0, queue_rows()))
    status, doc, _ = c.req("GET", "/api/installed")
    items = (doc or {}).get("items", [])
    row = next((i for i in items if i.get("id") == UI_ID), {})
    check("installed list refresh shows new row", row.get("transaction_confirmed") is True and bool(re.fullmatch(r"[0-9a-f]{64}", row.get("sha256") or "")), row)

    # 6. disable -> enable round trip (no body: routes sit outside the JSON gate)
    q1 = queue_rows()
    status, doc, _ = c.req("POST", "/api/mods/zb-lua/disable?title=" + TITLE, csrf=True, origin=True)
    ver = (doc or {}).get("verification", {}).get("mods", [])
    check("disable 200 with verification projection", status == 200 and doc and doc.get("ok") is True and any(m.get("id") == "zb-lua" and m.get("enabled") is False for m in ver), (status, ver))
    status, doc, _ = c.req("GET", "/api/installed")
    row = next((i for i in (doc or {}).get("items", []) if i.get("id") == "zb-lua"), {})
    check("list shows zb-lua disabled after disable", row.get("enabled") is False, row)
    status, doc, _ = c.req("POST", "/api/mods/zb-lua/enable?title=" + TITLE, csrf=True, origin=True)
    ver = (doc or {}).get("verification", {}).get("mods", [])
    check("enable 200 restores enabled projection", status == 200 and doc and doc.get("ok") is True and any(m.get("id") == "zb-lua" and m.get("enabled") is True for m in ver), (status, ver))
    status, doc, _ = c.req("GET", "/api/installed")
    row = next((i for i in (doc or {}).get("items", []) if i.get("id") == "zb-lua"), {})
    check("list shows zb-lua enabled again (round trip)", row.get("enabled") is True, row)
    check("toggle staged recompose commands", queue_rows() >= q1 + 2, (q1, queue_rows()))

    # 7. uninstall with confirm semantics (DELETE)
    q2 = queue_rows()
    status, doc, _ = c.req("DELETE", "/api/mods/%s?title=%s" % (UI_ID, TITLE), csrf=True, origin=True)
    check("uninstall 200", status == 200 and doc and doc.get("ok") is True, (status, doc))
    status, doc, _ = c.req("GET", "/api/installed")
    raw_items = (doc or {}).get("items", [])
    # The tab renders exactly what app.js loadInstalled filters:
    # transaction_confirmed === true && 64-hex sha256.
    ui_items = [i for i in raw_items
                if i.get("transaction_confirmed") is True
                and re.fullmatch(r"[0-9a-fA-F]{64}", i.get("sha256") or "")]
    check("installed list refresh drops uninstalled row (UI filter)",
          all(i.get("id") != UI_ID for i in ui_items) and
          any(i.get("id") == UI_ID and i.get("transaction_confirmed") is False
              for i in raw_items), [i.get("id") for i in raw_items])
    status, doc, _ = c.req("DELETE", "/api/mods/%s?title=%s" % (UI_ID, TITLE), csrf=True, origin=True)
    check("second uninstall idempotent 200", status == 200, (status, doc))
    check("uninstall staged remove command", queue_rows() >= q2 + 1, (q2, queue_rows()))

    # 8. final integrity: zb-lua still installed and enabled
    status, doc, _ = c.req("GET", "/api/installed")
    items = (doc or {}).get("items", [])
    check("zb-lua still installed+enabled at end", any(i.get("id") == "zb-lua" and i.get("enabled") is True and i.get("transaction_confirmed") is True for i in items), [i.get("id") for i in items])

    print("== PASS=%d FAIL=%d ==" % (PASS, FAIL))
    out = WORKER / "receipts" / "ui-flow-tests.json"
    out.write_text(json.dumps({
        "worker": "worker-app-ui-flows", "date": "2026-10-04",
        "server": "download-runtime-ui/desktop_fixture-download/modbridge-native-host-desktop-download",
        "build_id": "59755cf4ecc2e92226268b00f2f7a85520111f4e015693270fcc1cacb3959434",
        "artifact_sha256": "e1204720d973a36a0de02ccdfd671fdcf7b944b30553f060c18f72918ccea239",
        "mode": "real TCP server, real session cookie + CSRF + Origin admission (fixture OAuth config), real fork/exec modmgr driver + pinned installer binary, fixture tree /tmp/mb-ui-flows-fix",
        "pass": PASS, "fail": FAIL, "checks": RECEIPT,
        "console_executed": False, "provider_contact": False,
    }, indent=1) + "\n")
    return 0 if FAIL == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
