#include "kotodama/ebookviewer.h"
#include "kotodama/highlightersyntax.h"
#include "kotodama/termmanager.h"
#include "kotodama/thememanager.h"
#include "kotodama/languagemanager.h"
#include "kotodama/databasemanager.h"
#include "kotodama/progresscalculator.h"
#include "kotodama/constants.h"

#include <algorithm>
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
#include <QTimer>

EbookViewer::EbookViewer(QWidget *parent)
    : QMainWindow(parent), m_infoPanelPinned(false)
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

    // Per-paragraph QSyntaxHighlighter — avoids document-wide layout revalidation.
    m_highlighter = new TermHighlighter(textDisplay->document(), &m_model);

    // Create info panel widget
    infoPanel = new TermInfoPanel(centralWidget);
    connect(infoPanel, &TermInfoPanel::termSaved, this, &EbookViewer::onTermSaved);
    connect(infoPanel, &TermInfoPanel::editCancelled, this, &EbookViewer::onEditCancelled);
    connect(infoPanel, &TermInfoPanel::termDeleted, this, &EbookViewer::onTermDeleted);

    layout->addWidget(infoPanel);
    setCentralWidget(centralWidget);

    // Periodic position save — debounced so the DB write only happens 2s after
    // the user stops scrolling. Guards against losing position on crash/kill.
    positionSaveTimer = new QTimer(this);
    positionSaveTimer->setSingleShot(true);
    positionSaveTimer->setInterval(Constants::Timing::POSITION_SAVE_DELAY_MS);
    connect(positionSaveTimer, &QTimer::timeout, this, &EbookViewer::saveReadingPosition);

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
    m_infoPanelPinned = false;
    m_pinnedTermText.clear();
    m_pinnedLanguage.clear();
    infoPanel->reset();

    QFile file(filepath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + filepath);
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    m_currentUuid = uuid;

    // Load content into model (model handles all analysis)
    m_model.loadContent(content, language);

    // Apply font size from settings
    QSettings settings;
    int fontSize = settings.value("font/size", Constants::Font::SIZE_DEFAULT).toInt();
    QFont font = textDisplay->font();
    font.setPointSize(fontSize);
    textDisplay->setFont(font);

    // Block autosave during load — setPlainText fires valueChanged(0) which
    // would otherwise overwrite the real saved position before restore kicks in.
    m_loadingFile = true;

    // Display plain text. QSyntaxHighlighter auto-triggers on setPlainText
    // via contentsChange — no need for explicit rehighlight() call.
    textDisplay->setPlainText(content);

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

void EbookViewer::updateFocusHighlight()
{
    int focusIndex = m_model.getFocusedTokenIndex();
    if (focusIndex < 0) {
        textDisplay->setExtraSelections({});
        return;
    }

    QPair<int, int> range = m_model.getFocusRange(focusIndex);
    if (range.first < 0 || range.second <= range.first) {
        textDisplay->setExtraSelections({});
        return;
    }

    QTextEdit::ExtraSelection sel;
    sel.cursor = QTextCursor(textDisplay->document());
    sel.cursor.setPosition(range.first);
    sel.cursor.setPosition(range.second, QTextCursor::KeepAnchor);

    QColor focusColor = ThemeManager::instance().getColor(ThemeColor::Focus);
    sel.format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    sel.format.setUnderlineColor(focusColor);

    QColor highlightBg = focusColor;
    highlightBg.setAlpha(40);
    sel.format.setBackground(highlightBg);

    textDisplay->setExtraSelections({sel});
}

void EbookViewer::rehighlightBatched(std::shared_ptr<std::vector<QPair<int,int>>> ranges,
                                      int start, int gen)
{
    if (m_rebuildGeneration != gen) return;
    if (!ranges || start >= static_cast<int>(ranges->size())) {
        updateFocusHighlight();
        return;
    }

    // 20 blocks per event-loop yield keeps UI responsive
    static const int kBatchSize = 20;

    // Short-circuit: for small ranges, apply synchronously to avoid event loop overhead.
    if (static_cast<int>(ranges->size()) <= kBatchSize) {
        m_highlighter->rehighlightForRanges(*ranges);
        updateFocusHighlight();
        return;
    }

    int end = std::min(start + kBatchSize, static_cast<int>(ranges->size()));
    std::vector<QPair<int,int>> batch(ranges->begin() + start, ranges->begin() + end);
    m_highlighter->rehighlightForRanges(batch);

    if (end < static_cast<int>(ranges->size())) {
        QTimer::singleShot(0, this, [this, ranges, end, gen]() {
            rehighlightBatched(ranges, end, gen);
        });
    } else {
        updateFocusHighlight();
    }
}

