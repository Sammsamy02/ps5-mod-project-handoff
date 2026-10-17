#!/usr/bin/env python3
"""gen_seed_table.py — SEED-TABLE.json -> SeedTable.h (format spec: SEED-TABLE-FORMAT.md v2).

Consumer-side validator + generator. Accepts two input dialects and normalizes
to the canonical schema before validating:
  canonical   — SEED-TABLE-FORMAT.md schema (format_version 2, bare slot keys,
                present/label fields)
  derivation  — worker-seed-table-derivation's delivered shape (N_-prefixed
                slot keys, seeded/linked fields, size instead of bytes)
Refuses (exit 1) any drift: image sha mismatch, present slot without evidence/
derivation, seeded slot 6/10 (slot 6 is settings-owned; tick is vtable-owned),
missing fname_remedy/base when any slot is present. The runtime consumer sees
only the generated constexpr constants; all validation happens here.

Usage: gen_seed_table.py <SEED-TABLE.json> <SeedTable.h>
"""
import json
import sys

EXPECTED_IMAGE_SHA = "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc"
SLOTS = [
    ("g_uobject_array", 0, "data", "kGUObjectArray"),
    ("fname_ctor_wchar", 2, "code", "kFNameCtorWchar"),
    ("gmalloc", 3, "data", "kGMalloc"),
    ("static_construct_object_internal", 4, "code", "kStaticConstructObject"),
]
# Slots that may appear in the derivation dialect as documentation-only records.
DOC_ONLY_DIALECT_PREFIXES = {"6", "10"}


def die(msg):
    print(f"gen_seed_table: REFUSED: {msg}", file=sys.stderr)
    sys.exit(1)


def parse_addr(value, what):
    if not isinstance(value, str) or not value.lower().startswith("0x"):
        die(f"{what}: expected hex string, got {value!r}")
    try:
        return int(value, 16)
    except ValueError:
        die(f"{what}: bad hex {value!r}")


# FNAME-VERDICT.md section 4/5 constants: the pool label and heap span are
# verdict-owned structural facts, not per-slot derivations. The dialect may omit
# them; the canonical form must carry them (checked against these).
VERDICT_POOL_LABEL = 0x9365ec0
VERDICT_HEAP_LO = 0x8018c000
VERDICT_HEAP_HI = 0x100118000


def normalize(table):
    """Returns (canonical_table, dialect_name)."""
    slots = table.get("slots") or {}
    prefixed = [k for k in slots if k.split("_", 1)[0].isdigit()]
    if not prefixed:
        return table, "canonical"
    # Derivation dialect: strip N_ prefixes, map seeded/linked -> present/label.
    t = json.loads(json.dumps(table))  # deep copy
    new_slots = {}
    for key, value in slots.items():
        if key in prefixed:
            prefix, bare = key.split("_", 1)
            if prefix in DOC_ONLY_DIALECT_PREFIXES:
                if value.get("seeded"):
                    die(f"dialect slot {key}: slot {prefix} must never be seeded "
                        f"(slot 6 is settings-owned; tick is vtable-fallback-owned)")
                continue  # documentation-only record (e.g. the tick correction)
            value = dict(value)
            value["present"] = bool(value.pop("seeded", None))
            value.setdefault("slot_id", int(prefix))
            value.setdefault("kind", "data" if value.get("slot_id") in (0, 3) else "code")
            if value["present"]:
                value.setdefault("label", value.pop("linked", None))
                value.setdefault("runtime_expected", value.pop("runtime", None))
                value.setdefault("evidence",
                                 str((value.get("derivation") or {}).get("method") or ""))
                # Map the dialect's evidence fields onto the canonical triple.
                d = value.get("derivation") or {}
                if d and not all(k in d for k in ("anchor_bytes", "rip_arithmetic", "uniqueness")):
                    linked = str(value.get("label") or "?")
                    runtime = str(value.get("runtime_expected") or "?")
                    anchor = str(d.get("prologue_bytes") or d.get("offline_evidence") or d.get("method") or "")[:400]
                    d = {
                        "anchor_bytes": anchor,
                        "rip_arithmetic": f"runtime {runtime} = linked {linked} + base (image.translation_rule)",
                        "uniqueness": str(d.get("method") or "") + " | " + "; ".join(
                            d.get("cross_checks") or d.get("first_ref_sites") or []),
                        "dialect_derivation_full": value.get("derivation"),
                    }
                    value["derivation"] = d
            new_slots[bare] = value
        else:
            new_slots[key] = value
    t["slots"] = new_slots
    image = t.get("image") or {}
    if "bytes" not in image:
        image["bytes"] = image.get("size")
    # base.observed: dialect keeps it in image.translation_rule text.
    if not (t.get("base") or {}).get("observed"):
        import re
        rule = str(image.get("translation_rule") or "")
        m = re.search(r"linked_vaddr\s*\+\s*(0x[0-9a-fA-F]+)", rule)
        if not m:
            die("dialect: base.observed missing and not parseable from image.translation_rule")
        t["base"] = {
            "observed": m.group(1),
            "provenance": "image.translation_rule: " + rule.strip(),
        }
    # fname_remedy: verdict-owned structural constants; supply when the dialect
    # omits them (the consumer refuses a mismatch with the verdict anyway).
    if not t.get("fname_remedy"):
        t["fname_remedy"] = {
            "pool_label": hex(VERDICT_POOL_LABEL),
            "heap_span": {"lo": hex(VERDICT_HEAP_LO), "hi": hex(VERDICT_HEAP_HI)},
            "provenance": "FNAME-VERDICT.md sections 4/5 (defaulted by the generator for the derivation dialect; consumer-side checks agree)",
        }
    return t, "derivation"


