ECHO OFF

REM Copy files to Zorro installation

ECHO Install scripts to Zorro directory %ZorroInstallDir%

IF "%ZorroInstallDir%" == "" (
  GOTO env_error
)

SET SCRIPT_DIR=%~dp0
SET ZORRO_STRATEGY_DIR="%ZorroInstallDir%\Strategy"
SET ZORRO_HISTORY_DIR="%ZorroInstallDir%\History"
SET SOURCE="%SCRIPT_DIR%..\zorro_scripts"
SET PLUGIN_SOURCE="%SCRIPT_DIR%..\zorro_fxcm_fix_plugin"

ECHO Copy strategy files to %ZORRO_STRATEGY_DIR%...
robocopy %SOURCE% %ZORRO_STRATEGY_DIR% "*.c" > nul
robocopy %SOURCE% %ZORRO_STRATEGY_DIR% "*.cpp" > nul
robocopy %SOURCE% %ZORRO_STRATEGY_DIR% "*.h" > nul
robocopy %PLUGIN_SOURCE% %ZORRO_STRATEGY_DIR% "zorro_fxcm_fix_include.h" > nul

ECHO Copy asset files to %ZORRO_HISTORY_DIR%...
robocopy %SOURCE% %ZORRO_HISTORY_DIR% "*.csv" > nul

ECHO Copy FIX spec files to %ZorroInstallDir%\Plugin\spec...
robocopy "%SCRIPT_DIR%..\spec" "%ZorroInstallDir%\Plugin\spec"

ECHO Copy plugin config toml file
robocopy %PLUGIN_SOURCE% "%ZorroInstallDir%\Plugin" "*.toml" > nul

ECHO Creating FIX session configuration file with username and password from environment settings
CALL "%SCRIPT_DIR%..\scripts\generate_fxcm_fix_client_cfg.bat" "%SCRIPT_DIR%..\zorro_fxcm_fix_plugin\zorro_fxcm_fix_client_template.cfg" "%ZorroInstallDir%\Plugin\zorro_fxcm_fix_client.cfg"

CALL %SCRIPT_DIR%..\scripts\check_fxcm_dlls.bat

ECHO Intallation completed
GOTO :eof

:env_error
ECHO Set environment variable %ZorroInstallDir% first 
EXIT /B 1
