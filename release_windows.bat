@echo off
REM ============================================
REM Kotodama Windows Release Build Script
REM ============================================

setlocal enabledelayedexpansion

REM Configuration
set APP_NAME=kotodama
set VERSION=0.1
set BUILD_DIR=build
set RELEASE_DIR=release

echo ========================================
echo Kotodama Windows Release Builder
echo Version: %VERSION%
echo ========================================
echo.

REM ============================================
REM Step 0: Detect Build Environment
REM ============================================
echo [0/5] Detecting build environment...

REM Try to find Qt installation
set QT_PATH=
set QT_COMPILER=
set CMAKE_GENERATOR=

REM Check for MinGW Qt installations first (more common for open source)
for %%Q in (
    "C:\Qt\6.10.1\mingw_64"
    "C:\Qt\6.9.0\mingw_64"
    "C:\Qt\6.8.0\mingw_64"
    "C:\Qt\6.7.0\mingw_64"
    "C:\Qt\6.6.0\mingw_64"
    "C:\Qt\6.5.0\mingw_64"
    "%USERPROFILE%\Qt\6.10.1\mingw_64"
    "%USERPROFILE%\Qt\6.9.0\mingw_64"
    "%USERPROFILE%\Qt\6.8.0\mingw_64"
) do (
    if exist "%%~Q\bin\qmake.exe" (
        set "QT_PATH=%%~Q"
        set "QT_COMPILER=mingw"
        goto :qt_found
    )
)

REM Check for MSVC Qt installations
for %%Q in (
    "C:\Qt\6.10.1\msvc2022_64"
    "C:\Qt\6.9.0\msvc2022_64"
    "C:\Qt\6.8.0\msvc2022_64"
    "C:\Qt\6.7.0\msvc2022_64"
    "C:\Qt\6.6.0\msvc2022_64"
    "C:\Qt\6.5.0\msvc2022_64"
    "%USERPROFILE%\Qt\6.10.1\msvc2022_64"
    "%USERPROFILE%\Qt\6.9.0\msvc2022_64"
    "%USERPROFILE%\Qt\6.8.0\msvc2022_64"
) do (
    if exist "%%~Q\bin\qmake.exe" (
        set "QT_PATH=%%~Q"
        set "QT_COMPILER=msvc"
        goto :qt_found
    )
)

REM Qt not found - provide installation instructions
echo.
echo ERROR: Qt installation not found!
echo.
echo Please install Qt 6.x with MinGW or MSVC support:
echo.
echo   1. Download Qt Online Installer from: https://www.qt.io/download-qt-installer
echo   2. Run the installer and sign in (free account required)
echo   3. Select "Qt 6.x" (e.g., Qt 6.10.1)
echo   4. Under the Qt version, select one of:
echo      - MinGW 64-bit (recommended, no Visual Studio needed)
echo      - MSVC 2022 64-bit (requires Visual Studio)
echo   5. Complete the installation
echo.
echo   Default installation path: C:\Qt
echo.
echo After installation, run this script again.
echo.
exit /b 1

:qt_found
echo   Found Qt at: %QT_PATH%
echo   Compiler: %QT_COMPILER%
set WINDEPLOYQT=%QT_PATH%\bin\windeployqt.exe

REM Configure based on compiler type
if "%QT_COMPILER%"=="mingw" (
    call :setup_mingw
) else (
    call :setup_msvc
)

if errorlevel 1 exit /b 1

REM Check for CMake
where cmake >nul 2>nul
if errorlevel 1 (
    echo.
    echo ERROR: CMake not found in PATH!
    echo.
    echo Please install CMake:
    echo   Download from: https://cmake.org/download/
    echo   During installation, select "Add CMake to system PATH"
    echo.
    echo   Or install via winget: winget install Kitware.CMake
    echo.
    exit /b 1
)

for /f "tokens=3" %%v in ('cmake --version ^| findstr /i "version"') do set CMAKE_VERSION=%%v
echo   Found CMake version: %CMAKE_VERSION%

echo.
echo Build environment OK!
echo.

