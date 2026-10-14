"""A tiny linear data-flow tagger shared by the layout detectors.

The member-layout detectors all ask the same question about one function: *which incoming value
(argument, stack argument, call result, ...) is stored at which offset of which object?*  The
class-registration helper stores its size argument into the new ``UClass``; the ``UObjectBase``
constructor stores ``InClass`` / ``InFlags`` / ``InOuter`` / ``InName``; ``ProcessEvent`` loads
the fields of its ``UFunction`` argument.  ``Flow`` answers it by walking the decoded instructions
once, in address order, keeping a *tag* for every register and every ``[rbp-X]`` spill slot:

    ("arg", i)             i-th SysV integer argument (rdi rsi rdx rcx r8 r9 = 0..5)
    ("stk", i)             i-th stack argument (``[rbp+0x10+8*i]`` in a frame-pointer function)
    ("stkv", i, n)         n consecutive stack arguments loaded by one vector move
    ("imm", v)             an immediate (zero idioms included)
    ("addr", va)           ``lea r,[rip+X]`` -- the address X
    ("glob", va)           the contents of the global at X
    ("fp", disp)           the address of the stack slot ``[rbp+disp]``
    ("ptr", base, disp)    ``lea r,[base+disp]``: an interior pointer
    ("ld", base, disp, n)  an n-byte load from ``[base+disp]``
    ("ret", site, target)  the result of the call at ``site`` (``target`` = callee vaddr, or the tag the
                           indirect call went through)
    ("out", site, target, k)  what the call at ``site`` wrote through its k-th argument (a stack slot)
    ("op", frozenset)      anything computed from several tags (or/add/and/...)

Control flow is ignored: the walk is linear and a tag is only as good as that approximation, which
is why every detector that uses it demands several independent sites or a second channel before it
reports more than ``medium``.  Nothing here knows about any game.
"""
from __future__ import annotations

from typing import Any, Dict, Iterable, List, Optional, Tuple

from ..x86util import CALLER_SAVED, canon64

CALLEE_SAVED = {"rbx", "rbp", "r12", "r13", "r14", "r15"}

Tag = Tuple[Any, ...]
ARG_REGS = ["rdi", "rsi", "rdx", "rcx", "r8", "r9"]

_ALU = {"add", "sub", "or", "and", "xor", "adc", "sbb", "shl", "shr", "sar", "sal", "rol", "ror", "imul", "andn",
        "neg", "not", "inc", "dec", "bts", "btr", "btc", "bsf", "bsr", "popcnt", "lzcnt", "tzcnt", "bswap"}
_MOVE = {"mov", "movabs", "movzx", "movsx", "movsxd"}
_VMOV_LOAD = {"movups", "movaps", "movdqu", "movdqa", "vmovups", "vmovaps", "vmovdqu", "vmovdqa", "movq", "vmovq",
              "movsd", "vmovsd", "movss", "vmovss"}
_VZERO = {"xorps", "vxorps", "pxor", "vpxor", "xorpd", "vxorpd"}


def imm_tag(v: int) -> Tag:
    return ("imm", int(v))


def mentions(tag: Optional[Tag], kind: str, index: Optional[int] = None) -> bool:
    """Does ``tag`` (recursively through ``op``) contain ``(kind, index)``?"""
    if tag is None:
        return False
    if tag[0] == kind and (index is None or tag[1] == index):
        return True
    if tag[0] == "op":
        return any(mentions(t, kind, index) for t in tag[1])
    return False


def combine(*tags: Optional[Tag]) -> Optional[Tag]:
    flat = set()
    for t in tags:
        if t is None:
            return None
        if t[0] == "op":
            flat |= set(t[1])
        elif t[0] != "imm":
            flat.add(t)
    if not flat:
        return ("imm", 0)
    if len(flat) == 1:
        return next(iter(flat))
    return ("op", frozenset(flat))


class Event:
    """One memory access or call seen by the walk."""

    __slots__ = ("kind", "addr", "base", "disp", "size", "value", "target", "args", "mnemonic")

    def __init__(self, kind, addr, base=None, disp=0, size=0, value=None, target=None, args=None, mnemonic=""):
        self.kind, self.addr, self.base, self.disp, self.size = kind, addr, base, disp, size
        self.value, self.target, self.args, self.mnemonic = value, target, args or {}, mnemonic

    def __repr__(self) -> str:                                        # pragma: no cover - debugging aid
        return "Event(%s 0x%x base=%r disp=0x%x size=%d value=%r target=%r)" % (
            self.kind, self.addr, self.base, self.disp, self.size, self.value, self.target)


