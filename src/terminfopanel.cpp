#include "kotodama/terminfopanel.h"
#include "kotodama/thememanager.h"
#include "kotodama/constants.h"
#include "kotodama/aimanager.h"
#include <QHBoxLayout>
#include <QPixmap>
#include <QPainter>
#include <QRadioButton>
#include <QButtonGroup>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QMessageBox>

TermInfoPanel::TermInfoPanel(QWidget *parent)
    : QWidget(parent), currentLevel(TermLevel::Recognized)
{
    setupUI();

    // Connect AIManager signals
    connect(&AIManager::instance(), &AIManager::definitionGenerated,
            this, &TermInfoPanel::onAIDefinitionGenerated);
    connect(&AIManager::instance(), &AIManager::requestFailed,
            this, &TermInfoPanel::onAIRequestFailed);
    connect(&AIManager::instance(), &AIManager::enabledChanged,
            this, &TermInfoPanel::updateAIButtonVisibility);

    // Set initial AI button visibility
    updateAIButtonVisibility();
}

void TermInfoPanel::setupUI()
{
    setStyleSheet(QString("background-color: %1; border-top: 1px solid %2;")
                      .arg(ThemeManager::instance().getColorHex(ThemeColor::CardBackground))
                      .arg(ThemeManager::instance().getColorHex(ThemeColor::Border)));

    // Set size policy to prevent panel from growing beyond content
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    setFixedHeight(Constants::Panel::TERM_INFO_PREVIEW_HEIGHT);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ========== PREVIEW CONTAINER ==========
    previewContainer = new QWidget(this);
    previewContainer->setMinimumHeight(Constants::Panel::TERM_INFO_PREVIEW_MIN_HEIGHT);
    previewContainer->setMaximumHeight(Constants::Panel::TERM_INFO_PREVIEW_HEIGHT);

    QHBoxLayout* previewMainLayout = new QHBoxLayout(previewContainer);
    previewMainLayout->setContentsMargins(0, 0, 0, 0);
    previewMainLayout->setSpacing(0);

    // Colored left border (4px wide)
    QFrame* colorBar = new QFrame(previewContainer);
    colorBar->setObjectName("colorBar");
    colorBar->setFixedWidth(4);
    colorBar->setStyleSheet(QString("background-color: %1;")
                                .arg(ThemeManager::instance().getColorHex(ThemeColor::Primary)));  // Default primary color
    previewMainLayout->addWidget(colorBar);

    // Content area
    QVBoxLayout* previewContentLayout = new QVBoxLayout();
    previewContentLayout->setContentsMargins(8, 4, 8, 4);
    previewContentLayout->setSpacing(0);

    // Term (bold)
    previewTermLabel = new QLabel(previewContainer);
    QFont termFont = previewTermLabel->font();
    termFont.setBold(true);
    termFont.setPointSize(Constants::Font::SIZE_MEDIUM);
    previewTermLabel->setFont(termFont);
    previewTermLabel->setStyleSheet(QString("color: %1; background-color: transparent;")
                                        .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary)));
    previewTermLabel->setWordWrap(true);
    previewTermLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    previewContentLayout->addWidget(previewTermLabel);

    // Pronunciation (smaller, italic)
    previewPronunciationLabel = new QLabel(previewContainer);
    QFont pronFont = previewPronunciationLabel->font();
    pronFont.setItalic(true);
    pronFont.setPointSize(Constants::Font::SIZE_SMALL);
    previewPronunciationLabel->setFont(pronFont);
    previewPronunciationLabel->setStyleSheet(QString("color: %1; background-color: transparent;")
                                                 .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary)));
    previewPronunciationLabel->setWordWrap(true);
    previewPronunciationLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    previewContentLayout->addWidget(previewPronunciationLabel);

    // Definition (regular, small)
    previewDefinitionLabel = new QLabel(previewContainer);
    QFont defFont = previewDefinitionLabel->font();
    defFont.setPointSize(Constants::Font::SIZE_SMALL);
    previewDefinitionLabel->setFont(defFont);
    previewDefinitionLabel->setStyleSheet(QString("color: %1; background-color: transparent;")
                                              .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary)));
    previewDefinitionLabel->setWordWrap(true);
    previewDefinitionLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    previewContentLayout->addWidget(previewDefinitionLabel);

    previewContentLayout->addStretch();
    previewMainLayout->addLayout(previewContentLayout);

    mainLayout->addWidget(previewContainer);

    // ========== EDIT CONTAINER ==========
    editContainer = new QWidget(this);
    editContainer->setMinimumHeight(Constants::Panel::TERM_INFO_EDIT_MIN_HEIGHT);
    editContainer->setMaximumHeight(Constants::Panel::TERM_INFO_EDIT_HEIGHT);

    QVBoxLayout* editLayout = new QVBoxLayout(editContainer);
    editLayout->setContentsMargins(Constants::Margins::LARGE, Constants::Margins::MEDIUM,
                                    Constants::Margins::LARGE, Constants::Margins::MEDIUM);
    editLayout->setSpacing(8);

    // Term label
    editTermLabel = new QLabel(editContainer);
    QFont editTermFont = editTermLabel->font();
    editTermFont.setBold(true);
    editTermFont.setPointSize(Constants::Font::SIZE_LARGE);
    editTermLabel->setFont(editTermFont);
    editTermLabel->setStyleSheet(QString("color: %1; background-color: transparent;")
                                     .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary)));
    editTermLabel->setWordWrap(true);
    editTermLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    editLayout->addWidget(editTermLabel);

    // Pronunciation field
    QVBoxLayout* pronLayout = new QVBoxLayout();
    pronLayout->setSpacing(4);
    QLabel* pronLabel = new QLabel("Pronunciation", editContainer);
    pronLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background-color: transparent;")
                                 .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary)));
    pronLayout->addWidget(pronLabel);
    pronunciationEdit = new QLineEdit(editContainer);
    pronunciationEdit->setPlaceholderText("Enter pronunciation");
    pronunciationEdit->setStyleSheet(QString(R"(
        QLineEdit {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 4px;
            padding: 8px;
            font-size: 14px;
            color: %3;
        }
        QLineEdit:focus {
            border: 2px solid %4;
            padding: 7px;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::InputBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Border))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Primary)));
    pronLayout->addWidget(pronunciationEdit);
    editLayout->addLayout(pronLayout);

    // Definition field
    QVBoxLayout* defLayout = new QVBoxLayout();
    defLayout->setSpacing(4);
    QLabel* defLabel = new QLabel("Definition", editContainer);
    defLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background-color: transparent;")
                                .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary)));
    defLayout->addWidget(defLabel);
    definitionEdit = new QLineEdit(editContainer);
    definitionEdit->setPlaceholderText("Enter definition");
    definitionEdit->setStyleSheet(QString(R"(
        QLineEdit {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 4px;
            padding: 8px;
            font-size: 14px;
            color: %3;
        }
        QLineEdit:focus {
            border: 2px solid %4;
            padding: 7px;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::InputBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Border))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Primary)));

    // Create horizontal layout for definition field and AI button
    QHBoxLayout* defFieldLayout = new QHBoxLayout();
    defFieldLayout->setSpacing(8);
    defFieldLayout->addWidget(definitionEdit);

    // AI generate button
    aiGenerateButton = new QPushButton("✨", editContainer);
    aiGenerateButton->setToolTip("Generate definition using AI");
    aiGenerateButton->setFixedSize(32, 32);
    aiGenerateButton->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 4px;
            font-size: 16px;
            padding: 4px;
        }
        QPushButton:hover {
            background-color: %3;
        }
        QPushButton:disabled {
            color: %4;
            background-color: %5;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::InputBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Border))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::CardBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::InputBackgroundDisabled)));
    connect(aiGenerateButton, &QPushButton::clicked,
            this, &TermInfoPanel::onAIGenerateClicked);
    defFieldLayout->addWidget(aiGenerateButton);

    defLayout->addLayout(defFieldLayout);
    editLayout->addLayout(defLayout);

    // Level section with radio buttons
    QVBoxLayout* levelSectionLayout = new QVBoxLayout();
    levelSectionLayout->setSpacing(4);

    QLabel* levelLabel = new QLabel("Level", editContainer);
    levelLabel->setStyleSheet(QString("color: %1; font-size: 12px; font-weight: bold; background-color: transparent;")
                                  .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary)));
    levelSectionLayout->addWidget(levelLabel);

    // Radio buttons container
    QWidget* levelWidget = new QWidget(editContainer);
    levelWidget->setStyleSheet(QString(R"(
        QWidget {
            background-color: %1;
            border: 1px solid %2;
            border-radius: 4px;
            padding: 4px;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::InputBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Border)));

    QHBoxLayout* levelLayout = new QHBoxLayout(levelWidget);
    levelLayout->setContentsMargins(4, 4, 4, 4);
    levelLayout->setSpacing(Constants::Spacing::MEDIUM);

    // Get colors from theme manager
    QColor newColor = ThemeManager::instance().getColor(ThemeColor::LevelRecognized);
    QColor learningColor = ThemeManager::instance().getColor(ThemeColor::LevelLearning);
    QColor knownColor = ThemeManager::instance().getColor(ThemeColor::LevelKnown);
    QColor wellKnownColor = ThemeManager::instance().getColor(ThemeColor::LevelWellKnown);
    QColor ignoredColor = ThemeManager::instance().getColor(ThemeColor::LevelIgnored);

    // Create button group
    levelButtonGroup = new QButtonGroup(this);

    // Create radio buttons with icons
    levelNewRadio = new QRadioButton();
    levelNewRadio->setIcon(createColorIcon(newColor));
    levelNewRadio->setText("Recognized");
    levelLearningRadio = new QRadioButton();
    levelLearningRadio->setIcon(createColorIcon(learningColor));
    levelLearningRadio->setText("Learning");
    levelKnownRadio = new QRadioButton();
    levelKnownRadio->setIcon(createColorIcon(knownColor));
    levelKnownRadio->setText("Known");
    levelWellKnownRadio = new QRadioButton();
    levelWellKnownRadio->setIcon(createColorIcon(wellKnownColor));
    levelWellKnownRadio->setText("Well Known");
    levelIgnoredRadio = new QRadioButton();
    levelIgnoredRadio->setIcon(createColorIcon(ignoredColor));
    levelIgnoredRadio->setText("Ignored");

    // Add to button group with IDs matching TermLevel enum
    levelButtonGroup->addButton(levelNewRadio, static_cast<int>(TermLevel::Recognized));
    levelButtonGroup->addButton(levelLearningRadio, static_cast<int>(TermLevel::Learning));
    levelButtonGroup->addButton(levelKnownRadio, static_cast<int>(TermLevel::Known));
    levelButtonGroup->addButton(levelWellKnownRadio, static_cast<int>(TermLevel::WellKnown));
    levelButtonGroup->addButton(levelIgnoredRadio, static_cast<int>(TermLevel::Ignored));

    // Style radio buttons
    QString radioStyle = QString(R"(
        QRadioButton {
            font-size: 14px;
            color: %12
            padding: 6px 8px;
            background-color: transparent;
        }
        QRadioButton::indicator {
            width: 12px;
            height: 12px;
        }
        QRadioButton::indicator:unchecked {
            border: 2px solid %2;
            border-radius: 8px;
            background-color: %3;
        }
        QRadioButton::indicator:checked {
            border: 2px solid %4;
            border-radius: 8px;
            background-color: %4;
        }
        QRadioButton:hover {
            background-color: %5;
            border-radius: 4px;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Border))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::CardBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Primary))
       .arg(ThemeManager::instance().isDarkMode() ? "#2D3F4D" : "#F5F5F5");

    levelNewRadio->setStyleSheet(radioStyle);
    levelLearningRadio->setStyleSheet(radioStyle);
    levelKnownRadio->setStyleSheet(radioStyle);
    levelWellKnownRadio->setStyleSheet(radioStyle);
    levelIgnoredRadio->setStyleSheet(radioStyle);

    // Add to layout
    levelLayout->addWidget(levelNewRadio);
    levelLayout->addWidget(levelLearningRadio);
    levelLayout->addWidget(levelKnownRadio);
    levelLayout->addWidget(levelWellKnownRadio);
    levelLayout->addWidget(levelIgnoredRadio);

    levelSectionLayout->addWidget(levelWidget);
    editLayout->addLayout(levelSectionLayout);

    // Bottom controls
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setContentsMargins(0, 8, 0, 0);
    bottomLayout->setSpacing(Constants::Spacing::MEDIUM);

    // Delete button (left side)
    deleteButton = new QPushButton("Delete", editContainer);
    deleteButton->setMinimumHeight(Constants::Control::MIN_BUTTON_HEIGHT);
    deleteButton->setCursor(Qt::PointingHandCursor);
    QString deleteHoverBg = ThemeManager::instance().isDarkMode() ? "#4D2D2D" : "#FFEBEE";
    QString deletePressedBg = ThemeManager::instance().isDarkMode() ? "#3A1F1F" : "#FFCDD2";
    deleteButton->setStyleSheet(QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: %2;
        }
        QPushButton:pressed {
            background-color: %3;
        }
    )").arg(ThemeManager::instance().isDarkMode() ? "#EF5350" : "#D32F2F")
       .arg(deleteHoverBg)
       .arg(deletePressedBg));
    bottomLayout->addWidget(deleteButton);

    bottomLayout->addStretch();

    cancelButton = new QPushButton("Cancel", editContainer);
    cancelButton->setMinimumHeight(Constants::Control::MIN_BUTTON_HEIGHT);
    cancelButton->setCursor(Qt::PointingHandCursor);
    QString cancelHoverBg = ThemeManager::instance().isDarkMode() ? "#2D3F4D" : "#E3F2FD";
    QString cancelPressedBg = ThemeManager::instance().isDarkMode() ? "#1F2F3A" : "#BBDEFB";
    cancelButton->setStyleSheet(QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: %2;
        }
        QPushButton:pressed {
            background-color: %3;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::Primary))
       .arg(cancelHoverBg)
       .arg(cancelPressedBg));
    bottomLayout->addWidget(cancelButton);

    saveButton = new QPushButton("Save", editContainer);
    saveButton->setMinimumHeight(Constants::Control::MIN_BUTTON_HEIGHT);
    saveButton->setCursor(Qt::PointingHandCursor);
    QString saveTextColor = ThemeManager::instance().isDarkMode() ? "#000000" : "#FFFFFF";
    saveButton->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            color: %2;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: %3;
        }
        QPushButton:pressed {
            background-color: %4;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::Primary))
       .arg(saveTextColor)
       .arg(ThemeManager::instance().getColorHex(ThemeColor::PrimaryHover))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::PrimaryPressed)));
    bottomLayout->addWidget(saveButton);

    editLayout->addLayout(bottomLayout);
    mainLayout->addWidget(editContainer);

    // ========== WARNING CONTAINER ==========
    warningContainer = new QWidget(this);
    warningContainer->setMinimumHeight(Constants::Panel::TERM_INFO_WARNING_MIN_HEIGHT);
    warningContainer->setMaximumHeight(Constants::Panel::TERM_INFO_WARNING_HEIGHT);

    QVBoxLayout* warningLayout = new QVBoxLayout(warningContainer);
    warningLayout->setContentsMargins(Constants::Margins::LARGE, Constants::Margins::LARGE,
                                       Constants::Margins::LARGE, Constants::Margins::LARGE);
    warningLayout->setSpacing(Constants::Spacing::MEDIUM);

    // Term label (bold)
    warningTermLabel = new QLabel(warningContainer);
    QFont warningTermFont = warningTermLabel->font();
    warningTermFont.setBold(true);
    warningTermFont.setPointSize(Constants::Font::SIZE_LARGE);
    warningTermLabel->setFont(warningTermFont);
    warningTermLabel->setStyleSheet(QString("color: %1; background-color: transparent;")
                                         .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary)));
    warningTermLabel->setWordWrap(true);
    warningLayout->addWidget(warningTermLabel);

    // Warning message label
    warningMessageLabel = new QLabel(warningContainer);
    warningMessageLabel->setWordWrap(true);
    warningMessageLabel->setStyleSheet(R"(
        color: #D32F2F;
        background-color: #FFEBEE;
        padding: 12px;
        border-radius: 4px;
        border-left: 3px solid #D32F2F;
        font-size: 14px;
    )");
    warningLayout->addWidget(warningMessageLabel);
    warningLayout->addStretch();

    mainLayout->addWidget(warningContainer);

    // Connect buttons
    connect(saveButton, &QPushButton::clicked, this, &TermInfoPanel::onSaveClicked);
    connect(cancelButton, &QPushButton::clicked, this, &TermInfoPanel::onCancelClicked);
    connect(deleteButton, &QPushButton::clicked, this, &TermInfoPanel::onDeleteClicked);

    // Start in preview mode with default message
    editContainer->setVisible(false);
    warningContainer->setVisible(false);
    previewTermLabel->setText("Hover over a term to see its definition, click to edit");
    previewPronunciationLabel->hide();
    previewDefinitionLabel->hide();
}

