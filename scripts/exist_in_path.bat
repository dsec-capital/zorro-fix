@echo off

rem Prompt the user for the DLL name
set DLL_NAME=%1

rem Check if the DLL can be found in the PATH
where %DLL_NAME% >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo The DLL '%DLL_NAME%' was found in the PATH.
) else (
    echo The DLL '%DLL_NAME%' was not found in the PATH.
)
