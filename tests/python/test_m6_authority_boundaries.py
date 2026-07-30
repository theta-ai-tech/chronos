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