REM ============================================
REM Step 1: Clean and prepare
REM ============================================
echo [1/5] Cleaning previous builds...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if not exist "%RELEASE_DIR%" mkdir "%RELEASE_DIR%"

REM ============================================
REM Step 2: Configure CMake for Release
REM ============================================
echo [2/5] Configuring CMake (Release mode)...

if "%QT_COMPILER%"=="mingw" (
    cmake -B "%BUILD_DIR%" -S . ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_PREFIX_PATH="%QT_PATH%" ^
        -DDEV_BUILD=OFF ^
        -G "MinGW Makefiles"
) else (
    cmake -B "%BUILD_DIR%" -S . ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_PREFIX_PATH="%QT_PATH%" ^
        -DDEV_BUILD=OFF ^
        -G "%CMAKE_GENERATOR%" ^
        -A x64
)

if errorlevel 1 (
    echo.
    echo ERROR: CMake configuration failed!
    echo.
    echo Common fixes:
    echo   - Ensure Qt is properly installed
    echo   - For MinGW: Make sure Qt's MinGW bin folder is in PATH
    echo   - For MSVC: Try running from "Developer Command Prompt for VS 2022"
    echo.
    exit /b 1
)

REM ============================================
REM Step 3: Build the application
REM ============================================
echo [3/5] Building application...

if "%QT_COMPILER%"=="mingw" (
    cmake --build "%BUILD_DIR%" --config Release -j%NUMBER_OF_PROCESSORS%
) else (
    cmake --build "%BUILD_DIR%" --config Release --parallel
)

if errorlevel 1 (
    echo.
    echo ERROR: Build failed!
    echo.
    exit /b 1
)

REM Find the executable
set "EXE_PATH="
if exist "%BUILD_DIR%\Release\%APP_NAME%.exe" (
    set "EXE_PATH=%BUILD_DIR%\Release\%APP_NAME%.exe"
    set "EXE_DIR=%BUILD_DIR%\Release"
) else if exist "%BUILD_DIR%\%APP_NAME%.exe" (
    set "EXE_PATH=%BUILD_DIR%\%APP_NAME%.exe"
    set "EXE_DIR=%BUILD_DIR%"
) else (
    echo ERROR: %APP_NAME%.exe not found!
    echo Searching for it...
    for /r "%BUILD_DIR%" %%f in (%APP_NAME%.exe) do (
        echo Found: %%f
        set "EXE_PATH=%%f"
        set "EXE_DIR=%%~dpf"
    )
    if not defined EXE_PATH (
        echo Could not find built executable.
        exit /b 1
    )
)

echo   Built executable: %EXE_PATH%

REM ============================================
REM Step 4: Create deployment directory and bundle Qt
REM ============================================
echo [4/5] Creating deployment directory and bundling Qt DLLs...
set DEPLOY_DIR=%BUILD_DIR%\deploy
if exist "%DEPLOY_DIR%" rmdir /s /q "%DEPLOY_DIR%"
mkdir "%DEPLOY_DIR%"

REM Copy executable
copy "%EXE_PATH%" "%DEPLOY_DIR%\"

REM Run windeployqt to bundle Qt dependencies
echo Running windeployqt...
"%WINDEPLOYQT%" --release --no-translations --no-opengl-sw "%DEPLOY_DIR%\%APP_NAME%.exe"

if errorlevel 1 (
    echo.
    echo ERROR: windeployqt failed!
    echo.
    exit /b 1
)

