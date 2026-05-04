#include "kotodama/mecabtokenizer.h"
#include "kotodama/languagemodulemanager.h"
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QList>
#include <QString>
#include <cstdlib>
#include <mecab.h>
#include <stdexcept>

namespace {
QString resolvedDicPath()
{
    // Override hook: tests and dev runs can point at a dict outside the
    // user's data dir (e.g. the staged dict in the build dir).
    const QByteArray override = qgetenv("KOTODAMA_MECAB_DICDIR");
    if (!override.isEmpty()) {
        return QDir(QString::fromLocal8Bit(override)).absolutePath();
    }
    // Module is downloaded on demand. The Language Manager UI prevents
    // reaching this code path before the module is installed, so a
    // missing dict here is a programmer error.
    return QDir(LanguageModuleManager::moduleDictPathFor("ja")).absolutePath();
}
}

struct MeCabTokenizer::Impl {
    MeCab::Tagger* tagger = nullptr;
    Impl() {
        QString dictDir = resolvedDicPath();

        // MeCab requires a usable mecabrc; system-wide lookup is fragile
        // (especially on Windows where vcpkg's default rc path may not be
        // installed). Synthesize a transient one pointing at our dict.
        QString rcPath = QDir::temp().absoluteFilePath("kotodama-mecabrc");
        QFile rc(rcPath);
        if (rc.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            rc.write("dicdir = ");
            rc.write(dictDir.toUtf8());
            rc.write("\n");
            rc.close();
        }
        qputenv("MECABRC", rcPath.toUtf8());

        std::string dicPath = dictDir.toStdString();
        const char* argv[] = { "mecab", "-d", dicPath.c_str() };
        tagger = MeCab::createTagger(3, const_cast<char**>(argv));
        if (!tagger) {
            std::string err = MeCab::getLastError() ? MeCab::getLastError() : "(no error message)";
            throw std::runtime_error(
                "MeCab tagger creation failed. Dictionary path: " + dicPath
                + ". rcfile: " + rcPath.toStdString() + ". MeCab error: " + err);
        }
    }
    ~Impl() { delete tagger; }
};

std::vector<TokenResult> MeCabTokenizer::tokenize(const QString& text)
{
    std::vector<TokenResult> tokens;
    if (!d || !d->tagger) return tokens;

    QByteArray utf8 = text.toUtf8();
    const MeCab::Node* node = d->tagger->parseToNode(utf8.constData());

    // Pre-build byte→char offset map for correct position reporting
    // QString uses UTF-16: BMP characters are 1 code unit, but supplementary
    // characters (U+10000+, 4-byte UTF-8) require a surrogate pair = 2 code units.
    QList<int> byteToChar(utf8.size() + 1, -1);
    int charIdx = 0;
    for (int byteIdx = 0; byteIdx < utf8.size(); ) {
        byteToChar[byteIdx] = charIdx;
        unsigned char c = static_cast<unsigned char>(utf8[byteIdx]);
        int seqLen = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        for (int k = 1; k < seqLen && byteIdx + k < utf8.size(); ++k)
            byteToChar[byteIdx + k] = charIdx;
        byteIdx += seqLen;
        // Supplementary characters (4-byte UTF-8 → U+10000+) occupy
        // 2 UTF-16 code units in QString, not 1
        charIdx += (seqLen == 4) ? 2 : 1;
    }
    byteToChar[utf8.size()] = charIdx;

    int byteOffset = 0;
    for (; node; node = node->next) {
        if (node->stat == MECAB_BOS_NODE || node->stat == MECAB_EOS_NODE) continue;

        std::string surface(node->surface, node->length);

        // Skip tokens with no word characters (punctuation, whitespace, symbols).
        // Use Qt's Unicode-aware isLetter()/isDigit() so this works for any script
        // regardless of MeCab dictionary coverage.
        {
            QString surfaceStr = QString::fromUtf8(surface.c_str(), static_cast<qsizetype>(surface.size()));
            bool hasWordChar = false;
            for (const QChar& ch : surfaceStr) {
                if (ch.isLetter() || ch.isDigit()) {
                    hasWordChar = true;
                    break;
                }
            }
            if (!hasWordChar) {
                byteOffset += node->rlength;
                continue;
            }
        }

        int byteStart = byteOffset + (node->rlength - node->length);
        int byteEnd   = byteStart + static_cast<int>(node->length);
        tokens.push_back({
            surface,
            byteToChar[byteStart],
            byteToChar[byteEnd]
        });
        byteOffset += node->rlength;
    }
    return tokens;
}

bool MeCabTokenizer::isAvailable() const { return d && d->tagger != nullptr; }

MeCabTokenizer::MeCabTokenizer() : d(std::make_unique<Impl>()) {}

MeCabTokenizer::~MeCabTokenizer() = default;
