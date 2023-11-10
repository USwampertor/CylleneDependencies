@echo off
setlocal

set EXE="%~dp0\example.crashreporter.exe"
set DMP_WILDCARD=%~dp0\*.dmp
set UNIQUE_ID=%TIME%
set UNIQUE_ID_IMMEDIATE=%UNIQUE_ID%-immediate
set UNIQUE_ID_DELAYED=%UNIQUE_ID%-error-test-failure
set UNIQUE_ID_IMMEDIATE2=%UNIQUE_ID%-delayed-and-immediate
set URL_PRE=https://internal.backtrace.nvidia.com/p/OmniverseKit-Test/list?filters=((test-id%%2Ccontains%%2c%%22
SET URL_POST=%%22))^^^&select=(timestamp%%2Cfingerprint%%2Ccallstack%%2Ctest-id)^^^&sort=(timestamp%%2Cselect%%2Cdesc)

echo Looking for existing .dmp files in: %DMP_WILDCARD%
for %%f in ("%DMP_WILDCARD%") do (
    echo error: found existing .dmp file: %%f
    echo error: remove the .dmp file and rerun to continue
    exit /b 1
)

echo Generating and uploading .dmp...
set CMD=%EXE% "--/crashreporter/data/test-id=%UNIQUE_ID_IMMEDIATE%"
echo    %CMD%
%CMD%

echo Generating .dmp without upload...
set CMD=%EXE% --/crashreporter/data/test-id=%UNIQUE_ID_DELAYED% --/crashreporter/url=
echo    %CMD%
%CMD%

echo Generating and uploading .dmp (with previous dumps)...
set CMD=%EXE% "--/crashreporter/data/test-id=%UNIQUE_ID_IMMEDIATE2%"
echo    %CMD%
%CMD%

echo.
echo There should be:
echo.
echo   2 .dmps whose test-id matches: %UNIQUE_ID_IMMEDIATE2%
echo   2 .dmp.toml files whose test-id mathces: %UNIQUE_ID_IMMEDIATE2%
echo   1 .dmp  whose test-id matches: %UNIQUE_ID_IMMEDIATE%
echo   1 .dmp.toml file whose test-id matches: %UNIQUE_ID_IMMEDIATE%
echo.
echo at the following URL:
echo.
echo   %URL_PRE%%UNIQUE_ID%%URL_POST%