void EbookViewer::updatePreviewForFocusedToken()
{
    const TokenInfo* token = m_model.getFocusedToken();
    if (!token) {
        infoPanel->reset();
        return;
    }

    TermPreview preview = m_model.getPreviewForToken(token);
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

void EbookViewer::ensureTokenVisible(int tokenIndex)
{
    const std::vector<TokenInfo>& tokens = m_model.getTokenBoundaries();
    if (tokenIndex < 0 || tokenIndex >= static_cast<int>(tokens.size())) {
        return;
    }

    QTextCursor cursor(textDisplay->document());
    cursor.setPosition(tokens[tokenIndex].startPos);
    textDisplay->setTextCursor(cursor);
    textDisplay->ensureCursorVisible();

    cursor.clearSelection();
    textDisplay->setTextCursor(cursor);
}

void EbookViewer::showEditPanelForFocusedToken()
{
    EditRequest request = m_model.getEditRequestForFocusedToken();

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

    m_infoPanelPinned = true;
    m_pinnedTermText = request.termText;
    m_pinnedLanguage = request.language;
    const TokenInfo* focused = m_model.getFocusedToken();
    if (focused) {
        m_pinnedStartPos = focused->startPos;
        m_pinnedEndPos   = focused->endPos;
    } else {
        m_pinnedStartPos = -1;
        m_pinnedEndPos   = -1;
    }
}

void EbookViewer::handleMouseHover(QMouseEvent* event)
{
    // Don't update Focus if panel is pinned in edit mode
    if (m_infoPanelPinned) {
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
    int tokenIdx = m_model.findTokenAtPosition(pos);

    if (tokenIdx < 0) {
        // Not hovering over a token - don't change Focus
        return;
    }

    // Only update Focus if we've moved to a different token
    if (tokenIdx == m_model.getFocusedTokenIndex()) {
        return;
    }

    // Check if moving between tokens in the same term (same highlight range)
    // This prevents flicker when hovering across multi-token terms
    int previousIndex = m_model.getFocusedTokenIndex();
    if (previousIndex >= 0) {
        QPair<int, int> previousRange = m_model.getFocusRange(previousIndex);
        QPair<int, int> newRange = m_model.getFocusRange(tokenIdx);
        if (previousRange == newRange) {
            // Same term - just update the focused token index without re-highlighting
            m_model.setFocusedTokenIndex(tokenIdx);
            return;
        }
    }

    // Set Focus to new token
    if (!m_model.setFocusedTokenIndex(tokenIdx)) {
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
        request = m_model.getEditRequestForSelection(cursor.selectionStart(), cursor.selectionEnd());
    } else {
        // Single click - use getEditRequestForPosition
        int clickPos = cursor.position();

        // Adjust click position based on character bounds (same logic as hover)
        QPoint viewportPos = textDisplay->viewport()->mapFromGlobal(event->globalPos());
        clickPos = adjustPositionForCharacterCenter(viewportPos, clickPos);

        request = m_model.getEditRequestForPosition(clickPos);
    }

    if (request.termText.isEmpty()) {
        // If already pinned and clicking on nothing, unpin the panel
        if (m_infoPanelPinned) {
            m_infoPanelPinned = false;
            m_pinnedTermText.clear();
            m_pinnedLanguage.clear();
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

void EbookViewer::onTermSaved(QString pronunciation, QString definition, TermLevel level)
{
    if (!m_infoPanelPinned) {
        return;
    }

    bool exists = TermManager::instance().termExists(m_pinnedTermText, m_pinnedLanguage);

    bool success = false;

    if (exists) {
        success = TermManager::instance().updateTerm(m_pinnedTermText, m_pinnedLanguage,
                                                      level, definition, pronunciation);
    } else {
        success = TermManager::instance().addTerm(m_pinnedTermText, m_pinnedLanguage,
                                                   level, definition, pronunciation);
    }

    if (!success) {
        QMessageBox::warning(this, exists ? "Error Updating Term" : "Error Adding Term",
                           QString("Failed to save the term \"%1\".\n\n"
                                  "Please try again. If the problem persists, restart the application.")
                           .arg(m_pinnedTermText));
        return; // Keep panel open so user can try again
    }

    // Save scroll position before panel reset, which changes layout height
    int savedScroll = textDisplay->verticalScrollBar()->value();

    // Close editor immediately — user sees instant response
    m_infoPanelPinned = false;
    QString termText = m_pinnedTermText;
    QString lang = m_pinnedLanguage;
    m_pinnedTermText.clear();
    m_pinnedLanguage.clear();
    m_pinnedStartPos = -1;
    m_pinnedEndPos   = -1;
    infoPanel->reset();

    Term saved = TermManager::instance().getTerm(termText, lang);
    if (saved.term.isEmpty()) {
        saved.term  = termText;
        saved.level = level;
    }

    int capturedGen = ++m_rebuildGeneration;
    std::vector<QPair<int,int>> changed = m_model.addTermPositions(saved);

    if (!changed.empty()) {
        auto ranges = std::make_shared<std::vector<QPair<int,int>>>(std::move(changed));
        rehighlightBatched(ranges, 0, capturedGen);
    }

    ProgressCalculator::instance().invalidateProgressCache(lang);
    emit termChanged(lang);

    QTimer::singleShot(0, this, [this, savedScroll]() {
        textDisplay->verticalScrollBar()->setValue(savedScroll);
    });
}

void EbookViewer::onEditCancelled()
{
    // Save scroll position before panel reset, which changes layout height
    int savedScroll = textDisplay->verticalScrollBar()->value();

    // Unpin the panel
    m_infoPanelPinned = false;
    m_pinnedTermText.clear();
    m_pinnedLanguage.clear();
    m_pinnedStartPos = -1;
    m_pinnedEndPos   = -1;
    infoPanel->reset();

    QTimer::singleShot(0, this, [this, savedScroll]() {
        textDisplay->verticalScrollBar()->setValue(savedScroll);
    });
}

void EbookViewer::onTermDeleted()
{
    if (!m_infoPanelPinned) {
        return;
    }

    // Delete the term if it exists
    if (TermManager::instance().termExists(m_pinnedTermText, m_pinnedLanguage)) {
        bool success = TermManager::instance().deleteTerm(m_pinnedTermText, m_pinnedLanguage);

        if (!success) {
            QMessageBox::warning(this, "Error Deleting Term",
                               QString("Failed to delete the term \"%1\".\n\n"
                                      "Please try again. If the problem persists, restart the application.")
                               .arg(m_pinnedTermText));
            return; // Keep panel open so user can try again
        }

        // Save scroll position before panel reset, which changes layout height
        int savedScroll = textDisplay->verticalScrollBar()->value();

        // Close editor immediately
        m_infoPanelPinned = false;
        QString termText = m_pinnedTermText;
        QString lang = m_pinnedLanguage;
        m_pinnedTermText.clear();
        m_pinnedLanguage.clear();
        m_pinnedStartPos = -1;
        m_pinnedEndPos   = -1;
        infoPanel->reset();

        int capturedGen = ++m_rebuildGeneration;
        std::vector<QPair<int,int>> changed = m_model.removeTermPositions(termText);
        if (!changed.empty()) {
            auto ranges = std::make_shared<std::vector<QPair<int,int>>>(std::move(changed));
            rehighlightBatched(ranges, 0, capturedGen);
        }

        ProgressCalculator::instance().invalidateProgressCache(lang);
        emit termChanged(lang);

        QTimer::singleShot(0, this, [this, savedScroll]() {
            textDisplay->verticalScrollBar()->setValue(savedScroll);
        });
    } else {
        // Save scroll position before panel reset
        int savedScroll = textDisplay->verticalScrollBar()->value();

        // Close panel even if term didn't exist
        m_infoPanelPinned = false;
        m_pinnedTermText.clear();
        m_pinnedLanguage.clear();
        m_pinnedStartPos = -1;
        m_pinnedEndPos   = -1;
        infoPanel->reset();

        QTimer::singleShot(0, this, [this, savedScroll]() {
            textDisplay->verticalScrollBar()->setValue(savedScroll);
        });
    }
}


void EbookViewer::keyPressEvent(QKeyEvent* event)
{
    // Escape: unpin panel if pinned, otherwise close viewer
    if (event->key() == Qt::Key_Escape) {
        if (m_infoPanelPinned) {
            m_infoPanelPinned = false;
            m_pinnedTermText.clear();
            m_pinnedLanguage.clear();
            infoPanel->reset();
            event->accept();
            return;
        }
        close();
        event->accept();
        return;
    }

    // If panel is pinned, pass other keys through
    if (m_infoPanelPinned) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    // Handle arrow keys for focus navigation (Left/Right only)
    switch (event->key()) {
    case Qt::Key_Right:
        {
            int prevIndex = m_model.getFocusedTokenIndex();
            if (prevIndex < 0) {
                // No current focus - start at first visible token in viewport
                QTextCursor viewportCursor = textDisplay->cursorForPosition(QPoint(0, 0));
                int viewportPos = viewportCursor.position();
                int firstVisibleToken = m_model.findFirstTokenAtOrAfter(viewportPos);
                if (firstVisibleToken >= 0) {
                    m_model.setFocusedTokenIndex(firstVisibleToken);
                    updateFocusHighlight();
                    updatePreviewForFocusedToken();
                    ensureTokenVisible(m_model.getFocusedTokenIndex());
                }
            } else {
                // Already has focus - move to next token
                if (m_model.moveFocusNext()) {
                    updateFocusHighlight();
                    updatePreviewForFocusedToken();
                    ensureTokenVisible(m_model.getFocusedTokenIndex());
                }
            }
        }
        event->accept();
        return;

    case Qt::Key_Left:
        {
            int prevIndex = m_model.getFocusedTokenIndex();
            if (prevIndex < 0) {
                // No current focus - start at last visible token in viewport
                QRect viewportRect = textDisplay->viewport()->rect();
                QTextCursor viewportCursor = textDisplay->cursorForPosition(
                    QPoint(viewportRect.width() - 1, viewportRect.height() - 1));
                int viewportPos = viewportCursor.position();
                int lastVisibleToken = m_model.findLastTokenAtOrBefore(viewportPos);
                if (lastVisibleToken >= 0) {
                    m_model.setFocusedTokenIndex(lastVisibleToken);
                    updateFocusHighlight();
                    updatePreviewForFocusedToken();
                    ensureTokenVisible(m_model.getFocusedTokenIndex());
                }
            } else {
                // Already has focus - move to previous token
                if (m_model.moveFocusPrevious()) {
                    updateFocusHighlight();
                    updatePreviewForFocusedToken();
                    ensureTokenVisible(m_model.getFocusedTokenIndex());
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
    if (m_currentUuid.isEmpty() || m_loadingFile) {
        return;
    }

    int scrollPosition = textDisplay->verticalScrollBar()->value();
    DatabaseManager::instance().updateReadingPosition(m_currentUuid, scrollPosition);
}

void EbookViewer::restoreReadingPosition()
{
    if (m_currentUuid.isEmpty()) {
        m_loadingFile = false;
        return;
    }

    TextInfo textInfo = DatabaseManager::instance().getText(m_currentUuid);
    if (textInfo.readingPosition <= 0) {
        m_loadingFile = false;
        return;
    }

    int pos = textInfo.readingPosition;

    // If the scrollbar range already covers our position, set it immediately
    // and re-enable autosave. Covers the common case where layout finished
    // before we got here.
    int maxVal = textDisplay->verticalScrollBar()->maximum();
    if (pos <= maxVal) {
        textDisplay->verticalScrollBar()->setValue(pos);
        m_loadingFile = false;
        return;
    }

    // Layout is still in progress — wait for the scrollbar range to expand.
    // rangeChanged fires when the document layout pushes the max up.
    disconnect(m_restoreConnection);
    m_restoreConnection = connect(textDisplay->verticalScrollBar(), &QScrollBar::rangeChanged,
        this, [this, pos](int /*min*/, int max) {
            if (max >= pos) {
                textDisplay->verticalScrollBar()->setValue(pos);
                disconnect(m_restoreConnection);
                m_loadingFile = false;
            }
        });

    // Fallback: if the range never reaches pos (window resized, font changed,
    // etc.) force-restore with clamping after 500ms so we don't silently fail.
    QTimer::singleShot(500, this, [this, pos]() {
        if (m_loadingFile) {
            int maxVal2 = textDisplay->verticalScrollBar()->maximum();
            textDisplay->verticalScrollBar()->setValue(std::min(pos, maxVal2));
            disconnect(m_restoreConnection);
            m_loadingFile = false;
        }
    });
}

void EbookViewer::onScrollChanged()
{
    // Don't autosave while loading — setPlainText fires valueChanged(0)
    // which would overwrite the real saved position before restore completes.
    if (m_loadingFile) {
        return;
    }

    // Restart the debounce timer — position saved 2s after scrolling stops
    positionSaveTimer->start();
}

void EbookViewer::closeEvent(QCloseEvent* event)
{
    // Save position immediately on close (in addition to periodic autosave)
    saveReadingPosition();
    QMainWindow::closeEvent(event);
}
