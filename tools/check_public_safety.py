#!/usr/bin/env python3
"""Fail when source candidates contain secrets, identity markers, or oversized files."""

from __future__ import annotations

import re
from pathlib import Path
import sys


EXCLUDED_PARTS = {
    ".git",
    ".tools",
    ".deps",
    ".west",
    ".venv",
    "build",
    "outputs",
    "downloads",
    "sstate-cache",
    "__pycache__",
}
MAX_PUBLIC_FILE_BYTES = 2 * 1024 * 1024

# Construct deny-list values so the scanner does not flag its own source text.
DENIED_IDENTIFIERS = {
    chr(0x5EE3) + chr(0x9054),
    "".join(chr(value) for value in (81, 117, 97, 110, 116, 97)),
    "".join(chr(value) for value in (57, 49, 105, 56, 112)),
    "".join(chr(value) for value in (49, 48, 52, 46, 99, 111, 109, 46, 116, 119, 47, 106, 111, 98)),
}

SECRET_PATTERNS = {
    "private key": re.compile(r"BEGIN\s+(?:RSA\s+|EC\s+|OPENSSH\s+)?PRIVATE\s+KEY"),
    "cloud access key": re.compile(r"AKIA[0-9A-Z]{16}"),
    "source-hosting token": re.compile(r"gh[pousr]_[A-Za-z0-9]{20,}"),
    "assigned API credential": re.compile(
        r"(?i)(?:api[_-]?key|client[_-]?secret|access[_-]?token)\s*[:=]\s*['\"][^'\"]{12,}['\"]"
    ),
    "absolute user profile path": re.compile(r"(?i)[A-Z]:\\Users\\[^\\\s]+"),
}


def candidates(root: Path):
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        relative = path.relative_to(root)
        if any(part in EXCLUDED_PARTS or part.startswith("build-") for part in relative.parts):
            continue
        yield path, relative


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    findings: list[str] = []

    for path, relative in candidates(root):
        size = path.stat().st_size
        if size > MAX_PUBLIC_FILE_BYTES:
            findings.append(f"oversized file ({size} bytes): {relative}")
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            findings.append(f"unexpected binary file: {relative}")
            continue

        for identifier in DENIED_IDENTIFIERS:
            if identifier.casefold() in text.casefold():
                findings.append(f"prohibited identity marker in {relative}")
        for label, pattern in SECRET_PATTERNS.items():
            if pattern.search(text):
                findings.append(f"possible {label} in {relative}")

    if findings:
        print("Public-safety scan failed:")
        for finding in findings:
            print(f"- {finding}")
        return 1

    print("Public-safety scan passed: no identity markers, secrets, private paths, or oversized files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

