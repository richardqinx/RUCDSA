CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Wpedantic -O2

TARGET := triplet
SOURCE := main.cc
HEADER := Triplet.hh

.PHONY: build test

build: $(TARGET)

$(TARGET): $(SOURCE) $(HEADER)
	$(CXX) $(CXXFLAGS) $(SOURCE) -o $(TARGET)

test: build
	@passed=0; \
	failed=0; \
	total=0; \
	for input in *.in; do \
		if [ ! -f "$$input" ]; then \
			continue; \
		fi; \
		total=$$((total + 1)); \
		base=$${input%.in}; \
		expected="$$base.out"; \
		actual="$$base.actual"; \
		if [ ! -f "$$expected" ]; then \
			echo "[FAIL] $$input: missing $$expected"; \
			failed=$$((failed + 1)); \
			continue; \
		fi; \
		if ./$(TARGET) < "$$input" > "$$actual"; then \
			if diff -u "$$expected" "$$actual" > /dev/null; then \
				echo "[PASS] $$base"; \
				passed=$$((passed + 1)); \
			else \
				echo "[FAIL] $$base"; \
				diff -u "$$expected" "$$actual" || true; \
				failed=$$((failed + 1)); \
			fi; \
		else \
			echo "[FAIL] $$base: program exited with an error"; \
			failed=$$((failed + 1)); \
		fi; \
		rm -f "$$actual"; \
	done; \
	echo; \
	echo "Passed: $$passed / $$total"; \
	if [ "$$failed" -ne 0 ]; then \
		exit 1; \
	fi
