# Kotodama Tests

This directory contains all unit tests for the Kotodama project using Google Test.

## Running Tests

### Easy way - Use the test runner script:
```bash
./run_tests.sh
```

### Run tests with verbose output:
```bash
./run_tests.sh --verbose
```

### Run specific tests:
```bash
./run_tests.sh -R tokenizer
```

### Manual way:
```bash
cd build/Qt_6_10_1_for_macOS-Debug  # Or your specific build directory
ctest
```

## Test Organization

- **Test file naming**: `tst_<component>.cpp`
- **Test suite naming**: `<Component>Test`
- **Test naming**: Descriptive, e.g., `HandlesApostrophes`, `BasicTokenization`

### Example Test Structure:
```cpp
#include <gtest/gtest.h>
#include "../tokenizer.h"

TEST(TokenizerTest, BasicTokenization) {
    // Arrange
    Tokenizer tokenizer("en");

    // Act
    auto tokens = tokenizer.tokenize("Hello world");

    // Assert
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "Hello");
    EXPECT_EQ(tokens[1], "world");
}

TEST(TokenizerTest, FrenchApostrophes) {
    Tokenizer tokenizer("fr");
    auto tokens = tokenizer.tokenize("l'odeur");

    ASSERT_EQ(tokens.size(), 1);
    EXPECT_EQ(tokens[0], "l'odeur");
}
```

## Adding New Tests

1. Create a new test file in `tests/` directory:
   ```bash
   touch tests/tst_mynewcomponent.cpp
   ```

2. Add it to `CMakeLists.txt`:
   ```cmake
   add_executable(
     mynewcomponent_test
     tests/tst_mynewcomponent.cpp
     mynewcomponent.cpp
     mynewcomponent.h
     # ... other dependencies
   )

   target_link_libraries(
     mynewcomponent_test
     GTest::gtest_main
     GTest::gmock
     Qt${QT_VERSION_MAJOR}::Core
     # ... other Qt modules if needed
   )

   gtest_discover_tests(mynewcomponent_test)
   ```

3. Rebuild and run tests:
   ```bash
   cmake --build build
   ./run_tests.sh
   ```

## Test Coverage Goals

- All core tokenization logic
- Term management (add, update, delete, search)
- Trie data structure operations
- Language-specific text processing
- Edge cases (empty strings, special characters, etc.)
