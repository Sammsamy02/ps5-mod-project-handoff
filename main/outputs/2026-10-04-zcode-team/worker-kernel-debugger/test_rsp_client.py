#!/usr/bin/env python3
"""
test_rsp_client.py - GDB Remote Serial Protocol conformance client.

Speaks raw RSP over TCP against ./gdbstub exactly like GDB/LLDB do
($data#checksum framing, + / - acks, '}' escaping, '*' RLE, no-ack mode)
and asserts on every packet class the prototype claims to support:

  handshake : qSupported, QStartNoAckMode, qHostInfo, qAttached, qC
  xml       : qXfer:features:read:target.xml, qRegisterInfo (LLDB path)
  registers : g / G / p / P   (x86-64 536-byte layout)
  memory    : m / M / X  (incl. RLE-compressed M, fault -> E01)
  breakpoints: Z0 / z0 (0xCC patch + restore), Z1 unsupported
  exec      : c / s / vCont? / vCont;c answered as immediate stops
  attach    : vAttach;pid, qRcmd (monitor) O-packet output
  wire      : bad checksum -> '-', retransmit recovers; raw 0x03 -> T02

Usage: test_rsp_client.py [port]   (default 2345; stub must be running)
Exit code 0 = all checks passed.
"""
import socket
import sys
import time

HOST = "127.0.0.1"
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 2345

MOCK_BASE = 0x400000
PASS = 0
FAIL = 0
FAILURES = []


class RSPClient:
    def __init__(self, host, port):
        self.s = socket.create_connection((host, port), timeout=5)
        self.s.settimeout(5)
        self.noack = False
        self.log = []

    # ---- wire helpers -------------------------------------------------
    @staticmethod
    def _csum(data: bytes) -> int:
        return sum(data) & 0xFF

    def send_packet(self, data: bytes, expect_ack=True, escape=True):
        payload = b""
        for b in data:  # escape $ # } *
            if escape and b in (0x24, 0x23, 0x7D, 0x2A):
                payload += b"}" + bytes([b ^ 0x20])
            else:
                payload += bytes([b])
        frame = b"$" + payload + b"#%02x" % self._csum(payload)
        for _ in range(4):
            self.s.sendall(frame)
            if self.noack:
                return
            a = self.s.recv(1)
            if a == b"+":
                return
            if a != b"-":
                raise ConnectionError("lost peer during ack: %r" % a)
        raise ConnectionError("peer kept naking")

    def _read_byte(self) -> int:
        b = self.s.recv(1)
        if not b:
            raise ConnectionError("eof")
        return b[0]

    def recv_packet(self) -> bytes:
        while True:
            c = self._read_byte()
            if c in (ord("+"), ord("-")):
                continue
            if c == ord("$"):
                break
        raw = bytearray()
        while True:
            c = self._read_byte()
            if c == ord("#"):
                break
            raw.append(c)
        c1 = self._read_byte()
        c2 = self._read_byte()
        csum = int(bytes([c1, c2]), 16)
        if csum != self._csum(raw):
            if not self.noack:
                self.s.sendall(b"-")
            return self.recv_packet()
        if not self.noack:
            self.s.sendall(b"+")
        # unescape + RLE-expand
        out = bytearray()
        i = 0
        while i < len(raw):
            b = raw[i]
            i += 1
            if b == ord("}"):
                out.append(raw[i] ^ 0x20)
                i += 1
            elif b == ord("*") and out and i < len(raw):
                rep = raw[i] - 29
                i += 1
                assert 1 <= rep <= 97, "bad RLE count"
                out.extend(bytes([out[-1]]) * rep)
            else:
                out.append(b)
        return bytes(out)

    # ---- convenience ---------------------------------------------------
    def cmd(self, packet) -> bytes:
        p = packet.encode() if isinstance(packet, str) else packet
        self.send_packet(p)
        r = self.recv_packet()
        self.log.append((p, r))
        return r

    def close(self):
        try:
            self.s.sendall(b"+")  # courtesy ack noise, must be ignored
        except OSError:
            pass
        self.s.close()


def check(name, cond, detail=""):
    global PASS, FAIL
    if cond:
        PASS += 1
        print("  ok   %s" % name)
    else:
        FAIL += 1
        FAILURES.append(name)
        print("  FAIL %s   %s" % (name, detail))


def hx(n):
    return b"%x" % n


