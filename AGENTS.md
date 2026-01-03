# Kotodama Project

A Qt-based language learning application with robust MVC architecture, trie-based phrase matching, and comprehensive testing.

## Project Info

- **Tech Stack:** Qt 6.10.1, C++17, SQLite, Google Test 1.14.0
- **Platform:** Cross-platform (macOS/Windows/Linux) via Qt framework
- **Build System:** CMake 3.16+
- **Qt Modules:** Widgets, SQL, Network, LinguistTools
- **Version:** 0.1

## Build Commands

### Standard Build
```bash
# Configure (from project root)
cmake -B build -S .

# Build
cmake --build build
```

### Run Application
```bash
# After building
open build/kotodama.app
```

### Testing
```bash
# Run all tests
./run_tests.sh

# Or manually
cd build
ctest

# Run specific test
cd build
./tests/tst_tokenizer
```

## Project Structure

### Source Organization
```
kotodama/
├── src/                 # Source implementation files (.cpp)
├── include/kotodama/    # Header files (.h)
├── tests/               # Google Test suite (10 test files)
├── build/               # CMake build output
└── release/             # Release build artifacts
```

### Key Components

**UI Layer (View)**
- `mainwindow.cpp/h` - Main application window
- `ebookviewer.cpp/h` - Text display with highlighting
- `terminfopanel.cpp/h` - Term information panel
- `textcard.cpp/h` - Text card widget
- `importvocabdialog.cpp/h` - Vocabulary import
- `languageeditdialog.cpp/h` - Language editing dialog
- `languagemanagerdialog.cpp/h` - Language management dialog
- `languageselectiondialog.cpp/h` - Language selection dialog
- `languageselectionwidget.cpp/h` - Language selection widget
- `settingsdialog.cpp/h` - Application settings

**Business Logic Layer (Model)**
- `mainwindowmodel.cpp/h` - Text list management, import workflows
- `ebookviewmodel.cpp/h` - Text analysis, tokenization, highlighting

**Data Management (Singleton Managers)**
- `databasemanager.cpp/h` - SQLite persistence
- `librarymanager.cpp/h` - File import/export, UUID management
- `termmanager.cpp/h` - In-memory trie + term cache
- `languagemanager.cpp/h` - Language configuration management
- `thememanager.cpp/h` - Theme and appearance management
- `backupmanager.cpp/h` - Backup and restore functionality
- `aimanager.cpp/h` - AI integration management

**Core Utilities**
- `tokenizer.cpp/h` - Regex-based text tokenization
- `trienode.cpp/h` - Multi-way tree for phrase matching
- `languageconfig.cpp/h` - Language-specific regex patterns
- `progresscalculator.cpp/h` - Progress tracking and statistics
- `singleinstanceguard.cpp/h` - Single instance application enforcement

**Data Structures**
- `term.h` - Term struct with TermLevel enum (New, Learning, Known, WellKnown, Ignored)
- `constants.h` - Application-wide constants

## Architecture Patterns

### 1. MVC Pattern
- **View:** UI classes (MainWindow, EbookViewer, dialogs)
- **Model:** Business logic classes (MainWindowModel, EbookViewModel)
- **Controller:** Qt signals/slots connecting views to models

Example:
```cpp
// MainWindow (View) uses MainWindowModel (Model)
MainWindow::MainWindow() {
    model = new MainWindowModel(this);
    connect(importButton, &QPushButton::clicked, model, &MainWindowModel::importText);
}
```

### 2. Singleton Pattern
All manager classes use singleton pattern:
```cpp
static ClassName& instance();
private:
    ClassName();
    ClassName(const ClassName&) = delete;
    ClassName& operator=(const ClassName&) = delete;
```

Usage: `DatabaseManager::instance().someMethod()`

### 3. Memory Management
- Explicit cleanup in destructors
- Use of Qt parent-child ownership where appropriate
- Manual delete for non-Qt objects stored in containers

```cpp
~TermManager() {
    clearCache();
    for (Term* term : terms[language]) {
        delete term;
    }
}
```

## Code Conventions

### Naming
- **Files:** `lowercase.cpp/h` (e.g., `termmanager.cpp`)
- **Headers:** Located in `include/kotodama/` directory
- **Classes:** `PascalCase` (e.g., `TermManager`)
- **Functions:** `camelCase` (e.g., `addTerm()`)
- **Member variables:** `camelCase` (e.g., `languageComboBox`)
- **Enums:** `PascalCase` (e.g., `TermLevel::New`)

### Include Organization
```cpp
// Project headers (use kotodama/ prefix)
#include "kotodama/filename.h"

// Qt headers
#include <QtModule/Header>

// Standard library
#include <vector>
#include <string>
```

### Qt Integration
- Use Qt containers: `QList`, `QString`, `QMap`
- Use `Q_OBJECT` macro for MOC processing
- Private slots for signal handlers: `private slots:`
- QSettings for persistent configuration

