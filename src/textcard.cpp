#include "kotodama/textcard.h"
#include "kotodama/thememanager.h"
#include "kotodama/constants.h"

#include <QMouseEvent>
#include <QDateTime>

TextCard::TextCard(QWidget *parent)
    : QFrame(parent)
    , mousePressed(false)
{
    setupUI();
    setupStyling();
}

TextCard::~TextCard()
{
}

void TextCard::setupUI()
{
    // Set object name for styling
    setObjectName("textCard");

    // Main card layout
    QVBoxLayout* cardLayout = new QVBoxLayout(this);
    cardLayout->setContentsMargins(Constants::Margins::LARGE, Constants::Margins::LARGE,
                                    Constants::Margins::LARGE, Constants::Margins::LARGE);
    cardLayout->setSpacing(Constants::Spacing::MEDIUM);

    // Title row with delete button
    QHBoxLayout* titleLayout = new QHBoxLayout();
    titleLayout->setSpacing(8);

    titleLabel = new QLabel(this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(Constants::Font::SIZE_LARGE);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet(QString("color: %1; background-color: transparent;")
                                  .arg(ThemeManager::instance().getColorHex(ThemeColor::TextPrimary)));
    titleLabel->setWordWrap(true);
    titleLayout->addWidget(titleLabel, 1);

    deleteButton = new QPushButton("×", this);
    deleteButton->setFixedSize(Constants::Control::DELETE_BUTTON_SIZE, Constants::Control::DELETE_BUTTON_SIZE);
    deleteButton->setCursor(Qt::PointingHandCursor);
    deleteButton->setStyleSheet(QString(R"(
        QPushButton {
            background-color: transparent;
            color: %1;
            border: none;
            border-radius: 14px;
            font-size: 24px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: %2;
            color: #F44336;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary))
       .arg(ThemeManager::instance().isDarkMode() ? "#3A1F1F" : "#FFEBEE"));
    titleLayout->addWidget(deleteButton);

    cardLayout->addLayout(titleLayout);

    // Metadata row
    QHBoxLayout* metadataLayout = new QHBoxLayout();
    metadataLayout->setSpacing(Constants::Spacing::EXTRA_LARGE);

    languageLabel = new QLabel(this);
    QFont metadataFont = languageLabel->font();
    metadataFont.setPointSize(Constants::Font::SIZE_SMALL);
    languageLabel->setFont(metadataFont);
    QString metadataStyle = QString("color: %1; background-color: transparent;")
                                .arg(ThemeManager::instance().getColorHex(ThemeColor::TextSecondary));
    languageLabel->setStyleSheet(metadataStyle);
    metadataLayout->addWidget(languageLabel);

    dateLabel = new QLabel(this);
    dateLabel->setFont(metadataFont);
    dateLabel->setStyleSheet(metadataStyle);
    metadataLayout->addWidget(dateLabel);

    wordCountLabel = new QLabel(this);
    wordCountLabel->setFont(metadataFont);
    wordCountLabel->setStyleSheet(metadataStyle);
    metadataLayout->addWidget(wordCountLabel);

    metadataLayout->addStretch();
    cardLayout->addLayout(metadataLayout);

    // Divider
    divider = new QWidget(this);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QString("background-color: %1;")
                               .arg(ThemeManager::instance().getColorHex(ThemeColor::Divider)));
    cardLayout->addWidget(divider);

    // Progress row
    QHBoxLayout* progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(Constants::Spacing::MEDIUM);

    progressBar = new QProgressBar(this);
    progressBar->setMinimumHeight(24);
    progressBar->setTextVisible(true);
    progressBar->setFormat("%p% new");
    progressLayout->addWidget(progressBar, 1);

    newTermsLabel = new QLabel(this);
    QFont badgeFont = newTermsLabel->font();
    badgeFont.setPointSize(Constants::Font::SIZE_DEFAULT);
    badgeFont.setBold(true);
    newTermsLabel->setFont(badgeFont);
    newTermsLabel->setStyleSheet(
        "color: #FFFFFF; "
        "background-color: #FF5252; "
        "border-radius: 12px; "
        "padding: 4px 12px;"
    );
    newTermsLabel->setAlignment(Qt::AlignCenter);
    progressLayout->addWidget(newTermsLabel);

    cardLayout->addLayout(progressLayout);

    // Set cursor
    setCursor(Qt::PointingHandCursor);

    // Connect delete button
    connect(deleteButton, &QPushButton::clicked, this, &TextCard::onDeleteClicked);
}

void TextCard::setupStyling()
{
    // Card styles - use theme colors
    cardStyleNormal = QString(R"(
        QFrame#textCard {
            background-color: %1;
            border-radius: 8px;
            border: 1px solid %2;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::CardBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Border));

    cardStyleHover = QString(R"(
        QFrame#textCard {
            background-color: %1;
            border-radius: 8px;
            border: 2px solid %2;
        }
    )").arg(ThemeManager::instance().getColorHex(ThemeColor::CardBackground))
       .arg(ThemeManager::instance().getColorHex(ThemeColor::Primary));

    setStyleSheet(cardStyleNormal);

    // Shadow effect
    shadowEffect = new QGraphicsDropShadowEffect(this);
    shadowEffect->setBlurRadius(8);
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(2);
    shadowEffect->setColor(QColor(0, 0, 0, 25));  // 10% opacity
    setGraphicsEffect(shadowEffect);
}

