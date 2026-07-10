.PHONY: cpp-build cpp-check python-check m0-check

cpp-build:
	cmake -S . -B build -G Ninja
	cmake --build build

cpp-check: cpp-build
	ctest --test-dir build --output-on-failure

python-check: cpp-build
	uv run --group dev pytest
	uv run --group dev ruff check .
	uv run --group dev ruff format --check .

m0-check: cpp-check python-check
