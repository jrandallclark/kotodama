#include "kotodama/mecabtokenizer.h"
#include <QList>
#include <QString>

#ifdef HAVE_MECAB
#include <mecab.h>

struct MeCabTokenizer::Impl {
    MeCab::Tagger* tagger = nullptr;
    Impl() { tagger = MeCab::createTagger(""); }
    ~Impl() { delete tagger; }
};

std::vector<TokenResult> MeCabTokenizer::tokenize(const QString& text)
{
    std::vector<TokenResult> tokens;
    if (!d || !d->tagger) return tokens;

    QByteArray utf8 = text.toUtf8();
    const MeCab::Node* node = d->tagger->parseToNode(utf8.constData());

    // Pre-build byte→char offset map for correct position reporting
    QList<int> byteToChar(utf8.size() + 1, -1);
    int charIdx = 0;
    for (int byteIdx = 0; byteIdx < utf8.size(); ) {
        byteToChar[byteIdx] = charIdx;
        unsigned char c = static_cast<unsigned char>(utf8[byteIdx]);
        int seqLen = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        for (int k = 1; k < seqLen && byteIdx + k < utf8.size(); ++k)
            byteToChar[byteIdx + k] = charIdx;
        byteIdx += seqLen;
        ++charIdx;
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

#else

// Stub when MeCab is not available
struct MeCabTokenizer::Impl {
    // Empty stub — ensures unique_ptr<Impl> destructor compiles
};

std::vector<TokenResult> MeCabTokenizer::tokenize(const QString&) { return {}; }
bool MeCabTokenizer::isAvailable() const { return false; }

#endif

MeCabTokenizer::MeCabTokenizer()
#ifdef HAVE_MECAB
    : d(std::make_unique<Impl>())
#endif
{}

MeCabTokenizer::~MeCabTokenizer() = default;
