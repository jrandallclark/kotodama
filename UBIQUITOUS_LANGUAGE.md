# Ubiquitous Language

## Core Domain Concepts

| Term | Definition | Aliases to avoid |
|------|------------|------------------|
| **Text** | A piece of content (book, article, etc.) being studied | Document, file, ebook |
| **Term** | A word or phrase in the target language with associated metadata | Word, vocabulary, entry |
| **Phrase** | A multi-word sequence treated as a single unit | Expression, multi-word term |
| **Language** | A natural language being learned (e.g., Japanese, Spanish) | Target language, L2 |
| **Source Language** | The user's native or reference language | Base language, L1 |

## Term Proficiency System

| Term | Definition | Aliases to avoid |
|------|------------|------------------|
| **Term Level** | The proficiency classification of a term | Status, knowledge level |
| **New** | A term the user has not yet encountered | Unknown, unseen |
| **Learning** | A term actively being studied | Studying, practicing |
| **Known** | A term the user recognizes | Familiar, learned |
| **Well Known** | A term the user knows thoroughly | Mastered, expert |
| **Ignored** | A term excluded from study | Skipped, blacklisted |

## Text Analysis

| Term | Definition | Aliases to avoid |
|------|------------|------------------|
| **Token** | A discrete unit of text (word, particle, etc.) | Segment, piece |
| **Tokenization** | The process of breaking text into tokens | Parsing, splitting |
| **Match** | A token sequence that corresponds to a known term | Hit, find |
| **Highlight** | Visual indication of a term's level in the text | Color, mark |

## Data Management

| Term | Definition | Aliases to avoid |
|------|------------|------------------|
| **Library** | Collection of imported texts | Collection, archive |
| **Import** | The process of adding external content | Load, add |
| **Vocabulary** | The set of all terms for a language | Dictionary, lexicon |
| **Backup** | A saved copy of user data | Export, snapshot |

## UI Components

| Term | Definition | Aliases to avoid |
|------|------------|------------------|
| **Viewer** | The text display with highlighting | Reader, display |
| **Panel** | An information sidebar showing term details | Sidebar, info box |
| **Card** | A compact widget showing term information | Widget, info card |

## Relationships

- A **Text** contains many **Tokens**
- A **Token** may be part of zero or more **Phrases**
- A **Term** has exactly one **Term Level** per user
- A **Library** contains many **Texts**
- A **Language** has one **Vocabulary** containing many **Terms**
- A **Match** connects a **Token** sequence to a **Term**

## Example Dialogue

> **Dev:** "When a user clicks a highlighted **Term** in the **Viewer**, what should the **Panel** display?"

> **Domain Expert:** "Show the **Term**'s **Term Level**, its source language translation, and any notes. If it's **New**, give options to mark it as **Learning** or **Ignored**."

> **Dev:** "How do we determine which **Tokens** to highlight?"

> **Domain Expert:** "During **Tokenization**, we check if **Token** sequences **Match** any **Phrase** in the **Vocabulary**. Longer **Phrases** take priority over single **Tokens**."

> **Dev:** "What happens when a user imports a **Text**?"

> **Domain Expert:** "We add it to the **Library**, run **Tokenization** to identify known **Terms**, and the **Viewer** displays it with **Highlights** based on each **Term's** **Term Level**."

## Flagged Ambiguities

- "Language" was used to mean both the natural language being learned (e.g., Japanese) and the programming context (e.g., C++). In this domain, **Language** always refers to natural languages.
- "Word" is ambiguous — it could mean a single token or any term. Use **Token** for individual units and **Term** for vocabulary entries.
- "Import" vs "Add" — **Import** specifically refers to bringing external content into the **Library**, while adding a **Term** to **Vocabulary** is simply "adding a term".
