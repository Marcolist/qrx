#!/usr/bin/env python3
"""
QRX 0.0.9 Genesis - Phase 185 remote malformed-TX survival test.

MAINNET BLOCKER REGRESSION.

Starts a real qrx node ("node-run"), completes a genuine signed HELLO
handshake using a freshly generated attacker key, and then floods the node
with malformed and fuzzed transactions over the real P2P wire protocol.

The node must:
  * reject every malformed transaction,
  * stay alive after every single attack,
  * still answer a fresh handshake afterwards,
  * still accept a valid transaction afterwards.

Any process exit during the attack phase is a release blocker: it means
untrusted network input can still reach die()/exit()/abort().

Note on peer reputation: the node bans a source IP after a few invalid
transactions (peer_add_score +30, BAN_THRESHOLD 100) and rate-limits to
RATE_MAX_MSGS per RATE_WINDOW_SECS. A single loopback address would
therefore be banned long before the fuzz budget is exhausted. To emulate a
distributed attacker the test clears <node_dir>/peer_state.db between
batches. That only resets per-IP reputation; it does not weaken any
validation path under test.

usage: genesis_phase185_remote_malformed_tx_survival.py <qrx>
"""

import base64
import os
import pathlib
import random
import socket
import struct
import subprocess
import sys
import tempfile
import time

if len(sys.argv) != 2:
    raise SystemExit("usage: genesis_phase185_remote_malformed_tx_survival.py <qrx>")

QRX = pathlib.Path(sys.argv[1]).resolve()
if not QRX.exists():
    raise SystemExit("qrx executable missing")

ENV = os.environ.copy()
ENV["QRX_PASSPHRASE"] = "Phase185-Wallet-Passphrase!"

HOST = "127.0.0.1"
FUZZ_ROUNDS = 400
BATCH_RESET = 8          # clear peer reputation every N attacks
RECV_TIMEOUT = 5.0

random.seed(185)


# --------------------------------------------------------------------------
# helpers
# --------------------------------------------------------------------------

def run(*args, ok=True):
    p = subprocess.run([str(QRX), *map(str, args)], text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=ENV)
    if ok and p.returncode != 0:
        raise AssertionError("command failed: %s\nstdout=%s\nstderr=%s"
                             % (args, p.stdout, p.stderr))
    return p


def sh(cmd):
    return subprocess.check_output(["bash", "-lc", cmd], text=True).strip()


def free_port():
    s = socket.socket()
    s.bind((HOST, 0))
    port = s.getsockname()[1]
    s.close()
    return port


def send_frame(sock, payload: bytes):
    sock.sendall(struct.pack("!I", len(payload)) + payload)


def recv_frame(sock):
    hdr = b""
    while len(hdr) < 4:
        c = sock.recv(4 - len(hdr))
        if not c:
            return None
        hdr += c
    n = struct.unpack("!I", hdr)[0]
    if n > 1 << 20:
        return None
    buf = b""
    while len(buf) < n:
        c = sock.recv(n - len(buf))
        if not c:
            return None
        buf += c
    return buf.decode("utf-8", "replace")


# --------------------------------------------------------------------------
# attacker identity: any self-generated Ed25519 key passes HELLO
# --------------------------------------------------------------------------

class Attacker:
    def __init__(self, root):
        self.priv = root / "attacker_ed25519.pem"
        self.pub = root / "attacker_ed25519_pub.pem"
        sh("openssl genpkey -algorithm ed25519 -out '%s' 2>/dev/null" % self.priv)
        sh("openssl pkey -in '%s' -pubout -out '%s' 2>/dev/null" % (self.priv, self.pub))
        self.pub_hex = sh(
            "openssl pkey -pubin -in '%s' -outform DER 2>/dev/null "
            "| tail -c 32 | od -An -tx1 | tr -d ' \\n'" % self.pub)
        self.root = root

    def sign_hex(self, payload: str) -> str:
        msg = self.root / "hello_payload.bin"
        sig = self.root / "hello_payload.sig"
        msg.write_bytes(payload.encode())
        sh("openssl pkeyutl -sign -inkey '%s' -rawin -in '%s' -out '%s' 2>/dev/null"
           % (self.priv, msg, sig))
        return sh("od -An -tx1 '%s' | tr -d ' \\n'" % sig)


