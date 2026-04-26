#include "kotodama/ebookviewer.h"
#include "kotodama/termmanager.h"
#include "kotodama/thememanager.h"
#include "kotodama/languagemanager.h"
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

    // Install event filter on textDisplay to intercept keys for navigation
    textDisplay->installEventFilter(this);

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

    // Intercept key events from textDisplay for keyboard navigation
    if (obj == textDisplay && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // Only intercept Left/Right for token navigation (Up/Down scrolls)
        switch (keyEvent->key()) {
        case Qt::Key_Right:
        case Qt::Key_Left:
        case Qt::Key_Space:
            // Process these keys in our keyPressEvent handler
            keyPressEvent(keyEvent);
            if (keyEvent->isAccepted()) {
                return true; // Event handled, don't pass to textDisplay
            }
            break;
        }
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

    // Apply focus highlight if active
    updateFocusHighlight();
}

void EbookViewer::updateFocusHighlight()
{
    int focusIndex = model.getFocusedTokenIndex();
    if (focusIndex < 0) {
        return;
    }

    // Use Model's unified focus range (expands to full term span when applicable)
    QPair<int, int> range = model.getFocusRange(focusIndex);
    if (range.first < 0 || range.second <= range.first) {
        return;
    }

    QTextCursor cursor(textDisplay->document());
    cursor.setPosition(range.first);
    cursor.setPosition(range.second, QTextCursor::KeepAnchor);

    // Get the existing format and add soft selection highlighting
    QTextCharFormat format = cursor.charFormat();

    // Get focus color from theme manager
    QColor focusColor = ThemeManager::instance().getColor(ThemeColor::Focus);

    // Add a prominent underline to show selection
    format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    format.setUnderlineColor(focusColor);

    // Also add a subtle background highlight
    QColor highlightBg = focusColor;
    highlightBg.setAlpha(40);  // Semi-transparent
    format.setBackground(highlightBg);

    cursor.setCharFormat(format);
}

void EbookViewer::updatePreviewForFocusedToken()
{
    const TokenInfo* token = model.getFocusedToken();
    if (!token) {
        infoPanel->reset();
        return;
    }

    TermPreview preview = model.getPreviewForToken(token);
    showTermPreview(preview);
}

void EbookViewer::showTermPreview(const TermPreview& preview)
{
    Term term;
    term.term = preview.term;
    term.pronunciation = preview.pronunciation;
    term.definition = preview.definition;
    infoPanel->showPreview(term);
}

void EbookViewer::clearFocusHighlight(int startPos, int endPos)
{
    if (startPos < 0 || endPos <= startPos) {
        return;
    }

    // Re-apply the base highlight for this range
    QTextCursor cursor(textDisplay->document());
    cursor.setPosition(startPos);
    cursor.setPosition(endPos, QTextCursor::KeepAnchor);

    // Find what format this range should have (same logic as base highlights)
    Term* term = model.findTermAtPosition(startPos);
    QTextCharFormat format;
    if (term) {
        format = getFormatForLevel(term->level);
    } else {
        // Unknown word format
        QColor unknownColor = ThemeManager::instance().getColor(ThemeColor::LevelUnknown);
        if (unknownColor != Qt::transparent) {
            format.setBackground(unknownColor);
        }
    }

    // Explicitly clear focus formatting
    format.setUnderlineStyle(QTextCharFormat::NoUnderline);
    cursor.setCharFormat(format);
}

void EbookViewer::ensureTokenVisible(int tokenIndex)
{
    const std::vector<TokenInfo>& tokens = model.getTokenBoundaries();
    if (tokenIndex < 0 || tokenIndex >= static_cast<int>(tokens.size())) {
        return;
    }

    QTextCursor cursor(textDisplay->document());
    cursor.setPosition(tokens[tokenIndex].startPos);
    textDisplay->setTextCursor(cursor);
    textDisplay->ensureCursorVisible();

    // Clear the cursor selection visual
    cursor.clearSelection();
    textDisplay->setTextCursor(cursor);
}

void EbookViewer::showEditPanelForFocusedToken()
{
    EditRequest request = model.getEditRequestForFocusedToken();

    if (request.termText.isEmpty()) {
        return;
    }

    showEditRequest(request);
}

void EbookViewer::showEditRequest(const EditRequest& request)
{
    if (request.showWarning) {
        infoPanel->showWarning(request.termText, request.warningMessage);
    } else if (request.exists) {
        infoPanel->showEdit(request.termText, request.language,
                           request.existingPronunciation,
                           request.existingDefinition,
                           request.existingLevel);
    } else {
        infoPanel->showEdit(request.termText, request.language,
                           "", "", TermLevel::Recognized);
    }

    infoPanelPinned = true;
    pinnedTermText = request.termText;
    pinnedLanguage = request.language;
}

