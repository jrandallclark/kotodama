# MeCab Integration Plan: Composition-Based Tokenizer Architecture

## Overview

Integrate MeCab for Japanese morphological analysis using a **composition-based strategy pattern**. Tokenizer backends are owned by `LanguageConfig`, not hardcoded in a central switch statement.

**Key Principle:** Languages configure their own tokenization strategy. The `Tokenizer` class is a thin wrapper that delegates to a `TokenizerBackend`.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    LanguageConfig                           │
│  ┌─────────────────────────────────────────────────────┐   │
│  │  tokenizer()                                        │   │
│  │    ├── Japanese → MeCabTokenizer (if available)    │   │
│  │    │              ↓ RegexTokenizer (fallback)      │   │
│  │    ├── Chinese  → RegexTokenizer (char-based)      │   │
│  │    └── English  → RegexTokenizer (word-based)      │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      Tokenizer                              │
│              (thin wrapper, delegates)                      │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                 TokenizerBackend                            │
│                    (interface)                              │
└─────────────────────────────────────────────────────────────┘
         │                           │
         ▼                           ▼
┌──────────────────┐      ┌──────────────────┐
│ RegexTokenizer   │      │ MeCabTokenizer   │
│                  │      │ (HAVE_MECAB)     │
└──────────────────┘      └──────────────────┘
```

---

## Phase 1: Core Interface (1-2 hours)

### 1.1 Create TokenizerBackend Interface

**File:** `include/kotodama/tokenizerbackend.h`

```cpp
#ifndef TOKENIZERBACKEND_H
#define TOKENIZERBACKEND_H

#include <QString>
#include <vector>
#include <string>

struct TokenResult {
    std::string text;
    int startPos;  // character offset in source QString
    int endPos;    // exclusive end offset
};

class TokenizerBackend
{
public:
    virtual ~TokenizerBackend() = default;
    
    virtual std::vector<TokenResult> tokenize(const QString& text) = 0;
    virtual QString name() const = 0;
    virtual bool isAvailable() const { return true; }
};

#endif
```

### 1.2 Create RegexTokenizer Implementation

**File:** `include/kotodama/regextokenizer.h`

```cpp
#ifndef REGEXTOKENIZER_H
#define REGEXTOKENIZER_H

#include "kotodama/tokenizerbackend.h"
#include <QRegularExpression>

class RegexTokenizer : public TokenizerBackend
{
public:
    RegexTokenizer(const QString& pattern, bool splitIntoChars = false);
    
    std::vector<TokenResult> tokenize(const QString& text) override;
    QString name() const override { return "regex"; }
    
private:
    QRegularExpression regex;
    bool splitIntoChars;
};

#endif
```

**File:** `src/regextokenizer.cpp`

Move regex logic from `tokenizer.cpp`, preserving position data:

```cpp
std::vector<TokenResult> RegexTokenizer::tokenize(const QString& text)
{
    std::vector<TokenResult> tokens;
    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString matched = match.captured(0);
        if (matched.isEmpty()) continue;

        if (splitIntoChars) {
            int start = match.capturedStart();
            for (int i = 0; i < matched.length(); ++i) {
                tokens.push_back({
                    matched.mid(i, 1).toStdString(),
                    start + i,
                    start + i + 1
                });
            }
        } else {
            tokens.push_back({
                matched.toStdString(),
                match.capturedStart(),
                match.capturedEnd()
            });
        }
    }
    return tokens;
}
```

---

## Phase 2: MeCab Integration (2-3 hours)

### 2.1 Build System Updates

**File:** `CMakeLists.txt`

```cmake
# Find MeCab
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MECAB QUIET mecab)
endif()

if(NOT MECAB_FOUND)
    find_path(MECAB_INCLUDE_DIR mecab.h
        PATHS /usr/local/include /opt/homebrew/include /usr/include)
    find_library(MECAB_LIBRARY mecab
        PATHS /usr/local/lib /opt/homebrew/lib /usr/lib)
    if(MECAB_INCLUDE_DIR AND MECAB_LIBRARY)
        set(MECAB_FOUND TRUE)
        set(MECAB_INCLUDE_DIRS ${MECAB_INCLUDE_DIR})
        set(MECAB_LIBRARIES ${MECAB_LIBRARY})
    endif()
endif()

