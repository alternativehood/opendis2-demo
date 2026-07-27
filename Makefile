# ── OpenDis2 Makefile ──────────────────────────────────────────────────────────
# Developer convenience wrappers around CMake presets (vcpkg manifest mode).
#
# Targets:
#   configure         — configure CMake with the 'dev' preset (no build)
#   build             — configure + build dev binaries (no tests)
#   test              — configure + build test binaries + run CTest
#   test-integration  — configure + build + run integration tests (requires game data)
#   lint              — run all lint/guardrail checks
#   lint-fix          — run auto-fixes, then lint
#   lint-changed      — check formatting for changed files
#   lint-changed-fix  — format changed files
#   clean             — remove build/dev (preserves vcpkg_installed)
#   distclean         — remove build/dev and vcpkg_installed
#   windows-docker    — cross-build Windows x64 artifact in Docker
#   windows-docker-clean — remove Windows Docker export only
#   help              — print this message
# ──────────────────────────────────────────────────────────────────────────────

SHELL := /bin/bash

.DEFAULT_GOAL := help

ENV_FREE_GOALS := help windows-docker windows-docker-clean
REQUESTED_GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),help)
REQUIRES_ENV := $(filter-out $(ENV_FREE_GOALS),$(REQUESTED_GOALS))

# ── Load .env only for goals that need the local vcpkg environment ───────────
ifneq ($(strip $(REQUIRES_ENV)),)
ifneq ("$(wildcard .env)","")
include .env
export
else
$(error .env not found. Run: cp .env.example .env && edit VCPKG_ROOT in .env)
endif
endif

# ── Guard: VCPKG_ROOT must be set ────────────────────────────────────────────
ifneq ($(strip $(REQUIRES_ENV)),)
ifeq ($(origin VCPKG_ROOT),undefined)
    $(error VCPKG_ROOT is not set in .env)
endif
endif

# ── Derived paths ────────────────────────────────────────────────────────────
BUILD_DIR         := build/dev
BUILD_PRESET      := dev
TEST_PRESET       := dev-unit
INTEGRATION_PRESET := dev-integration
WINDOWS_DOCKER_OUTPUT ?= dist/windows-mingw-x64
WINDOWS_DOCKER_JOBS ?= 4
# Dev binaries (NOT test binaries)
DEV_TARGETS := opendis2 opendis2-dev-extractor opendis2-dev-scenario-gen opendis2-dev-terrain-assets-dump opendis2-dev-terrain-calibration-export opendis2-dev-battle
TEST_TARGETS := opendis2-dev-tests

# ── Default target ──────────────────────────────────────────────────────────
.PHONY: help
help:
	@echo "OpenDis2 build targets"
	@echo "  make configure         — configure CMake only"
	@echo "  make build             — configure + build dev binaries (no tests)"
	@echo "  make test              — configure + build test binaries + run tests"
	@echo "  make test-integration  — configure + build + run integration tests (requires DISCIPLES2_GAME_ROOT + game data)"
	@echo "  make validate-full-game — full-game extraction validation (may exceed 3s per test)"
	@echo "  make test-timings       — print CTest runtimes sorted descending for diagnostics"
	@echo "  make test-integration-timings — print integration test runtimes"
	@echo "  make lint              — run all lint/guardrail checks"
	@echo "  make lint-fix          — auto-fix formatting, then run lint"
	@echo "  make lint-changed      — format-check changed source/test files"
	@echo "  make lint-changed-fix  — format changed source/test files"
	@echo "  make windows-docker    — cross-build Windows x64 artifact in Docker"
	@echo "  make windows-docker-clean — remove Windows Docker export only"
	@echo "  make verify            — build + test + lint"
	@echo "  make verify-integration — build + test + lint + integration tests"
	@echo "  make clean             — remove build/dev (preserves vcpkg_installed)"
	@echo "  make distclean         — remove build/dev and vcpkg_installed"
	@echo ""
	@echo "Prerequisites:"
	@echo "  1. Install vcpkg: https://vcpkg.io"
	@echo "  2. cp .env.example .env && edit VCPKG_ROOT in .env"
	@echo "  3. make build"

# ── Configure ────────────────────────────────────────────────────────────────
.PHONY: configure
configure:
	@echo "=== configure (vcpkg manifest mode) ==="
	cmake --preset dev

# ── Build (dev binaries only, no tests) ──────────────────────────────────────
.PHONY: build
build: configure
	@echo "=== build (dev binaries) ==="
	cmake --build --preset $(BUILD_PRESET) --target $(DEV_TARGETS)

# ── Test (unit tests, excludes integration) ──────────────────────────────────
# Builds everything (dev + test binaries), then runs CTest in parallel.
# Override jobs:  CTEST_PARALLEL_LEVEL=12 make test
.PHONY: test
test: build
	@echo "=== test (run CTest, parallel, unit tests only) ==="
	cmake --build --preset $(BUILD_PRESET) --target $(TEST_TARGETS)
	ctest --preset $(TEST_PRESET) -LE integration $(CTEST_ARGS)

