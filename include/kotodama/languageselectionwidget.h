#ifndef LANGUAGESELECTIONWIDGET_H
#define LANGUAGESELECTIONWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QLabel>
#include <QMap>

class LanguageSelectionWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LanguageSelectionWidget(QWidget *parent = nullptr);
    ~LanguageSelectionWidget();

    QStringList getSelectedLanguages() const;
    void setSelectedLanguages(const QStringList& codes);

    bool hasSelection() const;
    int getSelectionCount() const;

signals:
    void selectionChanged();

private slots:
    void onCheckboxChanged();

private:
    void setupUI();
    void populateLanguages();
    void updateCountLabel();

    QVBoxLayout* mainLayout;
    QScrollArea* scrollArea;
    QWidget* scrollContent;
    QVBoxLayout* scrollLayout;
    QMap<QString, QCheckBox*> languageCheckboxes;
    QLabel* instructionLabel;
    QLabel* countLabel;
};

#endif // LANGUAGESELECTIONWIDGET_H
