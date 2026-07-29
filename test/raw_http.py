#!/usr/bin/env python3
"""Send one exact HTTP request from stdin and write the raw response to stdout."""

import socket
import sys


def main() -> int:
    if len(sys.argv) not in (2, 3):
        print(f"usage: {sys.argv[0]} PORT [TIMEOUT_SECONDS]", file=sys.stderr)
        return 2

    port = int(sys.argv[1])
    timeout = float(sys.argv[2]) if len(sys.argv) == 3 else 5.0
    request = sys.stdin.buffer.read()

    response = bytearray()
    with socket.create_connection(("127.0.0.1", port), timeout=timeout) as sock:
        sock.settimeout(timeout)
        sock.sendall(request)
        sock.shutdown(socket.SHUT_WR)
        while True:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                break
            if not chunk:
                break
            response.extend(chunk)

    sys.stdout.buffer.write(response)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
