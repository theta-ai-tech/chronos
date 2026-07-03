.PHONY: python-check

python-check:
	uv run --group dev pytest
	uv run --group dev ruff check .
	uv run --group dev ruff format --check .
