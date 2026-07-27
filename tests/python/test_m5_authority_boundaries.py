import importlib.util
import sys
from pathlib import Path


def load_tool():
    path = (
        Path(__file__).resolve().parents[2] / "tools/development/verify_m5_authority_boundaries.py"
    )
    spec = importlib.util.spec_from_file_location("verify_m5_authority_boundaries", path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


boundary = load_tool()


def write_other_authorities(root: Path) -> None:
    strategy = root / "runtime/strategies/src/strategy_evaluation.cpp"
    strategy.parent.mkdir(parents=True)
    strategy.write_text(
        '#include "chronos/runtime/strategies/strategy_evaluation.hpp"\n',
        encoding="utf-8",
    )
    (root / "runtime/strategies/CMakeLists.txt").write_text(
        "add_library(chronos_strategy_runtime STATIC src/strategy_evaluation.cpp)\n",
        encoding="utf-8",
    )
    recommendation = root / "core/recommendation/src/recommendation.cpp"
    recommendation.parent.mkdir(parents=True)
    recommendation.write_text(
        '#include "chronos/core/recommendation/recommendation.hpp"\n',
        encoding="utf-8",
    )
    (root / "core/recommendation/CMakeLists.txt").write_text(
        "add_library(chronos_recommendation STATIC src/recommendation.cpp)\n",
        encoding="utf-8",
    )


def write_feature_owner(root: Path, cmake: str) -> None:
    feature_header = root / "core/include/chronos/core/features/feature_runtime.hpp"
    feature_header.parent.mkdir(parents=True)
    feature_header.write_text("#pragma once\n", encoding="utf-8")
    feature_cmake = root / "core/features/CMakeLists.txt"
    feature_cmake.parent.mkdir(parents=True, exist_ok=True)
    feature_cmake.write_text(cmake, encoding="utf-8")
    (root / "core/CMakeLists.txt").write_text("add_subdirectory(features)\n", encoding="utf-8")
    (root / "CMakeLists.txt").write_text("add_subdirectory(core)\n", encoding="utf-8")


def test_current_m5_authorities_have_no_downstream_capabilities() -> None:
    root = Path(__file__).resolve().parents[2]
    assert boundary.find_violations(root) == []


def test_downstream_include_is_rejected(tmp_path: Path) -> None:
    feature = tmp_path / "core/features/src/feature_observation.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text(
        '#include "chronos/adapters/execution/order_gateway.hpp"\n', encoding="utf-8"
    )
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC src/feature_observation.cpp)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n",
    )
    write_other_authorities(tmp_path)
    strategy = tmp_path / "runtime/strategies/src/strategy_evaluation.cpp"
    strategy.write_text(
        '#include "chronos/core/risk/risk_authority.hpp"\n#include <filesystem>\n',
        encoding="utf-8",
    )
    (tmp_path / "core/recommendation/CMakeLists.txt").write_text(
        "add_library(chronos_recommendation STATIC src/recommendation.cpp)\n"
        "target_link_libraries(chronos_recommendation PRIVATE chronos_execution)\n",
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {
        "chronos/adapters/execution/order_gateway.hpp",
        "chronos/core/risk/risk_authority.hpp",
        "chronos_execution",
        "filesystem",
    }


def test_only_feature_target_dependencies_are_scanned(tmp_path: Path) -> None:
    feature = tmp_path / "core/features/src/feature_runtime.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text('#include "chronos/core/features/feature_runtime.hpp"\n', encoding="utf-8")
    write_feature_owner(
        tmp_path,
        "ADD_LIBRARY(chronos_features STATIC src/feature_runtime.cpp)\n"
        "add_subdirectory(execution_planning)\n"
        "TARGET_LINK_LIBRARIES(chronos_features PUBLIC chronos_contracts "
        "PRIVATE chronos_risk)\n",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"chronos_risk"}


def test_feature_sources_must_stay_inside_scanned_boundary(tmp_path: Path) -> None:
    feature = tmp_path / "core/src/feature_observation.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text("#include <cstdint>\n", encoding="utf-8")
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC)\n"
        "target_sources(chronos_features PRIVATE ../src/feature_observation.cpp)\n",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"source-outside-authority"}


