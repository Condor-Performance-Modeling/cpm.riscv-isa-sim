#!/bin/bash

export RISCV_TEST_DIR=./test_binaries/share/riscv-tests/isa
ALLOWED_TESTS_FILE=./scripts/isa_test_list.txt

passed_tests=0
failed_tests=0
total_tests=0
test_counter=0
last_test_duration=0

passed_tests_list=()

if [ -z "$SPIKE_BINARY" ]; then
    echo "Error: SPIKE_BINARY environment variable is not set!"
    exit 1
fi

if [ ! -d "$RISCV_TEST_DIR" ]; then
    echo "Error: Test directory $RISCV_TEST_DIR does not exist"
    exit 1
fi

if [ ! -f "$ALLOWED_TESTS_FILE" ]; then
    echo "Error: Allowed tests file $ALLOWED_TESTS_FILE does not exist"
    exit 1
fi

mapfile -t allowed_test_files < "$ALLOWED_TESTS_FILE"

trap ctrl_c_handler SIGINT
ctrl_c_handler() {
    echo "Stopping test execution..."
    exit 1
}

run_test() {
    local test_file="$1"
    test_counter=$((test_counter + 1))

    # Print "Running test test_name ..."
    echo -n "Running test $test_file ... "

    # Run the test
    $SPIKE_BINARY "$test_file" &
    SPIKE_PID=$!

    wait $SPIKE_PID
    EXIT_CODE=$?

    # Append PASS or FAIL to the same line
    if [ $EXIT_CODE -eq 0 ]; then
        echo "PASS"
        passed_tests=$((passed_tests + 1))
        passed_tests_list+=("$test_file")
    else
        echo "FAIL (exit code $EXIT_CODE)"
        failed_tests=$((failed_tests + 1))
    fi
}

for test_file in "${allowed_test_files[@]}"; do
    test_file=$(echo "$test_file" | xargs)

    if [[ -z "$test_file" ]]; then
        continue
    fi

    if [[ "$test_file" =~ ^# ]] || [[ "$test_file" =~ ^x ]]; then
        continue
    fi

    full_test_path="$RISCV_TEST_DIR/$test_file"
    total_tests=$((total_tests + 1))

    if [ -f "$full_test_path" ]; then
        run_test "$full_test_path"
    else
        echo "Warning: Test file $full_test_path does not exist"
        echo "Test $test_file failed due to missing file"
        failed_tests=$((failed_tests + 1))
    fi
done

echo
echo "Tests passed: $passed_tests"
echo "Tests failed: $failed_tests"
echo "-----"
echo

exit $failed_tests
