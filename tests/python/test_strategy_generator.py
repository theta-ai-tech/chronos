import json
import subprocess
import sys
from pathlib import Path

TOOL = Path(__file__).resolve().parents[2] / "tools/development/generate_strategy_definition.py"


def manifest() -> dict[str, object]:
    def version(seed: int) -> dict[str, object]:
        return {
            "definition_id": f"{seed:02x}" + "00" * 15,
            "version": 1,
        }

    return {
        "definition_version": version(1),
        "implementation_version": version(2),
        "feature_definition_version": version(3),
        "parameter_id": "04" + "00" * 15,
        "parameter_definition_version": version(5),
        "arithmetic_version": version(6),
        "explanation_policy_version": version(7),
        "feature_factor_id": "08" + "00" * 15,
        "parameter_factor_id": "09" + "00" * 15,
        "signal_horizon_nanoseconds": 1_000_000,
    }


def generate(tmp_path: Path, value: dict[str, object]) -> subprocess.CompletedProcess[str]:
    definition = tmp_path / "definition.json"
    header = tmp_path / "include/test.hpp"
    source = tmp_path / "src/test.cpp"
    definition.write_text(json.dumps(value), encoding="utf-8")
    return subprocess.run(
        (
            sys.executable,
            str(TOOL),
            "--target",
            "test_strategy",
            "--manifest",
            str(definition),
            "--header",
            str(header),
            "--source",
            str(source),
        ),
        text=True,
        capture_output=True,
        check=False,
    )


def test_generator_accepts_only_data_and_emits_fixed_program(tmp_path: Path) -> None:
    result = generate(tmp_path, manifest())
    assert result.returncode == 0, result.stderr
    source = (tmp_path / "src/test.cpp").read_text(encoding="utf-8")
    assert "FinishDirectionalThreshold" in source
    assert "pthread" not in source


def test_generator_rejects_schema_extension_and_invalid_bounds(tmp_path: Path) -> None:
    extended = manifest()
    extended["source"] = "strategy.cpp"
    assert generate(tmp_path, extended).returncode != 0
    invalid = manifest()
    invalid["signal_horizon_nanoseconds"] = 0
    assert generate(tmp_path, invalid).returncode != 0
