.PHONY: bootstrap module-stubs-check cpp-build cpp-test cpp-check python-test python-lint python-format python-check m0-round-trip m0-check m0-gate-proof

bootstrap:
	python3 tools/development/bootstrap_m0.py

module-stubs-check:
	python3 tools/development/validate_module_stubs.py

cpp-build:
	cmake -S . -B build -G Ninja
	cmake --build build

cpp-test: cpp-build
	ctest --test-dir build --output-on-failure

cpp-check: cpp-test

python-test: cpp-build
	uv run --locked --group dev pytest

python-lint:
	uv run --locked --group dev ruff check .

python-format:
	uv run --locked --group dev ruff format --check .

python-check: python-test python-lint python-format

m0-round-trip: cpp-build
	uv run --locked --group dev python tools/development/run_m0_round_trip.py

m0-check: module-stubs-check cpp-check python-check

m0-gate-proof:
	uv run --locked --group dev python tools/development/prove_m0_gates.py
