from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path

INCLUDE_DIRECTIVE = re.compile(r"^[ \t]*#[ \t]*include(?P<body>.*)$", re.MULTILINE)
QUOTED_INCLUDE = re.compile(r'^"([^"]+)"$')
SYSTEM_INCLUDE = re.compile(r"^<([^>]+)>$")
NATIVE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
CMAKE_TOKEN = re.compile(r"[A-Za-z0-9_./:+$<>{}-]+")

AUTHORITY_PATH = Path("core/portfolio")
AUTHORITY_SOURCE_ROOT = Path("core/portfolio/src")
AUTHORITY_CMAKE = Path("core/portfolio/CMakeLists.txt")
CORE_CMAKE = Path("core/CMakeLists.txt")
TARGET = "chronos_portfolio"
ALLOWED_INCLUDES = (
    "chronos/core/portfolio/",
    "chronos/contracts/digest.hpp",
    "chronos/contracts/fixed_point.hpp",
    "chronos/core/recommendation/recommendation.hpp",
)
ALLOWED_SYSTEM_INCLUDES = {
    "algorithm",
    "array",
    "cstddef",
    "cstdint",
    "limits",
    "optional",
    "span",
    "string_view",
    "type_traits",
    "utility",
    "variant",
    "vector",
}
ALLOWED_TARGET_DEPENDENCIES = {
    "chronos_contracts",
    "chronos_recommendation",
    "chronos_options",
    "chronos_warnings",
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


def strip_cpp_comments(text: str) -> str:
    output: list[str] = []
    index = 0
    quote: str | None = None
    while index < len(text):
        character = text[index]
        if quote is not None:
            output.append(character)
            if character == "\\" and index + 1 < len(text):
                output.append(text[index + 1])
                index += 2
                continue
            if character == quote:
                quote = None
            index += 1
            continue
        if character in {'"', "'"}:
            quote = character
            output.append(character)
            index += 1
            continue
        if text.startswith("//", index):
            end = text.find("\n", index)
            if end == -1:
                break
            output.append("\n")
            index = end + 1
            continue
        if text.startswith("/*", index):
            end = text.find("*/", index + 2)
            end = len(text) if end == -1 else end + 2
            output.append("\n" * text[index:end].count("\n"))
            index = end
            continue
        output.append(character)
        index += 1
    return "".join(output)


def include_dependencies(text: str) -> list[tuple[str, str]]:
    preprocessed = re.sub(r"\\\r?\n", "", text)
    uncommented = strip_cpp_comments(preprocessed)
    dependencies: list[tuple[str, str]] = []
    for match in INCLUDE_DIRECTIVE.finditer(uncommented):
        body = match.group("body").strip()
        quoted = QUOTED_INCLUDE.fullmatch(body)
        system = SYSTEM_INCLUDE.fullmatch(body)
        if quoted:
            dependencies.append(("local", quoted.group(1)))
        elif system:
            dependencies.append(("system", system.group(1)))
        else:
            dependencies.append(("nonliteral", "nonliteral-include"))
    return dependencies


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


def top_level_cmake_commands(text: str) -> list[tuple[str, list[str]]]:
    commands: list[tuple[str, list[str]]] = []
    block_depth = 0
    block_starts = {"function", "macro", "if", "foreach", "while", "block"}
    block_ends = {"endfunction", "endmacro", "endif", "endforeach", "endwhile", "endblock"}
    for command, arguments in cmake_commands(text):
        if command in block_ends:
            block_depth = max(0, block_depth - 1)
            continue
        if block_depth == 0 and command not in block_starts:
            commands.append((command, arguments))
        if command in block_starts:
            block_depth += 1
    return commands


def cmake_call_bodies(
    text: str, command: str, target: str, *, top_level: bool = False
) -> list[str]:
    commands = top_level_cmake_commands(text) if top_level else cmake_commands(text)
    return [
        " ".join(arguments[1:])
        for name, arguments in commands
        if name == command.lower() and arguments and arguments[0] == target
    ]


def cmake_tokens(bodies: list[str]) -> list[str]:
    return [token for body in bodies for token in CMAKE_TOKEN.findall(body)]


def property_mutates_target(text: str, *, top_level: bool = False) -> bool:
    commands = top_level_cmake_commands(text) if top_level else cmake_commands(text)
    for command, arguments in commands:
        if (
            command == "set_property"
            and len(arguments) > 1
            and arguments[0].upper() == "TARGET"
            and arguments[1] == TARGET
        ):
            return True
        if command == "set_target_properties" and arguments and arguments[0] == TARGET:
            return True
    return False


def unsupported_owner_mutation(text: str) -> bool:
    return property_mutates_target(text, top_level=True) or any(
        cmake_call_bodies(text, command, TARGET, top_level=True)
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


def dynamically_composes_target(text: str) -> bool:
    for command, arguments in cmake_commands(text):
        if command == "string" and len(arguments) > 2 and arguments[0].upper() == "CONCAT":
            if "".join(arguments[2:]) == TARGET:
                return True
        if command == "set" and len(arguments) > 1 and "".join(arguments[1:]) == TARGET:
            return True
        if command == "list" and len(arguments) > 2 and "".join(arguments[2:]) == TARGET:
            return True
    return False


def protected_target_usage(text: str) -> str | None:
    for command, arguments in cmake_commands(text):
        target_positions = [index for index, argument in enumerate(arguments) if argument == TARGET]
        if not target_positions:
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


def target_declarations(text: str, *, top_level: bool = False) -> list[list[str]]:
    commands = top_level_cmake_commands(text) if top_level else cmake_commands(text)
    return [
        arguments
        for command, arguments in commands
        if command == "add_library" and arguments and arguments[0] == TARGET
    ]


def has_callable_definition(text: str) -> bool:
    return any(command in {"function", "macro"} for command, _ in cmake_commands(text))


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
        if not generated:
            files.append(path)
    return sorted(files)


def is_generated_path(path: Path, root: Path) -> bool:
    ancestor = path.parent
    while True:
        if (ancestor / "CMakeCache.txt").is_file():
            return True
        if ancestor == root:
            return False
        ancestor = ancestor.parent


def within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def is_owner_registered(core_cmake: str) -> bool:
    return any(
        command == "add_subdirectory" and arguments and arguments[0] == "portfolio"
        for command, arguments in top_level_cmake_commands(core_cmake)
    )


def find_violations(root: Path) -> list[BoundaryViolation]:
    violations: list[BoundaryViolation] = []
    reported_dynamic_paths: set[Path] = set()
    all_cmake = {
        path: strip_cmake_comments(path.read_text(encoding="utf-8")) for path in cmake_files(root)
    }
    owner_path = root / AUTHORITY_CMAKE
    core_path = root / CORE_CMAKE
    owner_cmake = all_cmake.get(owner_path)
    core_cmake = all_cmake.get(core_path)
    if owner_cmake is None:
        return [BoundaryViolation(owner_path, "authority-owner-not-registered")]
    if core_cmake is None or not is_owner_registered(core_cmake):
        violations.append(BoundaryViolation(core_path, "authority-owner-not-registered"))

    owner_declarations = target_declarations(owner_cmake, top_level=True)
    if (
        len(owner_declarations) != 1
        or len(owner_declarations[0]) < 2
        or owner_declarations[0][1].upper() != "STATIC"
    ):
        violations.append(BoundaryViolation(owner_path, "invalid-owner-target-declaration"))
    if has_callable_definition(owner_cmake):
        violations.append(BoundaryViolation(owner_path, "opaque-owner-target-mutation"))

    authority_path = root / AUTHORITY_PATH
    for path in sorted(authority_path.rglob("*")):
        if (
            not path.is_file()
            or path.suffix.lower() not in NATIVE_SUFFIXES
            or is_generated_path(path, root)
        ):
            continue
        for kind, dependency in include_dependencies(path.read_text(encoding="utf-8")):
            if kind == "local" and not any(
                dependency.startswith(prefix) if prefix.endswith("/") else dependency == prefix
                for prefix in ALLOWED_INCLUDES
            ):
                violations.append(BoundaryViolation(path, dependency))
            elif kind == "system" and dependency not in ALLOWED_SYSTEM_INCLUDES:
                violations.append(BoundaryViolation(path, dependency))
            elif kind == "nonliteral":
                violations.append(BoundaryViolation(path, dependency))

    if dynamically_mutates_target(owner_cmake):
        violations.append(BoundaryViolation(owner_path, "dynamic-target-mutation"))
        reported_dynamic_paths.add(owner_path)
    if unsupported_owner_mutation(owner_cmake):
        violations.append(BoundaryViolation(owner_path, "unsupported-owner-target-mutation"))

    dependencies = cmake_tokens(
        cmake_call_bodies(owner_cmake, "target_link_libraries", TARGET, top_level=True)
    )
    for dependency in dependencies:
        if dependency not in CMAKE_LINK_KEYWORDS and dependency not in ALLOWED_TARGET_DEPENDENCIES:
            violations.append(BoundaryViolation(owner_path, dependency))

    for candidate, candidate_cmake in all_cmake.items():
        if candidate == owner_path:
            continue
        if target_declarations(candidate_cmake):
            violations.append(BoundaryViolation(candidate, "target-created-outside-owner"))
            continue
        usage = protected_target_usage(candidate_cmake)
        if (
            dynamically_mutates_target(candidate_cmake)
            or dynamically_composes_target(candidate_cmake)
        ) and candidate not in reported_dynamic_paths:
            violations.append(BoundaryViolation(candidate, "dynamic-target-mutation"))
            reported_dynamic_paths.add(candidate)
        elif usage is not None:
            violations.append(BoundaryViolation(candidate, usage))

    source_root = (root / AUTHORITY_SOURCE_ROOT).resolve()
    declared_sources: set[Path] = set()
    sources = cmake_tokens(
        cmake_call_bodies(owner_cmake, "add_library", TARGET, top_level=True)
        + cmake_call_bodies(owner_cmake, "target_sources", TARGET, top_level=True)
    )
    for source in sources:
        if "$" in source:
            violations.append(BoundaryViolation(owner_path, "dynamic-authority-source"))
            continue
        source_path = Path(source)
        if source_path.suffix.lower() not in NATIVE_SUFFIXES:
            continue
        resolved_source = (owner_path.parent / source_path).resolve()
        if not within(resolved_source, source_root):
            violations.append(BoundaryViolation(owner_path, "source-outside-authority"))
            continue
        declared_sources.add(resolved_source)
    actual_sources = {
        path.resolve()
        for path in source_root.rglob("*")
        if (
            path.is_file()
            and path.suffix.lower() in NATIVE_SUFFIXES
            and not is_generated_path(path, root)
        )
    }
    if not actual_sources.issubset(declared_sources):
        violations.append(BoundaryViolation(owner_path, "authority-source-not-declared"))
    return violations


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    violations = find_violations(root)
    if violations:
        for violation in violations:
            print(f"[FAIL] {violation.path}: forbidden M6 dependency {violation.dependency}")
        return 1
    print("[OK] M6 portfolio authority has no forbidden dependencies")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
