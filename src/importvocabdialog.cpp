#include "kotodama/importvocabdialog.h"
#include "kotodama/term.h"
#include "kotodama/languagemanager.h"
#include "kotodama/databasemanager.h"
#include "kotodama/constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QGroupBox>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QScrollArea>

ImportVocabDialog::ImportVocabDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    setModal(true);
    setWindowTitle("Import Vocabulary");
    resize(Constants::Dialog::VOCAB_IMPORT_WIDTH, Constants::Dialog::VOCAB_IMPORT_HEIGHT);
}

void ImportVocabDialog::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // File selection section
    QGroupBox* fileGroup = new QGroupBox("CSV File");
    QHBoxLayout* fileLayout = new QHBoxLayout(fileGroup);

    selectFileButton = new QPushButton("Select File...");
    fileLayout->addWidget(selectFileButton);

    filePathLabel = new QLabel("No file selected");
    filePathLabel->setStyleSheet("color: gray; font-style: italic;");
    fileLayout->addWidget(filePathLabel, 1);

    mainLayout->addWidget(fileGroup);

    // Separator selection
    QGroupBox* separatorGroup = new QGroupBox("Field Separator");
    QHBoxLayout* separatorLayout = new QHBoxLayout(separatorGroup);

    separatorCombo = new QComboBox();
    separatorCombo->addItem("Comma (,)", ",");
    separatorCombo->addItem("Semicolon (;)", ";");
    separatorCombo->addItem("Tab", "\t");
    separatorCombo->addItem("Pipe (|)", "|");
    separatorLayout->addWidget(separatorCombo);
    separatorLayout->addStretch();

    mainLayout->addWidget(separatorGroup);

    // Preview table
    QGroupBox* previewGroup = new QGroupBox("Preview");
    QVBoxLayout* previewLayout = new QVBoxLayout(previewGroup);

    previewTable = new QTableWidget();
    previewTable->setMaximumHeight(Constants::Panel::PREVIEW_TABLE_MAX_HEIGHT);
    previewTable->setAlternatingRowColors(true);
    previewTable->horizontalHeader()->setStretchLastSection(true);
    previewLayout->addWidget(previewTable);

    QLabel* previewNote = new QLabel("Showing first 5 rows");
    previewNote->setStyleSheet("color: gray; font-size: 10px;");
    previewLayout->addWidget(previewNote);

    mainLayout->addWidget(previewGroup);

    // Field mapping
    QGroupBox* mappingGroup = new QGroupBox("Field Mapping");
    QFormLayout* mappingLayout = new QFormLayout(mappingGroup);

    termColumnCombo = new QComboBox();
    mappingLayout->addRow("Term column:", termColumnCombo);

    definitionColumnCombo = new QComboBox();
    definitionColumnCombo->addItem("(none)", -1);
    mappingLayout->addRow("Definition column:", definitionColumnCombo);

    pronunciationColumnCombo = new QComboBox();
    pronunciationColumnCombo->addItem("(none)", -1);
    mappingLayout->addRow("Pronunciation column:", pronunciationColumnCombo);

    levelColumnCombo = new QComboBox();
    levelColumnCombo->addItem("(none)", -1);
    mappingLayout->addRow("Level column:", levelColumnCombo);

    mainLayout->addWidget(mappingGroup);

    // Level mapping (initially hidden)
    levelMappingGroup = new QGroupBox("Level Mapping");
    levelMappingGroup->setVisible(false);

    QVBoxLayout* levelMappingLayout = new QVBoxLayout(levelMappingGroup);
    QLabel* levelMappingLabel = new QLabel("Map the levels from your CSV to our system:");
    levelMappingLabel->setStyleSheet("color: gray; font-size: 10px;");
    levelMappingLayout->addWidget(levelMappingLabel);

    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setMaximumHeight(Constants::Panel::SCROLL_AREA_MAX_HEIGHT);

    levelMappingWidget = new QWidget();
    scrollArea->setWidget(levelMappingWidget);

    levelMappingLayout->addWidget(scrollArea);
    mainLayout->addWidget(levelMappingGroup);

    // Language selection
    QFormLayout* languageLayout = new QFormLayout();
    languageCombo = new QComboBox();

    // Populate with user's learning languages
    QStringList userLanguageCodes = DatabaseManager::instance().getUserLanguages();
    for (const QString& code : userLanguageCodes) {
        LanguageConfig config = LanguageManager::instance().getLanguageByCode(code);
        languageCombo->addItem(config.name(), code);
    }

    languageLayout->addRow("Import to language:", languageCombo);
    mainLayout->addLayout(languageLayout);

    mainLayout->addStretch();

    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    cancelButton = new QPushButton("Cancel");
    buttonLayout->addWidget(cancelButton);

    importButton = new QPushButton("Import");
    importButton->setEnabled(false);
    buttonLayout->addWidget(importButton);

    mainLayout->addLayout(buttonLayout);

    mainLayout->addStretch();

    // Connect signals
    connect(selectFileButton, &QPushButton::clicked, this, &ImportVocabDialog::onSelectFileClicked);
    connect(separatorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportVocabDialog::onSeparatorChanged);
    connect(termColumnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportVocabDialog::onFieldMappingChanged);
    connect(definitionColumnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportVocabDialog::onFieldMappingChanged);
    connect(pronunciationColumnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportVocabDialog::onFieldMappingChanged);
    connect(levelColumnCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImportVocabDialog::onLevelColumnChanged);
    connect(importButton, &QPushButton::clicked, this, &ImportVocabDialog::onImportClicked);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void ImportVocabDialog::onSelectFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Select CSV File",
        QDir::homePath(),
        "CSV Files (*.csv *.txt);;All Files (*)"
        );

    if (!filePath.isEmpty()) {
        selectedFilePath = filePath;

        QFileInfo fileInfo(filePath);
        filePathLabel->setText(fileInfo.fileName());
        filePathLabel->setStyleSheet("color: black;");

        loadAndPreviewCSV();
        updateImportButtonState();
    }
}

