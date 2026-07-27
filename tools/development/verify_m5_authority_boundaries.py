from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

LOCAL_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
SYSTEM_INCLUDE = re.compile(r"^\s*#\s*include\s+<([^>]+)>", re.MULTILINE)

ALLOWED_INCLUDES = {
    Path("core/features"): (
        "chronos/core/features/",
        "chronos/contracts/digest.hpp",
        "chronos/contracts/fixed_point.hpp",
        "chronos/contracts/state_lineage.hpp",
        "chronos/core/market_state/listing_view_publisher.hpp",
    ),
    Path("runtime/strategies"): (
        "chronos/runtime/strategies/",
        "chronos/contracts/recommendation_policy.hpp",
        "chronos/core/dispatch/run_input_dispatcher.hpp",
        "chronos/strategies/sdk/strategy_host.hpp",
    ),
    Path("core/recommendation"): (
        "chronos/core/recommendation/",
        "chronos/contracts/recommendation_policy.hpp",
        "chronos/runtime/strategies/strategy_evaluation.hpp",
    ),
}

ALLOWED_SYSTEM_INCLUDES = {
    Path("core/features"): {
        "algorithm",
        "cstdint",
        "limits",
        "memory",
        "optional",
        "string_view",
        "type_traits",
        "variant",
        "vector",
    },
    Path("runtime/strategies"): {
        "algorithm",
        "array",
        "cstddef",
        "cstdint",
        "optional",
        "span",
        "string_view",
        "type_traits",
        "utility",
        "variant",
        "vector",
    },
    Path("core/recommendation"): {
        "algorithm",
        "cstddef",
        "cstdint",
        "optional",
        "span",
        "string_view",
        "type_traits",
        "utility",
        "variant",
        "vector",
    },
}

AUTHORITY_PATHS = {
    Path("core/features"): (
        Path("core/include/chronos/core/features"),
        Path("core/src/feature_runtime.cpp"),
    ),
    Path("runtime/strategies"): (Path("runtime/strategies"),),
    Path("core/recommendation"): (Path("core/recommendation"),),
}

CMAKE_PATHS = {
    Path("core/features"): Path("core/CMakeLists.txt"),
    Path("runtime/strategies"): Path("runtime/strategies/CMakeLists.txt"),
    Path("core/recommendation"): Path("core/recommendation/CMakeLists.txt"),
}

FORBIDDEN_CMAKE_TARGET_FRAGMENTS = (
    "accounting",
    "adapter",
    "execution",
    "portfolio",
    "risk",
)


@dataclass(frozen=True)
class BoundaryViolation:
    path: Path
    dependency: str


def find_violations(root: Path) -> list[BoundaryViolation]:
    violations: list[BoundaryViolation] = []
    for authority, allowed in ALLOWED_INCLUDES.items():
        source_paths: list[Path] = []
        for relative_path in AUTHORITY_PATHS[authority]:
            path = root / relative_path
            source_paths.extend(path.rglob("*") if path.is_dir() else (path,))
        for path in sorted(source_paths):
            if not path.is_file() or path.suffix not in {".cpp", ".hpp"}:
                continue
            for dependency in LOCAL_INCLUDE.findall(path.read_text(encoding="utf-8")):
                if not any(
                    dependency.startswith(prefix) if prefix.endswith("/") else dependency == prefix
                    for prefix in allowed
                ):
                    violations.append(BoundaryViolation(path, dependency))
            for dependency in SYSTEM_INCLUDE.findall(path.read_text(encoding="utf-8")):
                if dependency not in ALLOWED_SYSTEM_INCLUDES[authority]:
                    violations.append(BoundaryViolation(path, dependency))

        cmake_path = root / CMAKE_PATHS[authority]
        cmake = cmake_path.read_text(encoding="utf-8").lower()
        for fragment in FORBIDDEN_CMAKE_TARGET_FRAGMENTS:
            if fragment in cmake:
                violations.append(BoundaryViolation(cmake_path, fragment))
    return violations


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    violations = find_violations(root)
    if violations:
        for violation in violations:
            print(f"[FAIL] {violation.path}: forbidden M5 dependency {violation.dependency}")
        return 1
    print("[OK] M5 authorities have no downstream capability dependencies")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
