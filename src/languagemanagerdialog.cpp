#include "kotodama/languagemanagerdialog.h"
#include "kotodama/constants.h"
#include "kotodama/languageeditdialog.h"
#include "kotodama/constants.h"
#include "kotodama/languagemanager.h"
#include "kotodama/constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialogButtonBox>

LanguageManagerDialog::LanguageManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    populateTable();
}

LanguageManagerDialog::~LanguageManagerDialog()
{
}

void LanguageManagerDialog::setupUI()
{
    setWindowTitle("Manage Languages");
    setMinimumSize(Constants::Dialog::LANGUAGE_MANAGER_MIN_WIDTH, Constants::Dialog::LANGUAGE_MANAGER_MIN_HEIGHT);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Table widget
    languageTable = new QTableWidget();
    languageTable->setColumnCount(6);
    languageTable->setHorizontalHeaderLabels({"Name", "Code", "Regex Pattern", "Char-based", "Token limit", "Type"});
    languageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    languageTable->setSelectionMode(QAbstractItemView::SingleSelection);
    languageTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    languageTable->horizontalHeader()->setStretchLastSection(false);
    languageTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    languageTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    languageTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    languageTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    languageTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    languageTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    languageTable->setColumnWidth(0, Constants::TableColumn::LANGUAGE_NAME_WIDTH);

    connect(languageTable, &QTableWidget::itemSelectionChanged,
            this, &LanguageManagerDialog::onTableSelectionChanged);
    connect(languageTable, &QTableWidget::cellDoubleClicked,
            this, &LanguageManagerDialog::onTableDoubleClicked);

    mainLayout->addWidget(languageTable);

    // Button layout
    QHBoxLayout* buttonLayout = new QHBoxLayout();

    addButton = new QPushButton("Add");
    editButton = new QPushButton("Edit");
    deleteButton = new QPushButton("Delete");

    editButton->setEnabled(false);
    deleteButton->setEnabled(false);

    connect(addButton, &QPushButton::clicked, this, &LanguageManagerDialog::onAddClicked);
    connect(editButton, &QPushButton::clicked, this, &LanguageManagerDialog::onEditClicked);
    connect(deleteButton, &QPushButton::clicked, this, &LanguageManagerDialog::onDeleteClicked);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    // Close button
    QDialogButtonBox* closeButtonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(closeButtonBox, &QDialogButtonBox::rejected, this, &QDialog::accept);
    mainLayout->addWidget(closeButtonBox);
}

void LanguageManagerDialog::populateTable()
{
    languageTable->setRowCount(0);

    QList<LanguageConfig> languages = LanguageManager::instance().getAllLanguages();

    for (const LanguageConfig& lang : languages) {
        int row = languageTable->rowCount();
        languageTable->insertRow(row);

        // Name
        QTableWidgetItem* nameItem = new QTableWidgetItem(lang.name());
        languageTable->setItem(row, 0, nameItem);

        // Code
        QTableWidgetItem* codeItem = new QTableWidgetItem(lang.code());
        languageTable->setItem(row, 1, codeItem);

        // Regex
        QTableWidgetItem* regexItem = new QTableWidgetItem(lang.wordRegex());
        languageTable->setItem(row, 2, regexItem);

        // Char-based
        QTableWidgetItem* charBasedItem = new QTableWidgetItem(lang.isCharBased() ? "Yes" : "No");
        charBasedItem->setTextAlignment(Qt::AlignCenter);
        languageTable->setItem(row, 3, charBasedItem);

        // Token limit
        QTableWidgetItem* tokenLimitItem = new QTableWidgetItem(QString::number(lang.tokenLimit()));
        tokenLimitItem->setTextAlignment(Qt::AlignCenter);
        languageTable->setItem(row, 4, tokenLimitItem);

        // Type
        bool isBuiltIn = LanguageManager::instance().isBuiltIn(lang.code());
        QTableWidgetItem* typeItem = new QTableWidgetItem(isBuiltIn ? "Built-in" : "Custom");
        typeItem->setTextAlignment(Qt::AlignCenter);
        languageTable->setItem(row, 5, typeItem);

        // Style built-in rows
        if (isBuiltIn) {
            for (int col = 0; col < languageTable->columnCount(); ++col) {
                QTableWidgetItem* item = languageTable->item(row, col);
                if (item) {
                    item->setBackground(QColor(240, 240, 240));
                    item->setForeground(QColor(100, 100, 100));
                }
            }
        }
    }
}

