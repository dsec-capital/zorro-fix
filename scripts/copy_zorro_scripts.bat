@ECHO OFF

REM Copy files to Zorro installation

ECHO Install scripts to Zorro installation %ZorroInstallDir%

IF "%ZorroInstallDir%" == "" (
  GOTO env_error
)

SET SCRIPT_DIR=%~dp0
SET ZORRO_STRATEGY_DIR="%ZorroInstallDir%\Strategy"
SET ZORRO_HISTORY_DIR="%ZorroInstallDir%\History"
SET SOURCE="%SCRIPT_DIR%..\zorro_scripts"

ECHO Copy strategy files to %ZORRO_STRATEGY_DIR%...
robocopy %SOURCE% %ZORRO_STRATEGY_DIR% "*.c" > nul
robocopy %SOURCE% %ZORRO_STRATEGY_DIR% "*.h" > nul

ECHO Copy asset files to %ZORRO_HISTORY_DIR%...
robocopy %SOURCE% %ZORRO_HISTORY_DIR% "*.csv" > nul

ECHO Intallation completed
GOTO :eof

:env_error
ECHO Set environment variable %ZorroInstallDir% first 
EXIT /B 1
