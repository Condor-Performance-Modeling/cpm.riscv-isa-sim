#!/bin/bash

TEST_BINARIES_DIR="./test_binaries/custom-output/stf_gen"

if [ -z "$SPIKE_BINARY" ]; then
  echo "Error: SPIKE_BINARY is not set!"
  exit 1
fi

if [ ! -d "$TEST_BINARIES_DIR" ]; then
  echo "Error: Test binaries directory $TEST_BINARIES_DIR does not exist!"
  exit 1
fi

total_tests=0
passed_tests=0
failed_tests=0
failed_tests_list=()
passed_tests_list=()

run_test() {
    local test_file="$1"
    total_tests=$((total_tests + 1))

    echo -n "Running test $test_file ... "

    #TODO - figure out how to run this test on spike, if possible, it freezes now
    #$SPIKE_BINARY "$test_file" &
    SPIKE_PID=$!
    wait $SPIKE_PID
    #EXIT_CODE=$?
    EXIT_CODE=1

    if [ $EXIT_CODE -eq 0 ]; then
        echo "PASS"
        passed_tests=$((passed_tests + 1))
        passed_tests_list+=("$test_file")
    else
        echo "FAIL (exit code $EXIT_CODE)"
        failed_tests=$((failed_tests + 1))
        failed_tests_list+=("$test_file")
    fi
}

for binary in "$TEST_BINARIES_DIR"/*; do
    if [ -x "$binary" ]; then
        run_test "$binary"
    else
        echo "Skipping $binary (not executable)"
    fi
done

if [ ${#failed_tests_list[@]} -gt 0 ]; then
    echo
    echo "Failed Tests:"
    for failed_test in "${failed_tests_list[@]}"; do
        echo "  $failed_test"
    done
fi

echo
echo "Tests passed: $passed_tests"
echo "Tests failed: $failed_tests"
echo "-----"
echo

exit $failed_tests
