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
    void onTableSelectionChanged();
    void onTableDoubleClicked(int row, int column);

private:
    QTableWidget* languageTable;
    QPushButton* addButton;
    QPushButton* editButton;
    QPushButton* deleteButton;

    void setupUI();
    void populateTable();
    bool isBuiltInRow(int row);
};

#endif // LANGUAGEMANAGERDIALOG_H
