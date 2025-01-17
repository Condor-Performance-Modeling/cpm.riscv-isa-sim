TEST_SCRIPTS := $(wildcard scripts/*.sh)
TEST_TARGETS := $(patsubst scripts/%,run-%, $(TEST_SCRIPTS))

.PHONY: all
all: $(TEST_TARGETS)
	@echo "All tests complete!"

$(TEST_TARGETS): run-%: scripts/%
	@echo "Running $< with SPIKE_BINARY=$(SPIKE_BINARY)..."
	@if SPIKE_BINARY=$(SPIKE_BINARY) bash $<; then \
		echo "Test $< PASSED!"; \
	else \
		echo "Test $< FAILED!"; \
	fi

.PHONY: list
list:
	@echo "Available test scripts:"
	@for script in $(TEST_SCRIPTS); do \
		echo "  $$script"; \
	done
