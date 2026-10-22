#!/usr/bin/env python3
"""Receive Vita DEBUGNET UDP output; run only on a trusted development LAN."""

import argparse
import ipaddress
import socket
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bind", type=ipaddress.IPv4Address, default="0.0.0.0",
                        help="local interface address (default: all IPv4 interfaces)")
    args = parser.parse_args()
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as connection:
            connection.bind((str(args.bind), 18194))
            print(f"Listening for Vita DEBUGNET on {args.bind}:18194/UDP; Ctrl+C stops.",
                  file=sys.stderr, flush=True)
            while True:
                data, _ = connection.recvfrom(65535)
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
    except KeyboardInterrupt:
        return 0
    except OSError as error:
        print(f"Vita log receiver failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
