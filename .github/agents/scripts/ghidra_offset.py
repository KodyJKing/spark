"""
ghidra_offset.py — Look up module-relative offsets for functions or addresses.

Usage:
    python ghidra_offset.py <name_or_address> [<name_or_address> ...]

Examples:
    python ghidra_offset.py damageEntity02
    python ghidra_offset.py damageEntity01 damageEntity02
    python ghidra_offset.py 0x7fff495dea28

Requires the Ghidra HTTP server to be running at http://127.0.0.1:8080/.
"""

import sys
import requests

GHIDRA = "http://127.0.0.1:8080/"


def get_base() -> int:
    resp = requests.get(GHIDRA + "segments", params={"limit": 10}, timeout=5)
    resp.raise_for_status()
    for line in resp.text.splitlines():
        if line.startswith("Headers:"):
            return int(line.split()[1], 16)
    raise RuntimeError("Could not find Headers segment to determine image base.")


def resolve_va(query: str) -> tuple[str, int]:
    """Return (label, VA). query can be a function name or a hex address."""
    if query.startswith("0x") or query.startswith("0X"):
        va = int(query, 16)
        return query, va

    resp = requests.get(GHIDRA + "searchFunctions", params={"query": query, "limit": 1}, timeout=5)
    resp.raise_for_status()
    lines = resp.text.strip().splitlines()
    if not lines or lines[0].startswith("Error"):
        raise ValueError(f"Function not found: {query!r}  (response: {lines})")

    # Expected format: "funcName @ 7fff495dea28"
    parts = lines[0].split("@")
    label = parts[0].strip()
    va = int(parts[1].strip(), 16)
    return label, va


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    base = get_base()
    print(f"Image base: 0x{base:X}\n")

    for query in sys.argv[1:]:
        try:
            label, va = resolve_va(query)
            offset = va - base
            print(f"{label}")
            print(f"  VA:     0x{va:X}")
            print(f"  Offset: 0x{offset:X}")
        except Exception as e:
            print(f"{query}: ERROR — {e}")


if __name__ == "__main__":
    main()
