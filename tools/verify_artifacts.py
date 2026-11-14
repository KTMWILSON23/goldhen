#!/usr/bin/env python3
"""Verify the rainbow payload and write reproducible artifact hashes."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prx", required=True, type=Path)
    parser.add_argument("--payload", required=True, type=Path)
    parser.add_argument("--map", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--sha-manifest", required=True, type=Path)
    args = parser.parse_args()

    prx = args.prx.read_bytes()
    payload = args.payload.read_bytes()
    linker_map = args.map.read_bytes()
    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    prx_digest = hashlib.sha256(prx).hexdigest()
    payload_digest = hashlib.sha256(payload).hexdigest()
    map_digest = hashlib.sha256(linker_map).hexdigest()

    if manifest.get("component") != args.prx.name:
        raise SystemExit("manifest component does not match the built PRX")
    if manifest.get("size") != len(prx) or manifest.get("sha256") != prx_digest:
        raise SystemExit("manifest does not describe the built PRX")
    if not linker_map:
        raise SystemExit("linker map is empty")
    if len(payload) < len(prx):
        raise SystemExit("payload is smaller than its embedded PRX")
    occurrences = payload.count(prx)
    if occurrences != 1:
        raise SystemExit(
            f"expected one exact embedded PRX in payload, found {occurrences}"
        )

    manifest["artifacts"] = {
        args.prx.name: {"sha256": prx_digest, "size": len(prx)},
        args.payload.name: {"sha256": payload_digest, "size": len(payload)},
        args.map.name: {"sha256": map_digest, "size": len(linker_map)},
    }
    args.manifest.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )

    hash_lines = [
        f"{prx_digest}  {args.prx.as_posix()}",
        f"{payload_digest}  {args.payload.as_posix()}",
        f"{map_digest}  {args.map.as_posix()}",
        f"{hashlib.sha256(args.manifest.read_bytes()).hexdigest()}  "
        f"{args.manifest.as_posix()}",
    ]
    args.sha_manifest.write_text(
        "\n".join(hash_lines) + "\n", encoding="ascii", newline="\n"
    )

    print(
        f"verified payload={len(payload)} bytes sha256={payload_digest}; "
        f"embedded_prx={len(prx)} bytes sha256={prx_digest}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
