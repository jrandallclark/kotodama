#ifndef LANGUAGEEDITDIALOG_H
#define LANGUAGEEDITDIALOG_H

#include "languageconfig.h"

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>

class LanguageEditDialog : public QDialog
{
    Q_OBJECT

public:
    LanguageEditDialog(QWidget* parent = nullptr,
                       const LanguageConfig& config = LanguageConfig(),
                       bool isNew = true);

    LanguageConfig getLanguageConfig() const;

private slots:
    void onAccept();
    void onTestRegex();

private:
    QLineEdit* nameEdit;
    QLineEdit* codeEdit;
    QLineEdit* regexEdit;
    QSpinBox* tokenLimitSpinBox;
    QCheckBox* charBasedCheckBox;
    QPushButton* testRegexButton;
    QLabel* errorLabel;

    LanguageConfig originalConfig;
    bool isNewLanguage;

    void setupUI();
    bool validate();
};

#endif // LANGUAGEEDITDIALOG_H
