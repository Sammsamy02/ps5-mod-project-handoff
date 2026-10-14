"""Value envelope and the detection context shared by every detector.

Every profile leaf is an ``Envelope``: ``{value, confidence, method, evidence[]}``.  A detector
never writes JSON; it calls ``Context.put`` with the facts it observed and the context turns that
into envelopes, so confidence/evidence handling lives in one place.
"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Dict, Iterable, List, Optional

HIGH = "high"
MEDIUM = "medium"
LOW = "low"
_ORDER = {LOW: 0, MEDIUM: 1, HIGH: 2}


def weakest(*levels: str) -> str:
    """The lowest confidence among ``levels`` (a derived value is only as good as its inputs)."""
    return min(levels, key=lambda l: _ORDER[l])


def strongest(*levels: str) -> str:
    return max(levels, key=lambda l: _ORDER[l])


class DetectorUnavailable(RuntimeError):
    """A detector cannot run (missing optional dependency, unsupported image shape)."""


class Hex(int):
    """An integer that serialises as a lowercase ``0x`` string (addresses, offsets, sizes)."""

    def __repr__(self) -> str:
        return hex_str(int(self))


def hex_str(n: int) -> str:
    return ("-0x%x" % -n) if n < 0 else ("0x%x" % n)


@dataclass
class Envelope:
    value: Any
    confidence: str
    method: str
    evidence: List[str] = field(default_factory=list)

    def to_json(self) -> Dict[str, Any]:
        v = self.value
        if isinstance(v, Hex):
            v = hex_str(int(v))
        return {"value": v, "confidence": self.confidence, "method": self.method,
                "evidence": list(self.evidence)}


@dataclass
class Unresolved:
    id: str
    why: str
    needs: str = "more-analysis"

    def to_json(self) -> Dict[str, str]:
        return {"id": self.id, "why": self.why, "needs": self.needs}


class Context:
    """What a detector gets: the image, its indexes, the param.json and earlier results."""

    def __init__(self, elf, index, param: Optional[dict] = None, options: Optional[dict] = None) -> None:
        self.elf = elf
        self.index = index
        self.param = param or {}
        self.options = options or {}
        self.values: Dict[str, Envelope] = {}      # dotted path -> envelope
        self.facts: Dict[str, Any] = {}            # raw machine facts passed between detectors
        self.evidence_files: Dict[str, Any] = {}   # name -> JSON-able detail for evidence/
        self.unresolved: List[Unresolved] = []
        self.timings: Dict[str, float] = {}
        self.notes: List[str] = []

    # -- writing results ------------------------------------------------------------------
    def put(self, path: str, value: Any, confidence: str, method: str,
            evidence: Optional[Iterable[str]] = None) -> Envelope:
        env = Envelope(value, confidence, method, list(evidence or []))
        self.values[path] = env
        return env

    def fail(self, ident: str, why: str, needs: str = "more-analysis") -> None:
        self.unresolved.append(Unresolved(ident, why, needs))

    # -- reading results ------------------------------------------------------------------
    def get(self, path: str) -> Optional[Envelope]:
        return self.values.get(path)

    def val(self, path: str, default: Any = None) -> Any:
        env = self.values.get(path)
        return default if env is None else env.value

    @property
    def load_base(self) -> int:
        return int(self.val("load.base", 0))

    def runtime_of(self, label: int) -> int:
        return label + self.load_base
