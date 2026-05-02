#ifndef CONSTANTS_H
#define CONSTANTS_H

namespace Constants {

// ============================================================================
// FILE & DATA LIMITS
// ============================================================================
namespace File {
    constexpr int MAX_FILE_SIZE_MB = 50;
    constexpr int MAX_FILE_SIZE_BYTES = MAX_FILE_SIZE_MB * 1024 * 1024;
    constexpr int WARNING_FILE_SIZE_MB = 10;
}

namespace Term {
    constexpr int DEFAULT_TOKEN_LIMIT = 12;
    constexpr int MAX_TERM_LENGTH = 100;
    constexpr int MAX_DEFINITION_LENGTH = 1000;
    constexpr int MAX_PRONUNCIATION_LENGTH = 200;
    constexpr int PREVIEW_TEXT_LENGTH = 30;  // For CSV preview truncation
}

namespace Progress {
    constexpr int BEGINNER_THRESHOLD = 25;     // < 25% known
    constexpr int INTERMEDIATE_THRESHOLD = 50; // < 50% known
    constexpr int ADVANCED_THRESHOLD = 75;     // < 75% known
    constexpr int PERCENT_MULTIPLIER = 100;
}

namespace Batch {
    constexpr int MAX_ERROR_DISPLAY = 10;      // Max errors to show in UI
    constexpr int MAX_DUPLICATE_DISPLAY = 10;  // Max duplicates to show
    constexpr int LARGE_BATCH_SIZE = 100;      // For performance testing
}

// ============================================================================
// UI DIMENSIONS
// ============================================================================
namespace Window {
    constexpr int DEFAULT_WIDTH = 800;
    constexpr int DEFAULT_HEIGHT = 600;
    constexpr int MIN_WIDTH = 500;
    constexpr int MIN_HEIGHT = 400;
}

namespace Dialog {
    constexpr int VOCAB_IMPORT_WIDTH = 800;
    constexpr int VOCAB_IMPORT_HEIGHT = 600;
    constexpr int LANGUAGE_EDIT_MIN_WIDTH = 500;
    constexpr int LANGUAGE_MANAGER_MIN_WIDTH = 700;
    constexpr int LANGUAGE_MANAGER_MIN_HEIGHT = 400;
    constexpr int SETTINGS_WIDTH = 700;
    constexpr int SETTINGS_HEIGHT = 550;
    constexpr int TEST_DIALOG_MIN_WIDTH = 400;
}

namespace Panel {
    constexpr int TERM_INFO_PREVIEW_HEIGHT = 110;
    constexpr int TERM_INFO_PREVIEW_MIN_HEIGHT = 35;
    constexpr int TERM_INFO_EDIT_HEIGHT = 400;
    constexpr int TERM_INFO_EDIT_MIN_HEIGHT = 300;
    constexpr int TERM_INFO_WARNING_HEIGHT = 180;
    constexpr int TERM_INFO_WARNING_MIN_HEIGHT = 120;
    constexpr int TOP_CONTROLS_HEIGHT = 72;
    constexpr int PREVIEW_TABLE_MAX_HEIGHT = 150;
    constexpr int SCROLL_AREA_MAX_HEIGHT = 150;
    constexpr int SAMPLE_TEXT_MAX_HEIGHT = 100;
}

namespace Control {
    constexpr int LANGUAGE_COMBO_MIN_WIDTH = 200;
    constexpr int LANGUAGE_COMBO_HEIGHT = 40;
    constexpr int IMPORT_BUTTON_MIN_WIDTH = 140;
    constexpr int IMPORT_BUTTON_MIN_HEIGHT = 40;
    constexpr int DELETE_BUTTON_SIZE = 28;
    constexpr int MIN_BUTTON_HEIGHT = 32;
    constexpr int SPIN_BOX_MIN_WIDTH = 80;
    constexpr int SETTINGS_LABEL_MIN_WIDTH = 100;
    constexpr int SETTINGS_BUTTON_MIN_WIDTH = 130;
    constexpr int SETTINGS_BUTTON_MAX_WIDTH = 130;
    constexpr int MIN_WIDTH_FOR_LABELS = 500;  // TermInfoPanel label display threshold
}

namespace TableColumn {
    constexpr int LANGUAGE_NAME_WIDTH = 150;
}

namespace Spacing {
    constexpr int SMALL = 10;
    constexpr int MEDIUM = 12;
    constexpr int LARGE = 15;
    constexpr int EXTRA_LARGE = 16;
}

namespace Margins {
    constexpr int SMALL = 10;
    constexpr int MEDIUM = 12;
    constexpr int LARGE = 16;
}

// ============================================================================
// FONTS
// ============================================================================
namespace Font {
    constexpr int SIZE_DEFAULT = 12;
    constexpr int SIZE_SMALL = 11;
    constexpr int SIZE_MEDIUM = 13;
    constexpr int SIZE_LARGE = 16;
    constexpr int MAX_SIZE = 72;
}

// ============================================================================
// COLORS & VISUAL EFFECTS
// ============================================================================
namespace Color {
    constexpr int BRIGHTNESS_THRESHOLD = 128;  // For determining if background is light/dark
    constexpr int BRIGHTNESS_RED_WEIGHT = 299;
    constexpr int BRIGHTNESS_GREEN_WEIGHT = 587;
    constexpr int BRIGHTNESS_BLUE_WEIGHT = 114;
    constexpr int BRIGHTNESS_DIVISOR = 1000;
}

namespace Shadow {
    constexpr int OPACITY_NORMAL = 25;   // 10% opacity (25/255)
    constexpr int OPACITY_HOVER = 51;    // 20% opacity (51/255)
    constexpr int BLUR_RADIUS_NORMAL = 8;
    constexpr int BLUR_RADIUS_HOVER = 16;
}

namespace Icon {
    constexpr int SIZE = 16;
    constexpr int BORDER_RADIUS = 2;
    constexpr int BORDER_OFFSET = 1;
    constexpr int INNER_SIZE = 14;  // SIZE - 2 * BORDER_OFFSET
}

// ============================================================================
// TIMING
// ============================================================================
namespace Timing {
    constexpr int POSITION_SAVE_DELAY_MS = 2000;  // Delay before saving scroll position
    constexpr int STATUS_MESSAGE_DURATION_MS = 3000;
    constexpr int SHORT_TIMEOUT_MS = 100;
    constexpr int MEDIUM_TIMEOUT_MS = 500;
    constexpr int LONG_TIMEOUT_MS = 1000;
    constexpr int SLEEP_MS = 10;  // For short test delays
}

// ============================================================================
// UNICODE RANGES
// ============================================================================
namespace Unicode {
    // Japanese character ranges
    constexpr const char* HIRAGANA_RANGE = "\\x{3040}-\\x{309F}";
    constexpr const char* KATAKANA_RANGE = "\\x{30A0}-\\x{30FF}";
    constexpr const char* KANJI_RANGE = "\\x{4E00}-\\x{9FFF}";

