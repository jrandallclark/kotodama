#ifndef EBOOKVIEWER_H
#define EBOOKVIEWER_H

#include <QMainWindow>
#include <QTextBrowser>
#include <QMouseEvent>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>

#include <memory>
#include <vector>
#include <utility>

#include "term.h"
#include "ebookviewmodel.h"
#include "terminfopanel.h"

class TermHighlighter;

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

signals:
    void termChanged(const QString& language);

private:
    QTextBrowser* textDisplay;
    QString m_currentUuid;

    // Model containing all business logic
    EbookViewModel m_model;

    // Per-paragraph QSyntaxHighlighter — avoids document-wide layout revalidation.
    // Owned by QTextDocument (QSyntaxHighlighter parent).
    TermHighlighter* m_highlighter;

    // Info panel widget
    TermInfoPanel* infoPanel;

    // Pinned state for edit mode
    bool m_infoPanelPinned;
    QString m_pinnedTermText;
    QString m_pinnedLanguage;
    int m_pinnedStartPos = -1;
    int m_pinnedEndPos = -1;

    // Highlight-batch generation counter — bumped on each save/delete so
    // pending rehighlight batches from a previous edit can be cancelled.
    int m_rebuildGeneration = 0;

    // Reading position tracking
    QTimer* positionSaveTimer;

    // Focus for keyboard navigation
    void updateFocusHighlight();
    void showEditPanelForFocusedToken();
    void updatePreviewForFocusedToken();

    // UI helpers: pure rendering, no business logic
    void showTermPreview(const TermPreview& preview);
    void showEditRequest(const EditRequest& request);

    // UI methods
    void handleTextSelection(QMouseEvent* event);
    void handleMouseHover(QMouseEvent* event);
    void saveReadingPosition();
    void restoreReadingPosition();
    int adjustPositionForCharacterCenter(QPoint viewportPos, int textPos);
    void ensureTokenVisible(int tokenIndex);
    void rehighlightBatched(std::shared_ptr<std::vector<QPair<int,int>>> ranges, int start, int gen);

private slots:
    void onTermSaved(QString pronunciation, QString definition, TermLevel level);
    void onEditCancelled();
    void onTermDeleted();
    void onScrollChanged();

};

#endif // EBOOKVIEWER_H
