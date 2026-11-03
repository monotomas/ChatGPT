@echo off
setlocal DisableDelayedExpansion
cd /d "%~dp0"
if not exist "%~dp0Install-Flysnapper.ps1" (
  echo This is the source directory. Use the complete Windows installation ZIP.
  echo The ready-to-install package contains Install-Flysnapper.ps1 and Flysnapper.vst3.
  pause
  exit /b 2
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0Install-Flysnapper.ps1" %*
set "FLYSNAPPER_INSTALL_RESULT=%ERRORLEVEL%"
if not "%FLYSNAPPER_INSTALL_RESULT%"=="0" (
  echo Installation did not complete. The previous installation was preserved or restored.
)
pause
exit /b %FLYSNAPPER_INSTALL_RESULT%
