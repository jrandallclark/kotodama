#!/bin/bash

# Find ctest - check common locations
CTEST=""
if command -v ctest &> /dev/null; then
    CTEST="ctest"
elif [ -f "$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/ctest" ]; then
    CTEST="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/ctest"
elif [ -f "/Applications/CMake.app/Contents/bin/ctest" ]; then
    CTEST="/Applications/CMake.app/Contents/bin/ctest"
else
    echo "Error: Could not find ctest"
    echo "Please install CMake or add it to your PATH"
    exit 1
fi

# Find cmake - check common locations
CMAKE=""
if command -v cmake &> /dev/null; then
    CMAKE="cmake"
elif [ -f "$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/cmake" ]; then
    CMAKE="$HOME/Qt/Tools/CMake/CMake.app/Contents/bin/cmake"
elif [ -f "/Applications/CMake.app/Contents/bin/cmake" ]; then
    CMAKE="/Applications/CMake.app/Contents/bin/cmake"
else
    echo "Error: Could not find cmake"
    echo "Please install CMake or add it to your PATH"
    exit 1
fi

# Find the build directory - Qt Creator uses kit-specific names
BUILD_DIR=$(find build -maxdepth 1 -type d -name "*Debug*" -o -name "*Release*" | head -1)

if [ -z "$BUILD_DIR" ]; then
    echo "Error: Could not find build directory"
    echo "Looking for directories matching build/*Debug* or build/*Release*"
    exit 1
fi

echo "Using cmake: $CMAKE"
echo "Using ctest: $CTEST"
echo "Using build directory: $BUILD_DIR"
echo ""

# Build the project
echo "Building project..."
"$CMAKE" --build "$BUILD_DIR"
BUILD_RESULT=$?

if [ $BUILD_RESULT -ne 0 ]; then
    echo ""
    echo "Error: Build failed with exit code $BUILD_RESULT"
    exit $BUILD_RESULT
fi

echo ""
echo "Build successful. Running tests..."
echo ""

# Run ctest with output on failure and pass any additional arguments
"$CTEST" --test-dir "$BUILD_DIR" --output-on-failure "$@"
TEST_RESULT=$?

# Clean up temporary test artifacts
if [ -d "$BUILD_DIR/Testing/Temporary" ]; then
    rm -rf "$BUILD_DIR/Testing/Temporary"
fi

exit $TEST_RESULT
