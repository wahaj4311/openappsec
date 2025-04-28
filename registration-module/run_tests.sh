#!/bin/bash

# Exit on any error
set -e

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure CMake with coverage flags
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="--coverage" ..

# Build the project
make -j$(nproc)

# Create directory for test configuration
mkdir -p conf

# Run tests
echo "Running tests..."
./tests/registration_tests

# Generate coverage report
echo "Generating coverage report..."
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_report

echo "Tests completed. Coverage report available in build/coverage_report/index.html" 