REM For MinGW, also copy required runtime DLLs
if "%QT_COMPILER%"=="mingw" (
    echo Copying MinGW runtime DLLs...

    REM Find and copy MinGW runtime DLLs from Qt's bin directory
    if exist "%QT_PATH%\bin\libgcc_s_seh-1.dll" (
        copy "%QT_PATH%\bin\libgcc_s_seh-1.dll" "%DEPLOY_DIR%\" >nul
    )
    if exist "%QT_PATH%\bin\libstdc++-6.dll" (
        copy "%QT_PATH%\bin\libstdc++-6.dll" "%DEPLOY_DIR%\" >nul
    )
    if exist "%QT_PATH%\bin\libwinpthread-1.dll" (
        copy "%QT_PATH%\bin\libwinpthread-1.dll" "%DEPLOY_DIR%\" >nul
    )

    REM Also check Qt Tools MinGW folder
    for /d %%D in ("C:\Qt\Tools\mingw*") do (
        if exist "%%D\bin\libgcc_s_seh-1.dll" (
            copy "%%D\bin\libgcc_s_seh-1.dll" "%DEPLOY_DIR%\" >nul 2>nul
        )
        if exist "%%D\bin\libstdc++-6.dll" (
            copy "%%D\bin\libstdc++-6.dll" "%DEPLOY_DIR%\" >nul 2>nul
        )
        if exist "%%D\bin\libwinpthread-1.dll" (
            copy "%%D\bin\libwinpthread-1.dll" "%DEPLOY_DIR%\" >nul 2>nul
        )
    )
)

echo Qt dependencies bundled successfully

REM ============================================
REM Step 5: Create installer/package
REM ============================================
echo [5/5] Creating installer...

REM Check if NSIS is installed
set MAKENSIS=
where makensis >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    set "MAKENSIS=makensis"
) else if exist "%ProgramFiles(x86)%\NSIS\makensis.exe" (
    set "MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
) else if exist "%ProgramFiles%\NSIS\makensis.exe" (
    set "MAKENSIS=%ProgramFiles%\NSIS\makensis.exe"
)

if defined MAKENSIS (
    echo NSIS found, creating installer...
    call :create_nsis_installer
) else (
    echo NSIS not found, creating ZIP package instead...
    call :create_zip_package
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo Installer/package created in: %RELEASE_DIR%\
echo.
echo Next steps:
echo 1. Test the installer on a clean Windows system
echo 2. Test on Windows 10 and Windows 11
echo 3. Consider code signing for trusted distribution
echo.
echo Done!
goto :eof

REM ============================================
REM Function: Setup MinGW environment
REM ============================================
:setup_mingw
    echo   Setting up MinGW build environment...

    REM Find MinGW in Qt Tools
    set MINGW_PATH=
    for /d %%D in ("C:\Qt\Tools\mingw*") do (
        if exist "%%D\bin\gcc.exe" (
            set "MINGW_PATH=%%D"
        )
    )

    if not defined MINGW_PATH (
        REM Check if mingw32-make is already in PATH
        where mingw32-make >nul 2>nul
        if errorlevel 1 (
            echo.
            echo ERROR: MinGW compiler not found!
            echo.
            echo Please install MinGW via Qt Maintenance Tool:
            echo   1. Run C:\Qt\MaintenanceTool.exe
            echo   2. Select "Add or remove components"
            echo   3. Under "Developer and Designer Tools", select MinGW
            echo   4. Complete the installation
            echo.
            exit /b 1
        )
        echo   Using MinGW from PATH
    ) else (
        echo   Found MinGW at: %MINGW_PATH%
        set "PATH=%MINGW_PATH%\bin;%PATH%"
    )

    REM Also add Qt bin to PATH for the build
    set "PATH=%QT_PATH%\bin;%PATH%"

    set CMAKE_GENERATOR=MinGW Makefiles
    goto :eof

REM ============================================
REM Function: Setup MSVC environment
REM ============================================
:setup_msvc
    echo   Setting up MSVC build environment...

    set VS_FOUND=0
    set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

    if exist %VSWHERE% (
        for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
            set "VS_PATH=%%i"
            set VS_FOUND=1
        )
    )

    if %VS_FOUND%==0 (
        if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
            set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
            set VS_FOUND=1
        )
        if exist "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
            set "VS_PATH=%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
            set VS_FOUND=1
        )
    )

    if %VS_FOUND%==0 (
        echo.
        echo ERROR: Visual Studio or Build Tools not found!
        echo.
        echo For MSVC Qt builds, please install Visual Studio 2022:
        echo   Download from: https://visualstudio.microsoft.com/vs/community/
        echo   Select "Desktop development with C++"
        echo.
        echo Alternatively, reinstall Qt with MinGW support instead.
        echo.
        exit /b 1
    )

    echo   Found Visual Studio at: %VS_PATH%
    set CMAKE_GENERATOR=Visual Studio 17 2022
    goto :eof