void TermInfoPanel::showPreview(const Term& term)
{
    // Switch to preview mode
    editContainer->setVisible(false);
    warningContainer->setVisible(false);
    previewContainer->setVisible(true);

    // Store level for color bar
    currentLevel = term.level;

    // Update color bar
    QFrame* colorBar = previewContainer->findChild<QFrame*>("colorBar");
    if (colorBar) {
        colorBar->setStyleSheet("background-color: " + getColorForLevel(term.level) + ";");
    }

    // Set term data
    previewTermLabel->setText(term.term);

    if (!term.pronunciation.isEmpty()) {
        previewPronunciationLabel->setText(term.pronunciation);
        previewPronunciationLabel->show();
    } else {
        previewPronunciationLabel->hide();
    }

    if (!term.definition.isEmpty()) {
        previewDefinitionLabel->setText(term.definition);
        previewDefinitionLabel->show();
    } else {
        previewDefinitionLabel->hide();
    }

    previewContainer->setMaximumHeight(Constants::Panel::TERM_INFO_PREVIEW_HEIGHT);
    setFixedHeight(Constants::Panel::TERM_INFO_PREVIEW_HEIGHT);
}

void TermInfoPanel::showEdit(const QString& termText, const QString& language,
                              const QString& pronunciation, const QString& definition, TermLevel level)
{
    // Switch to edit mode
    previewContainer->setVisible(false);
    warningContainer->setVisible(false);
    editContainer->setVisible(true);
    setFixedHeight(Constants::Panel::TERM_INFO_EDIT_HEIGHT);  // Set panel to edit mode size

    // Store language for AI requests
    currentLanguage = language;

    // Set term data
    editTermLabel->setText(termText);
    pronunciationEdit->setText(pronunciation);
    definitionEdit->setText(definition);

    // Set level
    QAbstractButton* button = levelButtonGroup->button(static_cast<int>(level));
    if (button) {
        button->setChecked(true);
    }

    // Update radio button labels based on current width
    updateRadioButtonLabels(width());

    // Don't auto-focus any field to allow keyboard shortcuts to work
    editContainer->setFocus();
}

