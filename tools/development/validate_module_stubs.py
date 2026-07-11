"""Validate the ownership metadata promised by the M0.1 module scaffold."""

from __future__ import annotations

from pathlib import Path

REQUIRED_FIELDS = (
    "Owner",
    "Plane",
    "Language",
    "Purpose",
    "Accepted dependencies",
    "Public API",
)

EXPECTED_STUBS = (
    "accounting/ledger/README.md",
    "accounting/reconciliation/README.md",
    "accounting/valuation/README.md",
    "adapters/execution/README.md",
    "adapters/external/README.md",
    "adapters/market_data/README.md",
    "adapters/paper/README.md",
    "adapters/sdk/README.md",
    "applications/benchmark_runner/README.md",
    "applications/cli/README.md",
    "applications/console/README.md",
    "applications/local_api/README.md",
    "applications/replay_runner/README.md",
    "config/defaults/README.md",
    "config/examples/README.md",
    "config/schemas/README.md",
    "contracts/commands/README.md",
    "contracts/conformance/README.md",
    "contracts/domain/README.md",
    "contracts/events/README.md",
    "contracts/views/README.md",
    "core/dispatch/README.md",
    "core/execution_planning/README.md",
    "core/features/README.md",
    "core/market_state/README.md",
    "core/portfolio/README.md",
    "core/recommendation/README.md",
    "core/risk/README.md",
    "core/strategy_runtime/README.md",
    "research/dataset_tools/README.md",
    "research/experiments/README.md",
    "research/registry/README.md",
    "research/reports/README.md",
    "runtime/audit/README.md",
    "runtime/configuration/README.md",
    "runtime/datasets/README.md",
    "runtime/observability/README.md",
    "runtime/persistence/README.md",
    "runtime/run_control/README.md",
    "strategies/fixtures/README.md",
    "strategies/reference/README.md",
    "strategies/sdk/README.md",
    "tests/contract/README.md",
    "tests/end_to_end/README.md",
    "tests/integration/README.md",
    "tests/performance/README.md",
    "tests/recovery/README.md",
    "tests/replay/README.md",
    "tools/development/README.md",
    "tools/profiling/README.md",
    "tools/release/README.md",
    "tools/schema/README.md",
)


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    failures: list[str] = []
    expected_paths = {Path(path) for path in EXPECTED_STUBS}
    actual_paths: set[Path] = set()

    for stub in sorted(root.glob("*/*/README.md")):
        content = stub.read_text(encoding="utf-8")
        if "Submodule owner stub" not in content:
            continue
        relative_path = stub.relative_to(root)
        actual_paths.add(relative_path)
        missing = [field for field in REQUIRED_FIELDS if f"**{field}:**" not in content]
        if missing:
            failures.append(f"{relative_path}: missing {', '.join(missing)}")

    for missing_stub in sorted(expected_paths - actual_paths):
        failures.append(f"{missing_stub}: expected owner stub is missing")
    for unexpected_stub in sorted(actual_paths - expected_paths):
        failures.append(f"{unexpected_stub}: owner stub is not in the M0.1 manifest")

    if failures:
        print("M0.1 module-stub validation failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1

    print(f"M0.1 module-stub metadata is complete ({len(actual_paths)} stubs).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
