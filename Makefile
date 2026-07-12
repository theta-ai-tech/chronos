.PHONY: bootstrap module-stubs-check cpp-build cpp-test cpp-format cpp-check cpp-profiles-check python-build python-test python-lint python-format python-check m0-round-trip m0-check m0-gate-proof m1-conformance

bootstrap:
	python3 tools/development/bootstrap_m0.py

module-stubs-check:
	python3 tools/development/validate_module_stubs.py

cpp-build:
	cmake -S . -B build -G Ninja
	cmake --build build

cpp-test: cpp-build
	ctest --test-dir build --output-on-failure

cpp-format:
	uv run --locked --group dev clang-format --dry-run --Werror $$(find contracts core adapters tests -type f \( -name '*.cpp' -o -name '*.hpp' \))

cpp-check: cpp-format cpp-test

cpp-profiles-check:
	python3 tools/development/verify_cpp_profiles.py

python-build:
	uv run --locked --group dev python tools/development/verify_python_distribution.py

python-test: cpp-build
	uv run --locked --group dev pytest

python-lint:
	uv run --locked --group dev ruff check .

python-format:
	uv run --locked --group dev ruff format --check .

python-check: python-build python-test python-lint python-format

m0-round-trip: cpp-build
	uv run --locked --group dev python tools/development/run_m0_round_trip.py

m0-check: module-stubs-check cpp-check python-check

m0-gate-proof:
	uv run --locked --group dev python tools/development/prove_m0_gates.py $$(test -z "$(PROOF)" || printf '%s' '--proof $(PROOF)')

m1-conformance: cpp-build
	uv run --locked --group dev pytest tests/python/test_serialization.py