if(MECAB_FOUND)
    target_compile_definitions(kotodama PRIVATE HAVE_MECAB=1)
    target_include_directories(kotodama PRIVATE ${MECAB_INCLUDE_DIRS})
    target_link_libraries(kotodama PRIVATE ${MECAB_LIBRARIES})
endif()
```

### 2.2 Create MeCabTokenizer (Conditional)

**File:** `include/kotodama/mecabtokenizer.h`

```cpp
#ifndef MECABTOKENIZER_H
#define MECABTOKENIZER_H

#include "kotodama/tokenizerbackend.h"
#include <memory>

// MeCabTokenizer is always declared; isAvailable() returns false when
// HAVE_MECAB is not defined. The #ifdef is kept in the .cpp only,
// keeping mecab.h out of all consumer translation units.

class MeCabTokenizer : public TokenizerBackend
{
public:
    MeCabTokenizer();
    ~MeCabTokenizer();
    
    std::vector<TokenResult> tokenize(const QString& text) override;
    QString name() const override { return "mecab"; }
    bool isAvailable() const override;
    
private:
    class Impl;
    std::unique_ptr<Impl> d;  // null when HAVE_MECAB not defined
};

#endif // MECABTOKENIZER_H
```

**File:** `src/mecabtokenizer.cpp`

- All `#ifdef HAVE_MECAB` lives here — `mecab.h` never leaks into headers
- PIMPL `Impl` owns the `MeCab::Tagger*`
- Use `node->rlength` / `node->length` for byte offsets; convert to character offsets via `QString::fromUtf8` indexing
- Try system dictionary paths; support bundled dictionary for deployment

```cpp
#ifdef HAVE_MECAB
#include <mecab.h>

struct MeCabTokenizer::Impl {
    MeCab::Tagger* tagger = nullptr;
    Impl() { tagger = MeCab::createTagger(""); }
    ~Impl() { delete tagger; }
};

std::vector<TokenResult> MeCabTokenizer::tokenize(const QString& text)
{
    std::vector<TokenResult> tokens;
    if (!d || !d->tagger) return tokens;

    QByteArray utf8 = text.toUtf8();
    const MeCab::Node* node = d->tagger->parseToNode(utf8.constData());

    // Pre-build byte→char offset map for correct position reporting
    QList<int> byteToChar(utf8.size() + 1, -1);
    int charIdx = 0;
    for (int byteIdx = 0; byteIdx < utf8.size(); ) {
        byteToChar[byteIdx] = charIdx;
        unsigned char c = utf8[byteIdx];
        int seqLen = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        for (int k = 1; k < seqLen && byteIdx + k < utf8.size(); ++k)
            byteToChar[byteIdx + k] = charIdx;
        byteIdx += seqLen;
        ++charIdx;
    }
    byteToChar[utf8.size()] = charIdx;

    int byteOffset = 0;
    for (; node; node = node->next) {
        if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) continue;
        std::string surface(node->surface, node->length);
        int byteStart = byteOffset + (node->rlength - node->length);
        int byteEnd   = byteStart + node->length;
        tokens.push_back({
            surface,
            byteToChar[byteStart],
            byteToChar[byteEnd]
        });
        byteOffset += node->rlength;
    }
    return tokens;
}

bool MeCabTokenizer::isAvailable() const { return d && d->tagger; }

#else

std::vector<TokenResult> MeCabTokenizer::tokenize(const QString&) { return {}; }
bool MeCabTokenizer::isAvailable() const { return false; }

#endif

MeCabTokenizer::MeCabTokenizer()
#ifdef HAVE_MECAB
    : d(std::make_unique<Impl>())
#endif
{}

MeCabTokenizer::~MeCabTokenizer() = default;
```

---

## Phase 3: Tokenizer Refactoring (1-2 hours)

### 3.1 Refactor Tokenizer as Wrapper

**File:** `include/kotodama/tokenizer.h`

```cpp
#ifndef TOKENIZER_H
#define TOKENIZER_H

#include "kotodama/tokenizerbackend.h"  // for TokenResult
#include <memory>
#include <vector>

class TokenizerBackend;
class QString;

class Tokenizer
{
public:
    explicit Tokenizer(std::unique_ptr<TokenizerBackend> backend);
    
    std::vector<TokenResult> tokenize(const QString& text) const;
    bool isAvailable() const;
    QString backendName() const;
    
    // Factory methods
    static std::unique_ptr<Tokenizer> createRegex(
        const QString& pattern, bool splitIntoChars = false);
    static std::unique_ptr<Tokenizer> createJapanese();
    
private:
    std::unique_ptr<TokenizerBackend> backend;
};

#endif
```

