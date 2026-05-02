#include "kotodama/languagemanagerdialog.h"
#include "kotodama/constants.h"
#include "kotodama/languageeditdialog.h"
#include "kotodama/languagemanager.h"
#include "kotodama/languagemodulemanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QProgressDialog>

#include <memory>

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
    languageTable->setColumnCount(7);
    languageTable->setHorizontalHeaderLabels({"Name", "Code", "Regex Pattern", "Char-based", "Token limit", "Type", "Module"});
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
    languageTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
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
    moduleButton = new QPushButton("Install support module");

    editButton->setEnabled(false);
    deleteButton->setEnabled(false);
    moduleButton->setVisible(false);

    connect(addButton, &QPushButton::clicked, this, &LanguageManagerDialog::onAddClicked);
    connect(editButton, &QPushButton::clicked, this, &LanguageManagerDialog::onEditClicked);
    connect(deleteButton, &QPushButton::clicked, this, &LanguageManagerDialog::onDeleteClicked);
    connect(moduleButton, &QPushButton::clicked, this, &LanguageManagerDialog::onModuleClicked);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addWidget(moduleButton);
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

        // Module status
        QString moduleText;
        if (LanguageManager::languageRequiresModule(lang.code())) {
            moduleText = LanguageModuleManager::instance().isInstalled(lang.code())
                         ? "Installed" : "Not installed";
        } else {
            moduleText = "—";
        }
        QTableWidgetItem* moduleItem = new QTableWidgetItem(moduleText);
        moduleItem->setTextAlignment(Qt::AlignCenter);
        languageTable->setItem(row, 6, moduleItem);

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
        moduleButton->setVisible(false);
        return;
    }

    int selectedRow = languageTable->currentRow();
    bool isBuiltIn = isBuiltInRow(selectedRow);

    // Edit button enabled only for custom languages
    editButton->setEnabled(!isBuiltIn);

    // Delete button enabled only for custom languages
    deleteButton->setEnabled(!isBuiltIn);

    updateModuleButton();
}

QString LanguageManagerDialog::selectedLanguageCode() const
{
    int row = languageTable->currentRow();
    if (row < 0) return QString();
    QTableWidgetItem* codeItem = languageTable->item(row, 1);
    return codeItem ? codeItem->text() : QString();
}

void LanguageManagerDialog::updateModuleButton()
{
    QString code = selectedLanguageCode();
    if (code.isEmpty() || !LanguageManager::languageRequiresModule(code)) {
        moduleButton->setVisible(false);
        return;
    }
    moduleButton->setVisible(true);
    if (LanguageModuleManager::instance().isInstalled(code)) {
        moduleButton->setText("Remove support module");
    } else {
        moduleButton->setText("Install support module");
    }
}

void LanguageManagerDialog::onModuleClicked()
{
    QString code = selectedLanguageCode();
    if (code.isEmpty() || !LanguageManager::languageRequiresModule(code)) {
        return;
    }

    auto& mgr = LanguageModuleManager::instance();

    if (mgr.isInstalled(code)) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Remove support module",
            QString("Remove the support module for %1? You will need to "
                    "reinstall it to use this language again.")
                .arg(languageTable->item(languageTable->currentRow(), 0)->text()),
            QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            mgr.uninstall(code);
            populateTable();
            updateModuleButton();
        }
        return;
    }

    // Install flow with progress dialog.
    // Kick off install BEFORE wiring signal connections. If install() bails
    // (busy), we just discard the dialog — the still-running install for
    // some other code path is unaffected.
    if (!mgr.install(code)) {
        QMessageBox::warning(this, "Install failed",
            "Another module install is already in progress.");
        return;
    }

    QProgressDialog* progress = new QProgressDialog(
        QString("Downloading %1 support module…")
            .arg(languageTable->item(languageTable->currentRow(), 0)->text()),
        "Cancel", 0, 100, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setValue(0);

    // Track connections so we can disconnect all three when the install
    // finishes. Heap-alloc so lambdas can capture by value and still
    // observe the populated struct.
    struct ModuleConns {
        QMetaObject::Connection progressConn;
        QMetaObject::Connection extractingConn;
        QMetaObject::Connection finishedConn;
    };
    auto conns = std::make_shared<ModuleConns>();

    conns->progressConn = connect(&mgr, &LanguageModuleManager::progress, this,
        [progress, code](const QString& lang, qint64 received, qint64 total) {
            if (lang != code) return;
            if (total > 0) {
                progress->setMaximum(100);
                progress->setValue(static_cast<int>((received * 100) / total));
            } else {
                progress->setMaximum(0);  // indeterminate
            }
        });

    conns->extractingConn = connect(&mgr, &LanguageModuleManager::extracting, this,
        [progress, code](const QString& lang) {
            if (lang != code) return;
            progress->setLabelText("Extracting support module…");
            progress->setMaximum(0);  // indeterminate
        });

    conns->finishedConn = connect(&mgr, &LanguageModuleManager::finished, this,
        [this, progress, code, conns](const QString& lang, bool success, const QString& err) {
            if (lang != code) return;
            disconnect(conns->progressConn);
            disconnect(conns->extractingConn);
            disconnect(conns->finishedConn);
            progress->close();
            progress->deleteLater();
            if (success) {
                QMessageBox::information(this, "Module installed",
                    "Support module installed successfully.");
            } else {
                QMessageBox::warning(this, "Install failed",
                    QString("Could not install support module:\n\n%1").arg(err));
            }
            populateTable();
            updateModuleButton();
        });

    connect(progress, &QProgressDialog::canceled, &mgr, &LanguageModuleManager::cancel);
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
