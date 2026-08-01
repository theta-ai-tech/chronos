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
    "chronos/contracts/value_objects.hpp",
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
DIRECTORY_SCOPED_LINK_COMMANDS = {
    "add_link_options",
    "link_directories",
    "link_libraries",
}


@dataclass(frozen=True)
class BoundaryViolation:
    path: Path
    dependency: str


@dataclass(frozen=True)
class CallableTargetMutation:
    positions: frozenset[int] = frozenset()
    dynamic: bool = False
    fixed_protected: bool = False


@dataclass(frozen=True)
class CMakeIncludeCall:
    source_path: Path
    current_list_path: Path
    argument: str | None


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


def cmake_command_bodies(text: str) -> list[tuple[str, str]]:
    commands: list[tuple[str, str]] = []
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
        commands.append((match.group("name").lower(), body))
        position = index
    return commands


def cmake_commands(text: str) -> list[tuple[str, list[str]]]:
    return [(command, CMAKE_TOKEN.findall(body)) for command, body in cmake_command_bodies(text)]


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


def runtime_cmake_command_bodies(text: str) -> list[tuple[str, str]]:
    commands: list[tuple[str, str]] = []
    callable_depth = 0
    for command, body in cmake_command_bodies(text):
        if command in {"function", "macro"}:
            callable_depth += 1
            continue
        if command in {"endfunction", "endmacro"}:
            callable_depth = max(0, callable_depth - 1)
            continue
        if callable_depth == 0:
            commands.append((command, body))
    return commands


def cmake_arguments(body: str) -> list[str]:
    arguments: list[str] = []
    index = 0
    while index < len(body):
        while index < len(body) and body[index].isspace():
            index += 1
        if index == len(body):
            break
        if body[index] == '"':
            output: list[str] = []
            index += 1
            while index < len(body):
                if body[index] == "\\" and index + 1 < len(body):
                    output.append(body[index + 1])
                    index += 2
                    continue
                if body[index] == '"':
                    index += 1
                    arguments.append("".join(output))
                    break
                output.append(body[index])
                index += 1
            else:
                return arguments
            continue
        bracket = re.match(r"\[(=*)\[", body[index:])
        if bracket:
            closing = "]" + bracket.group(1) + "]"
            start = index + len(bracket.group(0))
            end = body.find(closing, start)
            if end == -1:
                return arguments
            arguments.append(body[start:end])
            index = end + len(closing)
            continue
        match = re.match(r"[^\s]+", body[index:])
        if match is None:
            break
        arguments.append(match.group(0))
        index += len(match.group(0))
    return arguments


def cmake_first_argument(body: str) -> str | None:
    arguments = cmake_arguments(body)
    return arguments[0] if arguments else None


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


def has_callable_definition(text: str) -> bool:
    return any(command in {"function", "macro"} for command, _ in cmake_commands(text))


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
    ancestor = path.parent
    while ancestor != root:
        if (ancestor / "CMakeCache.txt").is_file():
            return True
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
        command == "add_subdirectory" and arguments and arguments[0] == "portfolio"
        for command, arguments in top_level_cmake_commands(core_cmake)
    )


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


def raw_callable_definitions(
    cmake_texts: dict[Path, str],
) -> dict[str, list[tuple[Path, list[str], list[tuple[str, str]]]]]:
    definitions: dict[str, list[tuple[Path, list[str], list[tuple[str, str]]]]] = {}
    for path, text in cmake_texts.items():
        active: list[tuple[str, list[str], list[tuple[str, str]]]] = []
        for command, body in cmake_command_bodies(text):
            arguments = cmake_arguments(body)
            if command in {"function", "macro"} and arguments:
                active.append((arguments[0].lower(), arguments[1:], []))
                continue
            if command in {"endfunction", "endmacro"}:
                if active:
                    name, parameters, commands = active.pop()
                    definitions.setdefault(name, []).append((path, parameters, commands))
                continue
            if active:
                active[-1][2].append((command, body))
    return definitions


def substitute_cmake_bindings(value: str, bindings: dict[str, str]) -> str:
    for variable, replacement in bindings.items():
        value = value.replace("${" + variable + "}", replacement)
    return value