def main():
    c = RSPClient(HOST, PORT)
    print("connected to %s:%d" % (HOST, PORT))

    # ---------------- handshake (GDB order) ---------------------------
    r = c.cmd("qSupported:multiprocess+;qXfer:features:read+")
    check("qSupported advertises PacketSize=4000", b"PacketSize=4000" in r, r)
    check("qSupported advertises target.xml xfer",
          b"qXfer:features:read+" in r, r)
    check("qSupported no-ack support", b"QStartNoAckMode+" in r, r)
    check("qSupported declines multiprocess", b"multiprocess-" in r, r)

    # bad checksum while still in ACK mode: server must '-' and the
    # retransmission must then be honored
    good = b"m%x,4" % MOCK_BASE
    c.s.sendall(b"$" + good + b"#99")  # wrong checksum
    a = c.s.recv(1)
    check("bad checksum in ack mode -> '-'", a == b"-", a)
    c.send_packet(good)
    r = c.recv_packet()
    check("retransmit after '-' accepted", r == b"7f454c46", r)

    r = c.cmd("QStartNoAckMode")
    check("QStartNoAckMode -> OK", r == b"OK", r)
    c.noack = True  # wire switches after this reply

    r = c.cmd("qHostInfo")
    triple = b""
    for part in r.split(b";"):
        if part.startswith(b"triple:"):
            triple = bytes.fromhex(part[7:].decode())
    check("qHostInfo triple=x86_64-unknown-freebsd12",
          triple == b"x86_64-unknown-freebsd12", r)

    r = c.cmd("qAttached")
    check("qAttached -> 1 (never kill on quit)", r == b"1", r)

    r = c.cmd("?")
    check("? -> T05 stop reply", r.startswith(b"T05"), r)

    # ---------------- target description ------------------------------
    xml = b""
    off = 0
    while True:
        r = c.cmd("qXfer:features:read:target.xml:%x,400" % off)
        if r.startswith(b"m"):
            xml += r[1:]
            off += len(r) - 1
        elif r == b"l":
            break
        else:
            raise AssertionError("bad qXfer reply %r" % r)
    check("target.xml architecture i386:x86-64",
          b"<architecture>i386:x86-64</architecture>" in xml,
          xml[:120])
    check("target.xml self-contained (no xi:include)",
          b"xi:include" not in xml, xml[:200])
    check("target.xml defines rip regnum 16",
          b'name="rip" regnum="16"' in xml)
    check("target.xml defines xmm15 regnum 55",
          b'name="xmm15" regnum="55"' in xml)
    check("target.xml defines mxcsr regnum 56",
          b'name="mxcsr" regnum="56"' in xml)

    r = c.cmd("qRegisterInfo0")
    check("qRegisterInfo0 -> name:rax (LLDB fallback)",
          r.startswith(b"name:rax;"), r)
    r = c.cmd("qRegisterInfo10")
    check("qRegisterInfo10 -> name:rip", r.startswith(b"name:rip;"), r)

    # ---------------- register file: g / p / P / G ---------------------
    r = c.cmd("g")
    check("g packet length = 1072 hex chars", len(r) == 1072, len(r))
    regs = {}
    off = 0
    widths = [8] * 16 + [8, 4] + [4] * 6 + [10] * 8 + [4] * 8 + [16] * 16 + [4]
    names = (["rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp"] +
             ["r%d" % i for i in range(8, 16)] +
             ["rip", "eflags", "cs", "ss", "ds", "es", "fs", "gs"] +
             ["st%d" % i for i in range(8)] +
             ["fctrl", "fstat", "ftag", "fiseg", "fioff", "foseg",
              "fooff", "fop"] +
             ["xmm%d" % i for i in range(16)] + ["mxcsr"])
    for nm, w in zip(names, widths):
        regs[nm] = int.from_bytes(bytes.fromhex(r[off:off + w * 2]
                                                .decode()), "little")
        off += w * 2
    check("g layout ends exactly at 536 bytes", off == 1072)
    check("g rax = 0x1122334455667788",
          regs["rax"] == 0x1122334455667788, hex(regs["rax"]))
    check("g rbx = 0xdeadbeefcafe0000",
          regs["rbx"] == 0xDEADBEEFCAFE0000, hex(regs["rbx"]))
    check("g rip = 0x400104",
          regs["rip"] == MOCK_BASE + 0x104, hex(regs["rip"]))
    check("g rsp = 0x7ffffffe00",
          regs["rsp"] == 0x7FFFFFFE00, hex(regs["rsp"]))
    check("g eflags = 0x246 (ZF|PF|IF)", regs["eflags"] == 0x246,
          hex(regs["eflags"]))
    check("g cs = 0x33", regs["cs"] == 0x33, hex(regs["cs"]))
    check("g xmm0 = 0x5a5a...5a",
          regs["xmm0"] == 0x5A5A5A5A5A5A5A5A5A5A5A5A5A5A5A5A)

    r = c.cmd("p0")  # rax
    check("p0 -> rax little-endian", r == b"8877665544332211", r)
    r = c.cmd("p10")  # rip
    check("p10 -> rip", r == b"0401400000000000", r)
    r = c.cmd("p27")  # hex 0x27 = regnum 39 = fop
    check("p27 (fop, last fp reg) -> 00000000", r == b"00000000", r)
    r = c.cmd("p38")  # hex 0x38 = regnum 56 = mxcsr
    check("p38 (mxcsr) -> 0x1f80 LE", r == b"801f0000", r)
    r = c.cmd("p39")  # hex 0x39 = regnum 57 -> out of range
    check("p39 -> E01 (out of range)", r == b"E01", r)

    r = c.cmd("P1=" + "11" * 8)  # rbx = 0x1111111111111111
    check("P rbx -> OK", r == b"OK", r)
    r = c.cmd("p1")
    check("p1 reflects P write (rbx=0x1111...)",
          r == b"1111111111111111", r)

    # G: full file write (change rcx via G, keep the rest)
    gbuf = bytearray(c.cmd("g"))
    rcx_off = 2 * (8 * 2)  # regnum 2 * width 8 -> hex offset 32
    gbuf[rcx_off:rcx_off + 16] = b"2222222222222222"
    r = c.cmd("G" + gbuf.decode())
    check("G -> OK", r == b"OK", r)
    r = c.cmd("p2")
    check("p2 reflects G write (rcx=0x2222...)",
          r == b"2222222222222222", r)

    # ---------------- memory: m / M / X / faults -----------------------
    r = c.cmd("m%x,12" % MOCK_BASE)  # 18 bytes (RSP lengths are hex!)
    check("m ELF header (18 bytes)",
          r == b"7f454c46" b"02010100" b"0000000000000000" b"0300", r)
    r = c.cmd("m%x,8" % (MOCK_BASE + 0x20))
    check("m u64 magic LE", r == b"efbeaddebebafeca", r)
    r = c.cmd("m%x,8" % (MOCK_BASE + 0x40))
    check("m banner 'PS5 UE4S'", r == b"5053352055453453", r)  # P S 5 ' ' U E 4 S
    r = c.cmd("m%x,4" % MOCK_BASE)  # short read
    check("m 4 bytes", r == b"7f454c46", r)
    r = c.cmd("x%x,4" % MOCK_BASE)  # LLDB binary read packet
    check("x binary read returns raw bytes", r == b"\x7fELF", r)
    r = c.cmd("m10,4")  # unmapped
    check("m unmapped -> E01", r == b"E01", r)
    r = c.cmd("m%x,0" % MOCK_BASE)  # zero-length -> empty
    check("m zero length -> empty", r == b"", r)

    r = c.cmd("M%x,8:deadbeefdeadbeef" % (MOCK_BASE + 0x1000))
    check("M write -> OK", r == b"OK", r)
    r = c.cmd("m%x,8" % (MOCK_BASE + 0x1000))
    check("M readback", r == b"deadbeefdeadbeef", r)

    # M with RLE: 32 zero bytes = 64 '0' hex chars, RLE-encoded as the
    # run "0*\" ('*' marker, 0x5C=92 -> 1 + (92-29) = 64 chars total).
    # Sent raw (no escaping) so the '*' stays an RSE marker on the wire.
    rle = b"M%x,20:0*\\" % (MOCK_BASE + 0x1100)
    c.send_packet(rle, escape=False)
    r = c.recv_packet()
    check("M with RLE zero-run -> OK", r == b"OK", r)
    r = c.cmd("m%x,20" % (MOCK_BASE + 0x1100))
    check("RLE write decoded to 32 zero bytes", r == b"00" * 32, r[:20])

    # X binary write (server must accept raw binary incl. escapes)
    payload = bytes([0xDE, 0xAD, 0xBE, 0xEF, 0x24, 0x23, 0x7D, 0x2A])
    c.send_packet(b"X%x,8:" % (MOCK_BASE + 0x1200) + payload)
    r = c.recv_packet()
    check("X binary write -> OK", r == b"OK", r)
    r = c.cmd("m%x,8" % (MOCK_BASE + 0x1200))
    check("X readback incl. escaped chars", r == b"deadbeef24237d2a", r)

    # ---------------- breakpoints --------------------------------------
    r = c.cmd("Z0,%x,1" % (MOCK_BASE + 0x100))
    check("Z0 set -> OK", r == b"OK", r)
    r = c.cmd("m%x,1" % (MOCK_BASE + 0x100))
    check("Z0 patches int3 0xCC", r == b"cc", r)
    r = c.cmd("z0,%x,1" % (MOCK_BASE + 0x100))
    check("z0 remove -> OK", r == b"OK", r)
    r = c.cmd("m%x,1" % (MOCK_BASE + 0x100))
    check("z0 restores original 0x55", r == b"55", r)
    c.send_packet(b"Z1,%x,1" % (MOCK_BASE + 0x100))
    r = c.recv_packet()
    check("Z1 hardware bp -> unsupported (empty)", r == b"", r)

    # ---------------- execution control (phase-1 semantics) ------------
    r = c.cmd("c")
    check("c -> immediate stop reply T05", r.startswith(b"T05"), r)
    r = c.cmd("s")
    check("s -> immediate stop reply T05", r.startswith(b"T05"), r)
    r = c.cmd("vCont?")
    check("vCont? -> vCont;c;s", r == b"vCont;c;s", r)
    r = c.cmd("vCont;c")
    check("vCont;c -> T05", r.startswith(b"T05"), r)

    # ---------------- threads / attach / monitor ------------------------
    r = c.cmd("Hg0")
    check("Hg0 -> OK", r == b"OK", r)
    r = c.cmd("qfThreadInfo")
    check("qfThreadInfo -> m1", r == b"m1", r)
    r = c.cmd("qsThreadInfo")
    check("qsThreadInfo -> l", r == b"l", r)
    r = c.cmd("qC")
    check("qC default -> QC1", r == b"QC1", r)

    r = c.cmd("vAttach;3039")
    check("vAttach;3039 -> stop reply", r.startswith(b"T05"), r)
    r = c.cmd("qC")
    check("qC after attach -> QC3039", r == b"QC3039", r)

    def monitor(cmdstr):
        hexcmd = cmdstr.encode().hex()
        outs = b""
        c.send_packet(("qRcmd,%s" % hexcmd).encode())
        while True:
            p = c.recv_packet()
            if p == b"OK" or p.startswith(b"E"):
                return outs, p
            if p.startswith(b"O"):          # console output (not "OK"!)
                outs += bytes.fromhex(p[1:].decode())
            else:
                return outs, p

    text, r = monitor("info")
    check("monitor info -> OK", r == b"OK", r)
    check("monitor info reports pid 3039",
          b"pid=12345 (0x3039)" in text, text)
    text, r = monitor("help")
    check("monitor help lists pid command", b"pid <n>" in text, text)

    # ---------------- wire robustness -----------------------------------
    # bad checksum in NO-ACK mode: no retransmit mechanism exists, the
    # corrupt packet must be dropped silently; the next packet works.
    c.s.sendall(b"$m%x,4#99" % MOCK_BASE)  # wrong checksum
    c.s.settimeout(0.4)
    dropped = False
    try:
        c.s.recv(1)
    except socket.timeout:
        dropped = True
    c.s.settimeout(5)
    check("bad checksum in no-ack mode dropped silently", dropped)
    r = c.cmd("m%x,4" % MOCK_BASE)
    check("next packet after drop works", r == b"7f454c46", r)

    # stray ack bytes between packets must be ignored
    c.s.sendall(b"+")
    r = c.cmd("m%x,4" % MOCK_BASE)
    check("stray '+' ignored", r == b"7f454c46", r)

    # async interrupt byte (Ctrl-C equivalent) -> stop reply
    c.s.sendall(b"\x03")
    r = c.recv_packet()
    check("raw 0x03 interrupt -> T02", r.startswith(b"T02"), r)

    # unsupported packet -> empty reply
    r = c.cmd("qTotallyMadeUp")
    check("unknown q packet -> empty (unsupported)", r == b"", r)

    # detach ends the session cleanly
    r = c.cmd("D")
    check("D detach -> OK", r == b"OK", r)

    c.close()
    print("\n%d passed, %d failed" % (PASS, FAIL))
    if FAILURES:
        print("failed checks:", ", ".join(FAILURES))
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
