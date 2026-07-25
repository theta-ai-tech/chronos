"""Reject forbidden ambient-capability references from a strategy archive."""

from __future__ import annotations

import argparse
import re
import subprocess
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class SymbolViolation:
    symbol: str
    capability: str


FORBIDDEN_SYMBOLS = {
    r"\bgetenv\b": "environment-or-secret",
    r"\b(?:clock_gettime|gettimeofday|time)\b": "host-clock",
    r"system_clock::now": "host-clock",
    r"std::__[^ ]*filesystem": "filesystem",
    r"\b(?:socket|connect|send|recv|getaddrinfo)\b": "network",
    r"\b(?:sendto|recvfrom)\b": "network",
    r"\bcurl_[A-Za-z0-9_]+\b": "network",
    r"random_device": "nondeterministic-randomness",
    r"std::(?:basic_)?(?:i|o|f)fstream": "filesystem",
    r"\b(?:fopen|freopen)\b": "filesystem",
    r"std::(?:cout|cerr|clog)": "telemetry-or-process-output",
    r"\b(?:printf|fprintf|puts|fputs)\b": "telemetry-or-process-output",
    r"\b(?:sleep|usleep|nanosleep)\b": "host-scheduling",
    r"\b(?:open|read|write|syscall)\b": "host-syscall",
    r"\benviron\b": "environment-or-secret",
    r"operator (?:new|delete)": "unbounded-allocation",
}

SYMBOL_PATTERNS = [(re.compile(pattern), name) for pattern, name in FORBIDDEN_SYMBOLS.items()]


def find_symbol_violations(output: str) -> list[SymbolViolation]:
    violations: list[SymbolViolation] = []
    for line in output.splitlines():
        for pattern, capability in SYMBOL_PATTERNS:
            if pattern.search(line):
                violations.append(SymbolViolation(line.strip(), capability))
    return violations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    completed = subprocess.run(
        ["nm", "-u", "-C", str(args.archive)],
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        print(completed.stdout, end="")
        print(completed.stderr, end="")
        return completed.returncode
    violations = find_symbol_violations(completed.stdout)
    for violation in violations:
        print(f"{args.archive}: forbidden {violation.capability}: {violation.symbol}")
    if violations:
        print(f"[FAIL] {len(violations)} forbidden strategy symbol reference(s)")
        return 1
    print(f"[OK] linked strategy capability boundary checked: {args.archive}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