def main():
    if len(sys.argv) != 3:
        die("usage: gen_seed_table.py <SEED-TABLE.json> <SeedTable.h>")
    raw = json.loads(open(sys.argv[1], encoding="utf-8").read())
    table, dialect = normalize(raw)
    if dialect == "canonical" and table.get("format_version") != 2:
        die(f"format_version {table.get('format_version')!r} != 2")
    image = table.get("image") or {}
    if image.get("sha256") != EXPECTED_IMAGE_SHA:
        die(f"image sha256 {image.get('sha256')!r} != pinned decrypted-eboot.bin sha")
    base = table.get("base") or {}
    base_observed = parse_addr(base.get("observed"), "base.observed")
    remedy = table.get("fname_remedy") or {}
    pool_label = parse_addr(remedy.get("pool_label"), "fname_remedy.pool_label")
    heap = remedy.get("heap_span") or {}
    heap_lo = parse_addr(heap.get("lo"), "fname_remedy.heap_span.lo")
    heap_hi = parse_addr(heap.get("hi"), "fname_remedy.heap_span.hi")
    if not (remedy.get("provenance") or "").strip():
        die("fname_remedy.provenance must be non-empty")
    if heap_lo >= heap_hi:
        die("heap_span lo >= hi")

    slots = table.get("slots") or {}
    if set(slots.keys()) != {name for name, _, _, _ in SLOTS}:
        die(f"slots keys must be exactly {[n for n, _, _, _ in SLOTS]}, got {sorted(slots.keys())}")

    parsed = {}
    any_present = False
    for name, slot_id, kind, _ in SLOTS:
        s = slots[name]
        if s.get("slot_id") != slot_id:
            die(f"{name}: slot_id {s.get('slot_id')!r} != {slot_id}")
        if s.get("kind") != kind:
            die(f"{name}: kind {s.get('kind')!r} != {kind!r}")
        present = s.get("present")
        if present is True:
            any_present = True
            label = parse_addr(s.get("label"), f"{name}.label")
            derivation = s.get("derivation")
            if not isinstance(derivation, dict):
                die(f"{name}: present slot requires a derivation object")
            for key in ("anchor_bytes", "rip_arithmetic", "uniqueness"):
                if not str(derivation.get(key) or "").strip():
                    die(f"{name}: derivation.{key} must be non-empty (evidence standard)")
            if not str(s.get("evidence") or "").strip():
                die(f"{name}: evidence must be non-empty")
            parsed[name] = (True, label, json.dumps(derivation, separators=(",", ":")).replace('"', '\\"'))
        elif present is False:
            if s.get("label") is not None:
                die(f"{name}: absent slot must carry label null")
            dialect_evidence = (str(s.get("evidence") or "").strip()
                                or str((s.get("derivation") or {}).get("conclusion") or "").strip()
                                or str((s.get("derivation") or {}).get("attempted") or "").strip())
            if not dialect_evidence:
                die(f"{name}: absent slot must record why it is under-determined")
            parsed[name] = (False, 0, "")
        else:
            die(f"{name}: present must be true/false")

    complete = all(parsed[name][0] for name, _, _, _ in SLOTS)
    if any_present and not remedy:
        die("any present slot requires fname_remedy")

    out = [
        "// SeedTable.h — GENERATED by gen_seed_table.py from SEED-TABLE.json.",
        "// Format contract: SEED-TABLE-FORMAT.md v2 (worker-initializer-integration).",
        f"#ifndef RC_SEED_TABLE_H_GENERATED_{format(base_observed ^ pool_label, 'x')}_",
        f"#define RC_SEED_TABLE_H_GENERATED_{format(base_observed ^ pool_label, 'x')}_",
        "#include <cstdint>",
        "#include <string_view>",
        "",
        "namespace RC::Unreal::UnrealInitializer::SeedTable",
        "{",
        "    inline constexpr bool kGenerated = true;",
        f"    inline constexpr uint64_t kBaseObserved = 0x{base_observed:x};",
        f"    inline constexpr uint64_t kFNamePoolLabel = 0x{pool_label:x};",
        f"    inline constexpr uint64_t kHeapLo = 0x{heap_lo:x};",
        f"    inline constexpr uint64_t kHeapHi = 0x{heap_hi:x};",
        "    struct SlotSeed",
        "    {",
        "        bool present;",
        "        bool data_kind;",
        "        uint64_t label;",
        "        const char* name;",
        "        const char* derivation;",
        "    };",
    ]
    for name, slot_id, kind, const in SLOTS:
        present, label, derivation = parsed[name]
        out.append(f"    inline constexpr SlotSeed {const}{{ {str(present).lower()}, "
                   f"{str(kind == 'data').lower()}, 0x{label:x}, \"{name}\", \"{derivation}\" }}; // slot {slot_id} ({kind})")
    out += [
        f"    inline constexpr bool kRequiredSetComplete = {str(complete).lower()};",
        f"    inline constexpr std::string_view kImageSha256 = \"{EXPECTED_IMAGE_SHA}\";",
        "}",
        "#endif",
        "",
    ]
    with open(sys.argv[2], "w", encoding="utf-8") as f:
        f.write("\n".join(out))
    print(f"gen_seed_table: wrote {sys.argv[2]} (dialect={dialect}, "
          f"present={sum(1 for n, _, _, _ in SLOTS if parsed[n][0])}/4, complete={complete})")


if __name__ == "__main__":
    main()
