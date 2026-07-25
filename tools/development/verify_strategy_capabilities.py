"""Reject ambient host capabilities from native strategy implementations."""

from __future__ import annotations

import argparse
import hashlib
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
    "cstdio": "process-output-or-filesystem",
    "curl/curl.h": "network",
    "cstdlib": "environment-or-allocation",
    "filesystem": "filesystem",
    "fstream": "filesystem",
    "future": "host-scheduling",
    "list": "unbounded-allocation",
    "map": "unbounded-allocation",
    "memory": "unbounded-allocation",
    "netdb.h": "network",
    "netinet/in.h": "network",
    "dlfcn.h": "dynamic-loading",
    "pthread.h": "host-scheduling",
    "random": "nondeterministic-randomness",
    "set": "unbounded-allocation",
    "string": "unbounded-allocation",
    "sys/syscall.h": "host-syscall",
    "sys/socket.h": "network",
    "thread": "host-scheduling",
    "unistd.h": "host-syscall",
    "unordered_map": "unbounded-allocation",
    "unordered_set": "unbounded-allocation",
    "vector": "unbounded-allocation",
}

FORBIDDEN_TOKENS = {
    r"\bstd::chrono\b": "host-clock",
    r"\bstd::filesystem\b": "filesystem",
    r"\bstd::(?:i|o|f)fstream\b": "filesystem",
    r"\b(?:std::)?getenv\s*\(": "environment-or-secret",
    r"\bstd::random_device\b": "nondeterministic-randomness",
    r"\bstd::thread\b": "host-scheduling",
    r"\b(?:socket|connect|send|recv)\s*\(": "network",
    r"\b(?:sendto|recvfrom|getaddrinfo)\s*\(": "network",
    r"\bcurl_[a-zA-Z0-9_]+\s*\(": "network",
    r"\b(?:malloc|calloc|realloc|free)\s*\(": "unbounded-allocation",
    r"\b(?:new|delete)\b": "unbounded-allocation",
    r"\boperator\s+(?:new|delete)\b": "unbounded-allocation",
    r"\bstd::(?:vector|list|map|set|unordered_map|unordered_set|string)\b": "unbounded-allocation",
    r"\bstd::(?:cout|cerr|clog)\b": "telemetry-or-process-output",
    r"\b(?:printf|fprintf|puts|fputs)\s*\(": "telemetry-or-process-output",
    r"\b(?:sleep|usleep|nanosleep)\s*\(": "host-scheduling",
    r"\bpthread_[a-zA-Z0-9_]+\s*\(": "host-scheduling",
    r"\b(?:dlopen|dlsym|dlclose)\s*\(": "dynamic-loading",
    r"\b(?:open|read|write|syscall)\s*\(": "host-syscall",
    r"\benviron\b": "environment-or-secret",
}

INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]')
TOKEN_PATTERNS = [(re.compile(pattern), name) for pattern, name in FORBIDDEN_TOKENS.items()]
CMAKE_ADD_LIBRARY_PATTERN = re.compile(r"\badd_library\s*\(\s*([^\s)]+)")
CMAKE_LINK_PATTERN = re.compile(r"\btarget_link_libraries\s*\(\s*([^\s)]+)")
REGISTERED_PACK_PATTERN = re.compile(
    r"\s*chronos_add_strategy\s*\(\s*[A-Za-z_][A-Za-z0-9_]*\s+"
    r"DEFINITION\s+\$\{CMAKE_CURRENT_SOURCE_DIR\}/definition\.json\s*\)\s*"
)
SDK_LIBRARY_PATTERN = re.compile(
    r"add_library\s*\(\s*chronos_strategy_sdk\s+STATIC\s+"
    r"sdk/src/strategy\.cpp\s+sdk/src/strategy_host\.cpp\s*\)"
)
SDK_TARGET_SOURCES_PATTERN = re.compile(r"\btarget_sources\s*\(\s*chronos_strategy_sdk\b")
TRUSTED_SDK_SOURCES = {
    Path("strategies/sdk/include/chronos/strategies/sdk/strategy.hpp"),
    Path("strategies/sdk/include/chronos/strategies/sdk/strategy_host.hpp"),
    Path("strategies/sdk/src/strategy.cpp"),
    Path("strategies/sdk/src/strategy_host.cpp"),
}
TRUSTED_ROOT_CMAKE_SHA256 = "10671f41a3917ae6ad87272064be171cb83b4328d39f56153a5ca2842d6a4ffe"
CPP_SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".ipp"}


