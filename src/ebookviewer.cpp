#include "kotodama/ebookviewer.h"
#include "kotodama/termmanager.h"
#include "kotodama/thememanager.h"
#include "kotodama/languagemanager.h"
#include "kotodama/languageconfig.h"
#include "kotodama/tokenizer.h"
#include "kotodama/databasemanager.h"
#include "kotodama/constants.h"

#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QTextCursor>
#include <QSettings>
#include <QFrame>
#include <QDebug>
#include <QRegularExpression>
#include <QCloseEvent>
#include <QScrollBar>

EbookViewer::EbookViewer(QWidget *parent)
    : QMainWindow(parent), infoPanelPinned(false)
{
    // Create central widget and layout
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);
    layout->setContentsMargins(Constants::Margins::SMALL, Constants::Margins::SMALL,
                                Constants::Margins::SMALL, Constants::Margins::SMALL);
    layout->setSpacing(0);

    // Setup the text display
    textDisplay = new QTextBrowser(centralWidget);

    // Allow text interaction for cursor positioning, but we'll clear selections manually
    textDisplay->setTextInteractionFlags(Qt::TextBrowserInteraction);

    // Set theme-aware colors for text display
    QString selectionBg = ThemeManager::instance().isDarkMode() ? "rgba(66, 165, 245, 0.15)" : "rgba(33, 150, 243, 0.12)";
    textDisplay->setStyleSheet(QString(R"(
        QTextBrowser {
            background-color: %1;
            color: %2;
            border: none;
            selection-background-color: %3;
            selection-color: %2;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::CardBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary))
       .arg(selectionBg));

    layout->addWidget(textDisplay);

    textDisplay->viewport()->setMouseTracking(true);
    textDisplay->viewport()->installEventFilter(this);

    // Create info panel widget
    infoPanel = new TermInfoPanel(centralWidget);
    connect(infoPanel, &TermInfoPanel::termSaved, this, &EbookViewer::onTermSaved);
    connect(infoPanel, &TermInfoPanel::editCancelled, this, &EbookViewer::onEditCancelled);
    connect(infoPanel, &TermInfoPanel::termDeleted, this, &EbookViewer::onTermDeleted);

    layout->addWidget(infoPanel);
    setCentralWidget(centralWidget);

    // Setup reading position tracking
    positionSaveTimer = new QTimer(this);
    positionSaveTimer->setSingleShot(true);
    positionSaveTimer->setInterval(Constants::Timing::POSITION_SAVE_DELAY_MS);
    connect(positionSaveTimer, &QTimer::timeout, this, &EbookViewer::saveReadingPosition);

    // Connect scroll events
    connect(textDisplay->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &EbookViewer::onScrollChanged);

    // Set window properties
    resize(Constants::Window::DEFAULT_WIDTH, Constants::Window::DEFAULT_HEIGHT);
    setWindowTitle("Ebook Viewer");
}

EbookViewer::~EbookViewer()
{
}

bool EbookViewer::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == textDisplay->viewport() && event->type() == QEvent::MouseMove) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        handleMouseHover(mouseEvent);
        return false; // Let the event continue to be processed
    }
    return QMainWindow::eventFilter(obj, event); // Standard event processing
}

void EbookViewer::loadFile(const QString& filepath, const QString& uuid, const QString& language, const QString& title)
{
    // Reset viewer state when loading new text
    infoPanelPinned = false;
    pinnedTermText.clear();
    pinnedLanguage.clear();
    infoPanel->reset();

    QFile file(filepath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + filepath);
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    currentFile = filepath;
    currentUuid = uuid;

    // Load content into model (model handles all analysis)
    model.loadContent(content, language);

    // Apply font size from settings
    QSettings settings;
    int fontSize = settings.value("font/size", Constants::Font::SIZE_DEFAULT).toInt();
    QFont font = textDisplay->font();
    font.setPointSize(fontSize);
    textDisplay->setFont(font);

    // Display plain text
    textDisplay->setPlainText(content);

    // Apply highlights from model
    applyHighlights();

    // Clear any text selection
    QTextCursor cursor = textDisplay->textCursor();
    cursor.clearSelection();
    textDisplay->setTextCursor(cursor);

    // Extract filename for window title
    QFileInfo fileInfo(filepath);
    setWindowTitle(title);

    // Restore saved reading position
    restoreReadingPosition();
}

