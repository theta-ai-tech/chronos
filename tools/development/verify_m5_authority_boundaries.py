from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

LOCAL_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
SYSTEM_INCLUDE = re.compile(r"^\s*#\s*include\s+<([^>]+)>", re.MULTILINE)
NATIVE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
CMAKE_TOKEN = re.compile(r"[A-Za-z0-9_./:+$<>{}-]+")

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
        Path("core/features/src"),
    ),
    Path("runtime/strategies"): (Path("runtime/strategies"),),
    Path("core/recommendation"): (Path("core/recommendation"),),
}

CMAKE_TARGETS = {
    Path("core/features"): (Path("core/CMakeLists.txt"), "chronos_core"),
    Path("runtime/strategies"): (
        Path("runtime/strategies/CMakeLists.txt"),
        "chronos_strategy_runtime",
    ),
    Path("core/recommendation"): (
        Path("core/recommendation/CMakeLists.txt"),
        "chronos_recommendation",
    ),
}

ALLOWED_TARGET_DEPENDENCIES = {
    Path("core/features"): {"chronos_contracts", "chronos_options", "chronos_warnings"},
    Path("runtime/strategies"): {
        "chronos_core",
        "chronos_options",
        "chronos_strategy_sdk",
        "chronos_warnings",
    },
    Path("core/recommendation"): {
        "chronos_options",
        "chronos_strategy_runtime",
        "chronos_warnings",
    },
}

CMAKE_LINK_KEYWORDS = {"PUBLIC", "PRIVATE", "INTERFACE", "debug", "optimized", "general"}


@dataclass(frozen=True)
class BoundaryViolation:
    path: Path
    dependency: str


def cmake_call_bodies(text: str, command: str, target: str) -> list[str]:
    call = re.compile(
        rf"\b{re.escape(command)}\s*\(\s*{re.escape(target)}\b(?P<body>[^)]*)\)",
        re.DOTALL,
    )
    return [match.group("body") for match in call.finditer(text)]


def cmake_tokens(bodies: list[str]) -> list[str]:
    return [token for body in bodies for token in CMAKE_TOKEN.findall(body)]


def find_violations(root: Path) -> list[BoundaryViolation]:
    violations: list[BoundaryViolation] = []
    for authority, allowed in ALLOWED_INCLUDES.items():
        source_paths: list[Path] = []
        for relative_path in AUTHORITY_PATHS[authority]:
            path = root / relative_path
            source_paths.extend(path.rglob("*") if path.is_dir() else (path,))
        for path in sorted(source_paths):
            if not path.is_file() or path.suffix not in NATIVE_SUFFIXES:
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

        relative_cmake, target = CMAKE_TARGETS[authority]
        cmake_path = root / relative_cmake
        cmake = cmake_path.read_text(encoding="utf-8")
        dependencies = cmake_tokens(cmake_call_bodies(cmake, "target_link_libraries", target))
        for dependency in dependencies:
            if (
                dependency not in CMAKE_LINK_KEYWORDS
                and dependency not in ALLOWED_TARGET_DEPENDENCIES[authority]
            ):
                violations.append(BoundaryViolation(cmake_path, dependency))

        if authority == Path("core/features"):
            sources = cmake_tokens(cmake_call_bodies(cmake, "add_library", target))
            for source in sources:
                source_path = Path(source)
                if (
                    source_path.suffix in NATIVE_SUFFIXES
                    and source_path.stem.startswith("feature")
                    and not source.startswith("features/src/")
                ):
                    violations.append(
                        BoundaryViolation(cmake_path, "feature-source-outside-core/features")
                    )
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