# ── Test (integration tests, requires game data) ────────────────────────────
.PHONY: test-integration
test-integration: build
	@echo "=== test-integration (requires DISCIPLES2_GAME_ROOT + game data) ==="
	cmake --build --preset $(BUILD_PRESET) --target opendis2-dev-integration-tests
	ctest --preset dev-integration $(CTEST_ARGS)

# ── Test (all tests, serial — for debugging parallel issues) ────────────────
.PHONY: test-serial
test-serial: build
	@echo "=== test-serial (all tests, serial) ==="
	cmake --build --preset $(BUILD_PRESET) --target $(TEST_TARGETS)
	ctest --preset dev-serial $(CTEST_ARGS)

# ── Full-game validation (outside CTest — may exceed 3-second limit) ────────
.PHONY: validate-full-game
validate-full-game: build
	@echo "=== validate-full-game (full-game extraction, requires DISCIPLES2_GAME_ROOT) ==="
	cmake --build --preset $(BUILD_PRESET) --target opendis2-dev-tests opendis2-dev-full-game-validation
	@OPENDIS2_MAKE_TARGET=$@ tools/validate_full_game.sh

# ── Test timing report (unit tests) ──────────────────────────────────────────
.PHONY: test-timings
test-timings:
	@echo "=== test-timings (unit) ==="
	@mkdir -p $(BUILD_DIR)/Testing
	@rm -f $(CURDIR)/$(BUILD_DIR)/Testing/unit_junit.xml
	@cmake --build --preset $(BUILD_PRESET) --target $(TEST_TARGETS); \
	status=$$?; \
	if [ $$status -ne 0 ]; then echo "BUILD FAILED"; exit $$status; fi; \
	ctest --test-dir $(BUILD_DIR) --output-junit $(CURDIR)/$(BUILD_DIR)/Testing/unit_junit.xml -LE integration $(CTEST_ARGS); \
	ctest_status=$$?; \
	if [ -f $(CURDIR)/$(BUILD_DIR)/Testing/unit_junit.xml ]; then \
		guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; \
		OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" \
		python3 tools/test_timings.py $(CURDIR)/$(BUILD_DIR)/Testing/unit_junit.xml; \
		parser_status=$$?; \
		if [ $$ctest_status -ne 0 ]; then exit $$ctest_status; fi; \
		exit $$parser_status; \
	else \
		echo "ERROR: No fresh timing result produced"; \
		exit 1; \
	fi

# ── Integration test timing report ───────────────────────────────────────────
.PHONY: test-integration-timings
test-integration-timings:
	@echo "=== test-integration-timings (integration) ==="
	@mkdir -p $(BUILD_DIR)/Testing
	@rm -f $(CURDIR)/$(BUILD_DIR)/Testing/integration_junit.xml
	@cmake --build --preset $(BUILD_PRESET) --target opendis2-dev-integration-tests; \
	status=$$?; \
	if [ $$status -ne 0 ]; then echo "BUILD FAILED"; exit $$status; fi; \
	ctest --test-dir $(BUILD_DIR) --output-junit $(CURDIR)/$(BUILD_DIR)/Testing/integration_junit.xml -L integration $(CTEST_ARGS); \
	ctest_status=$$?; \
	if [ -f $(CURDIR)/$(BUILD_DIR)/Testing/integration_junit.xml ]; then \
		guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; \
		OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" \
		python3 tools/test_timings.py $(CURDIR)/$(BUILD_DIR)/Testing/integration_junit.xml; \
		parser_status=$$?; \
		if [ $$ctest_status -ne 0 ]; then exit $$ctest_status; fi; \
		exit $$parser_status; \
	else \
		echo "ERROR: No fresh timing result produced"; \
		exit 1; \
	fi

# ── Archive ──────────────────────────────────────────────────────────────────
.PHONY: archive
archive:
	@echo "=== archive ==="
	@OPENDIS2_MAKE_TARGET=$@ tools/archive.sh

# ── Lint ─────────────────────────────────────────────────────────────────────
.PHONY: lint
lint:
	@mkdir -p $(BUILD_DIR)
	@if [ ! -x tools/lint-check.sh ]; then echo "ERROR: tools/lint-check.sh is not executable" >&2; echo "Run:" >&2; echo "  chmod +x tools/lint-check.sh" >&2; exit 1; fi
	@if [ ! -x tools/guardrail_tools_guards.sh ]; then echo "ERROR: tools/guardrail_tools_guards.sh is not executable" >&2; echo "Run:" >&2; echo "  chmod +x tools/guardrail_tools_guards.sh" >&2; exit 1; fi
	@guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/guardrail_tools_guards.sh && OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/lint-check.sh