def executed_cmake_includes(
    path: Path,
    text: str,
    definitions: dict[str, list[tuple[Path, list[str], list[tuple[str, str]]]]],
) -> list[CMakeIncludeCall]:
    includes: list[CMakeIncludeCall] = []

    def execute(
        commands: list[tuple[str, str]],
        source_path: Path,
        current_list_path: Path,
        bindings: dict[str, str],
        active: frozenset[str],
    ) -> None:
        for command, body in commands:
            if command == "include":
                argument = cmake_first_argument(body)
                includes.append(
                    CMakeIncludeCall(
                        source_path,
                        current_list_path,
                        substitute_cmake_bindings(argument, bindings)
                        if argument is not None
                        else None,
                    )
                )
                continue
            if command == "cmake_language":
                arguments = cmake_arguments(body)
                dispatched = len(arguments) > 1 and arguments[0].upper() == "CALL"
                if arguments and (
                    arguments[0].upper() == "EVAL"
                    or (
                        dispatched
                        and arguments[1].lower() in DIRECTORY_SCOPED_LINK_COMMANDS | {"include"}
                    )
                ):
                    includes.append(CMakeIncludeCall(source_path, current_list_path, None))
                continue
            if command not in definitions or command in active:
                continue
            invocation_arguments = [
                substitute_cmake_bindings(argument, bindings) for argument in cmake_arguments(body)
            ]
            for definition_path, parameters, definition_body in definitions[command]:
                callable_bindings = {
                    parameter: invocation_arguments[index]
                    for index, parameter in enumerate(parameters)
                    if index < len(invocation_arguments)
                }
                callable_bindings.update(
                    {
                        f"ARGV{index}": argument
                        for index, argument in enumerate(invocation_arguments)
                    }
                )
                execute(
                    definition_body,
                    definition_path,
                    current_list_path,
                    callable_bindings,
                    active | {command},
                )

    execute(
        runtime_cmake_command_bodies(text),
        path,
        path,
        {},
        frozenset(),
    )
    return includes


def cmake_module_directories(
    root: Path,
    directory_scope: Path,
    paths: list[Path],
    all_cmake: dict[Path, str],
    inherited: set[Path],
    inherited_unsafe: bool,
) -> tuple[set[Path], bool]:
    directories = set(inherited)
    unsafe = inherited_unsafe
    for path in paths:
        text = all_cmake.get(path)
        if text is None:
            continue
        for command, body in runtime_cmake_command_bodies(text):
            arguments = cmake_arguments(body)
            values: list[str] | None = None
            if (
                command == "list"
                and len(arguments) > 2
                and arguments[0].upper() in {"APPEND", "PREPEND"}
                and arguments[1] == "CMAKE_MODULE_PATH"
            ):
                values = arguments[2:]
            elif command == "set" and arguments and arguments[0] == "CMAKE_MODULE_PATH":
                values = arguments[1:]
            elif "CMAKE_MODULE_PATH" in arguments:
                unsafe = True
            if values is None:
                continue
            for value in values:
                expanded = value
                for variable, replacement in {
                    "${CMAKE_CURRENT_LIST_DIR}": str(path.parent),
                    "${CMAKE_CURRENT_SOURCE_DIR}": str(directory_scope),
                    "${CMAKE_SOURCE_DIR}": str(root),
                    "${PROJECT_SOURCE_DIR}": str(root),
                }.items():
                    expanded = expanded.replace(variable, replacement)
                resolved = Path(expanded).resolve()
                if "$" in expanded or not within(resolved, root):
                    unsafe = True
                else:
                    directories.add(resolved)
    return directories, unsafe


