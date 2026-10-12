#!/usr/bin/env python3
"""Static validation for embedded and Linux integration metadata."""

from __future__ import annotations

from pathlib import Path
import sys


REQUIRED_FRAGMENTS = {
    "firmware/west.yml": (
        'version: "1.2"',
        "revision: v4.4.0",
        "name-allowlist:",
        "- hal_ti",
    ),
    "tools/zephyr-stage-manifest/west.yml": (
        "revision: v4.4.0",
        "west-commands: scripts/west-commands.yml",
        "revision: 512cc7e895e8491696b61f7ba8066b4a182569b8",
    ),
    "firmware/zephyr/sample.yaml": ("platform_allow:", "qemu_cortex_m3"),
    "firmware/zephyr/tests/core/testcase.yaml": ("tests:", "qemu_cortex_m3"),
    "yocto/kas.yml": (
        "machine: qemux86-64",
        "commit: scarthgap",
        'IMAGE_INSTALL:append = " rtsafe-supervisor"',
    ),
    "yocto/meta-rtsafe/conf/layer.conf": ('LAYERSERIES_COMPAT_rtsafe = "scarthgap"',),
    "yocto/meta-rtsafe/recipes-rtsafe/rtsafe-supervisor/rtsafe-supervisor_1.0.bb": (
        'LICENSE = "CLOSED"',
        "inherit cmake externalsrc systemd",
        'SYSTEMD_AUTO_ENABLE:${PN} = "enable"',
    ),
    "yocto/meta-rtsafe/recipes-rtsafe/rtsafe-supervisor/files/rtsafe-validate.service": (
        "Type=oneshot",
        "ExecStart=/usr/bin/rtsafe-supervisor",
    ),
    "yocto/meta-rtsafe/recipes-rtsafe/rtsafe-supervisor/files/rtsafe-validate.path": (
        "PathChanged=/var/lib/rtsafe/telemetry.bin",
        "WantedBy=multi-user.target",
    ),
}


def main() -> int:
    root = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    failures: list[str] = []
    for relative, fragments in REQUIRED_FRAGMENTS.items():
        path = root / relative
        if not path.is_file():
            failures.append(f"missing file: {relative}")
            continue
        text = path.read_text(encoding="utf-8")
        for fragment in fragments:
            if fragment not in text:
                failures.append(f"missing metadata in {relative}: {fragment}")

    if failures:
        print("Metadata validation failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print("Metadata validation passed: Zephyr and Yocto pins/services are consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
