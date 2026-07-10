.PHONY: cpp-build cpp-test cpp-check python-test python-lint python-format python-check m0-check m0-gate-proof

cpp-build:
	cmake -S . -B build -G Ninja
	cmake --build build

cpp-test: cpp-build
	ctest --test-dir build --output-on-failure

cpp-check: cpp-test

python-test: cpp-build
	uv run --group dev pytest

python-lint:
	uv run --group dev ruff check .

python-format:
	uv run --group dev ruff format --check .

python-check: python-test python-lint python-format

m0-check: cpp-check python-check

m0-gate-proof:
	uv run --group dev python tools/development/prove_m0_gates.py
