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


def write_portfolio_owner(root: Path, cmake: str) -> None:
    header = root / "core/portfolio/include/chronos/core/portfolio/portfolio_construction.hpp"
    header.parent.mkdir(parents=True)
    header.write_text("#pragma once\n", encoding="utf-8")
    owner = root / "core/portfolio/CMakeLists.txt"
    owner.parent.mkdir(parents=True, exist_ok=True)
    owner.write_text(cmake, encoding="utf-8")
    (root / "core/CMakeLists.txt").write_text("add_subdirectory(portfolio)\n", encoding="utf-8")
    (root / "CMakeLists.txt").write_text("add_subdirectory(core)\n", encoding="utf-8")


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


def test_current_m6_portfolio_authority_has_no_forbidden_dependencies() -> None:
    root = Path(__file__).resolve().parents[2]
    assert boundary.find_violations(root) == []


def test_forbidden_portfolio_include_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path, '#include "chronos/core/risk/risk_authority.hpp"\n')
    write_portfolio_owner(tmp_path, valid_owner_cmake())

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"chronos/core/risk/risk_authority.hpp"}


def test_forbidden_portfolio_link_dependency_is_rejected(tmp_path: Path) -> None:
    write_portfolio_source(tmp_path)
    write_portfolio_owner(
        tmp_path,
        valid_owner_cmake().replace("chronos_warnings", "chronos_warnings chronos_risk"),
    )

    violations = boundary.find_violations(tmp_path)

    assert {item.dependency for item in violations} == {"chronos_risk"}


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
        "add_subdirectory(core)\ntarget_link_libraries(chronos_portfolio PRIVATE chronos_risk)\n",
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
        'target_link_libraries("${PORTFOLIO_TARGET}" PRIVATE chronos_risk)\n',
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
        "add_subdirectory(core)\nadd_library(chronos_portfolio STATIC elsewhere.cpp)\n",
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
        "add_dep(${protected} chronos_risk)\n",
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
        "mutate_portfolio()\n",
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
        "add_dep(chronos_portfolio chronos_risk)\n",
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
        "wrap(chronos_portfolio chronos_risk)\n",
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
