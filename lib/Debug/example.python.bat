@echo off
setlocal

:: The executable path will be where python is located. So we set Cabonite app path through env var:
set CARB_APP_PATH=%~dp0
:: Set CARB_APP_PATH as working directory so that python bindings can find carb.dll/libcarb.so around.
pushd %CARB_APP_PATH%
:: Set PYTHONPATH to CARB_APP_PATH so that python can find packages with Carbonite bindings to import.
set PYTHONPATH=%CARB_APP_PATH%/bindings-python

call "%~dp0..\..\..\_build\target-deps\python\python.exe" %* "example.python.py"
popd
if errorlevel 1 ( goto ErrorRunningPython )

:Success
endlocal
exit /B 0

:ErrorRunningPython
echo There was an error running python.
endlocal
exit /B 1