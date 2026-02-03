# Building Windows Executable (.exe)

This guide shows how to create a standalone `MediaStreamClient.exe` that can run on any Windows 10 machine without requiring Qt installation.

## 🎯 Goal

Create a single-folder distribution with:
- `MediaStreamClient.exe` - Main executable
- Required Qt DLLs
- All dependencies bundled

## 📋 Prerequisites

- Windows 10
- Visual Studio 2019 or 2022 (Community Edition is free)
- CMake 3.10+
- Qt 5.15+ or vcpkg

## 🔧 Method 1: Using Qt Installer (Recommended)

### Step 1: Install Qt

1. Download Qt installer: https://www.qt.io/download-qt-installer
2. Install Qt 5.15.2 (or latest 5.x)
3. Select components:
   - MSVC 2019 64-bit
   - Qt Multimedia
   - Qt Network

Default install path: `C:\Qt\5.15.2\msvc2019_64`

### Step 2: Build Release Version

```powershell
# Open "x64 Native Tools Command Prompt for VS 2019"
cd C:\Users\YourName\mediaStream\client
mkdir build-release
cd build-release

# Configure for Release
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64

# Build
cmake --build . --config Release
```

### Step 3: Deploy with windeployqt

Qt provides `windeployqt` tool that automatically copies all required DLLs:

```powershell
# Navigate to build directory
cd build-release\Release

# Run windeployqt
C:\Qt\5.15.2\msvc2019_64\bin\windeployqt.exe MediaStreamClient.exe

# This will copy all Qt DLLs and plugins to the current directory
```

### Step 4: Create Distribution Folder

```powershell
# Create distribution folder
mkdir C:\MediaStreamClient-Distribution
cd C:\MediaStreamClient-Distribution

# Copy everything from Release folder
xcopy /E /I C:\Users\YourName\mediaStream\client\build-release\Release\* .

# Your folder now contains:
# - MediaStreamClient.exe
# - Qt5Core.dll, Qt5Gui.dll, Qt5Widgets.dll, Qt5Network.dll, Qt5Multimedia.dll
# - platforms\qwindows.dll
# - Other required DLLs
```

### Step 5: Test on Clean Machine

Copy the entire `MediaStreamClient-Distribution` folder to another Windows 10 PC and run `MediaStreamClient.exe`. It should work without Qt installed!

## 🔧 Method 2: Using vcpkg (Static Linking)

For a truly standalone .exe with no DLL dependencies:

### Step 1: Install vcpkg

```powershell
cd C:\
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

### Step 2: Install Qt Static

```powershell
# This takes a while (1-2 hours)
.\vcpkg install qt5-base:x64-windows-static qt5-multimedia:x64-windows-static qt5-network:x64-windows-static
```

### Step 3: Build Static Executable

```powershell
cd C:\Users\YourName\mediaStream\client
mkdir build-static
cd build-static

cmake .. -G "Visual Studio 16 2019" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DCMAKE_BUILD_TYPE=Release

cmake --build . --config Release
```

Result: Single `MediaStreamClient.exe` with no DLL dependencies (larger file ~15-20 MB)

## 🔧 Method 3: Using NSIS Installer (Professional)

Create a proper Windows installer:

### Step 1: Install NSIS

Download from: https://nsis.sourceforge.io/Download

### Step 2: Create Installer Script

Create `client/installer.nsi`:

```nsis
; MediaStream Client Installer
!define APP_NAME "MediaStream Client"
!define APP_VERSION "1.0.0"
!define APP_PUBLISHER "Your Name"
!define APP_EXE "MediaStreamClient.exe"

Name "${APP_NAME}"
OutFile "MediaStreamClient-Setup.exe"
InstallDir "$PROGRAMFILES64\${APP_NAME}"

Page directory
Page instfiles

Section "Install"
    SetOutPath "$INSTDIR"
    
    ; Copy all files
    File /r "build-release\Release\*.*"
    
    ; Create shortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    CreateShortcut "$DESKTOP\${APP_NAME}.lnk" "$INSTDIR\${APP_EXE}"
    
    ; Create uninstaller
    WriteUninstaller "$INSTDIR\Uninstall.exe"
    
    ; Add to Add/Remove Programs
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" "$INSTDIR\Uninstall.exe"
SectionEnd

Section "Uninstall"
    Delete "$INSTDIR\*.*"
    RMDir /r "$INSTDIR"
    Delete "$SMPROGRAMS\${APP_NAME}\*.*"
    RMDir "$SMPROGRAMS\${APP_NAME}"
    Delete "$DESKTOP\${APP_NAME}.lnk"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
