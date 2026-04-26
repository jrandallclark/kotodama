# Kotodama

A Qt-based desktop language-learning application with robust MVC architecture, trie-based phrase matching, and comprehensive testing.

**Tech Stack:** Qt 6.10.1, C++17, SQLite, Google Test 1.14.0, CMake 3.16+

## Quick Start

```bash
# Configure
cmake -B build -S .

# Build
cmake --build build

# Run (macOS)
open build/kotodama.app

# Run (Linux / Windows)
./build/kotodama
```

## Running Tests

```bash
# All tests
./run_tests.sh

# Or via CTest
cd build && ctest | tail -30

# Specific test binary
cd build && ./tests/tst_tokenizer
```

## Architecture

### MVC Pattern

- **View:** UI classes render data and forward user actions. No business logic.
- **Model:** Business logic classes provide data structures ready for display and enforce all rules.
- **Controller:** Qt signals/slots connecting views to models.

### Singleton Managers

All data management is centralized in singletons:

- `DatabaseManager` — SQLite persistence
- `LibraryManager` — File import/export, UUID management
- `TermManager` — In-memory trie + term cache
- `LanguageManager` — Language configuration management
- `ThemeManager` — Theme and appearance management
- `BackupManager` — Backup and restore functionality
- `AIManager` — AI integration management

### Core Data Structures

- **TrieNode** — Multi-way tree for efficient longest-match phrase recognition
- **Tokenizer** — Regex-based text tokenization with language-specific patterns
- **Term** — Struct with `TermLevel` enum (`Recognized`, `Learning`, `Known`, `WellKnown`, `Ignored`)

## Project Structure

```
kotodama/
├── src/                 # Implementation files (.cpp)
├── include/kotodama/    # Header files (.h)
├── tests/               # Google Test suite (10 test files)
├── docs/                # User-facing documentation
├── images/              # Screenshots and assets
├── release/             # Pre-built binaries
├── AGENTS.md            # Developer docs (conventions, patterns, troubleshooting)
├── CMakeLists.txt       # Build configuration
└── LICENSE              # GPL v3
```

### Key Components

| Layer | Files |
|-------|-------|
| **UI (View)** | `mainwindow`, `ebookviewer`, `terminfopanel`, `textcard`, various dialogs |
| **Business Logic (Model)** | `mainwindowmodel`, `ebookviewmodel` |
| **Data Management** | `databasemanager`, `librarymanager`, `termmanager`, `languagemanager`, `thememanager`, `backupmanager`, `aimanager` |
| **Core Utilities** | `tokenizer`, `trienode`, `languageconfig`, `progresscalculator`, `singleinstanceguard` |

## Contributing

1. Read [`AGENTS.md`](AGENTS.md) for detailed conventions, architecture patterns, and build troubleshooting.
2. Follow MVC separation — business logic belongs in models, not views.
3. Use singleton pattern for manager classes.
4. Write tests for new behavior.
5. Use Qt types and patterns for cross-platform compatibility.

## License

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This application uses the Qt framework, available under the GNU Lesser General Public License (LGPL) version 3.
