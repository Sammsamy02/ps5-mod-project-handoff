"""Bounded trusted title snapshots; platform metadata providers are opt-in."""
import json
import re
import secrets
import threading


MAX_RECORDS = 128
MAX_ISSUES = 32
MAX_SNAPSHOT_BYTES = 8192
MAX_TITLE_ID_BYTES = 64
MAX_DISPLAY_NAME_BYTES = 256
MAX_VERSION_BYTES = 128
MAX_SOURCE_BYTES = 64
MAX_ISSUE_CODE_BYTES = 64
MAX_CAPTURE_BYTES = 512 * 1024
MAX_ITEMS_PER_PAGE = 8
MAX_PAGE_BYTES = 8191
MAX_PAGES = 128
MAX_TOKEN_ATTEMPTS = 8

_TOKEN = re.compile(r"^[a-f0-9]{32}$")
_TITLE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
_SOURCE_SCOPE = re.compile(r"^[a-z0-9][a-z0-9_.-]{0,47}$")
_SOURCE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.:-]{0,63}$")
_REASON = re.compile(r"^[a-z][a-z0-9_]{0,63}$")
_SHA256 = re.compile(r"^[a-f0-9]{64}$")

_CAPTURE_FIELDS = {"status", "complete", "source_scope", "reason", "issues", "items"}
_ITEM_FIELDS = {"title_id", "display_name", "build_version", "build_fingerprint"}
_METADATA_FIELDS = {"value", "status", "source"}
_FINGERPRINT_FIELDS = {"algorithm", "value", "confidence", "source", "provenance"}
_PROVENANCE_FIELDS = {"method", "byte_length", "complete", "artifact_kind"}
_REASON_CODES = frozenset({"platform_provider_unavailable", "provider_error",
                           "invalid_provider_capture", "inventory_resource_limit",
                           "source_changed", "provider_observation_incomplete"})
_ISSUE_CODES = _REASON_CODES


class TitleInventoryError(Exception):
    """Safe typed failure for invalid or stale opaque target references."""

    def __init__(self, code):
        self.code = code
        super().__init__(code)


def validate_reference(reference):
    """Validate and copy an exact opaque target reference without resolving it."""
    if type(reference) is not dict or set(reference) != {"snapshot_id", "title_key"}:
        raise TitleInventoryError("invalid_input")
    snapshot_id = reference["snapshot_id"]
    title_key = reference["title_key"]
    if (type(snapshot_id) is not str or not _TOKEN.fullmatch(snapshot_id) or
            type(title_key) is not str or not _TOKEN.fullmatch(title_key)):
        raise TitleInventoryError("invalid_input")
    return {"snapshot_id": snapshot_id, "title_key": title_key}


def _utf8_text(value, limit, *, pattern=None):
    if type(value) is not str or not value or len(value) > limit:
        raise ValueError("text")
    if value != value.strip():
        raise ValueError("text")
    try:
        encoded = value.encode("utf-8", "strict")
    except UnicodeError:
        raise ValueError("text") from None
    if len(encoded) > limit or any(ord(char) < 32 or ord(char) == 127 for char in value):
        raise ValueError("text")
    if pattern is not None and not pattern.fullmatch(value):
        raise ValueError("text")
    return value


def _safe_reason(value, *, optional=False):
    if value is None and optional:
        return None
    return _utf8_text(value, MAX_ISSUE_CODE_BYTES, pattern=_REASON)


def _metadata(value, *, required=False, limit=MAX_DISPLAY_NAME_BYTES, pattern=None):
    if type(value) is not dict or len(value) != len(_METADATA_FIELDS) or set(value) != _METADATA_FIELDS:
        raise ValueError("metadata")
    state = value["status"]
    source = value["source"]
    text = value["value"]
    if type(state) is str and state == "observed":
        _utf8_text(text, limit, pattern=pattern)
        _utf8_text(source, MAX_SOURCE_BYTES, pattern=_SOURCE)
        return {"value": text, "status": "observed", "source": source}
    if type(state) is str and state in ("unavailable", "error") and text is None and source is None and not required:
        return {"value": None, "status": state, "source": None}
    raise ValueError("metadata")


