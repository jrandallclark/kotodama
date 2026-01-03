#ifndef TEXTCARD_H
#define TEXTCARD_H

#include "mainwindowmodel.h"

#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGraphicsDropShadowEffect>

class TextCard : public QFrame
{
    Q_OBJECT

public:
    explicit TextCard(QWidget *parent = nullptr);
    ~TextCard();

    void setTextInfo(const TextDisplayItem& item, const TextProgressStats& stats);

signals:
    void clicked(QString uuid);
    void deleteRequested(QString uuid);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void onDeleteClicked();

private:
    void setupUI();
    void setupStyling();
    void setHoverState(bool hovering);

    // Data
    QString uuid;
    bool mousePressed;

    // UI Elements
    QLabel* titleLabel;
    QPushButton* deleteButton;
    QLabel* languageLabel;
    QLabel* dateLabel;
    QLabel* wordCountLabel;
    QWidget* divider;
    QProgressBar* progressBar;
    QLabel* newTermsLabel;

    // Effects
    QGraphicsDropShadowEffect* shadowEffect;

    // Styling
    QString cardStyleNormal;
    QString cardStyleHover;
};

#endif // TEXTCARD_H
