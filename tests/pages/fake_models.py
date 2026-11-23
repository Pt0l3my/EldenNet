#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""tests/pages/fake_models.py: a fake model server of the tests' own, for the AI pages' self-test and model list.

An OpenAI-compatible server on 127.0.0.1, at a port of its own, in a thread of the caller's process: GET
<prefix>/v1/models lists invented models, and POST <prefix>/v1/chat/completions streams an invented reply as server-sent
events (a short line, or for a request with `response_format` the two-field object the JSON check asks for), each only
for a request whose Authorization is the key it was made with: anything else is answered 401. Under `open_prefix`, if
given, it is a server that takes no key instead (a local server's), listing `open_models` to any request and counted
apart ("open_models", "open_chats"). `scripted`, if given, is the replies a conversation's chats get in turn (each a
list of the pieces streamed, invented words of the tests' own), the default line once they are spent ("scripted"
counts them); a request's `stop` sequences are counted ("stops"). It holds the key in memory alone and prints nothing;
it counts what it answered (`counts`), and never keeps a request's text.

    server = fake_models.FakeModels(key, ["invented-model-a", "invented-model-b"],
                                    open_prefix="/open/", open_models=["invented-model-c"],
                                    scripted=[["Evening. ", "The lamp burns low."]])
    ... server.port, server.counts ...
    server.close()

Standard library only.
"""
import http.server
import json
import threading
import time


class FakeModels:
    def __init__(self, key, models, first_s=0.15, gap_s=0.03, open_prefix=None, open_models=(), scripted=()):
        self.key = key
        self.models = list(models)
        self.first_s = first_s
        self.gap_s = gap_s
        self.open_prefix = open_prefix
        self.open_models = list(open_models)
        self.scripted = [list(r) for r in scripted]
        self.lock = threading.Lock()
        self.counts = {"models": 0, "chats": 0, "json": 0, "refused": 0, "unknown": 0, "open_models": 0,
                       "open_chats": 0, "scripted": 0, "stops": 0}
        fake = self

        class Handler(http.server.BaseHTTPRequestHandler):
            protocol_version = "HTTP/1.1"

            def log_message(self, *args):  # nothing printed: a request's headers hold the key
                pass

            def count(self, what):
                with fake.lock:
                    fake.counts[what] += 1

            def open(self):
                return fake.open_prefix is not None and self.path.startswith(fake.open_prefix)

            def authorised(self):
                return self.open() or self.headers.get("Authorization", "") == "Bearer " + fake.key

            def answer(self, status, body):
                data = json.dumps(body).encode("utf-8")
                self.send_response(status)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(data)))
                self.end_headers()
                self.wfile.write(data)

            def refuse(self):
                self.count("refused")
                self.answer(401, {"error": {"message": "invented refusal: no such key", "type": "auth"}})

            def do_GET(self):
                if not self.path.endswith("/v1/models"):
                    self.count("unknown")
                    self.answer(404, {"error": {"message": "invented: no such path"}})
                    return
                if not self.authorised():
                    self.refuse()
                    return
                self.count("open_models" if self.open() else "models")
                listed = fake.open_models if self.open() else fake.models
                self.answer(200, {"object": "list", "data": [{"id": m, "object": "model"} for m in listed]})

            def do_POST(self):
                length = int(self.headers.get("Content-Length", "0") or 0)
                raw = self.rfile.read(length) if length > 0 else b""
                if not self.path.endswith("/v1/chat/completions"):
                    self.count("unknown")
                    self.answer(404, {"error": {"message": "invented: no such path"}})
                    return
                if not self.authorised():
                    self.refuse()
                    return
                try:
                    body = json.loads(raw.decode("utf-8"))
                except ValueError:
                    self.answer(400, {"error": {"message": "invented: not JSON"}})
                    return
                wants_json = "response_format" in body
                self.count("open_chats" if self.open() else "json" if wants_json else "chats")
                if isinstance(body.get("stop"), list) and body["stop"]:
                    self.count("stops")
                pieces = None
                if not wants_json:
                    with fake.lock:
                        if fake.scripted:
                            pieces = fake.scripted.pop(0)
                            fake.counts["scripted"] += 1
                if pieces is None:
                    pieces = (['{"place": "harbour", ', '"count": 4}'] if wants_json else
                              ["A lantern ", "burns low ", "by the road ", "at dusk, ", "and waits."])
                model = body.get("model", "")
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Transfer-Encoding", "chunked")
                self.end_headers()
                time.sleep(fake.first_s)
                for i, piece in enumerate(pieces):
                    if i:
                        time.sleep(fake.gap_s)
                    self.chunk({"id": "fake", "object": "chat.completion.chunk", "model": model,
                                "choices": [{"index": 0, "delta": {"content": piece}}]})
                self.chunk({"id": "fake", "object": "chat.completion.chunk", "model": model,
                            "choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}],
                            "usage": {"prompt_tokens": 24, "completion_tokens": 12, "total_tokens": 36}})
                self.write_chunk(b"data: [DONE]\n\n")
                self.wfile.write(b"0\r\n\r\n")
                self.wfile.flush()

            def chunk(self, event):
                self.write_chunk(("data: " + json.dumps(event) + "\n\n").encode("utf-8"))

            def write_chunk(self, data):
                self.wfile.write(b"%X\r\n" % len(data) + data + b"\r\n")
                self.wfile.flush()

        self.server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.server.daemon_threads = True
        self.port = self.server.server_address[1]
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def snapshot(self):
        with self.lock:
            return dict(self.counts)

    def close(self):
        self.server.shutdown()
        self.server.server_close()
