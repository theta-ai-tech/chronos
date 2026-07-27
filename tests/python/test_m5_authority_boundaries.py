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


def test_current_m5_authorities_have_no_downstream_capabilities() -> None:
    root = Path(__file__).resolve().parents[2]
    assert boundary.find_violations(root) == []


def test_downstream_include_is_rejected(tmp_path: Path) -> None:
    feature = tmp_path / "core/src/feature_runtime.cpp"
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
        "add_library(chronos_core STATIC src/feature_runtime.cpp)\n", encoding="utf-8"
    )
    strategy = tmp_path / "runtime/strategies/src/strategy_evaluation.cpp"
    strategy.parent.mkdir(parents=True)
    strategy.write_text(
        '#include "chronos/core/risk/risk_authority.hpp"\n#include <filesystem>\n',
        encoding="utf-8",
    )
    (tmp_path / "runtime/strategies/CMakeLists.txt").write_text(
        "add_library(chronos_strategy_runtime STATIC src/strategy_evaluation.cpp)\n",
        encoding="utf-8",
    )
    recommendation = tmp_path / "core/recommendation/src/recommendation.cpp"
    recommendation.parent.mkdir(parents=True)
    recommendation.write_text(
        '#include "chronos/core/recommendation/recommendation.hpp"\n',
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
        "execution",
        "filesystem",
    }
