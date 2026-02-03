@echo off
echo ========================================
echo MediaStream Client - Windows Builder
echo ========================================
echo.

REM Configuration
set QT_PATH=C:\Qt\5.15.2\msvc2019_64
set BUILD_DIR=build-release
set DIST_DIR=dist

REM Check if Qt exists
if not exist "%QT_PATH%" (
    echo ERROR: Qt not found at %QT_PATH%
    echo Please install Qt or update QT_PATH in this script
    pause
    exit /b 1
)

echo [1/5] Cleaning previous build...
if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
if exist %DIST_DIR% rmdir /s /q %DIST_DIR%
mkdir %BUILD_DIR%

echo [2/5] Configuring CMake...
cd %BUILD_DIR%
cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QT_PATH%
if errorlevel 1 (
    echo ERROR: CMake configuration failed
    cd ..
    pause
    exit /b 1
)

echo [3/5] Building Release version...
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed
    cd ..
    pause
    exit /b 1
)

echo [4/5] Deploying Qt dependencies...
cd Release
"%QT_PATH%\bin\windeployqt.exe" client.exe --release --no-translations
if errorlevel 1 (
    echo WARNING: windeployqt had issues, but continuing...
)

echo [5/5] Creating distribution folder...
cd ..\..
mkdir %DIST_DIR%
xcopy /E /I /Y %BUILD_DIR%\Release\* %DIST_DIR%\

REM Create config file
echo [Server] > %DIST_DIR%\config.ini
echo host=192.168.1.37 >> %DIST_DIR%\config.ini
echo port=8000 >> %DIST_DIR%\config.ini
echo. >> %DIST_DIR%\config.ini
echo [UI] >> %DIST_DIR%\config.ini
echo theme=dark >> %DIST_DIR%\config.ini

REM Create README
echo MediaStream Client > %DIST_DIR%\README.txt
echo. >> %DIST_DIR%\README.txt
echo 1. Edit config.ini to set your server IP address >> %DIST_DIR%\README.txt
echo 2. Run client.exe >> %DIST_DIR%\README.txt
echo 3. Enjoy streaming! >> %DIST_DIR%\README.txt

echo.
echo ========================================
echo Build Complete!
echo ========================================
echo.
echo Distribution folder: %DIST_DIR%
echo Executable: %DIST_DIR%\client.exe
echo.
echo To test: cd %DIST_DIR% ^&^& client.exe
echo.
pause

@REM Made with Bob
