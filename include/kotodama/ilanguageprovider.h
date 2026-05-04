#ifndef ILANGUAGEPROVIDER_H
#define ILANGUAGEPROVIDER_H

#include "languageconfig.h"
#include <QString>

struct ILanguageProvider {
    virtual ~ILanguageProvider() = default;
    virtual LanguageConfig getLanguageByCode(const QString&) const = 0;
    virtual QString        getWordRegex(const QString&) const = 0;
    virtual bool           isCharacterBased(const QString&) const = 0;
};

#endif // ILANGUAGEPROVIDER_H