def _fingerprint(value):
    if value is None:
        return None
    if (type(value) is not dict or len(value) != len(_FINGERPRINT_FIELDS) or
            set(value) != _FINGERPRINT_FIELDS):
        raise ValueError("fingerprint")
    if (type(value["algorithm"]) is not str or value["algorithm"] != "sha256" or
            type(value["value"]) is not str or not _SHA256.fullmatch(value["value"]) or
            type(value["confidence"]) is not str or value["confidence"] != "exact_bytes"):
        raise ValueError("fingerprint")
    source = _utf8_text(value["source"], MAX_SOURCE_BYTES, pattern=_SOURCE)
    provenance = value["provenance"]
    if (type(provenance) is not dict or len(provenance) != len(_PROVENANCE_FIELDS) or
            set(provenance) != _PROVENANCE_FIELDS):
        raise ValueError("provenance")
    length = provenance["byte_length"]
    artifact_kind = provenance["artifact_kind"]
    if (type(provenance["method"]) is not str or provenance["method"] != "sha256_exact_bytes" or
            type(provenance["complete"]) is not bool or not provenance["complete"] or
            type(length) is not int or length <= 0 or length > 0x7fffffffffffffff or
            type(artifact_kind) is not str or artifact_kind not in ("executable", "content")):
        raise ValueError("provenance")
    return {
        "algorithm": "sha256",
        "value": value["value"],
        "confidence": "exact_bytes",
        "source": source,
        "provenance": {
            "method": "sha256_exact_bytes",
            "byte_length": length,
            "complete": True,
            "artifact_kind": artifact_kind,
        },
    }


def _error_snapshot(reason):
    return {
        "schema_version": 1,
        "status": "error",
        "complete": False,
        "snapshot_id": None,
        "source_scope": "provider",
        "items": [],
        "reason": reason,
        "issues": [],
    }


class _ResourceLimit(Exception):
    pass


class _TokenFailure(Exception):
    pass


def _canonical_bytes(value):
    return json.dumps(value, ensure_ascii=True, separators=(",", ":"),
                      sort_keys=True, allow_nan=False).encode("ascii")


def _new_token(excluded):
    for _ in range(MAX_TOKEN_ATTEMPTS):
        try:
            value = secrets.token_hex(16)
        except Exception:
            continue
        if type(value) is str and _TOKEN.fullmatch(value) and value not in excluded:
            return value
    raise _TokenFailure("opaque token generation failed")


def _v2_issue(issue):
    code = issue["code"]
    return {"code": code if code in _ISSUE_CODES else "provider_observation_incomplete"}


def _v2_item(item):
    fingerprint = item["build_fingerprint"]
    if fingerprint is not None:
        provenance = dict(fingerprint["provenance"])
        provenance["byte_length"] = str(provenance["byte_length"])
        fingerprint = {**fingerprint, "provenance": provenance}
    return {"title_key": item["title_key"], "title_id": dict(item["title_id"]),
            "display_name": dict(item["display_name"]), "build_version": dict(item["build_version"]),
            "build_fingerprint": fingerprint}


def _private_capture(snapshot, items):
    return {
        "schema_version": 2,
        "kind": "title_inventory_capture",
        "snapshot": {
            "status": snapshot["status"], "complete": snapshot["complete"],
            "snapshot_id": snapshot["snapshot_id"], "source_scope": snapshot["source_scope"],
            "reason": snapshot["reason"], "issues": [_v2_issue(issue) for issue in snapshot["issues"]],
        },
        "items": items,
    }


