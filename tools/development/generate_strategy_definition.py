"""Generate a fixed-capability native strategy definition from strict JSON."""

# ruff: noqa: E501

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

FIELDS = {
    "definition_version",
    "implementation_version",
    "feature_definition_version",
    "parameter_id",
    "parameter_definition_version",
    "arithmetic_version",
    "explanation_policy_version",
    "feature_factor_id",
    "parameter_factor_id",
    "signal_horizon_nanoseconds",
}
HEX_ID = re.compile(r"[0-9a-fA-F]{32}")
TARGET = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")


def identifier(value: object, field: str) -> str:
    if not isinstance(value, str) or not HEX_ID.fullmatch(value) or int(value, 16) == 0:
        raise ValueError(f"{field} must be a non-zero 16-byte hexadecimal id")
    return ", ".join(f"0x{value[index : index + 2]}" for index in range(0, 32, 2))


def version(value: object, field: str) -> tuple[str, int]:
    if not isinstance(value, dict) or set(value) != {"definition_id", "version"}:
        raise ValueError(f"{field} must contain only definition_id and version")
    number = value["version"]
    if isinstance(number, bool) or not isinstance(number, int) or number <= 0:
        raise ValueError(f"{field}.version must be positive")
    return identifier(value["definition_id"], f"{field}.definition_id"), number


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", required=True)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--header", required=True, type=Path)
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()
    if not TARGET.fullmatch(args.target):
        raise ValueError("target must be a C++ identifier")
    data = json.loads(args.manifest.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) != FIELDS:
        raise ValueError(f"manifest fields must be exactly: {', '.join(sorted(FIELDS))}")
    horizon = data["signal_horizon_nanoseconds"]
    if (
        isinstance(horizon, bool)
        or not isinstance(horizon, int)
        or not 0 < horizon <= 86_400_000_000_000
    ):
        raise ValueError("signal_horizon_nanoseconds is outside the accepted bound")

    versions = {
        name: version(data[name], name)
        for name in (
            "definition_version",
            "implementation_version",
            "feature_definition_version",
            "parameter_definition_version",
            "arithmetic_version",
            "explanation_policy_version",
        )
    }
    ids = {
        name: identifier(data[name], name)
        for name in ("parameter_id", "feature_factor_id", "parameter_factor_id")
    }
    function = f"accepted_{args.target}_definition"
    args.header.parent.mkdir(parents=True, exist_ok=True)
    args.source.parent.mkdir(parents=True, exist_ok=True)
    args.header.write_text(
        f"""#pragma once
#include "chronos/strategies/sdk/strategy.hpp"
#include <optional>
namespace chronos::strategies::generated {{
[[nodiscard]] std::optional<sdk::AcceptedStrategyDefinition> {function}() noexcept;
}}
""",
        encoding="utf-8",
    )

    def id_expr(contents: str) -> str:
        return f"id<contracts::DefinitionId>({{{contents}}})"

    def ver_expr(name: str) -> str:
        contents, number = versions[name]
        return f"version({{{contents}}}, {number})"

    source = f"""#include "chronos/strategies/generated/{args.target}.hpp"
#include <array>
namespace chronos::strategies::generated {{ namespace {{
template <typename Id> Id id(typename Id::bytes_type bytes) noexcept {{ return *Id::from_bytes(bytes); }}
contracts::VersionRef version(contracts::DefinitionId::bytes_type bytes, std::uint64_t value) noexcept {{
  return *contracts::VersionRef::from(id<contracts::DefinitionId>(bytes), value);
}}
}} // namespace
std::optional<sdk::AcceptedStrategyDefinition> {function}() noexcept {{
  static const std::array dependencies = {{sdk::FeatureDependency{{sdk::StrategyFeatureKind::OrderBookImbalance, {ver_expr("feature_definition_version")}}}}};
  static const std::array parameters = {{sdk::StrategyParameterSchema{{{id_expr(ids["parameter_id"])}, {ver_expr("parameter_definition_version")}, *contracts::DecimalScale::from_exponent(6)}}}};
  static const std::array instructions = {{
    sdk::StrategyInstruction{{sdk::StrategyOpcode::LoadFeature}},
    sdk::StrategyInstruction{{sdk::StrategyOpcode::RequireValidScaledRatio}},
    sdk::StrategyInstruction{{sdk::StrategyOpcode::LoadParameter}},
    sdk::StrategyInstruction{{sdk::StrategyOpcode::RequirePositiveParameter}},
    sdk::StrategyInstruction{{sdk::StrategyOpcode::CompareAbsoluteFeatureAtLeastParameter}},
    sdk::StrategyInstruction{{sdk::StrategyOpcode::AppendFeatureFactor}},
    sdk::StrategyInstruction{{sdk::StrategyOpcode::AppendParameterFactor, 1}},
    sdk::StrategyInstruction{{sdk::StrategyOpcode::FinishDirectionalThreshold}}
  }};
  static const std::array factors = {{
    sdk::ProgramFactorDefinition{{{id_expr(ids["feature_factor_id"])}, sdk::ExplanationSource::Feature}},
    sdk::ProgramFactorDefinition{{{id_expr(ids["parameter_factor_id"])}, sdk::ExplanationSource::StrategyParameter}}
  }};
  return sdk::AcceptedStrategyDefinition::accept({{
    .descriptor = {{.definition_version = {ver_expr("definition_version")}, .implementation_version = {ver_expr("implementation_version")},
      .required_features = dependencies, .parameter_schema = parameters, .arithmetic_version = {ver_expr("arithmetic_version")},
      .explanation_policy_version = {ver_expr("explanation_policy_version")},
      .resource_limits = {{sdk::kMaximumEvaluationOperations, 1, 1, factors.size(), sdk::kInterpreterWorkingBytes}}}},
    .program = {{instructions, factors, {horizon}}}
  }});
}}
}} // namespace chronos::strategies::generated
"""
    args.source.write_text(source, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