void EbookViewer::handleMouseHover(QMouseEvent* event)
{
    // Don't update Focus if panel is pinned in edit mode
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

    // Check if there's a token at this position
    int tokenIdx = model.findTokenAtPosition(pos);

    if (tokenIdx < 0) {
        // Not hovering over a token - don't change Focus
        return;
    }

    // Only update Focus if we've moved to a different token
    if (tokenIdx == model.getFocusedTokenIndex()) {
        return;
    }

    // Check if moving between tokens in the same term (same highlight range)
    // This prevents flicker when hovering across multi-token terms
    int previousIndex = model.getFocusedTokenIndex();
    if (previousIndex >= 0) {
        QPair<int, int> previousRange = model.getFocusRange(previousIndex);
        QPair<int, int> newRange = model.getFocusRange(tokenIdx);
        if (previousRange == newRange) {
            // Same term - just update the focused token index without re-highlighting
            model.setFocusedTokenIndex(tokenIdx);
            return;
        }
        // Clear previous focus range
        clearFocusHighlight(previousRange.first, previousRange.second);
    }

    // Set Focus to new token
    if (!model.setFocusedTokenIndex(tokenIdx)) {
        return;  // Invalid index, abort
    }
    updateFocusHighlight();

    // Update preview panel for the Focused token
    updatePreviewForFocusedToken();
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
    EditRequest request;

    if (cursor.hasSelection()) {
        // Multi-token selection - use getEditRequestForSelection
        request = model.getEditRequestForSelection(cursor.selectionStart(), cursor.selectionEnd());
    } else {
        // Single click - use getEditRequestForPosition
        int clickPos = cursor.position();

        // Adjust click position based on character bounds (same logic as hover)
        QPoint viewportPos = textDisplay->viewport()->mapFromGlobal(event->globalPos());
        clickPos = adjustPositionForCharacterCenter(viewportPos, clickPos);

        request = model.getEditRequestForPosition(clickPos);
    }

    if (request.termText.isEmpty()) {
        // If already pinned and clicking on nothing, unpin the panel
        if (infoPanelPinned) {
            infoPanelPinned = false;
            pinnedTermText.clear();
            pinnedLanguage.clear();
            infoPanel->reset();
        }
        return;
    }

    showEditRequest(request);
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
    // Handle Escape first - close panel but keep focus
    if (event->key() == Qt::Key_Escape && infoPanelPinned) {
        infoPanelPinned = false;
        pinnedTermText.clear();
        pinnedLanguage.clear();
        infoPanel->reset();
        event->accept();
        return;
    }

    // If panel is pinned, only handle Escape (above) and pass other keys through
    if (infoPanelPinned) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    // Handle arrow keys for focus navigation (Left/Right only)
    switch (event->key()) {
    case Qt::Key_Right:
        {
            int prevIndex = model.getFocusedTokenIndex();
            if (prevIndex < 0) {
                // No current focus - start at first visible token in viewport
                QTextCursor viewportCursor = textDisplay->cursorForPosition(QPoint(0, 0));
                int viewportPos = viewportCursor.position();
                int firstVisibleToken = model.findFirstTokenAtOrAfter(viewportPos);
                if (firstVisibleToken >= 0) {
                    model.setFocusedTokenIndex(firstVisibleToken);
                    updateFocusHighlight();
                    updatePreviewForFocusedToken();
                    ensureTokenVisible(model.getFocusedTokenIndex());
                }
            } else {
                // Already has focus - move to next token
                QPair<int, int> prevRange = model.getFocusRange(prevIndex);
                previousFocusStartPos = prevRange.first;
                previousFocusEndPos = prevRange.second;
                if (model.moveFocusNext()) {
                    clearFocusHighlight(previousFocusStartPos, previousFocusEndPos);
                    updateFocusHighlight();
                    updatePreviewForFocusedToken();
                    ensureTokenVisible(model.getFocusedTokenIndex());
                }
            }
        }
        event->accept();
        return;

    case Qt::Key_Left:
        {
            int prevIndex = model.getFocusedTokenIndex();
            if (prevIndex < 0) {
                // No current focus - start at last visible token in viewport
                QRect viewportRect = textDisplay->viewport()->rect();
                QTextCursor viewportCursor = textDisplay->cursorForPosition(
                    QPoint(viewportRect.width() - 1, viewportRect.height() - 1));
                int viewportPos = viewportCursor.position();
                int lastVisibleToken = model.findLastTokenAtOrBefore(viewportPos);
                if (lastVisibleToken >= 0) {
                    model.setFocusedTokenIndex(lastVisibleToken);
                    updateFocusHighlight();
                    updatePreviewForFocusedToken();
                    ensureTokenVisible(model.getFocusedTokenIndex());
                }
            } else {
                // Already has focus - move to previous token
                QPair<int, int> prevRange = model.getFocusRange(prevIndex);
                previousFocusStartPos = prevRange.first;
                previousFocusEndPos = prevRange.second;
                if (model.moveFocusPrevious()) {
                    clearFocusHighlight(previousFocusStartPos, previousFocusEndPos);
                    updateFocusHighlight();
                    updatePreviewForFocusedToken();
                    ensureTokenVisible(model.getFocusedTokenIndex());
                }
            }
        }
        event->accept();
        return;

    case Qt::Key_Space:
        showEditPanelForFocusedToken();
        event->accept();
        return;

    default:
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
