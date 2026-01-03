#ifndef TERMINFOPANEL_H
#define TERMINFOPANEL_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QButtonGroup>
#include <QPushButton>
#include <QVBoxLayout>

#include "term.h"

// Lightweight widget for displaying and editing term information
// Emits signals for save/cancel - parent handles business logic
class TermInfoPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TermInfoPanel(QWidget *parent = nullptr);

    // Show term in preview mode (read-only)
    void showPreview(const Term& term);

    // Show term in edit mode (editable fields)
    void showEdit(const QString& termText, const QString& language,
                  const QString& pronunciation, const QString& definition, TermLevel level);

    // Show warning message (for terms that are too long, etc.)
    void showWarning(const QString& termText, const QString& message);

    // Reset to default empty state
    void reset();

signals:
    // Emitted when user clicks Save in edit mode
    void termSaved(QString pronunciation, QString definition, TermLevel level);

    // Emitted when user clicks Cancel in edit mode
    void editCancelled();

    // Emitted when user clicks Delete in edit mode
    void termDeleted();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setupUI();
    QString getColorForLevel(TermLevel level);
    QIcon createColorIcon(const QColor& color);
    void updateRadioButtonLabels(int width);

    // Preview mode UI
    QWidget* previewContainer;
    QLabel* previewTermLabel;
    QLabel* previewPronunciationLabel;
    QLabel* previewDefinitionLabel;

    // Edit mode UI
    QWidget* editContainer;
    QLabel* editTermLabel;
    QLineEdit* pronunciationEdit;
    QLineEdit* definitionEdit;
    QPushButton* aiGenerateButton;
    QButtonGroup* levelButtonGroup;
    QRadioButton* levelNewRadio;
    QRadioButton* levelLearningRadio;
    QRadioButton* levelKnownRadio;
    QRadioButton* levelWellKnownRadio;
    QRadioButton* levelIgnoredRadio;
    QPushButton* deleteButton;
    QPushButton* cancelButton;
    QPushButton* saveButton;

    // Warning mode UI
    QWidget* warningContainer;
    QLabel* warningTermLabel;
    QLabel* warningMessageLabel;

    // Track current level for preview border color
    TermLevel currentLevel;

    // Track current language for AI requests
    QString currentLanguage;

private slots:
    void onSaveClicked();
    void onCancelClicked();
    void onDeleteClicked();
    void onAIGenerateClicked();
    void onAIDefinitionGenerated(QString definition);
    void onAIRequestFailed(QString errorMessage);
    void updateAIButtonVisibility();
};

#endif // TERMINFOPANEL_H