QChar ImportVocabDialog::getCurrentSeparator() const
{
    QString sep = separatorCombo->currentData().toString();
    return sep.isEmpty() ? ',' : sep[0];
}

void ImportVocabDialog::loadAndPreviewCSV()
{
    QFile file(selectedFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Could not open file: " + selectedFilePath);
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    parseCSV(content, getCurrentSeparator());
    updatePreview();
    updateFieldMappingCombos();
}

void ImportVocabDialog::parseCSV(const QString& content, QChar separator)
{
    csvData.clear();

    QStringList lines = content.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines) {
        QStringList fields;
        QString currentField;
        bool inQuotes = false;

        for (int i = 0; i < line.length(); ++i) {
            QChar c = line[i];

            if (c == '"') {
                inQuotes = !inQuotes;
            } else if (c == separator && !inQuotes) {
                fields.append(currentField.trimmed());
                currentField.clear();
            } else {
                currentField.append(c);
            }
        }

        // Add last field
        fields.append(currentField.trimmed());

        if (!fields.isEmpty()) {
            csvData.append(fields);
        }
    }
}

void ImportVocabDialog::updatePreview()
{
    previewTable->clear();

    if (csvData.isEmpty()) {
        return;
    }

    // Show first 5 rows
    int rowCount = qMin(5, csvData.size());
    int colCount = csvData[0].size();

    previewTable->setRowCount(rowCount);
    previewTable->setColumnCount(colCount);

    // Set headers
    QStringList headers;
    for (int i = 0; i < colCount; ++i) {
        headers << QString::number(i + 1);
    }
    previewTable->setHorizontalHeaderLabels(headers);

    // Fill data
    for (int row = 0; row < rowCount; ++row) {
        for (int col = 0; col < colCount; ++col) {
            if (col < csvData[row].size()) {
                QTableWidgetItem* item = new QTableWidgetItem(csvData[row][col]);
                item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                previewTable->setItem(row, col, item);
            }
        }
    }

    previewTable->resizeColumnsToContents();
}

