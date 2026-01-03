#include "kotodama/languageeditdialog.h"
#include "kotodama/constants.h"
#include "kotodama/languagemanager.h"
#include "kotodama/constants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QTextEdit>
#include <QRegularExpression>

LanguageEditDialog::LanguageEditDialog(QWidget* parent,
                                       const LanguageConfig& config,
                                       bool isNew)
    : QDialog(parent)
    , originalConfig(config)
    , isNewLanguage(isNew)
{
    setupUI();

    // Populate fields if editing existing language
    if (!isNew) {
        nameEdit->setText(config.name());
        codeEdit->setText(config.code());
        codeEdit->setEnabled(false); // Can't change code when editing
        regexEdit->setText(config.wordRegex());
        charBasedCheckBox->setChecked(config.isCharBased());
        tokenLimitSpinBox->setValue(config.tokenLimit());
    }
}

void LanguageEditDialog::setupUI()
{
    setWindowTitle(isNewLanguage ? "Add Custom Language" : "Edit Custom Language");
    setMinimumWidth(Constants::Dialog::LANGUAGE_EDIT_MIN_WIDTH);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Form layout for input fields
    QFormLayout* formLayout = new QFormLayout();

    nameEdit = new QLineEdit();
    nameEdit->setPlaceholderText("e.g., Spanish, German, Korean");
    formLayout->addRow("Language Name:", nameEdit);

    codeEdit = new QLineEdit();
    codeEdit->setPlaceholderText("e.g., es, de, ko (2-3 lowercase letters)");
    codeEdit->setMaxLength(3);
    formLayout->addRow("Language Code:", codeEdit);

    regexEdit = new QLineEdit();
    regexEdit->setPlaceholderText("e.g., [-'a-zA-ZáéíóúñÑ]+");
    formLayout->addRow("Regex Pattern:", regexEdit);

    testRegexButton = new QPushButton("Test Regex");
    connect(testRegexButton, &QPushButton::clicked, this, &LanguageEditDialog::onTestRegex);
    formLayout->addRow("", testRegexButton);

    charBasedCheckBox = new QCheckBox("Character-based (e.g., Japanese or Chinese)");
    formLayout->addRow("", charBasedCheckBox);

    tokenLimitSpinBox = new QSpinBox();
    tokenLimitSpinBox->setMinimum(1);
    tokenLimitSpinBox->setMaximum(Constants::Term::DEFAULT_TOKEN_LIMIT);
    tokenLimitSpinBox->setValue(6);
    tokenLimitSpinBox->setMinimumWidth(Constants::Control::SPIN_BOX_MIN_WIDTH);
    formLayout->addWidget(tokenLimitSpinBox);

    mainLayout->addLayout(formLayout);

    // Error label
    errorLabel = new QLabel();
    errorLabel->setStyleSheet("QLabel { color: red; }");
    errorLabel->setWordWrap(true);
    errorLabel->hide();
    mainLayout->addWidget(errorLabel);

    // Button box
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &LanguageEditDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

bool LanguageEditDialog::validate()
{
    errorLabel->hide();

    // Validate name
    if (nameEdit->text().trimmed().isEmpty()) {
        errorLabel->setText("Language name cannot be empty.");
        errorLabel->show();
        nameEdit->setFocus();
        return false;
    }

    // Validate code
    QString code = codeEdit->text().trimmed().toLower();
    if (code.isEmpty()) {
        errorLabel->setText("Language code cannot be empty.");
        errorLabel->show();
        codeEdit->setFocus();
        return false;
    }

    // Validate code format (2-3 lowercase letters)
    QRegularExpression codeRegex("^[a-z]{2,3}$");
    if (!codeRegex.match(code).hasMatch()) {
        errorLabel->setText("Language code must be 2-3 lowercase letters (e.g., 'es', 'de', 'ko').");
        errorLabel->show();
        codeEdit->setFocus();
        return false;
    }

    // Check uniqueness (unless editing the same code)
    QString oldCode = isNewLanguage ? "" : originalConfig.code();
    if (!LanguageManager::instance().validateLanguageCode(code, oldCode)) {
        errorLabel->setText("Language code '" + code + "' already exists or is reserved.");
        errorLabel->show();
        codeEdit->setFocus();
        return false;
    }

    // Validate regex
    QString regex = regexEdit->text().trimmed();
    if (regex.isEmpty()) {
        errorLabel->setText("Regex pattern cannot be empty.");
        errorLabel->show();
        regexEdit->setFocus();
        return false;
    }

    if (!LanguageManager::instance().validateRegexPattern(regex)) {
        errorLabel->setText("Invalid regex pattern. Please check syntax.");
        errorLabel->show();
        regexEdit->setFocus();
        return false;
    }

    return true;
}

void LanguageEditDialog::onAccept()
{
    if (validate()) {
        accept();
    }
}

void LanguageEditDialog::onTestRegex()
{
    QString regex = regexEdit->text().trimmed();

    if (regex.isEmpty()) {
        QMessageBox::warning(this, "Test Regex", "Please enter a regex pattern first.");
        return;
    }

    // Test if regex compiles
    QRegularExpression testRegex(regex);
    if (!testRegex.isValid()) {
        QMessageBox::warning(this, "Test Regex",
                             "Invalid regex pattern:\n" + testRegex.errorString());
        return;
    }

    // Create a simple test dialog
    QDialog testDialog(this);
    testDialog.setWindowTitle("Test Regex Pattern");
    testDialog.setMinimumWidth(Constants::Dialog::TEST_DIALOG_MIN_WIDTH);

    QVBoxLayout* layout = new QVBoxLayout(&testDialog);

    layout->addWidget(new QLabel("Enter sample text to test the regex pattern:"));

    QTextEdit* sampleText = new QTextEdit();
    sampleText->setPlaceholderText("Enter text here...");
    sampleText->setMaximumHeight(Constants::Panel::SAMPLE_TEXT_MAX_HEIGHT);
    layout->addWidget(sampleText);

    QPushButton* testButton = new QPushButton("Test");
    QLabel* resultLabel = new QLabel();
    resultLabel->setWordWrap(true);

    connect(testButton, &QPushButton::clicked, [&]() {
        QString text = sampleText->toPlainText();
        if (text.isEmpty()) {
            resultLabel->setText("Please enter some text to test.");
            return;
        }

        QRegularExpressionMatchIterator it = testRegex.globalMatch(text);
        QStringList matches;
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            matches.append(match.captured());
        }

        if (matches.isEmpty()) {
            resultLabel->setText("No matches found.");
        } else {
            resultLabel->setText("Matches found: " + QString::number(matches.size()) +
                                 "\n\n" + matches.join(", "));
        }
    });

    layout->addWidget(testButton);
    layout->addWidget(resultLabel);

    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, &testDialog, &QDialog::accept);
    layout->addWidget(buttonBox);

    testDialog.exec();
}

LanguageConfig LanguageEditDialog::getLanguageConfig() const
{
    return LanguageConfig(
        nameEdit->text().trimmed(),
        codeEdit->text().trimmed().toLower(),
        regexEdit->text().trimmed(),
        charBasedCheckBox->isChecked(),
        tokenLimitSpinBox->value()
    );
}
