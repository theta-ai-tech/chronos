import importlib.util
import sys
from pathlib import Path


def load_tool():
    path = (
        Path(__file__).resolve().parents[2] / "tools/development/verify_m6_authority_boundaries.py"
    )
    spec = importlib.util.spec_from_file_location("verify_m6_authority_boundaries", path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


boundary = load_tool()

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
NATIVE_GUARD_PATH = Path("core/portfolio/AssertTargetBoundary.cmake")
NATIVE_GUARD_TAIL = "include(core/portfolio/AssertTargetBoundary.cmake)\n"


def write_portfolio_owner(root: Path, cmake: str) -> None:
    header = root / "core/portfolio/include/chronos/core/portfolio/portfolio_construction.hpp"
    header.parent.mkdir(parents=True)
    header.write_text("#pragma once\n", encoding="utf-8")
    owner = root / "core/portfolio/CMakeLists.txt"
    owner.parent.mkdir(parents=True, exist_ok=True)
    owner.write_text(cmake, encoding="utf-8")
    guard = root / NATIVE_GUARD_PATH
    guard.write_text(
        (REPOSITORY_ROOT / NATIVE_GUARD_PATH).read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    (root / "core/CMakeLists.txt").write_text("add_subdirectory(portfolio)\n", encoding="utf-8")
    (root / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n" + NATIVE_GUARD_TAIL, encoding="utf-8"
    )


def write_portfolio_source(root: Path, contents: str = "") -> None:
    source = root / "core/portfolio/src/portfolio_construction.cpp"
    source.parent.mkdir(parents=True)
    source.write_text(
        '#include "chronos/core/portfolio/portfolio_construction.hpp"\n' + contents,
        encoding="utf-8",
    )


def valid_owner_cmake(source: str = "src/portfolio_construction.cpp") -> str:
    return (
        f"add_library(chronos_portfolio STATIC {source})\n"
        "target_link_libraries(chronos_portfolio PUBLIC chronos_contracts "
        "chronos_recommendation PRIVATE chronos_options chronos_warnings)\n"
    )


def write_native_cmake_project(
    root: Path,
    *,
    root_preamble: str = "",
    root_before_core: str = "",
    core_before_owner: str = "",
    owner_before_target: str = "",
    owner_before_guard: str = "",
    root_after_core: str = "",
) -> None:
    write_portfolio_source(root)
    owner_cmake = (REPOSITORY_ROOT / "core/portfolio/CMakeLists.txt").read_text(encoding="utf-8")
    write_portfolio_owner(root, owner_before_target + owner_cmake + owner_before_guard)
    (root / "core/CMakeLists.txt").write_text(
        core_before_owner + "add_subdirectory(portfolio)\n",
        encoding="utf-8",
    )
    (root / "CMakeLists.txt").write_text(
        root_preamble + "cmake_minimum_required(VERSION 3.24)\n"
        "project(M6BoundaryFixture LANGUAGES CXX)\n"
        "add_library(chronos_contracts INTERFACE)\n"
        "add_library(chronos_recommendation INTERFACE)\n"
        "add_library(chronos_options INTERFACE)\n"
        "add_library(chronos_warnings INTERFACE)\n"
        + root_before_core
        + "add_subdirectory(core)\n"
        + root_after_core
        + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )


def configured_properties(root: Path, *, configure_directly: bool = False) -> set[str]:
    violations = (
        boundary.configured_graph_violations(root)
        if configure_directly
        else boundary.find_violations(root)
    )
    return {
        violation.dependency.removeprefix("configured-target-property:")
        for violation in violations
        if violation.dependency.startswith("configured-target-property:")
    }


def test_native_guard_accepts_exact_target_and_leaves_no_build_tree(tmp_path: Path) -> None:
    write_native_cmake_project(tmp_path)

    assert boundary.find_violations(tmp_path) == []
    assert list(tmp_path.rglob("CMakeCache.txt")) == []


def test_included_directory_mutations_in_root_core_and_owner_are_rejected(
    tmp_path: Path,
) -> None:
    mutations = (
        ("link_libraries(chronos_risk)\n", "LINK_LIBRARIES"),
        ("link_directories(${CMAKE_CURRENT_LIST_DIR}/risk)\n", "LINK_DIRECTORIES"),
        ("add_link_options(-Wl,--whole-archive)\n", "LINK_OPTIONS"),
    )
    scopes = ("root", "core", "owner")
    for scope in scopes:
        for index, (mutation, expected_property) in enumerate(mutations):
            root = tmp_path / f"{scope}-{index}"
            module = root / f"cmake/{scope}-mutation.cmake"
            module.parent.mkdir(parents=True)
            module.write_text(mutation, encoding="utf-8")
            include = f'include("{module}")\n'
            suffix = "target" if scope == "owner" else "owner" if scope == "core" else "core"
            arguments = {f"{scope}_before_{suffix}": include}
            write_native_cmake_project(root, **arguments)

            assert configured_properties(root) == {expected_property}


def test_cmake_language_call_to_user_function_cannot_bypass_guard(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core=(
            "function(add_scoped_link dependency)\n"
            "  link_libraries(${dependency})\n"
            "endfunction()\n"
            "cmake_language(CALL add_scoped_link chronos_risk)\n"
        ),
    )

    assert configured_properties(tmp_path) == {"LINK_LIBRARIES"}


def test_callable_local_module_path_cannot_select_external_mutation(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    external = tmp_path / "external"
    external.mkdir()
    (external / "PortfolioMutation.cmake").write_text(
        "link_libraries(chronos_risk)\n",
        encoding="utf-8",
    )
    write_native_cmake_project(
        root,
        root_before_core=(
            "function(configure_portfolio)\n"
            f'  list(PREPEND CMAKE_MODULE_PATH "{external}")\n'
            "  include(PortfolioMutation)\n"
            "endfunction()\n"
            "configure_portfolio()\n"
        ),
    )

    assert configured_properties(root) == {"LINK_LIBRARIES"}


def test_escaped_parenthesis_cannot_truncate_boundary_check(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_preamble="set(escaped_parenthesis value\\()\nlink_libraries(chronos_risk)\n",
    )

    assert configured_properties(tmp_path) == {"LINK_LIBRARIES"}


def test_unquoted_list_expansion_cannot_hide_dispatched_mutation(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core=(
            "function(add_scoped_links)\n"
            "  link_libraries(${ARGV})\n"
            "endfunction()\n"
            "set(scoped_links chronos_risk chronos_execution)\n"
            "add_scoped_links(${scoped_links})\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"LINK_LIBRARIES"}


def test_direct_include_and_compile_capabilities_are_rejected(tmp_path: Path) -> None:
    mutations = (
        (
            "target_include_directories(chronos_portfolio PRIVATE /unexpected)\n",
            "INCLUDE_DIRECTORIES",
        ),
        (
            "target_compile_definitions(chronos_portfolio PRIVATE UNEXPECTED)\n",
            "COMPILE_DEFINITIONS",
        ),
        ("target_compile_features(chronos_portfolio PRIVATE cxx_std_23)\n", "COMPILE_FEATURES"),
        ("target_compile_options(chronos_portfolio PRIVATE -fno-exceptions)\n", "COMPILE_OPTIONS"),
    )
    for index, (mutation, expected_property) in enumerate(mutations):
        root = tmp_path / str(index)
        write_native_cmake_project(root, owner_before_guard=mutation)

        assert configured_properties(root, configure_directly=True) == {expected_property}


def test_external_module_cannot_inject_sources_or_consumer_direct_links(
    tmp_path: Path,
) -> None:
    mutations = (
        (
            "set_property(TARGET chronos_portfolio PROPERTY "
            "INTERFACE_LINK_LIBRARIES_DIRECT chronos_risk)\n",
            "INTERFACE_LINK_LIBRARIES_DIRECT",
        ),
        (
            "set_property(TARGET chronos_portfolio PROPERTY "
            "INTERFACE_LINK_LIBRARIES_DIRECT_EXCLUDE chronos_contracts)\n",
            "INTERFACE_LINK_LIBRARIES_DIRECT_EXCLUDE",
        ),
        (
            'target_sources(chronos_portfolio PRIVATE "${CMAKE_CURRENT_LIST_DIR}/external.cpp")\n',
            "SOURCES",
        ),
        (
            "target_sources(chronos_portfolio INTERFACE "
            '"${CMAKE_CURRENT_LIST_DIR}/external.cpp")\n',
            "INTERFACE_SOURCES",
        ),
    )
    for index, (mutation, expected_property) in enumerate(mutations):
        root = tmp_path / str(index)
        external = root / "external"
        external.mkdir(parents=True)
        (external / "external.cpp").write_text("int external() { return 0; }\n", encoding="utf-8")
        module = external / "PortfolioMutation.cmake"
        module.write_text(mutation, encoding="utf-8")
        write_native_cmake_project(
            root,
            root_before_core="add_library(chronos_risk INTERFACE)\n",
            owner_before_guard=f'include("{module}")\n',
            root_after_core=(
                "add_library(chronos_consumer STATIC consumer.cpp)\n"
                "target_link_libraries(chronos_consumer PRIVATE chronos_portfolio)\n"
            ),
        )
        (root / "consumer.cpp").write_text("int consumer() { return 0; }\n", encoding="utf-8")

        assert configured_properties(root, configure_directly=True) == {expected_property}


def test_after_core_dynamic_target_link_mutation_is_rejected(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    late_subdirectory = root / "late"
    late_subdirectory.mkdir(parents=True)
    (late_subdirectory / "CMakeLists.txt").write_text(
        "function(attach_dependency target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "set(portfolio_target chronos_portfolio)\n"
        "attach_dependency(${portfolio_target} chronos_risk)\n",
        encoding="utf-8",
    )
    write_native_cmake_project(
        root,
        root_before_core="add_library(chronos_risk INTERFACE)\n",
        root_after_core="add_subdirectory(late)\n",
    )

    assert configured_properties(root, configure_directly=True) == {"LINK_LIBRARIES"}


def test_source_file_compile_definition_mutation_is_rejected(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    source = root / "core/portfolio/src/portfolio_construction.cpp"
    write_native_cmake_project(
        root,
        root_after_core=(
            f'set_source_files_properties("{source}"\n'
            "  TARGET_DIRECTORY chronos_portfolio\n"
            "  PROPERTIES COMPILE_DEFINITIONS REVIEWER_BYPASS)\n"
        ),
    )

    assert configured_properties(root, configure_directly=True) == {"SOURCE_COMPILE_DEFINITIONS"}


def test_message_override_cannot_swallow_boundary_failure(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    external = tmp_path / "external"
    external.mkdir()
    module = external / "MessageOverride.cmake"
    module.write_text(
        "function(message)\nendfunction()\nlink_libraries(chronos_risk)\n",
        encoding="utf-8",
    )
    write_native_cmake_project(
        root,
        root_before_core=(f'add_library(chronos_risk INTERFACE)\ninclude("{module}")\n'),
    )

    assert configured_properties(root, configure_directly=True) == {"COMMAND_OVERRIDE"}


def test_double_message_override_cannot_swallow_boundary_failure(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    external = tmp_path / "external"
    external.mkdir()
    module = external / "MessageOverride.cmake"
    module.write_text(
        "function(message)\nendfunction()\n"
        "function(_message)\nendfunction()\n"
        "link_libraries(chronos_risk)\n",
        encoding="utf-8",
    )
    write_native_cmake_project(
        root,
        root_before_core=(f'add_library(chronos_risk INTERFACE)\ninclude("{module}")\n'),
    )

    assert configured_properties(root, configure_directly=True) == {"COMMAND_OVERRIDE"}


def test_deferred_guard_redefinition_cannot_disable_assertion(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core="add_library(chronos_risk INTERFACE)\n",
        root_after_core=(
            "target_link_libraries(chronos_portfolio PRIVATE chronos_risk)\n"
            "function(_chronos_assert_portfolio_boundary)\nendfunction()\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"LINK_LIBRARIES"}


def test_deferred_guard_cancellation_cannot_disable_assertion(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core="add_library(chronos_risk INTERFACE)\n",
        root_after_core=(
            "target_link_libraries(chronos_portfolio PRIVATE chronos_risk)\n"
            "function(cancel_portfolio_guard)\n"
            "  cmake_language(DEFER CANCEL_CALL chronos_m6_portfolio_boundary)\n"
            "endfunction()\n"
            "cmake_language(DEFER CALL cancel_portfolio_guard)\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"LINK_LIBRARIES"}


def test_generator_specific_source_mutations_are_rejected(tmp_path: Path) -> None:
    mutations = (
        ("VS_SETTINGS", "ExcludedFromBuild=true"),
        ("XCODE_EXPLICIT_FILE_TYPE", "sourcecode.c.c"),
    )
    for index, (property_name, value) in enumerate(mutations):
        root = tmp_path / str(index)
        source = root / "core/portfolio/src/portfolio_construction.cpp"
        write_native_cmake_project(
            root,
            root_after_core=(
                f'set_source_files_properties("{source}"\n'
                "  TARGET_DIRECTORY chronos_portfolio\n"
                f"  PROPERTIES {property_name} {value})\n"
            ),
        )

        assert configured_properties(root, configure_directly=True) == {f"SOURCE_{property_name}"}


def test_custom_configuration_source_definition_is_rejected(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    source = root / "core/portfolio/src/portfolio_construction.cpp"
    write_native_cmake_project(
        root,
        root_before_core='set(CMAKE_BUILD_TYPE Reviewer CACHE STRING "" FORCE)\n',
        root_after_core=(
            f'set_source_files_properties("{source}"\n'
            "  TARGET_DIRECTORY chronos_portfolio\n"
            "  PROPERTIES COMPILE_DEFINITIONS_REVIEWER REVIEWER_BYPASS)\n"
        ),
    )

    assert configured_properties(root, configure_directly=True) == {
        "SOURCE_COMPILE_DEFINITIONS_REVIEWER"
    }


def test_configure_tail_registration_rejects_deletion_inert_and_reorder(
    tmp_path: Path,
) -> None:
    valid = tmp_path / "valid"
    write_native_cmake_project(valid)
    owner_cmake = (valid / "core/portfolio/CMakeLists.txt").read_text(encoding="utf-8")
    root_cmake = (valid / "CMakeLists.txt").read_text(encoding="utf-8")
    guard_cmake = (valid / NATIVE_GUARD_PATH).read_text(encoding="utf-8")

    assert boundary.has_native_target_guard(owner_cmake, root_cmake, guard_cmake)

    invalid_roots = (
        root_cmake.replace(NATIVE_GUARD_TAIL, "", 1),
        root_cmake.replace(
            NATIVE_GUARD_TAIL,
            "if(FALSE)\n" + NATIVE_GUARD_TAIL + "endif()\n",
            1,
        ),
        root_cmake.replace(NATIVE_GUARD_TAIL, "", 1).replace(
            "add_subdirectory(core)\n",
            NATIVE_GUARD_TAIL + "add_subdirectory(core)\n",
            1,
        ),
    )
    for invalid_root_cmake in invalid_roots:
        assert not boundary.has_native_target_guard(owner_cmake, invalid_root_cmake, guard_cmake)


def test_configure_tail_is_required_even_without_project_command(tmp_path: Path) -> None:
    write_native_cmake_project(tmp_path)
    owner_cmake = (tmp_path / "core/portfolio/CMakeLists.txt").read_text(encoding="utf-8")
    root_cmake = (tmp_path / "CMakeLists.txt").read_text(encoding="utf-8")
    guard_cmake = (tmp_path / NATIVE_GUARD_PATH).read_text(encoding="utf-8")
    root_cmake = root_cmake.replace("project(M6BoundaryFixture LANGUAGES CXX)\n", "", 1)
    root_cmake = root_cmake.replace(NATIVE_GUARD_TAIL, "", 1)

    assert not boundary.has_native_target_guard(owner_cmake, root_cmake, guard_cmake)


def test_repository_message_override_is_a_static_violation(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core="function(message)\nendfunction()\n",
    )

    assert "fatal-command-interception" in {
        item.dependency for item in boundary.find_violations(tmp_path)
    }


def test_deleting_native_guard_is_a_structural_violation(tmp_path: Path) -> None:
    write_native_cmake_project(tmp_path)
    (tmp_path / NATIVE_GUARD_PATH).unlink()

    assert {item.dependency for item in boundary.find_violations(tmp_path)} == {
        "missing-native-boundary-guard"
    }


def test_partial_or_inert_native_guard_is_a_structural_violation(tmp_path: Path) -> None:
    exact_condition = (
        'if(NOT "${_chronos_portfolio_link_libraries}" STREQUAL\n'
        '    "${_chronos_portfolio_expected_link_libraries}")'
    )
    exact_message = (
        "  message(FATAL_ERROR\n"
        '    "CHRONOS_M6_BOUNDARY_VIOLATION:LINK_LIBRARIES: expected "\n'
        '    "`${_chronos_portfolio_expected_link_libraries}`, got "\n'
        '    "`${_chronos_portfolio_link_libraries}`")\n'
    )
    transformations = (
        (
            "get_target_property(\n"
            "  _chronos_portfolio_link_libraries chronos_portfolio LINK_LIBRARIES)\n",
            "",
        ),
        (exact_condition, "if(FALSE)"),
        (
            exact_condition,
            'if("${_chronos_portfolio_link_libraries}" STREQUAL\n'
            '   "${_chronos_portfolio_expected_link_libraries}")',
        ),
        (exact_message, ""),
    )
    for index, (original, replacement) in enumerate(transformations):
        root = tmp_path / str(index)
        write_native_cmake_project(root)
        owner = root / NATIVE_GUARD_PATH
        contents = owner.read_text(encoding="utf-8")
        assert original in contents
        owner.write_text(contents.replace(original, replacement, 1), encoding="utf-8")

        assert "missing-native-boundary-guard" in {
            item.dependency for item in boundary.find_violations(root)
        }


def test_direct_consumer_linkage_remains_valid(tmp_path: Path) -> None:
    (tmp_path / "consumer.cpp").write_text("int consumer() { return 0; }\n", encoding="utf-8")
    write_native_cmake_project(
        tmp_path,
        root_after_core=(
            "add_library(chronos_consumer STATIC consumer.cpp)\n"
            "target_link_libraries(chronos_consumer PRIVATE chronos_portfolio)\n"
        ),
    )

    assert boundary.find_violations(tmp_path) == []


def test_current_m6_portfolio_authority_has_no_forbidden_dependencies() -> None:
    root = Path(__file__).resolve().parents[2]
    assert boundary.find_violations(root) == []


def test_forbidden_portfolio_include_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path, '#include "chronos/core/risk/risk_authority.hpp"\n')
    write_portfolio_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"chronos/core/risk/risk_authority.hpp"}


def test_portfolio_include_allowlist_rejects_parent_traversal(tmp_path: Path) -> None:
    dependency = "chronos/core/portfolio/../../risk/risk_authority.hpp"
    write_portfolio_source(tmp_path, f'#include "{dependency}"\n')
    write_portfolio_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {dependency}


def test_portfolio_value_objects_include_is_allowed(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path, '#include "chronos/contracts/value_objects.hpp"\n')
    write_portfolio_owner(tmp_path, valid_owner_cmake())

    assert boundary.find_violations(tmp_path) == []


def test_forbidden_portfolio_link_dependency_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        valid_owner_cmake().replace("chronos_warnings", "chronos_warnings chronos_risk"),
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"chronos_risk"}


def test_owner_directory_scoped_link_mutations_are_rejected(tmp_path: Path) -> None:
    mutations = (
        "link_libraries(chronos_risk)\n",
        "link_directories(${CMAKE_SOURCE_DIR}/risk)\n",
        "add_link_options(-Wl,--whole-archive)\n",
    )
    for index, mutation in enumerate(mutations):
        root = tmp_path / str(index)
        write_portfolio_source(root)
        write_portfolio_owner(root, mutation + valid_owner_cmake())

        violations = boundary.find_violations(root)

        assert {item.dependency for item in violations} == {"directory-scoped-link-mutation"}


def test_core_ancestor_directory_scoped_link_mutations_are_rejected(
    tmp_path: Path,
) -> None:
    mutations = (
        "link_libraries(chronos_risk)\n",
        "link_directories(${CMAKE_SOURCE_DIR}/risk)\n",
        "add_link_options(-Wl,--whole-archive)\n",
    )
    for index, mutation in enumerate(mutations):
        root = tmp_path / str(index)
        write_portfolio_source(root)
        write_portfolio_owner(root, valid_owner_cmake())
        (root / "core/CMakeLists.txt").write_text(
            mutation + "add_subdirectory(portfolio)\n", encoding="utf-8"
        )

        violations = boundary.find_violations(root)

        assert {item.dependency for item in violations} == {"directory-scoped-link-mutation"}


def test_root_ancestor_directory_scoped_link_mutations_are_rejected(
    tmp_path: Path,
) -> None:
    mutations = (
        "link_libraries(chronos_risk)\n",
        "link_directories(${CMAKE_SOURCE_DIR}/risk)\n",
        "add_link_options(-Wl,--whole-archive)\n",
    )
    for index, mutation in enumerate(mutations):
        root = tmp_path / str(index)
        write_portfolio_source(root)
        write_portfolio_owner(root, valid_owner_cmake())
        (root / "CMakeLists.txt").write_text(
            mutation + "add_subdirectory(core)\n" + NATIVE_GUARD_TAIL,
            encoding="utf-8",
        )

        violations = boundary.find_violations(root)

        assert {item.dependency for item in violations} == {"directory-scoped-link-mutation"}


def test_root_ancestor_directory_scoped_link_helper_is_rejected(
    tmp_path: Path,
) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    helper = tmp_path / "cmake/PortfolioLinks.cmake"
    helper.parent.mkdir(parents=True)
    helper.write_text(
        "function(add_portfolio_links)\n  link_libraries(chronos_risk)\nendfunction()\n",
        encoding="utf-8",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "include(cmake/PortfolioLinks.cmake)\nadd_portfolio_links()\nadd_subdirectory(core)\n"
        + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"directory-scoped-link-mutation"}


def test_portfolio_source_outside_owner_directory_is_rejected(tmp_path: Path) -> None:
    outside = tmp_path / "core/src/portfolio_construction.cpp"
    outside.parent.mkdir(parents=True)
    outside.write_text("#include <cstdint>\n", encoding="utf-8")
    write_portfolio_owner(tmp_path, valid_owner_cmake("../src/portfolio_construction.cpp"))

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"source-outside-authority"}


def test_portfolio_target_cannot_be_mutated_outside_owner(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\ntarget_link_libraries(chronos_portfolio PRIVATE chronos_risk)\n"
        + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_dynamic_portfolio_target_mutation_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n"
        "set(PORTFOLIO_TARGET chronos_portfolio)\n"
        'target_link_libraries("${PORTFOLIO_TARGET}" PRIVATE chronos_risk)\n' + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_missing_portfolio_owner_registration_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "core/CMakeLists.txt").write_text("", encoding="utf-8")

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"authority-owner-not-registered"}


def test_owner_requires_one_direct_static_target_declaration(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        "add_library(chronos_portfolio SHARED src/portfolio_construction.cpp)\n"
        "target_link_libraries(chronos_portfolio PUBLIC chronos_contracts "
        "chronos_recommendation PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"invalid-owner-target-declaration"}


def test_owner_rejects_duplicate_direct_target_declarations(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        "add_library(chronos_portfolio STATIC src/portfolio_construction.cpp)\n"
        "add_library(chronos_portfolio STATIC src/portfolio_construction.cpp)\n"
        "target_link_libraries(chronos_portfolio PUBLIC chronos_contracts "
        "chronos_recommendation PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"invalid-owner-target-declaration"}


def test_target_sources_cannot_replace_direct_target_declaration(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        "target_sources(chronos_portfolio PRIVATE src/portfolio_construction.cpp)\n"
        "target_link_libraries(chronos_portfolio PUBLIC chronos_contracts "
        "chronos_recommendation PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"invalid-owner-target-declaration"}


def test_owner_target_declaration_cannot_be_hidden_in_helper(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        "function(create_portfolio)\n"
        "  add_library(chronos_portfolio STATIC src/portfolio_construction.cpp)\n"
        "endfunction()\n"
        "target_link_libraries(chronos_portfolio PUBLIC chronos_contracts "
        "chronos_recommendation PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {
        "authority-source-not-declared",
        "invalid-owner-target-declaration",
        "opaque-owner-target-mutation",
    }


def test_owner_target_declaration_cannot_be_hidden_in_conditional_block(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        "if(FALSE)\n"
        "  add_library(chronos_portfolio STATIC src/portfolio_construction.cpp)\n"
        "endif()\n"
        "target_link_libraries(chronos_portfolio PUBLIC chronos_contracts "
        "chronos_recommendation PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {
        "authority-source-not-declared",
        "invalid-owner-target-declaration",
    }


def test_target_cannot_be_created_outside_owner(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\nadd_library(chronos_portfolio STATIC elsewhere.cpp)\n"
        + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"target-created-outside-owner"}


def test_nonliteral_and_obscured_includes_are_rejected(tmp_path: Path) -> None:
    write_portfolio_source(
        tmp_path,
        "#include PORTFOLIO_HEADER\n"
        '#include/**/"chronos/core/risk/risk_authority.hpp"\n'
        "#incl\\\nude <filesystem>\n",
    )
    write_portfolio_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {
        "chronos/core/risk/risk_authority.hpp",
        "filesystem",
        "nonliteral-include",
    }


def test_include_text_in_comments_is_ignored(tmp_path: Path) -> None:
    write_portfolio_source(
        tmp_path,
        "// #include PORTFOLIO_HEADER\\\n/* #include <filesystem> */\n",
    )
    write_portfolio_owner(tmp_path, valid_owner_cmake())

    assert boundary.find_violations(tmp_path) == []


def test_owner_helper_cannot_attach_hidden_external_source(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        valid_owner_cmake() + "function(attach_source target)\n"
        "  target_sources(${target} PRIVATE ../risk/risk_authority.cpp)\n"
        "endfunction()\n"
        "attach_source(chronos_portfolio)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"opaque-owner-target-mutation"}


def test_owner_rejects_property_based_target_mutation(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        valid_owner_cmake() + "set_property(TARGET chronos_portfolio APPEND PROPERTY "
        "LINK_LIBRARIES chronos_risk)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"unsupported-owner-target-mutation"}


def test_composed_dynamic_external_target_mutation_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "string(CONCAT protected chronos_ portfolio)\n"
        "add_dep(${protected} chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_helper_composed_dynamic_target_mutation_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "function(mutate_portfolio)\n"
        "  string(CONCAT protected chronos_ portfolio)\n"
        "  add_dep(${protected} chronos_risk)\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "mutate_portfolio()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_invoked_helper_cannot_mutate_portfolio_target(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "add_dep(chronos_portfolio chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_nested_argv_helper_cannot_mutate_portfolio_target(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep)\n"
        "  target_link_libraries(${ARGV0} PRIVATE ${ARGV1})\n"
        "endfunction()\n"
        "function(wrap target dependency)\n"
        "  add_dep(${target} ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "wrap(chronos_portfolio chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_quoted_owner_target_cannot_hide_forbidden_link(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        valid_owner_cmake()
        .replace(
            "target_link_libraries(chronos_portfolio",
            'target_link_libraries("chronos_portfolio"',
        )
        .replace("chronos_warnings)", "chronos_warnings chronos_risk)"),
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"chronos_risk"}


def test_bracket_quoted_owner_target_cannot_hide_external_source(tmp_path: Path) -> None:
    outside = tmp_path / "core/risk/risk_authority.cpp"
    outside.parent.mkdir(parents=True)
    outside.write_text("#include <cstdint>\n", encoding="utf-8")
    write_portfolio_owner(
        tmp_path,
        "add_library([[chronos_portfolio]] STATIC)\n"
        "target_sources([[chronos_portfolio]] PRIVATE ../risk/risk_authority.cpp)\n"
        "target_link_libraries([[chronos_portfolio]] PUBLIC chronos_contracts "
        "chronos_recommendation PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"source-outside-authority"}


def test_undeclared_portfolio_source_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    extra = tmp_path / "core/portfolio/src/extra.cpp"
    extra.write_text(
        '#include "chronos/core/portfolio/portfolio_construction.hpp"\n', encoding="utf-8"
    )
    write_portfolio_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"authority-source-not-declared"}


def test_generated_portfolio_native_sources_are_not_scanned(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    generated = tmp_path / "core/portfolio/generated"
    generated.mkdir(parents=True)
    (generated / "CMakeCache.txt").write_text("", encoding="utf-8")
    (generated / "generated.cpp").write_text(
        '#include "chronos/core/risk/risk_authority.hpp"\n', encoding="utf-8"
    )
    write_portfolio_owner(tmp_path, valid_owner_cmake())

    assert boundary.find_violations(tmp_path) == []


def test_dead_registration_helper_does_not_register_portfolio_owner(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "core/CMakeLists.txt").write_text(
        "function(register_portfolio)\n  add_subdirectory(portfolio)\nendfunction()\n",
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"authority-owner-not-registered"}


def test_conditional_registration_does_not_register_portfolio_owner(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "core/CMakeLists.txt").write_text(
        "if(FALSE)\n  add_subdirectory(portfolio)\nendif()\n",
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"authority-owner-not-registered"}


def test_owner_cannot_invoke_external_target_mutating_helper(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    helper = tmp_path / "cmake/PortfolioHelpers.cmake"
    helper.parent.mkdir(parents=True)
    helper.write_text(
        "function(attach_external_source target)\n"
        "  target_sources(${target} PRIVATE ../risk/risk_authority.cpp)\n"
        "endfunction()\n"
        "function(wrap_external_source target)\n"
        "  attach_external_source(${target})\n"
        "endfunction()\n",
        encoding="utf-8",
    )
    write_portfolio_owner(
        tmp_path,
        valid_owner_cmake() + "include(${CMAKE_SOURCE_DIR}/cmake/PortfolioHelpers.cmake)\n"
        "wrap_external_source(chronos_portfolio)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"opaque-owner-target-mutation"}


def test_append_composed_target_passed_to_helper_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "set(protected chronos_)\n"
        "string(APPEND protected portfolio)\n"
        "add_dep(${protected} chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_multistep_variable_target_passed_to_transitive_helper_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "function(wrap_dep target dependency)\n"
        "  add_dep(${target} ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "set(prefix chronos_)\n"
        "set(protected ${prefix})\n"
        "string(APPEND protected portfolio)\n"
        "wrap_dep(${protected} chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_nonleading_variable_target_passed_to_helper_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep dependency target)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "set(protected chronos_)\n"
        "string(APPEND protected portfolio)\n"
        "add_dep(chronos_risk ${protected})\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_direct_target_can_link_portfolio_as_dependency(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n"
        "add_library(chronos_consumer STATIC consumer.cpp)\n"
        "target_link_libraries(chronos_consumer PRIVATE chronos_portfolio)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    assert boundary.find_violations(tmp_path) == []


def test_root_cache_does_not_hide_tracked_portfolio_sources(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path, '#include "chronos/core/risk/risk_authority.hpp"\n')
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeCache.txt").write_text("", encoding="utf-8")

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"chronos/core/risk/risk_authority.hpp"}


def test_owner_helper_invocation_in_conditional_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    helper = tmp_path / "cmake/PortfolioHelpers.cmake"
    helper.parent.mkdir(parents=True)
    helper.write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n",
        encoding="utf-8",
    )
    write_portfolio_owner(
        tmp_path,
        valid_owner_cmake() + "if(TRUE)\n  add_dep(chronos_portfolio chronos_risk)\nendif()\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"opaque-owner-target-mutation"}


def test_external_helper_invocation_in_conditional_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "if(TRUE)\n"
        "  add_dep(chronos_portfolio chronos_risk)\n"
        "endif()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_transitive_composed_helper_invocation_in_loop_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "function(wrap_dep target dependency)\n"
        "  add_dep(${target} ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "set(protected chronos_)\n"
        "string(APPEND protected portfolio)\n"
        "foreach(item IN ITEMS one)\n"
        "  wrap_dep(${protected} chronos_risk)\n"
        "endforeach()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_fixed_unrelated_target_helper_is_accepted(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(configure_consumer)\n"
        "  target_link_libraries(chronos_consumer PRIVATE chronos_portfolio)\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "configure_consumer()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    assert boundary.find_violations(tmp_path) == []


def test_fixed_portfolio_target_helper_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(configure_portfolio)\n"
        "  target_link_libraries(chronos_portfolio PRIVATE chronos_risk)\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "configure_portfolio()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}