void TermInfoPanel::showWarning(const QString& termText, const QString& message)
{
    // Switch to warning mode
    previewContainer->setVisible(false);
    editContainer->setVisible(false);
    warningContainer->setVisible(true);
    setFixedHeight(Constants::Panel::TERM_INFO_WARNING_HEIGHT);  // Set panel to warning mode size

    // Set warning data
    warningTermLabel->setText(termText);
    warningMessageLabel->setText(message);
}

void TermInfoPanel::reset()
{
    // Reset to default state - show preview with message
    editContainer->setVisible(false);
    warningContainer->setVisible(false);
    previewContainer->setVisible(true);

    previewContainer->setMaximumHeight(Constants::Panel::TERM_INFO_PREVIEW_HEIGHT);
    setFixedHeight(Constants::Panel::TERM_INFO_PREVIEW_HEIGHT);

    previewPronunciationLabel->hide();
    previewDefinitionLabel->hide();

    // Reset color bar to default
    QFrame* colorBar = previewContainer->findChild<QFrame*>("colorBar");
    if (colorBar) {
        colorBar->setStyleSheet(QString("background-color: %1;")
                                    .arg(ThemeManager::instance().getColorHex(ThemeColor::Primary)));
    }
}

void TermInfoPanel::onSaveClicked()
{
    QString pronunciation = pronunciationEdit->text().trimmed();
    QString definition = definitionEdit->text().trimmed();
    TermLevel level = static_cast<TermLevel>(levelButtonGroup->checkedId());

    emit termSaved(pronunciation, definition, level);
}

