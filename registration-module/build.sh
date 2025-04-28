#!/bin/bash

# Exit on any error
set -e

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build the project
make -j$(nproc)

# Run tests if they were built
if [ -f "tests/registration_tests" ]; then
    echo "Running tests..."
    ./tests/registration_tests
fi

# Install if requested
if [ "$1" = "install" ]; then
    echo "Installing..."
    sudo make install
fi

echo "Build completed successfully!" 