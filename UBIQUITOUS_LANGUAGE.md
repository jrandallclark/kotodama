# Ubiquitous Language: Kotodama Reader

## Text Display & Navigation

| Term | Definition | Aliases to avoid |
|------|------------|------------------|
| **Token** | A unit of text (word or character) identified by the tokenizer | Word, segment |
| **valid Token** | A **Token** that can receive **Focus**: any contiguous sequence of characters classified as a token by the tokenizer (excludes whitespace and punctuation). | Navigable token, focusable token |
| **Term** | A single **Token** or a phrase (sequence of multiple **Tokens**) that exists in the vocabulary database with associated metadata. Created from **Selection** for multi-token terms or by adding a single **Token** directly | Vocabulary item, dictionary entry |
| **Focus** | The currently active token for keyboard navigation and preview, shown with underline and subtle background indication | Soft selection, soft highlight, cursor, hover selection |
| **Highlight** | Background color applied to tokens indicating their learning level (known, learning, unknown, etc.) | Color, background |
| **Selection** | Drag-to-select text operation for creating multi-token terms | Text selection, drag selection |
| **Preview** | Displaying information for the focused **Token** in the info panel without entering **Edit Mode**. Shows term details for known **Terms**, or a "not in vocabulary" message with an "Add as Term" action for unknown **Tokens**. | Hover info, tooltip |
| **Edit Mode** | State where the info panel allows editing pronunciation, definition, and level | Editing, modify mode |

## User Interactions

| Term | Definition | Aliases to avoid |
|------|------------|------------------|
| **Focus Navigation** | Moving the Focus between tokens using Left/Right arrow keys | Keyboard navigation, token navigation |
| **Set Focus** | Establishing Focus on a token via mouse hover or keyboard navigation | Activate, select |
| **Clear Focus** | Removing Focus from the current token | Deselect, unfocus |
| **Commit** | Saving term changes and exiting edit mode | Save, apply |
| **Dismiss** | Exiting edit mode without saving changes | Cancel, close |

## Relationships

- A **Token** may or may not be a known **Term**
- **Focus** can be set by mouse hover or **Focus Navigation**
- **Focus Navigation** always continues from the current **Focus** position
- When no **Focus** exists, the first **Focus Navigation** action (e.g., pressing Right arrow) **Sets Focus** on the first valid **Token** visible in the viewport
- Mouse hover **Sets Focus** only when hovering over a valid **Token**
- If mouse hover **Sets Focus** on a **Token** after keyboard navigation, subsequent **Focus Navigation** continues from the hover-set **Focus**
- Pressing Space on a **Token** with **Focus** opens **Edit Mode**
- **Edit Mode** displays the **Preview** area as editable fields

## Example Dialogue

> **Dev:** "When a user hovers over a **Token**, should we **Set Focus** immediately?"

> **Domain Expert:** "Yes — hovering **Sets Focus** and shows the **Preview**. But **Focus Navigation** with arrow keys should continue from wherever the **Focus** currently is."

> **Dev:** "What happens if the user moves the mouse to an area between **Tokens**?"

> **Domain Expert:** "The **Focus** stays where it was. Only valid **Tokens** can receive **Focus** — moving over whitespace or punctuation doesn't change anything."

> **Dev:** "And if they've opened **Edit Mode** for a term, then hit Escape?"

> **Domain Expert:** "The panel dismisses but **Focus** remains on that **Token**. They can immediately press Right to move **Focus** to the next **Token**."

## Flagged Ambiguities

- "Selection" was used to mean both drag-to-select text and the Focus concept. These are distinct: **Selection** creates text ranges for adding terms, while **Focus** indicates the active token for viewing/navigation.
- "Highlight" was overloaded between the learning-level background colors and the Focus indication. The Focus uses an underline + subtle background, while level **Highlights** use solid background colors.
