"""Reject ambient host capabilities from native strategy implementations."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Violation:
    path: Path
    line: int
    capability: str


FORBIDDEN_INCLUDES = {
    "arpa/inet.h": "network",
    "boost/asio.hpp": "network",
    "chrono": "host-clock",
    "curl/curl.h": "network",
    "cstdlib": "environment-or-allocation",
    "filesystem": "filesystem",
    "fstream": "filesystem",
    "future": "host-scheduling",
    "netdb.h": "network",
    "netinet/in.h": "network",
    "random": "nondeterministic-randomness",
    "sys/socket.h": "network",
    "thread": "host-scheduling",
}

FORBIDDEN_TOKENS = {
    r"\bstd::chrono\b": "host-clock",
    r"\bstd::filesystem\b": "filesystem",
    r"\bstd::(?:i|o|f)fstream\b": "filesystem",
    r"\b(?:std::)?getenv\s*\(": "environment-or-secret",
    r"\bstd::random_device\b": "nondeterministic-randomness",
    r"\bstd::thread\b": "host-scheduling",
    r"\b(?:socket|connect|send|recv)\s*\(": "network",
    r"\bcurl_[a-zA-Z0-9_]+\s*\(": "network",
    r"\b(?:malloc|calloc|realloc|free)\s*\(": "unbounded-allocation",
    r"\b(?:new|delete)\b": "unbounded-allocation",
    r"\bstd::(?:cout|cerr|clog)\b": "telemetry-or-process-output",
}

INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
TOKEN_PATTERNS = [(re.compile(pattern), name) for pattern, name in FORBIDDEN_TOKENS.items()]


def find_violations(paths: list[Path]) -> list[Violation]:
    violations: list[Violation] = []
    for path in paths:
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            include = INCLUDE_PATTERN.match(line)
            if include:
                included = include.group(1)
                if included in FORBIDDEN_INCLUDES:
                    violations.append(Violation(path, line_number, FORBIDDEN_INCLUDES[included]))
                if included.startswith("chronos/core/") or included.endswith("/strategy_host.hpp"):
                    violations.append(Violation(path, line_number, "undeclared-host-or-core"))
            for pattern, capability in TOKEN_PATTERNS:
                if pattern.search(line):
                    violations.append(Violation(path, line_number, capability))
    return violations


def default_strategy_sources(root: Path) -> list[Path]:
    source_root = root / "strategies"
    return sorted(
        path
        for path in source_root.rglob("*")
        if path.suffix in {".cpp", ".hpp"} and "sdk" not in path.parts
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    paths = args.paths or default_strategy_sources(args.root)
    violations = find_violations(paths)
    for violation in violations:
        print(f"{violation.path}:{violation.line}: forbidden {violation.capability} capability")
    if violations:
        print(f"[FAIL] {len(violations)} forbidden strategy capability reference(s)")
        return 1
    print(f"[OK] strategy capability boundary checked ({len(paths)} source files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