def find_violations(paths: list[Path]) -> list[Violation]:
    violations: list[Violation] = []
    for path in paths:
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            include = INCLUDE_PATTERN.match(line)
            if include:
                included = include.group(1)
                if included in FORBIDDEN_INCLUDES:
                    violations.append(Violation(path, line_number, FORBIDDEN_INCLUDES[included]))
                if (
                    included.startswith("chronos/core/")
                    or included.startswith("chronos/runtime/")
                    or included.endswith("/strategy_host.hpp")
                ):
                    violations.append(Violation(path, line_number, "undeclared-host-or-core"))
            for pattern, capability in TOKEN_PATTERNS:
                if pattern.search(line):
                    violations.append(Violation(path, line_number, capability))
    return violations


def find_authored_strategy_code(paths: list[Path]) -> list[Violation]:
    return [Violation(path, 1, "authored-strategy-code") for path in paths]


def default_strategy_sources(root: Path) -> list[Path]:
    source_root = root / "strategies"
    return sorted(
        path
        for path in source_root.rglob("*")
        if path.suffix.lower() in CPP_SOURCE_SUFFIXES
        and path.relative_to(root) not in TRUSTED_SDK_SOURCES
    )


def trusted_sdk_sources(root: Path) -> list[Path]:
    return sorted(root / path for path in TRUSTED_SDK_SOURCES)


def allowed_trusted_host_include(violation: Violation) -> bool:
    if violation.capability != "undeclared-host-or-core":
        return False
    line = violation.path.read_text(encoding="utf-8").splitlines()[violation.line - 1].strip()
    return line in {
        '#include "chronos/core/features/feature_runtime.hpp"',
        '#include "chronos/strategies/sdk/strategy_host.hpp"',
    }


def find_cmake_violations(root: Path) -> list[Violation]:
    violations: list[Violation] = []
    for path in sorted((root / "strategies").rglob("CMakeLists.txt")):
        if path.parent == root / "strategies":
            source = path.read_text(encoding="utf-8")
            digest = hashlib.sha256(source.encode("utf-8")).hexdigest()
            if (
                digest != TRUSTED_ROOT_CMAKE_SHA256
                or not SDK_LIBRARY_PATTERN.search(source)
                or SDK_TARGET_SOURCES_PATTERN.search(source)
            ):
                violations.append(Violation(path, 1, "untrusted-sdk-target-shape"))
        if path.parent != root / "strategies" and not REGISTERED_PACK_PATTERN.fullmatch(
            path.read_text(encoding="utf-8")
        ):
            violations.append(Violation(path, 1, "unregistered-strategy-build-command"))
        for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            library = CMAKE_ADD_LIBRARY_PATTERN.search(line)
            if library and library.group(1) != "chronos_strategy_sdk":
                violations.append(Violation(path, line_number, "unregistered-strategy-target"))
            linked = CMAKE_LINK_PATTERN.search(line)
            if linked and linked.group(1) != "chronos_strategy_sdk":
                violations.append(Violation(path, line_number, "unregistered-strategy-link"))
    return violations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="*", type=Path)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    args = parser.parse_args()
    paths = args.paths or default_strategy_sources(args.root)
    trusted = trusted_sdk_sources(args.root) if not args.paths else []
    missing_trusted = [path for path in trusted if not path.is_file()]
    trusted_violations = [
        violation
        for violation in find_violations([path for path in trusted if path.is_file()])
        if not allowed_trusted_host_include(violation)
    ]
    violations = (
        find_authored_strategy_code(paths)
        + [Violation(path, 1, "missing-trusted-sdk-source") for path in missing_trusted]
        + trusted_violations
        + find_cmake_violations(args.root)
    )
    for violation in violations:
        print(f"{violation.path}:{violation.line}: forbidden {violation.capability} capability")
    if violations:
        print(f"[FAIL] {len(violations)} forbidden strategy capability reference(s)")
        return 1
    print(f"[OK] strategy capability boundary checked ({len(paths)} source files)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
