cmake_minimum_required(VERSION 3.16)

project(kotodama VERSION 0.1 LANGUAGES CXX)

# Option to build as dev version (uses separate data directories)
option(DEV_BUILD "Build as development version with separate data directory" ON)

# Google Test setup
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Sql Network)

# Find MeCab (optional)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(MECAB QUIET mecab)
endif()

if(NOT MECAB_FOUND)
    find_path(MECAB_INCLUDE_DIR mecab.h
        PATHS /opt/homebrew/include /usr/local/include /usr/include
        NO_DEFAULT_PATH)
    find_library(MECAB_LIBRARY mecab
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
        NO_DEFAULT_PATH)
    if(MECAB_INCLUDE_DIR AND MECAB_LIBRARY)
        set(MECAB_FOUND TRUE CACHE INTERNAL "" FORCE)
        set(MECAB_INCLUDE_DIRS ${MECAB_INCLUDE_DIR})
        set(MECAB_LIBRARIES ${MECAB_LIBRARY})
    endif()
endif()

# Set include directories for the project
include_directories(${CMAKE_SOURCE_DIR}/include)

set(TS_FILES kotodama_en_US.ts)

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        include/kotodama/mainwindow.h
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(kotodama
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        include/kotodama/ebookviewer.h src/ebookviewer.cpp
        include/kotodama/ebookviewmodel.h src/ebookviewmodel.cpp
        include/kotodama/mainwindowmodel.h src/mainwindowmodel.cpp
        include/kotodama/databasemanager.h src/databasemanager.cpp
        include/kotodama/librarymanager.h src/librarymanager.cpp
        src/trienode.cpp
        include/kotodama/trienode.h
        include/kotodama/term.h
        include/kotodama/tokenizerbackend.h
        include/kotodama/tokenizer.h
        src/tokenizer.cpp
        include/kotodama/regextokenizer.h src/regextokenizer.cpp
        include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
        include/kotodama/languageconfig.h src/languageconfig.cpp
        include/kotodama/languagemanager.h src/languagemanager.cpp
        include/kotodama/termmanager.h src/termmanager.cpp
        include/kotodama/importvocabdialog.h src/importvocabdialog.cpp
        include/kotodama/languageeditdialog.h src/languageeditdialog.cpp
        include/kotodama/languagemanagerdialog.h src/languagemanagerdialog.cpp
        include/kotodama/languageselectionwidget.h src/languageselectionwidget.cpp
        include/kotodama/languageselectiondialog.h src/languageselectiondialog.cpp
        include/kotodama/settingsdialog.h src/settingsdialog.cpp
        include/kotodama/textcard.h src/textcard.cpp
        include/kotodama/terminfopanel.h src/terminfopanel.cpp
        include/kotodama/thememanager.h src/thememanager.cpp
        include/kotodama/progresscalculator.h src/progresscalculator.cpp
        include/kotodama/singleinstanceguard.h src/singleinstanceguard.cpp
        include/kotodama/aimanager.h src/aimanager.cpp
        include/kotodama/backupmanager.h src/backupmanager.cpp
        include/kotodama/constants.h
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET kotodama APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(kotodama SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(kotodama
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(kotodama PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Network)

if(MECAB_FOUND)
    target_compile_definitions(kotodama PRIVATE HAVE_MECAB=1)
    target_include_directories(kotodama PRIVATE ${MECAB_INCLUDE_DIRS})
    target_link_libraries(kotodama PRIVATE ${MECAB_LIBRARIES})
    message(STATUS "MeCab found — building with HAVE_MECAB")
else()
    message(STATUS "MeCab not found — Japanese will fall back to regex tokenizer")
endif()

# Add DEV_BUILD preprocessor definition if enabled
if(DEV_BUILD)
    target_compile_definitions(kotodama PRIVATE DEV_BUILD)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(DEV_BUILD)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.yourcompany.kotodama-dev)
else()
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.yourcompany.kotodama)
endif()

set_target_properties(kotodama PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS kotodama
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(kotodama)
endif()

# Tests
enable_testing()

add_executable(
  tokenizer_test
  tests/tst_tokenizer.cpp
  include/kotodama/tokenizerbackend.h
  include/kotodama/tokenizer.h
  src/tokenizer.cpp
  include/kotodama/regextokenizer.h src/regextokenizer.cpp
  include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
  src/languageconfig.cpp
  include/kotodama/languageconfig.h
  src/languagemanager.cpp
  include/kotodama/languagemanager.h
  src/databasemanager.cpp
  include/kotodama/databasemanager.h
  src/librarymanager.cpp
  include/kotodama/librarymanager.h
  include/kotodama/term.h
)

target_link_libraries(
  tokenizer_test
  GTest::gtest_main
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Sql
)

include(GoogleTest)
gtest_discover_tests(tokenizer_test)

# TrieNode tests
add_executable(
  trienode_test
  tests/tst_trienode.cpp
  src/trienode.cpp
  include/kotodama/trienode.h
  include/kotodama/term.h
)

target_link_libraries(
  trienode_test
  GTest::gtest_main
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
)

gtest_discover_tests(trienode_test)

# LanguageConfig tests
add_executable(
  languageconfig_test
  tests/tst_languageconfig.cpp
  tests/test_main.cpp
  src/languageconfig.cpp
  include/kotodama/languageconfig.h
  src/languagemanager.cpp
  include/kotodama/languagemanager.h
  src/databasemanager.cpp
  include/kotodama/databasemanager.h
  src/librarymanager.cpp
  include/kotodama/librarymanager.h
  include/kotodama/tokenizerbackend.h
  include/kotodama/tokenizer.h
  src/tokenizer.cpp
  include/kotodama/regextokenizer.h src/regextokenizer.cpp
  include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
  include/kotodama/term.h
)

target_link_libraries(
  languageconfig_test
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Sql
)

gtest_discover_tests(languageconfig_test)

# TermManager tests (integration tests - uses real database)
add_executable(
  termmanager_test
  tests/tst_termmanager.cpp
  tests/test_main.cpp
  src/termmanager.cpp
  include/kotodama/termmanager.h
  src/databasemanager.cpp
  include/kotodama/databasemanager.h
  src/librarymanager.cpp
  include/kotodama/librarymanager.h
  src/languageconfig.cpp
  include/kotodama/languageconfig.h
  src/languagemanager.cpp
  include/kotodama/languagemanager.h
  include/kotodama/tokenizerbackend.h
  include/kotodama/tokenizer.h
  src/tokenizer.cpp
  include/kotodama/regextokenizer.h src/regextokenizer.cpp
  include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
  src/trienode.cpp
  include/kotodama/trienode.h
  include/kotodama/term.h
  include/kotodama/constants.h
)

target_link_libraries(
  termmanager_test
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Sql
)

gtest_discover_tests(termmanager_test)

# LibraryManager tests
add_executable(
  librarymanager_test
  tests/tst_librarymanager.cpp
  tests/simple_test_main.cpp
  src/librarymanager.cpp
  include/kotodama/librarymanager.h
)

target_link_libraries(
  librarymanager_test
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
)

gtest_discover_tests(librarymanager_test)

# DatabaseManager tests
add_executable(
  databasemanager_test
  tests/tst_databasemanager.cpp
  tests/test_main.cpp
  src/databasemanager.cpp
  include/kotodama/databasemanager.h
  src/librarymanager.cpp
  include/kotodama/librarymanager.h
  src/languageconfig.cpp
  include/kotodama/languageconfig.h
  src/languagemanager.cpp
  include/kotodama/languagemanager.h
  include/kotodama/tokenizerbackend.h
  include/kotodama/tokenizer.h
  src/tokenizer.cpp
  include/kotodama/regextokenizer.h src/regextokenizer.cpp
  include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
  include/kotodama/term.h
)

target_link_libraries(
  databasemanager_test
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Sql
)

gtest_discover_tests(databasemanager_test)

# EbookViewModel tests
add_executable(
  ebookviewmodel_test
  tests/tst_ebookviewmodel.cpp
  tests/test_main.cpp
  src/ebookviewmodel.cpp
  include/kotodama/ebookviewmodel.h
  src/termmanager.cpp
  include/kotodama/termmanager.h
  src/databasemanager.cpp
  include/kotodama/databasemanager.h
  src/librarymanager.cpp
  include/kotodama/librarymanager.h
  src/languageconfig.cpp
  include/kotodama/languageconfig.h
  src/languagemanager.cpp
  include/kotodama/languagemanager.h
  include/kotodama/tokenizerbackend.h
  include/kotodama/tokenizer.h
  src/tokenizer.cpp
  include/kotodama/regextokenizer.h src/regextokenizer.cpp
  include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
  src/trienode.cpp
  include/kotodama/trienode.h
  include/kotodama/term.h
  include/kotodama/constants.h
)

target_link_libraries(
  ebookviewmodel_test
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Sql
)

gtest_discover_tests(ebookviewmodel_test)

# MainWindowModel tests
add_executable(
  mainwindowmodel_test
  tests/tst_mainwindowmodel.cpp
  tests/test_main.cpp
  src/mainwindowmodel.cpp
  include/kotodama/mainwindowmodel.h
  src/progresscalculator.cpp
  include/kotodama/progresscalculator.h
  src/ebookviewmodel.cpp
  include/kotodama/ebookviewmodel.h
  src/databasemanager.cpp
  include/kotodama/databasemanager.h
  src/librarymanager.cpp
  include/kotodama/librarymanager.h
  src/languageconfig.cpp
  include/kotodama/languageconfig.h
  src/languagemanager.cpp
  include/kotodama/languagemanager.h
  src/termmanager.cpp
  include/kotodama/termmanager.h
  include/kotodama/tokenizerbackend.h
  include/kotodama/tokenizer.h
  src/tokenizer.cpp
  include/kotodama/regextokenizer.h src/regextokenizer.cpp
  include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
  src/trienode.cpp
  include/kotodama/trienode.h
  include/kotodama/term.h
  include/kotodama/constants.h
)

target_link_libraries(
  mainwindowmodel_test
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Gui
  Qt${QT_VERSION_MAJOR}::Sql
)

gtest_discover_tests(mainwindowmodel_test)

# SingleInstanceGuard tests
add_executable(
  singleinstanceguard_test
  tests/tst_singleinstanceguard.cpp
  tests/simple_test_main.cpp
  src/singleinstanceguard.cpp
  include/kotodama/singleinstanceguard.h
)

target_link_libraries(
  singleinstanceguard_test
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network
)

gtest_discover_tests(singleinstanceguard_test)

# BackupManager tests
add_executable(
  backupmanager_test
  tests/tst_backupmanager.cpp
  tests/test_main.cpp
  src/backupmanager.cpp
  include/kotodama/backupmanager.h
  src/databasemanager.cpp
  include/kotodama/databasemanager.h
  src/librarymanager.cpp
  include/kotodama/librarymanager.h
  src/languagemanager.cpp
  include/kotodama/languagemanager.h
  src/languageconfig.cpp
  include/kotodama/languageconfig.h
  include/kotodama/tokenizerbackend.h
  include/kotodama/tokenizer.h
  src/tokenizer.cpp
  include/kotodama/regextokenizer.h src/regextokenizer.cpp
  include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
  include/kotodama/term.h
)

target_link_libraries(
  backupmanager_test
  GTest::gtest
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Sql
)

gtest_discover_tests(backupmanager_test)

# Helper: apply MeCab flags to a test target if MeCab was found
function(apply_mecab_flags target)
    if(MECAB_FOUND)
        target_compile_definitions(${target} PRIVATE HAVE_MECAB=1)
        target_include_directories(${target} PRIVATE ${MECAB_INCLUDE_DIRS})
        target_link_libraries(${target} ${MECAB_LIBRARIES})
    endif()
endfunction()

# Apply MeCab flags to all test targets that include mecabtokenizer
apply_mecab_flags(tokenizer_test)
apply_mecab_flags(languageconfig_test)
apply_mecab_flags(termmanager_test)
apply_mecab_flags(databasemanager_test)
apply_mecab_flags(ebookviewmodel_test)
apply_mecab_flags(mainwindowmodel_test)
apply_mecab_flags(backupmanager_test)

# RegexTokenizer tests
add_executable(
  regextokenizer_test
  tests/tst_regextokenizer.cpp
  include/kotodama/tokenizerbackend.h
  include/kotodama/regextokenizer.h src/regextokenizer.cpp
)

target_link_libraries(
  regextokenizer_test
  GTest::gtest_main
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
)

gtest_discover_tests(regextokenizer_test)

# MeCabTokenizer tests
add_executable(
  mecabtokenizer_test
  tests/tst_mecabtokenizer.cpp
  include/kotodama/tokenizerbackend.h
  include/kotodama/mecabtokenizer.h src/mecabtokenizer.cpp
)

target_link_libraries(
  mecabtokenizer_test
  GTest::gtest_main
  GTest::gmock
  Qt${QT_VERSION_MAJOR}::Core
)

apply_mecab_flags(mecabtokenizer_test)

gtest_discover_tests(mecabtokenizer_test)