    // Arabic character ranges
    constexpr const char* ARABIC_RANGE = "\\x{0600}-\\x{06FF}";
    constexpr const char* ARABIC_SUPPLEMENT_RANGE = "\\x{0750}-\\x{077F}";
    constexpr const char* ARABIC_EXTENDED_RANGE = "\\x{08A0}-\\x{08FF}";

    // Western character ranges
    constexpr const char* LATIN_BASIC_RANGE = "a-zA-Z";
    constexpr const char* LATIN_SUPPLEMENT_RANGE = "À-ÖØ-ö";
    constexpr const char* LATIN_EXTENDED_A_RANGE = "\\x{0100}-\\x{01BF}";
    constexpr const char* LATIN_EXTENDED_B_RANGE = "\\x{01C4}-\\x{024F}";
    constexpr const char* GREEK_COPTIC_RANGE = "\\x{0370}-\\x{052F}";
    constexpr const char* LATIN_EXTENDED_ADDITIONAL_RANGE = "\\x{1E00}-\\x{1FFF}";

    // Special characters
    constexpr const char* APOSTROPHE_CURLY = "\\x{2019}";  // Right single quotation mark '
}

// ============================================================================
// TEST DATA
// ============================================================================
// Base URL for downloadable language support modules. Points at the
// kotodama-module-<code>.zip release assets attached to each tagged
// release. Override at compile time with -DKOTODAMA_MODULE_BASE_URL=...
// if the repo ever moves to a different owner/org.
#ifndef KOTODAMA_MODULE_BASE_URL
#define KOTODAMA_MODULE_BASE_URL "https://github.com/jrandallclark/kotodama/releases/latest/download"
#endif
namespace Module {
    constexpr const char* BASE_URL = KOTODAMA_MODULE_BASE_URL;
}

namespace Test {
    constexpr int UUID_LENGTH_WITH_HYPHENS = 36;
    constexpr int UUID_LENGTH_WITHOUT_HYPHENS = 32;
    constexpr int SAMPLE_FILE_SIZE = 1000;
    constexpr int SMALL_FILE_SIZE = 5;
    constexpr int SAMPLE_CHARACTER_COUNT = 20;
    constexpr int OUT_OF_BOUNDS_INDEX = 10;
}

} // namespace Constants

#endif // CONSTANTS_H
