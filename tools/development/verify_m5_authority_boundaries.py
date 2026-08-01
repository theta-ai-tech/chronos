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

AUTHORITY_SOURCE_ROOTS = {
    Path("core/features"): (Path("core/features/src"),),
    Path("runtime/strategies"): (Path("runtime/strategies/src"),),
    Path("core/recommendation"): (Path("core/recommendation/src"),),
}

CMAKE_TARGETS = {
    Path("core/features"): (Path("core/features/CMakeLists.txt"), "chronos_features"),
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
    Path("core/features"): {
        "chronos_contracts",
        "chronos_core",
        "chronos_options",
        "chronos_warnings",
    },
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
CMAKE_TARGET_COMMANDS = (
    "add_dependencies",
    "target_compile_definitions",
    "target_compile_features",
    "target_compile_options",
    "target_include_directories",
    "target_link_libraries",
    "target_link_options",
    "target_precompile_headers",
    "target_sources",
)
OWNER_ALLOWED_TARGET_COMMANDS = {
    "target_include_directories",
    "target_link_libraries",
    "target_sources",
}
READ_ONLY_TARGET_LIST_DECLARATIONS = {
    Path("core/portfolio/CMakeLists.txt"): {
        "_chronos_portfolio_expected_link_libraries": (
            "chronos_contracts",
            "chronos_recommendation",
            "chronos_options",
            "chronos_warnings",
        ),
        "_chronos_portfolio_expected_interface_link_libraries": (
            "chronos_contracts",
            "chronos_recommendation",
            "$<LINK_ONLY:chronos_options>",
            "$<LINK_ONLY:chronos_warnings>",
        ),
    }
}


@dataclass(frozen=True)
class BoundaryViolation:
    path: Path
    dependency: str


def strip_cmake_comments(text: str) -> str:
    output: list[str] = []
    index = 0
    while index < len(text):
        if text[index] == '"':
            start = index
            index += 1
            while index < len(text):
                if text[index] == "\\":
                    index += 2
                elif text[index] == '"':
                    index += 1
                    break
                else:
                    index += 1
            output.append(text[start:index])
            continue

        bracket = re.match(r"\[(=*)\[", text[index:])
        if bracket:
            closing = "]" + bracket.group(1) + "]"
            end = text.find(closing, index + len(bracket.group(0)))
            end = len(text) if end == -1 else end + len(closing)
            output.append(text[index:end])
            index = end
            continue

        if text[index] == "#":
            bracket_comment = re.match(r"#\[(=*)\[", text[index:])
            if bracket_comment:
                closing = "]" + bracket_comment.group(1) + "]"
                end = text.find(closing, index + len(bracket_comment.group(0)))
                end = len(text) if end == -1 else end + len(closing)
                output.append("\n" * text[index:end].count("\n"))
                index = end
                continue
            end = text.find("\n", index)
            if end == -1:
                break
            output.append("\n")
            index = end + 1
            continue

        output.append(text[index])
        index += 1
    return "".join(output)


def cmake_call_bodies(text: str, command: str, target: str) -> list[str]:
    return [
        " ".join(arguments[1:])
        for name, arguments in cmake_commands(text)
        if name == command.lower() and arguments and arguments[0] == target
    ]


def cmake_tokens(bodies: list[str]) -> list[str]:
    return [token for body in bodies for token in CMAKE_TOKEN.findall(body)]


def property_mutates_target(text: str, target: str) -> bool:
    for command, arguments in cmake_commands(text):
        if (
            command == "set_property"
            and len(arguments) > 1
            and arguments[0].upper() == "TARGET"
            and arguments[1] == target
        ):
            return True
        if command == "set_target_properties" and arguments and arguments[0] == target:
            return True
    return False


def unsupported_owner_mutation(text: str, target: str) -> bool:
    if property_mutates_target(text, target):
        return True
    return any(
        cmake_call_bodies(text, command, target)
        for command in CMAKE_TARGET_COMMANDS
        if command not in OWNER_ALLOWED_TARGET_COMMANDS
    )


def dynamically_mutates_target(text: str) -> bool:
    callable_definition = re.compile(
        r"\b(?:function|macro)\s*\([^)]*\).*?"
        r"\bend(?:function|macro)\s*\([^)]*\)",
        re.DOTALL | re.IGNORECASE,
    )
    invocations = callable_definition.sub("", text)
    direct_target = re.compile(
        rf'\b(?:{"|".join(CMAKE_TARGET_COMMANDS)})\s*\(\s*"?[^\s)]*\$',
        re.IGNORECASE,
    )
    property_target = re.compile(r'\bset_property\s*\(\s*TARGET\s+"?[^\s)]*\$', re.IGNORECASE)
    target_properties = re.compile(r'\bset_target_properties\s*\(\s*"?[^\s)]*\$', re.IGNORECASE)
    return bool(
        direct_target.search(invocations)
        or property_target.search(invocations)
        or target_properties.search(invocations)
    )


def cmake_commands(text: str) -> list[tuple[str, list[str]]]:
    commands: list[tuple[str, list[str]]] = []
    command = re.compile(r"\b(?P<name>[A-Za-z_][A-Za-z0-9_]*)\s*\(")
    position = 0
    while match := command.search(text, position):
        index = match.end()
        depth = 1
        while index < len(text) and depth > 0:
            if text[index] == '"':
                index += 1
                while index < len(text):
                    if text[index] == "\\":
                        index += 2
                    elif text[index] == '"':
                        index += 1
                        break
                    else:
                        index += 1
                continue
            bracket = re.match(r"\[(=*)\[", text[index:])
            if bracket:
                closing = "]" + bracket.group(1) + "]"
                end = text.find(closing, index + len(bracket.group(0)))
                index = len(text) if end == -1 else end + len(closing)
                continue
            if text[index] == "(":
                depth += 1
            elif text[index] == ")":
                depth -= 1
            index += 1
        if depth != 0:
            break
        body = text[match.end() : index - 1]
        commands.append((match.group("name").lower(), CMAKE_TOKEN.findall(body)))
        position = index
    return commands


def protected_target_usage(
    text: str,
    target: str,
    read_only_list_variables: dict[str, tuple[str, ...]] | None = None,
) -> str | None:
    # Outside its owner, a protected target may only be named as a dependency
    # of a built-in command. Passing it to an opaque helper is intentionally
    # forbidden so this gate never has to interpret arbitrary CMake code.
    for command, arguments in cmake_commands(text):
        target_positions = [index for index, argument in enumerate(arguments) if argument == target]
        if not target_positions:
            continue
        if (
            command == "set"
            and arguments[0] in (read_only_list_variables or set())
            and tuple(arguments[1:]) == (read_only_list_variables or {})[arguments[0]]
            and all(index > 0 for index in target_positions)
        ):
            continue
        if command in {"target_link_libraries", "add_dependencies"} and all(
            index > 0 for index in target_positions
        ):
            continue
        if command == "message":
            continue
        if command in {"set", "list", "string"}:
            return "dynamic-target-mutation"
        return "protected-target-mutated-outside-owner"
    return None


def has_only_canonical_read_only_declarations(
    text: str, declarations: dict[str, tuple[str, ...]]
) -> bool:
    commands = cmake_commands(text)
    for variable, expected_values in declarations.items():
        assignments = [
            arguments
            for command, arguments in commands
            if command == "set" and arguments and arguments[0] == variable
        ]
        if assignments != [[variable, *expected_values]]:
            return False
        reference = "${" + variable + "}"
        if any(
            reference in arguments and command not in {"if", "message"}
            for command, arguments in commands
        ):
            return False
    return True


def cmake_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*"):
        if not path.is_file() or (
            path.name != "CMakeLists.txt" and path.suffix.lower() != ".cmake"
        ):
            continue
        relative = path.relative_to(root)
        if any(part in {".codex", ".git", ".venv"} for part in relative.parts):
            continue
        ancestor = path.parent
        generated = False
        while ancestor != root:
            if (ancestor / "CMakeCache.txt").is_file():
                generated = True
                break
            ancestor = ancestor.parent
        if generated:
            continue
        files.append(path)
    return sorted(files)


def within_any(path: Path, roots: tuple[Path, ...]) -> bool:
    for root in roots:
        try:
            path.relative_to(root)
            return True
        except ValueError:
            continue
    return False


def find_violations(root: Path) -> list[BoundaryViolation]:
    violations: list[BoundaryViolation] = []
    reported_dynamic_paths: set[Path] = set()
    all_cmake = {
        path: strip_cmake_comments(path.read_text(encoding="utf-8")) for path in cmake_files(root)
    }
    invalid_read_only_paths: set[Path] = set()
    for relative_path, declarations in READ_ONLY_TARGET_LIST_DECLARATIONS.items():
        path = root / relative_path
        if path in all_cmake and not has_only_canonical_read_only_declarations(
            all_cmake[path], declarations
        ):
            violations.append(BoundaryViolation(path, "dynamic-target-mutation"))
            invalid_read_only_paths.add(path)
            reported_dynamic_paths.add(path)
    for authority, allowed in ALLOWED_INCLUDES.items():
        source_paths: list[Path] = []
        for relative_path in AUTHORITY_PATHS[authority]:
            path = root / relative_path
            source_paths.extend(path.rglob("*") if path.is_dir() else (path,))
        for path in sorted(source_paths):
            if not path.is_file() or path.suffix.lower() not in NATIVE_SUFFIXES:
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
        cmake = strip_cmake_comments(cmake_path.read_text(encoding="utf-8"))
        if dynamically_mutates_target(cmake) and cmake_path not in reported_dynamic_paths:
            violations.append(BoundaryViolation(cmake_path, "dynamic-target-mutation"))
            reported_dynamic_paths.add(cmake_path)
        if unsupported_owner_mutation(cmake, target):
            violations.append(BoundaryViolation(cmake_path, "unsupported-owner-target-mutation"))
        dependencies = cmake_tokens(cmake_call_bodies(cmake, "target_link_libraries", target))
        for dependency in dependencies:
            if (
                dependency not in CMAKE_LINK_KEYWORDS
                and dependency not in ALLOWED_TARGET_DEPENDENCIES[authority]
            ):
                violations.append(BoundaryViolation(cmake_path, dependency))

        for candidate in all_cmake:
            if candidate == cmake_path:
                continue
            if candidate in invalid_read_only_paths:
                continue
            candidate_cmake = all_cmake[candidate]
            relative_candidate = candidate.relative_to(root)
            usage = protected_target_usage(
                candidate_cmake,
                target,
                READ_ONLY_TARGET_LIST_DECLARATIONS.get(relative_candidate),
            )
            if (
                dynamically_mutates_target(candidate_cmake)
                and candidate not in reported_dynamic_paths
            ):
                violations.append(BoundaryViolation(candidate, "dynamic-target-mutation"))
                reported_dynamic_paths.add(candidate)
            elif usage is not None:
                violations.append(BoundaryViolation(candidate, usage))

        sources = cmake_tokens(
            cmake_call_bodies(cmake, "add_library", target)
            + cmake_call_bodies(cmake, "target_sources", target)
        )
        source_roots = tuple(
            (root / relative).resolve() for relative in AUTHORITY_SOURCE_ROOTS[authority]
        )
        declared_sources: set[Path] = set()
        for source in sources:
            if "$" in source:
                violations.append(BoundaryViolation(cmake_path, "dynamic-authority-source"))
                continue
            source_path = Path(source)
            if source_path.suffix.lower() not in NATIVE_SUFFIXES:
                continue
            resolved_source = (cmake_path.parent / source_path).resolve()
            if not within_any(resolved_source, source_roots):
                violations.append(BoundaryViolation(cmake_path, "source-outside-authority"))
                continue
            declared_sources.add(resolved_source)
        actual_sources = {
            path.resolve()
            for source_root in source_roots
            for path in source_root.rglob("*")
            if path.is_file() and path.suffix.lower() in NATIVE_SUFFIXES
        }
        if not actual_sources.issubset(declared_sources):
            violations.append(BoundaryViolation(cmake_path, "authority-source-not-declared"))
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