void EbookViewer::applyHighlights()
{
    QTextDocument* document = textDisplay->document();
    QTextCursor cursor(document);

    cursor.beginEditBlock();

    // Clear any existing formatting
    QTextCharFormat plainFormat;
    cursor.select(QTextCursor::Document);
    cursor.setCharFormat(plainFormat);
    cursor.clearSelection();

    // Get highlights from model
    std::vector<HighlightInfo> highlights = model.getHighlights();

    // Pre-compute all formats to avoid repeated settings lookups
    // Cache unknown word format
    QTextCharFormat unknownFormat;
    QColor unknownColor = ThemeManager::instance().getColor(ThemeColor::LevelUnknown);
    if (unknownColor != Qt::transparent) {
        unknownFormat.setBackground(unknownColor);
    }

    // Cache known term formats
    QTextCharFormat formatNew = getFormatForLevel(TermLevel::Recognized);
    QTextCharFormat formatLearning = getFormatForLevel(TermLevel::Learning);
    QTextCharFormat formatKnown = getFormatForLevel(TermLevel::Known);
    QTextCharFormat formatWellKnown = getFormatForLevel(TermLevel::WellKnown);
    QTextCharFormat formatIgnored = getFormatForLevel(TermLevel::Ignored);

    // Apply each highlight using a single reused cursor
    for (const HighlightInfo& highlight : highlights) {
        cursor.setPosition(highlight.startPos);
        cursor.setPosition(highlight.endPos, QTextCursor::KeepAnchor);

        if (cursor.hasSelection()) {
            QTextCharFormat format;
            if (highlight.isUnknown) {
                format = unknownFormat;
            } else {
                // Use pre-computed format based on level
                switch (highlight.level) {
                    case TermLevel::Recognized:
                        format = formatNew;
                        break;
                    case TermLevel::Learning:
                        format = formatLearning;
                        break;
                    case TermLevel::Known:
                        format = formatKnown;
                        break;
                    case TermLevel::WellKnown:
                        format = formatWellKnown;
                        break;
                    case TermLevel::Ignored:
                        format = formatIgnored;
                        break;
                }
            }
            cursor.setCharFormat(format);
        }
        cursor.clearSelection();
    }

    cursor.endEditBlock();
}

void EbookViewer::handleMouseHover(QMouseEvent* event)
{
    // Don't update preview if panel is pinned in edit mode
    if (infoPanelPinned) {
        return;
    }

    // Event comes from viewport event filter, so pos() is in viewport coordinates
    QPoint viewportPos = event->pos();
    QTextCursor cursor = textDisplay->cursorForPosition(viewportPos);
    int pos = cursor.position();

    // Adjust position based on which half of the character the mouse is over
    // This fixes hovering issues with character-based languages (Japanese, Chinese)
    pos = adjustPositionForCharacterCenter(viewportPos, pos);

    Term* term = model.findTermAtPosition(pos);

    if (term) {
        // Known term - show full preview
        infoPanel->showPreview(*term);
    } else {
        // Check if there's a token at this position (unknown word)
        int tokenIdx = model.findTokenAtPosition(pos);
        if (tokenIdx >= 0) {
            const std::vector<TokenInfo>& tokens = model.getTokenBoundaries();
            QString tokenText = tokens[tokenIdx].text;

            // Show preview for unknown token with hint to add
            Term unknownTerm;
            unknownTerm.term = tokenText;
            unknownTerm.pronunciation = "";
            unknownTerm.definition = "Click to add definition";
            infoPanel->showPreview(unknownTerm);
        } else {
            // No term or token - reset to default state
            infoPanel->reset();
        }
    }
}

void EbookViewer::mouseMoveEvent(QMouseEvent* event)
{
    QMainWindow::mouseMoveEvent(event);
}

void EbookViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Don't handle clicks inside the info panel
        if (!infoPanel->geometry().contains(event->pos())) {
            handleTextSelection(event);

            // Clear any visual text selection after processing the click
            QTextCursor cursor = textDisplay->textCursor();
            cursor.clearSelection();
            textDisplay->setTextCursor(cursor);
        }
    }
    QMainWindow::mouseReleaseEvent(event);
}

void EbookViewer::handleTextSelection(QMouseEvent* event)
{
    QTextCursor cursor = textDisplay->textCursor();
    QString selectedText;
    Term* existingTerm = nullptr;

    if (cursor.hasSelection()) {
        // First check if they clicked inside a multi-word term
        int clickPos = cursor.selectionStart();

        // Adjust click position based on character bounds (same logic as hover)
        // Event comes from mouseReleaseEvent, convert to viewport coordinates
        QPoint viewportPos = textDisplay->viewport()->mapFromGlobal(event->globalPos());
        clickPos = adjustPositionForCharacterCenter(viewportPos, clickPos);

        existingTerm = model.findTermAtPosition(clickPos);

        if (existingTerm && existingTerm->tokenCount > 1) {
            // Clicked on a multi-word term - use the full term
            selectedText = existingTerm->term;
        } else {
            // Get token-snapped selection from model
            selectedText = model.getTokenSelectionText(
                cursor.selectionStart(),
                cursor.selectionEnd()
            );
        }
    } else {
        // Single click - check if it's on a multi-word term first
        int clickPos = cursor.position();

        // Adjust click position based on character bounds (same logic as hover)
        // Event comes from mouseReleaseEvent, convert to viewport coordinates
        QPoint viewportPos = textDisplay->viewport()->mapFromGlobal(event->globalPos());
        clickPos = adjustPositionForCharacterCenter(viewportPos, clickPos);

        existingTerm = model.findTermAtPosition(clickPos);

        if (existingTerm) {
            // Clicked on a term (single or multi-word)
            selectedText = existingTerm->term;
        } else {
            // Not on a term - select token under cursor
            int tokenIdx = model.findTokenAtPosition(clickPos);
            if (tokenIdx >= 0) {
                const std::vector<TokenInfo>& tokens = model.getTokenBoundaries();
                selectedText = tokens[tokenIdx].text;
            }
        }
    }

    if (selectedText.isEmpty()) {
        // If already pinned and clicking on nothing, unpin the panel
        if (infoPanelPinned) {
            infoPanelPinned = false;
            pinnedTermText.clear();
            pinnedLanguage.clear();
            infoPanel->reset();
        }
        return;
    }

    // Clean the selected text using TermManager
    CleanedText cleaned = TermManager::cleanTermText(selectedText, model.getLanguage());

    if (cleaned.cleaned.isEmpty()) {
        QMessageBox::information(this, "Invalid Selection",
                                 "The selection contains no valid text.");
        return;
    }

    // Validate term size before showing edit panel
    QString language = model.getLanguage();
    LanguageConfig config = LanguageManager::instance().getLanguageByCode(language);
    int maxSize = config.tokenLimit();
    bool termTooLong = false;
    QString warningMessage;

    if (config.isCharBased()) {
        // Character-based language: count characters
        int charCount = cleaned.cleaned.length();
        if (charCount > maxSize) {
            termTooLong = true;
            warningMessage = QString("Term too long. Maximum %1 characters allowed for %2.")
                              .arg(maxSize)
                              .arg(config.name());
        }
    } else {
        // Word-based language: count tokens
        auto tokenResults = config.tokenizer()->tokenize(cleaned.cleaned);
        int tokenCount = static_cast<int>(tokenResults.size());

        if (tokenCount > maxSize) {
            termTooLong = true;
            warningMessage = QString("Term too long. Maximum %1 words allowed for %2.")
                              .arg(maxSize)
                              .arg(config.name());
        }
    }

    if (termTooLong) {
        // Show warning in the panel
        infoPanel->showWarning(cleaned.cleaned, warningMessage);

        // Pin the panel so clicking elsewhere dismisses it
        infoPanelPinned = true;
        pinnedTermText = cleaned.cleaned;
        pinnedLanguage = model.getLanguage();
        return;
    }

    bool exists = TermManager::instance().termExists(cleaned.cleaned, model.getLanguage());

    // Show edit mode in the panel instead of opening dialog
    if (exists) {
        Term term = TermManager::instance().getTerm(cleaned.cleaned, model.getLanguage());
        infoPanel->showEdit(term.term, model.getLanguage(), term.pronunciation, term.definition, term.level);
    } else {
        infoPanel->showEdit(cleaned.cleaned, model.getLanguage(), "", "", TermLevel::Recognized);
    }

    // Pin the panel for editing
    infoPanelPinned = true;
    pinnedTermText = cleaned.cleaned;
    pinnedLanguage = model.getLanguage();
}

