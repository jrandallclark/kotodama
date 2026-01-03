#ifndef TERM_H
#define TERM_H

#include <QString>

enum class TermLevel {
    Recognized = 0,
    Learning = 1,
    Known = 2,
    WellKnown = 3,
    Ignored = 4
};

struct Term {
    int id = 0;
    QString term;
    QString language;
    TermLevel level = TermLevel::Recognized;
    QString definition;
    QString pronunciation;
    QString addedDate;
    QString lastModified;
    int tokenCount = 0;

    // Helper to convert enum to string for display
    static QString levelToString(TermLevel level) {
        switch (level) {
        case TermLevel::Recognized: return "Recognized";
        case TermLevel::Learning: return "Learning";
        case TermLevel::Known: return "Known";
        case TermLevel::WellKnown: return "Well Known";
        case TermLevel::Ignored: return "Ignored";
        }
        return "Unknown";
    }
};

#endif // TERM_H
