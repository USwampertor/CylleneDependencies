@echo off
setlocal

:: Setup python env from generated file (generated by tools/repoman/build.py)
if exist "%~dp0setup_python_env.bat" (
    call "%~dp0setup_python_env.bat"
)

:: The executable path will be where python is located. So we set Cabonite app path through env var:
set CARB_APP_PATH=%~dp0

:: By default use our python, but allow overriding it by checking if PYTHONEXE env var is defined:
IF "%PYTHONEXE%"=="" (
    IF exist "%~dp0python\python.exe" (
        set PYTHONEXE="%~dp0python\python.exe"
        goto Found
    )
    IF exist "%~dp0..\..\target-deps\python-3.10\python.exe" (
        set PYTHONEXE="%~dp0..\..\target-deps\python-3.10\python.exe"
        goto Found
    )
    IF exist "%~dp0..\..\target-deps\python-3.9\python.exe" (
        set PYTHONEXE="%~dp0..\..\target-deps\python-3.9\python.exe"
        goto Found
    )
    IF exist "%~dp0..\..\target-deps\python-3.8\python.exe" (
        set PYTHONEXE="%~dp0..\..\target-deps\python-3.8\python.exe"
        goto Found
    )
    IF exist "%~dp0..\..\target-deps\python-3.7\python.exe" (
        set PYTHONEXE="%~dp0..\..\target-deps\python-3.7\python.exe"
        goto Found
    )

    echo Python not found
    goto ErrorRunningPython
)

:Found
call %PYTHONEXE% %*

if errorlevel 1 ( goto ErrorRunningPython )

:Success
endlocal
exit /B 0

:ErrorRunningPython
echo There was an error running python.
endlocal
exit /B 1