## Testing Conventions

### Framework
- **Google Test (gtest)** with **Google Mock (gmock)**
- Version 1.14.0 (auto-fetched via CMake FetchContent)

### Test Files
Located in `tests/` directory:
- `tst_tokenizer.cpp` - Tokenization logic
- `tst_trienode.cpp` - Trie data structure
- `tst_languageconfig.cpp` - Language configuration
- `tst_termmanager.cpp` - Term management integration
- `tst_librarymanager.cpp` - File operations
- `tst_databasemanager.cpp` - Database operations
- `tst_ebookviewmodel.cpp` - Text analysis and highlighting
- `tst_mainwindowmodel.cpp` - Main window model logic
- `tst_backupmanager.cpp` - Backup and restore functionality
- `tst_singleinstanceguard.cpp` - Single instance guard

### Test Patterns
- **AAA Pattern:** Arrange-Act-Assert
- **Naming:** `TEST(ComponentTest, DescriptiveName)`
- **Setup:** Tests use temp database (auto-cleaned)

Example:
```cpp
TEST(TokenizerTest, BasicTokenization) {
    // Arrange
    QString text = "Hello world";

    // Act
    QStringList tokens = Tokenizer::tokenize(text, "English");

    // Assert
    ASSERT_EQ(tokens.size(), 2);
    EXPECT_EQ(tokens[0], "Hello");
}
```

### Test Entry Points
- `test_main.cpp` - Full Qt application setup with temp database
- `simple_test_main.cpp` - Minimal setup for Qt Core only

## Database

### SQLite via Qt SQL
- **Location:** User data directory via `QStandardPaths::AppDataLocation`
- **Tables:** `texts`, `terms`
- **Pattern:** Create-if-not-exists schema, parameter binding via `QSqlQuery`

Example:
```cpp
QSqlQuery query;
query.prepare("INSERT INTO terms (term, language) VALUES (:term, :lang)");
query.bindValue(":term", term);
query.bindValue(":lang", language);
if (!query.exec()) {
    qDebug() << query.lastError();
}
```

## Common Workflows

### Adding a New Feature
1. **Create test first** in `tests/tst_component.cpp`
2. **Implement in appropriate layer:**
   - UI changes → View classes
   - Business logic → Model classes
   - Data operations → Manager singletons
3. **Follow MVC separation** - UI should not contain business logic

### Refactoring
- Recent work focused on **MVC separation** (see git history)
- Extract business logic from UI classes into model classes
- Maintain singleton pattern for managers
- Update tests to match new architecture

### Memory Safety
- Watch for **dangling pointers** (recent issue addressed)
- Ensure proper cleanup in destructors
- Use Qt parent-child ownership when possible
- Manual delete for non-Qt objects in containers

## Recent Development Focus

Based on git history:
- Settings menu implementation
- MVC refactoring (MainWindow, EbookViewer)
- Test suite expansion
- Memory safety improvements (dangling pointers)

## Important Notes

### CMake Configuration
- **Dual Qt Support:** Automatically detects Qt 5 or Qt 6
- **Auto Tools:** AUTOUIC, AUTOMOC, AUTORCC enabled
- **macOS Bundle:** MACOSX_BUNDLE configuration present
- **Test Discovery:** `gtest_discover_tests()` for automatic CTest integration
- **DEV_BUILD Option:** Optional flag for development builds with separate data directories
- **Include Directories:** Configured to use `include/` directory for headers

### C++ Standard
- **C++17** required (set via `CMAKE_CXX_STANDARD 17`)
- Use modern C++ features where appropriate

### Cross-Platform Considerations
- Primary development on **macOS**
- Qt framework handles platform differences
- File paths should use Qt classes (`QDir`, `QFile`)
- Use `QStandardPaths` for platform-specific directories

## Build Troubleshooting

### Common Issues
- **Qt not found:** Ensure Qt is in CMAKE_PREFIX_PATH
- **CMake version:** Requires 3.16+
- **Test failures:** Run `./run_tests.sh` to see detailed output
- **Database errors:** Check temp directory permissions

### Clean Rebuild
```bash
rm -rf build
cmake -B build -S .
cmake --build build
```

## Code Style Preferences

- **Avoid over-engineering:** Keep solutions simple and focused
- **No unnecessary abstractions:** Three similar lines > premature abstraction
- **Explicit cleanup:** Prefer explicit destructors over RAII for complex objects
- **Qt-first:** Use Qt classes/containers over STL when working with Qt APIs
- **Test coverage:** Write tests for all business logic
- **MVC separation:** Keep UI and business logic strictly separated

## Future Sessions

When working on this project:
1. Follow MVC pattern - no business logic in UI classes
2. Use singleton pattern for managers (DatabaseManager, LibraryManager, TermManager)
3. Write tests first for new features
4. Check for memory leaks and dangling pointers
5. Use Qt classes for cross-platform compatibility