class Flow:
    """Walk ``ins`` (capstone instructions with detail) once and collect the events."""

    def __init__(self, decoder, ins: List[Any], args: Optional[Dict[str, Tag]] = None, frame_args: bool = True,
                 max_ins: int = 4000) -> None:
        from capstone import x86 as xo
        from capstone import x86_const as xc
        self._xo, self._xc = xo, xc
        self.dec = decoder
        self.ins = ins[:max_ins]
        self.regs: Dict[str, Tag] = {}
        for k, name in enumerate(ARG_REGS):
            self.regs[name] = ("arg", k)
        if args:
            self.regs.update(args)
        self.frame: Dict[int, Tag] = {}
        self.ver: Dict[str, int] = {}
        self.frame_args = frame_args
        self.events: List[Event] = []
        self._snaps: Dict[int, Tuple[Dict[str, Tag], Dict[int, Tag]]] = {}
        self._run()

    # -- helpers -------------------------------------------------------------------------------------------
    def _name(self, rid: int) -> str:
        return canon64(self.dec.reg_name(rid)) if rid else ""

    def _is_rbp(self, op) -> bool:
        return op.type == self._xo.X86_OP_MEM and op.mem.base == self._xc.X86_REG_RBP and op.mem.index == 0

    def _stack_arg(self, disp: int, size: int) -> Optional[Tag]:
        if not self.frame_args or disp < 0x10 or (disp - 0x10) % 8:
            return None
        k = (disp - 0x10) // 8
        if size == 16:
            return ("stkv", k, 2)
        return ("stk", k)

    def _load_tag(self, op) -> Optional[Tag]:
        """Tag of the value a memory operand reads (or None when unknown)."""
        xo, xc = self._xo, self._xc
        m = op.mem
        if m.base == xc.X86_REG_RIP:
            return None
        if self._is_rbp(op):
            t = self.frame.get(m.disp)
            if t is not None:
                return t
            return self._stack_arg(m.disp, op.size)
        if m.base and m.index == 0:
            b = self.regs.get(self._name(m.base))
            if b is not None:
                return ("ld", b, m.disp, op.size)
        return None

    def _mem_base(self, op) -> Optional[Tag]:
        """Tag of the base register; an untagged register is named by (register, write count)."""
        m = op.mem
        if m.index or not m.base or m.base == self._xc.X86_REG_RIP:
            return None
        name = self._name(m.base)
        if name in ("rsp", "rbp"):
            return None
        t = self.regs.get(name)
        return t if t is not None else ("reg", name, self.ver.get(name, 0))

    def _kill(self, reg: str) -> None:
        self.regs.pop(reg, None)
        self.ver[reg] = self.ver.get(reg, 0) + 1

    def _set(self, reg: str, tag: Optional[Tag]) -> None:
        self.ver[reg] = self.ver.get(reg, 0) + 1
        if tag is None:
            self.regs.pop(reg, None)
        else:
            self.regs[reg] = tag

    def _src_tag(self, op) -> Optional[Tag]:
        xo = self._xo
        if op.type == xo.X86_OP_IMM:
            return imm_tag(op.imm)
        if op.type == xo.X86_OP_REG:
            return self.regs.get(self._name(op.reg))
        if op.type == xo.X86_OP_MEM:
            return self._load_tag(op)
        return None

    # -- the walk ------------------------------------------------------------------------------------------
    def _snapshot_to(self, target: int) -> None:
        """Remember the state at a forward branch (the first branch to a target wins, later ones only add what it lacked)."""
        old = self._snaps.get(target)
        if old is None:
            self._snaps[target] = (dict(self.regs), dict(self.frame))
        else:
            regs = dict(old[0])
            for k, v in self.regs.items():
                regs.setdefault(k, v)
            frame = dict(old[1])
            for k, v in self.frame.items():
                frame.setdefault(k, v)
            self._snaps[target] = (regs, frame)

    def _join(self, addr: int, after_terminal: bool) -> None:
        snap = self._snaps.get(addr)
        if snap is None:
            return
        if after_terminal:                               # only reachable through the earlier branch(es)
            self.regs, self.frame = dict(snap[0]), dict(snap[1])
        else:                                            # also reached by falling through: the fall-through tags win, the
            for k, v in snap[0].items():                  # branch only fills in registers the fall-through path left unknown
                self.regs.setdefault(k, v)
            for k, v in snap[1].items():
                self.frame.setdefault(k, v)

    def _run(self) -> None:
        xo, xc = self._xo, self._xc
        terminal = False
        for i in self.ins:
            mn = i.mnemonic
            ops = i.operands
            self._join(i.address, terminal)
            terminal = mn in ("ret", "jmp", "ud2", "hlt")
            if mn.startswith("j") and ops and ops[0].type == xo.X86_OP_IMM and ops[0].imm > i.address:
                self._snapshot_to(int(ops[0].imm))
            if mn == "call":
                self._call(i, ops)
                continue
            if mn == "ret":
                self.events.append(Event("ret", i.address, None, 0, 0, self.regs.get("rax")))
                continue
            if mn.startswith("j"):
                continue
            if mn in ("push", "pop", "nop", "cmp", "test", "endbr64", "int3", "ud2", "cdqe", "cqo", "cdq", "cwde"):
                if mn == "pop" and ops and ops[0].type == xo.X86_OP_REG and self._name(ops[0].reg) not in CALLEE_SAVED:
                    self._kill(self._name(ops[0].reg))               # epilogue pops must not hide the tags of the other paths
                if mn in ("cmp", "test"):
                    self._compare(i, ops)
                continue
            if not ops:
                continue
            dst = ops[0]
            # --- stores / read-modify-write on memory
            if dst.type == xo.X86_OP_MEM and mn not in ("lea", "prefetcht0", "prefetchw"):
                m = dst.mem
                if m.index and m.base and m.base != xc.X86_REG_RIP:
                    # scaled/indexed destination: remember which tagged values form the address and what is written
                    bt = self.regs.get(self._name(m.base))
                    it = self.regs.get(self._name(m.index))
                    if bt is not None or it is not None:
                        self.events.append(Event("idxmem", i.address, bt, m.disp, dst.size, it,
                                                 self._src_tag(ops[1]) if len(ops) >= 2 else None, None, mn))
                base = self._mem_base(dst)
                value = None
                if len(ops) >= 2:
                    value = self._src_tag(ops[1])
                    if mn in _VZERO:
                        value = imm_tag(0)
                if mn in _MOVE or mn in _VMOV_LOAD or mn in ("movnti", "stosq"):
                    wr = value
                else:
                    wr = combine(self._load_tag(dst), value) if value is not None else None
                if mn in ("xchg", "cmpxchg", "xadd") or mn.startswith(("lock",)):
                    wr = None
                if base is not None:
                    self.events.append(Event("store", i.address, base, dst.mem.disp, dst.size, wr, mnemonic=mn))
                if self._is_rbp(dst):
                    if wr is None:
                        self.frame.pop(dst.mem.disp, None)
                    else:
                        self.frame[dst.mem.disp] = wr
                continue
            if dst.type != xo.X86_OP_REG:
                continue
            reg = self._name(dst.reg)
            full = dst.size >= 4
            # --- reads of memory (for the event log) when the source is a memory operand
            if len(ops) >= 2 and ops[1].type == xo.X86_OP_MEM and mn != "lea":
                src = ops[1]
                base = self._mem_base(src)
                if base is not None:
                    self.events.append(Event("load", i.address, base, src.mem.disp, src.size, None, mnemonic=mn))
            if mn == "lea" and len(ops) == 2 and ops[1].type == xo.X86_OP_MEM:
                m = ops[1].mem
                if m.base == xc.X86_REG_RIP and not m.index:
                    self._set(reg, ("addr", i.address + i.size + m.disp))
                elif m.base == xc.X86_REG_RBP and not m.index:
                    self._set(reg, ("fp", m.disp))
                elif m.base and not m.index and self.regs.get(self._name(m.base)) is not None:
                    self._set(reg, ("ptr", self.regs[self._name(m.base)], m.disp))
                else:
                    self._set(reg, None)
                continue
            if mn in _MOVE and len(ops) == 2:
                if not full and mn == "mov":
                    self._set(reg, None)
                    continue
                if ops[1].type == xo.X86_OP_MEM and ops[1].mem.base == xc.X86_REG_RIP:
                    self._set(reg, ("glob", i.address + i.size + ops[1].mem.disp))
                    continue
                t = self._src_tag(ops[1])
                if mn in ("movzx", "movsx", "movsxd") and t is not None and t[0] == "imm":
                    t = None
                self._set(reg, t)
                continue
            if mn in _VMOV_LOAD and len(ops) == 2 and ops[1].type in (xo.X86_OP_MEM, xo.X86_OP_REG):
                t = self._src_tag(ops[1])
                self._set(self._xmm(dst), t)
                continue
            if mn in _VZERO and len(ops) >= 2:
                self._set(self._xmm(dst), imm_tag(0))
                continue
            if mn.startswith("cmov") and len(ops) == 2:
                t = combine(self.regs.get(reg), self._src_tag(ops[1]))
                self._set(reg, t)
                continue
            if mn == "xor" and len(ops) == 2 and ops[1].type == xo.X86_OP_REG and ops[1].reg == dst.reg:
                self._set(reg, imm_tag(0))
                continue
            if mn in _ALU:
                if mn in ("and", "andn", "or") and len(ops) >= 2:
                    srcs = [self._src_tag(o) for o in ops]
                    big = [t for t in srcs if t is not None and t[0] == "imm" and t[1] >= 0x10000]
                    loaded = [t for t in srcs if t is not None and t[0] == "ld"]
                    if big and loaded:
                        self.events.append(Event("mask", i.address, loaded[0], int(big[0][1]), 8, None, mnemonic=mn))
                if len(ops) >= 2:
                    t = combine(self.regs.get(reg), self._src_tag(ops[1]))
                    if len(ops) == 3:                                  # imul r, r/m, imm
                        t = combine(self._src_tag(ops[1]))
                else:
                    t = combine(self.regs.get(reg))
                self._set(reg, t)
                continue
            if mn in ("xchg",):
                self._set(reg, None)
                if len(ops) == 2 and ops[1].type == xo.X86_OP_REG:
                    self._set(self._name(ops[1].reg), None)
                continue
            # unknown writer: forget the destination register
            for w in self._written(i):
                self._kill(w)

    def _xmm(self, op) -> str:
        return self.dec.reg_name(op.reg) if op.reg else ""

    def _written(self, i) -> Iterable[str]:
        try:
            _r, w = i.regs_access()
        except Exception:
            return ()
        return [self._name(r) for r in w]

    def _compare(self, i, ops) -> None:
        xo = self._xo
        if len(ops) != 2:
            return
        a, b = ops
        if a.type == xo.X86_OP_REG and b.type == xo.X86_OP_IMM:
            t = self.regs.get(self._name(a.reg))
            if t is not None:
                self.events.append(Event("cmpreg", i.address, t, int(b.imm), a.size, None, mnemonic=i.mnemonic))
        if a.type == xo.X86_OP_MEM:
            base = self._mem_base(a)
            if base is not None:
                imm = b.imm if b.type == xo.X86_OP_IMM else None
                self.events.append(Event(i.mnemonic, i.address, base, a.mem.disp, a.size,
                                         imm_tag(imm) if imm is not None else self._src_tag(b), mnemonic=i.mnemonic))

    def _call(self, i, ops) -> None:
        xo = self._xo
        args = {name: self.regs.get(name) for name in ARG_REGS}
        site = i.address
        target: Any = None
        if ops and ops[0].type == xo.X86_OP_IMM:
            target = int(ops[0].imm)
        elif ops and ops[0].type == xo.X86_OP_MEM:
            m = ops[0].mem
            if self._is_rbp(ops[0]):
                target = self.frame.get(m.disp) or self._stack_arg(m.disp, 8)
            else:
                base = self._mem_base(ops[0])
                if base is not None:
                    target = ("vcall", base, m.disp)
        elif ops and ops[0].type == xo.X86_OP_REG:
            target = self.regs.get(self._name(ops[0].reg))
        self.events.append(Event("call", site, None, 0, 0, None, target, args, "call"))
        for k, name in enumerate(ARG_REGS):                       # stack slots whose address was passed get a result tag
            t = args.get(name)
            if t is not None and t[0] == "fp":
                self.frame[t[1]] = ("out", site, target, k)
        for r in CALLER_SAVED:
            self._kill(r)
        self.regs["rax"] = ("ret", site, target)

    # -- queries -------------------------------------------------------------------------------------------
    def stores(self, base_pred=None, size: Optional[int] = None) -> List[Event]:
        out = []
        for e in self.events:
            if e.kind != "store":
                continue
            if size is not None and e.size != size:
                continue
            if base_pred is not None and not base_pred(e.base):
                continue
            out.append(e)
        return out

    def calls(self) -> List[Event]:
        return [e for e in self.events if e.kind == "call"]