void TermInfoPanel::onCancelClicked()
{
    emit editCancelled();
}

void TermInfoPanel::onDeleteClicked()
{
    emit termDeleted();
}

void TermInfoPanel::keyPressEvent(QKeyEvent* event)
{
    // Only handle shortcuts when in edit mode
    if (!editContainer->isVisible()) {
        QWidget::keyPressEvent(event);
        return;
    }

    // Check if a text field has focus - if so, don't intercept
    if (pronunciationEdit->hasFocus() || definitionEdit->hasFocus()) {
        QWidget::keyPressEvent(event);
        return;
    }

    // Handle number key shortcuts for quick level selection and save
    switch (event->key()) {
    case Qt::Key_1:
        levelNewRadio->setChecked(true);
        onSaveClicked();
        event->accept();
        return;
    case Qt::Key_2:
        levelLearningRadio->setChecked(true);
        onSaveClicked();
        event->accept();
        return;
    case Qt::Key_3:
        levelKnownRadio->setChecked(true);
        onSaveClicked();
        event->accept();
        return;
    case Qt::Key_4:
        levelWellKnownRadio->setChecked(true);
        onSaveClicked();
        event->accept();
        return;
    case Qt::Key_5:
        levelIgnoredRadio->setChecked(true);
        onSaveClicked();
        event->accept();
        return;
    default:
        QWidget::keyPressEvent(event);
        break;
    }
}

