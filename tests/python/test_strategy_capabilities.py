import importlib.util
import sys
from pathlib import Path


def load_tool(name: str):
    path = Path(__file__).resolve().parents[2] / f"tools/development/{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None
    assert spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


find_violations = load_tool("verify_strategy_capabilities").find_violations
find_symbol_violations = load_tool("verify_strategy_symbols").find_symbol_violations


def write_source(tmp_path: Path, source: str) -> Path:
    path = tmp_path / "strategy.cpp"
    path.write_text(source, encoding="utf-8")
    return path


def test_deterministic_strategy_surface_passes(tmp_path: Path) -> None:
    path = write_source(
        tmp_path,
        '#include "chronos/strategies/sdk/strategy.hpp"\n#include <array>\n',
    )
    assert find_violations([path]) == []


def test_strategy_cannot_import_core_or_host_authority(tmp_path: Path) -> None:
    path = write_source(
        tmp_path,
        '#include "chronos/core/features/feature_runtime.hpp"\n'
        '#include "chronos/strategies/sdk/strategy_host.hpp"\n',
    )
    capabilities = [item.capability for item in find_violations([path])]
    assert capabilities == ["undeclared-host-or-core", "undeclared-host-or-core"]


def test_host_clock_filesystem_network_and_secret_access_fail(tmp_path: Path) -> None:
    path = write_source(
        tmp_path,
        "\n".join(
            [
                "#include <chrono>",
                "#include <filesystem>",
                "#include <sys/socket.h>",
                'auto secret = getenv("TOKEN");',
            ]
        ),
    )
    capabilities = {item.capability for item in find_violations([path])}
    assert capabilities == {"host-clock", "filesystem", "network", "environment-or-secret"}


def test_dynamic_allocation_randomness_and_process_output_fail(tmp_path: Path) -> None:
    path = write_source(
        tmp_path,
        "auto value = new int(1);\nstd::random_device source;\nstd::cerr << *value;\n",
    )
    capabilities = {item.capability for item in find_violations([path])}
    assert capabilities == {
        "unbounded-allocation",
        "nondeterministic-randomness",
        "telemetry-or-process-output",
    }


def test_linked_forbidden_symbols_fail() -> None:
    output = """                 U getenv
                 U std::chrono::system_clock::now()
                 U connect
                 U chronos::contracts::sha256(...)
"""
    capabilities = {item.capability for item in find_symbol_violations(output)}
    assert capabilities == {"environment-or-secret", "host-clock", "network"}
