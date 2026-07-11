"""Validate the ownership metadata promised by the M0.1 module scaffold."""

from __future__ import annotations

from pathlib import Path

REQUIRED_FIELDS = (
    "Owner",
    "Plane",
    "Language",
    "Purpose",
    "Accepted dependencies",
    "Public API",
)


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    failures: list[str] = []
    owner_stubs = 0

    for stub in sorted(root.glob("*/*/README.md")):
        content = stub.read_text(encoding="utf-8")
        if "Submodule owner stub" not in content:
            continue
        owner_stubs += 1
        missing = [field for field in REQUIRED_FIELDS if f"**{field}:**" not in content]
        if missing:
            failures.append(f"{stub.relative_to(root)}: missing {', '.join(missing)}")

    if owner_stubs == 0:
        failures.append("no second-level owner stubs found")

    if failures:
        print("M0.1 module-stub validation failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print(f"M0.1 module-stub metadata is complete ({owner_stubs} stubs).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
