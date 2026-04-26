#ifndef EBOOKVIEWER_H
#define EBOOKVIEWER_H

#include <QMainWindow>
#include <QTextBrowser>
#include <QMouseEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>

#include "term.h"
#include "ebookviewmodel.h"
#include "terminfopanel.h"

class EbookViewer : public QMainWindow
{
    Q_OBJECT

public:
    explicit EbookViewer(QWidget *parent = nullptr);
    ~EbookViewer();

    void loadFile(const QString& filepath, const QString& uuid, const QString& language, const QString& title);

protected:
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    QTextBrowser* textDisplay;
    QString currentFile;
    QString currentUuid;

    // Model containing all business logic
    EbookViewModel model;

    // Info panel widget
    TermInfoPanel* infoPanel;

    // Pinned state for edit mode
    bool infoPanelPinned;
    QString pinnedTermText;
    QString pinnedLanguage;

    // Reading position tracking
    QTimer* positionSaveTimer;

    // Focus for keyboard navigation
    int previousFocusStartPos = -1;  // Track previous focus range for clearing
    int previousFocusEndPos = -1;
    void updateFocusHighlight();
    void clearFocusHighlight(int startPos, int endPos);
    void showEditPanelForFocusedToken();
    void updatePreviewForFocusedToken();

    // UI helpers: pure rendering, no business logic
    void showTermPreview(const TermPreview& preview);
    void showEditRequest(const EditRequest& request);

    // UI methods
    void applyHighlights();
    QTextCharFormat getFormatForLevel(TermLevel level);
    void handleTextSelection(QMouseEvent* event);
    void handleMouseHover(QMouseEvent* event);
    void saveReadingPosition();
    void restoreReadingPosition();
    int adjustPositionForCharacterCenter(QPoint viewportPos, int textPos);
    void ensureTokenVisible(int tokenIndex);

private slots:
    void onTermSaved(QString pronunciation, QString definition, TermLevel level);
    void onEditCancelled();
    void onTermDeleted();
    void onScrollChanged();

};

#endif // EBOOKVIEWER_H