QString TermInfoPanel::getColorForLevel(TermLevel level)
{
    QColor color;

    switch (level) {
    case TermLevel::Recognized:
        color = ThemeManager::instance().getColor(ThemeColor::LevelRecognized);
        break;
    case TermLevel::Learning:
        color = ThemeManager::instance().getColor(ThemeColor::LevelLearning);
        break;
    case TermLevel::Known:
        color = ThemeManager::instance().getColor(ThemeColor::LevelKnown);
        break;
    case TermLevel::WellKnown:
        color = ThemeManager::instance().getColor(ThemeColor::LevelWellKnown);
        break;
    case TermLevel::Ignored:
        color = ThemeManager::instance().getColor(ThemeColor::LevelIgnored);
        break;
    default:
        color = ThemeManager::instance().getColor(ThemeColor::Primary);
        break;
    }

    return color.name();  // Convert QColor to hex string
}

QIcon TermInfoPanel::createColorIcon(const QColor& color)
{
    // Create a colored square icon
    QPixmap pixmap(Constants::Icon::SIZE, Constants::Icon::SIZE);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw filled square with border
    painter.setBrush(QBrush(color));
    painter.setPen(QPen(ThemeManager::instance().getColor(ThemeColor::Border), 1));
    painter.drawRoundedRect(Constants::Icon::BORDER_OFFSET, Constants::Icon::BORDER_OFFSET,
                           Constants::Icon::INNER_SIZE, Constants::Icon::INNER_SIZE,
                           Constants::Icon::BORDER_RADIUS, Constants::Icon::BORDER_RADIUS);

    return QIcon(pixmap);
}

void TermInfoPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateRadioButtonLabels(event->size().width());
}

void TermInfoPanel::updateRadioButtonLabels(int width)
{
    // Only update labels if in edit mode (radio buttons are visible)
    if (!editContainer->isVisible()) {
        return;
    }

    // Define width threshold below which labels should be hidden
    // Chosen to accommodate minimum width while keeping UI usable
    if (width < Constants::Control::MIN_WIDTH_FOR_LABELS) {
        // Hide labels, show only colored icons
        levelNewRadio->setText("");
        levelLearningRadio->setText("");
        levelKnownRadio->setText("");
        levelWellKnownRadio->setText("");
        levelIgnoredRadio->setText("");
    } else {
        // Show labels with icons
        levelNewRadio->setText("Recognized");
        levelLearningRadio->setText("Learning");
        levelKnownRadio->setText("Known");
        levelWellKnownRadio->setText("Well Known");
        levelIgnoredRadio->setText("Ignored");
    }
}

void TermInfoPanel::onAIGenerateClicked()
{
    // Disable button and show loading state
    aiGenerateButton->setEnabled(false);
    aiGenerateButton->setText("...");

    // Request AI definition
    AIManager::instance().generateDefinition(
        editTermLabel->text(),
        currentLanguage
    );
}

void TermInfoPanel::onAIDefinitionGenerated(QString definition)
{
    // Fill definition field with AI response
    definitionEdit->setText(definition);

    // Reset button state
    aiGenerateButton->setEnabled(true);
    aiGenerateButton->setText("✨");

    // Focus definition field for review/editing
    definitionEdit->setFocus();
}

void TermInfoPanel::onAIRequestFailed(QString errorMessage)
{
    // Show error message
    QMessageBox::warning(this, "AI Generation Failed", errorMessage);

    // Reset button state
    aiGenerateButton->setEnabled(true);
    aiGenerateButton->setText("✨");
}

void TermInfoPanel::updateAIButtonVisibility()
{
    // Show AI button only if AI is enabled
    aiGenerateButton->setVisible(AIManager::instance().isEnabled());
}
