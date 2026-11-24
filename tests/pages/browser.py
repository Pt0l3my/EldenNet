# SPDX-License-Identifier: MIT
"""tests/pages/browser.py: a browser of our own for EldenNet's tests of its pages: the page load's session cookie, a
ticket, and a WebSocket to GraceUI's server (grace-serve, in graceui.dll in game or in grace-dev), sending what a
browser sends (GraceUI DESIGN §6.3 and §7.2: Host, Origin, Fetch Metadata, the cookie, masked frames). Loopback only.
Standard library only; it prints neither the cookie nor the ticket.

    view = Browser(port)            # GET / for the cookie, POST /api/ticket, GET /api/ws, hello, welcome
    view.sub("eldennet", "status")  # then view.next(...) for its frames

A paired device at home (GraceUI's G9, D221 to D223), on the test build's listener at 127.0.0.2 alone:

    cookie = pair(port, code)                                # POST /api/pair with the sheet's code: the device cookie
    phone = Browser(port, address="127.0.0.2", cookie=cookie)
"""
import base64
import http.client
import json
import os
import socket
import struct
import time


class BrowserError(Exception):
    pass


def pair(port, code, name="Invented test tablet", address="127.0.0.2", timeout=5.0):
    """Pairs a device with the code GraceUI's pairing sheet shows in game: its device cookie ("en_device=..."), or a
    BrowserError. Only on the test build's home listener, never a real address."""
    host = "%s:%d" % (address, port)
    post = http.client.HTTPConnection(address, port, timeout=timeout)
    post.request("POST", "/api/pair", body=json.dumps({"code": code, "name": name}), headers={
        "Host": host, "Origin": "http://" + host, "Content-Type": "application/json", "Sec-Fetch-Site": "same-origin",
        "Sec-Fetch-Mode": "cors"})
    reply = post.getresponse()
    reply.read()
    post.close()
    cookie = (reply.getheader("Set-Cookie") or "").split(";", 1)[0]
    if reply.status != 200 or not cookie.startswith("en_device="):
        raise BrowserError("the pairing gave no device cookie (status %d)" % reply.status)
    return cookie