int EbookViewer::adjustPositionForCharacterCenter(QPoint viewportPos, int textPos)
{
    // No adjustment needed at the very start of the document
    if (textPos <= 0) {
        return textPos;
    }

    // mousePos is now in viewport coordinates
    // We need to check both the character before and at the cursor position

    // Create cursors to get bounding rectangles for adjacent characters
    QTextCursor cursorBefore(textDisplay->document());
    cursorBefore.setPosition(textPos - 1);
    cursorBefore.setPosition(textPos, QTextCursor::KeepAnchor);

    // Get the bounding rectangle of the character before the cursor
    // cursorRect returns coordinates in viewport's coordinate system
    QRect charRectBefore = textDisplay->cursorRect(cursorBefore);

    // Check if there's a character at the current position
    QTextCursor cursorAt(textDisplay->document());
    cursorAt.setPosition(textPos);
    cursorAt.setPosition(textPos + 1, QTextCursor::KeepAnchor);
    QRect charRectAt = textDisplay->cursorRect(cursorAt);

    // Calculate distances from mouse to the center of each character
    qreal distToBefore = qAbs(viewportPos.x() - charRectBefore.center().x());
    qreal distToAt = qAbs(viewportPos.x() - charRectAt.center().x());

    // Select whichever character the mouse is closer to
    if (distToBefore < distToAt) {
        return textPos - 1;
    }

    return textPos;
}

QTextCharFormat EbookViewer::getFormatForLevel(TermLevel level)
{
    QTextCharFormat format;
    QColor bgColor;

    switch (level) {
    case TermLevel::Recognized:
        bgColor = ThemeManager::instance().getColor(ThemeColor::LevelRecognized);
        format.setBackground(bgColor);
        break;
    case TermLevel::Learning:
        bgColor = ThemeManager::instance().getColor(ThemeColor::LevelLearning);
        format.setBackground(bgColor);
        break;
    case TermLevel::Known:
        bgColor = ThemeManager::instance().getColor(ThemeColor::LevelKnown);
        format.setBackground(bgColor);
        break;
    case TermLevel::WellKnown:
        bgColor = ThemeManager::instance().getColor(ThemeColor::LevelWellKnown);
        if (bgColor != Qt::transparent) {
            format.setBackground(bgColor);
            // Set text color based on background brightness
            int brightness = (bgColor.red() * Constants::Color::BRIGHTNESS_RED_WEIGHT +
                            bgColor.green() * Constants::Color::BRIGHTNESS_GREEN_WEIGHT +
                            bgColor.blue() * Constants::Color::BRIGHTNESS_BLUE_WEIGHT) /
                           Constants::Color::BRIGHTNESS_DIVISOR;
            format.setForeground(brightness > Constants::Color::BRIGHTNESS_THRESHOLD ? Qt::black : Qt::white);
        }
        break;
    case TermLevel::Ignored:
        bgColor = ThemeManager::instance().getColor(ThemeColor::LevelIgnored);
        format.setBackground(bgColor);
        break;
    }
    return format;
}