REM ============================================
REM Function: Create NSIS Installer
REM ============================================
:create_nsis_installer
    set NSI_FILE=%BUILD_DIR%\installer.nsi

    REM Generate NSIS script (disable delayed expansion to preserve ! characters)
    setlocal disabledelayedexpansion
    (
        echo !define PRODUCT_NAME "Kotodama"
        echo !define PRODUCT_VERSION "%VERSION%"
        echo !define PRODUCT_PUBLISHER "Kotodama"
        echo !define INSTALL_DIR "$PROGRAMFILES64\Kotodama"
        echo.
        echo Name "${PRODUCT_NAME} ${PRODUCT_VERSION}"
        echo OutFile "..\%RELEASE_DIR%\%APP_NAME%-${PRODUCT_VERSION}-windows-installer.exe"
        echo InstallDir "${INSTALL_DIR}"
        echo RequestExecutionLevel admin
        echo.
        echo Page directory
        echo Page instfiles
        echo UninstPage uninstConfirm
        echo UninstPage instfiles
        echo.
        echo Section "Install"
        echo   SetOutPath "$INSTDIR"
        echo   File /r "deploy\*.*"
        echo.
        echo   CreateDirectory "$SMPROGRAMS\Kotodama"
        echo   CreateShortcut "$SMPROGRAMS\Kotodama\Kotodama.lnk" "$INSTDIR\%APP_NAME%.exe"
        echo   CreateShortcut "$DESKTOP\Kotodama.lnk" "$INSTDIR\%APP_NAME%.exe"
        echo.
        echo   WriteUninstaller "$INSTDIR\Uninstall.exe"
        echo   CreateShortcut "$SMPROGRAMS\Kotodama\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
        echo.
        echo   ; Register with Windows Add/Remove Programs
        echo   WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama" "DisplayName" "${PRODUCT_NAME}"
        echo   WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama" "DisplayVersion" "${PRODUCT_VERSION}"
        echo   WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama" "Publisher" "${PRODUCT_PUBLISHER}"
        echo   WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama" "InstallLocation" "$INSTDIR"
        echo   WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama" "UninstallString" "$INSTDIR\Uninstall.exe"
        echo   WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama" "DisplayIcon" "$INSTDIR\%APP_NAME%.exe"
        echo   WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama" "NoModify" 1
        echo   WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama" "NoRepair" 1
        echo SectionEnd
        echo.
        echo Section "Uninstall"
        echo   Delete "$INSTDIR\Uninstall.exe"
        echo   Delete "$SMPROGRAMS\Kotodama\*.lnk"
        echo   Delete "$DESKTOP\Kotodama.lnk"
        echo   RMDir "$SMPROGRAMS\Kotodama"
        echo   RMDir /r "$INSTDIR"
        echo.
        echo   ; Remove Windows registry entries
        echo   DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Kotodama"
        echo SectionEnd
    ) > "%NSI_FILE%"
    endlocal

    REM Compile installer
    "%MAKENSIS%" "%NSI_FILE%"
    if errorlevel 1 (
        echo ERROR: NSIS installer creation failed!
        exit /b 1
    )

    echo NSIS installer created successfully
    goto :eof

REM ============================================
REM Function: Create ZIP Package
REM ============================================
:create_zip_package
    set ZIP_NAME=%APP_NAME%-%VERSION%-windows-portable.zip
    set ZIP_PATH=%RELEASE_DIR%\%ZIP_NAME%

    REM Use PowerShell to create ZIP
    powershell -command "Compress-Archive -Path '%DEPLOY_DIR%\*' -DestinationPath '%ZIP_PATH%' -Force"

    echo ZIP package created: %ZIP_NAME%
    echo.
    echo To create a proper installer, install NSIS from:
    echo https://nsis.sourceforge.io/Download
    goto :eof