def resolve_cmake_include(
    root: Path,
    current_list_path: Path,
    directory_scope: Path,
    argument: str,
    all_cmake: dict[Path, str],
    module_directories: set[Path],
    unsafe_module_path: bool,
) -> Path | None:
    substitutions = {
        "${CMAKE_CURRENT_LIST_DIR}": str(current_list_path.parent),
        "${CMAKE_CURRENT_SOURCE_DIR}": str(directory_scope),
        "${CMAKE_SOURCE_DIR}": str(root),
        "${PROJECT_SOURCE_DIR}": str(root),
    }
    expanded = argument
    for variable, value in substitutions.items():
        expanded = expanded.replace(variable, value)
    if "$" in expanded:
        return None

    include_path = Path(expanded)
    module_form = (
        not include_path.is_absolute()
        and "/" not in expanded
        and "\\" not in expanded
        and include_path.suffix.lower() != ".cmake"
    )
    if module_form:
        if unsafe_module_path:
            return None
        matches = [
            (directory / (expanded + ".cmake")).resolve()
            for directory in module_directories
            if (directory / (expanded + ".cmake")).resolve() in all_cmake
        ]
        return matches[0] if len(matches) == 1 else None

    if include_path.is_absolute():
        candidate = include_path
    else:
        candidate = directory_scope / include_path
    resolved = candidate.resolve()
    return resolved if within(resolved, root) and resolved in all_cmake else None


def protected_scope_include_graph(
    root: Path,
    scope_path: Path,
    all_cmake: dict[Path, str],
    inherited_paths: list[Path],
    inherited_module_directories: set[Path],
    inherited_unsafe_module_path: bool,
) -> tuple[list[Path], list[BoundaryViolation], set[Path], bool]:
    directory_scope = scope_path.parent
    reachable = [scope_path]
    visited = {scope_path}
    violations: list[BoundaryViolation] = []
    changed = True
    while changed:
        changed = False
        available_paths = list(dict.fromkeys(inherited_paths + reachable))
        module_directories, unsafe_module_path = cmake_module_directories(
            root,
            directory_scope,
            reachable,
            all_cmake,
            inherited_module_directories,
            inherited_unsafe_module_path,
        )
        definitions = raw_callable_definitions(
            {path: all_cmake[path] for path in available_paths if path in all_cmake}
        )
        for path in list(reachable):
            cmake = all_cmake.get(path)
            if cmake is None:
                continue
            for include in executed_cmake_includes(path, cmake, definitions):
                argument = include.argument
                included_path = (
                    resolve_cmake_include(
                        root,
                        include.current_list_path,
                        directory_scope,
                        argument,
                        all_cmake,
                        module_directories,
                        unsafe_module_path,
                    )
                    if argument is not None
                    else None
                )
                if included_path is None:
                    dependency = (
                        "dynamic-include-path"
                        if argument is None or "$" in argument
                        else "unresolved-include-path"
                    )
                    violations.append(BoundaryViolation(include.source_path, dependency))
                    continue
                if included_path not in visited:
                    visited.add(included_path)
                    reachable.append(included_path)
                    changed = True
    return (
        reachable,
        list(dict.fromkeys(violations)),
        module_directories,
        unsafe_module_path,
    )


def find_violations(root: Path) -> list[BoundaryViolation]:
    root = root.resolve()
    violations: list[BoundaryViolation] = []
    reported_dynamic_paths: set[Path] = set()
    all_cmake = {
        path.resolve(): strip_cmake_comments(path.read_text(encoding="utf-8"))
        for path in cmake_files(root)
    }
    mutating_callables = target_mutating_callables(list(all_cmake.values()))
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
    if any(command in mutating_callables for command, _ in runtime_cmake_commands(owner_cmake)):
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

    inherited_paths: list[Path] = []
    inherited_module_directories: set[Path] = set()
    inherited_unsafe_module_path = False
    for scope_path in (root / "CMakeLists.txt", core_path, owner_path):
        (
            reachable,
            include_violations,
            module_directories,
            unsafe_module_path,
        ) = protected_scope_include_graph(
            root,
            scope_path,
            all_cmake,
            inherited_paths,
            inherited_module_directories,
            inherited_unsafe_module_path,
        )
        violations.extend(include_violations)
        available_paths = list(dict.fromkeys(inherited_paths + reachable))
        directory_link_callables = directory_scoped_link_callables(
            [all_cmake[path] for path in available_paths if path in all_cmake]
        )
        for path in reachable:
            if path in all_cmake and directory_scoped_link_mutation(
                all_cmake[path], directory_link_callables
            ):
                violations.append(BoundaryViolation(path, "directory-scoped-link-mutation"))
        inherited_paths = available_paths
        inherited_module_directories = module_directories
        inherited_unsafe_module_path = unsafe_module_path

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
    return list(dict.fromkeys(violations))


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