void EbookViewer::onTermSaved(QString pronunciation, QString definition, TermLevel level)
{
    if (!infoPanelPinned) {
        return;
    }

    bool exists = TermManager::instance().termExists(pinnedTermText, pinnedLanguage);
    bool success = false;

    if (exists) {
        success = TermManager::instance().updateTerm(pinnedTermText, pinnedLanguage,
                                                      level, definition, pronunciation);
        if (!success) {
            QMessageBox::warning(this, "Error Updating Term",
                               QString("Failed to update the term \"%1\".\n\n"
                                      "Possible causes:\n"
                                      "• Database is locked by another process\n"
                                      "• Insufficient disk space\n\n"
                                      "Please try again. If the problem persists, restart the application.")
                               .arg(pinnedTermText));
            return; // Keep panel open so user can try again
        }
    } else {
        success = TermManager::instance().addTerm(pinnedTermText, pinnedLanguage,
                                                   level, definition, pronunciation);
        if (!success) {
            QMessageBox::warning(this, "Error Adding Term",
                               QString("Failed to add the term \"%1\".\n\n"
                                      "Possible causes:\n"
                                      "• Term already exists (try updating instead)\n"
                                      "• Database is locked by another process\n"
                                      "• Insufficient disk space\n\n"
                                      "Please try again. If the problem persists, restart the application.")
                               .arg(pinnedTermText));
            return; // Keep panel open so user can try again
        }
    }

    // Refresh term matches only (don't re-tokenize - much faster)
    model.refreshTermMatches();
    applyHighlights();

    // Unpin the panel
    infoPanelPinned = false;
    pinnedTermText.clear();
    pinnedLanguage.clear();
    infoPanel->reset();
}

void EbookViewer::onEditCancelled()
{
    // Unpin the panel
    infoPanelPinned = false;
    pinnedTermText.clear();
    pinnedLanguage.clear();
    infoPanel->reset();
}

void EbookViewer::onTermDeleted()
{
    if (!infoPanelPinned) {
        return;
    }

    // Delete the term if it exists
    if (TermManager::instance().termExists(pinnedTermText, pinnedLanguage)) {
        bool success = TermManager::instance().deleteTerm(pinnedTermText, pinnedLanguage);

        if (!success) {
            QMessageBox::warning(this, "Error Deleting Term",
                               QString("Failed to delete the term \"%1\".\n\n"
                                      "Possible causes:\n"
                                      "• Database is locked by another process\n"
                                      "• Insufficient permissions\n\n"
                                      "Please try again. If the problem persists, restart the application.")
                               .arg(pinnedTermText));
            return; // Keep panel open so user can try again
        }

        // Refresh term matches and highlights
        model.refreshTermMatches();
        applyHighlights();
    }

    // Unpin the panel
    infoPanelPinned = false;
    pinnedTermText.clear();
    pinnedLanguage.clear();
    infoPanel->reset();
}


void EbookViewer::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape && infoPanelPinned) {
        // Unpin the panel
        infoPanelPinned = false;
        pinnedTermText.clear();
        pinnedLanguage.clear();
        infoPanel->reset();
        event->accept();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

void EbookViewer::saveReadingPosition()
{
    if (currentUuid.isEmpty()) {
        return;
    }

    int scrollPosition = textDisplay->verticalScrollBar()->value();
    DatabaseManager::instance().updateReadingPosition(currentUuid, scrollPosition);
}

void EbookViewer::restoreReadingPosition()
{
    if (currentUuid.isEmpty()) {
        return;
    }

    TextInfo textInfo = DatabaseManager::instance().getText(currentUuid);
    if (textInfo.readingPosition > 0) {
        // Use a single-shot timer to restore position after the text browser has finished layout
        QTimer::singleShot(0, [this, textInfo]() {
            textDisplay->verticalScrollBar()->setValue(textInfo.readingPosition);
        });
    }
}

void EbookViewer::onScrollChanged()
{
    // Restart the timer - position will be saved 2 seconds after scrolling stops
    positionSaveTimer->start();
}

void EbookViewer::closeEvent(QCloseEvent* event)
{
    // Save position immediately when closing
    saveReadingPosition();
    QMainWindow::closeEvent(event);
}
