"""Bootstrap, build, test, and run the current M0 scaffold."""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Step:
    name: str
    command: tuple[str, ...]


REQUIRED_TOOLS = (
    ("make", "Install POSIX make."),
    ("c++", "Install AppleClang 16.x or Clang 18.x."),
    ("cmake", "Install CMake 3.24 or newer."),
    ("ninja", "Install Ninja 1.11 or newer."),
    ("uv", "Install uv 0.11 or newer from https://docs.astral.sh/uv/."),
)

MINIMUM_VERSIONS = (
    ("cmake", ("cmake", "--version"), (3, 24, 0)),
    ("ninja", ("ninja", "--version"), (1, 11, 0)),
    ("uv", ("uv", "--version"), (0, 11, 0)),
)


BOOTSTRAP_STEPS = (
    Step("sync locked Python dev dependencies", ("uv", "sync", "--locked", "--group", "dev")),
    Step("run the canonical M0 build and quality gate", ("make", "m0-check")),
    Step("run M0.4 Python to C++ round trip", ("make", "m0-round-trip")),
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def check_required_tools() -> None:
    missing = [(tool, hint) for tool, hint in REQUIRED_TOOLS if shutil.which(tool) is None]
    if not missing:
        return

    print("Chronos M0 bootstrap cannot start because required tools are missing:", file=sys.stderr)
    for tool, hint in missing:
        print(f"- {tool}: {hint}", file=sys.stderr)
    raise SystemExit(1)


def parse_version(output: str) -> tuple[int, int, int]:
    match = re.search(r"(?<!\d)(\d+)\.(\d+)(?:\.(\d+))?", output)
    if match is None:
        raise ValueError(f"could not parse version from: {output.strip()!r}")
    return tuple(int(part or 0) for part in match.groups())


def check_tool_versions() -> None:
    failures = []
    for name, command, minimum in MINIMUM_VERSIONS:
        completed = subprocess.run(command, text=True, capture_output=True, check=True)
        actual = parse_version(completed.stdout or completed.stderr)
        if actual < minimum:
            failures.append(
                f"{name} {'.'.join(map(str, actual))} is too old; "
                f"need {'.'.join(map(str, minimum))} or newer"
            )

    if sys.version_info < (3, 9):  # noqa: UP036 - bootstrap runs before project install
        failures.append(
            f"Python {sys.version_info.major}.{sys.version_info.minor} is too old; "
            "need 3.9 or newer"
        )

    if failures:
        print("Chronos M0 bootstrap found unsupported tool versions:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        raise SystemExit(1)


def run_step(root: Path, step: Step) -> None:
    print(f"==> {step.name}", flush=True)
    subprocess.run(step.command, cwd=root, check=True)


def main() -> int:
    root = repo_root()
    check_required_tools()
    check_tool_versions()

    for step in BOOTSTRAP_STEPS:
        run_step(root, step)

    print("Chronos M0 bootstrap complete.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