bool LanguageManagerDialog::isBuiltInRow(int row)
{
    if (row < 0 || row >= languageTable->rowCount()) {
        return false;
    }

    QTableWidgetItem* codeItem = languageTable->item(row, 1);
    if (!codeItem) {
        return false;
    }

    QString code = codeItem->text();
    return LanguageManager::instance().isBuiltIn(code);
}

void LanguageManagerDialog::onTableSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = languageTable->selectedItems();
    if (selectedItems.isEmpty()) {
        editButton->setEnabled(false);
        deleteButton->setEnabled(false);
        return;
    }

    int selectedRow = languageTable->currentRow();
    bool isBuiltIn = isBuiltInRow(selectedRow);

    // Edit button enabled only for custom languages
    editButton->setEnabled(!isBuiltIn);

    // Delete button enabled only for custom languages
    deleteButton->setEnabled(!isBuiltIn);
}

void LanguageManagerDialog::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    // Only allow editing custom languages
    if (!isBuiltInRow(row)) {
        onEditClicked();
    }
}

void LanguageManagerDialog::onAddClicked()
{
    LanguageEditDialog dialog(this, LanguageConfig(), true);
    if (dialog.exec() == QDialog::Accepted) {
        LanguageConfig newConfig = dialog.getLanguageConfig();

        if (LanguageManager::instance().addCustomLanguage(newConfig)) {
            QMessageBox::information(this, "Success",
                                     "Language '" + newConfig.name() + "' added successfully.");
            populateTable();
        } else {
            QMessageBox::warning(this, "Error",
                                 "Failed to add language. Please check the console for details.");
        }
    }
}

void LanguageManagerDialog::onEditClicked()
{
    int selectedRow = languageTable->currentRow();
    if (selectedRow < 0) {
        return;
    }

    if (isBuiltInRow(selectedRow)) {
        QMessageBox::information(this, "Cannot Edit",
                                 "Built-in languages cannot be edited.");
        return;
    }

    // Get current language config
    QString code = languageTable->item(selectedRow, 1)->text();
    LanguageConfig currentConfig = LanguageManager::instance().getLanguageByCode(code);

    LanguageEditDialog dialog(this, currentConfig, false);
    if (dialog.exec() == QDialog::Accepted) {
        LanguageConfig newConfig = dialog.getLanguageConfig();

        if (LanguageManager::instance().updateCustomLanguage(code, newConfig)) {
            QMessageBox::information(this, "Success",
                                     "Language updated successfully.");
            populateTable();
        } else {
            QMessageBox::warning(this, "Error",
                                 "Failed to update language. Please check the console for details.");
        }
    }
}

void LanguageManagerDialog::onDeleteClicked()
{
    int selectedRow = languageTable->currentRow();
    if (selectedRow < 0) {
        return;
    }

    if (isBuiltInRow(selectedRow)) {
        QMessageBox::information(this, "Cannot Delete",
                                 "Built-in languages cannot be deleted.");
        return;
    }

    QString name = languageTable->item(selectedRow, 0)->text();
    QString code = languageTable->item(selectedRow, 1)->text();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Confirm Delete",
        "Are you sure you want to delete the language '" + name + "' (" + code + ")?",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        if (LanguageManager::instance().deleteCustomLanguage(code)) {
            QMessageBox::information(this, "Success",
                                     "Language deleted successfully.");
            populateTable();
        } else {
            QMessageBox::warning(this, "Error",
                                 "Failed to delete language. Please check the console for details.");
        }
    }
}
