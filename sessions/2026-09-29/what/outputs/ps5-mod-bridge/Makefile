CXX ?= c++
PYTHON ?= python3
NODE ?= node
BUILD_DIR ?= build
CPPFLAGS += -Iinclude -Itests -isystem third_party -isystem third_party/libarchive -isystem third_party/picosha2
CXXFLAGS += -std=c++20 -Wall -Wextra -Wpedantic
LDFLAGS ?=
ARCHIVE_LIBS ?= -larchive
TITLE_PS5_SDK ?= native-host/build/sdk-v0.43/ps5-payload-sdk

HEADERS := $(wildcard include/modbridge/*.hpp tests/*.hpp) $(wildcard third_party/*/*.h third_party/*/*.hpp)
MODULE_SOURCES := $(filter-out src/main.cpp,$(wildcard src/*.cpp))
TEST_SOURCES := $(filter-out tests/main.cpp,$(wildcard tests/*_tests.cpp))

.PHONY: all test test-core test-python test-ui test-all test-hash test-dds test-inventory test-planner test-journal test-nexus test-nexus-bridge test-pak test-pak-candidate test-format test-title test-title-source test-title-paging test-selection sanitize-test verify-deps archive-version clean
all: $(BUILD_DIR)/modbridge

verify-deps:
	$(PYTHON) tools/bootstrap_deps.py

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/modbridge: src/main.cpp $(MODULE_SOURCES) $(HEADERS) | $(BUILD_DIR) verify-deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp $(MODULE_SOURCES) $(LDFLAGS) $(ARCHIVE_LIBS) -o $@

$(BUILD_DIR)/test-hash: tests/main.cpp tests/hash_tests.cpp src/hash.cpp $(HEADERS) | $(BUILD_DIR) verify-deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/main.cpp tests/hash_tests.cpp src/hash.cpp $(LDFLAGS) -o $@

$(BUILD_DIR)/test-dds: tests/main.cpp tests/dds_tests.cpp src/hash.cpp src/dds.cpp src/inspection.cpp $(HEADERS) | $(BUILD_DIR) verify-deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/main.cpp tests/dds_tests.cpp src/hash.cpp src/dds.cpp src/inspection.cpp $(LDFLAGS) -o $@

$(BUILD_DIR)/test-inventory: tests/main.cpp tests/inventory_tests.cpp src/hash.cpp src/inventory.cpp src/inspection.cpp src/dds.cpp $(HEADERS) | $(BUILD_DIR) verify-deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/main.cpp tests/inventory_tests.cpp src/hash.cpp src/inventory.cpp src/inspection.cpp src/dds.cpp $(LDFLAGS) $(ARCHIVE_LIBS) -o $@

$(BUILD_DIR)/test-planner: tests/main.cpp tests/planner_tests.cpp src/hash.cpp src/planner.cpp src/cache.cpp src/inventory.cpp src/inspection.cpp src/dds.cpp $(HEADERS) | $(BUILD_DIR) verify-deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/main.cpp tests/planner_tests.cpp src/hash.cpp src/planner.cpp src/cache.cpp src/inventory.cpp src/inspection.cpp src/dds.cpp $(LDFLAGS) $(ARCHIVE_LIBS) -o $@

$(BUILD_DIR)/test-journal: tests/main.cpp tests/journal_tests.cpp src/hash.cpp src/journal.cpp $(HEADERS) | $(BUILD_DIR) verify-deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/main.cpp tests/journal_tests.cpp src/hash.cpp src/journal.cpp $(LDFLAGS) -o $@

$(BUILD_DIR)/test-nexus: tests/main.cpp tests/nexus_records_tests.cpp src/nexus.cpp $(HEADERS) | $(BUILD_DIR) verify-deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/main.cpp tests/nexus_records_tests.cpp src/nexus.cpp $(LDFLAGS) -o $@

$(BUILD_DIR)/test-core: tests/main.cpp $(TEST_SOURCES) $(MODULE_SOURCES) $(HEADERS) | $(BUILD_DIR) verify-deps
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/main.cpp $(TEST_SOURCES) $(MODULE_SOURCES) $(LDFLAGS) $(ARCHIVE_LIBS) -o $@

test-hash: $(BUILD_DIR)/test-hash
	./$(BUILD_DIR)/test-hash

test-dds: $(BUILD_DIR)/test-dds
	./$(BUILD_DIR)/test-dds

test-inventory: $(BUILD_DIR)/test-inventory
	./$(BUILD_DIR)/test-inventory

test-planner: $(BUILD_DIR)/test-planner
	./$(BUILD_DIR)/test-planner

test-journal: $(BUILD_DIR)/test-journal
	./$(BUILD_DIR)/test-journal

test-nexus: $(BUILD_DIR)/test-nexus
	./$(BUILD_DIR)/test-nexus

test-nexus-bridge:
	$(PYTHON) -m unittest discover -s native-host/tests -p test_nexus_bridge.py -v

test-pak:
	$(PYTHON) native-host/pak/verify.py --mbedtls-source "$(PAK_MBEDTLS_SOURCE)" --zlib-source "$(PAK_ZLIB_SOURCE)"
	PAK_MBEDTLS_SOURCE="$(PAK_MBEDTLS_SOURCE)" PAK_ZLIB_SOURCE="$(PAK_ZLIB_SOURCE)" $(PYTHON) native-host/pak/tests/test_verify_driver.py
	$(PYTHON) native-host/pak/tests/test_probe_output.py --probe native-host/build/pak/payload_probe

test-pak-candidate:
	$(PYTHON) native-host/pak/verify_candidate.py --mbedtls-source "$(PAK_MBEDTLS_SOURCE)" --zlib-source "$(PAK_ZLIB_SOURCE)" --output-dir native-host/build/pak-candidate

test-format:
	$(PYTHON) native-host/format/verify.py --mbedtls-source "$(PAK_MBEDTLS_SOURCE)" --zlib-source "$(PAK_ZLIB_SOURCE)" --output-dir native-host/build/format

test-title:
	$(PYTHON) native-host/title/verify.py --ps5-sdk "$(TITLE_PS5_SDK)" --output-dir native-host/build/title

test-title-source:
	$(PYTHON) native-host/title/source_verify.py host

test-title-paging:
	$(PYTHON) tests/title_paging_tests.py
	$(PYTHON) tests/host_title_paging_tests.py
	$(NODE) tests/ui_paging_tests.cjs
	PYTHON="$(PYTHON)" $(NODE) tests/title_paging_browser_integration.cjs

test-selection:
	$(PYTHON) tests/title_inventory_tests.py
	$(PYTHON) tests/analysis_report_tests.py
	$(NODE) tests/generic_selection_ui_tests.cjs

test-core: $(BUILD_DIR)/test-core
	./$(BUILD_DIR)/test-core

test-python: all
	MODBRIDGE_BIN="$(abspath $(BUILD_DIR)/modbridge)" $(PYTHON) -m unittest discover -s tests -p '*test*.py'

test: test-core test-python

test-ui: all
	$(NODE) tests/ui_tests.cjs
	$(NODE) tests/generic_selection_ui_tests.cjs
	PYTHON="$(PYTHON)" MODBRIDGE_BIN="$(abspath $(BUILD_DIR)/modbridge)" $(NODE) tests/host_ui_integration.cjs

test-all: test test-ui

sanitize-test:
	$(MAKE) BUILD_DIR=build-sanitize CXXFLAGS='-std=c++20 -Wall -Wextra -Wpedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' test

archive-version: | $(BUILD_DIR)
	@printf '#include <archive.h>\n#include <stdio.h>\nint main(void) { puts(archive_version_details()); }\n' | $(CXX) -x c++ -isystem third_party/libarchive -isystem third_party/picosha2 - $(ARCHIVE_LIBS) -o $(BUILD_DIR)/archive-version
	@./$(BUILD_DIR)/archive-version

clean:
	rm -rf build build-sanitize
