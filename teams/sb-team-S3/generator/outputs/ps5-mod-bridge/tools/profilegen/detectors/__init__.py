"""Detectors: one module per value family, each ``detect(ctx)`` writing envelopes into the context.

The order matters only where a detector consumes an earlier result (``ctx.facts``):

    platform        binding / ELF / load base / build id
    build_axes      shipping vs development, STATS, version estimate
    engine_version  major.minor.patch, changelist, branch, revision tag
    fname_ctor      FName::FName(const wchar_t*)
    fname_pool      FNamePool, its layout, FMemory::Malloc          (-> gmalloc)
    gmalloc         GMalloc, FMemory::Free, FMalloc vtable slots
    uobject_array   GUObjectArray + FUObjectArray/TUObjectArray layout
    static_construct   StaticConstructObject_Internal
    static_find_object StaticFindObject
    process_event   UObject::ProcessEvent vtable slot + function

    The layout detectors below run after the ones above because they consume their facts (the FName constructor, the
    allocator, GUObjectArray, StaticConstructObject, ProcessEvent).  Each records *observations* of member offsets in the
    layout book (``_book.py``); ``layout_merge`` runs last and turns them into envelopes, so two detectors that see the
    same member from different code corroborate each other (and a disagreement is reported, not published).

    class_registration  native class registration helper, class sizes, UStruct / UClass layout it writes
    uobject_base        UObjectBase constructor and FUObjectArray::AllocateUObjectIndex: UObjectBase, FUObjectItem
    process_event_members  members ProcessEvent reads: UFunction, UStruct, FField, FProperty (+ two property vtable slots)
    class_default_object   UClass::CreateDefaultObject: ClassDefaultObject, SuperStruct, FField chain
    field_classes       FFieldClass constructor, FField constructor, property class sizes
    property_subtypes   FStructProperty / FArrayProperty / FEnumProperty / FBoolProperty members (overridden virtuals)
    engine_tick         UGameEngine::Tick and the UEngine::Tick vtable slot
    fname_tostring      FName::ToString (both overloads)
    gnatives            GNatives, the native script function table
    layout_merge        resolve the layout book into envelopes
"""
from __future__ import annotations

import time
import traceback
from typing import Callable, List, Tuple

from ..model import Context, DetectorUnavailable
from . import (build_axes, class_default_object, class_registration, engine_tick, engine_version, field_classes, fname_ctor,
               fname_pool, fname_tostring, gmalloc, gnatives, layout_merge, platform, process_event, process_event_members,
               property_subtypes, static_construct, static_find_object, uobject_array, uobject_base)

ORDER: List[Tuple[str, Callable[[Context], None]]] = [
    ("platform", platform.detect),
    ("build_axes", build_axes.detect),
    ("engine_version", engine_version.detect),
    ("fname_ctor", fname_ctor.detect),
    ("fname_pool", fname_pool.detect),
    ("gmalloc", gmalloc.detect),
    ("uobject_array", uobject_array.detect),
    ("static_construct", static_construct.detect),
    ("static_find_object", static_find_object.detect),
    ("process_event", process_event.detect),
    ("class_registration", class_registration.detect),
    ("uobject_base", uobject_base.detect),
    ("process_event_members", process_event_members.detect),
    ("class_default_object", class_default_object.detect),
    ("field_classes", field_classes.detect),
    ("property_subtypes", property_subtypes.detect),
    ("engine_tick", engine_tick.detect),
    ("fname_tostring", fname_tostring.detect),
    ("gnatives", gnatives.detect),
    ("layout_merge", layout_merge.detect),
]

# detectors that only record observations; ``layout_merge`` turns them into envelopes, so it always follows them
LAYOUT_DETECTORS = ("class_registration", "uobject_base", "process_event_members", "class_default_object", "field_classes",
                    "property_subtypes")


def run_all(ctx: Context, strict: bool = False, only=None) -> None:
    if only and any(n in only for n in LAYOUT_DETECTORS):
        only = set(only) | {"layout_merge"}
    for name, fn in ORDER:
        if only and name not in only:
            continue
        t0 = time.time()
        try:
            fn(ctx)
        except DetectorUnavailable as exc:
            ctx.fail("detector." + name, "unavailable: %s" % exc, "install the optional dependency")
        except Exception as exc:                     # a broken detector must not lose the others
            if strict:
                raise
            ctx.fail("detector." + name, "crashed: %s: %s" % (type(exc).__name__, exc), "more-analysis")
            ctx.notes.append(traceback.format_exc())
        ctx.timings["detector." + name] = round(time.time() - t0, 3)
