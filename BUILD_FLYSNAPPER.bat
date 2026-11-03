@echo off
setlocal DisableDelayedExpansion
cd /d "%~dp0"
rem Prefer the Windows Python launcher; fall back to Python on PATH.
where py >nul 2>&1
if errorlevel 1 goto try_python
py -3 -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) and sys.maxsize > 2**32 else 1)" >nul 2>&1
if errorlevel 1 goto try_python
py -3 "%~dp0build_plugin.py" %*
set "FLYSNAPPER_RESULT=%ERRORLEVEL%"
goto finished

:try_python
where python >nul 2>&1
if errorlevel 1 goto missing_python
python -c "import sys; sys.exit(0 if sys.version_info >= (3, 10) and sys.maxsize > 2**32 else 1)" >nul 2>&1
if errorlevel 1 goto missing_python
python "%~dp0build_plugin.py" %*
set "FLYSNAPPER_RESULT=%ERRORLEVEL%"
goto finished

:missing_python
echo.
echo Flysnapper requires Python 3.10 or newer, 64-bit. Python 3.12 is recommended.
echo Install it from python.org with the Python launcher or PATH option enabled.
echo Then close this window and run BUILD_FLYSNAPPER.bat again.
echo No tools or plug-ins have been installed by this script.
pause
exit /b 2

:finished
if not "%FLYSNAPPER_RESULT%"=="0" (
  echo Build or tests failed. No untested plug-in was installed. See logs.
) else (
  echo Build and tests complete. Installation requires --install or --install-only.
)
echo.
pause
exit /b %FLYSNAPPER_RESULT%
