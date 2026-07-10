"""Bootstrap, build, test, and run the current M0 scaffold."""

from __future__ import annotations

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
    ("cmake", "Install CMake 3.24 or newer."),
    ("ninja", "Install Ninja or make it available on PATH."),
    ("uv", "Install uv from https://docs.astral.sh/uv/."),
)


BOOTSTRAP_STEPS = (
    Step("sync locked Python dev dependencies", ("uv", "sync", "--locked", "--group", "dev")),
    Step("configure C++ build", ("cmake", "-S", ".", "-B", "build", "-G", "Ninja")),
    Step("build C++ targets", ("cmake", "--build", "build")),
    Step("run C++ unit tests", ("ctest", "--test-dir", "build", "--output-on-failure")),
    Step("run Python unit tests", ("uv", "run", "--locked", "--group", "dev", "pytest")),
    Step("run Python lint", ("uv", "run", "--locked", "--group", "dev", "ruff", "check", ".")),
    Step(
        "run Python format check",
        ("uv", "run", "--locked", "--group", "dev", "ruff", "format", "--check", "."),
    ),
    Step(
        "run M0.4 Python to C++ round trip",
        (
            "uv",
            "run",
            "--locked",
            "--group",
            "dev",
            "python",
            "tools/development/run_m0_round_trip.py",
        ),
    ),
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


def run_step(root: Path, step: Step) -> None:
    print(f"==> {step.name}", flush=True)
    subprocess.run(step.command, cwd=root, check=True)


def main() -> int:
    root = repo_root()
    check_required_tools()

    for step in BOOTSTRAP_STEPS:
        run_step(root, step)

    print("Chronos M0 bootstrap complete.", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
