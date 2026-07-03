from importlib.metadata import version

import chronos


def test_python_package_exposes_project_version() -> None:
    assert version("chronos-engine") == chronos.__version__ == "0.0.1"
