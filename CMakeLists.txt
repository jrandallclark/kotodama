cmake_minimum_required(VERSION 3.16)

project(kotodama VERSION 0.1 LANGUAGES C CXX)

# Option to build as dev version (uses separate data directories)
option(DEV_BUILD "Build as development version with separate data directory" ON)

# Option to build tests (disabled for sandboxed builds like Flatpak)
option(BUILD_TESTS "Build the test suite" ON)

include(FetchContent)

if(BUILD_TESTS)
  # Google Test setup
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  # For Windows: Prevent overriding the parent project's compiler/linker settings
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# miniz: single-file zip extraction library used by LanguageModuleManager
# to unpack downloaded language support modules.
# miniz: pulled either from a vendored copy at third_party/miniz (used by
# sandboxed builds like Flatpak that have no network at configure time) or
# downloaded via FetchContent. The vendored copy is populated by the
# Flatpak manifest as a separate `archive` source.
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/miniz/miniz.c")
    set(_miniz_dir "${CMAKE_SOURCE_DIR}/third_party/miniz")
    message(STATUS "miniz: using vendored copy at ${_miniz_dir}")
else()
    FetchContent_Declare(
      miniz
      GIT_REPOSITORY https://github.com/richgel999/miniz.git
      GIT_TAG 3.0.2
      GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(miniz)
    if(NOT miniz_POPULATED)
      FetchContent_Populate(miniz)
    endif()
    set(_miniz_dir "${miniz_SOURCE_DIR}")
    message(STATUS "miniz: fetched via FetchContent at ${_miniz_dir}")
endif()
# miniz.h includes a generated miniz_export.h which is normally produced
# by miniz's own CMake build. Synthesize a stub for static linkage.
set(_miniz_stub_dir "${CMAKE_BINARY_DIR}/miniz_stub")
file(MAKE_DIRECTORY "${_miniz_stub_dir}")
file(WRITE "${_miniz_stub_dir}/miniz_export.h"
     "#ifndef MINIZ_EXPORT_H\n#define MINIZ_EXPORT_H\n#define MINIZ_EXPORT\n#endif\n")

# The repo ships the 4 component files separately; amalgamation requires
# running their build script. Easier to just compile all four.
add_library(kotodama_miniz STATIC
    ${_miniz_dir}/miniz.c
    ${_miniz_dir}/miniz_tdef.c
    ${_miniz_dir}/miniz_tinfl.c
    ${_miniz_dir}/miniz_zip.c
)
target_include_directories(kotodama_miniz PUBLIC ${_miniz_dir} "${_miniz_stub_dir}")
set_property(TARGET kotodama_miniz PROPERTY POSITION_INDEPENDENT_CODE ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools Sql Network)

# Find MeCab (required).
# Use find_path/find_library to get absolute paths (rather than pkg-config,
# which returns library names and directories that aren't propagated to the
# linker by default — caused undefined-symbol failures on macOS arm64).
find_path(MECAB_INCLUDE_DIR mecab.h
    PATHS /opt/homebrew/include /usr/local/include /usr/include)
find_library(MECAB_LIBRARY mecab
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
    NO_DEFAULT_PATH)
if(MECAB_INCLUDE_DIR AND MECAB_LIBRARY)
    set(MECAB_FOUND TRUE CACHE INTERNAL "" FORCE)
    set(MECAB_INCLUDE_DIRS ${MECAB_INCLUDE_DIR})
    set(MECAB_LIBRARIES ${MECAB_LIBRARY})
endif()

if(NOT MECAB_FOUND)
    # vcpkg provides a CMake config for MeCab on Windows.
    # Note: vcpkg installs the header to include/mecab/mecab.h, but the
    # imported target's INTERFACE_INCLUDE_DIRECTORIES is just <prefix>/include.
    # Our sources do #include <mecab.h>, so add the include/mecab subdirectory.
    find_package(mecab CONFIG QUIET)
    if(mecab_FOUND)
        set(MECAB_FOUND TRUE CACHE INTERNAL "" FORCE)
        get_target_property(_mecab_iface_inc mecab::mecab INTERFACE_INCLUDE_DIRECTORIES)
        set(MECAB_INCLUDE_DIRS "")
        foreach(_inc IN LISTS _mecab_iface_inc)
            list(APPEND MECAB_INCLUDE_DIRS "${_inc}" "${_inc}/mecab")
        endforeach()
        set(MECAB_LIBRARIES mecab::mecab)
    endif()
endif()

if(NOT MECAB_FOUND)
    message(FATAL_ERROR "MeCab is required but was not found. Install MeCab and ensure mecab.h and the mecab library are in your include/library paths.")
endif()

message(STATUS "MECAB_INCLUDE_DIRS=${MECAB_INCLUDE_DIRS}")
message(STATUS "MECAB_LIBRARIES=${MECAB_LIBRARIES}")
if(NOT MECAB_LIBRARIES)
    message(FATAL_ERROR "MECAB_LIBRARIES is empty despite MECAB_FOUND=TRUE. Check find logic above.")
endif()

# Set include directories for the project
include_directories(${CMAKE_SOURCE_DIR}/include)

set(TS_FILES kotodama_en_US.ts)

set(PROJECT_SOURCES
        src/main.cpp
        src/mainwindow.cpp
        include/kotodama/mainwindow.h
        resources/resources.qrc
        ${TS_FILES}
)

set(ICON_RESOURCES)
if(APPLE)
    set(KOTODAMA_ICON ${CMAKE_SOURCE_DIR}/resources/icons/Kotodama.icns)
    set_source_files_properties(${KOTODAMA_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    list(APPEND ICON_RESOURCES ${KOTODAMA_ICON})
elseif(WIN32)
    list(APPEND ICON_RESOURCES ${CMAKE_SOURCE_DIR}/kotodama.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(kotodama
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        ${ICON_RESOURCES}
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
        include/kotodama/languagemodulemanager.h src/languagemodulemanager.cpp
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
            ${ICON_RESOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

target_link_libraries(kotodama PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Sql Qt${QT_VERSION_MAJOR}::Network kotodama_miniz)

target_include_directories(kotodama PRIVATE ${MECAB_INCLUDE_DIRS})
target_link_libraries(kotodama PRIVATE ${MECAB_LIBRARIES})

# ----------------------------------------------------------------------------
# Locate the MeCab IPA dictionary for *test runs only*. The dict is no longer
# bundled with the installed app — it's downloaded on demand at runtime by
# LanguageModuleManager into <AppDataLocation>/modules/ja/dic/. Tests still
# need a dict though, and they read it via the KOTODAMA_MECAB_DICDIR env var
# (set in the test target ENVIRONMENT below).
# ----------------------------------------------------------------------------
if(NOT KOTODAMA_IPADIC_DIR)
    if(APPLE)
        execute_process(
            COMMAND brew --prefix mecab-ipadic
            OUTPUT_VARIABLE _brew_ipadic_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE _brew_rc
            ERROR_QUIET
        )
        if(_brew_rc EQUAL 0 AND EXISTS "${_brew_ipadic_prefix}/lib/mecab/dic/ipadic/sys.dic")
            set(KOTODAMA_IPADIC_DIR "${_brew_ipadic_prefix}/lib/mecab/dic/ipadic")
        endif()
    else()
        foreach(_p
            "/usr/lib/x86_64-linux-gnu/mecab/dic/ipadic-utf8"
            "/usr/lib/mecab/dic/ipadic-utf8"
            "/usr/local/lib/mecab/dic/ipadic")
            if(EXISTS "${_p}/sys.dic")
                set(KOTODAMA_IPADIC_DIR "${_p}")
                break()
            endif()
        endforeach()
    endif()
endif()

if(KOTODAMA_IPADIC_DIR AND EXISTS "${KOTODAMA_IPADIC_DIR}/sys.dic")
    message(STATUS "MeCab IPA dict (tests only): ${KOTODAMA_IPADIC_DIR}")
elseif(BUILD_TESTS)
    message(WARNING
        "MeCab IPA dictionary not found. Tests that exercise MeCab will fail. "
        "Install mecab-ipadic (brew install mecab-ipadic on macOS, "
        "apt install mecab-ipadic-utf8 on Debian/Ubuntu) or pass "
        "-DKOTODAMA_IPADIC_DIR=/path/to/ipadic.")
endif()

# Add DEV_BUILD preprocessor definition if enabled
if(DEV_BUILD)
    target_compile_definitions(kotodama PRIVATE DEV_BUILD)
endif()

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(DEV_BUILD)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.jeremyrclark.kotodama-dev)
else()
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.jeremyrclark.kotodama)
endif()

set_target_properties(kotodama PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE Kotodama.icns
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

if(BUILD_TESTS)
  # Tests
  enable_testing()

  # Tests that touch Tokenizer::createJapanese / MeCabTokenizer need the IPA
  # dict at runtime. Point them at the dict CMake located above.
  set(KOTODAMA_TEST_ENV "KOTODAMA_MECAB_DICDIR=${KOTODAMA_IPADIC_DIR}")

  # Common test library to avoid duplicating sources across test targets
  add_library(
    kotodama_test_common
    STATIC
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
    include/kotodama/regextokenizer.h
    src/regextokenizer.cpp
    include/kotodama/mecabtokenizer.h
    src/mecabtokenizer.cpp
    src/trienode.cpp
    include/kotodama/trienode.h
    include/kotodama/term.h
    include/kotodama/constants.h
  )

  target_include_directories(
    kotodama_test_common
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
  )

  target_link_libraries(
    kotodama_test_common
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
    ${MECAB_LIBRARIES}
  )

  target_include_directories(kotodama_test_common PUBLIC ${MECAB_INCLUDE_DIRS})

  add_executable(
    tokenizer_test
    tests/tst_tokenizer.cpp
  )

  target_link_libraries(
    tokenizer_test
    kotodama_test_common
    GTest::gtest_main
    GTest::gmock
  )

  include(GoogleTest)
  gtest_discover_tests(tokenizer_test PROPERTIES ENVIRONMENT "${KOTODAMA_TEST_ENV}")

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
  )

  target_link_libraries(
    languageconfig_test
    kotodama_test_common
    GTest::gtest
    GTest::gmock
  )

  gtest_discover_tests(languageconfig_test PROPERTIES ENVIRONMENT "${KOTODAMA_TEST_ENV}")

  # TermManager tests (integration tests - uses real database)
  add_executable(
    termmanager_test
    tests/tst_termmanager.cpp
    tests/test_main.cpp
    src/termmanager.cpp
    include/kotodama/termmanager.h
  )

  target_link_libraries(
    termmanager_test
    kotodama_test_common
    GTest::gtest
    GTest::gmock
  )

  gtest_discover_tests(termmanager_test PROPERTIES ENVIRONMENT "${KOTODAMA_TEST_ENV}")

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
  )

  target_link_libraries(
    databasemanager_test
    kotodama_test_common
    GTest::gtest
    GTest::gmock
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
  )

  target_link_libraries(
    ebookviewmodel_test
    kotodama_test_common
    GTest::gtest
    GTest::gmock
    Qt${QT_VERSION_MAJOR}::Gui
  )

  gtest_discover_tests(ebookviewmodel_test PROPERTIES ENVIRONMENT "${KOTODAMA_TEST_ENV}")

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
    src/termmanager.cpp
    include/kotodama/termmanager.h
    src/languagemodulemanager.cpp
    include/kotodama/languagemodulemanager.h
  )

  target_link_libraries(
    mainwindowmodel_test
    kotodama_test_common
    GTest::gtest
    GTest::gmock
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Network
    kotodama_miniz
  )

  gtest_discover_tests(mainwindowmodel_test PROPERTIES ENVIRONMENT "${KOTODAMA_TEST_ENV}")

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
  )

  target_link_libraries(
    backupmanager_test
    kotodama_test_common
    GTest::gtest
    GTest::gmock
  )

  gtest_discover_tests(backupmanager_test)

  # Helper: apply MeCab include/link to a test target
  function(apply_mecab_flags target)
      target_include_directories(${target} PRIVATE ${MECAB_INCLUDE_DIRS})
      target_link_libraries(${target} ${MECAB_LIBRARIES})
  endfunction()

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

  gtest_discover_tests(mecabtokenizer_test
      PROPERTIES ENVIRONMENT "KOTODAMA_MECAB_DICDIR=${KOTODAMA_IPADIC_DIR}")
endif()
