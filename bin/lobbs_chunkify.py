#!/usr/bin/env python3
"""Turn a local file into LoBBS /upload and /commit commands (base62 chunks)."""

from __future__ import annotations

import argparse
import os
import sys
import zlib

ALPHABET = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
BYTE_TO_WIDTH = {1: 2, 2: 3, 3: 5, 4: 6, 5: 7, 6: 9, 7: 10, 8: 11}


def b62_encode_fixed(val: int, width: int) -> str:
    chars: list[str] = []
    for _ in range(width):
        val, rem = divmod(val, 62)
        chars.append(ALPHABET[rem])
    if val != 0:
        raise ValueError("value too large for width")
    return "".join(reversed(chars))


def encode_chunk(data: bytes) -> str:
    k = len(data)
    if k < 1 or k > 8:
        raise ValueError("chunk length must be 1..8")
    width = BYTE_TO_WIDTH[k]
    val = int.from_bytes(data, "big")
    return b62_encode_fixed(val, width)


def consume_payload(data: bytes, payload: str) -> int:
    """Return how many bytes from data the payload encodes."""
    pos = 0
    p = 0
    while p < len(payload):
        matched = False
        for k in (8, 7, 6, 5, 4, 3, 2, 1):
            if pos + k > len(data):
                continue
            w = BYTE_TO_WIDTH[k]
            if p + w > len(payload):
                continue
            seg = payload[p : p + w]
            if encode_chunk(data[pos : pos + k]) != seg:
                continue
            pos += k
            p += w
            matched = True
            break
        if not matched:
            raise ValueError(f"payload does not match data at byte {pos}")
    return pos


def pack_one_line(data: bytes, max_payload_chars: int) -> tuple[str, int]:
    payload = ""
    pos = 0
    while pos < len(data):
        take = min(8, len(data) - pos)
        piece = encode_chunk(data[pos : pos + take])
        if payload and len(payload) + len(piece) > max_payload_chars:
            break
        if not payload and len(piece) > max_payload_chars:
            raise ValueError("single chunk exceeds line budget")
        payload += piece
        pos += take
    if not payload:
        raise ValueError("empty line")
    return payload, pos


def main() -> int:
    parser = argparse.ArgumentParser(
        description="LoBBS chunked upload command generator"
    )
    parser.add_argument("local_path", help="Local file to upload")
    parser.add_argument(
        "dest_path", help="Destination on the node (e.g. /flash/files/foo.txt)"
    )
    parser.add_argument(
        "--max-line", type=int, default=200, help="Max characters per output line"
    )
    parser.add_argument(
        "--tmp-dir", default="/flash/tmp", help="Temp directory on the node"
    )
    args = parser.parse_args()

    with open(args.local_path, "rb") as f:
        blob = f.read()

    crc = zlib.crc32(blob) & 0xFFFFFFFF
    crc_hex = f"{crc:08x}"
    base = os.path.basename(args.local_path)
    tmp_dir = args.tmp_dir.rstrip("/")
    tmp_path = f"{tmp_dir}/{base}.{crc_hex}.tmp"

    print(f"/mkdir {tmp_dir}")

    offset = 0
    while offset < len(blob):
        prefix = f"/upload {tmp_path} {offset}:"
        budget = args.max_line - len(prefix)
        if budget < 2:
            print("line budget too small for one chunk", file=sys.stderr)
            return 1
        payload, nbytes = pack_one_line(blob[offset:], budget)
        consume_payload(blob[offset:], payload)
        print(f"{prefix}{payload}")
        offset += nbytes

    print(f"/commit {tmp_path} {args.dest_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
