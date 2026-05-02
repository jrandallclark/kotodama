#ifndef LANGUAGEMANAGERDIALOG_H
#define LANGUAGEMANAGERDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include "languageconfig.h"

class LanguageManagerDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LanguageManagerDialog(QWidget *parent = nullptr);
    ~LanguageManagerDialog();

private slots:
    void onAddClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onModuleClicked();
    void onTableSelectionChanged();
    void onTableDoubleClicked(int row, int column);

private:
    QTableWidget* languageTable;
    QPushButton* addButton;
    QPushButton* editButton;
    QPushButton* deleteButton;
    QPushButton* moduleButton;

    void setupUI();
    void populateTable();
    void updateModuleButton();
    bool isBuiltInRow(int row);
    QString selectedLanguageCode() const;
};

#endif // LANGUAGEMANAGERDIALOG_H
