#!/bin/bash
set -e  # Exit on error

# ============================================
# Kotodama Linux Release Build Script
# ============================================

# Configuration
APP_NAME="kotodama"
VERSION="0.1"
BUILD_DIR="build"
RELEASE_DIR="release"
QT_PATH="${HOME}/Qt/6.10.1/gcc_64"  # Adjust for your Qt installation
QMAKE="${QT_PATH}/bin/qmake"

# AppImage tools
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}Kotodama Linux Release Builder${NC}"
echo -e "${BLUE}Version: ${VERSION}${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# ============================================
# Step 1: Check/download AppImage tools
# ============================================
echo -e "${GREEN}[1/6] Checking AppImage tools...${NC}"
TOOLS_DIR="tools"
mkdir -p "${TOOLS_DIR}"

LINUXDEPLOY="${TOOLS_DIR}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${TOOLS_DIR}/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -f "${LINUXDEPLOY}" ]; then
    echo "Downloading linuxdeploy..."
    wget -O "${LINUXDEPLOY}" "${LINUXDEPLOY_URL}"
    chmod +x "${LINUXDEPLOY}"
fi

if [ ! -f "${LINUXDEPLOY_QT}" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    wget -O "${LINUXDEPLOY_QT}" "${LINUXDEPLOY_QT_URL}"
    chmod +x "${LINUXDEPLOY_QT}"
fi

echo "AppImage tools ready"

# ============================================
# Step 2: Clean and prepare
# ============================================
echo -e "${GREEN}[2/6] Cleaning previous builds...${NC}"
rm -rf "${BUILD_DIR}"
mkdir -p "${RELEASE_DIR}"

# ============================================
# Step 3: Configure CMake for Release
# ============================================
echo -e "${GREEN}[3/6] Configuring CMake (Release mode)...${NC}"
cmake -B "${BUILD_DIR}" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${QT_PATH}" \
    -DCMAKE_INSTALL_PREFIX=/usr

# ============================================
# Step 4: Build the application
# ============================================
echo -e "${GREEN}[4/6] Building application...${NC}"
cmake --build "${BUILD_DIR}" --config Release

# Verify the binary was created
if [ ! -f "${BUILD_DIR}/${APP_NAME}" ]; then
    echo -e "${YELLOW}Error: ${APP_NAME} binary not found in ${BUILD_DIR}${NC}"
    exit 1
fi

# ============================================
# Step 5: Create AppDir structure
# ============================================
echo -e "${GREEN}[5/6] Creating AppDir structure...${NC}"
APPDIR="${BUILD_DIR}/AppDir"
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

# Copy binary
cp "${BUILD_DIR}/${APP_NAME}" "${APPDIR}/usr/bin/"

# Create .desktop file
cat > "${APPDIR}/usr/share/applications/${APP_NAME}.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Kotodama
Comment=Language learning application
Exec=${APP_NAME}
Icon=${APP_NAME}
Categories=Education;Languages;
Terminal=false
EOF

# Create a simple icon (you should replace this with your actual icon)
# For now, we'll create a placeholder
cat > "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png.txt" <<EOF
Note: Replace this file with your actual 256x256 PNG icon
Named: ${APP_NAME}.png
EOF

# If you have an icon, uncomment and adjust:
# cp path/to/your/icon.png "${APPDIR}/usr/share/icons/hicolor/256x256/apps/${APP_NAME}.png"

echo "AppDir structure created"

# ============================================
# Step 6: Create AppImage
# ============================================
echo -e "${GREEN}[6/6] Creating AppImage...${NC}"

# Set Qt plugin path for linuxdeploy
export QMAKE="${QMAKE}"
export LD_LIBRARY_PATH="${QT_PATH}/lib:${LD_LIBRARY_PATH}"

# Run linuxdeploy with Qt plugin
cd "${BUILD_DIR}"
OUTPUT="../${RELEASE_DIR}/Kotodama-${VERSION}-x86_64.AppImage" \
    ../tools/linuxdeploy-x86_64.AppImage \
    --appdir AppDir \
    --plugin qt \
    --output appimage \
    --desktop-file "AppDir/usr/share/applications/${APP_NAME}.desktop"

cd ..

# ============================================
# Create tarball as alternative
# ============================================
echo -e "${GREEN}Creating tarball package...${NC}"
TARBALL_DIR="${BUILD_DIR}/${APP_NAME}-${VERSION}"
rm -rf "${TARBALL_DIR}"
mkdir -p "${TARBALL_DIR}"

# Copy binary and create simple structure
cp "${BUILD_DIR}/${APP_NAME}" "${TARBALL_DIR}/"

# Copy Qt libraries (basic bundling)
mkdir -p "${TARBALL_DIR}/lib"
ldd "${BUILD_DIR}/${APP_NAME}" | grep Qt | awk '{print $3}' | xargs -I{} cp {} "${TARBALL_DIR}/lib/" 2>/dev/null || true

# Create run script
cat > "${TARBALL_DIR}/run.sh" <<'EOF'
#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="${SCRIPT_DIR}/lib:${LD_LIBRARY_PATH}"
"${SCRIPT_DIR}/kotodama" "$@"
EOF
chmod +x "${TARBALL_DIR}/run.sh"

# Create tarball
tar -czf "${RELEASE_DIR}/${APP_NAME}-${VERSION}-linux-x86_64.tar.gz" -C "${BUILD_DIR}" "${APP_NAME}-${VERSION}"

# ============================================
# Completion
# ============================================
echo
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Build completed successfully!${NC}"
echo -e "${BLUE}========================================${NC}"
echo

# Show created files
echo -e "${GREEN}Packages created:${NC}"
for file in "${RELEASE_DIR}"/*; do
    if [ -f "$file" ]; then
        SIZE=$(du -h "$file" | cut -f1)
        echo "  - $(basename "$file") (${SIZE})"
    fi
done
echo

echo -e "${YELLOW}Next steps:${NC}"
echo "1. Add your application icon (256x256 PNG) and rebuild"
echo "2. Test AppImage: ./${RELEASE_DIR}/Kotodama-${VERSION}-x86_64.AppImage"
echo "3. Test on different Linux distributions (Ubuntu, Fedora, etc.)"
echo "4. Test tarball on systems without AppImage support"
echo

echo -e "${GREEN}Done!${NC}"
