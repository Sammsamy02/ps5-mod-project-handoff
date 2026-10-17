"""Observations of member offsets, merged into envelopes at the end of the run.

Several detectors can see the *same* member from different code (``UObjectBase::ObjectFlags`` is
stored by the object constructor and tested by ``ProcessEvent``).  Each of them records an
``Observation`` -- ``(path, value, channel, method, evidence, cap)`` -- in the ``LayoutBook`` kept
in ``ctx.facts``; ``resolve`` then turns the book into profile envelopes with one rule:

* the observations of one path are grouped by value;
* **one value, two or more independent channels** -> ``high`` (unless every channel is capped lower);
* **one value, one channel** -> the channel's own cap, at most ``medium``, unless that channel is
  marked ``solo_high`` (a structural invariant that two lines of the same function cannot fake);
* **two values** -> nothing is written; the path is reported unresolved with both candidates, because
  a measurement that disagrees with itself is not a measurement;
* **no observation** -> the path is reported unresolved with the reason the detectors recorded
  (or the generic one).

A *channel* is a way of reaching the number, named after the code it reads (``uobject_base.ctor``,
``process_event.loads``); two observations from the same channel never count as two.
"""
from __future__ import annotations

from collections import OrderedDict
from typing import Any, Dict, Iterable, List, Optional, Tuple

from ..model import HIGH, LOW, MEDIUM, Context, Hex, weakest

BOOK_KEY = "layout_book"
_RANK = {LOW: 0, MEDIUM: 1, HIGH: 2}


class Observation:
    __slots__ = ("path", "value", "channel", "method", "evidence", "cap", "solo_high")

    def __init__(self, path: str, value: Any, channel: str, method: str, evidence: Iterable[str], cap: str,
                 solo_high: bool) -> None:
        self.path, self.value, self.channel, self.method = path, value, channel, method
        self.evidence, self.cap, self.solo_high = list(evidence), cap, solo_high


class LayoutBook:
    def __init__(self) -> None:
        self.obs: "OrderedDict[str, List[Observation]]" = OrderedDict()
        self.reasons: Dict[str, str] = {}
        self.expected: List[str] = []

    def observe(self, path: str, value: Any, channel: str, method: str, evidence: Iterable[str],
                cap: str = MEDIUM, solo_high: bool = False) -> None:
        self.obs.setdefault(path, []).append(Observation(path, value, channel, method, evidence, cap, solo_high))

    def expect(self, paths: Iterable[str], reason: Optional[str] = None) -> None:
        for p in paths:
            if p not in self.expected:
                self.expected.append(p)
            if reason and p not in self.reasons:
                self.reasons[p] = reason

    def why_not(self, path: str, reason: str) -> None:
        """Record a more specific reason for an expected path (replaces the generic one)."""
        self.reasons[path] = reason


def book(ctx: Context) -> LayoutBook:
    b = ctx.facts.get(BOOK_KEY)
    if b is None:
        b = ctx.facts[BOOK_KEY] = LayoutBook()
    return b


def _key(value: Any) -> Any:
    return int(value) if isinstance(value, int) else value


def decide(observations: List[Observation]) -> Tuple[Optional[Any], str, str, List[str], List[Tuple[Any, List[str]]]]:
    """(value or None, confidence, method, evidence, conflicts)."""
    groups: "OrderedDict[Any, List[Observation]]" = OrderedDict()
    for o in observations:
        groups.setdefault(_key(o.value), []).append(o)
    if len(groups) > 1:
        return None, LOW, "", [], [(v, sorted({o.channel for o in os})) for v, os in groups.items()]
    value, obs = next(iter(groups.items()))
    channels: Dict[str, Observation] = {}
    for o in obs:
        best = channels.get(o.channel)
        if best is None or _RANK[o.cap] > _RANK[best.cap]:
            channels[o.channel] = o
    ordered = sorted(channels.values(), key=lambda o: (-_RANK[o.cap], o.channel))
    strong = [o for o in ordered if o.cap != LOW]
    if len(strong) >= 2:
        conf = HIGH
    else:
        top = ordered[0]
        conf = HIGH if (top.solo_high and top.cap == HIGH) else weakest(top.cap, MEDIUM)
    method = "+".join(dict.fromkeys(o.method for o in ordered))
    evidence: List[str] = []
    for o in ordered:
        evidence.extend(("[%s] %s" % (o.channel, line)) for line in o.evidence)
    if len(channels) > 1:
        evidence.append("%d independent channels agree: %s" % (len(channels), ", ".join(sorted(channels))))
    return value, conf, method, evidence, []


def resolve(ctx: Context) -> None:
    """Write one envelope per observed path (and one unresolved entry per expected path without a value)."""
    b: Optional[LayoutBook] = ctx.facts.get(BOOK_KEY)
    if b is None or ctx.facts.get(BOOK_KEY + ".resolved"):
        return
    ctx.facts[BOOK_KEY + ".resolved"] = True
    seen = set()
    for path, os in b.obs.items():
        seen.add(path)
        prior = ctx.values.get(path)                       # an envelope another detector wrote joins as one more channel
        if prior is not None and isinstance(prior.value, int):
            os = os + [Observation(path, int(prior.value), "earlier:" + prior.method, prior.method, prior.evidence, prior.confidence, False)]
        value, conf, method, evidence, conflicts = decide(os)
        if value is None:
            ctx.fail(path, "channels disagree: " + "; ".join("%s from %s" % (
                hex(v) if isinstance(v, int) else v, "+".join(ch)) for v, ch in conflicts), "more-analysis")
            continue
        ctx.put(path, Hex(value) if isinstance(value, int) else value, conf, method, evidence)
    for path in b.expected:
        if path in seen or path in ctx.values:
            continue
        ctx.fail(path, b.reasons.get(path, "no analysed code reads or writes this member, so the generator has no measurement"),
                 "more-analysis")
