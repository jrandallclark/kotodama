#include "kotodama/languageselectiondialog.h"
#include "kotodama/databasemanager.h"
#include "kotodama/constants.h"

#include <QMessageBox>

LanguageSelectionDialog::LanguageSelectionDialog(QWidget *parent)
    : QDialog(parent)
    , languageWidget(nullptr)
    , buttonBox(nullptr)
    , okButton(nullptr)
{
    setupUI();
    loadCurrentSelections();
}

LanguageSelectionDialog::~LanguageSelectionDialog()
{
}

void LanguageSelectionDialog::setupUI()
{
    setWindowTitle("Manage Learning Languages");
    setMinimumSize(500, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(Constants::Margins::MEDIUM, Constants::Margins::MEDIUM,
                                   Constants::Margins::MEDIUM, Constants::Margins::MEDIUM);
    mainLayout->setSpacing(Constants::Margins::MEDIUM);

    // Instruction label
    QLabel* instructionLabel = new QLabel(
        "Select the languages you want to learn. Your text library will be filtered to show only these languages."
    );
    instructionLabel->setWordWrap(true);
    QFont font = instructionLabel->font();
    font.setPointSize(Constants::Font::SIZE_SMALL);
    instructionLabel->setFont(font);
    instructionLabel->setStyleSheet("color: gray; margin-bottom: 10px;");
    mainLayout->addWidget(instructionLabel);

    // Language selection widget
    languageWidget = new LanguageSelectionWidget(this);
    connect(languageWidget, &LanguageSelectionWidget::selectionChanged,
            this, &LanguageSelectionDialog::onSelectionChanged);
    mainLayout->addWidget(languageWidget);

    // Dialog buttons
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    okButton = buttonBox->button(QDialogButtonBox::Ok);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &LanguageSelectionDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &LanguageSelectionDialog::onReject);

    mainLayout->addWidget(buttonBox);

    onSelectionChanged();
}

void LanguageSelectionDialog::loadCurrentSelections()
{
    QStringList currentLanguages = DatabaseManager::instance().getUserLanguages();
    languageWidget->setSelectedLanguages(currentLanguages);
    originalSelections = currentLanguages;
}

void LanguageSelectionDialog::onSelectionChanged()
{
    bool hasSelection = languageWidget->hasSelection();
    okButton->setEnabled(hasSelection);

    if (!hasSelection)
    {
        okButton->setToolTip("You must select at least one language");
    }
    else
    {
        okButton->setToolTip("");
    }
}

bool LanguageSelectionDialog::validateSelections()
{
    if (!languageWidget->hasSelection())
    {
        QMessageBox::warning(this, "No Languages Selected",
                           "You must select at least one language to learn.");
        return false;
    }
    return true;
}

void LanguageSelectionDialog::onAccept()
{
    if (!validateSelections())
    {
        return;
    }

    QStringList newSelections = languageWidget->getSelectedLanguages();

    // Clear existing selections
    for (const QString& code : originalSelections)
    {
        DatabaseManager::instance().removeUserLanguage(code);
    }

    // Add new selections
    for (const QString& code : newSelections)
    {
        DatabaseManager::instance().addUserLanguage(code);
    }

    accept();
}

void LanguageSelectionDialog::onReject()
{
    reject();
}