void ImportVocabDialog::updateFieldMappingCombos()
{
    if (csvData.isEmpty()) {
        return;
    }

    int colCount = csvData[0].size();

    // Block signals while updating
    termColumnCombo->blockSignals(true);
    definitionColumnCombo->blockSignals(true);
    pronunciationColumnCombo->blockSignals(true);
    levelColumnCombo->blockSignals(true);

    // Clear and repopulate
    termColumnCombo->clear();
    definitionColumnCombo->clear();
    pronunciationColumnCombo->clear();
    levelColumnCombo->clear();

    definitionColumnCombo->addItem("(none)", -1);
    pronunciationColumnCombo->addItem("(none)", -1);
    levelColumnCombo->addItem("(none)", -1);

    for (int i = 0; i < colCount; ++i) {
        QString label = QString("%1: %2").arg(i + 1).arg(
            csvData[0][i].left(Constants::Term::PREVIEW_TEXT_LENGTH) + (csvData[0][i].length() > 30 ? "..." : "")
            );

        termColumnCombo->addItem(label, i);
        definitionColumnCombo->addItem(label, i);
        pronunciationColumnCombo->addItem(label, i);
        levelColumnCombo->addItem(label, i);
    }

    // Set defaults (guess based on column count)
    if (colCount >= 1) termColumnCombo->setCurrentIndex(0);
    if (colCount >= 2) definitionColumnCombo->setCurrentIndex(2); // index 2 because (none) is index 1
    if (colCount >= 3) pronunciationColumnCombo->setCurrentIndex(3);
    if (colCount >= 4) levelColumnCombo->setCurrentIndex(4);

    // Unblock signals
    termColumnCombo->blockSignals(false);
    definitionColumnCombo->blockSignals(false);
    pronunciationColumnCombo->blockSignals(false);
    levelColumnCombo->blockSignals(false);
}

void ImportVocabDialog::onSeparatorChanged()
{
    if (!selectedFilePath.isEmpty()) {
        loadAndPreviewCSV();
    }
}

void ImportVocabDialog::onFieldMappingChanged()
{
    updateImportButtonState();
}

void ImportVocabDialog::updateImportButtonState()
{
    bool hasFile = !selectedFilePath.isEmpty();
    bool hasTermColumn = termColumnCombo->currentData().toInt() >= 0;

    importButton->setEnabled(hasFile && hasTermColumn && !csvData.isEmpty());
}

QString ImportVocabDialog::getSelectedLanguage() const
{
    return languageCombo->currentData().toString();
}

QList<ImportVocabDialog::ImportData> ImportVocabDialog::getImportData() const
{
    QList<ImportData> result;

    int termCol = termColumnCombo->currentData().toInt();
    int defCol = definitionColumnCombo->currentData().toInt();
    int pronCol = pronunciationColumnCombo->currentData().toInt();
    int levelCol = levelColumnCombo->currentData().toInt();

    for (int i = 0; i < csvData.size(); ++i) {
        const QStringList& row = csvData[i];

        if (termCol >= row.size()) continue;

        ImportData data;
        data.term = row[termCol];
        data.definition = (defCol >= 0 && defCol < row.size()) ? row[defCol] : "";
        data.pronunciation = (pronCol >= 0 && pronCol < row.size()) ? row[pronCol] : "";

        // Use mapped level
        if (levelCol >= 0 && levelCol < row.size()) {
            QString csvLevel = row[levelCol].trimmed();
            TermLevel mappedLevel = getLevelMappingFor(csvLevel);
            data.level = QString::number(static_cast<int>(mappedLevel));
        } else {
            data.level = QString::number(static_cast<int>(TermLevel::Recognized));
        }

        if (!data.term.isEmpty()) {
            result.append(data);
        }
    }

    return result;
}

void ImportVocabDialog::onImportClicked()
{
    accept();
}

QComboBox* ImportVocabDialog::createLevelMappingCombo()
{
    QComboBox* combo = new QComboBox();
    combo->addItem("Recognized", static_cast<int>(TermLevel::Recognized));
    combo->addItem("Learning", static_cast<int>(TermLevel::Learning));
    combo->addItem("Known", static_cast<int>(TermLevel::Known));
    combo->addItem("Well Known", static_cast<int>(TermLevel::WellKnown));
    combo->addItem("Ignored", static_cast<int>(TermLevel::Ignored));
    return combo;
}

