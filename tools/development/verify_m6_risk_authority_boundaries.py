from __future__ import annotations

import json
import re
import subprocess
import tempfile
from collections.abc import Collection
from dataclasses import dataclass
from pathlib import Path

INCLUDE_DIRECTIVE = re.compile(r"^[ \t]*#[ \t]*include(?P<body>.*)$", re.MULTILINE)
QUOTED_INCLUDE = re.compile(r'^"([^"]+)"$')
SYSTEM_INCLUDE = re.compile(r"^<([^>]+)>$")
NATIVE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
CMAKE_TOKEN = re.compile(r"[A-Za-z0-9_./:+$<>{}-]+")

AUTHORITY_PATH = Path("core/risk")
AUTHORITY_SOURCE_ROOT = Path("core/risk/src")
AUTHORITY_CMAKE = Path("core/risk/CMakeLists.txt")
AUTHORITY_GUARD_CMAKE = Path("core/risk/AssertTargetBoundary.cmake")
CORE_CMAKE = Path("core/CMakeLists.txt")
ROOT_CMAKE = Path("CMakeLists.txt")
TARGET = "chronos_risk"
ALLOWED_INCLUDES = (
    "chronos/core/risk/",
    "chronos/core/portfolio/portfolio_construction.hpp",
    "chronos/contracts/digest.hpp",
    "chronos/contracts/event_envelope.hpp",
    "chronos/contracts/fixed_point.hpp",
    "chronos/contracts/value_objects.hpp",
    "risk_arithmetic.hpp",
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
    "chronos_portfolio",
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
    "target_link_directories",
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
DIRECTORY_SCOPED_LINK_COMMANDS = {
    "add_link_options",
    "link_directories",
    "link_libraries",
}
NATIVE_GUARD_SENTINEL = "CHRONOS_M6_BOUNDARY_VIOLATION:"
NATIVE_GUARD_INCLUDE = "core/risk/AssertTargetBoundary.cmake"
PORTFOLIO_GUARD_INCLUDE = "core/portfolio/AssertTargetBoundary.cmake"
PORTFOLIO_GUARD_CMAKE = Path(PORTFOLIO_GUARD_INCLUDE)
NATIVE_GUARD_SYNTHETIC_FUNCTION = "_chronos_assert_risk_boundary"
NATIVE_GUARD_DIRECTORY_PROPERTY_VARIABLE = "_chronos_risk_directory_property"
NATIVE_GUARD_DIRECTORY_PROPERTY_LIST = "_chronos_risk_empty_directory_properties"
NATIVE_GUARD_EMPTY_DIRECTORY_PROPERTIES = (
    "COMPILE_DEFINITIONS",
    "COMPILE_OPTIONS",
    "INCLUDE_DIRECTORIES",
)
NATIVE_GUARD_EXACT_CHECKS = (
    (
        "LINK_LIBRARIES",
        "_chronos_risk_link_libraries",
        "_chronos_risk_expected_link_libraries",
        ("chronos_contracts", "chronos_portfolio", "chronos_options", "chronos_warnings"),
    ),
    (
        "INTERFACE_LINK_LIBRARIES",
        "_chronos_risk_interface_link_libraries",
        "_chronos_risk_expected_interface_link_libraries",
        (
            "chronos_contracts",
            "chronos_portfolio",
            "$<LINK_ONLY:chronos_options>",
            "$<LINK_ONLY:chronos_warnings>",
        ),
    ),
    (
        "SOURCES",
        "_chronos_risk_sources",
        "_chronos_risk_expected_sources",
        ("src/risk_arithmetic.hpp", "src/risk_decision.cpp"),
    ),
    (
        "INCLUDE_DIRECTORIES",
        "_chronos_risk_include_directories",
        "_chronos_risk_expected_include_directories",
        ("${CMAKE_CURRENT_LIST_DIR}/include",),
    ),
    (
        "INTERFACE_INCLUDE_DIRECTORIES",
        "_chronos_risk_interface_include_directories",
        "_chronos_risk_expected_include_directories",
        None,
    ),
)
NATIVE_GUARD_EMPTY_PROPERTIES = (
    "LINK_OPTIONS",
    "INTERFACE_LINK_OPTIONS",
    "LINK_DIRECTORIES",
    "INTERFACE_LINK_DIRECTORIES",
    "INTERFACE_LINK_LIBRARIES_DIRECT",
    "INTERFACE_LINK_LIBRARIES_DIRECT_EXCLUDE",
    "COMPILE_DEFINITIONS",
    "INTERFACE_COMPILE_DEFINITIONS",
    "COMPILE_FEATURES",
    "INTERFACE_COMPILE_FEATURES",
    "COMPILE_OPTIONS",
    "INTERFACE_COMPILE_OPTIONS",
    "PRECOMPILE_HEADERS",
    "INTERFACE_PRECOMPILE_HEADERS",
    "SYSTEM_INCLUDE_DIRECTORIES",
    "INTERFACE_SYSTEM_INCLUDE_DIRECTORIES",
    "INTERFACE_SOURCES",
)
NATIVE_GUARD_PROPERTY_VARIABLE = "_chronos_risk_property"
NATIVE_GUARD_PROPERTY_LIST = "_chronos_risk_empty_properties"
NATIVE_GUARD_SOURCE_PROPERTY_VARIABLE = "_chronos_risk_source_property"
NATIVE_GUARD_SOURCE_PROPERTY_LIST = "_chronos_risk_empty_source_properties"
NATIVE_GUARD_SOURCE_LIST = "_chronos_risk_canonical_sources"
NATIVE_GUARD_CANONICAL_SOURCE_PATHS = (
    "${CMAKE_CURRENT_LIST_DIR}/src/risk_decision.cpp",
    "${CMAKE_CURRENT_LIST_DIR}/src/risk_arithmetic.hpp",
)
NATIVE_GUARD_TRACE_SENTINEL_VARIABLE = "_chronos_m6_risk_boundary_assertion_complete"
NATIVE_GUARD_TRACE_SENTINEL_VALUE = "TRUE"
NATIVE_GUARD_EMPTY_SOURCE_PROPERTIES = (
    "COMPILE_DEFINITIONS",
    "COMPILE_FLAGS",
    "COMPILE_OPTIONS",
    "INCLUDE_DIRECTORIES",
    "HEADER_FILE_ONLY",
    "EXTERNAL_OBJECT",
    "KEEP_EXTENSION",
    "MACOSX_PACKAGE_LOCATION",
    "OBJECT_DEPENDS",
    "OBJECT_OUTPUTS",
    "SKIP_AUTOGEN",
    "SKIP_AUTOMOC",
    "SKIP_AUTORCC",
    "SKIP_AUTOUIC",
    "SKIP_LINTING",
    "SKIP_PRECOMPILE_HEADERS",
    "SKIP_UNITY_BUILD_INCLUSION",
    "SYMBOLIC",
    "UNITY_GROUP",
    "VS_COPY_TO_OUT_DIR",
    "VS_DEPLOYMENT_CONTENT",
    "VS_DEPLOYMENT_LOCATION",
    "VS_SETTINGS",
    "VS_SOURCE_SETTINGS_CXX",
    "VS_TOOL_OVERRIDE",
    "XCODE_EXPLICIT_FILE_TYPE",
    "XCODE_FILE_ATTRIBUTES",
    "XCODE_LAST_KNOWN_FILE_TYPE",
    "CXX_SCAN_FOR_MODULES",
)
NATIVE_GUARD_CONFIGURATIONS = (
    "Debug",
    "Release",
    "Benchmark",
    "RelWithDebInfo",
    "MinSizeRel",
    "${CMAKE_BUILD_TYPE}",
    "${CMAKE_CONFIGURATION_TYPES}",
)
GUARD_CRITICAL_COMMANDS = {
    "include",
    "message",
    "set",
    "set_property",
    "get_property",
    "get_target_property",
    "get_source_file_property",
    "list",
    "string",
}
POST_GUARD_TARGET_PROPERTIES = {
    "add_dependencies": "ADD_DEPENDENCIES",
    "target_compile_definitions": "COMPILE_DEFINITIONS",
    "target_compile_features": "COMPILE_FEATURES",
    "target_compile_options": "COMPILE_OPTIONS",
    "target_include_directories": "INCLUDE_DIRECTORIES",
    "target_link_directories": "LINK_DIRECTORIES",
    "target_link_libraries": "LINK_LIBRARIES",
    "target_link_options": "LINK_OPTIONS",
    "target_precompile_headers": "PRECOMPILE_HEADERS",
    "target_sources": "SOURCES",
}
POST_GUARD_MUTATION_COMMANDS = frozenset(
    set(POST_GUARD_TARGET_PROPERTIES)
    | {
        "add_custom_command",
        "set_property",
        "set_source_files_properties",
        "set_target_properties",
    }
)
CMAKE_VARIABLE_MUTATION_COMMANDS = frozenset({"list", "set", "string", "unset"})
CMAKE_LIST_MUTATION_OPERATIONS = frozenset(
    {
        "APPEND",
        "FILTER",
        "INSERT",
        "POP_BACK",
        "POP_FRONT",
        "PREPEND",
        "REMOVE_AT",
        "REMOVE_DUPLICATES",
        "REMOVE_ITEM",
        "REVERSE",
        "SORT",
        "TRANSFORM",
    }
)
CMAKE_STRING_MUTATION_OPERATIONS = frozenset({"APPEND", "CONCAT", "PREPEND"})
CANONICAL_AUTHORITY_SOURCES = (
    "src/risk_arithmetic.hpp",
    "src/risk_decision.cpp",
)


@dataclass(frozen=True)
class BoundaryViolation:
    path: Path
    dependency: str
    detail: str | None = None


@dataclass(frozen=True)
class CallableTargetMutation:
    positions: frozenset[int] = frozenset()
    dynamic: bool = False
    fixed_protected: bool = False


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


def has_local_include_traversal(dependency: str) -> bool:
    return any(component in {".", ".."} for component in dependency.split("/"))


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


def runtime_cmake_commands(text: str) -> list[tuple[str, list[str]]]:
    commands: list[tuple[str, list[str]]] = []
    callable_depth = 0
    for command, arguments in cmake_commands(text):
        if command in {"function", "macro"}:
            callable_depth += 1
            continue
        if command in {"endfunction", "endmacro"}:
            callable_depth = max(0, callable_depth - 1)
            continue
        if callable_depth == 0:
            commands.append((command, arguments))
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


def has_callable_definition(text: str, allowed: set[str] | None = None) -> bool:
    allowed = {name.lower() for name in (allowed or set())}
    return any(
        command in {"function", "macro"} and arguments and arguments[0].lower() not in allowed
        for command, arguments in cmake_commands(text)
    )


def normalize_cmake_builtin_alias(command: str, builtins: Collection[str]) -> str:
    normalized = command.lower()
    if normalized.startswith("_") and not normalized.startswith("__"):
        builtin = normalized[1:]
        if builtin in builtins:
            return builtin
    return normalized


def intercepts_guard_command(text: str) -> bool:
    return any(
        command in {"function", "macro"}
        and arguments
        and normalize_cmake_builtin_alias(arguments[0], GUARD_CRITICAL_COMMANDS)
        in GUARD_CRITICAL_COMMANDS
        for command, arguments in cmake_commands(text)
    )


def callable_definitions(
    text: str,
) -> dict[str, list[tuple[list[str], list[tuple[str, list[str]]]]]]:
    definitions: dict[str, list[tuple[list[str], list[tuple[str, list[str]]]]]] = {}
    active: list[tuple[str, list[str], list[tuple[str, list[str]]]]] = []
    for command, arguments in cmake_commands(text):
        if command in {"function", "macro"} and arguments:
            active.append((arguments[0].lower(), arguments[1:], []))
            continue
        if command in {"endfunction", "endmacro"}:
            if active:
                name, parameters, body = active.pop()
                definitions.setdefault(name, []).append((parameters, body))
            continue
        if active:
            active[-1][2].append((command, arguments))
    return definitions


def target_argument(command: str, arguments: list[str]) -> str | None:
    if command in set(CMAKE_TARGET_COMMANDS) | {"add_library"}:
        return arguments[0] if arguments else None
    if command == "set_property" and len(arguments) > 1 and arguments[0].upper() == "TARGET":
        return arguments[1]
    if command == "set_target_properties":
        return arguments[0] if arguments else None
    return None


def parameter_position(value: str | None, parameters: list[str]) -> int | None:
    if value is None or not value.startswith("${") or not value.endswith("}"):
        return None
    name = value[2:-1]
    if name.upper().startswith("ARGV") and name[4:].isdigit():
        return int(name[4:])
    try:
        return parameters.index(name)
    except ValueError:
        return None


def target_mutation_from_value(value: str | None, parameters: list[str]) -> CallableTargetMutation:
    if value == TARGET:
        return CallableTargetMutation(fixed_protected=True)
    if value is None or "$" not in value:
        return CallableTargetMutation()
    position = parameter_position(value, parameters)
    if position is None:
        return CallableTargetMutation(dynamic=True)
    return CallableTargetMutation(positions=frozenset({position}))


def combine_target_mutations(
    first: CallableTargetMutation, second: CallableTargetMutation
) -> CallableTargetMutation:
    return CallableTargetMutation(
        positions=first.positions | second.positions,
        dynamic=first.dynamic or second.dynamic,
        fixed_protected=first.fixed_protected or second.fixed_protected,
    )


def target_mutating_callables(
    cmake_texts: list[str],
) -> dict[str, CallableTargetMutation]:
    definitions: dict[str, list[tuple[list[str], list[tuple[str, list[str]]]]]] = {}
    for text in cmake_texts:
        for name, bodies in callable_definitions(text).items():
            definitions.setdefault(name, []).extend(bodies)

    mutating: dict[str, CallableTargetMutation] = {}
    for name, bodies in definitions.items():
        mutation = CallableTargetMutation()
        for parameters, body in bodies:
            for command, arguments in body:
                argument = target_argument(command, arguments)
                mutation = combine_target_mutations(
                    mutation, target_mutation_from_value(argument, parameters)
                )
        if mutation != CallableTargetMutation():
            mutating[name] = mutation

    changed = True
    while changed:
        changed = False
        for name, bodies in definitions.items():
            existing = mutating.get(name, CallableTargetMutation())
            known_mutating = name in mutating
            mutation = existing
            found_mutation = known_mutating
            for parameters, body in bodies:
                for command, arguments in body:
                    if command not in mutating:
                        continue
                    found_mutation = True
                    callee_mutation = mutating[command]
                    mutation = combine_target_mutations(
                        mutation,
                        CallableTargetMutation(
                            dynamic=callee_mutation.dynamic,
                            fixed_protected=callee_mutation.fixed_protected,
                        ),
                    )
                    for callee_position in callee_mutation.positions:
                        if callee_position >= len(arguments):
                            mutation = combine_target_mutations(
                                mutation, CallableTargetMutation(dynamic=True)
                            )
                            continue
                        mutation = combine_target_mutations(
                            mutation,
                            target_mutation_from_value(arguments[callee_position], parameters),
                        )
            if not found_mutation:
                continue
            if not known_mutating or mutation != existing:
                mutating[name] = mutation
                changed = True
    return mutating


def target_mutating_helper_invocation(
    text: str, mutating_callables: dict[str, CallableTargetMutation]
) -> str | None:
    for command, arguments in runtime_cmake_commands(text):
        if command not in mutating_callables:
            continue
        mutation = mutating_callables[command]
        if mutation.fixed_protected:
            return "protected-target-mutated-outside-owner"
        if mutation.dynamic:
            return "dynamic-target-mutation"
        for position in mutation.positions:
            if position >= len(arguments) or "$" in arguments[position]:
                return "dynamic-target-mutation"
            if arguments[position] == TARGET:
                return "protected-target-mutated-outside-owner"
    return None


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
    resolved_root = root.resolve()
    ancestor = path.resolve().parent
    while ancestor != resolved_root:
        if (ancestor / "CMakeCache.txt").is_file():
            return True
        if ancestor == ancestor.parent:
            return False
        ancestor = ancestor.parent
    return False


def within(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False


def is_owner_registered(core_cmake: str) -> bool:
    return any(
        command == "add_subdirectory" and arguments and arguments[0] == "risk"
        for command, arguments in top_level_cmake_commands(core_cmake)
    )


def has_native_target_guard(owner_cmake: str, root_cmake: str, guard_cmake: str | None) -> bool:
    if guard_cmake is None:
        return False
    stripped_root = strip_cmake_comments(root_cmake)
    tail = re.search(
        rf"(?is)include\s*\(\s*{re.escape(NATIVE_GUARD_INCLUDE)}\s*\)\s*"
        rf"include\s*\(\s*{re.escape(PORTFOLIO_GUARD_INCLUDE)}\s*\)\s*\Z",
        stripped_root,
    )
    if tail is None or not re.search(
        r"(?im)^\s*add_subdirectory\s*\(\s*core\s*\)", stripped_root[: tail.start()]
    ):
        return False
    if has_callable_definition(guard_cmake):
        return False

    commands = cmake_commands(
        f"function(_chronos_assert_risk_boundary)\n{guard_cmake}\nendfunction()"
    )
    block_starts = {"function", "macro", "if", "foreach", "while", "block"}
    block_ends = {"endfunction", "endmacro", "endif", "endforeach", "endwhile", "endblock"}
    entries: list[tuple[str, list[str], int]] = []
    depth = 0
    for command, arguments in commands:
        if command in block_ends:
            depth = max(0, depth - 1)
        entries.append((command, arguments, depth))
        if command in block_starts:
            depth += 1

    def matches(index: int, command: str, arguments: list[str], depth: int) -> bool:
        return index < len(entries) and entries[index] == (command, arguments, depth)

    for start in range(len(entries)):
        if entries[start] != ("function", [NATIVE_GUARD_SYNTHETIC_FUNCTION], 0):
            continue
        index = start + 1
        valid = matches(index, "if", ["COMMAND", "_message"], 1)
        index += 1
        valid = valid and index < len(entries)
        if valid:
            command, arguments, message_depth = entries[index]
            valid = (
                command == "message"
                and message_depth == 2
                and arguments[:2] == ["FATAL_ERROR", f"{NATIVE_GUARD_SENTINEL}COMMAND_OVERRIDE:"]
            )
        index += 1
        valid = valid and matches(
            index,
            "set_property",
            [
                "TARGET",
                "__chronos_m6_command_override_is_forbidden",
                "PROPERTY",
                "TYPE",
                "STATIC_LIBRARY",
            ],
            2,
        )
        index += 1
        valid = valid and matches(index, "endif", [], 1)
        index += 1

        directory_sequence = (
            (
                "set",
                [NATIVE_GUARD_DIRECTORY_PROPERTY_LIST, *NATIVE_GUARD_EMPTY_DIRECTORY_PROPERTIES],
                1,
            ),
            (
                "foreach",
                [
                    NATIVE_GUARD_DIRECTORY_PROPERTY_VARIABLE,
                    "IN",
                    "LISTS",
                    NATIVE_GUARD_DIRECTORY_PROPERTY_LIST,
                ],
                1,
            ),
            (
                "get_property",
                [
                    "_chronos_risk_directory_property_value",
                    "DIRECTORY",
                    "${CMAKE_CURRENT_LIST_DIR}",
                    "PROPERTY",
                    "${" + NATIVE_GUARD_DIRECTORY_PROPERTY_VARIABLE + "}",
                ],
                2,
            ),
            (
                "if",
                ["NOT", "${_chronos_risk_directory_property_value}", "STREQUAL"],
                2,
            ),
        )
        for command, arguments, expected_depth in directory_sequence:
            valid = valid and matches(index, command, arguments, expected_depth)
            index += 1
        if not valid or index >= len(entries):
            continue
        command, arguments, message_depth = entries[index]
        valid = (
            command == "message"
            and message_depth == 3
            and arguments[:2]
            == [
                "FATAL_ERROR",
                f"{NATIVE_GUARD_SENTINEL}DIRECTORY_${{{NATIVE_GUARD_DIRECTORY_PROPERTY_VARIABLE}}}:",
            ]
        )
        index += 1
        for command, expected_depth in (("endif", 2), ("endforeach", 1)):
            valid = valid and matches(index, command, [], expected_depth)
            index += 1
        if not valid:
            continue

        for property_name, actual, expected, expected_values in NATIVE_GUARD_EXACT_CHECKS:
            if expected_values is not None:
                valid = valid and matches(index, "set", [expected, *expected_values], 1)
                index += 1
            valid = valid and matches(
                index, "get_target_property", [actual, TARGET, property_name], 1
            )
            index += 1
            if property_name == "SOURCES":
                valid = valid and matches(index, "list", ["SORT", actual], 1)
                index += 1
            valid = valid and matches(
                index,
                "if",
                ["NOT", "${" + actual + "}", "STREQUAL", "${" + expected + "}"],
                1,
            )
            index += 1
            valid = valid and index < len(entries)
            if valid:
                command, arguments, message_depth = entries[index]
                valid = (
                    command == "message"
                    and message_depth == 2
                    and arguments[:2] == ["FATAL_ERROR", f"{NATIVE_GUARD_SENTINEL}{property_name}:"]
                )
            index += 1
            valid = valid and matches(index, "endif", [], 1)
            index += 1
            if not valid:
                break
        if not valid:
            continue

        empty_sequence = (
            ("set", [NATIVE_GUARD_PROPERTY_LIST, *NATIVE_GUARD_EMPTY_PROPERTIES], 1),
            (
                "foreach",
                [NATIVE_GUARD_PROPERTY_VARIABLE, "IN", "LISTS", NATIVE_GUARD_PROPERTY_LIST],
                1,
            ),
            (
                "get_property",
                [
                    "_chronos_risk_property_is_set",
                    "TARGET",
                    TARGET,
                    "PROPERTY",
                    "${" + NATIVE_GUARD_PROPERTY_VARIABLE + "}",
                    "SET",
                ],
                2,
            ),
            ("if", ["_chronos_risk_property_is_set"], 2),
            (
                "get_target_property",
                [
                    "_chronos_risk_property_value",
                    TARGET,
                    "${" + NATIVE_GUARD_PROPERTY_VARIABLE + "}",
                ],
                3,
            ),
            ("if", ["NOT", "${_chronos_risk_property_value}", "STREQUAL"], 3),
        )
        for command, arguments, expected_depth in empty_sequence:
            valid = valid and matches(index, command, arguments, expected_depth)
            index += 1
        if not valid or index >= len(entries):
            continue
        command, arguments, message_depth = entries[index]
        valid = (
            command == "message"
            and message_depth == 4
            and arguments[:2]
            == [
                "FATAL_ERROR",
                f"{NATIVE_GUARD_SENTINEL}${{{NATIVE_GUARD_PROPERTY_VARIABLE}}}:",
            ]
        )
        index += 1
        for command, expected_depth in (("endif", 3), ("endif", 2), ("endforeach", 1)):
            valid = valid and matches(index, command, [], expected_depth)
            index += 1
        if not valid:
            continue

        source_sequence = (
            (
                "set",
                [NATIVE_GUARD_SOURCE_PROPERTY_LIST, *NATIVE_GUARD_EMPTY_SOURCE_PROPERTIES],
                1,
            ),
            (
                "set",
                ["_chronos_risk_configurations", *NATIVE_GUARD_CONFIGURATIONS],
                1,
            ),
            (
                "list",
                ["REMOVE_DUPLICATES", "_chronos_risk_configurations"],
                1,
            ),
            (
                "foreach",
                [
                    "_chronos_risk_configuration",
                    "IN",
                    "LISTS",
                    "_chronos_risk_configurations",
                ],
                1,
            ),
            (
                "if",
                ["NOT", "${_chronos_risk_configuration}", "STREQUAL"],
                2,
            ),
            (
                "string",
                [
                    "TOUPPER",
                    "${_chronos_risk_configuration}",
                    "_chronos_risk_configuration",
                ],
                3,
            ),
            (
                "list",
                [
                    "APPEND",
                    NATIVE_GUARD_SOURCE_PROPERTY_LIST,
                    "COMPILE_DEFINITIONS_${_chronos_risk_configuration}",
                ],
                3,
            ),
            ("endif", [], 2),
            ("endforeach", [], 1),
            (
                "set",
                [NATIVE_GUARD_SOURCE_LIST, *NATIVE_GUARD_CANONICAL_SOURCE_PATHS],
                1,
            ),
            (
                "foreach",
                ["_chronos_risk_source", "IN", "LISTS", NATIVE_GUARD_SOURCE_LIST],
                1,
            ),
            ("set", ["_chronos_risk_expected_source_language"], 2),
            (
                "if",
                [
                    "${_chronos_risk_source}",
                    "STREQUAL",
                    "${CMAKE_CURRENT_LIST_DIR}/src/risk_decision.cpp",
                ],
                2,
            ),
            ("set", ["_chronos_risk_expected_source_language", "CXX"], 3),
            ("endif", [], 2),
            (
                "get_source_file_property",
                [
                    "_chronos_risk_source_language",
                    "${_chronos_risk_source}",
                    "TARGET_DIRECTORY",
                    TARGET,
                    "LANGUAGE",
                ],
                2,
            ),
            (
                "if",
                [
                    "NOT",
                    "${_chronos_risk_source_language}",
                    "STREQUAL",
                    "${_chronos_risk_expected_source_language}",
                ],
                2,
            ),
        )
        for command, arguments, expected_depth in source_sequence:
            valid = valid and matches(index, command, arguments, expected_depth)
            index += 1
        if not valid or index >= len(entries):
            continue
        command, arguments, message_depth = entries[index]
        valid = (
            command == "message"
            and message_depth == 3
            and arguments[:2] == ["FATAL_ERROR", f"{NATIVE_GUARD_SENTINEL}SOURCE_LANGUAGE:"]
        )
        index += 1
        source_tail_sequence = (
            ("endif", [], 2),
            (
                "get_source_file_property",
                [
                    "_chronos_risk_source_generated",
                    "${_chronos_risk_source}",
                    "TARGET_DIRECTORY",
                    TARGET,
                    "GENERATED",
                ],
                2,
            ),
            (
                "if",
                ["NOT", "${_chronos_risk_source_generated}", "STREQUAL", "0"],
                2,
            ),
        )
        for command, arguments, expected_depth in source_tail_sequence:
            valid = valid and matches(index, command, arguments, expected_depth)
            index += 1
        if not valid or index >= len(entries):
            continue
        command, arguments, message_depth = entries[index]
        valid = (
            command == "message"
            and message_depth == 3
            and arguments[:2] == ["FATAL_ERROR", f"{NATIVE_GUARD_SENTINEL}SOURCE_GENERATED:"]
        )
        index += 1
        source_property_sequence = (
            ("endif", [], 2),
            (
                "foreach",
                [
                    NATIVE_GUARD_SOURCE_PROPERTY_VARIABLE,
                    "IN",
                    "LISTS",
                    NATIVE_GUARD_SOURCE_PROPERTY_LIST,
                ],
                2,
            ),
            (
                "get_source_file_property",
                [
                    "_chronos_risk_source_property_value",
                    "${_chronos_risk_source}",
                    "TARGET_DIRECTORY",
                    TARGET,
                    "${" + NATIVE_GUARD_SOURCE_PROPERTY_VARIABLE + "}",
                ],
                3,
            ),
            (
                "if",
                [
                    "NOT",
                    "${_chronos_risk_source_property_value}",
                    "STREQUAL",
                    "NOTFOUND",
                ],
                3,
            ),
        )
        for command, arguments, expected_depth in source_property_sequence:
            valid = valid and matches(index, command, arguments, expected_depth)
            index += 1
        if not valid or index >= len(entries):
            continue
        command, arguments, message_depth = entries[index]
        valid = (
            command == "message"
            and message_depth == 4
            and arguments[:2]
            == [
                "FATAL_ERROR",
                f"{NATIVE_GUARD_SENTINEL}SOURCE_${{{NATIVE_GUARD_SOURCE_PROPERTY_VARIABLE}}}:",
            ]
        )
        index += 1
        for command, expected_depth in (
            ("endif", 3),
            ("endforeach", 2),
            ("endforeach", 1),
        ):
            valid = valid and matches(index, command, [], expected_depth)
            index += 1
        valid = valid and matches(
            index,
            "set",
            [NATIVE_GUARD_TRACE_SENTINEL_VARIABLE, NATIVE_GUARD_TRACE_SENTINEL_VALUE],
            1,
        )
        index += 1
        valid = valid and matches(index, "endfunction", [], 0)
        index += 1
        if not valid:
            continue

        owner_commands = cmake_commands(owner_cmake)
        return index == len(entries) and any(
            command in OWNER_ALLOWED_TARGET_COMMANDS | {"add_library"}
            and arguments
            and arguments[0] == TARGET
            for command, arguments in owner_commands
        )
    return False


def is_configurable_cmake_project(root_cmake: str | None) -> bool:
    if root_cmake is None:
        return False
    return all(
        re.search(rf"(?im)^[ \t]*{command}[ \t]*\(", root_cmake)
        for command in ("cmake_minimum_required", "project")
    )


def is_configure_eligible_cmake_project(root_cmake: str | None) -> bool:
    if root_cmake is None:
        return False
    return any(
        re.search(rf"(?im)^[ \t]*{command}[ \t]*\(", root_cmake)
        for command in ("cmake_minimum_required", "project")
    )


def missing_configure_prerequisites(root: Path, root_cmake: str | None) -> tuple[str, ...]:
    if root_cmake is None:
        return ()
    present = {
        command: bool(re.search(rf"(?im)^[ \t]*{command}[ \t]*\(", root_cmake))
        for command in ("cmake_minimum_required", "project")
    }
    if not any(present.values()) and not (root / PORTFOLIO_GUARD_CMAKE).is_file():
        return ()
    return tuple(command for command, is_present in present.items() if not is_present)


def trace_entries(output: str) -> list[dict[str, object]]:
    entries: list[dict[str, object]] = []
    for line in output.splitlines():
        try:
            trace = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(trace, dict) and "cmd" in trace:
            entries.append(trace)
    return entries


def trace_arguments(trace: dict[str, object]) -> list[str]:
    arguments = trace.get("args", [])
    if not isinstance(arguments, list):
        return []
    return [str(argument) for argument in arguments]


def trace_source_matches(value: str, sources: set[Path]) -> bool:
    candidate = Path(value)
    if candidate.is_absolute():
        return candidate.resolve() in sources
    normalized = value.replace("\\", "/")
    return normalized in {
        "src/risk_decision.cpp",
        "src/risk_arithmetic.hpp",
        "core/risk/src/risk_decision.cpp",
        "core/risk/src/risk_arithmetic.hpp",
    }


def is_project_trace(trace: dict[str, object], root: Path) -> bool:
    source = Path(str(trace.get("file", "")))
    if not source.is_absolute():
        return False
    resolved = source.resolve()
    return resolved == root or root in resolved.parents


def is_cxx_flags_variable(variable: str) -> bool:
    return variable == "CMAKE_CXX_FLAGS" or variable.startswith("CMAKE_CXX_FLAGS_")


def trace_mutated_cxx_flags_variable(trace: dict[str, object]) -> str | None:
    command = normalize_cmake_builtin_alias(
        str(trace.get("cmd", "")), CMAKE_VARIABLE_MUTATION_COMMANDS
    )
    arguments = trace_arguments(trace)
    if command in {"set", "unset"} and arguments:
        variable = arguments[0]
    elif (
        command == "list"
        and len(arguments) > 1
        and arguments[0].upper() in CMAKE_LIST_MUTATION_OPERATIONS
    ):
        variable = arguments[1]
    elif (
        command == "string"
        and len(arguments) > 1
        and arguments[0].upper() in CMAKE_STRING_MUTATION_OPERATIONS
    ):
        variable = arguments[1]
    else:
        return None
    return variable if is_cxx_flags_variable(variable) else None


def properties_after_marker(arguments: list[str], marker: str) -> list[str]:
    try:
        index = arguments.index(marker)
    except ValueError:
        return []
    return [arguments[position].upper() for position in range(index + 1, len(arguments), 2)]


def post_guard_mutation_properties(trace: dict[str, object], sources: set[Path]) -> list[str]:
    command = normalize_cmake_builtin_alias(str(trace.get("cmd", "")), POST_GUARD_MUTATION_COMMANDS)
    arguments = trace_arguments(trace)
    if command in POST_GUARD_TARGET_PROPERTIES and arguments and arguments[0] == TARGET:
        return [POST_GUARD_TARGET_PROPERTIES[command]]
    if command == "add_custom_command" and len(arguments) > 1:
        if arguments[0].upper() == "TARGET" and arguments[1] == TARGET:
            return ["CUSTOM_COMMAND"]
    if command == "set_target_properties":
        try:
            marker = arguments.index("PROPERTIES")
        except ValueError:
            return []
        if TARGET in arguments[:marker]:
            return properties_after_marker(arguments, "PROPERTIES")
    if command == "set_property" and arguments:
        scope = arguments[0].upper()
        try:
            marker = arguments.index("PROPERTY")
        except ValueError:
            return []
        if scope == "TARGET" and TARGET in arguments[1:marker]:
            return [arguments[marker + 1].upper()] if marker + 1 < len(arguments) else []
        if scope == "SOURCE" and any(
            trace_source_matches(value, sources)
            for value in arguments[1:marker]
            if value not in {"DIRECTORY", "TARGET_DIRECTORY", TARGET}
        ):
            return (
                [f"SOURCE_{arguments[marker + 1].upper()}"] if marker + 1 < len(arguments) else []
            )
    if command == "set_source_files_properties":
        markers = [
            index
            for index, value in enumerate(arguments)
            if value in {"DIRECTORY", "TARGET_DIRECTORY", "PROPERTIES"}
        ]
        source_end = min(markers, default=len(arguments))
        if any(trace_source_matches(value, sources) for value in arguments[:source_end]):
            return [
                f"SOURCE_{property_name}"
                for property_name in properties_after_marker(arguments, "PROPERTIES")
            ]
    return []


def configured_graph_violations(root: Path) -> list[BoundaryViolation]:
    root = root.resolve()
    owner_path = root / AUTHORITY_CMAKE
    try:
        with tempfile.TemporaryDirectory(prefix="chronos-m6-authority-") as build_directory:
            result = subprocess.run(
                [
                    "cmake",
                    "--trace-expand",
                    "--trace-format=json-v1",
                    "-S",
                    str(root),
                    "-B",
                    build_directory,
                    "-Wno-dev",
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=120,
                check=False,
            )
    except (OSError, subprocess.TimeoutExpired) as error:
        return [BoundaryViolation(owner_path, "cmake-configure-unavailable", str(error))]

    traces = trace_entries(result.stdout)
    for trace in traces:
        if trace.get("cmd") not in {"function", "macro"}:
            continue
        arguments = trace_arguments(trace)
        if (
            arguments
            and normalize_cmake_builtin_alias(str(arguments[0]), GUARD_CRITICAL_COMMANDS)
            in GUARD_CRITICAL_COMMANDS
        ):
            return [
                BoundaryViolation(
                    Path(str(trace.get("file", owner_path))),
                    "configured-target-property:COMMAND_OVERRIDE",
                    result.stdout.strip(),
                )
            ]

    owner_trace_indices = [
        index
        for index, trace in enumerate(traces)
        if Path(str(trace.get("file", ""))).resolve() == owner_path
    ]
    if owner_trace_indices:
        cxx_flags_violations = [
            BoundaryViolation(
                Path(str(trace.get("file", owner_path))),
                f"configured-cmake-variable:{variable}",
                result.stdout.strip(),
            )
            for trace in traces[owner_trace_indices[0] :]
            if is_project_trace(trace, root)
            if (variable := trace_mutated_cxx_flags_variable(trace)) is not None
        ]
        if cxx_flags_violations:
            return list(dict.fromkeys(cxx_flags_violations))

    sources = {
        (root / AUTHORITY_SOURCE_ROOT / "risk_decision.cpp").resolve(),
        (root / AUTHORITY_SOURCE_ROOT / "risk_arithmetic.hpp").resolve(),
    }
    custom_command_violations = [
        BoundaryViolation(
            Path(str(trace.get("file", owner_path))),
            "configured-target-property:CUSTOM_COMMAND",
            result.stdout.strip(),
        )
        for trace in traces
        if "CUSTOM_COMMAND" in post_guard_mutation_properties(trace, sources)
    ]
    if custom_command_violations:
        return list(dict.fromkeys(custom_command_violations))

    guard_path = (root / AUTHORITY_GUARD_CMAKE).resolve()
    sentinel_indices = [
        index
        for index, trace in enumerate(traces)
        if Path(str(trace.get("file", ""))).resolve() == guard_path
        and str(trace.get("cmd", "")).lower() == "set"
        and trace_arguments(trace)
        == [NATIVE_GUARD_TRACE_SENTINEL_VARIABLE, NATIVE_GUARD_TRACE_SENTINEL_VALUE]
    ]
    if result.returncode == 0 and not sentinel_indices:
        return [
            BoundaryViolation(
                root / AUTHORITY_GUARD_CMAKE,
                "configured-target-property:GUARD_NOT_EXECUTED",
                result.stdout.strip(),
            )
        ]
    if sentinel_indices:
        post_guard_violations: list[BoundaryViolation] = []
        for trace in traces[sentinel_indices[-1] + 1 :]:
            for property_name in post_guard_mutation_properties(trace, sources):
                post_guard_violations.append(
                    BoundaryViolation(
                        Path(str(trace.get("file", owner_path))),
                        f"configured-target-property:{property_name}",
                        result.stdout.strip(),
                    )
                )
        if post_guard_violations or result.returncode == 0:
            return list(dict.fromkeys(post_guard_violations))

    properties = list(
        dict.fromkeys(
            re.findall(
                rf"{re.escape(NATIVE_GUARD_SENTINEL)}([A-Z_]+):",
                result.stdout,
            )
        )
    )
    if properties:
        return [
            BoundaryViolation(
                owner_path,
                f"configured-target-property:{property_name}",
                result.stdout.strip(),
            )
            for property_name in properties
        ]
    return [
        BoundaryViolation(
            root / "CMakeLists.txt",
            "cmake-configure-failed",
            result.stdout.strip(),
        )
    ]


def directory_scoped_link_callables(cmake_texts: list[str]) -> set[str]:
    definitions: dict[str, list[tuple[list[str], list[tuple[str, list[str]]]]]] = {}
    for text in cmake_texts:
        for name, bodies in callable_definitions(text).items():
            definitions.setdefault(name, []).extend(bodies)

    mutating: set[str] = set()
    changed = True
    while changed:
        changed = False
        for name, bodies in definitions.items():
            if name in mutating:
                continue
            if any(
                command in DIRECTORY_SCOPED_LINK_COMMANDS or command in mutating
                for _, body in bodies
                for command, _ in body
            ):
                mutating.add(name)
                changed = True
    return mutating


def directory_scoped_link_mutation(text: str, mutating_callables: set[str]) -> bool:
    return any(
        command in DIRECTORY_SCOPED_LINK_COMMANDS or command in mutating_callables
        for command, _ in runtime_cmake_commands(text)
    )


def find_violations(root: Path) -> list[BoundaryViolation]:
    violations: list[BoundaryViolation] = []
    reported_dynamic_paths: set[Path] = set()
    all_cmake = {
        path: strip_cmake_comments(path.read_text(encoding="utf-8")) for path in cmake_files(root)
    }
    mutating_callables = target_mutating_callables(list(all_cmake.values()))
    directory_link_callables = directory_scoped_link_callables(list(all_cmake.values()))
    owner_path = root / AUTHORITY_CMAKE
    guard_path = root / AUTHORITY_GUARD_CMAKE
    core_path = root / CORE_CMAKE
    root_path = root / ROOT_CMAKE
    owner_cmake = all_cmake.get(owner_path)
    guard_cmake = all_cmake.get(guard_path)
    core_cmake = all_cmake.get(core_path)
    root_cmake = all_cmake.get(root_path)
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
    if root_cmake is None or not has_native_target_guard(owner_cmake, root_cmake, guard_cmake):
        violations.append(BoundaryViolation(owner_path, "missing-native-boundary-guard"))
    if has_callable_definition(owner_cmake):
        violations.append(BoundaryViolation(owner_path, "opaque-owner-target-mutation"))
    if any(command in mutating_callables for command, _ in runtime_cmake_commands(owner_cmake)):
        violations.append(BoundaryViolation(owner_path, "opaque-owner-target-mutation"))
    for candidate, candidate_cmake in all_cmake.items():
        if intercepts_guard_command(candidate_cmake):
            violations.append(BoundaryViolation(candidate, "fatal-command-interception"))

    authority_path = root / AUTHORITY_PATH
    for path in sorted(authority_path.rglob("*")):
        if (
            not path.is_file()
            or path.suffix.lower() not in NATIVE_SUFFIXES
            or is_generated_path(path, root)
        ):
            continue
        for kind, dependency in include_dependencies(path.read_text(encoding="utf-8")):
            if kind == "local" and (
                has_local_include_traversal(dependency)
                or not any(
                    dependency.startswith(prefix) if prefix.endswith("/") else dependency == prefix
                    for prefix in ALLOWED_INCLUDES
                )
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

    for scope in (root, root / "core", root / AUTHORITY_PATH):
        scope_path = scope / "CMakeLists.txt"
        scope_cmake = all_cmake.get(scope_path)
        if scope_cmake is not None and directory_scoped_link_mutation(
            scope_cmake, directory_link_callables
        ):
            violations.append(BoundaryViolation(scope_path, "directory-scoped-link-mutation"))

    dependencies = cmake_tokens(
        cmake_call_bodies(owner_cmake, "target_link_libraries", TARGET, top_level=True)
    )
    for dependency in dependencies:
        if dependency not in CMAKE_LINK_KEYWORDS and dependency not in ALLOWED_TARGET_DEPENDENCIES:
            violations.append(BoundaryViolation(owner_path, dependency))

    for candidate, candidate_cmake in all_cmake.items():
        if candidate in {owner_path, guard_path}:
            continue
        if target_declarations(candidate_cmake):
            violations.append(BoundaryViolation(candidate, "target-created-outside-owner"))
            continue
        helper_usage = target_mutating_helper_invocation(candidate_cmake, mutating_callables)
        if helper_usage is not None:
            violations.append(BoundaryViolation(candidate, helper_usage))
            continue
        usage = protected_target_usage(candidate_cmake)
        if dynamically_mutates_target(candidate_cmake) and candidate not in reported_dynamic_paths:
            violations.append(BoundaryViolation(candidate, "dynamic-target-mutation"))
            reported_dynamic_paths.add(candidate)
        elif usage is not None:
            violations.append(BoundaryViolation(candidate, usage))

    source_root = (root / AUTHORITY_SOURCE_ROOT).resolve()
    declared_sources: set[Path] = set()
    invalid_source_declaration = False
    sources = cmake_tokens(
        cmake_call_bodies(owner_cmake, "add_library", TARGET, top_level=True)
        + cmake_call_bodies(owner_cmake, "target_sources", TARGET, top_level=True)
    )
    for source in sources:
        if "$" in source:
            violations.append(BoundaryViolation(owner_path, "dynamic-authority-source"))
            invalid_source_declaration = True
            continue
        source_path = Path(source)
        if source_path.suffix.lower() not in NATIVE_SUFFIXES:
            continue
        resolved_source = (owner_path.parent / source_path).resolve()
        if not within(resolved_source, source_root):
            violations.append(BoundaryViolation(owner_path, "source-outside-authority"))
            invalid_source_declaration = True
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
    canonical_sources = {
        (owner_path.parent / source).resolve() for source in CANONICAL_AUTHORITY_SOURCES
    }
    if actual_sources != declared_sources:
        violations.append(BoundaryViolation(owner_path, "authority-source-not-declared"))
    elif not invalid_source_declaration and declared_sources != canonical_sources:
        violations.append(BoundaryViolation(owner_path, "noncanonical-authority-sources"))
    for prerequisite in missing_configure_prerequisites(root, root_cmake):
        violations.append(
            BoundaryViolation(
                root_path,
                f"missing-cmake-configure-prerequisite:{prerequisite}",
            )
        )
    violations = list(dict.fromkeys(violations))
    if is_configure_eligible_cmake_project(root_cmake):
        configured_violations = configured_graph_violations(root)
        if not violations or any(
            violation.dependency != "cmake-configure-failed" for violation in configured_violations
        ):
            violations.extend(configured_violations)
    return violations


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    violations = find_violations(root)
    if violations:
        for violation in violations:
            print(f"[FAIL] {violation.path}: forbidden M6 dependency {violation.dependency}")
            if violation.detail:
                print(violation.detail)
        return 1
    print("[OK] M6 risk authority has no forbidden dependencies")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
