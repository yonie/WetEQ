#!/bin/bash
set -e

echo "================================================"
echo " Merging platform binaries into universal VST3"
echo "================================================"
echo ""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SCRIPT_DIR"

# Create universal bundle structure
echo "Creating universal bundle structure..."
mkdir -p build/WetEQ.vst3/Contents/Resources
mkdir -p build/WetEQ.vst3/Contents/x86_64-win
mkdir -p build/WetEQ.vst3/Contents/x86_64-linux
mkdir -p build/WetEQ.vst3/Contents/MacOS

# Copy resources from macOS build
echo "Copying shared resources..."
if [ -d "artifacts/macos/Resources" ]; then
    cp -r artifacts/macos/Resources/* build/WetEQ.vst3/Contents/Resources/
elif [ -d "artifacts/macos/Contents/Resources" ]; then
    cp -r artifacts/macos/Contents/Resources/* build/WetEQ.vst3/Contents/Resources/
fi

# Copy Windows binary
# Note: Windows binary in cross-platform bundle MUST have .vst3 extension (not .dll)
echo "Copying Windows binary..."
WINDOWS_FOUND=false
if [ -f "artifacts/windows/Contents/x86_64-win/WetEQ.vst3" ]; then
    cp artifacts/windows/Contents/x86_64-win/WetEQ.vst3 build/WetEQ.vst3/Contents/x86_64-win/
    echo "  Found at artifacts/windows/Contents/x86_64-win/WetEQ.vst3"
    WINDOWS_FOUND=true
elif [ -f "artifacts/windows/VST3/Release/WetEQ.vst3/Contents/x86_64-win/WetEQ.vst3" ]; then
    cp artifacts/windows/VST3/Release/WetEQ.vst3/Contents/x86_64-win/WetEQ.vst3 build/WetEQ.vst3/Contents/x86_64-win/
    echo "  Found at artifacts/windows/VST3/Release/WetEQ.vst3/Contents/x86_64-win/WetEQ.vst3"
    WINDOWS_FOUND=true
elif [ -f "artifacts/windows/Contents/x86_64-win/WetEQ.dll" ]; then
    # Legacy: rename .dll to .vst3 for cross-platform bundle
    cp artifacts/windows/Contents/x86_64-win/WetEQ.dll build/WetEQ.vst3/Contents/x86_64-win/WetEQ.vst3
    echo "  Found at artifacts/windows/Contents/x86_64-win/WetEQ.dll (renamed to .vst3)"
    WINDOWS_FOUND=true
elif [ -f "artifacts/windows/VST3/Release/WetEQ.vst3/Contents/x86_64-win/WetEQ.dll" ]; then
    # Legacy: rename .dll to .vst3 for cross-platform bundle
    cp artifacts/windows/VST3/Release/WetEQ.vst3/Contents/x86_64-win/WetEQ.dll build/WetEQ.vst3/Contents/x86_64-win/WetEQ.vst3
    echo "  Found at artifacts/windows/VST3/Release/WetEQ.vst3/Contents/x86_64-win/WetEQ.dll (renamed to .vst3)"
    WINDOWS_FOUND=true
fi
if [ "$WINDOWS_FOUND" = false ]; then
    echo "WARNING: Windows binary not found!"
    echo "Looking for Windows binary..."
    find artifacts/windows -name "*.dll" -o -name "WetEQ.vst3" 2>/dev/null | head -5 || echo "No Windows binary found"
fi

# Copy Linux binary
echo "Copying Linux binary..."
LINUX_FOUND=false
if [ -f "artifacts/linux/Contents/x86_64-linux/WetEQ.so" ]; then
    cp artifacts/linux/Contents/x86_64-linux/WetEQ.so build/WetEQ.vst3/Contents/x86_64-linux/
    echo "  Found at artifacts/linux/Contents/x86_64-linux/WetEQ.so"
    LINUX_FOUND=true
elif [ -f "artifacts/linux/VST3/Release/WetEQ.vst3/Contents/x86_64-linux/WetEQ.so" ]; then
    cp artifacts/linux/VST3/Release/WetEQ.vst3/Contents/x86_64-linux/WetEQ.so build/WetEQ.vst3/Contents/x86_64-linux/
    echo "  Found at artifacts/linux/VST3/Release/WetEQ.vst3/Contents/x86_64-linux/WetEQ.so"
    LINUX_FOUND=true
fi
if [ "$LINUX_FOUND" = false ]; then
    echo "WARNING: Linux binary not found!"
    find artifacts/linux -name "*.so" 2>/dev/null | head -5 || echo "No SO files found"
fi

# Copy macOS universal binary
echo "Copying macOS universal binary..."
MACOS_FOUND=false
if [ -f "artifacts/macos/MacOS/WetEQ" ]; then
    cp artifacts/macos/MacOS/WetEQ build/WetEQ.vst3/Contents/MacOS/
    echo "  Found at artifacts/macos/MacOS/WetEQ"
    MACOS_FOUND=true
elif [ -f "artifacts/macos/Contents/MacOS/WetEQ" ]; then
    cp artifacts/macos/Contents/MacOS/WetEQ build/WetEQ.vst3/Contents/MacOS/
    echo "  Found at artifacts/macos/Contents/MacOS/WetEQ"
    MACOS_FOUND=true
fi
if [ "$MACOS_FOUND" = false ]; then
    echo "WARNING: macOS universal binary not found!"
    find artifacts/macos -type f -name "WetEQ" 2>/dev/null | head -5 || echo "No WetEQ executable found"
fi

# Copy macOS bundle metadata (Info.plist, PkgInfo) for codesign compatibility
echo "Copying macOS bundle metadata..."
MACOS_PLIST_FOUND=false
if [ -f "artifacts/macos/Info.plist" ]; then
    cp artifacts/macos/Info.plist build/WetEQ.vst3/Contents/
    [ -f "artifacts/macos/PkgInfo" ] && cp artifacts/macos/PkgInfo build/WetEQ.vst3/Contents/
    echo "  Found at artifacts/macos/"
    MACOS_PLIST_FOUND=true
elif [ -f "artifacts/macos/Contents/Info.plist" ]; then
    cp artifacts/macos/Contents/Info.plist build/WetEQ.vst3/Contents/
    [ -f "artifacts/macos/Contents/PkgInfo" ] && cp artifacts/macos/Contents/PkgInfo build/WetEQ.vst3/Contents/
    echo "  Found at artifacts/macos/Contents/"
    MACOS_PLIST_FOUND=true
fi
if [ "$MACOS_PLIST_FOUND" = false ]; then
    echo "WARNING: macOS Info.plist not found - codesign may fail!"
fi

echo ""
echo "================================================"
echo " Universal VST3 bundle created!"
echo "================================================"
echo ""
echo "Bundle location: build/WetEQ.vst3"
echo ""
echo "Bundle structure:"
find build/WetEQ.vst3 -type f | head -20
echo ""
echo "Bundle size: $(du -sh build/WetEQ.vst3 | cut -f1)"
echo ""