import importlib.util
import sys
from pathlib import Path


def load_tool():
    path = (
        Path(__file__).resolve().parents[2]
        / "tools/development/verify_m6_risk_authority_boundaries.py"
    )
    spec = importlib.util.spec_from_file_location("verify_m6_risk_authority_boundaries", path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


boundary = load_tool()

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
NATIVE_GUARD_PATH = Path("core/risk/AssertTargetBoundary.cmake")
NATIVE_GUARD_TAIL = (
    "include(core/risk/AssertTargetBoundary.cmake)\n"
    "include(core/portfolio/AssertTargetBoundary.cmake)\n"
)


def write_risk_owner(root: Path, cmake: str) -> None:
    header = root / "core/risk/include/chronos/core/risk/risk_decision.hpp"
    header.parent.mkdir(parents=True)
    header.write_text("#pragma once\n", encoding="utf-8")
    owner = root / "core/risk/CMakeLists.txt"
    owner.parent.mkdir(parents=True, exist_ok=True)
    owner.write_text(cmake, encoding="utf-8")
    guard = root / NATIVE_GUARD_PATH
    guard.write_text(
        (REPOSITORY_ROOT / NATIVE_GUARD_PATH).read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    (root / "core/CMakeLists.txt").write_text("add_subdirectory(risk)\n", encoding="utf-8")
    (root / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n" + NATIVE_GUARD_TAIL, encoding="utf-8"
    )


def write_risk_source(root: Path, contents: str = "") -> None:
    source = root / "core/risk/src/risk_decision.cpp"
    source.parent.mkdir(parents=True)
    source.write_text(
        '#include "chronos/core/risk/risk_decision.hpp"\n' + contents,
        encoding="utf-8",
    )
    (source.parent / "risk_arithmetic.hpp").write_text("#pragma once\n", encoding="utf-8")


def valid_owner_cmake(
    source: str = "src/risk_decision.cpp src/risk_arithmetic.hpp",
) -> str:
    return (
        f"add_library(chronos_risk STATIC {source})\n"
        "target_link_libraries(chronos_risk PUBLIC chronos_contracts "
        "chronos_portfolio PRIVATE chronos_options chronos_warnings)\n"
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
    write_risk_source(root)
    owner_cmake = (REPOSITORY_ROOT / "core/risk/CMakeLists.txt").read_text(encoding="utf-8")
    write_risk_owner(root, owner_before_target + owner_cmake + owner_before_guard)
    portfolio_source = root / "core/portfolio/src/portfolio_construction.cpp"
    portfolio_source.parent.mkdir(parents=True)
    portfolio_source.write_text("int portfolio_fixture() { return 0; }\n", encoding="utf-8")
    portfolio_owner = root / "core/portfolio/CMakeLists.txt"
    portfolio_owner.write_text(
        "add_library(chronos_portfolio STATIC src/portfolio_construction.cpp)\n"
        "target_include_directories(chronos_portfolio PUBLIC "
        "${CMAKE_CURRENT_SOURCE_DIR}/include)\n"
        "target_link_libraries(chronos_portfolio PUBLIC chronos_contracts "
        "chronos_recommendation PRIVATE chronos_options chronos_warnings)\n",
        encoding="utf-8",
    )
    portfolio_guard = root / "core/portfolio/AssertTargetBoundary.cmake"
    portfolio_guard.write_text(
        (REPOSITORY_ROOT / "core/portfolio/AssertTargetBoundary.cmake").read_text(encoding="utf-8"),
        encoding="utf-8",
    )
    (root / "core/CMakeLists.txt").write_text(
        core_before_owner + "add_subdirectory(portfolio)\nadd_subdirectory(risk)\n",
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


def test_generated_path_scan_normalizes_logical_root_alias(tmp_path: Path) -> None:
    actual_root = tmp_path / "actual"
    source = actual_root / "core/risk/src/risk_decision.cpp"
    source.parent.mkdir(parents=True)
    source.write_text("", encoding="utf-8")
    logical_root = tmp_path / "logical"
    logical_root.symlink_to(actual_root, target_is_directory=True)

    assert not boundary.is_generated_path(source.resolve(), logical_root)


def test_native_guard_accepts_exact_target_and_leaves_no_build_tree(tmp_path: Path) -> None:
    write_native_cmake_project(tmp_path)

    assert boundary.find_violations(tmp_path) == []
    assert list(tmp_path.rglob("CMakeCache.txt")) == []


def test_native_guard_accepts_canonical_risk_sources_in_either_declaration_order(
    tmp_path: Path,
) -> None:
    write_native_cmake_project(tmp_path)
    owner = tmp_path / "core/risk/CMakeLists.txt"
    owner.write_text(
        owner.read_text(encoding="utf-8").replace(
            "src/risk_decision.cpp\n  src/risk_arithmetic.hpp",
            "src/risk_arithmetic.hpp\n  src/risk_decision.cpp",
            1,
        ),
        encoding="utf-8",
    )

    assert boundary.find_violations(tmp_path) == []


def test_included_directory_mutations_in_root_core_and_owner_are_rejected(
    tmp_path: Path,
) -> None:
    mutations = (
        ("link_libraries(chronos_recommendation)\n", "LINK_LIBRARIES"),
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
            "cmake_language(CALL add_scoped_link chronos_recommendation)\n"
        ),
    )

    assert configured_properties(tmp_path) == {"LINK_LIBRARIES"}


def test_callable_local_module_path_cannot_select_external_mutation(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    external = tmp_path / "external"
    external.mkdir()
    (external / "RiskMutation.cmake").write_text(
        "link_libraries(chronos_recommendation)\n",
        encoding="utf-8",
    )
    write_native_cmake_project(
        root,
        root_before_core=(
            "function(configure_risk)\n"
            f'  list(PREPEND CMAKE_MODULE_PATH "{external}")\n'
            "  include(RiskMutation)\n"
            "endfunction()\n"
            "configure_risk()\n"
        ),
    )

    assert configured_properties(root) == {"LINK_LIBRARIES"}


def test_escaped_parenthesis_cannot_truncate_boundary_check(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_preamble="set(escaped_parenthesis value\\()\nlink_libraries(chronos_recommendation)\n",
    )

    assert configured_properties(tmp_path) == {"LINK_LIBRARIES"}


def test_unquoted_list_expansion_cannot_hide_dispatched_mutation(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core=(
            "function(add_scoped_links)\n"
            "  link_libraries(${ARGV})\n"
            "endfunction()\n"
            "set(scoped_links chronos_recommendation chronos_execution)\n"
            "add_scoped_links(${scoped_links})\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"LINK_LIBRARIES"}


def test_direct_include_and_compile_capabilities_are_rejected(tmp_path: Path) -> None:
    mutations = (
        (
            "target_include_directories(chronos_risk PRIVATE /unexpected)\n",
            "INCLUDE_DIRECTORIES",
        ),
        (
            "target_compile_definitions(chronos_risk PRIVATE UNEXPECTED)\n",
            "COMPILE_DEFINITIONS",
        ),
        ("target_compile_features(chronos_risk PRIVATE cxx_std_23)\n", "COMPILE_FEATURES"),
        ("target_compile_options(chronos_risk PRIVATE -fno-exceptions)\n", "COMPILE_OPTIONS"),
    )
    for index, (mutation, expected_property) in enumerate(mutations):
        root = tmp_path / str(index)
        write_native_cmake_project(root, owner_before_guard=mutation)

        assert configured_properties(root, configure_directly=True) == {expected_property}


def test_owner_directory_compile_definition_is_rejected(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        owner_before_guard="add_compile_definitions(BOUNDARY_BYPASS)\n",
    )

    assert configured_properties(tmp_path, configure_directly=True) == {
        "DIRECTORY_COMPILE_DEFINITIONS"
    }


def test_owner_directory_compile_and_include_variants_are_rejected(tmp_path: Path) -> None:
    mutations = (
        ("add_compile_options(-fno-exceptions)\n", "DIRECTORY_COMPILE_OPTIONS"),
        ("include_directories(/unexpected)\n", "DIRECTORY_INCLUDE_DIRECTORIES"),
    )
    for index, (mutation, expected_property) in enumerate(mutations):
        root = tmp_path / str(index)
        write_native_cmake_project(root, owner_before_guard=mutation)

        assert configured_properties(root, configure_directly=True) == {expected_property}


def test_target_custom_command_is_rejected(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        owner_before_guard=(
            "add_custom_command(TARGET chronos_risk POST_BUILD\n"
            "  COMMAND ${CMAKE_COMMAND} -E echo BOUNDARY_BYPASS)\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"CUSTOM_COMMAND"}


def test_dynamic_and_callable_target_custom_commands_are_rejected(tmp_path: Path) -> None:
    mutations = (
        (
            "function(attach_custom_command target)\n"
            "  add_custom_command(TARGET ${target} POST_BUILD\n"
            "    COMMAND ${CMAKE_COMMAND} -E echo BOUNDARY_BYPASS)\n"
            "endfunction()\n"
            "attach_custom_command(chronos_risk)\n"
        ),
        (
            "function(add_custom_command)\nendfunction()\n"
            "set(custom_command _add_custom_command)\n"
            "set(risk_target chronos_ris)\n"
            "string(APPEND risk_target k)\n"
            "cmake_language(CALL ${custom_command} TARGET ${risk_target} POST_BUILD\n"
            "  COMMAND ${CMAKE_COMMAND} -E echo BOUNDARY_BYPASS)\n"
        ),
    )
    for index, mutation in enumerate(mutations):
        root = tmp_path / str(index)
        write_native_cmake_project(root, owner_before_guard=mutation)

        assert configured_properties(root, configure_directly=True) == {"CUSTOM_COMMAND"}


def test_external_module_cannot_inject_sources_or_consumer_direct_links(
    tmp_path: Path,
) -> None:
    mutations = (
        (
            "set_property(TARGET chronos_risk PROPERTY "
            "INTERFACE_LINK_LIBRARIES_DIRECT chronos_risk)\n",
            "INTERFACE_LINK_LIBRARIES_DIRECT",
        ),
        (
            "set_property(TARGET chronos_risk PROPERTY "
            "INTERFACE_LINK_LIBRARIES_DIRECT_EXCLUDE chronos_contracts)\n",
            "INTERFACE_LINK_LIBRARIES_DIRECT_EXCLUDE",
        ),
        (
            'target_sources(chronos_risk PRIVATE "${CMAKE_CURRENT_LIST_DIR}/external.cpp")\n',
            "SOURCES",
        ),
        (
            'target_sources(chronos_risk INTERFACE "${CMAKE_CURRENT_LIST_DIR}/external.cpp")\n',
            "INTERFACE_SOURCES",
        ),
    )
    for index, (mutation, expected_property) in enumerate(mutations):
        root = tmp_path / str(index)
        external = root / "external"
        external.mkdir(parents=True)
        (external / "external.cpp").write_text("int external() { return 0; }\n", encoding="utf-8")
        module = external / "RiskMutation.cmake"
        module.write_text(mutation, encoding="utf-8")
        write_native_cmake_project(
            root,
            root_before_core="add_library(chronos_recommendation INTERFACE)\n",
            owner_before_guard=f'include("{module}")\n',
            root_after_core=(
                "add_library(chronos_consumer STATIC consumer.cpp)\n"
                "target_link_libraries(chronos_consumer PRIVATE chronos_risk)\n"
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
        "set(risk_target chronos_risk)\n"
        "attach_dependency(${risk_target} chronos_risk)\n",
        encoding="utf-8",
    )
    write_native_cmake_project(
        root,
        root_before_core="add_library(chronos_recommendation INTERFACE)\n",
        root_after_core="add_subdirectory(late)\n",
    )

    assert configured_properties(root, configure_directly=True) == {"LINK_LIBRARIES"}


def test_source_file_compile_definition_mutation_is_rejected(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    source = root / "core/risk/src/risk_decision.cpp"
    write_native_cmake_project(
        root,
        root_after_core=(
            f'set_source_files_properties("{source}"\n'
            "  TARGET_DIRECTORY chronos_risk\n"
            "  PROPERTIES COMPILE_DEFINITIONS REVIEWER_BYPASS)\n"
        ),
    )

    assert configured_properties(root, configure_directly=True) == {"SOURCE_COMPILE_DEFINITIONS"}


def test_header_language_mutation_is_rejected(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        owner_before_guard=(
            "set_source_files_properties(src/risk_arithmetic.hpp\n"
            "  TARGET_DIRECTORY chronos_risk\n"
            "  PROPERTIES LANGUAGE CXX)\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"SOURCE_LANGUAGE"}


def test_header_compile_capability_is_rejected(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        owner_before_guard=(
            "set_source_files_properties(src/risk_arithmetic.hpp\n"
            "  TARGET_DIRECTORY chronos_risk\n"
            "  PROPERTIES COMPILE_OPTIONS -fno-exceptions)\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"SOURCE_COMPILE_OPTIONS"}


def test_message_override_cannot_swallow_boundary_failure(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    external = tmp_path / "external"
    external.mkdir()
    module = external / "MessageOverride.cmake"
    module.write_text(
        "function(message)\nendfunction()\nlink_libraries(chronos_recommendation)\n",
        encoding="utf-8",
    )
    write_native_cmake_project(
        root,
        root_before_core=(f'add_library(chronos_recommendation INTERFACE)\ninclude("{module}")\n'),
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
        "link_libraries(chronos_recommendation)\n",
        encoding="utf-8",
    )
    write_native_cmake_project(
        root,
        root_before_core=(f'add_library(chronos_recommendation INTERFACE)\ninclude("{module}")\n'),
    )

    assert configured_properties(root, configure_directly=True) == {"COMMAND_OVERRIDE"}


def test_multi_underscore_guard_critical_function_is_not_interception(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core="function(__message)\nendfunction()\n",
    )

    assert boundary.find_violations(tmp_path) == []


def test_multi_underscore_guard_critical_function_is_not_trace_interception(
    tmp_path: Path,
) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core="function(__message)\nendfunction()\n",
    )

    assert boundary.configured_graph_violations(tmp_path) == []


def test_exact_guard_critical_builtin_aliases_are_rejected(tmp_path: Path) -> None:
    for alias in ("_message", "_include"):
        root = tmp_path / alias
        write_native_cmake_project(
            root,
            root_before_core=f"function({alias})\nendfunction()\n",
        )

        dependencies = {item.dependency for item in boundary.find_violations(root)}
        assert "fatal-command-interception" in dependencies
        assert "configured-target-property:COMMAND_OVERRIDE" in dependencies


def test_deferred_guard_redefinition_cannot_disable_assertion(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core="add_library(chronos_recommendation INTERFACE)\n",
        root_after_core=(
            "target_link_libraries(chronos_risk PRIVATE chronos_risk)\n"
            "function(_chronos_assert_risk_boundary)\nendfunction()\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"LINK_LIBRARIES"}


def test_deferred_guard_cancellation_cannot_disable_assertion(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core="add_library(chronos_recommendation INTERFACE)\n",
        root_after_core=(
            "target_link_libraries(chronos_risk PRIVATE chronos_risk)\n"
            "function(cancel_risk_guard)\n"
            "  cmake_language(DEFER CANCEL_CALL chronos_m6_risk_boundary)\n"
            "endfunction()\n"
            "cmake_language(DEFER CALL cancel_risk_guard)\n"
        ),
    )

    assert configured_properties(tmp_path, configure_directly=True) == {"LINK_LIBRARIES"}


def test_deferred_mutations_after_guard_are_rejected(tmp_path: Path) -> None:
    source = tmp_path / "core/risk/src/risk_decision.cpp"
    write_native_cmake_project(
        tmp_path,
        root_before_core=(
            "target_compile_definitions(chronos_recommendation INTERFACE "
            "DEFERRED_RISK_CAPABILITY)\n"
        ),
        root_after_core=(
            "set(late_target chronos_ris)\n"
            "string(APPEND late_target k)\n"
            f'set(late_source "{source}")\n'
            "cmake_language(DEFER CALL target_link_libraries "
            "${late_target} PRIVATE chronos_recommendation)\n"
            "cmake_language(DEFER CALL set_source_files_properties\n"
            '  "${late_source}"\n'
            '  TARGET_DIRECTORY "${late_target}"\n'
            "  PROPERTIES COMPILE_DEFINITIONS DEFERRED_SOURCE_CAPABILITY)\n"
        ),
    )

    assert configured_properties(tmp_path) == {
        "LINK_LIBRARIES",
        "SOURCE_COMPILE_DEFINITIONS",
    }


def test_deferred_builtin_alias_mutation_after_guards_is_rejected(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_after_core=(
            "function(target_link_libraries)\n"
            "endfunction()\n"
            "set(late_command _target_link_libraries)\n"
            "set(late_target chronos_ris)\n"
            "string(APPEND late_target k)\n"
            "cmake_language(DEFER CALL ${late_command} "
            "${late_target} PRIVATE chronos_recommendation)\n"
        ),
    )

    assert configured_properties(tmp_path) == {"LINK_LIBRARIES"}


def test_double_underscore_function_is_not_a_builtin_alias(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_after_core=(
            "function(__target_link_libraries)\n"
            "endfunction()\n"
            "set(late_command __target_link_libraries)\n"
            "set(late_target chronos_ris)\n"
            "string(APPEND late_target k)\n"
            "cmake_language(DEFER CALL ${late_command} "
            "${late_target} PRIVATE chronos_recommendation)\n"
        ),
    )

    assert boundary.find_violations(tmp_path) == []


def test_successful_configure_requires_guard_execution(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_before_core=(
            "target_compile_definitions(chronos_recommendation INTERFACE "
            "RETURN_BYPASS_CAPABILITY)\n"
        ),
        root_after_core=("return()\n"),
    )

    assert configured_properties(tmp_path) == {"GUARD_NOT_EXECUTED"}


def test_generator_specific_source_mutations_are_rejected(tmp_path: Path) -> None:
    mutations = (
        ("VS_SETTINGS", "ExcludedFromBuild=true"),
        ("XCODE_EXPLICIT_FILE_TYPE", "sourcecode.c.c"),
    )
    for index, (property_name, value) in enumerate(mutations):
        root = tmp_path / str(index)
        source = root / "core/risk/src/risk_decision.cpp"
        write_native_cmake_project(
            root,
            root_after_core=(
                f'set_source_files_properties("{source}"\n'
                "  TARGET_DIRECTORY chronos_risk\n"
                f"  PROPERTIES {property_name} {value})\n"
            ),
        )

        assert configured_properties(root, configure_directly=True) == {f"SOURCE_{property_name}"}


def test_custom_configuration_source_definition_is_rejected(tmp_path: Path) -> None:
    root = tmp_path / "repository"
    source = root / "core/risk/src/risk_decision.cpp"
    write_native_cmake_project(
        root,
        root_before_core='set(CMAKE_BUILD_TYPE Reviewer CACHE STRING "" FORCE)\n',
        root_after_core=(
            f'set_source_files_properties("{source}"\n'
            "  TARGET_DIRECTORY chronos_risk\n"
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
    owner_cmake = (valid / "core/risk/CMakeLists.txt").read_text(encoding="utf-8")
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
    owner_cmake = (tmp_path / "core/risk/CMakeLists.txt").read_text(encoding="utf-8")
    root_cmake = (tmp_path / "CMakeLists.txt").read_text(encoding="utf-8")
    guard_cmake = (tmp_path / NATIVE_GUARD_PATH).read_text(encoding="utf-8")
    root_cmake = root_cmake.replace("project(M6BoundaryFixture LANGUAGES CXX)\n", "", 1)
    root_cmake = root_cmake.replace(NATIVE_GUARD_TAIL, "", 1)

    assert not boundary.has_native_target_guard(owner_cmake, root_cmake, guard_cmake)


def test_missing_project_and_declared_extra_source_fail_closed(tmp_path: Path) -> None:
    write_native_cmake_project(tmp_path)
    extra = tmp_path / "core/risk/src/extra.cpp"
    extra.write_text("int extra() { return 0; }\n", encoding="utf-8")
    owner = tmp_path / "core/risk/CMakeLists.txt"
    owner.write_text(
        owner.read_text(encoding="utf-8").replace(
            "src/risk_arithmetic.hpp)",
            "src/risk_arithmetic.hpp\n  src/extra.cpp)",
            1,
        ),
        encoding="utf-8",
    )
    root_cmake = tmp_path / "CMakeLists.txt"
    root_cmake.write_text(
        root_cmake.read_text(encoding="utf-8").replace(
            "project(M6BoundaryFixture LANGUAGES CXX)\n",
            "",
            1,
        ),
        encoding="utf-8",
    )

    assert {item.dependency for item in boundary.find_violations(tmp_path)} == {
        "configured-target-property:SOURCES",
        "missing-cmake-configure-prerequisite:project",
        "noncanonical-authority-sources",
    }


def test_missing_project_still_checks_post_guard_trace(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_after_core=(
            "set(late_target chronos_ris)\n"
            "string(APPEND late_target k)\n"
            "cmake_language(DEFER CALL target_link_libraries "
            "${late_target} PRIVATE chronos_recommendation)\n"
        ),
    )
    root_cmake = tmp_path / "CMakeLists.txt"
    root_cmake.write_text(
        root_cmake.read_text(encoding="utf-8").replace(
            "project(M6BoundaryFixture LANGUAGES CXX)\n",
            "",
            1,
        ),
        encoding="utf-8",
    )

    assert {item.dependency for item in boundary.find_violations(tmp_path)} == {
        "configured-target-property:LINK_LIBRARIES",
        "missing-cmake-configure-prerequisite:project",
    }


def test_missing_cmake_minimum_still_checks_post_guard_trace(tmp_path: Path) -> None:
    write_native_cmake_project(
        tmp_path,
        root_after_core=(
            "set(late_target chronos_ris)\n"
            "string(APPEND late_target k)\n"
            "cmake_language(DEFER CALL target_link_libraries "
            "${late_target} PRIVATE chronos_recommendation)\n"
        ),
    )
    root_cmake = tmp_path / "CMakeLists.txt"
    root_cmake.write_text(
        root_cmake.read_text(encoding="utf-8").replace(
            "cmake_minimum_required(VERSION 3.24)\n",
            "",
            1,
        ),
        encoding="utf-8",
    )

    assert {item.dependency for item in boundary.find_violations(tmp_path)} == {
        "configured-target-property:LINK_LIBRARIES",
        "missing-cmake-configure-prerequisite:cmake_minimum_required",
    }


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
        'if(NOT "${_chronos_risk_link_libraries}" STREQUAL\n'
        '    "${_chronos_risk_expected_link_libraries}")'
    )
    exact_message = (
        "  message(FATAL_ERROR\n"
        '    "CHRONOS_M6_BOUNDARY_VIOLATION:LINK_LIBRARIES: expected "\n'
        '    "`${_chronos_risk_expected_link_libraries}`, got "\n'
        '    "`${_chronos_risk_link_libraries}`")\n'
    )
    transformations = (
        (
            "get_target_property(\n  _chronos_risk_link_libraries chronos_risk LINK_LIBRARIES)\n",
            "",
        ),
        (exact_condition, "if(FALSE)"),
        (
            exact_condition,
            'if("${_chronos_risk_link_libraries}" STREQUAL\n'
            '   "${_chronos_risk_expected_link_libraries}")',
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
            "target_link_libraries(chronos_consumer PRIVATE chronos_risk)\n"
        ),
    )

    assert boundary.find_violations(tmp_path) == []


def test_current_m6_risk_authority_has_no_forbidden_dependencies() -> None:
    root = Path(__file__).resolve().parents[2]
    assert boundary.find_violations(root) == []


def test_forbidden_risk_include_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path, '#include "chronos/core/recommendation/recommendation.hpp"\n')
    write_risk_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {
        "chronos/core/recommendation/recommendation.hpp"
    }


def test_risk_include_allowlist_rejects_parent_traversal(tmp_path: Path) -> None:
    dependency = "chronos/core/risk/../../risk/risk_authority.hpp"
    write_risk_source(tmp_path, f'#include "{dependency}"\n')
    write_risk_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {dependency}


def test_risk_value_objects_include_is_allowed(tmp_path: Path) -> None:
    write_risk_source(tmp_path, '#include "chronos/contracts/value_objects.hpp"\n')
    write_risk_owner(tmp_path, valid_owner_cmake())

    assert boundary.find_violations(tmp_path) == []


def test_forbidden_risk_link_dependency_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        valid_owner_cmake().replace("chronos_warnings", "chronos_warnings chronos_risk"),
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"chronos_risk"}


def test_owner_directory_scoped_link_mutations_are_rejected(tmp_path: Path) -> None:
    mutations = (
        "link_libraries(chronos_recommendation)\n",
        "link_directories(${CMAKE_SOURCE_DIR}/risk)\n",
        "add_link_options(-Wl,--whole-archive)\n",
    )
    for index, mutation in enumerate(mutations):
        root = tmp_path / str(index)
        write_risk_source(root)
        write_risk_owner(root, mutation + valid_owner_cmake())

        violations = boundary.find_violations(root)

        assert {item.dependency for item in violations} == {"directory-scoped-link-mutation"}


def test_core_ancestor_directory_scoped_link_mutations_are_rejected(
    tmp_path: Path,
) -> None:
    mutations = (
        "link_libraries(chronos_recommendation)\n",
        "link_directories(${CMAKE_SOURCE_DIR}/risk)\n",
        "add_link_options(-Wl,--whole-archive)\n",
    )
    for index, mutation in enumerate(mutations):
        root = tmp_path / str(index)
        write_risk_source(root)
        write_risk_owner(root, valid_owner_cmake())
        (root / "core/CMakeLists.txt").write_text(
            mutation + "add_subdirectory(risk)\n", encoding="utf-8"
        )

        violations = boundary.find_violations(root)

        assert {item.dependency for item in violations} == {"directory-scoped-link-mutation"}


def test_root_ancestor_directory_scoped_link_mutations_are_rejected(
    tmp_path: Path,
) -> None:
    mutations = (
        "link_libraries(chronos_recommendation)\n",
        "link_directories(${CMAKE_SOURCE_DIR}/risk)\n",
        "add_link_options(-Wl,--whole-archive)\n",
    )
    for index, mutation in enumerate(mutations):
        root = tmp_path / str(index)
        write_risk_source(root)
        write_risk_owner(root, valid_owner_cmake())
        (root / "CMakeLists.txt").write_text(
            mutation + "add_subdirectory(core)\n" + NATIVE_GUARD_TAIL,
            encoding="utf-8",
        )

        violations = boundary.find_violations(root)

        assert {item.dependency for item in violations} == {"directory-scoped-link-mutation"}


def test_root_ancestor_directory_scoped_link_helper_is_rejected(
    tmp_path: Path,
) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    helper = tmp_path / "cmake/RiskLinks.cmake"
    helper.parent.mkdir(parents=True)
    helper.write_text(
        "function(add_risk_links)\n  link_libraries(chronos_recommendation)\nendfunction()\n",
        encoding="utf-8",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "include(cmake/RiskLinks.cmake)\nadd_risk_links()\nadd_subdirectory(core)\n"
        + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"directory-scoped-link-mutation"}


def test_risk_source_outside_owner_directory_is_rejected(tmp_path: Path) -> None:
    outside = tmp_path / "core/src/risk_decision.cpp"
    outside.parent.mkdir(parents=True)
    outside.write_text("#include <cstdint>\n", encoding="utf-8")
    write_risk_owner(tmp_path, valid_owner_cmake("../src/risk_decision.cpp"))

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"source-outside-authority"}


def test_risk_target_cannot_be_mutated_outside_owner(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\ntarget_link_libraries(chronos_risk PRIVATE chronos_risk)\n"
        + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_dynamic_risk_target_mutation_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n"
        "set(RISK_TARGET chronos_risk)\n"
        'target_link_libraries("${RISK_TARGET}" PRIVATE chronos_risk)\n' + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_missing_risk_owner_registration_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "core/CMakeLists.txt").write_text("", encoding="utf-8")

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"authority-owner-not-registered"}


def test_owner_requires_one_direct_static_target_declaration(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        "add_library(chronos_risk SHARED src/risk_decision.cpp src/risk_arithmetic.hpp)\n"
        "target_link_libraries(chronos_risk PUBLIC chronos_contracts "
        "chronos_portfolio PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"invalid-owner-target-declaration"}


def test_owner_rejects_duplicate_direct_target_declarations(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        "add_library(chronos_risk STATIC src/risk_decision.cpp src/risk_arithmetic.hpp)\n"
        "add_library(chronos_risk STATIC src/risk_decision.cpp src/risk_arithmetic.hpp)\n"
        "target_link_libraries(chronos_risk PUBLIC chronos_contracts "
        "chronos_portfolio PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"invalid-owner-target-declaration"}


def test_target_sources_cannot_replace_direct_target_declaration(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        "target_sources(chronos_risk PRIVATE src/risk_decision.cpp src/risk_arithmetic.hpp)\n"
        "target_link_libraries(chronos_risk PUBLIC chronos_contracts "
        "chronos_portfolio PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"invalid-owner-target-declaration"}


def test_owner_target_declaration_cannot_be_hidden_in_helper(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        "function(create_risk)\n"
        "  add_library(chronos_risk STATIC src/risk_decision.cpp)\n"
        "endfunction()\n"
        "target_link_libraries(chronos_risk PUBLIC chronos_contracts "
        "chronos_portfolio PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {
        "authority-source-not-declared",
        "invalid-owner-target-declaration",
        "opaque-owner-target-mutation",
    }


def test_owner_target_declaration_cannot_be_hidden_in_conditional_block(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        "if(FALSE)\n"
        "  add_library(chronos_risk STATIC src/risk_decision.cpp)\n"
        "endif()\n"
        "target_link_libraries(chronos_risk PUBLIC chronos_contracts "
        "chronos_portfolio PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {
        "authority-source-not-declared",
        "invalid-owner-target-declaration",
    }


def test_target_cannot_be_created_outside_owner(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\nadd_library(chronos_risk STATIC elsewhere.cpp)\n"
        + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"target-created-outside-owner"}


def test_nonliteral_and_obscured_includes_are_rejected(tmp_path: Path) -> None:
    write_risk_source(
        tmp_path,
        "#include RISK_HEADER\n"
        '#include/**/"chronos/core/recommendation/recommendation.hpp"\n'
        "#incl\\\nude <filesystem>\n",
    )
    write_risk_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {
        "chronos/core/recommendation/recommendation.hpp",
        "filesystem",
        "nonliteral-include",
    }


def test_include_text_in_comments_is_ignored(tmp_path: Path) -> None:
    write_risk_source(
        tmp_path,
        "// #include RISK_HEADER\\\n/* #include <filesystem> */\n",
    )
    write_risk_owner(tmp_path, valid_owner_cmake())

    assert boundary.find_violations(tmp_path) == []


def test_owner_helper_cannot_attach_hidden_external_source(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        valid_owner_cmake() + "function(attach_source target)\n"
        "  target_sources(${target} PRIVATE ../recommendation/recommendation.cpp)\n"
        "endfunction()\n"
        "attach_source(chronos_risk)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"opaque-owner-target-mutation"}


def test_owner_rejects_property_based_target_mutation(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        valid_owner_cmake() + "set_property(TARGET chronos_risk APPEND PROPERTY "
        "LINK_LIBRARIES chronos_risk)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"unsupported-owner-target-mutation"}


def test_composed_dynamic_external_target_mutation_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "string(CONCAT protected chronos_ risk)\n"
        "add_dep(${protected} chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_helper_composed_dynamic_target_mutation_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "function(mutate_risk)\n"
        "  string(CONCAT protected chronos_ risk)\n"
        "  add_dep(${protected} chronos_risk)\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "mutate_risk()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_invoked_helper_cannot_mutate_risk_target(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "add_dep(chronos_risk chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_nested_argv_helper_cannot_mutate_risk_target(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep)\n"
        "  target_link_libraries(${ARGV0} PRIVATE ${ARGV1})\n"
        "endfunction()\n"
        "function(wrap target dependency)\n"
        "  add_dep(${target} ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "wrap(chronos_risk chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_quoted_owner_target_cannot_hide_forbidden_link(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(
        tmp_path,
        valid_owner_cmake()
        .replace(
            "target_link_libraries(chronos_risk",
            'target_link_libraries("chronos_risk"',
        )
        .replace("chronos_warnings)", "chronos_warnings chronos_risk)"),
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"chronos_risk"}


def test_bracket_quoted_owner_target_cannot_hide_external_source(tmp_path: Path) -> None:
    outside = tmp_path / "core/risk/risk_authority.cpp"
    outside.parent.mkdir(parents=True)
    outside.write_text("#include <cstdint>\n", encoding="utf-8")
    write_risk_owner(
        tmp_path,
        "add_library([[chronos_risk]] STATIC)\n"
        "target_sources([[chronos_risk]] PRIVATE ../recommendation/recommendation.cpp)\n"
        "target_link_libraries([[chronos_risk]] PUBLIC chronos_contracts "
        "chronos_portfolio PRIVATE chronos_options chronos_warnings)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"source-outside-authority"}


def test_undeclared_risk_source_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    extra = tmp_path / "core/risk/src/extra.cpp"
    extra.write_text('#include "chronos/core/risk/risk_decision.hpp"\n', encoding="utf-8")
    write_risk_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"authority-source-not-declared"}


def test_generated_risk_native_sources_are_not_scanned(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    generated = tmp_path / "core/risk/generated"
    generated.mkdir(parents=True)
    (generated / "CMakeCache.txt").write_text("", encoding="utf-8")
    (generated / "generated.cpp").write_text(
        '#include "chronos/core/recommendation/recommendation.hpp"\n', encoding="utf-8"
    )
    write_risk_owner(tmp_path, valid_owner_cmake())

    assert boundary.find_violations(tmp_path) == []


def test_dead_registration_helper_does_not_register_risk_owner(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "core/CMakeLists.txt").write_text(
        "function(register_risk)\n  add_subdirectory(risk)\nendfunction()\n",
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"authority-owner-not-registered"}


def test_conditional_registration_does_not_register_risk_owner(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "core/CMakeLists.txt").write_text(
        "if(FALSE)\n  add_subdirectory(risk)\nendif()\n",
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"authority-owner-not-registered"}


def test_owner_cannot_invoke_external_target_mutating_helper(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    helper = tmp_path / "cmake/RiskHelpers.cmake"
    helper.parent.mkdir(parents=True)
    helper.write_text(
        "function(attach_external_source target)\n"
        "  target_sources(${target} PRIVATE ../recommendation/recommendation.cpp)\n"
        "endfunction()\n"
        "function(wrap_external_source target)\n"
        "  attach_external_source(${target})\n"
        "endfunction()\n",
        encoding="utf-8",
    )
    write_risk_owner(
        tmp_path,
        valid_owner_cmake() + "include(${CMAKE_SOURCE_DIR}/cmake/RiskHelpers.cmake)\n"
        "wrap_external_source(chronos_risk)\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"opaque-owner-target-mutation"}


def test_append_composed_target_passed_to_helper_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "set(protected chronos_)\n"
        "string(APPEND protected risk)\n"
        "add_dep(${protected} chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_multistep_variable_target_passed_to_transitive_helper_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
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
        "string(APPEND protected risk)\n"
        "wrap_dep(${protected} chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_nonleading_variable_target_passed_to_helper_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep dependency target)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "set(protected chronos_)\n"
        "string(APPEND protected risk)\n"
        "add_dep(chronos_risk ${protected})\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_direct_target_can_link_risk_as_dependency(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n"
        "add_library(chronos_consumer STATIC consumer.cpp)\n"
        "target_link_libraries(chronos_consumer PRIVATE chronos_risk)\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    assert boundary.find_violations(tmp_path) == []


def test_root_cache_does_not_hide_tracked_risk_sources(tmp_path: Path) -> None:
    write_risk_source(tmp_path, '#include "chronos/core/recommendation/recommendation.hpp"\n')
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeCache.txt").write_text("", encoding="utf-8")

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {
        "chronos/core/recommendation/recommendation.hpp"
    }


def test_owner_helper_invocation_in_conditional_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    helper = tmp_path / "cmake/RiskHelpers.cmake"
    helper.parent.mkdir(parents=True)
    helper.write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n",
        encoding="utf-8",
    )
    write_risk_owner(
        tmp_path,
        valid_owner_cmake() + "if(TRUE)\n  add_dep(chronos_risk chronos_risk)\nendif()\n",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"opaque-owner-target-mutation"}


def test_external_helper_invocation_in_conditional_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "if(TRUE)\n"
        "  add_dep(chronos_risk chronos_risk)\n"
        "endif()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_transitive_composed_helper_invocation_in_loop_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dependency)\n"
        "  target_link_libraries(${target} PRIVATE ${dependency})\n"
        "endfunction()\n"
        "function(wrap_dep target dependency)\n"
        "  add_dep(${target} ${dependency})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "set(protected chronos_)\n"
        "string(APPEND protected risk)\n"
        "foreach(item IN ITEMS one)\n"
        "  wrap_dep(${protected} chronos_risk)\n"
        "endforeach()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_fixed_unrelated_target_helper_is_accepted(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(configure_consumer)\n"
        "  target_link_libraries(chronos_consumer PRIVATE chronos_risk)\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "configure_consumer()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    assert boundary.find_violations(tmp_path) == []


def test_fixed_risk_target_helper_is_rejected(tmp_path: Path) -> None:
    write_risk_source(tmp_path)
    write_risk_owner(tmp_path, valid_owner_cmake())
    (tmp_path / "CMakeLists.txt").write_text(
        "function(configure_risk)\n"
        "  target_link_libraries(chronos_risk PRIVATE chronos_risk)\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "configure_risk()\n" + NATIVE_GUARD_TAIL,
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}
