# Kotodama Release Guide

This guide explains how to build installers for macOS, Windows, and Linux.

## Prerequisites by Platform

### macOS
- Qt 6.10.1 (or compatible)
- CMake 3.16+
- Xcode Command Line Tools
- (Optional) Apple Developer Account for code signing

### Windows
- Qt 6.10.1 MSVC build
- CMake 3.16+
- Visual Studio 2022 (or 2019)
- (Optional) NSIS for installer creation: https://nsis.sourceforge.io/Download

### Linux
- Qt 6.10.1 GCC build
- CMake 3.16+
- GCC/G++
- wget (for downloading AppImage tools)

## Quick Start

### macOS
```bash
./release_macos.sh
```
**Output:** `release/kotodama-0.1-macos.dmg`

### Windows
```cmd
release_windows.bat
```
**Output:**
- `release/kotodama-0.1-windows-installer.exe` (with NSIS)
- `release/kotodama-0.1-windows-portable.zip` (without NSIS)

### Linux
```bash
./release_linux.sh
```
**Output:**
- `release/Kotodama-0.1-x86_64.AppImage` (universal)
- `release/kotodama-0.1-linux-x86_64.tar.gz` (tarball)

## Configuration

### Update Version Number

Edit the version in each script:

**release_macos.sh:**
```bash
VERSION="0.1"
```

**release_windows.bat:**
```bat
set VERSION=0.1
```

**release_linux.sh:**
```bash
VERSION="0.1"
```

### Qt Installation Paths

The release scripts will auto-detect Qt if `qmake` is in your PATH. Alternatively, you can set the `QT_PATH` environment variable:

**Using environment variable:**
```bash
# macOS/Linux
QT_PATH=/path/to/Qt/6.10.1/macos ./release_macos.sh

# Windows
set QT_PATH=C:\Qt\6.10.1\msvc2022_64
release_windows.bat
```

**Or edit the paths directly in the scripts:**

**release_macos.sh:**
```bash
QT_PATH="/path/to/Qt/6.x.y/macos"
```

**release_windows.bat:**
```bat
set QT_PATH=C:\Qt\6.x.y\msvc2022_64
```

**release_linux.sh:**
```bash
QT_PATH="/path/to/Qt/6.x.y/gcc_64"
```

## Code Signing (Recommended for Distribution)

### macOS Code Signing

1. Get an Apple Developer account ($99/year)
2. Create a "Developer ID Application" certificate
3. Update `release_macos.sh`:
```bash
SIGNING_IDENTITY="Developer ID Application: Your Name (TEAM_ID)"
```

### Windows Code Signing

1. Purchase a code signing certificate from a CA (e.g., DigiCert, Sectigo)
2. After NSIS installer creation, sign it:
```cmd
signtool sign /f your-certificate.pfx /p password /t http://timestamp.digicert.com release/kotodama-0.1-windows-installer.exe
```

### Linux

Linux doesn't require code signing for typical distribution.

## Distribution Formats Explained

### macOS DMG
- **Format:** Disk image file
- **User Experience:** Download → Open DMG → Drag app to Applications
- **Compatibility:** macOS 10.13+

### Windows Installer (NSIS)
- **Format:** Executable installer
- **User Experience:** Download → Run installer → Follow wizard
- **Features:** Start menu shortcuts, desktop icon, uninstaller
- **Compatibility:** Windows 10+

### Windows Portable (ZIP)
- **Format:** ZIP archive
- **User Experience:** Download → Extract → Run exe
- **Features:** No installation, can run from USB
- **Compatibility:** Windows 10+

### Linux AppImage
- **Format:** Self-contained executable
- **User Experience:** Download → Make executable → Run
- **Features:** Universal, works on most distros, no installation
- **Compatibility:** Most Linux distributions with glibc 2.27+

### Linux Tarball
- **Format:** Compressed archive
- **User Experience:** Download → Extract → Run via script
- **Features:** Traditional format, includes bundled libraries
- **Compatibility:** Most Linux distributions

## Testing Checklist

### Before Release
- [ ] Update version numbers in all scripts
- [ ] Update version in CMakeLists.txt if needed
- [ ] Test on clean systems (VM or real hardware)
- [ ] Verify all dependencies are bundled
- [ ] Check file sizes are reasonable
- [ ] Test basic functionality after installation

### macOS Testing
- [ ] Test on Intel Mac
- [ ] Test on Apple Silicon Mac
- [ ] Verify app opens without "unidentified developer" warning (if signed)
- [ ] Check app permissions (if accessing files, etc.)

### Windows Testing
- [ ] Test on Windows 10
- [ ] Test on Windows 11
- [ ] Verify installer creates shortcuts
- [ ] Test uninstaller
- [ ] Check antivirus doesn't flag (if signed)

### Linux Testing
- [ ] Test on Ubuntu/Debian-based
- [ ] Test on Fedora/RHEL-based
- [ ] Test on Arch-based
- [ ] Verify AppImage runs without installation
- [ ] Test tarball on systems without AppImage support

## File Sizes

Typical sizes (approximate):
- **macOS DMG:** 20-40 MB
- **Windows Installer:** 25-45 MB
- **Windows ZIP:** 25-45 MB
- **Linux AppImage:** 30-50 MB
- **Linux Tarball:** 20-40 MB

*Sizes vary based on Qt modules used and compression*

## Troubleshooting

### macOS: "App is damaged and can't be opened"
- **Cause:** Gatekeeper quarantine on unsigned apps
- **Solution:** Sign with Developer ID or users must: `xattr -cr /path/to/Kotodama.app`

### Windows: "Windows protected your PC"
- **Cause:** SmartScreen on unsigned executables
- **Solution:** Code sign the installer or users must click "More info" → "Run anyway"

### Linux: AppImage won't run
- **Cause:** Missing FUSE or execute permissions
- **Solutions:**
  - Install FUSE: `sudo apt install fuse` (Ubuntu)
  - Make executable: `chmod +x Kotodama-*.AppImage`
  - Use `--appimage-extract-and-run` flag

### Qt Libraries Missing
- **Cause:** Deployment tool didn't bundle everything
- **macOS Solution:** Check `macdeployqt` output, manually copy missing frameworks
- **Windows Solution:** Check `windeployqt` output, manually copy missing DLLs
- **Linux Solution:** Check `ldd` output, update LD_LIBRARY_PATH or copy libs

## Distribution Platforms

### Recommended Platforms
- **GitHub Releases** (free, easy, cross-platform)
- **itch.io** (indie-friendly, easy distribution)
- **Your own website** (full control)

### Platform-Specific Stores
- **macOS:** Mac App Store (requires $99/year Apple Developer account)
- **Windows:** Microsoft Store (requires developer account)
- **Linux:** Flathub (Flatpak), Snapcraft (Snap), AUR (Arch)

## Next Steps

1. Build for your platform
2. Test thoroughly on clean systems
3. Consider code signing for better user experience
4. Upload to distribution platform
5. Write release notes
6. Announce to users!

## Support

For build issues:
- Check Qt installation paths
- Verify CMake version
- Check build logs in `build/` directory
- Ensure all prerequisites are installed
