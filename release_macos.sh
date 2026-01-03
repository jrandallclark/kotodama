#!/bin/bash
set -e  # Exit on error

# ============================================
# Kotodama macOS Release Build Script
# ============================================

# Configuration
APP_NAME="kotodama"
VERSION="0.1"
BUILD_DIR="build"
RELEASE_DIR="release"

# Auto-detect Qt or use QT_PATH environment variable
if [ -n "$QT_PATH" ]; then
    echo "Using Qt from QT_PATH environment variable: $QT_PATH"
elif command -v qmake &> /dev/null; then
    QT_PATH=$(qmake -query QT_INSTALL_PREFIX)
    echo "Auto-detected Qt: $QT_PATH"
else
    echo "Error: Qt not found. Please install Qt or set QT_PATH environment variable."
    echo "Example: QT_PATH=/path/to/Qt/6.x.y/macos ./release_macos.sh"
    exit 1
fi

MACDEPLOYQT="${QT_PATH}/bin/macdeployqt"

# Code signing (leave empty if not signing)
SIGNING_IDENTITY=""  # e.g., "Developer ID Application: Your Name (TEAM_ID)"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Kotodama macOS Release Builder${NC}"
echo -e "${BLUE}Version: ${VERSION}${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# ============================================
# Step 1: Clean and prepare
# ============================================
echo -e "${GREEN}[1/5] Cleaning previous builds...${NC}"
rm -rf "${BUILD_DIR}"
mkdir -p "${RELEASE_DIR}"

# ============================================
# Step 2: Configure CMake for Release
# ============================================
echo -e "${GREEN}[2/5] Configuring CMake (Release mode)...${NC}"
cmake -B "${BUILD_DIR}" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${QT_PATH}"

# ============================================
# Step 3: Build the application
# ============================================
echo -e "${GREEN}[3/5] Building application...${NC}"
cmake --build "${BUILD_DIR}" --config Release

# Verify the .app was created
if [ ! -d "${BUILD_DIR}/${APP_NAME}.app" ]; then
    echo -e "${YELLOW}Error: ${APP_NAME}.app not found in ${BUILD_DIR}${NC}"
    exit 1
fi

# ============================================
# Step 4: Bundle Qt frameworks
# ============================================
echo -e "${GREEN}[4/5] Bundling Qt frameworks with macdeployqt...${NC}"
"${MACDEPLOYQT}" "${BUILD_DIR}/${APP_NAME}.app" -verbose=2

# Code signing (if configured)
if [ -n "$SIGNING_IDENTITY" ]; then
    echo -e "${GREEN}Code signing with identity: ${SIGNING_IDENTITY}${NC}"
    codesign --force --deep --sign "$SIGNING_IDENTITY" "${BUILD_DIR}/${APP_NAME}.app"

    # Verify signature
    codesign --verify --verbose=2 "${BUILD_DIR}/${APP_NAME}.app"
    echo -e "${GREEN}Code signing verified${NC}"
fi

# ============================================
# Step 5: Create DMG installer
# ============================================
echo -e "${GREEN}[5/5] Creating DMG installer...${NC}"
DMG_NAME="${APP_NAME}-${VERSION}-macos.dmg"
DMG_PATH="${RELEASE_DIR}/${DMG_NAME}"

# Remove old DMG if exists
rm -f "${DMG_PATH}"

# Create DMG
hdiutil create -volname "Kotodama ${VERSION}" \
    -srcfolder "${BUILD_DIR}/${APP_NAME}.app" \
    -ov \
    -format UDZO \
    "${DMG_PATH}"

# ============================================
# Completion
# ============================================
echo
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${BLUE}========================================${NC}"
echo
echo -e "Installer created: ${BLUE}${DMG_PATH}${NC}"

# Show file size
DMG_SIZE=$(du -h "${DMG_PATH}" | cut -f1)
echo -e "File size: ${DMG_SIZE}"
echo

# Next steps
echo -e "${YELLOW}Next steps:${NC}"
echo "1. Test the installer: open ${DMG_PATH}"
echo "2. Test installation on a clean macOS system"
if [ -z "$SIGNING_IDENTITY" ]; then
    echo "3. Consider code signing for distribution (set SIGNING_IDENTITY in script)"
fi
echo

echo -e "${GREEN}Done!${NC}"