def test_protected_target_cannot_be_mutated_outside_owner(tmp_path: Path) -> None:
    feature = tmp_path / "core/features/src/feature_runtime.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text('#include "chronos/core/features/feature_runtime.hpp"\n', encoding="utf-8")
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC src/feature_runtime.cpp)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n"
        "set(PROTECTED chronos_features)\n"
        'target_link_libraries("${PROTECTED}" PRIVATE chronos_risk)\n',
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"dynamic-target-mutation"}


def test_invoked_helper_cannot_mutate_protected_target(tmp_path: Path) -> None:
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep target dep)\n"
        "  target_link_libraries(${target} PRIVATE ${dep})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "add_dep(chronos_features chronos_risk)\n",
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_nested_and_argv_helpers_cannot_receive_protected_target(tmp_path: Path) -> None:
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "function(add_dep)\n"
        "  target_link_libraries(${ARGV0} PRIVATE ${ARGV1})\n"
        "endfunction()\n"
        "function(wrap target dep)\n"
        "  add_dep(${target} ${dep})\n"
        "endfunction()\n"
        "add_subdirectory(core)\n"
        "wrap(chronos_features chronos_risk)\n",
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_bracket_quoted_protected_target_cannot_be_mutated(tmp_path: Path) -> None:
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n"
        "target_link_libraries([[chronos_features]] PRIVATE chronos_risk)\n",
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_protected_target_cannot_be_mutated_from_cmake_module(tmp_path: Path) -> None:
    feature = tmp_path / "core/features/src/feature_runtime.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text('#include "chronos/core/features/feature_runtime.hpp"\n', encoding="utf-8")
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC src/feature_runtime.cpp)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n",
    )
    module = tmp_path / "cmake/build_helpers/AuthorityMutations.cmake"
    module.parent.mkdir(parents=True)
    module.write_text(
        "TARGET_LINK_LIBRARIES(chronos_features PRIVATE chronos_risk)\n",
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"protected-target-mutated-outside-owner"}


def test_owner_rejects_property_based_target_mutation(tmp_path: Path) -> None:
    feature = tmp_path / "core/features/src/feature_runtime.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text('#include "chronos/core/features/feature_runtime.hpp"\n', encoding="utf-8")
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC src/feature_runtime.cpp)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n"
        "SET_PROPERTY(TARGET chronos_features APPEND PROPERTY "
        "LINK_LIBRARIES chronos_risk)\n",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"unsupported-owner-target-mutation"}


def test_cmake_comments_are_not_parsed_as_commands(tmp_path: Path) -> None:
    feature = tmp_path / "core/features/src/feature_runtime.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text('#include "chronos/core/features/feature_runtime.hpp"\n', encoding="utf-8")
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC)\n"
        "# target_sources(chronos_features PRIVATE src/feature_runtime.cpp)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n",
    )
    (tmp_path / "CMakeLists.txt").write_text(
        "add_subdirectory(core)\n# target_link_libraries(chronos_features PRIVATE chronos_risk)\n",
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"authority-source-not-declared"}


def test_all_protected_targets_reject_external_sources(tmp_path: Path) -> None:
    feature = tmp_path / "core/features/src/feature_runtime.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text('#include "chronos/core/features/feature_runtime.hpp"\n', encoding="utf-8")
    write_feature_owner(
        tmp_path,
        "add_library(chronos_features STATIC src/feature_runtime.cpp)\n"
        "target_link_libraries(chronos_features PUBLIC chronos_contracts)\n",
    )
    write_other_authorities(tmp_path)
    external = tmp_path / "core/risk/risk_authority.CXX"
    external.parent.mkdir(parents=True)
    external.write_text("#include <filesystem>\n", encoding="utf-8")
    (tmp_path / "runtime/strategies/CMakeLists.txt").write_text(
        "add_library(chronos_strategy_runtime STATIC ../../core/risk/risk_authority.CXX)\n",
        encoding="utf-8",
    )
    (tmp_path / "core/recommendation/CMakeLists.txt").write_text(
        "add_library(chronos_recommendation STATIC ../risk/risk_authority.CXX)\n",
        encoding="utf-8",
    )

    violations = boundary.find_violations(tmp_path)
    outside = [item for item in violations if item.dependency == "source-outside-authority"]
    assert len(outside) == 2