.PHONY: lint-fix
lint-fix:
	@mkdir -p $(BUILD_DIR)
	@if [ ! -x tools/lint-fix.sh ]; then echo "ERROR: tools/lint-fix.sh is not executable" >&2; echo "Run:" >&2; echo "  chmod +x tools/lint-fix.sh" >&2; exit 1; fi
	@if [ ! -x tools/guardrail_tools_guards.sh ]; then echo "ERROR: tools/guardrail_tools_guards.sh is not executable" >&2; echo "Run:" >&2; echo "  chmod +x tools/guardrail_tools_guards.sh" >&2; exit 1; fi
	@guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/guardrail_tools_guards.sh && OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/lint-fix.sh

.PHONY: lint-changed
lint-changed:
	@mkdir -p $(BUILD_DIR)
	@if [ ! -x tools/lint-changed-check.sh ]; then echo "ERROR: tools/lint-changed-check.sh is not executable" >&2; echo "Run:" >&2; echo "  chmod +x tools/lint-changed-check.sh" >&2; exit 1; fi
	@if [ ! -x tools/guardrail_tools_guards.sh ]; then echo "ERROR: tools/guardrail_tools_guards.sh is not executable" >&2; echo "Run:" >&2; echo "  chmod +x tools/guardrail_tools_guards.sh" >&2; exit 1; fi
	@guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/guardrail_tools_guards.sh && OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/lint-changed-check.sh

.PHONY: lint-tidy
lint-tidy:
	@mkdir -p $(BUILD_DIR)
	@guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; MAKELEVEL=1 OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/lint-tidy-check.sh

.PHONY: lint-changed-fix
lint-changed-fix:
	@mkdir -p $(BUILD_DIR)
	@if [ ! -x tools/lint-changed-fix.sh ]; then echo "ERROR: tools/lint-changed-fix.sh is not executable" >&2; echo "Run:" >&2; echo "  chmod +x tools/lint-changed-fix.sh" >&2; exit 1; fi
	@if [ ! -x tools/guardrail_tools_guards.sh ]; then echo "ERROR: tools/guardrail_tools_guards.sh is not executable" >&2; echo "Run:" >&2; echo "  chmod +x tools/guardrail_tools_guards.sh" >&2; exit 1; fi
	@guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/guardrail_tools_guards.sh && OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/lint-changed-fix.sh

# ── Verify ───────────────────────────────────────────────────────────────────
.PHONY: verify
verify:
	@echo "=== verify: build + test + lint ==="
	@mkdir -p $(BUILD_DIR)
	@guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/verify.sh

.PHONY: verify-integration
verify-integration:
	@echo "=== verify-integration: build + test + lint + integration tests ==="
	@mkdir -p $(BUILD_DIR)
	@guard="$(CURDIR)/$(BUILD_DIR)/.make-guard-$@"; trap 'rm -f "$$guard"' EXIT; touch "$$guard"; OPENDIS2_MAKE_TARGET=$@ OPENDIS2_MAKE_GUARD="$$guard" tools/verify.sh --integration

# ── Clean ────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	@echo "=== clean ==="
	rm -rf $(BUILD_DIR)
	@echo "Removed $(BUILD_DIR)"

# ── Distclean ────────────────────────────────────────────────────────────────
.PHONY: distclean
distclean:
	@echo "=== distclean ==="
	rm -rf $(BUILD_DIR)
	rm -rf vcpkg_installed
	@echo "Removed $(BUILD_DIR) and vcpkg_installed"

# ── Windows x64 Docker cross-build ───────────────────────────────────────────
.PHONY: windows-docker
windows-docker:
	@echo "=== windows-docker ==="
	@command -v docker >/dev/null 2>&1 || { echo "ERROR: docker is required" >&2; exit 1; }
	@docker buildx version >/dev/null 2>&1 || { echo "ERROR: docker buildx is required" >&2; exit 1; }
	rm -rf $(WINDOWS_DOCKER_OUTPUT)
	mkdir -p $(dir $(WINDOWS_DOCKER_OUTPUT))
	docker buildx build \
		--platform linux/amd64 \
		--file docker/windows-mingw/Dockerfile \
		--target artifact \
		--build-arg BUILD_JOBS=$(WINDOWS_DOCKER_JOBS) \
		--output type=local,dest=$(WINDOWS_DOCKER_OUTPUT) \
		.
	@test -s $(WINDOWS_DOCKER_OUTPUT)/opendis2.exe || { echo "ERROR: missing $(WINDOWS_DOCKER_OUTPUT)/opendis2.exe" >&2; exit 1; }
	@test -s $(WINDOWS_DOCKER_OUTPUT)/pe-info.txt || { echo "ERROR: missing $(WINDOWS_DOCKER_OUTPUT)/pe-info.txt" >&2; exit 1; }
	@echo "$(WINDOWS_DOCKER_OUTPUT)/opendis2.exe"
	@echo "$(WINDOWS_DOCKER_OUTPUT)/pe-info.txt"

.PHONY: windows-docker-clean
windows-docker-clean:
	@echo "=== windows-docker-clean ==="
	rm -rf $(WINDOWS_DOCKER_OUTPUT)
	@echo "Removed $(WINDOWS_DOCKER_OUTPUT)"
