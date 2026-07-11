"""Prove M0 CI gates fail for representative violations.

The normal CI path runs `make m0-check` against the real tree. This script copies
the repository to temporary directories, injects one focused violation per gate,
and expects the matching gate command to fail. If a violation accidentally passes,
the script fails.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

Mutation = Callable[[Path], None]


@dataclass(frozen=True)
class GateProof:
    name: str
    command: tuple[str, ...]
    mutate: Mutation
    expected_output: tuple[str, ...]


IGNORED_NAMES = {
    ".DS_Store",
    ".claude",
    ".git",
    ".pytest_cache",
    ".ruff_cache",
    ".venv",
    "__pycache__",
    "build",
    "dist",
    "journal.html",
}


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    path.write_text(content, encoding="utf-8")


def replace_once(path: Path, before: str, after: str) -> None:
    content = read_text(path)
    if before not in content:
        raise RuntimeError(f"expected text not found in {path}: {before!r}")
    write_text(path, content.replace(before, after, 1))


def inject_cpp_warning_as_error(repo: Path) -> None:
    replace_once(
        repo / "core/src/version.cpp",
        "bool engine_alive() noexcept { return true; }",
        "bool engine_alive() noexcept {\n  int unused_probe = 1;\n  return true;\n}",
    )


def inject_cpp_unit_failure(repo: Path) -> None:
    replace_once(
        repo / "tests/unit/version_test.cpp",
        "CHECK(chronos::engine_alive());",
        "CHECK(!chronos::engine_alive());",
    )


def inject_cpp_format_failure(repo: Path) -> None:
    replace_once(
        repo / "core/src/version.cpp",
        "bool engine_alive() noexcept { return true; }",
        "bool engine_alive() noexcept {return true;}",
    )


def inject_python_unit_failure(repo: Path) -> None:
    replace_once(
        repo / "tests/python/test_package.py",
        'assert version("chronos-engine") == chronos.__version__ == "0.0.1"',
        'assert version("chronos-engine") == chronos.__version__ == "9.9.9"',
    )


def inject_python_lint_failure(repo: Path) -> None:
    with (repo / "tools/development/run_m0_round_trip.py").open("a", encoding="utf-8") as handle:
        handle.write("\nundefined_lint_probe\n")


def inject_python_format_failure(repo: Path) -> None:
    with (repo / "tests/python/test_package.py").open("a", encoding="utf-8") as handle:
        handle.write("\nformat_probe={    'bad':1}\n")


GATE_PROOFS = (
    GateProof(
        "cpp-warning-as-error",
        ("make", "m0-check"),
        inject_cpp_warning_as_error,
        ("unused_probe", "unused-variable", "-Werror"),
    ),
    GateProof(
        "cpp-unit-test",
        ("make", "m0-check"),
        inject_cpp_unit_failure,
        ("CHECK failed: !chronos::engine_alive()", "RESULT FAIL"),
    ),
    GateProof(
        "cpp-format",
        ("make", "m0-check"),
        inject_cpp_format_failure,
        ("code should be clang-formatted", "core/src/version.cpp"),
    ),
    GateProof(
        "python-unit-test",
        ("make", "m0-check"),
        inject_python_unit_failure,
        ("test_python_package_exposes_project_version", "9.9.9", "AssertionError"),
    ),
    GateProof(
        "python-lint",
        ("make", "m0-check"),
        inject_python_lint_failure,
        ("F821", "undefined_lint_probe"),
    ),
    GateProof(
        "python-format",
        ("make", "m0-check"),
        inject_python_format_failure,
        ("Would reformat: tests/python/test_package.py",),
    ),
)


def ignore_names(_directory: str, names: list[str]) -> set[str]:
    ignored = {name for name in names if name in IGNORED_NAMES}
    ignored.update({name for name in names if name.startswith("build-")})
    ignored.update({name for name in names if name.endswith(".pyc")})
    return ignored


def copy_repo(source: Path, destination: Path) -> None:
    shutil.copytree(source, destination, ignore=ignore_names)


def run_expected_failure(repo: Path, proof: GateProof, verbose: bool) -> None:
    proof.mutate(repo)

    env = os.environ.copy()
    env.setdefault("UV_NO_PROGRESS", "1")
    env.pop("VIRTUAL_ENV", None)

    completed = subprocess.run(
        proof.command,
        cwd=repo,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )

    if completed.returncode == 0:
        print(f"[FAIL] {proof.name}: representative violation passed unexpectedly")
        print(completed.stdout)
        raise SystemExit(1)

    missing_output = [needle for needle in proof.expected_output if needle not in completed.stdout]
    if missing_output:
        print(f"[FAIL] {proof.name}: gate failed, but not with the expected signature")
        print(f"missing output: {missing_output}")
        print(completed.stdout)
        raise SystemExit(1)

    print(f"[OK] {proof.name}: gate failed as expected")
    if verbose:
        print(completed.stdout)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--verbose", action="store_true", help="print command output")
    parser.add_argument(
        "--proof",
        choices=[proof.name for proof in GATE_PROOFS],
        help="run one proof (used by the CI matrix)",
    )
    args = parser.parse_args()

    source = Path(__file__).resolve().parents[2]

    with tempfile.TemporaryDirectory(prefix="chronos-m0-gate-proof-") as tmp:
        root = Path(tmp)
        selected = [proof for proof in GATE_PROOFS if args.proof in (None, proof.name)]
        for proof in selected:
            scenario_repo = root / proof.name
            copy_repo(source, scenario_repo)
            run_expected_failure(scenario_repo, proof, args.verbose)

    print("[OK] all M0 gate-failure proofs passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