class TitleInventory:
    """Cache one immutable provider capture and resolve only opaque current refs.

    A provider is a trusted injected callable returning metadata only. It never
    receives browser references and must not derive target facts from Nexus IDs.
    """

    def __init__(self, provider=None):
        if provider is not None and not callable(provider):
            raise TitleInventoryError("invalid_input")
        self.provider = provider
        self._current = None
        self._last_snapshot_id = None
        self._lock = threading.RLock()
        self._capture_in_flight = None
        self._capture_cache = None
        self._capture_bytes = None

    def snapshot(self, refresh=False):
        if type(refresh) is not bool:
            raise TitleInventoryError("invalid_input")
        cache = self._ensure_capture(refresh)
        with self._lock:
            if self._capture_in_flight is not None:
                raise TitleInventoryError("inventory_capture_in_progress")
            if cache is not self._capture_cache:
                raise TitleInventoryError("inventory_page_stale")
            return json.loads(cache["legacy_bytes"])

    def resolve(self, reference):
        normalized = validate_reference(reference)
        self._ensure_capture(False)
        with self._lock:
            if self._capture_in_flight is not None:
                raise TitleInventoryError("inventory_capture_in_progress")
            cache = self._capture_cache
            if cache is None:
                raise TitleInventoryError("stale_target_ref")
            capture = json.loads(cache["capture_bytes"])
            snapshot = capture["snapshot"]
            if (snapshot["status"] != "observed" or snapshot["complete"] is not True or
                    snapshot["snapshot_id"] != normalized["snapshot_id"]):
                raise TitleInventoryError("stale_target_ref")
            for item in capture["items"]:
                if item["title_key"] == normalized["title_key"]:
                    fingerprint = item["build_fingerprint"]
                    if fingerprint is not None:
                        fingerprint["provenance"]["byte_length"] = int(
                            fingerprint["provenance"]["byte_length"])
                    fingerprints = ({"build_sha256": fingerprint["value"]}
                                    if fingerprint is not None else {})
                    return {
                        "target_ref": normalized,
                        "title_id": item["title_id"],
                        "display_name": item["display_name"],
                        "build_version": item["build_version"],
                        "build_fingerprint": fingerprint,
                        "fingerprints": fingerprints,
                    }
            raise TitleInventoryError("stale_target_ref")

    def page(self, cursor=None, refresh=False):
        if type(refresh) is not bool:
            raise TitleInventoryError("invalid_input")
        if (cursor is not None and (type(cursor) is not str or not _TOKEN.fullmatch(cursor))) or (
                refresh and cursor is not None):
            raise TitleInventoryError("invalid_input")
        if cursor is not None:
            with self._lock:
                if self._capture_in_flight is not None:
                    raise TitleInventoryError("inventory_capture_in_progress")
                cache = self._capture_cache
                if cache is None:
                    raise TitleInventoryError("inventory_page_stale")
                index = cache["cursor_indices"].get(cursor)
                if index is None:
                    raise TitleInventoryError("inventory_page_stale")
                if index >= len(cache["pages"]):
                    raise TitleInventoryError("inventory_page_stale")
                return memoryview(cache["pages"][index]).tobytes()
        cache = self._ensure_capture(refresh)
        with self._lock:
            if self._capture_in_flight is not None:
                raise TitleInventoryError("inventory_capture_in_progress")
            if cache is not self._capture_cache:
                raise TitleInventoryError("inventory_page_stale")
            if not cache["pages"]:
                raise TitleInventoryError("inventory_page_stale")
            return memoryview(cache["pages"][0]).tobytes()

    def current(self, snapshot_id):
        if type(snapshot_id) is not str or not _TOKEN.fullmatch(snapshot_id):
            raise TitleInventoryError("invalid_input")
        with self._lock:
            if self._capture_in_flight is not None:
                raise TitleInventoryError("inventory_capture_in_progress")
            cache = self._capture_cache
            if (cache is None or cache["snapshot_id"] != snapshot_id or
                    cache["current_bytes"] is None):
                raise TitleInventoryError("inventory_page_stale")
            return memoryview(cache["current_bytes"]).tobytes()

    def _ensure_capture(self, refresh):
        with self._lock:
            if self._capture_in_flight is not None:
                code = "inventory_refresh_busy" if refresh else "inventory_capture_in_progress"
                raise TitleInventoryError(code)
            if not refresh and self._capture_cache is not None:
                return self._capture_cache
            if refresh:
                self._capture_cache = None
                self._capture_bytes = None
                self._current = None
            ticket = object()
            self._capture_in_flight = ticket
            provider = self.provider
            previous_snapshot_id = self._last_snapshot_id
        try:
            cache = self._capture_provider(provider, previous_snapshot_id)
        except BaseException:
            with self._lock:
                if self._capture_in_flight is ticket:
                    self._capture_in_flight = None
            raise
        with self._lock:
            if self._capture_in_flight is not ticket:
                raise TitleInventoryError("inventory_capture_in_progress")
            self._capture_cache = cache
            self._capture_bytes = cache["capture_bytes"]
            self._current = cache["legacy_bytes"].decode("ascii")
            self._capture_in_flight = None
            if cache["snapshot_id"] is not None:
                self._last_snapshot_id = cache["snapshot_id"]
            return cache

    def _capture_provider(self, provider, previous_snapshot_id):
        if provider is None:
            raw_capture = {
                "status": "unavailable",
                "complete": False,
                "source_scope": "none",
                "items": [],
                "reason": "platform_provider_unavailable",
                "issues": [],
            }
        else:
            try:
                raw_capture = provider()
            except Exception:
                return self._terminal_cache("provider_error")
        try:
            materialized = self._materialize(raw_capture, previous_snapshot_id)
            return self._assemble_cache(materialized)
        except _ResourceLimit:
            return self._terminal_cache("inventory_resource_limit")
        except _TokenFailure:
            return self._terminal_cache("provider_error")
        except Exception:
            return self._terminal_cache("invalid_provider_capture")

    def _materialize(self, capture, previous_snapshot_id):
        if (type(capture) is not dict or len(capture) != len(_CAPTURE_FIELDS) or
                set(capture) != _CAPTURE_FIELDS):
            raise ValueError("capture")
        status = capture["status"]
        complete = capture["complete"]
        if (type(status) is not str or status not in ("observed", "partial", "unavailable", "error") or
                type(complete) is not bool):
            raise ValueError("status")
        scope = _utf8_text(capture["source_scope"], 48, pattern=_SOURCE_SCOPE)
        reason = _safe_reason(capture["reason"], optional=status in ("observed", "partial"))
        issues = capture["issues"]
        records = capture["items"]
        if type(issues) is not list or len(issues) > MAX_ISSUES or type(records) is not list or len(records) > MAX_RECORDS:
            raise ValueError("limits")
        normalized_issues = []
        for issue in issues:
            if type(issue) is not dict or len(issue) != 1 or set(issue) != {"code"}:
                raise ValueError("issue")
            normalized_issues.append({"code": _safe_reason(issue["code"])})
        if status == "observed":
            if not complete or reason is not None:
                raise ValueError("observed")
        elif status == "partial":
            if complete:
                raise ValueError("partial")
        else:
            if complete or records or reason is None:
                raise ValueError("unavailable")
        used_tokens = set()
        if previous_snapshot_id is not None:
            used_tokens.add(previous_snapshot_id)
        snapshot_id = None
        if status in ("observed", "partial"):
            snapshot_id = _new_token(used_tokens)
            used_tokens.add(snapshot_id)
        snapshot = {
            "schema_version": 1,
            "status": status,
            "complete": complete,
            "snapshot_id": snapshot_id,
            "source_scope": scope,
            "items": [],
            "reason": reason,
            "issues": normalized_issues,
        }
        private_base = _private_capture(snapshot, [])
        base_bytes = _canonical_bytes(private_base)
        if len(base_bytes) > MAX_CAPTURE_BYTES:
            raise _ResourceLimit("capture envelope exceeds finite bounds")
        retained_items_size = len(base_bytes)
        items, v2_items = [], []
        for raw in records:
            if (type(raw) is not dict or len(raw) != len(_ITEM_FIELDS) or
                    set(raw) != _ITEM_FIELDS):
                raise ValueError("item")
            title_id = _metadata(raw["title_id"], required=True, limit=MAX_TITLE_ID_BYTES,
                                 pattern=_TITLE_ID)
            display_name = _metadata(raw["display_name"], limit=MAX_DISPLAY_NAME_BYTES)
            build_version = _metadata(raw["build_version"], limit=MAX_VERSION_BYTES)
            fingerprint = _fingerprint(raw["build_fingerprint"])
            title_key = _new_token(used_tokens)
            used_tokens.add(title_key)
            item = {
                "title_key": title_key,
                "title_id": title_id,
                "display_name": display_name,
                "build_version": build_version,
                "build_fingerprint": fingerprint,
            }
            v2_item = _v2_item(item)
            encoded_item = _canonical_bytes(v2_item)
            candidate_size = retained_items_size + len(encoded_item) + (1 if items else 0)
            if candidate_size > MAX_CAPTURE_BYTES:
                raise _ResourceLimit("capture records exceed finite bounds")
            retained_items_size = candidate_size
            items.append(item)
            v2_items.append(v2_item)
        snapshot["items"] = items
        capture_object = _private_capture(snapshot, v2_items)
        capture_bytes = _canonical_bytes(capture_object)
        if len(capture_bytes) > MAX_CAPTURE_BYTES:
            raise _ResourceLimit("capture exceeds finite bounds")
        return {
            "snapshot": snapshot,
            "capture_object": capture_object,
            "capture_bytes": capture_bytes,
        }

    def _assemble_cache(self, materialized):
        snapshot = materialized["snapshot"]
        private = materialized["capture_object"]
        pages, cursor_indices, current_bytes = self._build_pages(private)
        legacy_bytes = _canonical_bytes(snapshot)
        if len(legacy_bytes) > MAX_SNAPSHOT_BYTES:
            # Preserve the established v1 size-error shape without discarding the full cache.
            legacy_bytes = _canonical_bytes(_error_snapshot("invalid_provider_capture"))
        return {
            "capture_bytes": materialized["capture_bytes"],
            "legacy_bytes": legacy_bytes,
            "snapshot_id": snapshot["snapshot_id"],
            "pages": tuple(pages),
            "cursor_indices": cursor_indices,
            "current_bytes": current_bytes,
        }

    def _terminal_cache(self, reason):
        snapshot = _error_snapshot(reason)
        capture_object = _private_capture(snapshot, [])
        capture_bytes = _canonical_bytes(capture_object)
        pages, cursor_indices, current_bytes = self._build_pages(capture_object)
        return {
            "capture_bytes": capture_bytes,
            "legacy_bytes": _canonical_bytes(snapshot),
            "snapshot_id": None,
            "pages": tuple(pages),
            "cursor_indices": cursor_indices,
            "current_bytes": current_bytes,
        }

    @staticmethod
    def _public_snapshot(private_snapshot, *, first_page):
        status = private_snapshot["status"]
        reason = private_snapshot["reason"]
        if status != "observed" and reason not in _REASON_CODES:
            reason = "provider_observation_incomplete"
        return {
            "status": status,
            "complete": private_snapshot["complete"],
            "snapshot_id": private_snapshot["snapshot_id"] if status == "observed" else None,
            "source_scope": private_snapshot["source_scope"],
            "reason": reason,
            "issues": private_snapshot["issues"] if first_page else [],
        }

    @staticmethod
    def _render_page(private_snapshot, items, index, page_count, item_offset,
                     total_count, next_cursor):
        return _canonical_bytes({
            "schema_version": 2,
            "kind": "title_inventory_page",
            "snapshot": TitleInventory._public_snapshot(private_snapshot, first_page=index == 0),
            "page": {"index": index, "page_count": page_count,
                     "item_offset": item_offset, "total_count": total_count,
                     "next_cursor": next_cursor},
            "items": items,
        })

    def _build_pages(self, capture_object):
        private_snapshot = capture_object["snapshot"]
        status = private_snapshot["status"]
        records = capture_object["items"]
        if status != "observed" or private_snapshot["complete"] is not True:
            response = {
                "schema_version": 2,
                "kind": "title_inventory_page",
                "snapshot": self._public_snapshot(private_snapshot, first_page=True),
                "page": None,
                "items": [],
            }
            raw = _canonical_bytes(response)
            if len(raw) > MAX_PAGE_BYTES:
                raise _ResourceLimit("terminal page exceeds finite bounds")
            return [raw], {}, None

        snapshot_id = private_snapshot["snapshot_id"]
        if type(snapshot_id) is not str or not _TOKEN.fullmatch(snapshot_id):
            raise ValueError("observed snapshot ID")
        total = len(records)
        if total > MAX_RECORDS:
            raise _ResourceLimit("page item count exceeds finite bounds")
        placeholder = "f" * 32

        def partition(assumed_page_count):
            bounds = []
            start = 0
            if total == 0:
                candidate = self._render_page(private_snapshot, [], 0, assumed_page_count,
                                              0, 0, None)
                if len(candidate) > MAX_PAGE_BYTES:
                    raise _ResourceLimit("empty page exceeds finite bounds")
                return [(0, 0)]
            while start < total:
                index = len(bounds)
                end = start
                while end < total and end - start < MAX_ITEMS_PER_PAGE:
                    candidate_end = end + 1
                    cursor = None if candidate_end == total else placeholder
                    candidate = self._render_page(
                        private_snapshot, records[start:candidate_end], index,
                        assumed_page_count, start, total, cursor)
                    if len(candidate) > MAX_PAGE_BYTES:
                        break
                    end = candidate_end
                if end == start:
                    raise _ResourceLimit("one item cannot fit a bounded page")
                bounds.append((start, end))
                if len(bounds) > MAX_PAGES:
                    raise _ResourceLimit("page count exceeds finite bounds")
                start = end
            return bounds

        assumed_count = 1
        bounds = None
        for _ in range(5):
            candidate_bounds = partition(assumed_count)
            actual_count = len(candidate_bounds)
            if actual_count > MAX_PAGES:
                raise _ResourceLimit("page count exceeds finite bounds")
            bounds = candidate_bounds
            if actual_count == assumed_count:
                break
            assumed_count = actual_count
        else:
            raise _ResourceLimit("page partition did not stabilize")

        used_tokens = {snapshot_id}
        used_tokens.update(item["title_key"] for item in records)
        cursor_by_index = {0: None}
        cursor_indices = {}
        for index in range(1, len(bounds)):
            token = _new_token(used_tokens)
            used_tokens.add(token)
            cursor_by_index[index] = token
            cursor_indices[token] = index
        page_count = len(bounds)
        pages = []
        for index, (start, end) in enumerate(bounds):
            next_cursor = cursor_by_index.get(index + 1)
            raw = self._render_page(private_snapshot, records[start:end], index,
                                    page_count, start, total, next_cursor)
            if len(raw) > MAX_PAGE_BYTES:
                raise _ResourceLimit("final page exceeds finite bounds")
            pages.append(raw)
        current_bytes = _canonical_bytes({
            "schema_version": 2, "kind": "title_inventory_current",
            "snapshot_id": snapshot_id, "current": True,
        })
        if len(current_bytes) > MAX_PAGE_BYTES:
            raise _ResourceLimit("current confirmation exceeds finite bounds")
        return pages, cursor_indices, current_bytes

    @staticmethod
    def _serialize(snapshot):
        serialized = _canonical_bytes(snapshot)
        if len(serialized) > MAX_SNAPSHOT_BYTES:
            serialized = _canonical_bytes(_error_snapshot("invalid_provider_capture"))
        return serialized.decode("ascii")
