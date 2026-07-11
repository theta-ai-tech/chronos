import importlib.util
import sys
from pathlib import Path

import pytest


def load_bootstrap_module():
    path = Path(__file__).resolve().parents[2] / "tools/development/bootstrap_m0.py"
    spec = importlib.util.spec_from_file_location("bootstrap_m0", path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


@pytest.mark.parametrize(
    ("output", "expected"),
    [
        ("cmake version 4.3.1", (4, 3, 1)),
        ("1.13.2", (1, 13, 2)),
        ("uv 0.11.16 (Homebrew)", (0, 11, 16)),
        ("tool 18.1", (18, 1, 0)),
    ],
)
def test_parse_version(output: str, expected: tuple[int, int, int]) -> None:
    assert load_bootstrap_module().parse_version(output) == expected


def test_parse_version_rejects_unversioned_output() -> None:
    with pytest.raises(ValueError, match="could not parse version"):
        load_bootstrap_module().parse_version("unknown")