SectionEnd
```

### Step 3: Build Installer

```powershell
cd C:\Users\YourName\mediaStream\client
"C:\Program Files (x86)\NSIS\makensis.exe" installer.nsi
```

Result: `MediaStreamClient-Setup.exe` installer

## 📦 Recommended Distribution Structure

```
MediaStreamClient/
├── MediaStreamClient.exe          # Main executable
├── Qt5Core.dll                    # Qt core library
├── Qt5Gui.dll                     # Qt GUI library
├── Qt5Widgets.dll                 # Qt widgets
├── Qt5Network.dll                 # Qt network (for HTTP)
├── Qt5Multimedia.dll              # Qt multimedia (for video)
├── platforms/
│   └── qwindows.dll              # Windows platform plugin
├── mediaservice/
│   └── dsengine.dll              # DirectShow plugin
├── README.txt                     # Usage instructions
└── config.ini                     # Configuration file (optional)
```

## 🎨 Adding Application Icon

### Step 1: Create Icon

Create `client/resources/app.ico` (256x256 PNG converted to ICO)

### Step 2: Create Resource File

Create `client/resources/app.rc`:

```rc
IDI_ICON1 ICON DISCARDABLE "app.ico"
```

### Step 3: Update CMakeLists.txt

```cmake
# Add to client/CMakeLists.txt
if(WIN32)
    set(APP_ICON_RESOURCE_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    add_executable(client WIN32 
        main.cpp 
        mainwindow.cpp 
        mainwindow.h
        ${APP_ICON_RESOURCE_WINDOWS}
    )
else()
    add_executable(client 
        main.cpp 
        mainwindow.cpp 
        mainwindow.h
    )
endif()
```

## 📝 Configuration File

Create `config.ini` for easy server configuration:

```ini
[Server]
host=192.168.1.37
port=8000
use_https=false

[UI]
theme=dark
language=en
```

Update client to read this file:

```cpp
// In mainwindow.cpp
#include <QSettings>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    QSettings settings("config.ini", QSettings::IniFormat);
    serverIp = settings.value("Server/host", "192.168.1.37").toString();
    serverPort = settings.value("Server/port", 8000).toInt();
}
```

## 🚀 Quick Build Script

Create `client/build-windows.bat`:

```batch
@echo off
echo Building MediaStream Client for Windows...

REM Set Qt path
set QT_PATH=C:\Qt\5.15.2\msvc2019_64

REM Clean previous build
if exist build-release rmdir /s /q build-release
mkdir build-release
cd build-release

REM Configure
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT_PATH%

REM Build
cmake --build . --config Release

REM Deploy Qt dependencies
cd Release
%QT_PATH%\bin\windeployqt.exe MediaStreamClient.exe

REM Create distribution folder
cd ..\..
if exist dist rmdir /s /q dist
mkdir dist
xcopy /E /I build-release\Release\* dist\

echo.
echo Build complete! Distribution in 'dist' folder
echo Run: dist\MediaStreamClient.exe
pause
```

Usage:

```powershell
cd C:\Users\YourName\mediaStream\client
.\build-windows.bat
```

## 🧪 Testing the Executable

### Test 1: On Build Machine

```powershell
cd dist
.\MediaStreamClient.exe
```

### Test 2: On Clean Windows 10 VM

1. Create Windows 10 VM (VirtualBox/VMware)
2. Copy `dist` folder to VM
3. Run `MediaStreamClient.exe`
4. Should work without installing anything!

### Test 3: Dependency Check

Use Dependency Walker to verify all DLLs are included:

1. Download: http://www.dependencywalker.com/
2. Open `MediaStreamClient.exe`
3. Check for missing DLLs (should be none)

## 📊 File Sizes

Typical distribution sizes:

| Method | Size | Pros | Cons |
|--------|------|------|------|
| Dynamic (Qt DLLs) | ~50 MB | Smaller, faster build | Multiple files |
| Static | ~15-20 MB | Single .exe | Larger file, slow build |
| Installer | ~50 MB | Professional | Extra step |

## 🔒 Code Signing (Optional)

For professional distribution:

1. Get code signing certificate
2. Sign executable:

```powershell
signtool sign /f certificate.pfx /p password /t http://timestamp.digicert.com MediaStreamClient.exe
```

## ✅ Final Checklist

- [ ] Build in Release mode (not Debug)
- [ ] Run windeployqt to copy Qt DLLs
- [ ] Test on clean Windows 10 machine
- [ ] Include config.ini for easy server setup
- [ ] Add application icon
- [ ] Create README.txt with instructions
- [ ] Test all features (connect, browse, stream)
- [ ] Check for missing DLLs with Dependency Walker

## 🎯 Distribution Options

### Option 1: ZIP File
```powershell
# Create distributable ZIP
Compress-Archive -Path dist\* -DestinationPath MediaStreamClient-v1.0.0-Windows.zip
```

### Option 2: Installer
Use NSIS script above to create `MediaStreamClient-Setup.exe`

### Option 3: Portable
Just copy the `dist` folder - no installation needed!

## 📱 Future: Auto-Update

Consider implementing auto-update:

1. Check for updates on startup
2. Download new version from server
3. Replace executable and restart

Libraries: Qt Installer Framework, WinSparkle

---

**Result**: You'll have a professional Windows executable that users can run with a double-click, no Qt installation required!