class Browser:
    def __init__(self, port, timeout=5.0, address="127.0.0.1", cookie=None):
        self.port = port
        self.address = address
        self.host = "%s:%d" % (address, port)
        self.timeout = timeout
        self.cookie = cookie if cookie is not None else self._cookie()
        self.ticket = self._ticket()
        self.sock = None
        self.buffer = b""
        self.received = []  # every text frame, as it came: a case checks that none holds what it must not
        self.latest = {}    # (space, topic): the newest state body seen
        self.welcome = self._join()
        self.next_id = 1

    # ---- HTTP: the page's cookie and a ticket, as a browser fetches them.

    def _cookie(self):
        page = http.client.HTTPConnection(self.address, self.port, timeout=self.timeout)
        page.request("GET", "/", headers={"Host": self.host, "Sec-Fetch-Site": "none", "Sec-Fetch-Mode": "navigate",
                                          "Sec-Fetch-Dest": "document"})
        reply = page.getresponse()
        reply.read()
        page.close()
        cookie = (reply.getheader("Set-Cookie") or "").split(";", 1)[0]
        if reply.status != 200 or not cookie.startswith("en_session="):
            raise BrowserError("the page gave no session cookie (status %d)" % reply.status)
        return cookie

    def _ticket(self):
        post = http.client.HTTPConnection(self.address, self.port, timeout=self.timeout)
        post.request("POST", "/api/ticket", body="{}", headers={
            "Host": self.host, "Origin": "http://" + self.host, "Content-Type": "application/json",
            "Cookie": self.cookie, "Sec-Fetch-Site": "same-origin", "Sec-Fetch-Mode": "cors"})
        reply = post.getresponse()
        body = reply.read().decode("utf-8", "replace")
        post.close()
        if reply.status != 200:
            raise BrowserError("no ticket (status %d)" % reply.status)
        return json.loads(body)["ticket"]

    # ---- The WebSocket (RFC 6455): a client's frames are masked, a server's are not.

    def _join(self):
        self.sock = socket.create_connection((self.address, self.port), timeout=self.timeout)
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        head = ("GET /api/ws HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive, Upgrade\r\nUpgrade: websocket\r\n"
                "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: %s\r\nSec-Fetch-Site: same-origin\r\n"
                "Sec-Fetch-Mode: websocket\r\nOrigin: http://%s\r\nCookie: %s\r\n\r\n") % (self.host, key, self.host,
                                                                                         self.cookie)
        self.sock.sendall(head.encode("ascii"))
        deadline = time.monotonic() + self.timeout
        while b"\r\n\r\n" not in self.buffer:
            if time.monotonic() > deadline:
                raise BrowserError("no answer to the WebSocket's upgrade")
            chunk = self.sock.recv(4096)
            if not chunk:
                raise BrowserError("the server closed the upgrade")
            self.buffer += chunk
        head_end = self.buffer.index(b"\r\n\r\n")
        status = self.buffer[:head_end].split(b"\r\n", 1)[0]
        self.buffer = self.buffer[head_end + 4:]
        if b" 101 " not in status + b" ":
            raise BrowserError("the upgrade was refused: %s" % status.decode("ascii", "replace"))
        self.send({"v": 1, "t": "hello", "protocol": "1.1", "bundle": "eldennet-tests", "caps": [],
                   "ticket": self.ticket})
        welcome = self.next(lambda f: f.get("t") in ("welcome", "bye", "error"))
        if welcome.get("t") != "welcome":
            raise BrowserError("no welcome: %s" % welcome.get("t"))
        return welcome

    def send(self, message):
        payload = json.dumps(message, separators=(",", ":")).encode("utf-8")
        head = bytes([0x81])
        n = len(payload)
        if n < 126:
            head += bytes([0x80 | n])
        elif n <= 0xFFFF:
            head += bytes([0x80 | 126]) + struct.pack(">H", n)
        else:
            head += bytes([0x80 | 127]) + struct.pack(">Q", n)
        mask = os.urandom(4)
        self.sock.sendall(head + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))

    def _read(self, n, deadline):
        while len(self.buffer) < n:
            self._fill(deadline)
        out, self.buffer = self.buffer[:n], self.buffer[n:]
        return out

    def _fill(self, deadline):
        left = deadline - time.monotonic()
        if left <= 0:
            raise BrowserError("timed out waiting for a frame")
        self.sock.settimeout(left)
        try:
            chunk = self.sock.recv(65536)
        except socket.timeout as e:
            raise BrowserError("timed out waiting for a frame") from e
        if not chunk:
            raise BrowserError("the server closed the connection")
        self.buffer += chunk

    def _take(self):
        """A whole frame from the buffer (its opcode and payload), or None while it has not all come: a time-out
        between two reads never leaves half a frame taken."""
        b = self.buffer
        if len(b) < 2:
            return None
        n, at = b[1] & 0x7F, 2
        if n == 126:
            if len(b) < 4:
                return None
            n, at = struct.unpack(">H", b[2:4])[0], 4
        elif n == 127:
            if len(b) < 10:
                return None
            n, at = struct.unpack(">Q", b[2:10])[0], 10
        if len(b) < at + n:
            return None
        self.buffer = b[at + n:]
        return b[0] & 0x0F, b[at:at + n]

    def frame(self, timeout=None):
        """The next text frame, as JSON; a ping is answered on the way."""
        deadline = time.monotonic() + (timeout or self.timeout)
        while True:
            taken = self._take()
            if taken is None:
                self._fill(deadline)
                continue
            opcode, payload = taken
            if opcode == 0x9:  # ping
                mask = os.urandom(4)
                self.sock.sendall(bytes([0x8A, 0x80 | len(payload)]) + mask +
                                  bytes(b ^ mask[i % 4] for i, b in enumerate(payload)))
                continue
            if opcode == 0x8:
                raise BrowserError("the server closed the WebSocket")
            if opcode == 0x1:
                text = payload.decode("utf-8")
                self.received.append(text)
                f = json.loads(text)
                if f.get("t") == "state":
                    self.latest[(f.get("space"), f.get("topic"))] = f.get("body")
                if f.get("t") == "ping":  # the protocol's liveness, every 15 s: two unanswered close the socket
                    self.send({"v": 1, "t": "pong"})
                return f

    def next(self, want, timeout=None):
        """The next frame `want` accepts, passing over the others (a ping's pong, another topic's state)."""
        deadline = time.monotonic() + (timeout or self.timeout)
        while True:
            left = deadline - time.monotonic()
            if left <= 0:
                raise BrowserError("no frame of the kind wanted came")
            f = self.frame(left)
            if want(f):
                return f

    # ---- The protocol's page side (GraceUI DESIGN §6.3).

    def sub(self, space, topic):
        self.send({"v": 1, "t": "sub", "space": space, "id": self.next_id, "topic": topic})
        self.next_id += 1

    def unsub(self, space, topic):
        self.send({"v": 1, "t": "unsub", "space": space, "id": self.next_id, "topic": topic})
        self.next_id += 1

    def state(self, space, topic, timeout=None):
        return self.next(lambda f: f.get("t") in ("state", "rows") and f.get("space") == space and
                         f.get("topic") == topic, timeout)

    def command(self, space, name, args, timeout=None):
        cid = self.next_id
        self.next_id += 1
        self.send({"v": 1, "t": "cmd", "space": space, "id": cid, "name": name, "args": args})
        return self.next(lambda f: f.get("t") == "result" and f.get("id") == cid, timeout)

    def wait(self, space, topic, want, timeout=None):
        """The newest state body of a watched topic that `want` accepts: the one seen already, or the next to come."""
        deadline = time.monotonic() + (timeout or self.timeout)
        while True:
            body = self.latest.get((space, topic))
            if body is not None and want(body):
                return body
            left = deadline - time.monotonic()
            if left <= 0:
                raise BrowserError("%s never came as wanted: %r" % (topic, body))
            try:
                self.frame(min(left, 1.0))
            except BrowserError as e:
                if "timed out" not in str(e):
                    raise

    def close(self):
        if self.sock is not None:
            try:
                self.sock.close()
            finally:
                self.sock = None