def hello_message(attacker, meta, port):
    ts = str(int(time.time()))
    nonce = "%016x" % random.getrandbits(64)
    payload = (
        "type=HELLO\n"
        "network_id=%s\ngenesis_hash=%s\nprotocol_version=%s\n"
        "consensus_version=%s\nchain_id=%s\nmagic=%s\n"
        "timestamp=%s\nnonce=%s\nhost=%s\nport=%s\ned25519_pub_hex=%s\n"
        % (meta["network_id"], meta["genesis_hash"], meta["protocol_version"],
           meta["consensus_version"], meta["chain_id"], meta["magic"],
           ts, nonce, HOST, str(port), attacker.pub_hex))
    return payload + "sig_ed25519_hex=%s\n" % attacker.sign_hex(payload)


# --------------------------------------------------------------------------
# malformed transaction corpus
# --------------------------------------------------------------------------

def malformed_corpus(meta, valid_tx):
    net, gen = meta["network_id"], meta["genesis_hash"]
    base = ("tx_version=6\nnetwork_id=%s\ngenesis_hash=%s\nprotocol_version=9\n"
            "from=qrx1test\nto=qrx1test\namount=1\nfee=1000\nnonce=1\n"
            "timestamp=%d\n" % (net, gen, int(time.time())))
    corpus = [
        ("empty tx", ""),
        ("single newline", "\n"),
        ("nul bytes", "\x00\x00\x00\x00"),
        ("no fields", "garbage-without-any-key-value"),
        ("truncated tx", valid_tx[:len(valid_tx) // 3]),
        ("truncated mid-field", valid_tx[:len(valid_tx) - 12]),
        ("wrong network_id", valid_tx.replace(net, "qrx-evil-network")),
        ("wrong genesis_hash", valid_tx.replace(gen, "00" * 64)),
        ("negative amount", base + "amount=-1\n"),
        ("amount overflow", base + "amount=99999999999999999999999999\n"),
        ("amount LLONG_MAX", base + "amount=9223372036854775807\nfee=9223372036854775807\n"),
        ("fee underflow", base + "fee=-9223372036854775808\n"),
        ("nonce zero", base + "nonce=0\n"),
        ("nonce non numeric", base + "nonce=not-a-number\n"),
        ("nonce overflow", base + "nonce=9999999999999999999999\n"),
        ("bad ed25519 pub", base + "ed25519_pub_hex=zzzz\n"),
        ("short ed25519 pub", base + "ed25519_pub_hex=deadbeef\n"),
        ("bad mldsa b64", base + "mldsa65_pub_b64=!!!!not-base64!!!!\n"),
        ("bad ed signature", valid_tx.replace("sig_ed25519_hex=", "sig_ed25519_hex=ff")),
        ("bad mldsa signature", valid_tx.replace("sig_mldsa65_hex=", "sig_mldsa65_hex=ff")),
        ("unknown tx_type", base + "tx_type=TOTALLY_UNKNOWN\nlane_id=1\nexpiry_height=9\npayload=x\n"),
        ("expired tx", base + "tx_type=TRANSFER_FAST\nlane_id=1\nexpiry_height=1\npayload=x\n"),
        ("bad lane", base + "tx_type=TRANSFER_FAST\nlane_id=-7\nexpiry_height=99\npayload=x\n"),
        ("broken storage tx", base + "tx_type=STORAGE_ASSIGN_ACCEPT\nlane_id=1\nexpiry_height=99\npayload=@@@\n"),
        ("broken advertising tx", base + "tx_type=AD_IMPRESSION\nlane_id=1\nexpiry_height=99\npayload=@@@\n"),
        ("broken compute tx", base + "tx_type=POUC_SETTLEMENT\nlane_id=1\nexpiry_height=99\npayload=@@@\n"),
        ("broken governance tx", base + "tx_type=GOVERNANCE_PROTOCOL\nlane_id=1\nexpiry_height=99\npayload=@@@\n"),
        ("broken privacy tx", base + "tx_type=PRIVACY_SHIELD\nlane_id=1\nexpiry_height=99\npayload=@@@\n"),
        ("privacy bad amount", base + "tx_type=PRIVACY_SHIELD\nlane_id=1\nexpiry_height=99\namount=xx\npayload=x\n"),
        ("huge payload", base + "tx_type=TRANSFER_FAST\nlane_id=1\nexpiry_height=99\npayload=" + "A" * 200000 + "\n"),
        ("deep key nesting", "a=" * 50000),
        ("replay of valid tx", valid_tx),
    ]
    return corpus


def fuzz_variants(valid_tx, count):
    out = []
    data = valid_tx
    for i in range(count):
        mode = i % 5
        b = bytearray(data.encode("utf-8", "replace"))
        if not b:
            continue
        if mode == 0:                                    # bit flips
            for _ in range(random.randint(1, 12)):
                p = random.randrange(len(b))
                b[p] ^= 1 << random.randrange(8)
        elif mode == 1:                                  # truncation
            b = b[:random.randrange(1, len(b))]
        elif mode == 2:                                  # byte injection
            for _ in range(random.randint(1, 20)):
                b.insert(random.randrange(len(b) + 1), random.randrange(256))
        elif mode == 3:                                  # field duplication
            lines = data.split("\n")
            random.shuffle(lines)
            b = bytearray("\n".join(lines[:max(1, len(lines) // 2)]).encode())
        else:                                            # random noise
            b = bytearray(bytes(random.randrange(256) for _ in range(random.randrange(1, 4096))))
        out.append(("fuzz#%d" % i, b.decode("utf-8", "replace")))
    return out


# --------------------------------------------------------------------------
# attack primitives
# --------------------------------------------------------------------------

def handshake(attacker, meta, port):
    s = socket.create_connection((HOST, port), timeout=RECV_TIMEOUT)
    s.settimeout(RECV_TIMEOUT)
    send_frame(s, hello_message(attacker, meta, port).encode())
    reply = recv_frame(s)
    return s, reply


def send_tx(attacker, meta, port, tx_text):
    """Returns the node reply, or None if the connection died."""
    try:
        s, reply = handshake(attacker, meta, port)
    except OSError:
        return None
    try:
        if reply is None or "status=OK" not in reply:
            return reply  # banned / rate limited / rejected handshake
        b64 = base64.b64encode(tx_text.encode("utf-8", "replace")).decode()
        send_frame(s, ("type=TX\ntx_b64=%s\n" % b64).encode())
        return recv_frame(s)
    except OSError:
        return None
    finally:
        try:
            s.close()
        except OSError:
            pass


def alive(proc):
    return proc.poll() is None


def reset_peer_reputation(node_dir):
    db = node_dir / "peer_state.db"
    try:
        if db.exists():
            db.unlink()
    except OSError:
        pass


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

def main():
    failures = []
    with tempfile.TemporaryDirectory(prefix="qrx-phase185-") as td:
        root = pathlib.Path(td)
        chain, wallet, node_dir = root / "chain", root / "wallet", root / "node"

        run("seed-new", wallet)
        addr = (wallet / "address.txt").read_text().strip()
        run("init-chain", chain, "20", "5000", "2100000000000000",
            "25000000", "1000000000000", "qrx-regtest", "9", "QRXP185", "Phase185")
        run("faucet", chain, addr, "1000000")

        port = free_port()
        run("node-init", node_dir, chain, wallet, HOST, str(port))

        meta = {}
        for line in (node_dir / "node.conf").read_text().splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                meta[k.strip()] = v.strip()
        for key in ("network_id", "genesis_hash", "protocol_version",
                    "consensus_version", "chain_id", "magic"):
            if key not in meta:
                raise SystemExit("node.conf missing %s" % key)

        # a genuinely valid transaction, used as fuzz seed and as the final
        # proof that the node is still fully functional after the attack
        ed = sh("openssl pkey -pubin -in '%s/ed25519_pub.pem' -outform DER 2>/dev/null "
                "| tail -c 32 | od -An -tx1 | tr -d ' \\n'" % wallet)
        ml = base64.b64encode((wallet / "mldsa65_pub.pem").read_bytes()).decode()
        raw = root / "valid.raw"
        signed = root / "valid.signed"
        p = run("create-velocity-raw-tx", chain, addr, addr, "1", ed, ml,
                "TRANSFER_FAST", "6", "5000", "x")
        raw.write_text(p.stdout)
        run("signrawtransactionwithwallet", wallet, chain, raw, signed)
        valid_tx = signed.read_text()

        proc = subprocess.Popen([str(QRX), "node-run", str(node_dir)], env=ENV,
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            # wait for the listener
            ready = False
            for _ in range(100):
                if not alive(proc):
                    break
                try:
                    socket.create_connection((HOST, port), timeout=0.5).close()
                    ready = True
                    break
                except OSError:
                    time.sleep(0.1)
            if not ready:
                out, err = proc.communicate(timeout=5)
                raise SystemExit("node did not start\nstdout=%s\nstderr=%s" % (out, err))

            attacker = Attacker(root)

            attacks = malformed_corpus(meta, valid_tx)
            attacks += fuzz_variants(valid_tx, FUZZ_ROUNDS)
            total = len(attacks)
            print("phase185: launching %d malformed transactions" % total)

            accepted_malformed = []
            for i, (name, tx) in enumerate(attacks):
                if i % BATCH_RESET == 0:
                    reset_peer_reputation(node_dir)

                reply = send_tx(attacker, meta, port, tx)

                if not alive(proc):
                    failures.append("NODE DIED after attack %d (%s) rc=%s"
                                    % (i, name, proc.returncode))
                    break

                # A malformed transaction must never be accepted.
                if reply and "kind=tx" in reply and "status=OK" in reply:
                    if name != "replay of valid tx":
                        accepted_malformed.append(name)

            if failures:
                for f in failures:
                    print("  [FAIL] %s" % f)
            else:
                print("  [PASS] node survived all %d malformed transactions" % total)

            if accepted_malformed:
                failures.append("malformed transactions accepted: %s"
                                % ", ".join(accepted_malformed[:5]))
                print("  [FAIL] malformed transactions accepted: %s"
                      % ", ".join(accepted_malformed[:5]))
            else:
                print("  [PASS] no malformed transaction was accepted")

            # node must still complete a fresh handshake
            if alive(proc):
                reset_peer_reputation(node_dir)
                try:
                    s, reply = handshake(attacker, meta, port)
                    s.close()
                    if reply and "status=OK" in reply:
                        print("  [PASS] node still answers a fresh handshake")
                    else:
                        failures.append("handshake after attack failed: %r" % reply)
                        print("  [FAIL] handshake after attack failed: %r" % reply)
                except OSError as exc:
                    failures.append("handshake after attack raised %s" % exc)
                    print("  [FAIL] handshake after attack raised %s" % exc)

                # and must still accept a genuinely valid transaction
                reset_peer_reputation(node_dir)
                reply = send_tx(attacker, meta, port, valid_tx)
                if reply and "status=OK" in reply:
                    print("  [PASS] valid transaction still accepted after attack")
                else:
                    failures.append("valid tx rejected after attack: %r" % reply)
                    print("  [FAIL] valid tx rejected after attack: %r" % reply)
            else:
                failures.append("node process is dead; liveness checks skipped")
        finally:
            if alive(proc):
                proc.terminate()
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    proc.kill()

    print("-" * 60)
    if failures:
        print("PHASE185 FAILED (%d):" % len(failures))
        for f in failures:
            print("  - %s" % f)
        return 1
    print("PHASE185 PASS - remote malformed-TX DoS not reproducible")
    return 0


if __name__ == "__main__":
    sys.exit(main())
