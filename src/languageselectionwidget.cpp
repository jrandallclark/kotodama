#include "kotodama/languageselectionwidget.h"
#include "kotodama/languagemanager.h"
#include "kotodama/thememanager.h"
#include "kotodama/constants.h"

LanguageSelectionWidget::LanguageSelectionWidget(QWidget *parent)
    : QWidget(parent)
    , mainLayout(nullptr)
    , scrollArea(nullptr)
    , scrollContent(nullptr)
    , scrollLayout(nullptr)
    , instructionLabel(nullptr)
    , countLabel(nullptr)
{
    setupUI();
    populateLanguages();
    updateCountLabel();
}

LanguageSelectionWidget::~LanguageSelectionWidget()
{
}

void LanguageSelectionWidget::setupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(Constants::Margins::MEDIUM);

    // Instruction label
    instructionLabel = new QLabel("Select the languages you want to learn:");
    instructionLabel->setWordWrap(true);
    QFont font = instructionLabel->font();
    font.setPointSize(Constants::Font::SIZE_MEDIUM);
    instructionLabel->setFont(font);
    mainLayout->addWidget(instructionLabel);

    // Scroll area for language checkboxes
    scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    scrollContent = new QWidget();
    scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(Constants::Margins::SMALL, Constants::Margins::SMALL,
                                     Constants::Margins::SMALL, Constants::Margins::SMALL);
    scrollLayout->setSpacing(Constants::Margins::SMALL);

    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea, 1);

    // Count label
    countLabel = new QLabel();
    countLabel->setAlignment(Qt::AlignCenter);
    QFont countFont = countLabel->font();
    countFont.setPointSize(Constants::Font::SIZE_SMALL);
    countLabel->setFont(countFont);
    countLabel->setStyleSheet(QString("color: %1;")
        .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary)));
    mainLayout->addWidget(countLabel);
}

void LanguageSelectionWidget::populateLanguages()
{
    QList<LanguageConfig> allLanguages = LanguageManager::instance().getAllLanguages();

    for (const LanguageConfig& lang : allLanguages)
    {
        QCheckBox* checkbox = new QCheckBox(lang.name());
        checkbox->setProperty("code", lang.code());

        QFont checkboxFont = checkbox->font();
        checkboxFont.setPointSize(Constants::Font::SIZE_MEDIUM);
        checkbox->setFont(checkboxFont);

        connect(checkbox, &QCheckBox::checkStateChanged, this, &LanguageSelectionWidget::onCheckboxChanged);

        languageCheckboxes[lang.code()] = checkbox;
        scrollLayout->addWidget(checkbox);
    }

    // Add stretch at the end
    scrollLayout->addStretch();
}

QStringList LanguageSelectionWidget::getSelectedLanguages() const
{
    QStringList selected;

    for (auto it = languageCheckboxes.constBegin(); it != languageCheckboxes.constEnd(); ++it)
    {
        if (it.value()->isChecked())
        {
            selected.append(it.key());
        }
    }

    return selected;
}

void LanguageSelectionWidget::setSelectedLanguages(const QStringList& codes)
{
    for (auto it = languageCheckboxes.begin(); it != languageCheckboxes.end(); ++it)
    {
        it.value()->setChecked(codes.contains(it.key()));
    }
}

bool LanguageSelectionWidget::hasSelection() const
{
    for (auto it = languageCheckboxes.constBegin(); it != languageCheckboxes.constEnd(); ++it)
    {
        if (it.value()->isChecked())
        {
            return true;
        }
    }

    return false;
}

int LanguageSelectionWidget::getSelectionCount() const
{
    int count = 0;

    for (auto it = languageCheckboxes.constBegin(); it != languageCheckboxes.constEnd(); ++it)
    {
        if (it.value()->isChecked())
        {
            count++;
        }
    }

    return count;
}

void LanguageSelectionWidget::onCheckboxChanged()
{
    updateCountLabel();
    emit selectionChanged();
}

void LanguageSelectionWidget::updateCountLabel()
{
    int count = getSelectionCount();

    if (count == 0)
    {
        countLabel->setText("No languages selected");
    }
    else if (count == 1)
    {
        countLabel->setText("1 language selected");
    }
    else
    {
        countLabel->setText(QString("%1 languages selected").arg(count));
    }
}