void ImportVocabDialog::onLevelColumnChanged()
{
    updateLevelMapping();
    onFieldMappingChanged();
}

void ImportVocabDialog::updateLevelMapping()
{
    // Clear existing mappings
    levelMappingCombos.clear();
    delete levelMappingWidget->layout();

    int levelCol = levelColumnCombo->currentData().toInt();

    if (levelCol < 0 || csvData.isEmpty()) {
        levelMappingGroup->setVisible(false);
        return;
    }

    // Find all unique level values in the CSV
    QSet<QString> uniqueLevels;
    for (int i = 0; i < csvData.size(); ++i) {
        if (levelCol < csvData[i].size()) {
            QString level = csvData[i][levelCol].trimmed();
            if (!level.isEmpty()) {
                uniqueLevels.insert(level);
            }
        }
    }

    if (uniqueLevels.isEmpty()) {
        levelMappingGroup->setVisible(false);
        return;
    }

    // Create mapping UI
    QFormLayout* layout = new QFormLayout(levelMappingWidget);
    layout->setContentsMargins(5, 5, 5, 5);

    // Sort levels (try to sort numerically if possible)
    QList<QString> sortedLevels = uniqueLevels.values();
    std::sort(sortedLevels.begin(), sortedLevels.end(), [](const QString& a, const QString& b) {
        bool aIsNum, bIsNum;
        int aNum = a.toInt(&aIsNum);
        int bNum = b.toInt(&bIsNum);

        if (aIsNum && bIsNum) {
            return aNum < bNum;
        }
        return a < b;
    });

    // Create a combo for each unique level
    for (const QString& level : sortedLevels) {
        QComboBox* combo = createLevelMappingCombo();

        // Try to guess a good default mapping
        bool isNum;
        int numValue = level.toInt(&isNum);

        if (isNum) {
            if (numValue == 0 || numValue == 1) {
                combo->setCurrentIndex(0); // New
            } else if (numValue == 2 || numValue == 3) {
                combo->setCurrentIndex(1); // Learning
            } else if (numValue == 4 || numValue == 5) {
                combo->setCurrentIndex(2); // Known
            } else if (numValue >= 98) {
                combo->setCurrentIndex(4); // Ignored
            } else {
                combo->setCurrentIndex(2); // Known (default for other numbers)
            }
        } else {
            // String-based guessing
            QString lower = level.toLower();
            if (lower.contains("new") || lower.contains("unknown")) {
                combo->setCurrentIndex(0);
            } else if (lower.contains("learn")) {
                combo->setCurrentIndex(1);
            } else if (lower.contains("known")) {
                combo->setCurrentIndex(2);
            } else if (lower.contains("well") || lower.contains("master")) {
                combo->setCurrentIndex(3);
            } else if (lower.contains("ignore")) {
                combo->setCurrentIndex(4);
            }
        }

        levelMappingCombos[level] = combo;

        QString labelText = QString("\"%1\" maps to:").arg(level);
        layout->addRow(labelText, combo);
    }

    levelMappingGroup->setVisible(true);
}

TermLevel ImportVocabDialog::getLevelMappingFor(const QString& csvLevel) const
{
    if (levelMappingCombos.contains(csvLevel)) {
        QComboBox* combo = levelMappingCombos[csvLevel];
        return static_cast<TermLevel>(combo->currentData().toInt());
    }

    // Default to New if no mapping found
    return TermLevel::Recognized;
}

void ImportVocabDialog::closeEvent(QCloseEvent* event)
{
    // Warn if user has work in progress (selected a file but hasn't imported)
    if (!selectedFilePath.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Discard Import?",
            "You have configured an import but haven't completed it yet.\n\n"
            "Are you sure you want to close and discard your work?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }

    event->accept();
}

void ImportVocabDialog::reject()
{
    // Warn if user has work in progress (selected a file but hasn't imported)
    if (!selectedFilePath.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            "Discard Import?",
            "You have configured an import but haven't completed it yet.\n\n"
            "Are you sure you want to cancel and discard your work?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (reply == QMessageBox::No) {
            return;
        }
    }

    QDialog::reject();
}
