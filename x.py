#!/usr/bin/env python3
import argparse
import os
import re
import subprocess
import sys
import time

MSG_BYTES = 8192
BENCH_RUNS = 5
MAX_ATTEMPTS = 5
THROUGHPUT_RE = re.compile(
    r"wrote (\d+) bytes in ([0-9.]+)ms \(([0-9.]+) Kbps\)"
)


def compile_binaries() -> None:
    cflags = ["-O2", "-march=native"]
    subprocess.run(["gcc", *cflags, "send.c", "proto.c", "-o", "send"], check=True)
    subprocess.run(["gcc", *cflags, "recv.c", "proto.c", "-o", "recv"], check=True)


def pin_self() -> None:
    try:
        os.sched_setaffinity(0, {0})
    except (AttributeError, OSError):
        pass


def run_once(payload: bytes):
    recv = subprocess.Popen(
        ["taskset", "-c", "3", "./recv"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    time.sleep(0.001)
    send = subprocess.Popen(
        ["taskset", "-c", "7", "./send"],
        stdin=subprocess.PIPE,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
    )
    _, send_err = send.communicate(payload)
    recv_out, recv_err = recv.communicate()
    return send.returncode, recv.returncode, recv_out, send_err, recv_err


def transmit(payload: bytes):
    last = None
    for _ in range(MAX_ATTEMPTS):
        result = run_once(payload)
        send_rc, recv_rc, recv_out, _send_err, _recv_err = result
        last = result
        if send_rc == 0 and recv_rc == 0:
            return result
    return last


def bit_errors(expected: bytes, actual: bytes) -> tuple[int, int]:
    n = max(len(expected), len(actual))
    errors = 0
    for i in range(n):
        a = expected[i] if i < len(expected) else 0
        b = actual[i] if i < len(actual) else 0
        errors += (a ^ b).bit_count()
    return errors, n * 8


def run_bench() -> int:
    payload = b"x" * MSG_BYTES
    failures = 0

    for i in range(1, BENCH_RUNS + 1):
        send_rc, recv_rc, recv_out, send_err, recv_err = transmit(payload)
        errors, bits = bit_errors(payload, recv_out)
        if send_rc != 0 or recv_rc != 0:
            failures += 1

        send_text = send_err.decode("utf-8", errors="replace")
        if recv_err:
            sys.stderr.write(recv_err.decode("utf-8", errors="replace"))
        ber = errors / bits if bits else 0.0
        match = THROUGHPUT_RE.search(send_text)
        if match:
            print(f"[{i}] {match.group(0)}, ber={ber:.6%}")
        else:
            print(f"[{i}] send failed rc=({send_rc},{recv_rc}), ber={ber:.6%}")

    return 0 if failures == 0 else 1


def run_payload(payload: bytes, interactive: bool) -> int:
    send_rc, recv_rc, recv_out, send_err, recv_err = transmit(payload)
    if send_rc == 0 and recv_rc == 0:
        sys.stdout.buffer.write(recv_out)
        if interactive:
            sys.stdout.buffer.write(b"\n")
        sys.stdout.buffer.flush()
        sys.stderr.write(send_err.decode("utf-8", errors="replace"))
        return 0

    sys.stderr.write(send_err.decode("utf-8", errors="replace"))
    sys.stderr.write(recv_err.decode("utf-8", errors="replace"))
    print("recv failed after 5 attempts", file=sys.stderr)
    return 1


def run_stream() -> int:
    rc = 0
    if sys.stdin.isatty():
        while True:
            try:
                line = input("> ")
            except EOFError:
                break
            rc = run_payload(line.encode(), interactive=True)
            if rc != 0:
                break
    else:
        while True:
            payload = sys.stdin.buffer.read(MSG_BYTES)
            if not payload:
                break
            rc = run_payload(payload, interactive=False)
            if rc != 0:
                break
    return rc


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bench", action="store_true")
    args = parser.parse_args()

    pin_self()
    compile_binaries()

    if args.bench:
        return run_bench()
    return run_stream()


if __name__ == "__main__":
    raise SystemExit(main())
