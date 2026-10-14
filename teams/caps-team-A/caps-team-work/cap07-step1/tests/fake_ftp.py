"""Tiny in-process FTP server over a local folder, for host tests only.

Mimics the console server's observed shape: anonymous login, PASV, `ls -l`
style LIST rows ("-rwxrwxrwx 1 0 0 <size> Oct 01 16:54 <name>"), and DELE
answered with "226 File deleted" (the ftpsrv 0.21.1 reply the 2026-10-01
cleanup recorded).  Paths are absolute on the "console" and mapped under
`root` (so "/data/x" -> root/data/x).  Records every command for assertions.
"""
import os
import socket
import socketserver
import threading


class _Handler(socketserver.StreamRequestHandler):
    def setup(self):
        super().setup()
        self.pasv = None
        self.rnfr = None

    def reply(self, line):
        self.wfile.write((line + "\r\n").encode("utf-8"))
        self.wfile.flush()

    def path(self, arg):
        assert arg.startswith("/"), arg
        return os.path.join(self.server.root, arg.lstrip("/"))

    def data_conn(self):
        conn, _ = self.pasv.accept()
        self.pasv.close()
        self.pasv = None
        return conn

    def handle(self):
        self.reply("220 fake ftp ready")
        while True:
            raw = self.rfile.readline()
            if not raw:
                return
            line = raw.decode("utf-8").rstrip("\r\n")
            verb, _, arg = line.partition(" ")
            verb = verb.upper()
            self.server.commands.append((verb, arg))
            if verb == "USER":
                self.reply("331 ok")
            elif verb == "PASS":
                self.reply("230 logged in")
            elif verb == "TYPE":
                self.reply("200 type set")
            elif verb == "PASV":
                s = socket.socket()
                s.bind(("127.0.0.1", 0))
                s.listen(1)
                self.pasv = s
                port = s.getsockname()[1]
                self.reply("227 Entering Passive Mode (127,0,0,1,%d,%d)" % (port >> 8, port & 255))
            elif verb == "LIST":
                p = self.path(arg)
                if not os.path.isdir(p):
                    if self.pasv:
                        self.pasv.close()
                        self.pasv = None
                    self.reply("550 no such directory")
                    continue
                self.reply("150 listing")
                conn = self.data_conn()
                rows = []
                for n in sorted(os.listdir(p)):
                    q = os.path.join(p, n)
                    kind = "d" if os.path.isdir(q) else "-"
                    size = 0 if kind == "d" else os.path.getsize(q)
                    rows.append("%srwxrwxrwx 1 0 0 %d Oct 01 16:54 %s" % (kind, size, n))
                conn.sendall(("\r\n".join(rows) + ("\r\n" if rows else "")).encode("utf-8"))
                conn.close()
                self.reply("226 Transfer completed")
            elif verb == "RETR":
                p = self.path(arg)
                if not os.path.isfile(p):
                    if self.pasv:
                        self.pasv.close()
                        self.pasv = None
                    self.reply("550 no such file")
                    continue
                self.reply("150 sending")
                conn = self.data_conn()
                with open(p, "rb") as f:
                    while True:
                        b = f.read(65536)
                        if not b:
                            break
                        conn.sendall(b)
                conn.close()
                self.reply("226 Transfer completed")
            elif verb == "STOR":
                p = self.path(arg)
                self.reply("150 receiving")
                conn = self.data_conn()
                with open(p, "wb") as f:
                    while True:
                        b = conn.recv(65536)
                        if not b:
                            break
                        f.write(b)
                conn.close()
                self.reply("226 Transfer completed")
            elif verb == "RNFR":
                p = self.path(arg)
                if os.path.exists(p):
                    self.rnfr = p
                    self.reply("350 ready")
                else:
                    self.reply("550 no such file")
            elif verb == "RNTO":
                if self.rnfr is None:
                    self.reply("503 bad sequence")
                else:
                    os.rename(self.rnfr, self.path(arg))
                    self.rnfr = None
                    self.reply("250 renamed")
            elif verb == "DELE":
                p = self.path(arg)
                if os.path.isfile(p):
                    os.remove(p)
                    self.reply("226 File deleted")
                else:
                    self.reply("550 no such file")
            elif verb == "MKD":
                os.mkdir(self.path(arg))
                self.reply('257 "%s" created' % arg)
            elif verb == "QUIT":
                self.reply("221 Goodbye")
                return
            else:
                self.reply("502 not implemented")


class FakeFTP:
    def __init__(self, root):
        self.server = socketserver.ThreadingTCPServer(("127.0.0.1", 0), _Handler)
        self.server.daemon_threads = True
        self.server.root = root
        self.server.commands = []
        self.port = self.server.server_address[1]
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    @property
    def commands(self):
        return self.server.commands

    def stop(self):
        self.server.shutdown()
        self.server.server_close()
