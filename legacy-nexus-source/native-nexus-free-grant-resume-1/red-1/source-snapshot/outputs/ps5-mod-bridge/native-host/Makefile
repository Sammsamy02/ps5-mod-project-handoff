PYTHON ?= python3
CRYPTO_ARGS = --mbedtls-archive "$(MBEDTLS_ARCHIVE)" --framework-archive "$(FRAMEWORK_ARCHIVE)"
.PHONY: all desktop development test sanitize-test browser-test
all:
	$(PYTHON) build.py --profile loopback $(CRYPTO_ARGS)
desktop:
	$(PYTHON) build.py --profile desktop_fixture
development:
	$(PYTHON) build.py --profile development_lan --ps5-address ps5-host-not-configured.invalid $(CRYPTO_ARGS)
sanitize-test:
	MB_CORE_SANITIZE=1 $(PYTHON) -m unittest discover -s tests -p test_core.py -v
	MB_ENTROPY_SANITIZE=1 $(PYTHON) -m unittest discover -s tests -p test_entropy.py -v
	MB_DRBG_SANITIZE=1 $(PYTHON) -m unittest discover -s tests -p test_drbg.py -v
browser-test: desktop
	node tests/browser_fixture.cjs
test:
	$(PYTHON) -m unittest discover -s tests -v
