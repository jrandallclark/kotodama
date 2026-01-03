#ifndef LANGUAGESELECTIONDIALOG_H
#define LANGUAGESELECTIONDIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>

#include "languageselectionwidget.h"

class LanguageSelectionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LanguageSelectionDialog(QWidget *parent = nullptr);
    ~LanguageSelectionDialog();

private slots:
    void onAccept();
    void onReject();
    void onSelectionChanged();

private:
    void setupUI();
    void loadCurrentSelections();
    bool validateSelections();

    LanguageSelectionWidget* languageWidget;
    QDialogButtonBox* buttonBox;
    QPushButton* okButton;

    QStringList originalSelections;
};

#endif // LANGUAGESELECTIONDIALOG_H
