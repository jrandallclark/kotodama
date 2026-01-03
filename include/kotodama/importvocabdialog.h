#ifndef IMPORTVOCABDIALOG_H
#define IMPORTVOCABDIALOG_H

#include "term.h"

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QTableWidget>
#include <QGroupBox>
#include <QCloseEvent>

class ImportVocabDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportVocabDialog(QWidget *parent = nullptr);

    QString getSelectedFile() const { return selectedFilePath; }
    QString getSelectedLanguage() const;

    struct ImportData {
        QString term;
        QString definition;
        QString pronunciation;
        QString level;
    };

    QList<ImportData> getImportData() const;

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private slots:
    void onSelectFileClicked();
    void onSeparatorChanged();
    void onFieldMappingChanged();
    void onLevelColumnChanged();
    void onImportClicked();

private:
    // File selection
    QLabel* filePathLabel;
    QPushButton* selectFileButton;

    // Separator selection
    QComboBox* separatorCombo;

    // Preview table
    QTableWidget* previewTable;

    // Field mapping
    QComboBox* termColumnCombo;
    QComboBox* definitionColumnCombo;
    QComboBox* pronunciationColumnCombo;
    QComboBox* levelColumnCombo;

    //Level mapping
    QGroupBox* levelMappingGroup;
    QWidget* levelMappingWidget;
    QMap<QString, QComboBox*> levelMappingCombos;

    // Language
    QComboBox* languageCombo;

    // Buttons
    QPushButton* importButton;
    QPushButton* cancelButton;

    QString selectedFilePath;
    QList<QStringList> csvData;

    void setupUI();
    void updateImportButtonState();
    void loadAndPreviewCSV();
    void parseCSV(const QString& content, QChar separator);
    void updatePreview();
    void updateFieldMappingCombos();
    void updateLevelMapping();
    QChar getCurrentSeparator() const;
    QComboBox* createLevelMappingCombo();
    TermLevel getLevelMappingFor(const QString& csvLevel) const;

};

#endif // IMPORTVOCABDIALOG_H
