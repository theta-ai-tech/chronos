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


def test_current_m5_authorities_have_no_downstream_capabilities() -> None:
    root = Path(__file__).resolve().parents[2]
    assert boundary.find_violations(root) == []


def test_downstream_include_is_rejected(tmp_path: Path) -> None:
    feature = tmp_path / "core/features/src/feature_observation.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text(
        '#include "chronos/adapters/execution/order_gateway.hpp"\n', encoding="utf-8"
    )
    feature_header = tmp_path / "core/include/chronos/core/features/feature_runtime.hpp"
    feature_header.parent.mkdir(parents=True)
    feature_header.write_text(
        '#include "chronos/core/features/feature_runtime.hpp"\n', encoding="utf-8"
    )
    (tmp_path / "core/CMakeLists.txt").write_text(
        "add_library(chronos_core STATIC features/src/feature_observation.cpp)\n"
        "add_subdirectory(risk)\n",
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)
    strategy = tmp_path / "runtime/strategies/src/strategy_evaluation.cpp"
    strategy.write_text(
        '#include "chronos/core/risk/risk_authority.hpp"\n#include <filesystem>\n',
        encoding="utf-8",
    )
    (tmp_path / "core/recommendation/CMakeLists.txt").write_text(
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
    feature_header = tmp_path / "core/include/chronos/core/features/feature_runtime.hpp"
    feature_header.parent.mkdir(parents=True)
    feature_header.write_text("#pragma once\n", encoding="utf-8")
    (tmp_path / "core/CMakeLists.txt").write_text(
        "add_library(chronos_core STATIC features/src/feature_runtime.cpp)\n"
        "add_subdirectory(execution_planning)\n"
        "target_link_libraries(chronos_core PUBLIC chronos_contracts PRIVATE chronos_risk)\n",
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"chronos_risk"}


def test_feature_sources_must_stay_inside_scanned_boundary(tmp_path: Path) -> None:
    feature = tmp_path / "core/src/feature_observation.cpp"
    feature.parent.mkdir(parents=True)
    feature.write_text("#include <cstdint>\n", encoding="utf-8")
    feature_header = tmp_path / "core/include/chronos/core/features/feature_runtime.hpp"
    feature_header.parent.mkdir(parents=True)
    feature_header.write_text("#pragma once\n", encoding="utf-8")
    (tmp_path / "core/CMakeLists.txt").write_text(
        "add_library(chronos_core STATIC src/feature_observation.cpp)\n",
        encoding="utf-8",
    )
    write_other_authorities(tmp_path)

    violations = boundary.find_violations(tmp_path)
    assert {item.dependency for item in violations} == {"feature-source-outside-core/features"}
