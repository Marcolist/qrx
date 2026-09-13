#!/usr/bin/env python3
"""Run an AURA origin test against a loopback HTTPS fixture origin.

The production downloader remains HTTPS-only. Tests exercise the same libcurl
TLS path with a dedicated, test-only CA certificate rather than re-enabling
file:// support in production code.
"""
from __future__ import annotations
import argparse
import http.server
import os
from pathlib import Path
import shutil
import socket
import ssl
import subprocess
import sys
import tempfile
import threading

class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, fmt, *args):
        if os.environ.get("QRX_TEST_HTTPS_VERBOSE"):
            super().log_message(fmt, *args)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cert", required=True)
    ap.add_argument("--key", required=True)
    ap.add_argument("command", nargs=argparse.REMAINDER)
    ns = ap.parse_args()
    cmd = ns.command
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    if not cmd:
        ap.error("missing child command")

    root = Path(tempfile.mkdtemp(prefix="qrx-aura-https-"))
    try:
        handler = lambda *a, **kw: QuietHandler(*a, directory=str(root), **kw)
        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.minimum_version = ssl.TLSVersion.TLSv1_2
        ctx.load_cert_chain(ns.cert, ns.key)
        server.socket = ctx.wrap_socket(server.socket, server_side=True)
        port = server.server_address[1]
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()

        env = os.environ.copy()
        env["QRX_TEST_HTTPS_ROOT"] = str(root)
        env["QRX_TEST_HTTPS_API"] = f"https://127.0.0.1:{port}/api"
        env["QRX_TEST_HTTPS_BASE"] = f"https://127.0.0.1:{port}/hf"
        env["QRX_TEST_HTTPS_CA"] = str(Path(ns.cert).resolve())
        env["QRX_TEST_HTTPS_ACTIVE"] = "1"
        proc = subprocess.run(cmd, env=env)
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)
        return proc.returncode
    finally:
        shutil.rmtree(root, ignore_errors=True)

if __name__ == "__main__":
    raise SystemExit(main())
