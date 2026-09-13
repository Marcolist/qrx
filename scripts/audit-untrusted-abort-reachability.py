#!/usr/bin/env python3
"""
QRX 0.0.9 Genesis hardening.

Static reachability audit: can untrusted network input reach a process-fatal
call (die/exit/abort/assert) in the node daemon?

Entry points are the functions that consume data straight off an accepted
socket. The checker walks the static call graph of qrx.c (plus the .inc
translation units it textually includes) and reports every path from an
ingress entry point to a process-fatal sink.

Calls that are protected by the untrusted-input fault barrier are treated as
non-fatal, because die() longjmps to the guard frame instead of exiting while
the guard is active on that thread.

Exit code 0 = no unguarded fatal path reachable from untrusted input.
Exit code 1 = at least one reachable fatal path (release blocker).
"""

import os
import re
import sys
from collections import deque

# Functions that read attacker-controlled bytes off a socket.
INGRESS_ENTRY_POINTS = [
    "node_handle_client",
    "generals_relay_store",
]

# Functions whose whole subtree runs inside the fault barrier.
GUARDED_ROOTS = [
    "verify_tx_text_untrusted",
]

FATAL_SINKS = ("die", "exit", "abort", "assert", "_exit", "__assert_fail")

C_KEYWORDS = {
    "if", "for", "while", "switch", "return", "sizeof", "do", "else",
    "case", "break", "continue", "goto", "default", "struct", "union",
    "enum", "typedef", "static", "const", "void", "int", "char", "long",
    "unsigned", "signed", "float", "double", "va_start", "va_end", "va_arg",
    "defined", "offsetof", "alignof", "_Thread_local", "setjmp", "longjmp",
}

FUNC_DEF = re.compile(
    r"^[A-Za-z_][A-Za-z0-9_ \t\*]*?\b([a-z_][A-Za-z0-9_]*)\s*\([^;]*?\)\s*\{",
    re.M,
)
CALL = re.compile(r"\b([a-z_][A-Za-z0-9_]*)\s*\(")


def strip_comments_and_strings(text):
    """Remove comments and string/char literals so they cannot fake calls."""
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif c == "/" and nxt == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j
            out.append(" ")
        elif c in ('"', "'"):
            q = c
            i += 1
            while i < n:
                if text[i] == "\\":
                    i += 2
                    continue
                if text[i] == q:
                    i += 1
                    break
                i += 1
            out.append('""')
        else:
            out.append(c)
            i += 1
    return "".join(out)


def extract_functions(text):
    """Return {name: body} using brace matching from each definition."""
    funcs = {}
    for m in FUNC_DEF.finditer(text):
        name = m.group(1)
        if name in C_KEYWORDS:
            continue
        start = text.find("{", m.start())
        if start < 0:
            continue
        depth, i, n = 0, start, len(text)
        while i < n:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            i += 1
        body = text[start : i + 1]
        # Keep the largest definition if a name appears more than once.
        if name not in funcs or len(body) > len(funcs[name]):
            funcs[name] = body
    return funcs


def load_sources(src_dir):
    text_parts = []
    main_c = os.path.join(src_dir, "qrx.c")
    with open(main_c, encoding="utf-8", errors="replace") as fh:
        text_parts.append(fh.read())
    # qrx.c textually includes .inc translation units; they share the call graph.
    for root, _dirs, files in os.walk(src_dir):
        for fn in sorted(files):
            if fn.endswith(".inc"):
                with open(os.path.join(root, fn), encoding="utf-8", errors="replace") as fh:
                    text_parts.append(fh.read())
    return "\n".join(text_parts)


def main():
    src_dir = sys.argv[1] if len(sys.argv) > 1 else "qrx-core/src"
    if not os.path.isdir(src_dir):
        print("ERROR: source directory not found: %s" % src_dir)
        return 2

    raw = load_sources(src_dir)
    text = strip_comments_and_strings(raw)
    funcs = extract_functions(text)

    callees = {}
    for name, body in funcs.items():
        inner = body[body.find("{") + 1 :]
        found = set()
        for cm in CALL.finditer(inner):
            c = cm.group(1)
            if c in C_KEYWORDS or c == name:
                continue
            found.add(c)
        callees[name] = found

    guarded = set()
    for root in GUARDED_ROOTS:
        if root not in funcs:
            continue
        dq = deque([root])
        while dq:
            cur = dq.popleft()
            if cur in guarded:
                continue
            guarded.add(cur)
            for c in callees.get(cur, ()):  # noqa: B007
                if c not in guarded:
                    dq.append(c)

    print("=" * 74)
    print("QRX UNTRUSTED-INPUT ABORT REACHABILITY AUDIT")
    print("=" * 74)
    print("parsed functions        : %d" % len(funcs))
    print("guarded subtree size    : %d" % len(guarded))
    print("ingress entry points    : %s" % ", ".join(INGRESS_ENTRY_POINTS))
    print("-" * 74)

    violations = []
    for entry in INGRESS_ENTRY_POINTS:
        if entry not in funcs:
            print("WARN: ingress entry point not found in source: %s" % entry)
            continue
        # BFS keeping the path, never descending into the guarded subtree.
        seen = {entry}
        dq = deque([(entry, [entry])])
        while dq:
            cur, path = dq.popleft()
            for c in sorted(callees.get(cur, ())):
                if c in FATAL_SINKS:
                    violations.append((entry, path + [c]))
                    continue
                if c in guarded or c in seen or c not in funcs:
                    continue
                seen.add(c)
                dq.append((c, path + [c]))

    if not violations:
        print("RESULT: PASS")
        print("No unguarded die()/exit()/abort()/assert() reachable from")
        print("untrusted network ingress.")
        print("=" * 74)
        return 0

    # Group by the sink-owning function to keep the report readable.
    by_owner = {}
    for entry, path in violations:
        owner = path[-2]
        by_owner.setdefault(owner, []).append((entry, path))

    print("RESULT: FAIL - %d process-fatal path(s) reachable" % len(violations))
    print("-" * 74)
    for owner in sorted(by_owner):
        entry, path = by_owner[owner][0]
        print("  %s" % owner)
        print("     via: %s" % " -> ".join(path))
    print("=" * 74)
    print("Each entry is a remote node-crash candidate and blocks Mainnet.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