**File:** `src/tokenizer.cpp`

```cpp
#include "kotodama/tokenizer.h"
#include "kotodama/regextokenizer.h"
#include "kotodama/mecabtokenizer.h"

Tokenizer::Tokenizer(std::unique_ptr<TokenizerBackend> backend)
    : backend(std::move(backend))
{
}

std::vector<TokenResult> Tokenizer::tokenize(const QString& text) const
{
    if (!backend) return {};
    return backend->tokenize(text);
}

bool Tokenizer::isAvailable() const
{
    return backend && backend->isAvailable();
}

QString Tokenizer::backendName() const
{
    return backend ? backend->name() : QString("null");
}

std::unique_ptr<Tokenizer> Tokenizer::createRegex(
    const QString& pattern, bool splitIntoChars)
{
    return std::make_unique<Tokenizer>(
        std::make_unique<RegexTokenizer>(pattern, splitIntoChars));
}

std::unique_ptr<Tokenizer> Tokenizer::createJapanese()
{
    auto mecab = std::make_unique<MeCabTokenizer>();
    if (mecab->isAvailable()) {
        return std::make_unique<Tokenizer>(std::move(mecab));
    }
    // Fallback: regex with character splitting
    QString jaPattern = "[\\x{3040}-\\x{309F}\\x{30A0}-\\x{30FF}\\x{4E00}-\\x{9FFF}]+";
    return createRegex(jaPattern, true);
}
```

---

## Phase 4: LanguageConfig Integration (1-2 hours)

### 4.1 Add Tokenizer to LanguageConfig

**File:** `include/kotodama/languageconfig.h`

`LanguageConfig` owns the tokenizer exclusively; callers borrow a raw pointer.
`shared_ptr` is avoided — no shared ownership exists here.

```cpp
#include <memory>
class Tokenizer;

private:
    // ... existing members ...
    mutable std::unique_ptr<Tokenizer> tokenizerCache;

public:
    // Returns a non-owning pointer; valid for the lifetime of this LanguageConfig.
    const Tokenizer* tokenizer() const;
```

### 4.2 Implement Tokenizer Creation

**File:** `src/languageconfig.cpp`

```cpp
#include "kotodama/tokenizer.h"

const Tokenizer* LanguageConfig::tokenizer() const
{
    if (!tokenizerCache) {
        if (codeValue == "ja") {
            tokenizerCache = Tokenizer::createJapanese();
        } else {
            tokenizerCache = Tokenizer::createRegex(wordRegexValue, isCharBasedValue);
        }
    }
    return tokenizerCache.get();
}
```

---

## Phase 5: Update EbookViewModel (30 minutes)

### 5.1 Replace tokenizeText()

**Current code** (`src/ebookviewmodel.cpp:37-78`) manually drives `QRegularExpression`
and builds `TokenInfo` structs with position data. Replace it entirely with a call
to the backend, which now provides positions directly.

`EbookViewModel` has its own `TokenInfo` struct (startPos/endPos/text). Map from
`TokenResult` at the call site — no changes needed to the rest of `EbookViewModel`.

**Before:**
```cpp
void EbookViewModel::tokenizeText()
{
    tokenBoundaries.clear();
    if (text.isEmpty()) return;

    QString regexStr = LanguageManager::instance().getWordRegex(language);
    QRegularExpression regex(regexStr);
    bool isCharacterBased = LanguageManager::instance().isCharacterBased(language);

    QRegularExpressionMatchIterator it = regex.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        if (isCharacterBased) {
            QString matchedText = match.captured();
            int startPos = match.capturedStart();
            for (int i = 0; i < matchedText.length(); ++i) {
                TokenInfo tokenInfo;
                tokenInfo.startPos = startPos + i;
                tokenInfo.endPos   = startPos + i + 1;
                tokenInfo.text     = matchedText.mid(i, 1);
                tokenBoundaries.push_back(tokenInfo);
            }
        } else {
            TokenInfo tokenInfo;
            tokenInfo.startPos = match.capturedStart();
            tokenInfo.endPos   = match.capturedEnd();
            tokenInfo.text     = match.captured();
            tokenBoundaries.push_back(tokenInfo);
        }
    }
}
```