void TextCard::setTextInfo(const TextDisplayItem& item, const TextProgressStats& stats)
{
    uuid = item.uuid;

    // Set title
    titleLabel->setText(item.title);

    // Set metadata
    languageLabel->setText(item.languageName);

    // Parse and format date
    QDateTime dateTime = QDateTime::fromString(item.addedDate, Qt::ISODate);
    if (dateTime.isValid()) {
        dateLabel->setText(dateTime.toString("MMM d, yyyy"));
    } else {
        dateLabel->setText(item.addedDate);  // Fallback to raw string
    }

    wordCountLabel->setText(QString::number(stats.totalUniqueWords) + " words");

    // Set progress bar
    int percentKnown = static_cast<int>(stats.percentKnown);
    progressBar->setValue(100 - percentKnown);

    // Color code progress bar based on percentage
    QString progressBarStyle;
    QString borderColor = ThemeManager::instance().getColorHex(ThemeColor::Border);
    bool isDark = ThemeManager::instance().isDarkMode();

    if (percentKnown < 25) {
        // Red - mostly unknown
        QString bgColor = isDark ? "#3A1F1F" : "#FFE0E0";
        progressBarStyle = QString(R"(
            QProgressBar {
                border: 1px solid %1;
                border-radius: 4px;
                text-align: center;
                background-color: %2;
            }
            QProgressBar::chunk {
                background-color: #FF5252;
                border-radius: 3px;
            }
        )").arg(borderColor).arg(bgColor);
    } else if (percentKnown < 50) {
        // Orange - learning
        QString bgColor = isDark ? "#3A2F1F" : "#FFF3E0";
        progressBarStyle = QString(R"(
            QProgressBar {
                border: 1px solid %1;
                border-radius: 4px;
                text-align: center;
                background-color: %2;
            }
            QProgressBar::chunk {
                background-color: #FFB74D;
                border-radius: 3px;
            }
        )").arg(borderColor).arg(bgColor);
    } else if (percentKnown < 75) {
        // Green - known
        QString bgColor = isDark ? "#1F3A1F" : "#E8F5E9";
        progressBarStyle = QString(R"(
            QProgressBar {
                border: 1px solid %1;
                border-radius: 4px;
                text-align: center;
                background-color: %2;
            }
            QProgressBar::chunk {
                background-color: #4CAF50;
                border-radius: 3px;
            }
        )").arg(borderColor).arg(bgColor);
    } else {
        // Blue - well known
        QString bgColor = isDark ? "#1F2F3A" : "#E3F2FD";
        progressBarStyle = QString(R"(
            QProgressBar {
                border: 1px solid %1;
                border-radius: 4px;
                text-align: center;
                background-color: %2;
            }
            QProgressBar::chunk {
                background-color: #1976D2;
                border-radius: 3px;
            }
        )").arg(borderColor).arg(bgColor);
    }
    progressBar->setStyleSheet(progressBarStyle);

    // Set new terms badge
    if (stats.newWords > 0) {
        newTermsLabel->setText(QString::number(stats.newWords) + " new");
        newTermsLabel->show();
    } else {
        newTermsLabel->hide();
    }
}

void TextCard::setHoverState(bool hovering)
{
    if (hovering) {
        setStyleSheet(cardStyleHover);
        shadowEffect->setBlurRadius(16);
        shadowEffect->setYOffset(4);
        shadowEffect->setColor(QColor(0, 0, 0, 51));  // 20% opacity
    } else {
        setStyleSheet(cardStyleNormal);
        shadowEffect->setBlurRadius(8);
        shadowEffect->setYOffset(2);
        shadowEffect->setColor(QColor(0, 0, 0, 25));  // 10% opacity
    }
}

void TextCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        mousePressed = true;
    }
    QFrame::mousePressEvent(event);
}

void TextCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && mousePressed) {
        mousePressed = false;
        emit clicked(uuid);
    }
    QFrame::mouseReleaseEvent(event);
}

void TextCard::enterEvent(QEnterEvent *event)
{
    setHoverState(true);
    QFrame::enterEvent(event);
}

void TextCard::leaveEvent(QEvent *event)
{
    setHoverState(false);
    mousePressed = false;
    QFrame::leaveEvent(event);
}

void TextCard::onDeleteClicked()
{
    emit deleteRequested(uuid);
}