**After:**
```cpp
void EbookViewModel::tokenizeText()
{
    tokenBoundaries.clear();
    if (text.isEmpty()) return;

    LanguageConfig config = LanguageManager::instance().getConfig(language);
    const Tokenizer* tokenizer = config.tokenizer();

    for (const TokenResult& result : tokenizer->tokenize(text)) {
        TokenInfo tokenInfo;
        tokenInfo.startPos = result.startPos;
        tokenInfo.endPos   = result.endPos;
        tokenInfo.text     = QString::fromStdString(result.text);
        tokenBoundaries.push_back(tokenInfo);
    }
}
```

`LanguageManager::getConfig()` may not exist yet — add it, or retrieve the
`LanguageConfig` however `LanguageManager` already exposes it. The point is:
no language-branching logic in `EbookViewModel`; the tokenizer encapsulates it.

### 5.2 Handle Unavailable Backends

`isAvailable()` is false only when MeCab was requested but failed to init.
`createJapanese()` already falls back to regex in that case, so callers rarely
need to check. Add a warning log if desired:

```cpp
if (!tokenizer->isAvailable()) {
    qWarning() << "Tokenizer backend unavailable for language:" << language;
}
// tokenize() returns {} on unavailable backend — tokenBoundaries stays empty
```

---

## Phase 6: Testing (2-3 hours)

### 6.1 Backend Tests

**File:** `tests/tst_regextokenizer.cpp`

- Test all existing regex tokenization cases
- Verify character splitting works correctly
- Test edge cases (empty, unicode, mixed scripts)

**File:** `tests/tst_mecabtokenizer.cpp`

```cpp
TEST(MeCabTokenizer, AvailableOnSystem) {
    MeCabTokenizer tokenizer;
    if (!tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab not installed";
    }
    auto tokens = tokenizer.tokenize("太郎は本を読んだ");
    EXPECT_GT(tokens.size(), 3u);
    // Positions must be monotonically increasing and non-overlapping
    for (size_t i = 1; i < tokens.size(); ++i) {
        EXPECT_GE(tokens[i].startPos, tokens[i-1].endPos);
    }
    // First token starts at 0
    EXPECT_EQ(tokens.front().startPos, 0);
}

TEST(MeCabTokenizer, UnavailableReturnsEmpty) {
    // When HAVE_MECAB not defined, isAvailable() == false and tokenize() == {}
    MeCabTokenizer tokenizer;
    if (tokenizer.isAvailable()) {
        GTEST_SKIP() << "MeCab is installed — skipping unavailable stub test";
    }
    EXPECT_FALSE(tokenizer.isAvailable());
    EXPECT_TRUE(tokenizer.tokenize("test").empty());
}
```

### 6.2 Integration Tests

**File:** `tests/tst_tokenizer.cpp` (update)

- Update existing tests to use factory methods
- Test `Tokenizer::createJapanese()` returns correct backend type
- Test fallback behavior

### 6.3 LanguageConfig Tests

**File:** `tests/tst_languageconfig.cpp` (update)

```cpp
TEST(LanguageConfig, JapaneseUsesMeCabIfAvailable) {
    LanguageConfig ja("Japanese", "ja", "...", true, 12);
    const Tokenizer* tokenizer = ja.tokenizer();
    ASSERT_NE(tokenizer, nullptr);

    bool mecabPresent = Tokenizer::createJapanese()->isAvailable();
    EXPECT_EQ(tokenizer->backendName(), mecabPresent ? "mecab" : "regex");
}

TEST(LanguageConfig, EnglishUsesRegex) {
    LanguageConfig en("English", "en", "[a-zA-Z]+", false, 6);
    const Tokenizer* tokenizer = en.tokenizer();
    ASSERT_NE(tokenizer, nullptr);
    EXPECT_EQ(tokenizer->backendName(), "regex");
}

TEST(LanguageConfig, TokenizerReturnedWithPositions) {
    LanguageConfig en("English", "en", "[a-zA-Z]+", false, 6);
    auto results = en.tokenizer()->tokenize("hello world");
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].text, "hello");
    EXPECT_EQ(results[0].startPos, 0);
    EXPECT_EQ(results[0].endPos,   5);
    EXPECT_EQ(results[1].text, "world");
    EXPECT_EQ(results[1].startPos, 6);
    EXPECT_EQ(results[1].endPos,  11);
}
```

---

## Phase 7: Documentation & Settings (1 hour)

### 7.1 CLAUDE.md Updates

Add section explaining:
- How tokenizer composition works
- How to add new tokenization strategies
- MeCab installation requirements

### 7.2 Optional: Settings Integration

**File:** `SettingsDialog` (future enhancement)

Allow users to choose tokenizer per language:
```cpp
// Pseudocode for future settings UI
QComboBox* jaTokenizer = new QComboBox();
jaTokenizer->addItem("MeCab (recommended)", "mecab");
jaTokenizer->addItem("Character-based regex", "regex-chars");
jaTokenizer->addItem("Word-based regex", "regex-words");
```

---

## File Summary

### New Files
| File | Purpose |
|------|---------|
| `include/kotodama/tokenizerbackend.h` | Abstract interface |
| `include/kotodama/regextokenizer.h` | Regex implementation |
| `src/regextokenizer.cpp` | Regex logic (moved from tokenizer.cpp) |
| `include/kotodama/mecabtokenizer.h` | MeCab interface |
| `src/mecabtokenizer.cpp` | MeCab implementation |
| `tests/tst_regextokenizer.cpp` | Regex tokenizer tests |
| `tests/tst_mecabtokenizer.cpp` | MeCab tokenizer tests |

### Modified Files
| File | Changes |
|------|---------|
| `CMakeLists.txt` | Add MeCab detection |
| `include/kotodama/tokenizer.h` | Refactor to wrapper class |
| `src/tokenizer.cpp` | Delegate to backends, add factories |
| `include/kotodama/languageconfig.h` | Add tokenizer cache |
| `src/languageconfig.cpp` | Implement tokenizer creation |
| `src/ebookviewmodel.cpp` | Use LanguageConfig::tokenizer() |
| `tests/tst_tokenizer.cpp` | Update for new API |
| `tests/tst_languageconfig.cpp` | Add tokenizer tests |
| `CLAUDE.md` | Document architecture |

---

## Benefits

1. **No Switch Statements:** Languages own their tokenization strategy
2. **Position-Aware:** All backends return `TokenResult` with start/end offsets — no lossy intermediate step
3. **Testable:** Each backend testable in isolation
4. **Extensible:** Add new tokenizers without touching existing code
5. **Lazy Loading:** Tokenizers created on first use, cached thereafter
6. **Graceful Fallback:** MeCab unavailable → auto-fallback to regex
7. **No Header Leakage:** `mecab.h` confined to `mecabtokenizer.cpp`; `#ifdef HAVE_MECAB` never appears in public headers
8. **Clear Ownership:** `LanguageConfig` owns tokenizer via `unique_ptr`; callers get a raw `const Tokenizer*`

---

## Dependencies

### Build Dependencies
- CMake 3.16+
- Optional: MeCab development headers (`libmecab-dev`)

### Runtime Dependencies
- MeCab library (if built with HAVE_MECAB)
- MeCab dictionary (`mecab-ipadic` or `mecab-ipadic-utf8`)

### Installation Commands

**macOS:**
```bash
brew install mecab mecab-ipadic
```

**Ubuntu/Debian:**
```bash
sudo apt-get install libmecab-dev mecab-ipadic-utf8
```

---

## Future Extensions

- **Chinese:** Add `JiebaTokenizer` for Chinese word segmentation
- **Thai:** Add `LibThaiTokenizer` for Thai
- **User Dictionaries:** Allow per-language custom dictionaries
- **Settings UI:** Let users choose tokenizers per language

---

## Estimated Timeline

| Phase | Hours |
|-------|-------|
| 1: Core Interface | 1-2 |
| 2: MeCab Integration | 2-3 |
| 3: Tokenizer Refactoring | 1-2 |
| 4: LanguageConfig Integration | 1-2 |
| 5: Update EbookViewModel | 0.5 |
| 6: Testing | 2-3 |
| 7: Documentation | 1 |
| **Total** | **~11-14 hours** |
